#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Run libe3's cluster stage for one commit and say what it found.

`run` starts the `libe3-ci` pipeline for a branch over the cluster's REST API, waits for it,
and writes a markdown fragment for `CI report` from its results. It exits non-zero, with the
reason in the fragment, on a missing secret, an unreachable cluster, a failed build, a failed
cell or a timeout, and never exits zero without a pipeline having run and reported.

`gate` waits for the other workflows of the commit and fails if one failed, so a broken
commit does not occupy the cluster.

Environment for `run`: OPENSHIFT_SA_TOKEN, CLUSTER_API_URL, TEKTON_NAMESPACE, and optionally
CLUSTER_NODE (a node to pin the end-to-end and benchmark pods to).

Usage:
  cluster_stage.py run --branch B --tag T --fragment FILE --results FILE
  cluster_stage.py gate --repo OWNER/NAME --sha SHA --self-run-id ID [--pr N]
"""
import argparse
import hashlib
import json
import os
import sys
import time
import urllib.error
import urllib.request

BENCH_GROUPS = ("zmq-ipc", "zmq-tcp", "posix-ipc", "posix-tcp", "posix-sctp")
RUN_TASKS = ("e2e",) + tuple(f"bench-{group}" for group in BENCH_GROUPS)
REPORT_ROWS = {"6": "wire RAN to dApp", "15": "wire dApp to RAN", "Total": "round trip"}


# What the image of the build's packages depends on. VERSION is left out: it changes with every
# release and nothing in the packages depends on it.
DEPS_FILES = ("build_libe3", "docker/Dockerfile.deps")


class StageError(Exception):
    """A named reason the stage cannot say it passed."""


def settings(env: dict) -> tuple[str, str, str]:
    """The token, the API address and the namespace, each required."""
    values = []
    for name in ("OPENSHIFT_SA_TOKEN", "CLUSTER_API_URL", "TEKTON_NAMESPACE"):
        value = "".join(env.get(name, "").split())
        if not value:
            raise StageError(f"{name} is not set for this repository, so the cluster cannot be reached")
        values.append(value)
    return values[0], values[1].rstrip("/"), values[2]


def pod_template(terms: list) -> dict:
    return {"affinity": {"nodeAffinity": {"requiredDuringSchedulingIgnoredDuringExecution": {
        "nodeSelectorTerms": terms}}}}


def deps_tag(root: str = ".") -> str:
    """The tag of the packages image for the checkout in `root`: a hash of the files it is built from."""
    digest = hashlib.sha256()
    for name in DEPS_FILES:
        try:
            with open(os.path.join(root, name), "rb") as handle:
                data = handle.read()
        except OSError as exc:
            raise StageError(f"cannot read {name} to tag the packages image: {exc}")
        digest.update(name.encode() + b"\0" + data + b"\0")
    return digest.hexdigest()[:16]


def render_run(branch: str, tag: str, node: str = "", deps: str = "") -> dict:
    """The PipelineRun for `branch`.

    The build may run on any amd64 worker that is not a control-plane node. With
    `node`, the end-to-end and benchmark tasks are pinned to it, so that numbers of
    different runs come from one machine; the build is not, as it uses every core.
    """
    anywhere = [{"matchExpressions": [
        {"key": "kubernetes.io/arch", "operator": "In", "values": ["amd64"]},
        {"key": "node-role.kubernetes.io/master", "operator": "DoesNotExist"}]}]
    spec = {
        "timeouts": {"pipeline": "2h0m0s"},
        "pipelineRef": {"name": "libe3-ci"},
        "taskRunTemplate": {"serviceAccountName": "pipeline", "podTemplate": pod_template(anywhere)},
        "params": [{"name": "GIT_REPO_BRANCH", "value": branch},
                   {"name": "IMAGE_TAG", "value": tag},
                   {"name": "DEPS_TAG", "value": deps}],
    }
    if node:
        pinned = pod_template([{"matchExpressions": [
            {"key": "kubernetes.io/hostname", "operator": "In", "values": [node]}]}])
        spec["taskRunSpecs"] = [{"pipelineTaskName": name, "podTemplate": pinned} for name in RUN_TASKS]
    return {"apiVersion": "tekton.dev/v1", "kind": "PipelineRun",
            "metadata": {"generateName": "libe3-ci-"}, "spec": spec}


def request(method: str, url: str, token: str, body: dict | None = None) -> dict:
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method=method, headers={
        "Authorization": f"Bearer {token}", "Content-Type": "application/json",
        "Accept": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=60) as response:
            return json.load(response)
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode(errors="replace")[:300]
        raise StageError(f"the cluster answered {exc.code} to {method} {url.split('/apis/')[-1]}: {detail}")
    except (urllib.error.URLError, OSError) as exc:
        raise StageError(f"the cluster could not be reached: {exc}")


def state(run: dict) -> tuple[str, str]:
    """('running' | 'succeeded' | 'failed', the condition's reason and message)."""
    condition = (run.get("status", {}).get("conditions") or [{}])[0]
    detail = f"{condition.get('reason', '')}: {condition.get('message', '')}".strip(": ")
    status = condition.get("status")
    if status == "True":
        return "succeeded", detail
    if status == "False":
        return "failed", detail
    return "running", detail


def results(run: dict) -> dict[str, str]:
    return {r["name"]: r["value"] for r in run.get("status", {}).get("results", [])}


def exit_status(report: str) -> str:
    """The `exit=N` line the run task appends; 'missing' when there is none."""
    lines = [line for line in report.splitlines() if line.startswith("exit=")]
    return lines[-1][5:] if lines else "missing"


def parse_bench(report: str) -> list[dict]:
    """Cells of a benchmark CSV: name, samples and each row's mean, p50, p99, max."""
    cells: list[dict] = []
    for line in report.splitlines():
        fields = line.split(",")
        if len(fields) == 2 and "/" in fields[0]:
            cells.append({"cell": fields[0], "samples": fields[1], "rows": {}})
        elif len(fields) == 5 and cells:
            cells[-1]["rows"][fields[0]] = tuple(fields[1:])
    return cells


def bench_table(cells: list[dict]) -> list[str]:
    head = ["cell", "N"] + [f"{label} p50 / p99" for label in REPORT_ROWS.values()]
    lines = ["| " + " | ".join(head) + " |", "|" + "---|" * len(head)]
    for cell in cells:
        cols = [cell["cell"], cell["samples"]]
        for row in REPORT_ROWS:
            values = cell["rows"].get(row)
            cols.append(f"{values[1]} / {values[2]}" if values else "n/a")
        lines.append("| " + " | ".join(cols) + " |")
    return lines


def judge(run: dict, run_name: str, node_note: str) -> tuple[list[str], list[str]]:
    """(fragment lines, problems) for a finished PipelineRun."""
    kind, detail = state(run)
    lines = [f"### Cluster stage (`{run_name}`)", ""]
    problems: list[str] = []
    if kind != "succeeded":
        problems.append(f"the pipeline {kind}: {detail}")
        lines += [f"❌ The pipeline {kind}: {detail}", "",
                  "The build or a task did not finish; the logs are on the cluster."]
        return lines, problems
    found = results(run)
    e2e = found.get("e2e", "")
    if exit_status(e2e) != "0" or any(line.startswith("FAIL") for line in e2e.splitlines()):
        problems.append("an end-to-end cell failed")
    lines += ["End-to-end, every link, transport and encoding:", "", "```"]
    lines += [line for line in e2e.splitlines() if not line.startswith("exit=")] or ["(no report)"]
    lines += ["```", ""]
    cells: list[dict] = []
    for group in BENCH_GROUPS:
        report = found.get(f"bench-{group}", "")
        if exit_status(report) != "0":
            problems.append(f"the benchmark for {group} failed")
        cells += parse_bench(report)
    lines += ["Full-loop latency in microseconds:", ""] + bench_table(cells) + [""]
    if node_note:
        lines += [node_note, ""]
    return lines, problems


def command_run(args: argparse.Namespace) -> int:
    fragment_lines = ["### Cluster stage", ""]
    code = 1
    try:
        token, api, namespace = settings(os.environ)
        url = f"{api}/apis/tekton.dev/v1/namespaces/{namespace}/pipelineruns"
        manifest = render_run(args.branch, args.tag, os.environ.get("CLUSTER_NODE", ""), deps_tag())
        name = request("POST", url, token, manifest)["metadata"]["name"]
        print(f"created PipelineRun {name}")
        deadline = time.time() + args.minutes * 60
        run: dict = {}
        while time.time() < deadline:
            run = request("GET", f"{url}/{name}", token)
            if state(run)[0] != "running":
                break
            time.sleep(args.poll_seconds)
        else:
            raise StageError(f"the pipeline did not finish in {args.minutes} minutes")
        note = "End-to-end and benchmark pods pinned to one node." if os.environ.get("CLUSTER_NODE") else "Not pinned to a node."
        fragment_lines, problems = judge(run, name, note)
        with open(args.results, "w", encoding="utf-8") as handle:
            json.dump(results(run), handle, indent=1)
        for problem in problems:
            print(f"::error::{problem}")
        code = 1 if problems else 0
    except StageError as exc:
        fragment_lines += [f"❌ {exc}", ""]
        print(f"::error::{exc}")
    with open(args.fragment, "w", encoding="utf-8") as handle:
        handle.write("\n".join(fragment_lines) + "\n")
    return code


def gh_api(path: str) -> dict:
    import subprocess
    out = subprocess.run(["gh", "api", path], capture_output=True, text=True, check=True).stdout
    return json.loads(out)


def gate_verdict(runs: list[dict], own_run_id: str, skip: tuple[str, ...]) -> tuple[bool, list[str]]:
    """(all finished, names of failed runs) among the runs the cluster stage waits for."""
    waiting = [r for r in runs if str(r["id"]) != own_run_id and r["name"] not in skip]
    finished = all(r["status"] == "completed" for r in waiting)
    failed = [r["name"] for r in waiting if r.get("conclusion") in
              ("failure", "cancelled", "timed_out", "startup_failure")]
    return finished, failed


def command_gate(args: argparse.Namespace) -> int:
    deadline = time.time() + args.minutes * 60
    while time.time() < deadline:
        runs = gh_api(f"repos/{args.repo}/actions/runs?head_sha={args.sha}&per_page=100")["workflow_runs"]
        finished, failed = gate_verdict(runs, args.self_run_id, tuple(args.skip))
        if failed:
            print(f"::error::the gate failed ({', '.join(failed)}); the cluster stage did not run")
            return 1
        if finished and len(runs) > 1:
            return 0
        time.sleep(args.poll_seconds)
    print("::error::the gate did not finish in time; the cluster stage did not run")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    run = sub.add_parser("run")
    run.add_argument("--branch", required=True)
    run.add_argument("--tag", required=True)
    run.add_argument("--fragment", required=True)
    run.add_argument("--results", required=True)
    run.add_argument("--minutes", type=int, default=100)
    run.add_argument("--poll-seconds", type=int, default=30)
    gate = sub.add_parser("gate")
    gate.add_argument("--repo", required=True)
    gate.add_argument("--sha", required=True)
    gate.add_argument("--self-run-id", required=True)
    gate.add_argument("--skip", nargs="*", default=["CI report", "Cluster stage"])
    gate.add_argument("--minutes", type=int, default=40)
    gate.add_argument("--poll-seconds", type=int, default=30)
    args = parser.parse_args()
    return command_run(args) if args.command == "run" else command_gate(args)


if __name__ == "__main__":
    sys.exit(main())

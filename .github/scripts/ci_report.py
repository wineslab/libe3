#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0
"""Post one consolidated CI report comment on a pull request (see issue #64).

Waits until every workflow run for the head commit has reached a terminal
state, then renders a single sticky comment: a verdict, a per-workflow status
table, whatever detail each workflow published as a `ci-report-*` artifact,
and -- only when every required check is green -- the fast-forward merge
command. Replaces the five per-workflow bot comments this repository used to
post, and the `merge-command` job that polled for them.

The workflow list is discovered from the API and the checkout, never
hardcoded: a new workflow appears in the report on its own.

Usage:
  ci_report.py --repo OWNER/NAME --pr N --sha HEAD_SHA [--branch REF]
  ci_report.py --repo OWNER/NAME --pr N --sha HEAD_SHA --dry-run
"""
import argparse
import calendar
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

MARKER = "<!-- ci-report -->"

# The per-workflow comments this report replaces. Removed from the thread on
# first run so a migrating PR does not keep six comments alive.
LEGACY_MARKERS = (
    "<!-- ready-to-merge -->",
    "<!-- e2e-dapp-results -->",
    "<!-- e2e-topologies-results -->",
    "<!-- mpmc-bench-results -->",
    "<!-- full-loop-latency-results -->",
)

# Mirrors the `Detect code changes` filter every gated workflow applies. This
# decides only how the report *labels* a workflow whose steps self-skipped,
# never whether anything runs.
NONCODE = re.compile(r"(\.md$)|(^docs/)|(^\.github/)|(^LICENSE$)|(^CONTRIBUTORS$)|(^\.gitignore$)")

GATE_JOB = "Detect code changes"
FRAGMENT_PREFIX = "ci-report-"


def gh(*args: str, check: bool = True) -> str:
    proc = subprocess.run(["gh", *args], capture_output=True, text=True)
    if check and proc.returncode != 0:
        raise RuntimeError(f"gh {' '.join(args)} failed: {proc.stderr.strip()}")
    return proc.stdout


def api(path: str, check: bool = True) -> object:
    out = gh("api", "-H", "Accept: application/vnd.github+json", path, check=check)
    return json.loads(out) if out.strip() else None


def api_paged(path: str, key: str | None = None) -> list:
    """Page through `path`, returning the concatenated items."""
    items: list = []
    page = 1
    joiner = "&" if "?" in path else "?"
    while True:
        chunk = api(f"{path}{joiner}per_page=100&page={page}")
        batch = chunk if key is None else (chunk or {}).get(key, [])
        if not batch:
            return items
        items.extend(batch)
        if len(batch) < 100:
            return items
        page += 1


def required_contexts(repo: str, branch: str) -> set[str] | None:
    """Effective required checks for `branch`, or None if they can't be read.

    Read from the rules API rather than a hardcoded list so the merge command
    cannot drift from whatever the ruleset currently requires.
    """
    rules = api(f"repos/{repo}/rules/branches/{branch}", check=False)
    if not isinstance(rules, list):
        return None
    for rule in rules:
        if rule.get("type") == "required_status_checks":
            checks = rule.get("parameters", {}).get("required_status_checks", [])
            return {c["context"] for c in checks}
    return set()


def head_sha(repo: str, pr: int) -> str:
    return (api(f"repos/{repo}/pulls/{pr}") or {})["head"]["sha"]


def workflow_runs(repo: str, sha: str, self_run_id: str | None) -> list[dict]:
    """Newest attempt of each workflow that ran for `sha`, excluding ourselves."""
    runs = api_paged(f"repos/{repo}/actions/runs?head_sha={sha}", "workflow_runs")
    newest: dict[int, dict] = {}
    for run in runs:
        if self_run_id and str(run["id"]) == str(self_run_id):
            continue
        current = newest.get(run["workflow_id"])
        if current is None or run["run_attempt"] > current["run_attempt"]:
            newest[run["workflow_id"]] = run
    return sorted(newest.values(), key=lambda r: r["name"].lower())


def check_runs(repo: str, sha: str) -> list[dict]:
    return api_paged(f"repos/{repo}/commits/{sha}/check-runs", "check_runs")


def run_jobs(repo: str, run_id: int) -> list[dict]:
    return api_paged(f"repos/{repo}/actions/runs/{run_id}/jobs", "jobs")


def pr_touches_code(repo: str, pr: int) -> bool:
    files = api_paged(f"repos/{repo}/pulls/{pr}/files")
    return any(not NONCODE.search(f["filename"]) for f in files)


def wait_for_ci(repo: str, pr: int, sha: str, self_run_id: str | None,
                required: set[str] | None, deadline_s: int, poll_s: int) -> bool:
    """Block until every run and every required context is terminal.

    Returns False if the deadline passed with work outstanding. Iterates over
    the *required* list rather than over the check-runs found: a required
    context whose job has not been created yet appears nowhere in the
    check-runs response, so keying off that response alone can conclude
    "everything found is done" while a required check has not started.
    """
    end = time.monotonic() + deadline_s
    while True:
        pending = [r["name"] for r in workflow_runs(repo, sha, self_run_id)
                   if r["status"] != "completed"]
        seen = {c["name"]: c for c in check_runs(repo, sha)}
        for context in sorted(required or ()):
            check = seen.get(context)
            if check is None:
                pending.append(f"{context} (not created yet)")
            elif check["status"] != "completed":
                pending.append(context)

        if not pending:
            return True
        if time.monotonic() >= end:
            print(f"::warning::deadline reached with {len(pending)} check(s) outstanding: "
                  f"{', '.join(sorted(pending)[:8])}")
            return False
        if head_sha(repo, pr) != sha:
            print(f"head moved off {sha[:7]} while waiting; nothing to report")
            sys.exit(0)
        print(f"waiting on {len(pending)}: {', '.join(sorted(pending)[:6])}", flush=True)
        time.sleep(poll_s)


def duration(run: dict) -> str:
    started, ended = run.get("run_started_at"), run.get("updated_at")
    fmt = "%Y-%m-%dT%H:%M:%SZ"
    try:
        secs = calendar.timegm(time.strptime(ended, fmt)) - calendar.timegm(time.strptime(started, fmt))
    except (TypeError, ValueError):
        return "—"
    return f"{secs // 60}m{secs % 60:02d}s" if secs >= 0 else "—"


def classify(run: dict, jobs: list[dict], code_changed: bool) -> tuple[str, int]:
    """Human-readable result for a run, plus a sort rank (0 = worst)."""
    conclusion = run.get("conclusion")
    if run["status"] != "completed":
        return "⏳ still running", 0
    if conclusion == "failure":
        failed = [j["name"] for j in jobs if j.get("conclusion") == "failure"]
        detail = f" ({', '.join(failed[:3])})" if failed else ""
        return f"❌ failure{detail}", 0
    if conclusion in ("cancelled", "timed_out", "stale"):
        return f"⚪ {conclusion.replace('_', ' ')}", 1
    if conclusion == "skipped":
        return "⏭️ skipped", 3
    if conclusion == "success":
        gated = any(j["name"] == GATE_JOB for j in jobs)
        if gated and not code_changed:
            return "✅ success (steps skipped — no code changes)", 2
        return "✅ success", 2
    return f"• {conclusion or run['status']}", 2


def fragments(repo: str, runs: list[dict], workdir: str) -> dict[str, list[str]]:
    """Download every `ci-report-*` artifact, grouped by producing workflow."""
    collected: dict[str, list[str]] = {}
    for run in runs:
        arts = api_paged(f"repos/{repo}/actions/runs/{run['id']}/artifacts", "artifacts")
        for art in sorted(arts, key=lambda a: a["name"]):
            if not art["name"].startswith(FRAGMENT_PREFIX) or art.get("expired"):
                continue
            dest = os.path.join(workdir, str(run["id"]), art["name"])
            os.makedirs(dest, exist_ok=True)
            gh("run", "download", str(run["id"]), "--repo", repo,
               "-n", art["name"], "--dir", dest, check=False)
            for root, _, names in os.walk(dest):
                for name in sorted(names):
                    if not name.endswith(".md"):
                        continue
                    with open(os.path.join(root, name), encoding="utf-8") as handle:
                        text = handle.read().strip()
                    if text:
                        collected.setdefault(run["name"], []).append(text)
    return collected


def render(sha: str, branch: str, runs: list[dict], jobs_by_run: dict[int, list[dict]],
           missing: list[str], detail: dict[str, list[str]], required: set[str] | None,
           checks: list[dict], code_changed: bool, complete: bool) -> str:
    short = sha[:7]
    by_name = {c["name"]: c for c in checks}
    gate_note = ""
    if required is None:
        green = all(c.get("conclusion") in ("success", "skipped", "neutral") for c in checks)
        gate_note = (" Required checks could not be read from the ruleset, so the verdict "
                     "covers every check on the commit.")
    else:
        green = all(by_name.get(ctx, {}).get("conclusion") == "success" for ctx in required)

    failed_ctx = sorted(ctx for ctx in (required or set())
                        if by_name.get(ctx, {}).get("conclusion") not in (None, "success"))
    if not complete:
        verdict = "⏳ CI did not finish inside the report window"
    elif failed_ctx:
        verdict = f"❌ {len(failed_ctx)} required check{'s' if len(failed_ctx) > 1 else ''} failed"
    elif green:
        verdict = "✅ all checks passed"
    else:
        verdict = "⚠️ finished with non-successful checks"

    rows = []
    for run in runs:
        result, rank = classify(run, jobs_by_run.get(run["id"], []), code_changed)
        rows.append((rank, run["name"], result, duration(run),
                     f"[#{run['run_number']}]({run['html_url']})"))
    for name in missing:
        rows.append((4, name, "⏭️ not triggered (paths filter)", "—", "—"))
    rows.sort(key=lambda row: (row[0], row[1].lower()))

    out = [MARKER, f"## CI report — `{short}` — {verdict}", ""]
    if not code_changed:
        out += ["> No code changes in this PR: the workflows gated by `Detect code changes` "
                "report success without building or testing anything. A workflow triggered by "
                "its own `paths` filter still ran in full.", ""]
    out += ["| Workflow | Result | Time | Run |", "|---|---|---|---|"]
    out += [f"| {name} | {result} | {took} | {link} |" for _, name, result, took, link in rows]
    out.append("")

    if failed_ctx:
        out += ["**Failing required checks:** " + ", ".join(f"`{c}`" for c in failed_ctx), ""]

    for workflow in sorted(detail):
        out += [f"<details><summary>{workflow}</summary>", "",
                "\n\n".join(detail[workflow]), "", "</details>", ""]

    if complete and green and not failed_ctx:
        out += ["### Ready to merge (fast-forward only)", "",
                "A maintainer can land the reviewed commits with:", "",
                "```bash", "git fetch origin",
                f"git checkout main && git merge --ff-only {sha} && git push origin main",
                "```", "",
                f"Head: `{sha}` (branch `{branch}`). If `--ff-only` fails as non-fast-forward, "
                "the branch must be rebased on the latest `main`.", ""]
    out.append(f"> One comment per PR, rewritten in place once every workflow for `{short}` "
               f"finished.{gate_note}")
    return "\n".join(out)


def post(repo: str, pr: int, body: str) -> None:
    comments = api_paged(f"repos/{repo}/issues/{pr}/comments")
    mine = next((c for c in comments if c["body"].startswith(MARKER)), None)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8") as handle:
        json.dump({"body": body}, handle)
        payload = handle.name
    try:
        if mine:
            gh("api", f"repos/{repo}/issues/comments/{mine['id']}", "-X", "PATCH",
               "--input", payload)
            print(f"updated comment {mine['id']}")
        else:
            gh("api", f"repos/{repo}/issues/{pr}/comments", "-X", "POST", "--input", payload)
            print("posted a new report comment")
    finally:
        os.unlink(payload)

    for comment in comments:
        if comment["body"].lstrip().startswith(LEGACY_MARKERS):
            gh("api", f"repos/{repo}/issues/comments/{comment['id']}", "-X", "DELETE", check=False)
            print(f"removed superseded comment {comment['id']}")


def untriggered(workflows_dir: str, ran: set[str], self_workflow: str) -> list[str]:
    """Display names of PR-triggered workflows with no run here (a `paths` skip)."""
    if not os.path.isdir(workflows_dir):
        return []
    names = []
    for filename in sorted(os.listdir(workflows_dir)):
        if not filename.endswith((".yml", ".yaml")) or filename in ran or filename == self_workflow:
            continue
        with open(os.path.join(workflows_dir, filename), encoding="utf-8") as handle:
            head = handle.read(4000)
        if "pull_request" not in head:
            continue
        declared = re.search(r"(?m)^name:\s*(.+?)\s*$", head)
        names.append(declared.group(1).strip("\"'") if declared else filename)
    return names


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, help="OWNER/NAME")
    parser.add_argument("--pr", required=True, type=int)
    parser.add_argument("--sha", required=True, help="PR head SHA the report is about")
    parser.add_argument("--branch", default="?", help="PR head ref, for the merge hint")
    parser.add_argument("--base-branch", default="main", help="branch whose ruleset gates merging")
    parser.add_argument("--self-run-id", default=os.environ.get("GITHUB_RUN_ID"),
                        help="this workflow run, excluded from the wait set")
    parser.add_argument("--self-workflow", default="ci-report.yml",
                        help="this workflow's file name, excluded from the report")
    parser.add_argument("--workflows-dir", default=".github/workflows")
    parser.add_argument("--deadline-minutes", type=int, default=110)
    parser.add_argument("--poll-seconds", type=int, default=30)
    parser.add_argument("--dry-run", action="store_true",
                        help="render to stdout; post nothing and skip waiting")
    args = parser.parse_args()

    if head_sha(args.repo, args.pr) != args.sha:
        print(f"PR #{args.pr} has moved off {args.sha[:7]}; a later run will report")
        return 0

    required = required_contexts(args.repo, args.base_branch)
    complete = True
    if not args.dry_run:
        complete = wait_for_ci(args.repo, args.pr, args.sha, args.self_run_id, required,
                               args.deadline_minutes * 60, args.poll_seconds)

    runs = workflow_runs(args.repo, args.sha, args.self_run_id)
    if args.dry_run:
        complete = all(run["status"] == "completed" for run in runs)
    jobs_by_run = {run["id"]: run_jobs(args.repo, run["id"]) for run in runs}
    missing = untriggered(args.workflows_dir,
                          {os.path.basename(run["path"]) for run in runs}, args.self_workflow)
    code_changed = pr_touches_code(args.repo, args.pr)

    workdir = tempfile.mkdtemp(prefix="ci-report-")
    try:
        detail = fragments(args.repo, runs, workdir)
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    body = render(args.sha, args.branch, runs, jobs_by_run, missing, detail, required,
                  check_runs(args.repo, args.sha), code_changed, complete)
    if args.dry_run:
        print(body)
        return 0
    try:
        post(args.repo, args.pr, body)
    except RuntimeError as exc:
        # A fork PR hands this job a read-only token, so commenting is not
        # possible. The report is advisory: warn and stay green rather than
        # painting a failure the contributor cannot act on.
        print(f"::warning::could not post the CI report: {exc}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

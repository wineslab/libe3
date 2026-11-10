# SPDX-FileCopyrightText: Copyright (c) 2026 Northeastern University
# SPDX-License-Identifier: Apache-2.0

"""Tests of the apt helpers in build_libe3, run against a fake apt-get."""
import os
import re
import shutil
import stat
import subprocess
import tempfile
import textwrap
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def helpers() -> str:
    """The text of APT_OPTS, retry_apt and apt_install, taken from build_libe3."""
    with open(os.path.join(ROOT, "build_libe3")) as handle:
        text = handle.read()
    block = re.search(r"^APT_OPTS=\(.*?^apt_install\(\) \{.*?^\}\n", text, re.S | re.M)
    if not block:
        raise AssertionError("APT_OPTS .. apt_install not found in build_libe3")
    return "echo_warning() { echo \"warning: $*\" >&2; }\necho_error() { echo \"error: $*\" >&2; }\n" + block.group(0)


@unittest.skipUnless(shutil.which("timeout") and shutil.which("bash"), "needs bash and timeout")
class AptHelpers(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir)
        self.log = os.path.join(self.dir, "calls")
        self.write("sudo", 'exec "$@"\n')

    def write(self, name: str, body: str) -> None:
        path = os.path.join(self.dir, name)
        with open(path, "w") as handle:
            handle.write("#!/bin/bash\n" + body)
        os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR)

    def fake_apt(self, script: str) -> None:
        """An apt-get that logs its subcommand, then runs `script` ($sub; $n = calls of it so far)."""
        self.write("apt-get", textwrap.dedent(f'''
            sub=update
            for a in "$@"; do case "$a" in update|install) sub=$a;; esac; done
            case " $* " in *" --download-only "*) sub=download;; esac
            echo "$sub $*" >> "{self.log}"
            n=$(grep -c "^$sub " "{self.log}")
            {script}
        '''))

    def run_helper(self, call: str, **env) -> subprocess.CompletedProcess:
        script = helpers() + "\n" + call
        full = dict(os.environ, PATH=self.dir + ":" + os.environ["PATH"], **env)
        return subprocess.run(["bash", "-c", script], env=full, capture_output=True, text=True, timeout=120)

    def logged(self) -> list:
        try:
            with open(self.log) as handle:
                return handle.read().splitlines()
        except FileNotFoundError:
            return []

    def calls(self) -> list:
        return [line.split()[0] for line in self.logged()]

    def test_order_and_options(self):
        self.fake_apt("exit 0")
        result = self.run_helper("apt_install pkg-a pkg-b")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.calls(), ["update", "download", "install"])
        for line in self.logged():
            self.assertIn("Acquire::Retries=5", line)
            self.assertIn("DPkg::Lock::Timeout=300", line)
        self.assertIn("--download-only pkg-a pkg-b", "\n".join(self.logged()))

    def test_stalled_download_is_killed_and_retried(self):
        self.fake_apt('[ "$sub" = download ] && [ "$n" -lt 3 ] && sleep 30; exit 0')
        result = self.run_helper("apt_install pkg", APT_DOWNLOAD_LIMIT="1")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.calls(), ["update", "download", "download", "download", "install"])

    def test_unpacking_is_not_given_a_time_limit(self):
        self.fake_apt('[ "$sub" = install ] && sleep 3; exit 0')
        result = self.run_helper("apt_install pkg", APT_UPDATE_LIMIT="1", APT_DOWNLOAD_LIMIT="1")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.calls(), ["update", "download", "install"])

    def test_download_that_never_finishes_fails_without_unpacking(self):
        self.fake_apt('[ "$sub" = download ] && sleep 30; exit 0')
        result = self.run_helper("apt_install pkg", APT_DOWNLOAD_LIMIT="1")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.calls(), ["update", "download", "download", "download"])
        self.assertIn("failed after 3 attempts", result.stderr)

    def test_failing_install_fails_the_helper(self):
        self.fake_apt('[ "$sub" = install ] && exit 100; exit 0')
        result = self.run_helper("apt_install pkg")
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()

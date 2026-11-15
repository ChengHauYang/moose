# This file is part of the MOOSE framework
# https://mooseframework.inl.gov
#
# All rights reserved, see COPYRIGHT for full restrictions
# https://github.com/idaholab/moose/blob/master/COPYRIGHT
#
# Licensed under LGPL 2.1, please see LICENSE for details
# https://www.gnu.org/licenses/lgpl-2.1.html

"""Test harness controls and the macOS command entry point."""

import os
import signal
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CONTROL = ROOT / "scripts/harness/control.py"
HARNESS = ROOT / "scripts/mac/moose.sh"
BASELINE = ROOT / "scripts/harness/baseline.py"


class ControlTests(unittest.TestCase):
    """Exercise result reporting and checkout lock lifetime."""

    def report(self, output, code=0, unit=False):
        """Run the reporter with synthetic test output and a process exit code."""
        return subprocess.run(
            [
                sys.executable,
                str(CONTROL),
                "test",
                *(["--unit"] if unit else []),
                "sample",
                sys.executable,
                "-c",
                "import sys; print(%r); sys.exit(%d)" % (output, code),
            ],
            capture_output=True,
            text=True,
        )

    def test_counts_and_failure_list(self):
        """Report failed test names and preserve the footer counts."""
        result = self.report(
            "test:bad ..... FAILED (EXODIFF)\n2 passed, 1 skipped, 1 FAILED"
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("FAILED scope=sample test=test:bad", result.stdout)
        self.assertIn(
            "RESULT scope=sample passed=2 failed=1 skipped=1 status=FAIL exit=1",
            result.stdout,
        )

    def test_success(self):
        """Accept a complete passing summary containing terminal colors."""
        result = self.report("\x1b[32m2 passed, 1 skipped, 0 failed\x1b[0m")
        self.assertEqual(result.returncode, 0)
        self.assertIn("status=PASS exit=0", result.stdout)

    def test_empty_selection(self):
        """Reject an empty selection and suggest relevant scopes."""
        result = self.report("0 passed, 0 skipped, 0 failed")
        self.assertEqual(result.returncode, 1)
        self.assertIn("reason=no_tests_selected", result.stdout)
        self.assertIn("framework-kokkos", result.stdout)
        self.assertIn("neml2", result.stdout)

    def test_missing_footer(self):
        """Reject incomplete output even when individual tests passed."""
        result = self.report("test:one ..... OK")
        self.assertEqual(result.returncode, 1)
        self.assertIn("passed=unknown", result.stdout)
        self.assertIn("reason=missing_summary", result.stdout)

    def test_launch_error_still_reports_result(self):
        """Emit a result when the test command cannot launch."""
        result = subprocess.run(
            [sys.executable, str(CONTROL), "test", "sample", "/does/not/exist"],
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("RESULT scope=sample passed=unknown", result.stdout)
        self.assertIn("reason=launch_error", result.stdout)

    def test_nonzero_exit_is_never_hidden_by_passing_counts(self):
        """Preserve process errors despite passing test counts."""
        result = self.report("2 passed, 0 skipped, 0 failed", 129)
        self.assertEqual(result.returncode, 129)
        self.assertIn("status=ERROR exit=129 process_exit=129", result.stdout)

    def test_parser_error_is_not_success(self):
        """Reject parser errors accompanying otherwise passing tests."""
        result = self.report("2 passed, 0 skipped, 0 failed, FATAL PARSER ERROR")
        self.assertEqual(result.returncode, 1)
        self.assertIn("status=ERROR", result.stdout)

    def test_unit_counts(self):
        """Read GoogleTest counts and list the failed test names."""
        result = self.report(
            "[==========] 3 tests from 1 test suite ran. (0 ms total)\n"
            "[  PASSED  ] 1 test.\n[  SKIPPED ] 1 test.\n"
            "[  FAILED  ] 1 test, listed below:\n[  FAILED  ] Suite.Bad",
            unit=True,
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("passed=1 failed=1 skipped=1", result.stdout)
        self.assertIn("test=Suite.Bad", result.stdout)

    def test_unit_empty_selection(self):
        """Reject a GoogleTest filter that selects no tests."""
        result = self.report(
            "[==========] 0 tests from 0 test suites ran. (0 ms total)", unit=True
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("no_tests_selected", result.stdout)

    def test_lock_exclusion_interruption_and_stale_metadata(self):
        """Keep commands exclusive and release the lock after interruption."""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            ready = root / "ready"
            command = [sys.executable, str(CONTROL), "guard", str(root)]
            # Child readiness is a pipe handshake; no scheduling sleeps are needed.
            with subprocess.Popen(
                [
                    *command,
                    "compile",
                    sys.executable,
                    "-u",
                    "-c",
                    "from pathlib import Path; import signal; Path(%r).touch(); "
                    "print('ready', flush=True); signal.pause()" % str(ready),
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            ) as owner:
                try:
                    self.assertEqual(owner.stdout.readline().strip(), "ready")
                    blocked = subprocess.run(
                        [
                            *command,
                            "test",
                            sys.executable,
                            "-c",
                            "raise SystemExit(99)",
                        ],
                        capture_output=True,
                        text=True,
                    )
                    self.assertEqual(blocked.returncode, 1)
                    self.assertIn("operation=compile", blocked.stderr)
                finally:
                    owner.send_signal(signal.SIGTERM)
                    owner.communicate(timeout=10)
                self.assertEqual(owner.returncode, 143)
            # The lock file is deliberately retained; only the kernel lock matters.
            self.assertTrue((root / ".git/moose-harness.lock").exists())
            result = subprocess.run([*command, "test", sys.executable, "-c", "pass"])
            self.assertEqual(result.returncode, 0)


class MacEntryTests(unittest.TestCase):
    """Exercise the macOS entry point against isolated checkout fixtures."""

    def setUp(self):
        """Prepare a checkout without harness scripts and a minimal Conda fixture."""
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "framework").mkdir()
        (self.root / "configure").touch()
        (self.root / "test").mkdir()
        (self.root / "prefix/bin").mkdir(parents=True)
        (self.root / "prefix/bin/python3").symlink_to(sys.executable)
        profile = self.root / "home/miniforge/etc/profile.d"
        profile.mkdir(parents=True)
        (profile / "conda.sh").write_text(
            'conda() { export CONDA_PREFIX="$TEST_CHECKOUT/prefix"; '
            'export SDKROOT="$TEST_CHECKOUT"; }\n'
        )
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        self.env = {
            key: value
            for key, value in os.environ.items()
            if not key.startswith("MOOSE_") and key not in ("JOBS", "BUILD_JOBS")
        }
        self.env.update(
            HOME=str(self.root / "home"),
            TEST_CHECKOUT=str(self.root),
            MOOSE_NEML2_SRC="conda",
        )
        self.script("test/moose_test-opt", "exit 0\n")
        self.script(
            "test/run_tests", 'echo "ARGS=$*"\necho "1 passed, 0 skipped, 0 failed"\n'
        )

    def script(self, path, content):
        """Write an executable shell fixture relative to the test checkout."""
        path = self.root / path
        path.write_text("#!/bin/bash\n" + content)
        path.chmod(0o755)

    def run_cli(self, *arguments):
        """Run the current harness from outside the target checkout."""
        return subprocess.run(
            [str(HARNESS), "-C", str(self.root), *arguments],
            cwd="/tmp",
            env=self.env,
            capture_output=True,
            text=True,
            timeout=20,
        )

    def test_old_checkout_uses_current_harness_and_default_jobs(self):
        """Use the current harness and configured jobs on a checkout without scripts."""
        result = self.run_cli("test", "framework", "--failed")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("ARGS=-j3 --compute-device=cpu --failed-tests", result.stdout)
        self.assertIn("RESULT scope=framework passed=1", result.stdout)
        self.assertFalse((self.root / "scripts").exists())

    def test_explicit_jobs(self):
        """Let an explicit job count override the configured default."""
        result = self.run_cli("test", "framework", "-j", "2")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("ARGS=-j2", result.stdout)

    def test_zero_selection_via_entrypoint(self):
        """Propagate empty-selection errors through the shell entry point."""
        self.script("test/run_tests", 'echo "0 passed, 0 skipped, 0 failed"\n')
        result = self.run_cli("test", "framework", "neml2")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("no_tests_selected", result.stdout)
        self.assertNotIn("TEST_COMPLETED=1", result.stdout)

    def test_format_uses_conda_v21_and_target_repo_from_tmp(self):
        """Require the Conda formatter and run it from the target repository."""
        (self.root / ".clang-format").write_text("BasedOnStyle: Mozilla\n")
        (self.root / "bad.C").write_text("int   x;\n")
        subprocess.run(["git", "-C", str(self.root), "add", "configure"], check=True)
        subprocess.run(
            [
                "git",
                "-C",
                str(self.root),
                "-c",
                "user.name=Test",
                "-c",
                "user.email=test@example.org",
                "commit",
                "-qm",
                "fixture",
            ],
            check=True,
        )
        self.script(
            "prefix/bin/clang-format",
            'if [[ "$1" == "--version" ]]; then\n'
            'echo "clang-format version 21.1.0"; exit; fi\n'
            'echo "FORMAT_CWD=$PWD ARGS=$*"\nexit 1\n',
        )
        result = self.run_cli("format", "--check")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("FORMAT_CWD=" + str(self.root.resolve()), result.stdout)
        self.assertIn("--dry-run --Werror bad.C", result.stdout)
        self.assertEqual((self.root / "bad.C").read_text(), "int   x;\n")
        self.script("prefix/bin/clang-format", 'echo "clang-format version 19.1.7"\n')
        result = self.run_cli("format", "--check")
        self.assertEqual(result.returncode, 1)
        self.assertIn("requires Conda clang-format v21", result.stderr)

    def test_agent_help_is_ten_lines(self):
        """Keep the agent reference to ten lines."""
        result = self.run_cli("help", "--agent")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(len(result.stdout.splitlines()), 10)

    @unittest.skipUnless(
        (Path(sys.executable).parent / "clang-format").exists(),
        "Conda formatter unavailable",
    )
    def test_real_formatter_checks_and_applies_repository_style(self):
        """Check without edits, then apply and verify the repository style."""
        formatter = Path(sys.executable).parent / "clang-format"
        (self.root / "prefix/bin/clang-format").symlink_to(formatter)
        (self.root / ".clang-format").write_text(
            "BasedOnStyle: LLVM\nIndentWidth: 7\n"
            "AllowShortFunctionsOnASingleLine: None\n"
        )
        original = "int main(){return 0;}\n"
        (self.root / "bad.C").write_text(original)
        subprocess.run(["git", "-C", str(self.root), "add", "configure"], check=True)
        subprocess.run(
            [
                "git",
                "-C",
                str(self.root),
                "-c",
                "user.name=Test",
                "-c",
                "user.email=test@example.org",
                "commit",
                "-qm",
                "fixture",
            ],
            check=True,
        )
        check = self.run_cli("format", "--check")
        self.assertNotEqual(check.returncode, 0, check.stdout + check.stderr)
        self.assertEqual((self.root / "bad.C").read_text(), original)
        formatted = self.run_cli("format")
        self.assertEqual(formatted.returncode, 0, formatted.stdout + formatted.stderr)
        self.assertIn("\n       return 0;\n", (self.root / "bad.C").read_text())
        check = self.run_cli("format", "--check")
        self.assertEqual(check.returncode, 0, check.stdout + check.stderr)

    def test_invalid_checkout_does_not_fall_back_to_main(self):
        """Reject invalid target paths without using the main checkout."""
        result = subprocess.run(
            [str(HARNESS), "-C", str(self.root / "missing"), "help"],
            env=self.env,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("Invalid target checkout", result.stderr)
        self.assertNotIn("MOOSE-SCZM macOS CLI", result.stdout)

    def test_exodiff_defaults_and_explicit_override(self):
        """Pass default tolerances before explicit caller overrides."""
        self.script("prefix/bin/exodiff", 'echo "EXODIFF_ARGS=$*"\nexit 7\n')
        result = self.run_cli("exodiff", "old.e", "new.e", "-t", "1e-7")
        self.assertEqual(result.returncode, 7, result.stdout + result.stderr)
        self.assertIn(
            "EXODIFF_ARGS=-m -F 1e-10 -t 5.5e-6 -t 1e-7 old.e new.e", result.stdout
        )

    def test_baseline_uses_external_harness_and_retains_old_ref(self):
        """Retain an old worktree with shared dependencies and an independent lock."""
        origin = self.root / "media-origin"
        origin.mkdir()
        (origin / "input.txt").write_text("baseline test data\n")
        subprocess.run(["git", "init", "-q", str(origin)], check=True)
        subprocess.run(["git", "-C", str(origin), "add", "input.txt"], check=True)
        subprocess.run(
            [
                "git",
                "-C",
                str(origin),
                "-c",
                "user.name=Test",
                "-c",
                "user.email=test@example.org",
                "commit",
                "-qm",
                "test data",
            ],
            check=True,
        )
        subprocess.run(
            [
                "git",
                "-C",
                str(self.root),
                "-c",
                "protocol.file.allow=always",
                "submodule",
                "add",
                str(origin),
                "large_media",
            ],
            check=True,
            capture_output=True,
        )
        subprocess.run(["git", "-C", str(self.root), "add", "configure"], check=True)
        subprocess.run(
            [
                "git",
                "-C",
                str(self.root),
                "-c",
                "user.name=Test",
                "-c",
                "user.email=test@example.org",
                "commit",
                "-qm",
                "old ref without harness",
            ],
            check=True,
        )
        recorder = self.root / "record.sh"
        recorder.write_text(
            '#!/bin/bash\necho "BASELINE_ARGS=$* NEML2=$MOOSE_NEML2_SITE"\n'
        )
        recorder.chmod(0o755)
        environment = dict(
            self.env,
            CONDA_PREFIX="/existing/conda",
            MOOSE_NEML2_SITE="/existing/neml2",
            TMPDIR=str(self.root),
        )
        result = subprocess.run(
            [
                sys.executable,
                str(BASELINE),
                str(self.root),
                str(recorder),
                "HEAD",
                "neml2",
                "-j",
                "2",
            ],
            env=environment,
            capture_output=True,
            text=True,
            timeout=20,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        checkout = next(self.root.glob("moose-baseline-*/checkout"))
        self.assertFalse((checkout / "scripts").exists())
        self.assertEqual(
            (checkout / "large_media/input.txt").read_text(), "baseline test data\n"
        )
        self.assertIn(
            "verify neml2 --skip-neml2-sync -j 2 NEML2=/existing/neml2", result.stdout
        )
        independent = subprocess.run(
            [
                sys.executable,
                str(CONTROL),
                "guard",
                str(self.root),
                "baseline",
                sys.executable,
                str(CONTROL),
                "guard",
                str(checkout),
                "test",
                sys.executable,
                "-c",
                "pass",
            ],
            capture_output=True,
            text=True,
        )
        self.assertEqual(
            independent.returncode, 0, independent.stdout + independent.stderr
        )
        subprocess.run(
            [
                "git",
                "-C",
                str(self.root),
                "worktree",
                "remove",
                "--force",
                str(checkout),
            ],
            check=True,
        )


if __name__ == "__main__":
    unittest.main()

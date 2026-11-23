import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
CLI = ROOT / "scripts" / "harness" / "cli.py"
FIXTURES = Path(__file__).resolve().parent / "fixtures"
FIXTURE = FIXTURES / "step6_ld_sczm_mathypre_excerpt.log"
HARNESS_PASSED = FIXTURES / "testharness_sbm_passed.log"
HARNESS_FAILED = FIXTURES / "testharness_solid_mechanics_failed.log"
MOOSE_ERROR = FIXTURES / "moose_error_dtmin.log"


class LogstatTests(unittest.TestCase):
    def run_logstat(self, *contents):
        with tempfile.TemporaryDirectory() as directory:
            paths = []
            for index, content in enumerate(contents):
                path = Path(directory) / ("run%d.log" % index)
                path.write_text(content)
                paths.append(path)
            return self.run_paths(*paths)

    def run_paths(self, *paths, options=()):
        return subprocess.run(
            [sys.executable, str(CLI), "logstat", *map(str, paths), *options],
            capture_output=True,
            text=True,
        )

    def run_paths_verbose(self, content):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "run.log"
            path.write_text(content)
            return self.run_paths(path, options=("--verbose",))

    def test_real_log_excerpt(self):
        result = self.run_paths(FIXTURE)
        self.assertEqual(result.returncode, 0)
        self.assertIn("timesteps=5 newton=10 linear=574 snes=0", result.stdout)
        self.assertIn("retries=0 status=CONVERGED", result.stdout)

    def test_real_log_truncated_inside_timestep(self):
        lines = FIXTURE.read_text().splitlines(keepends=True)
        third_converged = [i for i, line in enumerate(lines) if "Solve Converged!" in line][2]
        result = self.run_logstat("".join(lines[:third_converged]))
        self.assertEqual(result.returncode, 1)
        self.assertIn("timesteps=2", result.stdout)
        self.assertIn("status=INCOMPLETE", result.stdout)

    def test_real_log_truncated_between_timesteps(self):
        text = FIXTURE.read_text()
        result = self.run_logstat(text[: text.index("Performance Graph:")])
        self.assertEqual(result.returncode, 1)
        self.assertIn("timesteps=5", result.stdout)
        self.assertIn("status=UNCONFIRMED", result.stdout)

    def test_terminal_records_exclude_iteration_zero(self):
        result = self.run_logstat(
            "Time Step 1, time = 1\n"
            " 0 Nonlinear |R| = 1.0e+00\n"
            "      0 Linear |R| = 1.0e+00\n"
            "      1 Linear |R| = 1.0e-01\n"
            "    Linear solve converged due to CONVERGED_RTOL iterations 1\n"
            " 1 Nonlinear |R| = 1.0e-01\n"
            "  Nonlinear solve converged due to CONVERGED_FNORM_ABS iterations 1\n"
            " Solve Converged!\n"
            "Performance Graph:\n"
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("timesteps=1 newton=1 linear=1 snes=0", result.stdout)
        self.assertIn("status=CONVERGED", result.stdout)

    def test_solve_did_not_converge_is_failure(self):
        result = self.run_logstat(
            "Time Step 1, time = 1\n"
            " 0 Nonlinear |R| = 1.0e+00\n"
            "  Nonlinear solve did not converge due to DIVERGED_MAX_IT iterations 10\n"
            " Solve Did NOT Converge!\n"
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("newton=10", result.stdout)
        self.assertIn("retries=1 status=SOLVER_FAILED", result.stdout)

    def test_multi_system_solve_did_not_converge_is_failure(self):
        result = self.run_logstat(
            "Time Step 1, time = 1\n"
            " 0 Nonlinear |R| = 1.0e+00\n"
            "System nl0: Solve Converged!\n"
            " 0 Nonlinear |R| = 1.0e+00\n"
            "System nl1: Solve Did NOT Converge!\n"
            "Performance Graph:\n"
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("status=SOLVER_FAILED", result.stdout)

    def test_linear_divergence_inside_converged_solve(self):
        result = self.run_paths_verbose(
            "Time Step 1, time = 1\n"
            " 0 Nonlinear |R| = 1.0e+00\n"
            "    Linear solve did not converge due to DIVERGED_ITS iterations 10000\n"
            " 1 Nonlinear |R| = 1.0e-02\n"
            "    Linear solve did not converge due to DIVERGED_ITS iterations 10000\n"
            " 2 Nonlinear |R| = 1.0e-13\n"
            "  Nonlinear solve converged due to CONVERGED_FNORM_ABS iterations 2\n"
            " Solve Converged!\n"
            "WARNING! There are options you set that were not used!\n"
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("newton=2 linear=20000", result.stdout)
        self.assertIn("status=CONVERGED", result.stdout)
        self.assertIn("reasons: DIVERGED_ITS, CONVERGED_FNORM_ABS\n", result.stdout)

    def test_failed_cutback_then_recovery(self):
        result = self.run_logstat(
            "Time Step 1, time = 1, dt = 1\n"
            " 0 Nonlinear |R| = 1.0e+00\n"
            "  Nonlinear solve did not converge due to DIVERGED_MAX_IT iterations 3\n"
            " Solve Did NOT Converge!\n"
            "Solve failed... cutting timestep\n"
            "Time Step 1, time = 0.5, dt = 0.5\n"
            " 0 Nonlinear |R| = 1.0e+00\n"
            "  Nonlinear solve converged due to CONVERGED_FNORM_ABS iterations 2\n"
            " Solve Converged!\n"
            "Performance Graph:\n"
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("timesteps=1 newton=5", result.stdout)
        self.assertIn("retries=1 status=CONVERGED", result.stdout)

    def test_failure_then_truncated_retry(self):
        result = self.run_logstat(
            "Time Step 1, time = 1, dt = 1\n"
            " 0 Nonlinear |R| = 1.0e+00\n"
            "  Nonlinear solve did not converge due to DIVERGED_MAX_IT iterations 3\n"
            " Solve Did NOT Converge!\n"
            "Time Step 1, time = 0.5, dt = 0.5\n"
            " 0 Nonlinear |R| = 1.0e+00\n"
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("retries=1 status=INCOMPLETE", result.stdout)

    def test_ksp_reason_does_not_mark_whole_solve_converged(self):
        result = self.run_logstat(
            "0 KSP Residual norm 1.0\n"
            "1 KSP Residual norm 1e-8\n"
            "KSP Converged reason: CONVERGED_RTOL\n"
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("linear=1", result.stdout)
        self.assertIn("status=UNKNOWN", result.stdout)

    def test_ksp_monitor_true_residual(self):
        result = self.run_logstat(
            "  0 KSP preconditioned resid norm 1.0e+00 true resid norm 1.0e+00 ||r(i)||/||b|| 1.0e+00\n"
            "  1 KSP preconditioned resid norm 1.0e-03 true resid norm 1.0e-03 ||r(i)||/||b|| 1.0e-03\n"
            "  2 KSP preconditioned resid norm 1.0e-06 true resid norm 1.0e-06 ||r(i)||/||b|| 1.0e-06\n"
        )
        self.assertIn("linear=2", result.stdout)

    def test_moose_error(self):
        result = self.run_paths(MOOSE_ERROR, options=("--verbose",))
        self.assertEqual(result.returncode, 1)
        self.assertIn("newton=1 linear=7 snes=0", result.stdout)
        self.assertIn("retries=1 status=MOOSE_ERROR", result.stdout)
        self.assertIn(
            "  error: The following occurred in the TimeStepper 'ConstantDT' of type ConstantDT. "
            "Solve failed and timestep already at or below dtmin, cannot continue!\n",
            result.stdout,
        )

    def test_moose_error_after_converged_solve(self):
        text = FIXTURE.read_text()
        result = self.run_logstat(
            text
            + "\n*** ERROR ***\nThe following occurred in the Output 'exodus' of type Exodus.\n"
            "\nSome failure\n\nStack frames: 1\n0: libMesh::print_trace(std::ostream&)\n"
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("status=MOOSE_ERROR", result.stdout)

    def test_multiapp_moose_error(self):
        result = self.run_paths_verbose(
            "Time Step 1, time = 1, dt = 1\n"
            " 0 Nonlinear |R| = 1.000000e+00\n"
            "sub0: \n"
            "sub0: *** ERROR ***\n"
            "sub0: Bad thing\n"
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("status=MOOSE_ERROR", result.stdout)
        self.assertIn("  error: sub0: Bad thing\n", result.stdout)

    def test_testharness_echoed_error_keeps_harness_verdict(self):
        text = HARNESS_PASSED.read_text()
        result = self.run_logstat(
            "test:errors.dtmin: *** ERROR ***\n"
            "test:errors.dtmin: Solve failed and timestep already at or below dtmin, cannot continue!\n"
            + text
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("status=TEST_PASSED", result.stdout)

    def test_testharness_passed(self):
        result = self.run_paths(HARNESS_PASSED)
        self.assertEqual(result.returncode, 0)
        self.assertIn("status=TEST_PASSED", result.stdout)

    def test_testharness_failed(self):
        result = self.run_paths(HARNESS_FAILED, options=("--verbose",))
        self.assertEqual(result.returncode, 1)
        # Echoed tester output ("<name>: Solve Converged!") is not counted as a solve.
        self.assertIn("timesteps=0 newton=0", result.stdout)
        self.assertIn("status=TEST_FAILED", result.stdout)
        self.assertIn("tests: passed=8 skipped=6 failed=1", result.stdout)

    def test_testharness_truncated_before_summary(self):
        lines = HARNESS_PASSED.read_text().splitlines(keepends=True)
        result = self.run_logstat("".join(lines[:3]))
        self.assertEqual(result.returncode, 1)
        self.assertIn("status=UNCONFIRMED", result.stdout)

    def test_testharness_failure_before_summary(self):
        text = HARNESS_FAILED.read_text()
        result = self.run_logstat(text[: text.index("Failed Tests:")])
        self.assertEqual(result.returncode, 1)
        self.assertIn("status=TEST_FAILED", result.stdout)

    def test_testharness_skipped_only(self):
        result = self.run_logstat(
            "test:kokkos/a.b ............ [cuda not in compute devices] SKIP\n"
            "Ran 0 tests in 1.0 seconds. Average test time 0.0 seconds, maximum test time 0.0 seconds.\n"
            "0 passed, 1 skipped, 0 failed\n"
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("status=SKIPPED", result.stdout)

    def test_testharness_no_tests(self):
        result = self.run_logstat(
            "Ran 0 tests in 1.0 seconds. Average test time 0.0 seconds, maximum test time 0.0 seconds.\n"
            "0 passed, 0 skipped, 0 failed\n"
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("status=NO_TESTS", result.stdout)

    def test_testharness_module_prefix_and_timing(self):
        result = self.run_logstat(
            "solid_mechanics/test:kokkos/a.b ............ OK [1.234s] [  45 MB]\n"
            "solid_mechanics/test:kokkos/a.c ...... [FINISHED] FAILED (TIMEOUT) [300.0s]\n"
            "1 passed, 0 skipped, 1 FAILED\n"
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("status=TEST_FAILED", result.stdout)

    def test_testharness_parser_error(self):
        result = self.run_logstat(
            "test:kokkos/a.b ............ OK\n"
            "1 passed, 0 skipped, 0 failed, FATAL PARSER ERROR\n"
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("status=TEST_FAILED", result.stdout)

    def test_incomplete(self):
        result = self.run_logstat("partial output\n")
        self.assertEqual(result.returncode, 1)
        self.assertIn("status=INCOMPLETE", result.stdout)

    def test_missing_solve_marker_is_incomplete(self):
        result = self.run_logstat(
            "Time Step 1, time = 1\n"
            "  Nonlinear solve converged due to CONVERGED_FNORM_ABS iterations 1\n"
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("status=INCOMPLETE", result.stdout)

    def test_multiple(self):
        content = (
            "Time Step 1, time = 1\n"
            "  Nonlinear solve converged due to CONVERGED_FNORM_ABS iterations 1\n"
            " Solve Converged!\n"
            "Performance Graph:\n"
        )
        result = self.run_logstat(content, content)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(len(result.stdout.splitlines()), 2)


if __name__ == "__main__":
    unittest.main()

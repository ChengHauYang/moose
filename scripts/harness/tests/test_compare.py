import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
CLI = ROOT / "scripts" / "harness" / "cli.py"
EXODUS_DIR = (
    ROOT / "test" / "tests" / "auxkernels" / "time_derivative_aux" / "gold"
)
EXODUS_A = EXODUS_DIR / "test_out.e"
EXODUS_B = EXODUS_DIR / "test_functor_out.e"


class CompareTests(unittest.TestCase):
    def run_compare(self, *arguments):
        return subprocess.run(
            [sys.executable, str(CLI), "compare", *map(str, arguments)],
            capture_output=True,
            text=True,
        )

    def write_pair(self, directory, first, second):
        file1 = Path(directory) / "first.csv"
        file2 = Path(directory) / "second.csv"
        file1.write_text(first)
        file2.write_text(second)
        return file1, file2

    def test_csv_same(self):
        with tempfile.TemporaryDirectory() as directory:
            file1, file2 = self.write_pair(
                directory, "time,u\n0,1\n0.1,2\n", "time,u\n0,1\n0.1,2\n"
            )
            result = self.run_compare(file1, file2)
        self.assertEqual(result.returncode, 0)
        self.assertIn("status: SAME", result.stdout)

    def test_csv_different_returns_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            file1, file2 = self.write_pair(
                directory,
                "time,u\n0,1\n0.1,2\n",
                "time,u\n0,1\n0.1,2.001\n",
            )
            result = self.run_compare(file1, file2)
        self.assertEqual(result.returncode, 1)
        self.assertIn("status: DIFFERENT", result.stdout)
        self.assertIn("worst: u", result.stdout)

    def test_csv_near_zero_reports_absolute_and_relative(self):
        with tempfile.TemporaryDirectory() as directory:
            file1, file2 = self.write_pair(
                directory,
                "time,u\n0,0\n0.1,1e-12\n",
                "time,u\n0,1e-15\n0.1,2e-12\n",
            )
            result = self.run_compare(file1, file2)
        self.assertEqual(result.returncode, 1)
        self.assertIn("max_abs=1e-12", result.stdout)
        self.assertIn("max_rel=1", result.stdout)

    def test_csv_missing_variable_is_error(self):
        with tempfile.TemporaryDirectory() as directory:
            file1, file2 = self.write_pair(
                directory, "time,u,v\n0,1,2\n", "time,u\n0,1\n"
            )
            result = self.run_compare(file1, file2)
        self.assertEqual(result.returncode, 2)
        self.assertIn("missing CSV variable", result.stdout)

    def test_csv_without_data_is_error(self):
        with tempfile.TemporaryDirectory() as directory:
            file1, file2 = self.write_pair(directory, "time,u\n", "time,u\n")
            result = self.run_compare(file1, file2)
        self.assertEqual(result.returncode, 2)
        self.assertIn("no data rows", result.stdout)

    def test_csv_nonfinite_is_error(self):
        with tempfile.TemporaryDirectory() as directory:
            file1, file2 = self.write_pair(
                directory, "time,u\n0,nan\n", "time,u\n0,nan\n"
            )
            result = self.run_compare(file1, file2)
        self.assertEqual(result.returncode, 2)
        self.assertIn("non-finite", result.stdout)

    def test_csv_matching_text_columns_are_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            file1, file2 = self.write_pair(
                directory,
                "time,label,u\n0,start,1\n",
                "time,label,u\n0,start,1\n",
            )
            result = self.run_compare(file1, file2)
        self.assertEqual(result.returncode, 0)
        self.assertIn("status: SAME", result.stdout)

    def test_csv_cmp_is_authoritative(self):
        with tempfile.TemporaryDirectory() as directory:
            file1, file2 = self.write_pair(
                directory, "time,u\n0,1\n", "time,u\n0,1.001\n"
            )
            comparison = Path(directory) / "test.cmp"
            comparison.write_text("GLOBAL VARIABLES relative 1e-2 floor 0\n    time\n    u\n")
            result = self.run_compare(file1, file2, "--cmp", comparison)
        self.assertEqual(result.returncode, 0)
        self.assertIn("max_abs=0.001", result.stdout)
        self.assertIn("status: PASS", result.stdout)

    def test_csv_cmp_can_select_subset_despite_summary_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            file1, file2 = self.write_pair(
                directory, "time,u,v\n0,1,2\n", "time,u\n0,1\n"
            )
            comparison = Path(directory) / "test.cmp"
            comparison.write_text("GLOBAL VARIABLES relative 1e-6 floor 0\n    time\n    u\n")
            result = self.run_compare(file1, file2, "--cmp", comparison)
        self.assertEqual(result.returncode, 0)
        self.assertIn("missing CSV variable", result.stdout)
        self.assertIn("status: PASS", result.stdout)

    @unittest.skipUnless(EXODUS_A.exists() and EXODUS_B.exists(), "Exodus fixtures unavailable")
    def test_exodus_different(self):
        result = self.run_compare(EXODUS_A, EXODUS_B)
        self.assertEqual(result.returncode, 1)
        self.assertIn("status: DIFFERENT", result.stdout)
        self.assertIn("variable_derivative", result.stdout)
        self.assertIn("is in the second file", result.stdout.lower())

    @unittest.skipUnless(EXODUS_A.exists(), "Exodus fixture unavailable")
    def test_exodus_same(self):
        result = self.run_compare(EXODUS_A, EXODUS_A)
        self.assertEqual(result.returncode, 0)
        self.assertIn("status: SAME", result.stdout)


if __name__ == "__main__":
    unittest.main()

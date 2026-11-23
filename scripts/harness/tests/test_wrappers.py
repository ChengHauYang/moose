import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
CUDA = ROOT / "scripts" / "cuda" / "moose_cuda.sh"
MAC = ROOT / "scripts" / "mac" / "moose.sh"


class WrapperTests(unittest.TestCase):
    def test_both_wrappers_dispatch_to_shared_cli(self):
        for wrapper in (CUDA, MAC):
            text = wrapper.read_text()
            self.assertIn('"$SCRIPT_DIR/../harness/cli.py"', text)
            self.assertIn("compare", text)
            self.assertIn("logstat", text)


if __name__ == "__main__":
    unittest.main()

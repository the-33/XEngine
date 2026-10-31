"""Run the SDL-independent input action timing and callback regression binary."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class InputActionCppTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("g++"), "GCC C++20 is required")
    def test_actions_compile_and_run(self):
        with tempfile.TemporaryDirectory() as temp:
            binary = Path(temp) / "input_actions_test"
            compiler = subprocess.run(
                [
                    "g++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "engine" / "include"),
                    str(ROOT / "tests" / "InputActionsTests.cpp"),
                    str(ROOT / "engine" / "src" / "InputActions.cpp"),
                    "-o", str(binary),
                ],
                cwd=ROOT, capture_output=True, text=True, check=False, timeout=90,
            )
            self.assertEqual(compiler.returncode, 0, compiler.stdout + compiler.stderr)
            result = subprocess.run(
                [str(binary)], cwd=ROOT, capture_output=True, text=True,
                check=False, timeout=20,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()

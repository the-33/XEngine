"""Regression: Behaviour property proxies compile under GCC C++20.

Run from XEngine root:
    python3 -m unittest discover -s tests -p 'test_behaviour_linux_compile.py'

A minimal temporary stand-in is used for SDL_stdinc.h solely because this
translation unit needs the SDLCALL macro via Engine.h. This test does not test
linking, SDL runtime operation or a full game build.
"""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class BehaviourLinuxCompileTest(unittest.TestCase):
    def test_gcc_cxx20_debug_release(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is not available")

        with tempfile.TemporaryDirectory() as td:
            stub = Path(td)
            (stub / "SDL_stdinc.h").write_text(
                "#pragma once\n#define SDLCALL\n", encoding="utf-8"
            )
            for config in ("DEBUG", "RELEASE"):
                with self.subTest(config=config):
                    command = [
                        compiler,
                        "-std=c++20",
                        "-fsyntax-only",
                        f"-DXENGINE_{config}=1",
                        *(["-DNDEBUG"] if config == "RELEASE" else []),
                        "-DXENGINE_TARGET_LINUX=1",
                        f"-I{stub}",
                        f"-I{ROOT / 'engine' / 'include'}",
                        str(ROOT / "engine" / "src" / "Behaviour.cpp"),
                    ]
                    result = subprocess.run(
                        command, capture_output=True, text=True, check=False
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()

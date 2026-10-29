"""Regression: Behaviour property proxies compile under GCC C++20.

Run from XEngine root:
    python3 -m unittest discover -s tests -p 'test_behaviour_linux_compile.py'

Minimal temporary SDL stand-ins validate syntax without requiring the native
SDL2 SDK. This is not a substitute for the real SDL2 compile/link build.
"""

import shutil
import subprocess
import sys
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
            (stub / "SDL.h").write_text(
                "#pragma once\n#include <cstdint>\n"
                "typedef struct SDL_Texture SDL_Texture;\n"
                "typedef struct SDL_Surface SDL_Surface;\n"
                "typedef struct SDL_Renderer SDL_Renderer;\n"
                "typedef struct SDL_Window SDL_Window;\n"
                "typedef union SDL_Event { std::uint32_t type; } SDL_Event;\n"
                "#define SDL_QUIT 0x100\n", encoding="utf-8"
            )
            (stub / "SDL_mixer.h").write_text(
                "#pragma once\ntypedef struct Mix_Chunk Mix_Chunk;\n"
                "typedef struct _Mix_Music Mix_Music;\n", encoding="utf-8"
            )
            (stub / "SDL_ttf.h").write_text(
                "#pragma once\ntypedef struct _TTF_Font TTF_Font;\n", encoding="utf-8"
            )
            for config in ("DEBUG", "RELEASE"):
                with self.subTest(config=config):
                    command = [
                        compiler,
                        "-std=c++20",
                        "-fsyntax-only",
                        f"-DXENGINE_{config}=1",
                        *(["-DNDEBUG"] if config == "RELEASE" else []),
                        "-DXENGINE_TARGET_WINDOWS=1" if sys.platform == 'win32'
                        else "-DXENGINE_TARGET_LINUX=1",
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

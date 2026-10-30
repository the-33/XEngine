"""Check the bundled benchmark API; gameplay examples are tested manually."""

from __future__ import annotations

from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
BENCHMARK = "XEngine Benchmark"
BUILD_FLAGS = {
    "DEBUG": ("-DXENGINE_DEBUG=1",),
    "RELEASE": ("-DNDEBUG", "-DXENGINE_RELEASE=1"),
}
SERVICE_ACCESSORS = ("Assets", "Scenes", "Renderer", "Sound", "Input",
                     "UI", "Window", "Time", "Physics", "Random", "CurrentCamera")


class BenchmarkApiTests(unittest.TestCase):
    def test_benchmark_uses_typed_service_accessors_correctly(self):
        invalid = re.compile(
            r"\bauto\s*\*\s*\w+\s*=\s*(?:" + "|".join(SERVICE_ACCESSORS) + r")\s*;"
        )
        game = ROOT / "games" / BENCHMARK
        self.assertTrue((game / "src/main.cpp").is_file())
        for path in game.rglob("*"):
            if path.suffix not in (".h", ".hpp", ".cpp"):
                continue
            with self.subTest(path=path.relative_to(ROOT)):
                self.assertNotRegex(path.read_text(encoding="utf-8"), invalid)

    @unittest.skipUnless(shutil.which("g++") and shutil.which("pkg-config"),
                         "G++ and SDL2 pkg-config metadata are required")
    def test_debug_and_release_benchmark_entry_point_compiles(self):
        flags = subprocess.run(
            ["pkg-config", "--cflags", "sdl2", "SDL2_image", "SDL2_ttf", "SDL2_mixer"],
            capture_output=True, text=True, check=False,
        )
        if flags.returncode:
            self.skipTest("SDL2 development headers are not installed")
        with tempfile.TemporaryDirectory() as temporary:
            generated = Path(temporary) / "GameBuild.generated.h"
            generated.write_text(
                "#pragma once\n#include <string>\n"
                "namespace XEngineGameBuild {\n"
                "const char* DevelopmentAssetsPath() noexcept;\n"
                "std::string EmbeddedEngineConfig();\n}\n",
                encoding="utf-8",
            )
            game = ROOT / "games" / BENCHMARK
            includes = [ROOT / "engine/include", generated.parent, game / "src",
                        game / "assets", *(p for p in (game / "assets").rglob("*")
                                            if p.is_dir())]
            for configuration, configuration_flags in BUILD_FLAGS.items():
                with self.subTest(configuration=configuration):
                    result = subprocess.run(
                        ["g++", "-std=c++20", "-fsyntax-only",
                         *configuration_flags,
                         *[f"-I{p}" for p in includes],
                         *shlex.split(flags.stdout), str(game / "src/main.cpp")],
                        cwd=ROOT, capture_output=True, text=True,
                        check=False, timeout=120,
                    )
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()

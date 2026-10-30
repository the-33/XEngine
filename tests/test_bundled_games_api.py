"""Keep all three bundled games buildable against XEngine's current API."""

from __future__ import annotations

from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
GAMES = ("Blob's Quest", "My Princess Run", "XEngine Benchmark")
SERVICE_ACCESSORS = ("Assets", "Scenes", "Renderer", "Sound", "Input",
                     "UI", "Window", "Time", "Physics", "Random", "CurrentCamera")


class BundledGameApiTests(unittest.TestCase):
    def test_all_bundled_games_use_typed_service_accessors_correctly(self):
        invalid = re.compile(
            r"\bauto\s*\*\s*\w+\s*=\s*(?:" + "|".join(SERVICE_ACCESSORS) + r")\s*;"
        )
        for name in GAMES:
            game = ROOT / "games" / name
            self.assertTrue((game / "src/main.cpp").is_file())
            for path in game.rglob("*"):
                if path.suffix not in (".h", ".hpp", ".cpp"):
                    continue
                with self.subTest(game=name, path=path.relative_to(ROOT)):
                    self.assertNotRegex(path.read_text(encoding="utf-8"), invalid)
        blob_init = (ROOT / "games" / GAMES[0] / "src/InitCallback.hpp").read_text()
        self.assertIn("auto* assets = Assets.get();", blob_init)

    @unittest.skipUnless(shutil.which("g++") and shutil.which("pkg-config"),
                         "G++ and SDL2 pkg-config metadata are required")
    def test_debug_and_release_entry_points_compile_against_current_engine(self):
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
            for game_name in GAMES:
                game = ROOT / "games" / game_name
                includes = [ROOT / "engine/include", generated.parent, game / "src",
                            game / "assets", *(p for p in (game / "assets").rglob("*")
                                                if p.is_dir())]
                for configuration in ("DEBUG", "RELEASE"):
                    with self.subTest(game=game_name, configuration=configuration):
                        result = subprocess.run(
                            ["g++", "-std=c++20", "-fsyntax-only",
                             f"-DXENGINE_{configuration}=1",
                             *[f"-I{p}" for p in includes],
                             *shlex.split(flags.stdout), str(game / "src/main.cpp")],
                            cwd=ROOT, capture_output=True, text=True,
                            check=False, timeout=120,
                        )
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()

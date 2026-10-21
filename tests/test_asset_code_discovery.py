"""Regression tests for path-independent C++ sources and headers under assets/."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xengine_dynamic_assets_build', ROOT / 'tools' / 'build.py')
assert spec is not None and spec.loader is not None
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


class DynamicAssetsTests(unittest.TestCase):
    def test_include_root_and_all_header_directories(self):
        with tempfile.TemporaryDirectory() as tmp:
            assets = Path(tmp) / 'assets'
            nested = assets / 'my_subsystem' / 'characters'
            nested.mkdir(parents=True)
            (nested / 'Player.hpp').write_text('#pragma once\n')
            (assets / 'scenes').mkdir()
            (assets / 'scenes' / 'Level1.hpp').write_text('#pragma once\n')
            # Non-C++ asset folders do not need to be compiler include paths.
            (assets / 'textures').mkdir()
            (assets / 'textures' / 'player.png').write_bytes(b'example')
            dirs = build.asset_include_dirs(assets)
            self.assertEqual(dirs[0], assets)
            self.assertIn(nested, dirs)
            self.assertIn(assets / 'scenes', dirs)
            self.assertNotIn(assets / 'textures', dirs)

    def test_cpp_sources_anywhere_below_assets(self):
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp)
            (game / 'src').mkdir()
            (game / 'src' / 'main.cpp').write_text('int main() { return 0; }')
            nested = game / 'assets' / 'custom' / 'deep'
            nested.mkdir(parents=True)
            extra = nested / 'Spawner.cpp'
            extra.write_text('void Spawn() {}')
            generated = game / 'generated.cpp'
            sources = build.game_sources(game, 'windows', generated)
            self.assertIn(extra, sources)
            self.assertIn(game / 'src' / 'main.cpp', sources)

    def test_all_headers_in_assets_are_tracked(self):
        with tempfile.TemporaryDirectory() as tmp:
            assets = Path(tmp) / 'assets'
            nested = assets / 'custom' / 'deep'
            nested.mkdir(parents=True)
            header = nested / 'Movement.hpp'
            header.write_text('class Movement {};')
            self.assertIn(header, build.all_headers([assets]))
            first = build.digest(build.all_headers([assets]), [])
            header.write_text('class Movement { int speed; };')
            second = build.digest(build.all_headers([assets]), [])
            self.assertNotEqual(first, second)

    @unittest.skipUnless(shutil.which('g++'), 'g++ not installed in testing host')
    def test_compiler_accepts_relative_and_basename_includes(self):
        with tempfile.TemporaryDirectory() as tmp:
            game = Path(tmp)
            assets = game / 'assets'
            folder = assets / 'custom' / 'characters'
            folder.mkdir(parents=True)
            (folder / 'Player.hpp').write_text('#pragma once\ninline int getHealth() {return 3;}\n')
            source = game / 'main.cpp'
            source.write_text('#include "custom/characters/Player.hpp"\n'
                              '#include "Player.hpp"\n'
                              'int main() {return getHealth() - 3;}\n')
            flags = ['-I' + str(p) for p in build.asset_include_dirs(assets)]
            exe = game / 'game'
            subprocess.run([shutil.which('g++'), *flags, str(source), '-o', str(exe)], check=True,
                           capture_output=True, text=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()

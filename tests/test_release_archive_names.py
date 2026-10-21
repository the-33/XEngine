"""Release archives keep the game/version name and ship only staged files."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xengine_release_archives', ROOT / 'tools/build.py')
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)


class ReleaseArchiveNamesTests(unittest.TestCase):
    def make_game(self, root: Path, name: str = "Blob's Quest") -> tuple[Path, Path]:
        game = root / name
        (game / 'assets' / 'textures').mkdir(parents=True)
        (game / 'assets' / 'textures' / 'sprite.png').write_bytes(b'image')
        (game / 'assets' / 'scripts').mkdir(parents=True)
        (game / 'assets' / 'scripts' / 'Gameplay.hpp').write_text('// not for distribution')
        (game / 'config.json').write_text(json.dumps({'project': {'name': name}}))
        return game, root / name

    def test_windows_zip_has_name_version_and_all_runtime_files(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            game, binary_base = self.make_game(root)
            binary = binary_base.with_name(binary_base.name + '.exe')
            binary.write_bytes(b'PE binary')

            def fake_windows_runtime(stage, _binary, tooling=None):
                (stage / 'SDL2.dll').write_bytes(b'SDL2')
                (stage / 'libstdc++-6.dll').write_bytes(b'C++ runtime')
                return ['SDL2.dll', 'libstdc++-6.dll']

            for cross in (False, True):
                with self.subTest(cross=cross):
                    tc = b.Toolchain('g++', 'ar', 'pkg-config', [], [], 'v', cross=cross)
                    with mock.patch.object(b, 'package_windows_libs', side_effect=fake_windows_runtime):
                        dist = b.package_game(game, game.name, '0.1.0', 'windows', 'x86_64',
                                              binary, tooling=tc)
                    archive = dist.parent / "Blob's Quest - 0.1.0.zip"
                    self.assertTrue(archive.is_file())
                    with zipfile.ZipFile(archive) as z:
                        self.assertIsNone(z.testzip())
                        names = set(z.namelist())
                        for expected in ("Blob's Quest.exe", 'SDL2.dll',
                                         'libstdc++-6.dll',
                                         'textures/sprite.png', 'version.json',
                                         'textures/'):
                            self.assertIn(expected, names)
                        self.assertNotIn('scripts/Gameplay.hpp', names)
                        self.assertFalse(any(x.startswith('0.1.0/') for x in names))
                        self.assertEqual(z.read('SDL2.dll'), b'SDL2')
                        self.assertEqual(z.read('version.json'),
                                         (dist / 'version.json').read_bytes())

    def test_windows_zip_is_replaced_when_release_is_repackaged(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            game, binary_base = self.make_game(root, name='Game Two')
            binary = binary_base.with_name(binary_base.name + '.exe')
            binary.write_bytes(b'first executable')
            with mock.patch.object(b, 'package_windows_libs', return_value=[]):
                result = b.package_game(game, game.name, '3.2.1', 'windows', 'x86_64', binary)
                archive = result.parent / 'Game Two - 3.2.1.zip'
                first = archive.read_bytes()
                (game / 'assets' / 'textures' / 'sprite.png').unlink()
                binary.write_bytes(b'second executable')
                result = b.package_game(game, game.name, '3.2.1', 'windows', 'x86_64', binary)
            with zipfile.ZipFile(archive) as z:
                self.assertNotIn('textures/sprite.png', z.namelist())
                self.assertEqual(z.read('Game Two.exe'), b'second executable')
            self.assertNotEqual(archive.read_bytes(), first)
            self.assertFalse(archive.with_name(archive.name + '.tmp').exists())

    def test_linux_tar_name_preserves_game_apostrophe_and_executable_bits(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            game, _ = self.make_game(root)
            (root / '_build').mkdir()
            binary = root / '_build' / game.name
            binary.write_bytes(b'ELF')
            tc = b.Toolchain('clang++', 'llvm-ar', 'pkg', [], [], 'v', cross=True)
            with mock.patch.object(b, 'package_linux_libs_cross',
                                   side_effect=lambda stage, out, _: b.write_linux_run(stage, out.name)):
                result = b.package_game(game, game.name, '2.3.4', 'linux', 'x86_64',
                                        binary, tooling=tc)
            archive = result.parent / "Blob's Quest - 2.3.4.tar.gz"
            with tarfile.open(archive, 'r:gz') as z:
                for filename in ("Blob's Quest", 'run.sh',
                                 'setup_desktop.sh'):
                    self.assertEqual(z.getmember(filename).mode & 0o777, 0o755)
                self.assertEqual(z.getmember('textures/sprite.png').mode & 0o777,
                                 0o644)
                self.assertNotIn('scripts/Gameplay.hpp', z.getnames())
                self.assertFalse(any(x.startswith('2.3.4/') for x in z.getnames()))


    def test_release_archives_extract_files_directly_into_destination(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            release = root / 'dist' / 'linux-x86_64' / '4.0.0'
            (release / 'textures').mkdir(parents=True)
            (release / 'textures' / 'sprite.png').write_bytes(b'PNG')
            (release / 'Game').write_bytes(b'ELF')
            (release / 'run.sh').write_text('#!/bin/sh\n')
            tar = b.write_linux_release_archive(release, 'Game', 'My Game')
            target = root / 'unpacked-linux'
            target.mkdir()
            with tarfile.open(tar, 'r:gz') as f:
                f.extractall(target, filter='data')
            self.assertEqual((target / 'Game').read_bytes(), b'ELF')
            self.assertTrue((target / 'textures' / 'sprite.png').is_file())
            self.assertFalse((target / '4.0.0').exists())
            self.assertEqual((release / 'Game').read_bytes(), b'ELF')

            windows = root / 'dist' / 'windows-x86_64' / '4.0.0'
            (windows / 'fonts').mkdir(parents=True)
            (windows / 'Game.exe').write_bytes(b'PE')
            (windows / 'fonts' / 'font.ttf').write_bytes(b'TTF')
            zip_path = b.write_windows_release_archive(windows, 'My Game')
            target = root / 'unpacked-windows'
            target.mkdir()
            with zipfile.ZipFile(zip_path) as f:
                f.extractall(target)
            self.assertEqual((target / 'Game.exe').read_bytes(), b'PE')
            self.assertTrue((target / 'fonts' / 'font.ttf').is_file())
            self.assertFalse((target / '4.0.0').exists())


if __name__ == '__main__':
    unittest.main()

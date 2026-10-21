"""Linux PNG desktop launcher: path validation, staging and relocation tests."""
from __future__ import annotations

import importlib.util
import json
import os
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xengine_linux_icons', ROOT / 'tools' / 'build.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def write_png(path: Path, size: int = 256) -> None:
    """Produce a small valid RGBA PNG using Python stdlib only."""
    path.parent.mkdir(parents=True, exist_ok=True)
    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    scanlines = b''.join(b'\x00' + b'\xff\xff\xff\xff' * size for _ in range(size))
    ihdr = struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr) +
                     chunk(b'IDAT', zlib.compress(scanlines)) + chunk(b'IEND', b''))


def fake_linux_libs(stage: Path, output: Path) -> None:
    shell = stage / 'run.sh'
    shell.write_text('#!/usr/bin/env bash\ncd "$(dirname "$0")"\nexec "./' + output.name + '" "$@"\n')
    shell.chmod(0o755)


class LinuxDesktopIconTests(unittest.TestCase):
    def test_icon_schema_and_windows_compatibility(self):
        with tempfile.TemporaryDirectory() as td:
            game = Path(td) / 'My Princess Run'
            png = game / 'assets' / 'icons' / 'princess.png'
            write_png(png)
            ico = game / 'assets' / 'icons' / 'princess.ico'
            ico.write_bytes(b'\x00\x00\x01\x00\x01\x00' +
                            b'\x10\x10\x00\x00\x01\x00\x20\x00\x01\x00\x00\x00\x16\x00\x00\x00X')
            cfg = {'project': {'icon': {'windows': 'assets/icons/princess.ico',
                                        'linux': 'assets/icons/princess.png'}}}
            self.assertEqual(mod.project_icon(game, cfg, 'linux'), png.resolve())
            self.assertEqual(mod.project_icon(game, cfg, 'windows'), ico.resolve())
            self.assertIsNone(mod.project_icon(game, {'project': {'icon': 'assets/icons/princess.ico'}}, 'linux'))
            self.assertIsNone(mod.project_icon(game, {'project': {'icon': {'linux': None, 'windows': None}}}, 'linux'))

    def test_linux_icon_rejects_invalid_paths_and_png(self):
        with tempfile.TemporaryDirectory() as td:
            game = Path(td) / 'Demo'
            write_png(game / 'assets/icons/good.png')
            for bad in ('../outside.png', 'C:/foo.png', '/tmp/foo.png',
                        'assets/icons/foo.ico', 'assets/icons/missing.png', 41):
                with self.subTest(bad=bad), self.assertRaises(mod.BuildError):
                    mod.project_icon(game, {'project': {'icon': {'linux': bad}}}, 'linux')
            corrupted = game / 'assets/icons/bad.png'
            corrupted.write_bytes(b'not actually a png')
            with self.assertRaisesRegex(mod.BuildError, 'Invalid Linux PNG'):
                mod.project_icon(game, {'project': {'icon': {'linux': 'assets/icons/bad.png'}}}, 'linux')
            with self.assertRaisesRegex(mod.BuildError, 'unsupported icon platforms'):
                mod.project_icon(game, {'project': {'icon': {'android': 'assets/icons/good.png'}}}, 'linux')
            outside = Path(td) / 'outside.png'
            write_png(outside)
            (game / 'assets/icons/link.png').symlink_to(outside)
            with self.assertRaisesRegex(mod.BuildError, 'outside'):
                mod.project_icon(game, {'project': {'icon': {'linux': 'assets/icons/link.png'}}}, 'linux')

    def test_package_linux_icon_and_refresh_after_moving(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td) / "Game's Development"
            game = root / 'My Princess Run'
            write_png(game / 'assets' / 'custom_icons' / 'my princess.png', 32)
            (game / 'assets/textures').mkdir(parents=True)
            (game / 'assets/textures/ground.png').write_bytes(b'texture')
            (game / 'assets/config').mkdir()
            (game / 'assets/config/engine.json').write_text('{}')
            (game / 'assets/scripts').mkdir()
            (game / 'assets/scripts/script.hpp').write_text('// c++')
            (game / 'config.json').write_text(json.dumps({'project': {
                'name': 'My Princess Run', 'version': '0.1.0',
                'icon': {'windows': None, 'linux': 'assets/custom_icons/my princess.png'}
            }}))
            binary = root / '_bin' / 'My Princess Run'
            binary.parent.mkdir(parents=True, exist_ok=True)
            binary.parent.mkdir(exist_ok=True, parents=True)
            binary.write_bytes(b'ELF-test')
            with mock.patch.object(mod, 'package_linux_libs', side_effect=fake_linux_libs):
                result = mod.package_game(game, 'My Princess Run', '0.1.0', 'linux', 'x86_64', binary)
            icon_path = result / 'icons' / (mod.linux_desktop_id('My Princess Run') + '.png')
            self.assertEqual(icon_path.read_bytes(), (game / 'assets/custom_icons/my princess.png').read_bytes())
            self.assertFalse((result / 'custom_icons/my princess.png').exists())
            self.assertFalse((result / 'config/engine.json').exists())
            self.assertFalse((result / 'scripts/script.hpp').exists())
            self.assertTrue((result / 'textures/ground.png').exists())
            desktop = result / 'My Princess Run.desktop'
            self.assertIn('Exec="' + str(result / 'run.sh') + '"', desktop.read_text())
            self.assertIn('Icon=' + str(icon_path), desktop.read_text())
            self.assertTrue(os.access(result / 'setup_desktop.sh', os.X_OK))
            # Relocating a release does not break run.sh. Running setup again
            # updates absolute desktop paths and optionally installs a shortcut.
            moved = Path(td) / "Moved Princess's Game"
            shutil.move(result, moved)
            xdg = Path(td) / 'xdg data'
            env = {**os.environ, 'XDG_DATA_HOME': str(xdg)}
            subprocess.run(['bash', str(moved / 'setup_desktop.sh'), '--install'],
                           env=env, check=True, stdout=subprocess.PIPE, text=True)
            new_desktop = (moved / 'My Princess Run.desktop').read_text()
            self.assertIn('Exec="' + str(moved / 'run.sh') + '"', new_desktop)
            self.assertIn('Icon=' + str(moved / 'icons' / icon_path.name), new_desktop)
            self.assertNotIn(str(result / 'run.sh'), new_desktop)
            installed = xdg / 'applications' / (mod.linux_desktop_id('My Princess Run') + '.desktop')
            self.assertEqual(installed.read_text(), new_desktop)

    def test_linux_release_without_icon_has_desktop_launcher(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            game = root / 'NoIcon'
            (game / 'assets/config').mkdir(parents=True)
            (game / 'assets/config/engine.json').write_text('{}')
            (game / 'config.json').write_text('{"project":{"icon":null}}')
            binary = root / '_bin' / 'NoIcon'
            binary.parent.mkdir(exist_ok=True, parents=True)
            binary.write_bytes(b'ELF-test')
            with mock.patch.object(mod, 'package_linux_libs', side_effect=fake_linux_libs):
                result = mod.package_game(game, 'NoIcon', '0.1.0', 'linux', 'x86_64', binary)
            self.assertIn('Icon=applications-games', (result / 'NoIcon.desktop').read_text())
            self.assertTrue((result / 'run.sh').exists())

    def test_blank_game_template_has_cross_platform_icon_fields(self):
        config = json.loads((ROOT / 'tools/templates/BlankGame/config.json').read_text())
        self.assertEqual(config['project']['icon'], {
            'windows': 'assets/icons/xengine.ico',
            'linux': 'assets/icons/xengine.png',
        })


if __name__ == '__main__':
    unittest.main()

"""Regressions for default game icons, template synchronization and build preflight."""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock
import zlib

ROOT = Path(__file__).resolve().parents[1]


def load(name: str, path: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


sync = load('xengine_icon_sync_v1611', 'tools/sync_game_icons.py')
build = load('xengine_build_icons_v1611', 'tools/build.py')


def png_512(pixel: bytes = b'\xff\x00\x00\xff') -> bytes:
    def chunk(tag, content):
        return struct.pack('>I', len(content)) + tag + content + struct.pack('>I', zlib.crc32(tag + content))
    rows = (b'\x00' + pixel * 512) * 512
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 512, 512, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')


def ico(payload: bytes = b'X') -> bytes:
    return (b'\x00\x00\x01\x00\x01\x00' +
            b'\x10\x10\x00\x00\x01\x00\x20\x00\x01\x00\x00\x00\x16\x00\x00\x00' + payload)


class IconSyncTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.template = self.root / sync.TEMPLATE
        self.template.mkdir(parents=True)
        (self.template / 'xengine_icon.ico').write_bytes(ico())
        (self.template / 'xengine_icon.png').write_bytes(png_512())
        self.game = self.root / 'games' / "Princess's Project"
        self.game.mkdir(parents=True)
        (self.game / 'config.json').write_text('{"project":{"name":"Princess\'s Project"}}')
        self.icons = self.game / 'assets' / 'icons'
        self.icons.mkdir(parents=True)

    def test_missing_creates_then_idempotent_and_check(self):
        self.assertEqual(sync.sync(self.root, check=True, quiet=True), 1)
        self.assertFalse((self.icons / 'xengine_icon.ico').exists())
        self.assertEqual(sync.sync(self.root, quiet=True), 0)
        self.assertEqual(sync.sync(self.root, check=True, quiet=True), 0)
        for name in sync.ICON_NAMES:
            self.assertEqual((self.icons / name).read_bytes(), (self.template / name).read_bytes())

    def test_changed_default_is_replaced_with_backup_custom_untouched(self):
        self.icons.joinpath('xengine_icon.ico').write_bytes(ico(b'OLD'))
        self.icons.joinpath('xengine_icon.png').write_bytes(png_512(b'\xff\x11\x00\xff'))
        self.icons.joinpath('personal.ico').write_bytes(b'KEEP')
        self.assertEqual(sync.sync(self.root, quiet=True), 0)
        self.assertEqual((self.icons / 'personal.ico').read_bytes(), b'KEEP')
        for name in sync.ICON_NAMES:
            self.assertEqual((self.icons / name).read_bytes(), (self.template / name).read_bytes())
            backups = list((self.root / sync.BACKUPS).rglob(name))
            self.assertEqual(len(backups), 1)
        self.assertEqual(sync.sync(self.root, check=True, quiet=True), 0)

    def test_invalid_template_blocks_changes(self):
        (self.template / 'xengine_icon.png').unlink()
        with self.assertRaisesRegex(sync.IconSyncError, 'missing'):
            sync.sync(self.root, quiet=True)
        self.assertFalse((self.icons / 'xengine_icon.ico').exists())
        (self.template / 'xengine_icon.png').write_bytes(png_512()[:28])
        with self.assertRaises(sync.IconSyncError):
            sync.sync(self.root, quiet=True)

    def test_no_symlink_overwrites(self):
        outside = self.root / 'outside.ico'
        outside.write_bytes(b'KEEP')
        try:
            (self.icons / 'xengine_icon.ico').symlink_to(outside)
        except OSError:
            self.skipTest('Windows symlink creation unavailable')
        with self.assertRaisesRegex(sync.IconSyncError, 'symlink'):
            sync.sync(self.root, quiet=True)
        self.assertEqual(outside.read_bytes(), b'KEEP')

    def test_explicit_game_does_not_modify_others(self):
        second = self.root / 'games' / 'Other'
        second.mkdir()
        (second / 'config.json').write_text('{}')
        self.assertEqual(sync.sync(self.root, [self.game.name], quiet=True), 0)
        self.assertFalse((second / 'assets/icons/xengine_icon.ico').exists())

    def test_cli_check_and_apply(self):
        self.assertEqual(sync.main(['--root', str(self.root), '--check']), 1)
        self.assertEqual(sync.main(['--root', str(self.root), '--apply']), 0)
        self.assertEqual(sync.main(['--root', str(self.root), '--check']), 0)

    def test_build_default_and_missing_custom_fallback(self):
        sync.sync(self.root, quiet=True)
        cfg = {'project': {'icon': {'windows': 'default', 'linux': 'default'}}}
        for target, filename in [('windows', 'xengine_icon.ico'), ('linux', 'xengine_icon.png')]:
            self.assertEqual(build.project_icon(self.game, cfg, target), (self.icons / filename).resolve())
        self.assertEqual(build.project_icon(self.game, {'project': {'icon': 'default'}}, 'linux'),
                         (self.icons / 'xengine_icon.png').resolve())
        with mock.patch.object(build, 'log') as logger:
            path = build.project_icon(self.game, {'project': {'icon': {'windows': 'assets/icons/custom.ico'}}}, 'windows')
        self.assertEqual(path, (self.icons / 'xengine_icon.ico').resolve())
        self.assertIn('WARNING', logger.call_args[0][0])
        self.assertEqual(build.project_icon(self.game, {'project': {'icon': {'linux': 'assets/icons/not-here.png'}}}, 'linux'),
                         (self.icons / 'xengine_icon.png').resolve())

    def test_old_logo_files_are_not_touched_by_new_icon_sync(self):
        legacy_ico = self.icons / 'xengine_logo.ico'
        legacy_png = self.icons / 'xengine_logo.png'
        legacy_ico.write_bytes(b'USER LEGACY ICO')
        legacy_png.write_bytes(b'USER LEGACY PNG')
        self.assertEqual(sync.sync(self.root, quiet=True), 0)
        self.assertEqual(legacy_ico.read_bytes(), b'USER LEGACY ICO')
        self.assertEqual(legacy_png.read_bytes(), b'USER LEGACY PNG')
        for name in sync.ICON_NAMES:
            self.assertEqual((self.icons / name).read_bytes(), (self.template / name).read_bytes())

    def test_invalid_custom_not_silently_replaced(self):
        sync.sync(self.root, quiet=True)
        (self.icons / 'bad.ico').write_bytes(b'bad')
        with self.assertRaisesRegex(build.BuildError, 'Invalid Windows ICO'):
            build.project_icon(self.game, {'project': {'icon': 'assets/icons/bad.ico'}})
        for path in ('../outside.ico', 'C:/outside.ico', 'assets/icons/something.png'):
            with self.subTest(path=path), self.assertRaises(build.BuildError):
                build.project_icon(self.game, {'project': {'icon': path}})
        self.assertIsNone(build.project_icon(self.game, {'project': {'icon': None}}))

    def test_prebuild_invokes_all_three_syncs_and_aborts(self):
        calls=[]
        with mock.patch.object(build, 'command', side_effect=lambda args, **kwargs: calls.append(args)):
            build.sync_before_game_build(self.game.name)
        self.assertEqual(len(calls), 3)
        self.assertTrue(calls[0][1].endswith('sync_game_configs.py'))
        self.assertTrue(calls[1][1].endswith('sync_engine_configs.py'))
        self.assertTrue(calls[2][1].endswith('sync_game_icons.py'))
        with mock.patch.object(build, 'command', side_effect=build.BuildError('compiler preflight failed')) as runner:
            with self.assertRaises(build.BuildError):
                build.sync_before_game_build(self.game.name)
            self.assertEqual(runner.call_count, 1)


if __name__ == '__main__':
    unittest.main()

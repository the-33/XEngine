"""POSIX executable permissions must survive Windows-to-Linux Releases."""
from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import stat
import subprocess
import tarfile
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xengine_release_perms', ROOT / 'tools/build.py')
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)


class LinuxReleasePermissionsTests(unittest.TestCase):
    def package_fake_game(self, root: Path, game_name: str = "Blob's Quest") -> Path:
        game = root / game_name
        (game / 'assets' / 'textures').mkdir(parents=True)
        (game / 'assets' / 'textures' / 'sprite.png').write_bytes(b'PNG')
        (game / 'config.json').write_text(json.dumps({'project': {
            'name': game_name, 'version': '0.1.0', 'icon': None
        }}))
        output = root / '_build' / game_name
        output.parent.mkdir()
        output.write_text('#!/usr/bin/env bash\nprintf "launched:%s\\n" "$1"\n')
        output.chmod(0o644)  # Simulates an executable created on Windows/NTFS.
        tc = b.Toolchain('clang++', 'llvm-ar', 'pkg-config', [], [], 'test',
                         cross=True, readelf='llvm-readelf', sysroot=root)
        with mock.patch.object(b, 'package_linux_libs_cross',
                               side_effect=lambda stage, exe, _: b.write_linux_run(stage, exe.name)):
            return b.package_game(game, game_name, '0.1.0', 'linux', 'x86_64', output,
                                  tooling=tc)

    def test_cross_release_archive_records_linux_execute_bits(self):
        with tempfile.TemporaryDirectory() as td:
            result = self.package_fake_game(Path(td))
            archive = result.parent / "Blob's Quest - 0.1.0.tar.gz"
            self.assertTrue(archive.is_file())
            with tarfile.open(archive, 'r:gz') as tf:
                names = ["Blob's Quest", 'run.sh',
                         'setup_desktop.sh']
                for name in names:
                    with self.subTest(name=name):
                        self.assertEqual(tf.getmember(name).mode & 0o777, 0o755)
                self.assertEqual(tf.getmember('textures/sprite.png').mode & 0o777,
                                 0o644)
                self.assertFalse(any(name.endswith('.desktop') for name in tf.getnames()))
                self.assertFalse(any(name.startswith('0.1.0/') for name in tf.getnames()))

    @unittest.skipUnless(os.name == 'posix', 'Requires chmod and bash on POSIX')
    def test_run_script_repairs_executable_after_windows_file_copy(self):
        with tempfile.TemporaryDirectory() as td:
            result = self.package_fake_game(Path(td))
            game = result / "Blob's Quest"
            game.chmod(0o644)  # Simulate transferring individual files from NTFS.
            self.assertFalse(os.access(game, os.X_OK))
            process = subprocess.run(['bash', str(result / 'run.sh'), 'argument'],
                                     cwd=td, text=True, capture_output=True)
            self.assertEqual(process.returncode, 0, process.stderr)
            self.assertIn('launched:argument', process.stdout)
            self.assertTrue(os.access(game, os.X_OK))

    @unittest.skipUnless(os.name == 'posix', 'Requires chmod and bash on POSIX')
    def test_desktop_setup_repairs_run_script_permissions(self):
        with tempfile.TemporaryDirectory() as td:
            result = self.package_fake_game(Path(td))
            wrapper = result / 'run.sh'
            wrapper.chmod(0o644)
            self.assertFalse(os.access(wrapper, os.X_OK))
            proc = subprocess.run(['bash', str(result / 'setup_desktop.sh')],
                                  capture_output=True, text=True, cwd=td)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            self.assertTrue(os.access(wrapper, os.X_OK))
            self.assertTrue((result / "Blob's Quest.desktop").exists())


if __name__ == '__main__':
    unittest.main()

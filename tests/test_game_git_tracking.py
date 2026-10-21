"""Private-by-default games, root-level opt-ins, and staging regression tests."""
from __future__ import annotations

import contextlib
import importlib.util
import io
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]


def load_tool():
    spec = importlib.util.spec_from_file_location('xengine_sync_game_gitignore',
                                                   ROOT / 'tools/sync_game_gitignore.py')
    tool = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tool)
    return tool


class GitGamePolicyTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.games = self.root / 'games'
        self.games.mkdir()
        for name in ('XEngine Benchmark', "Blob's Quest", 'My Princess Run'):
            p = self.games / name
            p.mkdir()
            (p / 'src').mkdir()
            (p / 'src/main.cpp').write_text('// game source\n', encoding='utf-8')
        (self.games / "Blob's Quest/assets/audio").mkdir(parents=True)
        (self.games / "Blob's Quest/assets/audio/.gitkeep").touch()
        (self.games / 'My Princess Run/assets/textures').mkdir(parents=True)
        (self.games / 'My Princess Run/assets/textures/.gitkeep').touch()
        self.tool = load_tool()
        self.patchers = (mock.patch.object(self.tool, 'ROOT', self.root),
                         mock.patch.object(self.tool, 'GAMES', self.games),
                         mock.patch.object(self.tool, 'IGNORE', self.games / '.gitignore'))
        for patcher in self.patchers:
            patcher.start()
            self.addCleanup(patcher.stop)

    def call(self, args=None):
        with contextlib.redirect_stdout(io.StringIO()):
            return self.tool.main(args or [])

    def test_defaults_benchmark_only_despite_legacy_asset_placeholders(self):
        self.assertEqual(self.call(), 0)
        self.assertEqual(self.tool.included_games(), ['XEngine Benchmark'])
        self.assertIn('!/XEngine Benchmark/', (self.games / '.gitignore').read_text())
        self.assertNotIn("!/Blob's Quest/", (self.games / '.gitignore').read_text())
        self.assertNotIn('!/My Princess Run/', (self.games / '.gitignore').read_text())
        self.assertEqual(self.call(['--check']), 0)
        (self.games / "Blob's Quest/.gitkeep").touch()
        self.assertEqual(self.call(['--check']), 1)
        self.assertEqual(self.call(), 0)
        self.assertIn("!/Blob's Quest/", (self.games / '.gitignore').read_text())

    @unittest.skipUnless(shutil.which('git'), 'Git required for integration test')
    def test_git_index_opt_in_and_out_keeps_private_files(self):
        def git(*args):
            return subprocess.run(['git', *args], cwd=self.root, check=True,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                  text=True).stdout
        git('init', '-q')
        git('config', 'user.email', 'xengine-test@example.test')
        git('config', 'user.name', 'XEngine test')
        self.assertEqual(self.call(), 0)
        git('add', '-A')
        listed = git('ls-files').splitlines()
        self.assertIn('games/XEngine Benchmark/src/main.cpp', listed)
        self.assertNotIn("games/Blob's Quest/src/main.cpp", listed)
        self.assertNotIn('games/My Princess Run/src/main.cpp', listed)
        # A previously versioned private game must be removed only from the index.
        git('add', '-f', '--', "games/Blob's Quest/src/main.cpp")
        self.assertIn("games/Blob's Quest/src/main.cpp", git('ls-files').splitlines())
        self.assertEqual(self.call(['--stage']), 0)
        self.assertNotIn("games/Blob's Quest/src/main.cpp", git('ls-files').splitlines())
        self.assertTrue((self.games / "Blob's Quest/src/main.cpp").exists())
        # The root-level marker enables the entire project, not just itself.
        (self.games / "Blob's Quest/.gitkeep").touch()
        self.assertEqual(self.call(['--stage']), 0)
        listed = git('ls-files').splitlines()
        self.assertIn("games/Blob's Quest/.gitkeep", listed)
        self.assertIn("games/Blob's Quest/src/main.cpp", listed)
        self.assertIn('games/XEngine Benchmark/src/main.cpp', listed)
        self.assertNotIn('games/My Princess Run/src/main.cpp', listed)
        (self.games / "Blob's Quest/.gitkeep").unlink()
        self.assertEqual(self.call(['--stage']), 0)
        self.assertNotIn("games/Blob's Quest/src/main.cpp", git('ls-files').splitlines())
        self.assertTrue((self.games / "Blob's Quest/src/main.cpp").exists())

    def test_escaped_names_are_literal(self):
        self.assertEqual(self.tool.escape_gitignore('Game [alpha]!'), r'Game \[alpha\]\!')
        self.assertEqual(self.tool.escape_gitignore('Some Game'), 'Some Game')


if __name__ == '__main__':
    unittest.main()

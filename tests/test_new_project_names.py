"""Regression tests for user-facing project names."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xengine_new_project', ROOT / 'tools' / 'new_project.py')
new_project = importlib.util.module_from_spec(spec)
spec.loader.exec_module(new_project)


class ProjectNamingTests(unittest.TestCase):
    def test_accept_names_with_spaces_and_apostrophes(self):
        for name in ("Blob's Quest", 'My Princess Run', 'Game-2', 'Juego_3', 'Aventura Española', 'Player’s Game'):
            with self.subTest(name=name):
                self.assertTrue(new_project.valid_project_name(name))

    def test_reject_invalid_cross_platform_names(self):
        for name in ('', ' Leading', 'Trailing ', '123Game', 'Bad/Game', 'Bad\\Game',
                     'Game:Two', 'Game?Two', 'Game*', 'CON', 'LPT1', 'A' * 65):
            with self.subTest(name=name):
                self.assertFalse(new_project.valid_project_name(name))

    def test_creates_original_name_and_buildable_config(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / 'games').mkdir()
            name = "Blob's Quest"
            with mock.patch.object(new_project, 'ROOT', root), \
                 mock.patch.object(new_project, 'TEMPLATE', ROOT / 'tools' / 'templates' / 'BlankGame'), \
                 mock.patch.object(sys, 'argv', ['new_project.py', name]), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(new_project.main(), 0)
            game = root / 'games' / name
            self.assertTrue((game / 'build.bat').exists())
            self.assertTrue((game / 'build.sh').exists())
            self.assertTrue((game / 'assets' / 'scripts' / 'ExampleBehaviour.hpp').exists())
            self.assertEqual(json.loads((game / 'config.json').read_text(encoding='utf-8'))['project']['name'], name)
            engine = json.loads((game / 'assets/config/engine.json').read_text(encoding='utf-8'))
            self.assertEqual(engine['Window']['title'], name)
            # The generated game must also work with the normal build configuration
            # mechanism; no slug or special escaping of the apostrophe is required.
            spec_build = importlib.util.spec_from_file_location('xengine_build_names', ROOT / 'tools' / 'build.py')
            build = importlib.util.module_from_spec(spec_build)
            spec_build.loader.exec_module(build)
            generated = build.generate_game_config(game, root / 'generated', 'debug')
            self.assertIn(str((game / 'assets').resolve()), generated.read_text(encoding='utf-8'))
            self.assertEqual((game / 'dist' / 'windows-x86_64' / '0.1.0' / (name + '.exe')).name,
                             "Blob's Quest.exe")

    def test_prevent_overwriting_existing_project(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / 'games' / "Blob's Quest").mkdir(parents=True)
            with mock.patch.object(new_project, 'ROOT', root), \
                 mock.patch.object(sys, 'argv', ['new_project.py', "Blob's Quest"]), \
                 contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(new_project.main(), 1)


if __name__ == '__main__':
    unittest.main()

"""VS Code support must be optional, accurate and safe for existing user files."""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import vscode  # noqa: E402


class VSCodeIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.game = self.root / 'games' / "Blob's Game"
        for folder in (self.root / 'engine' / 'src',
                       self.root / 'engine' / 'include',
                       self.game / 'src',
                       self.game / 'assets' / 'gameobjects',
                       self.game / 'assets' / 'config'):
            folder.mkdir(parents=True, exist_ok=True)
        (self.root / 'engine' / 'src' / 'Engine.cpp').write_text(
            '#include "Engine.h"\n', encoding='utf-8')
        (self.root / 'engine' / 'include' / 'Engine.h').write_text(
            '#pragma once\n', encoding='utf-8')
        (self.game / 'src' / 'main.cpp').write_text(
            '#include "Engine.h"\n', encoding='utf-8')
        (self.game / 'assets' / 'gameobjects' / 'Actor.hpp').write_text(
            '#pragma once\n', encoding='utf-8')
        (self.game / 'assets' / 'gameobjects' / 'Actor.cpp').write_text(
            '#include "Actor.hpp"\n', encoding='utf-8')
        (self.game / 'assets' / 'config' / 'engine.json').write_text(
            '{"Engine": {}}', encoding='utf-8')
        (self.game / 'config.json').write_text(
            '{"project": {"name": "Blob\'s Game"}, "defines": ["ACTOR_TEST=1"]}',
            encoding='utf-8')
        self.tooling = SimpleNamespace(
            compiler='/usr/bin/g++', pkg_cflags=['-I/opt/sdl/include', '-pthread'],
            cxxflags=['-fno-exceptions'], cross=False)

    def setup_ide(self):
        return vscode.refresh(self.root, setup=True, tooling=self.tooling)

    def test_shared_commands_cover_engine_games_generated_sources_and_flags(self):
        self.assertEqual(self.setup_ide(), [])
        database = self.root / 'build' / 'ide' / 'compile_commands.json'
        entries = json.loads(database.read_text(encoding='utf-8'))
        self.assertEqual(len(entries), 4)
        self.assertEqual(len({item['file'] for item in entries}), 4)
        for entry in entries:
            self.assertEqual(entry['directory'], str(self.root.resolve()))
            self.assertIsInstance(entry['arguments'], list)
            self.assertEqual(entry['arguments'][0], '/usr/bin/g++')
            self.assertIn('-std=c++20', entry['arguments'])
            self.assertIn('-DXENGINE_DEBUG=1', entry['arguments'])
            self.assertIn('-fno-exceptions', entry['arguments'])
            self.assertIn('-I/opt/sdl/include', entry['arguments'])
        game_entry = next(entry for entry in entries if entry['file'].endswith('main.cpp'))
        self.assertIn('-DACTOR_TEST=1', game_entry['arguments'])
        self.assertIn('-I' + str(self.game / 'assets' / 'gameobjects'),
                      game_entry['arguments'])
        self.assertIn('-I' + str(self.root / 'engine' / 'include'),
                      game_entry['arguments'])
        self.assertTrue(any(entry['file'].endswith('GameBuild.generated.cpp')
                            for entry in entries))

        engine_config = json.loads(
            (self.root / '.vscode' / 'c_cpp_properties.json').read_text(encoding='utf-8'))
        game_config = json.loads(
            (self.game / '.vscode' / 'c_cpp_properties.json').read_text(encoding='utf-8'))
        self.assertEqual(engine_config['configurations'][0]['compileCommands'],
                         ['${workspaceFolder}/build/ide/compile_commands.json'])
        self.assertEqual(game_config['configurations'][0]['compileCommands'],
                         ['${workspaceFolder}/../../build/ide/compile_commands.json'])
        self.assertEqual(game_config['configurations'][0]['cppStandard'], 'c++20')
        self.assertEqual(game_config['configurations'][0]['compilerPath'],
                         '/usr/bin/g++')

    def test_refresh_detects_new_sources_even_if_no_compile_is_needed(self):
        self.setup_ide()
        (self.game / 'src' / 'Movement.cpp').write_text(
            'void move() {}\n', encoding='utf-8')
        vscode.refresh(self.root, tooling=self.tooling)
        entries = json.loads((self.root / 'build' / 'ide' /
                              'compile_commands.json').read_text(encoding='utf-8'))
        self.assertEqual(len(entries), 5)
        self.assertTrue(any(e['file'].endswith('Movement.cpp') for e in entries))

    def test_preexisting_configuration_is_backed_up_and_restored(self):
        local = self.root / '.vscode' / 'c_cpp_properties.json'
        local.parent.mkdir(parents=True)
        previous = '{"configurations": [{"name": "My old config"}], "version": 4}\n'
        local.write_text(previous, encoding='utf-8')
        self.setup_ide()
        self.assertNotEqual(local.read_text(encoding='utf-8'), previous)
        self.assertEqual(vscode.disable(self.root), [])
        self.assertEqual(local.read_text(encoding='utf-8'), previous)
        self.assertFalse((self.game / '.vscode' / 'c_cpp_properties.json').exists())
        self.assertFalse((self.root / 'build' / 'ide' / 'vscode.json').exists())
        self.assertFalse((self.root / 'build' / 'ide' / 'compile_commands.json').exists())

    def test_user_edits_are_never_overwritten_during_refresh_or_disable(self):
        self.setup_ide()
        path = self.game / '.vscode' / 'c_cpp_properties.json'
        config = json.loads(path.read_text(encoding='utf-8'))
        config['configurations'][0]['cppStandard'] = 'c++23'
        content = json.dumps(config, indent=2) + '\n'
        path.write_text(content, encoding='utf-8')
        warnings = vscode.refresh(self.root, tooling=self.tooling)
        self.assertTrue(any('user-edited' in w for w in warnings))
        self.assertEqual(path.read_text(encoding='utf-8'), content)
        self.assertTrue(any('user-edited' in w for w in vscode.disable(self.root)))
        self.assertEqual(path.read_text(encoding='utf-8'), content)

    def test_is_opt_in_and_no_ide_files_are_versioned(self):
        with self.assertRaisesRegex(Exception, 'not enabled'):
            vscode.refresh(self.root, tooling=self.tooling)
        self.assertFalse((self.root / 'build').exists())
        ignore = (ROOT / '.gitignore').read_text(encoding='utf-8')
        self.assertIn('/build/', ignore)
        self.assertIn('.vscode/', ignore)


if __name__ == '__main__':
    unittest.main()

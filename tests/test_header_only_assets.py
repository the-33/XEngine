"""Checks the single-file gameplay convention and safe legacy migration."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[1]


class HeaderOnlyTests(unittest.TestCase):
    def test_game_assets_are_single_hpp_files(self):
        for game in (ROOT/'games'/"Blob's Quest", ROOT/'games'/'My Princess Run',
                     ROOT/'tools/templates/BlankGame'):
            for folder in ('scripts', 'gameobjects', 'scenes'):
                path=game/'assets'/folder
                if not path.exists(): continue
                self.assertFalse(list(path.glob('*.cpp')), f'Unexpected legacy .cpp: {path}')
                self.assertFalse(list(path.glob('*.h')), f'Unexpected legacy .h: {path}')
                for header in path.glob('*.hpp'):
                    self.assertTrue(header.read_text().startswith('#pragma once'), str(header))

    def test_main_loads_init_callback_hpp(self):
        game=ROOT/'games'/"Blob's Quest"
        self.assertIn('#include "InitCallback.hpp"', (game/'src/main.cpp').read_text())
        init=(game/'src/InitCallback.hpp').read_text()
        self.assertIn('Scenes->Register("level1", BuildLevel1)', init)
        self.assertIn('Scenes->SetActive("level1")', init)
        scene=(game/'assets/scenes/Level1.hpp').read_text()
        self.assertIn('BuildLevel1(Scene* scene)', scene)
        self.assertNotIn('Scenes->Register(', scene)
        self.assertNotIn('Scenes->SetActive(', scene)
        self.assertNotIn('LoadTexture(', scene)

    def test_no_cpp_in_game_sources_when_header_only(self):
        spec=importlib.util.spec_from_file_location('build_headeronly',ROOT/'tools/build.py')
        mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
        game=ROOT/'games'/"Blob's Quest"
        files=mod.game_sources(game,'windows',Path('/tmp/generated.cpp'))
        self.assertEqual(len(files),2)
        self.assertIn(game/'src/main.cpp', files)

    def test_migration_moves_only_legacy_file_to_backup(self):
        spec=importlib.util.spec_from_file_location('migrate_headeronly',ROOT/'tools/migrate_to_hpp.py')
        mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
        with tempfile.TemporaryDirectory() as td:
            project=Path(td)/'Test'
            folder=project/'assets/scripts'; folder.mkdir(parents=True)
            (folder/'ExampleBehaviour.hpp').write_text('#pragma once\n')
            (folder/'ExampleBehaviour.cpp').write_text('// custom user edits')
            with mock.patch.object(mod,'TARGETS',{project:{'scripts':('ExampleBehaviour',)}}),\
                 mock.patch.object(mod,'ROOT',Path(td)):
                self.assertEqual(mod.main(),0)
                self.assertFalse((folder/'ExampleBehaviour.cpp').exists())
                self.assertEqual((project/'_legacy_split_files/scripts/ExampleBehaviour.cpp').read_text(),
                                 '// custom user edits')

    @unittest.skipUnless(shutil.which('g++'), 'g++ unavailable')
    def test_inline_code_can_be_included_from_multiple_translation_units(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            (root/'test.hpp').write_text('#pragma once\nnamespace Test { inline int counter = 0; inline void increment() { ++counter; } }\n')
            (root/'a.cpp').write_text('#include "test.hpp"\nvoid f() { Test::increment(); }\n')
            (root/'b.cpp').write_text('#include "test.hpp"\nvoid f();\nint main() { f(); return Test::counter == 1 ? 0 : 1; }\n')
            cmd=['g++','-std=c++20',str(root/'a.cpp'),str(root/'b.cpp'),'-o',str(root/'test')]
            result=subprocess.run(cmd,capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(subprocess.run([str(root/'test')]).returncode,0)

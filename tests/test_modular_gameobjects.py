"""Regressions for gameobjects source discovery, template and Release filtering."""
import importlib.util
import pathlib
import tempfile
import unittest
from unittest import mock

ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('xengine_build_modular',ROOT/'tools'/'build.py')
build=importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)

class ModularGameTests(unittest.TestCase):
    def test_gameobject_sources_are_compiled(self):
        with tempfile.TemporaryDirectory() as td:
            game=pathlib.Path(td)
            (game/'src').mkdir()
            (game/'src/main.cpp').write_text('int main() {return 0;}')
            (game/'assets/gameobjects').mkdir(parents=True)
            obj=(game/'assets/gameobjects/MyPrefab.cpp')
            obj.write_text('void myPrefab(){}')
            src=build.game_sources(game,'windows',game/'generated.cpp')
            self.assertIn(obj,src)

    def test_template_contains_gameobjects_and_scene_uses_it(self):
        template=ROOT/'tools/templates/BlankGame/assets'
        self.assertTrue((template/'gameobjects/ExampleObject.hpp').is_file())
        scene=(template/'scenes/StartScene.hpp').read_text(encoding='utf-8')
        self.assertIn('CreateExampleObject(*scene)',scene)

    def test_blobs_quest_has_separate_behaviours_and_level_geometry(self):
        game=ROOT/'games'/"Blob's Quest"/'assets'
        self.assertFalse((game/'scripts/BlobQuestGameplay.cpp').exists())
        for behavior in ('PlayerController','EnemyFollow','EnemyDamage','FireballController',
                         'CoinPickup','CameraFollow','EnemySpawner','HUDController'):
            self.assertTrue((game/'scripts'/f'{behavior}.hpp').exists(),behavior)
            self.assertFalse((game/'scripts'/f'{behavior}.cpp').exists(),behavior)
        scene=(game/'scenes/Level1.hpp').read_text(encoding='utf-8')
        for f in ('CreatePlayer','CreateWallTile','CreateWallBoundary','SpawnCoin','CreateObject("HUD")'):
            self.assertIn(f,scene)

    def test_release_assets_exclude_gameobject_cpp(self):
        with tempfile.TemporaryDirectory() as td:
            tmp=pathlib.Path(td)
            game=tmp/'Game';(game/'assets/gameobjects').mkdir(parents=True)
            (game/'assets/gameobjects/Foo.cpp').write_text('void foo() {}')
            (game/'assets/gameobjects/Foo.hpp').write_text('// header')
            (game/'assets/textures').mkdir()
            (game/'assets/textures/test.png').write_bytes(b'PNG placeholder')
            (game/'assets/config').mkdir()
            (game/'assets/config/engine.json').write_text('{}')
            (game/'config.json').write_text('{"project": {"name": "Game"}}')
            exe=tmp/'Game.exe';exe.write_bytes(b'EXE')
            with mock.patch.object(build,'package_windows_libs',return_value=[]):
                dest=build.package_game(game,'Game','0.1.0','windows','x86_64',exe)
            self.assertTrue((dest/'textures/test.png').exists())
            self.assertFalse((dest/'gameobjects/Foo.cpp').exists())
            self.assertFalse((dest/'gameobjects/Foo.hpp').exists())
            self.assertFalse((dest/'config/engine.json').exists())

if __name__=='__main__':unittest.main()

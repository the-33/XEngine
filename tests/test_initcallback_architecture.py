"""The game initializer owns resources and scene selection; scene files only build."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class GameInitializerTests(unittest.TestCase):
    def test_games_and_template_use_one_initcallback_header(self):
        for game in (ROOT/'games'/"Blob's Quest", ROOT/'games/My Princess Run',
                     ROOT/'tools/templates/BlankGame'):
            with self.subTest(game=game.name):
                main = (game/'src/main.cpp').read_text(encoding='utf8')
                init_path = game/'src/InitCallback.hpp'
                self.assertTrue(init_path.is_file())
                self.assertIn('#include "InitCallback.hpp"', main)
                self.assertIn('Engine::SetInitCallback(', main)
                self.assertIn('Scenes->Register(',init_path.read_text(encoding='utf8'))
                self.assertIn('Scenes->SetActive(',init_path.read_text(encoding='utf8'))
                for scene in (game/'assets/scenes').glob('*.hpp'):
                    scene_text = scene.read_text(encoding='utf8')
                    self.assertNotIn('Scenes->Register(', scene_text)
                    self.assertNotIn('Scenes->SetActive(', scene_text)
                    self.assertNotIn('LoadTexture(', scene_text)
                    self.assertNotIn('LoadMusic(', scene_text)

    def test_blob_assets_loading_in_init_only(self):
        game=ROOT/'games'/"Blob's Quest"
        init=(game/'src/InitCallback.hpp').read_text(encoding='utf8')
        scene=(game/'assets/scenes/Level1.hpp').read_text(encoding='utf8')
        self.assertIn('LoadTexture("textures/player.png"',init)
        self.assertIn('SetupBackground()',init)
        self.assertIn('Sound->PlayMusic(',init)
        self.assertIn('CreatePlayer(*scene',scene)
        self.assertIn('SpawnCoin(*scene',scene)

if __name__ == '__main__':
    unittest.main()

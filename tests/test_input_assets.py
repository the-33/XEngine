"""Input JSON scaffolds and BlankGame action/profile compatibility."""
import json
from pathlib import Path
import tempfile
import unittest

from test_new_game_asset import new_game_asset

ROOT = Path(__file__).resolve().parents[1]
BLANK = ROOT / "tools" / "templates" / "BlankGame"


class InputAssetTests(unittest.TestCase):
    def test_create_json_assets(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp) / "game"
            (project / "assets").mkdir(parents=True)
            (project / "config.json").write_text("{}")
            for kind, name, root_key in (
                ("inputmap", "Vehicle", "maps"),
                ("inputscheme", "CustomScheme", "schemes"),
                ("inputprofile", "LocalCoop", "players"),
            ):
                with self.subTest(kind=kind):
                    path, copied = new_game_asset.create_asset(project, kind, name)
                    self.assertEqual(path, project / "assets" / "input" / (name + ".json"))
                    self.assertIsNone(copied)
                    self.assertIn(root_key, json.loads(path.read_text()))
                    with self.assertRaises(new_game_asset.GenerationError):
                        new_game_asset.create_asset(project, kind, name)

    def test_blankgame_references_exist_and_have_compatible_schema(self):
        assets = BLANK / "assets" / "input"
        actions = json.loads((assets / "Default.inputactions.json").read_text())
        schemes = json.loads((assets / "Default.inputschemes.json").read_text())
        profiles = json.loads((assets / "Default.players.json").read_text())
        self.assertIn("Gameplay", actions["maps"])
        self.assertIn("UI", actions["maps"])
        self.assertIn("Global", actions["maps"])
        self.assertIn("Playing", actions["contexts"])
        self.assertEqual(set(actions["maps"]["Gameplay"]["actions"]),
                         {"Move", "Look", "Jump", "Attack", "Interact"})
        self.assertEqual(profiles["players"][0]["mode"], "Prefer")
        self.assertEqual(profiles["players"][0]["idealDevice"], "Gamepad")
        self.assertEqual(profiles["players"][1]["mode"], "Strict")
        for player in profiles["players"]:
            self.assertIn(player["scheme"], schemes["schemes"])
            for map_name in player["maps"]:
                self.assertIn(map_name, actions["maps"])
        init = (BLANK / "src" / "InitCallback.hpp").read_text()
        self.assertIn("LoadActionAsset", init)
        self.assertIn("LoadInputSchemes", init)
        self.assertIn("LoadPlayerProfiles", init)


if __name__ == "__main__":
    unittest.main()

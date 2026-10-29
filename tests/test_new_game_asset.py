"""Regression tests for independent XEngine gameplay asset scaffolding."""

import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("new_game_asset", ROOT / "tools" / "new_game_asset.py")
new_game_asset = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(new_game_asset)


class NewGameAssetTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.project = Path(self.temp.name) / "Example Game"
        (self.project / "assets").mkdir(parents=True)
        (self.project / "config.json").write_text("{}", encoding="utf-8")

    def test_default_location_and_independent_core_includes(self):
        for kind, name, symbol in (
            ("behaviour", "Mover", "class Mover final : public Behaviour"),
            ("object", "Player", "CreatePlayer(Scene& scene)"),
            ("scene", "Level1", "BuildLevel1(Scene* scene)"),
        ):
            output, sprite = new_game_asset.create_asset(self.project, kind, name)
            self.assertEqual(output, self.project / "assets" / f"{name}.hpp")
            self.assertIsNone(sprite)
            content = output.read_text(encoding="utf-8")
            self.assertIn("#include <XEngine.h>", content)
            self.assertIn(symbol, content)
            self.assertNotIn("#include <Behaviour.h>", content)

    def test_texture_recovered_from_canonical_template_without_overwriting(self):
        original = (new_game_asset.SPRITE_SOURCES / "circle.png").read_bytes()
        output, copied = new_game_asset.create_asset(
            self.project, "object", "Ball", sprite="circle", collider="circle",
        )
        self.assertEqual(copied, self.project / "assets" / "textures" / "circle.png")
        self.assertEqual(copied.read_bytes(), original)
        source = output.read_text(encoding="utf-8")
        self.assertIn('Assets->LoadTexture("textures/circle.png")', source)
        self.assertIn("AddComponent<SpriteRenderer>()", source)
        self.assertIn("Collider2D::Shape::Circle", source)

        copied.write_bytes(b"custom game texture")
        _, restored = new_game_asset.create_asset(self.project, "object", "AnotherBall", sprite="circle")
        self.assertIsNone(restored)
        self.assertEqual(copied.read_bytes(), b"custom game texture")

    def test_behaviour_reference_and_optional_directory(self):
        new_game_asset.create_asset(self.project, "behaviour", "Mover", directory="scripts")
        output, _ = new_game_asset.create_asset(
            self.project, "object", "Hero", directory="gameobjects",
            sprite="square", collider="box", behaviour="Mover",
        )
        text = output.read_text(encoding="utf-8")
        self.assertIn('#include "scripts/Mover.hpp"', text)
        self.assertIn("object->AddComponent<Mover>();", text)
        self.assertIn("Collider2D::Shape::Box", text)

    def test_existing_output_and_invalid_options_do_not_modify_files(self):
        target = self.project / "assets" / "Player.hpp"
        target.write_text("custom", encoding="utf-8")
        with self.assertRaises(new_game_asset.GenerationError):
            new_game_asset.create_asset(self.project, "object", "Player", sprite="square")
        self.assertEqual(target.read_text(encoding="utf-8"), "custom")
        self.assertFalse((self.project / "assets" / "textures" / "square.png").exists())

        for invalid in ("class", "A B", "1Player", "CON"):
            with self.subTest(name=invalid), self.assertRaises(new_game_asset.GenerationError):
                new_game_asset.create_asset(self.project, "behaviour", invalid)
        with self.assertRaises(new_game_asset.GenerationError):
            new_game_asset.create_asset(self.project, "scene", "Next", collider="box")
        with self.assertRaises(new_game_asset.GenerationError):
            new_game_asset.create_asset(self.project, "object", "Enemy", behaviour="Missing")

    def test_prevents_output_outside_assets_including_symlinks(self):
        for path in ("../outside", "a/../../outside", "/tmp/outside", "C:\\outside"):
            with self.subTest(directory=path), self.assertRaises(new_game_asset.GenerationError):
                new_game_asset.create_asset(self.project, "scene", "Unsafe", directory=path)
        external = Path(self.temp.name) / "outside"
        external.mkdir()
        (self.project / "assets" / "linked").symlink_to(external, target_is_directory=True)
        with self.assertRaises(new_game_asset.GenerationError):
            new_game_asset.create_asset(self.project, "scene", "Unsafe", directory="linked")
        self.assertFalse((external / "Unsafe.hpp").exists())

    def test_cli_and_template_files_remain_independent_of_game_assets(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(new_game_asset.main(["--list-sprites"]), 0)
            self.assertEqual(new_game_asset.main([
                "--project-dir", str(self.project), "behaviour", "Controller",
            ]), 0)
        self.assertIn("circle", output.getvalue())
        self.assertTrue((self.project / "assets" / "Controller.hpp").is_file())
        self.assertTrue((new_game_asset.SCAFFOLDS / "Behaviour.hpp.tpl").is_file())

    def test_new_project_still_ships_local_launchers(self):
        template = ROOT / "tools" / "templates" / "BlankGame"
        self.assertTrue((template / "new_asset.sh").is_file())
        self.assertTrue((template / "new_asset.bat").is_file())
        for relative in (
            "assets/scripts/ExampleBehaviour.hpp",
            "assets/scenes/StartScene.hpp",
            "assets/gameobjects/ExampleObject.hpp",
        ):
            self.assertIn("#include <XEngine.h>", (template / relative).read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()

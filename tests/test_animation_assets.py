"""Animation asset scaffolds, public API, and documentation regressions."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "tools" / "new_game_asset.py"
SPEC = importlib.util.spec_from_file_location("animation_scaffolds", GENERATOR)
assert SPEC is not None and SPEC.loader is not None
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class AnimationAssetsTests(unittest.TestCase):
    def test_generator_creates_loadable_animation_and_controller_json(self):
        with tempfile.TemporaryDirectory(prefix="xengine-animations-") as root:
            game = Path(root)
            (game / "assets").mkdir()
            (game / "config.json").write_text("{}", encoding="utf-8")
            clip, _ = module.create_asset(game, "animation", "Idle")
            controller, _ = module.create_asset(game, "animator", "Idle")
            self.assertEqual(clip.relative_to(game).as_posix(), "assets/animations/Idle.animation.json")
            self.assertEqual(controller.relative_to(game).as_posix(), "assets/animators/Idle.animator.json")
            clip_json = json.loads(clip.read_text(encoding="utf-8"))
            controller_json = json.loads(controller.read_text(encoding="utf-8"))
            self.assertEqual(clip_json["version"], 1)
            self.assertEqual(clip_json["name"], "Idle")
            self.assertEqual(controller_json["entry"], "Idle")
            self.assertEqual(controller_json["states"][0]["clip"],
                             "animations/Idle.animation.json")
            with self.assertRaises(module.GenerationError):
                module.create_asset(game, "animation", "Idle")

    def test_public_headers_and_pipeline_are_connected(self):
        umbrella = (ROOT / "engine/include/XEngine.h").read_text(encoding="utf-8")
        source = (ROOT / "engine/src/Engine.cpp").read_text(encoding="utf-8")
        manager = (ROOT / "engine/include/AssetManager.h").read_text(encoding="utf-8")
        registry = (ROOT / "engine/include/AnimationPropertyRegistry.h").read_text(encoding="utf-8")
        animator = (ROOT / "engine/include/Animator.h").read_text(encoding="utf-8")
        for header in ("Animation.h", "Animator.h", "AnimationPropertyRegistry.h"):
            self.assertIn(f'#include "{header}"', umbrella)
            self.assertIn(f"@file {header}", (ROOT / "engine/include" / header).read_text())
        self.assertLess(source.index("scenes->Update(dt)"), source.index("scenes->LateUpdate(dt)"))
        self.assertIn("LoadAnimation(const std::string& path)", manager)
        self.assertIn("LoadAnimatorController(const std::string& path)", manager)
        self.assertIn("#define XENGINE_ANIMATABLE", registry)
        for method in ("SetController", "CrossFade", "SetTrigger", "RemoveEventListener",
                       "IsInTransition", "GetNormalizedTime"):
            self.assertIn(method, animator)

    def test_game_and_engine_guides_link_actual_api(self):
        developer_index = (ROOT / "docs/reference/game/GAME_DEVELOPERS.md").read_text()
        guide = (ROOT / "docs/reference/game/ANIMATIONS.md").read_text()
        input_guide = (ROOT / "docs/reference/game/INPUT_ACTIONS.md").read_text()
        self.assertIn("ANIMATIONS.md", developer_index)
        for field in ("Exit Time", "interruptionSource", "spritesheet", "frameDurations",
                      "XENGINE_ANIMATABLE", "LateUpdate", "CrossFade", "RestoreOriginal"):
            self.assertIn(field, guide)
        for field in ("SetNavigationPlayer", "TextInput", "Gamepad/Start",
                      "WasPressedThisFrame", "Default.players.json"):
            self.assertIn(field, input_guide)

    def test_animator_nested_records_and_clip_fields_are_documented(self):
        """Doxygen also visits public members of private nested structs."""
        animator = (ROOT / "engine/include/Animator.h").read_text(encoding="utf-8")
        animation = (ROOT / "engine/include/Animation.h").read_text(encoding="utf-8")

        for record, fields in {
            "Playhead": ("state", "clip", "elapsed", "duration"),
            "ActiveBlend": ("transition", "from", "to", "elapsed", "duration",
                            "curve", "spriteBlend"),
            "TrackPose": ("value", "binding"),
            "EventListener": ("id", "callback"),
        }.items():
            match = re.search(rf"\bstruct\s+{record}\s*\{{([^}}]+)\}};", animator, re.S)
            self.assertIsNotNone(match, f"Missing {record}")
            for field in fields:
                with self.subTest(record=record, field=field):
                    self.assertRegex(match.group(1),
                                     rf"\b{field}\b[^;]*;\s*///<\s*\S+")

        for record, fields in {
            "AnimationClip": ("loop", "exitBehavior", "tracks", "spritesheet", "events"),
            "AnimatorLayer": ("name", "mode", "weight", "mask"),
        }.items():
            match = re.search(rf"\bstruct\s+{record}\s*\{{(.*?)\n\}};", animation, re.S)
            self.assertIsNotNone(match, f"Missing {record}")
            for field in fields:
                with self.subTest(record=record, field=field):
                    self.assertRegex(match.group(1),
                                     rf"\b{field}\b[^;]*;\s*///<\s*\S+")

    def test_animation_value_header_compiles_without_sdl(self):
        compiler = shutil.which("g++")
        if not compiler:
            self.skipTest("g++ unavailable")
        source = '''#include "Animation.h"
#include <type_traits>
static_assert(std::is_constructible_v<AnimationValue, Vec2>);
static_assert(std::is_constructible_v<AnimationValue, Color>);
static_assert(std::is_constructible_v<AnimationValue, std::string>);
int main() { AnimatorTransition edge; return edge.hasExitTime ? 1 : 0; }
'''
        result = subprocess.run(
            [compiler, "-std=c++20", "-fsyntax-only", "-x", "c++", "-",
             "-I", str(ROOT / "engine/include")],
            input=source, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_animator_and_scene_lifecycle_cpp20_sdl_stub_syntax(self):
        compiler = shutil.which("g++")
        if not compiler:
            self.skipTest("g++ unavailable")
        # Reuse the project's existing, deliberately minimal SDL header fakes.
        stub_spec = importlib.util.spec_from_file_location(
            "sdl_stub_fixture", ROOT / "tests/test_camera_benchmark_gcc_compile.py")
        assert stub_spec is not None and stub_spec.loader is not None
        stub_fixture = importlib.util.module_from_spec(stub_spec)
        stub_spec.loader.exec_module(stub_fixture)
        with tempfile.TemporaryDirectory(prefix="xengine-animation-cpp-") as directory:
            stub = Path(directory)
            for name, content in stub_fixture.STUBS.items():
                location = stub / name
                location.parent.mkdir(parents=True, exist_ok=True)
                location.write_text(content, encoding="utf-8")
            # A syntax-only JSON declaration fixture; this deliberately does
            # not validate the actual nlohmann implementation/runtime parser.
            (stub / "nlohmann/json.hpp").write_text('''#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
namespace nlohmann {
class json {
  std::vector<json> contents_;
public:
  static json parse(const char*, const char*);
  static json array();
  static json object();
  bool is_object() const;
  bool is_boolean() const;
  bool is_number_integer() const;
  bool is_number() const;
  bool is_string() const;
  bool is_array() const;
  bool contains(const char*) const;
  std::size_t size() const;
  const json& at(const std::string&) const;
  const json& at(std::size_t) const;
  const json& operator[](std::size_t) const;
  template<class T> T value(const char*, T) const;
  template<class T> T get() const;
  std::vector<json>::const_iterator begin() const;
  std::vector<json>::const_iterator end() const;
};
}
''', encoding="utf-8")
            for name in ("Animation.cpp", "Animator.cpp", "AnimationPropertyRegistry.cpp",
                         "SpriteRenderer.cpp", "GameObject.cpp", "Scene.cpp",
                         "SceneManager.cpp"):
                with self.subTest(source=name):
                    result = subprocess.run(
                        [compiler, "-std=c++20", "-fsyntax-only", "-DXENGINE_DEBUG",
                         "-I", str(stub), "-I", str(ROOT / "engine/include"),
                         str(ROOT / "engine/src" / name)],
                        text=True, capture_output=True, check=False,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()

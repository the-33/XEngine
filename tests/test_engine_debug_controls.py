"""Source guards for the Debug F2/F3 contract and engine-owned log titles."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "engine"


class EngineDebugControlsTests(unittest.TestCase):
    def test_f3_controls_draw_mode_and_f2_only_controls_panel(self):
        source = (ENGINE / "src" / "Engine.cpp").read_text(encoding="utf-8")
        header = (ENGINE / "include" / "Engine.h").read_text(encoding="utf-8")
        colliders = (ENGINE / "src" / "Collider2D.cpp").read_text(encoding="utf-8")

        self.assertIn("bool mShowDebugDraws = false;", header)
        self.assertIn("bool mShowStatsOverlay = false;", header)
        self.assertRegex(source, r"KeyPressed\(Key::F3\)\)\s*\{\s*"
                          r"mShowDebugDraws = !mShowDebugDraws;\s*"
                          r"mShowStatsOverlay = mShowDebugDraws;")
        self.assertRegex(source, r"else if \(XEngineBuild::IsDebug && mLogStats && "
                          r"mShowDebugDraws && input &&\s*"
                          r"input->KeyPressed\(Key::F2\)\)\s*\{\s*"
                          r"mShowStatsOverlay = !mShowStatsOverlay;")
        self.assertIn("mShowDebugDraws = false;", source)
        self.assertIn("mShowStatsOverlay = false;", source)
        self.assertIn("mShowStatsOverlay && input &&\n        input->KeyPressed(Key::F4)", source)
        self.assertIn("!drawCollider_ && !Engine::GetInstancePtr()->mShowDebugDraws", colliders)
        self.assertNotIn("!Engine::GetInstancePtr()->mShowStatsOverlay", colliders)

    def test_all_engine_logerror_and_logwarning_titles_are_canonical(self):
        sources = [*sorted((ENGINE / "src").glob("*.cpp")),
                   ENGINE / "include" / "Singleton.h"]
        count = 0
        for path in sources:
            if path.name == "ErrorHandler.cpp":
                continue  # API forwards arbitrary user-defined captions as-is.
            source = path.read_text(encoding="utf-8")
            captions = re.findall(r'\bLog(?:Error|Warning)\(\s*"([^"]+)"', source)
            count += len(captions)
            for title in captions:
                self.assertTrue(title == "Engine" or title.startswith("Engine - "),
                                f"Non-engine title {title!r} in {path.relative_to(ROOT)}")
        self.assertGreater(count, 40)

    def test_logs_from_game_preserve_default_and_custom_titles(self):
        header = (ENGINE / "include" / "ErrorHandler.h").read_text(encoding="utf-8")
        handler = (ENGINE / "src" / "ErrorHandler.cpp").read_text(encoding="utf-8")
        self.assertIn('std::string_view title = "Game"', header)
        self.assertIn('Debug::Log(pending)', handler)
        self.assertIn('Debug::LogError(error, caption)', handler)
        for name, system in (("Animator.cpp", "Animation"),
                             ("AssetManager.cpp", "Assets"),
                             ("RenderManager.cpp", "Render"),
                             ("SoundManager.cpp", "Audio"),
                             ("WindowManager.cpp", "Window")):
            source = (ENGINE / "src" / name).read_text(encoding="utf-8")
            self.assertIn(f'"Engine - {system}"', source)
        startup = (ENGINE / "src" / "Engine.cpp").read_text(encoding="utf-8")
        self.assertIn('Console.Log("XEngine started successfully.", "Engine")', startup)
        self.assertIn('Console.Log("Shutting down RenderManager... OK", "Engine - Render")',
                      startup)

    def test_capsule_debug_contour_is_closed_and_joins_are_sealed(self):
        collider = (ENGINE / "src" / "Collider2D.cpp").read_text(encoding="utf-8")
        renderer = (ENGINE / "src" / "RenderManager.cpp").read_text(encoding="utf-8")
        self.assertIn("constexpr int segments = 32;", collider)
        self.assertIn("std::array<Vec2, 2 * (segments + 1)> outline{};", collider)
        self.assertIn("outline[(i + 1) % outline.size()]", collider)
        self.assertIn("outline[0] = capsule.b + normal * capsule.radius;", collider)
        self.assertIn("outline.back() = capsule.a + normal * capsule.radius;", collider)
        self.assertIn("if (mDrawPhase == DrawPhase::Debug)", renderer)
        self.assertIn("SDL_RenderDrawPointF(mRenderer, s1.x, s1.y)", renderer)
        self.assertIn("SDL_RenderDrawPointF(mRenderer, s2.x, s2.y)", renderer)


if __name__ == "__main__":
    unittest.main()

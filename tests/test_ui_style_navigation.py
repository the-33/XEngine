"""Regresiones de estilos, foco y navegación de la UI del motor."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def source(relative_path):
    return (ROOT / relative_path).read_text(encoding="utf-8")


class UIStyleNavigationTests(unittest.TestCase):
    def test_public_styling_api_and_reusable_colors(self):
        header = source("engine/include/UIManager.h")
        cpp = source("engine/src/UIManager.cpp")
        for name in (
            "SetStyle", "GetStyle", "PushStyle", "PopStyle", "cornerRadius",
            "UICornerRadiusUnit", "Percent", "Pixels",
            "borderWidth", "dropdownBackground", "dropdownArrow", "sliderFill",
            "toggleOn", "textSelection", "checkboxCheck", "placeholder",
        ):
            with self.subTest(name=name):
                self.assertIn(name, header)
        self.assertIn("mStyleStack.push_back(mActiveStyle)", cpp)
        self.assertIn("ApplyStyle_(mStyleStack.empty() ? mBaseStyle : previous)", cpp)
        self.assertIn("mActiveStyle.cornerRadius * mTextScale", cpp)
        self.assertIn("mActiveStyle.cornerRadiusUnit == UICornerRadiusUnit::Percent", cpp)
        self.assertIn("std::min(r.w, r.h)", cpp)
        self.assertIn("std::clamp(mActiveStyle.cornerRadius, 0.f, 50.f)", cpp)
        self.assertIn("mActiveStyle.borderWidth * mTextScale", cpp)

    def test_initial_focus_and_explicit_directional_rules(self):
        header = source("engine/include/UIManager.h")
        cpp = source("engine/src/UIManager.cpp")
        for token in (
            "UINavigationMode::Automatic", "UINavigationMode::Explicit",
            "UINavigationMode::None", "SetInitialFocus", "SetAutoSelectFirst",
            "SetNavigation", "ClearNavigation", "ClearAllNavigation",
            "mPendingFocusId == id", "if (navigation.mode == UINavigationMode::Explicit)",
            "if (!selectable(candidate)) continue;",
        ):
            with self.subTest(token=token):
                self.assertIn(token, header + cpp)
        self.assertIn("if (mFocusedId.empty() && mAutoSelectFirst", cpp)
        self.assertIn("mPendingFocusId.clear();", cpp)

    def test_dropdown_draws_geometry_instead_of_font_characters(self):
        cpp = source("engine/src/UIManager.cpp")
        dropdown = cpp[cpp.index("bool UIManager::Dropdown(const char* id, const Rect& r,"):]
        self.assertIn("PushChevron_(r, opened);", dropdown)
        self.assertNotIn('Label(opened ? "^" : "v"', dropdown)
        self.assertIn("mActiveStyle.dropdownArrow", cpp)
        self.assertIn("render->DrawLineScreen", cpp)

    def test_rounding_and_lines_work_with_both_render_paths(self):
        header = source("engine/include/RenderManager.h")
        renderer = source("engine/src/RenderManager.cpp")
        commands = source("engine/include/Engine.h")
        for primitive in ("ScreenRoundedRect", "ScreenLine"):
            with self.subTest(primitive=primitive):
                self.assertIn(primitive, commands)
                self.assertIn(f"case DrawCmd::Type::{primitive}:", renderer)
        self.assertIn("void DrawRoundedRectScreen(", header)
        self.assertIn("void DrawLineScreen(", header)
        self.assertIn("RoundedOutline_", renderer)
        self.assertIn("SDL_RenderGeometry", renderer)
        self.assertIn("if (snapRecord)", renderer)
        self.assertIn("DrawRoundedRectScreenImmediate_", renderer)
        self.assertIn("DrawLineScreenImmediate_", renderer)

    def test_configuration_and_documentation(self):
        engine = source("engine/src/Engine.cpp")
        guide = source("docs/reference/game/RENDERING_AND_UI.md")
        for name in ("cornerRadius", "borderWidth"):
            self.assertIn(f'ui.contains("{name}")', engine)
            self.assertIn(name, guide)
        self.assertIn('ui.contains("cornerRadiusUnit")', engine)
        self.assertIn('UICornerRadiusUnit::Percent', guide)
        for api in ("SetInitialFocus", "SetAutoSelectFirst", "SetNavigation",
                    "PushStyle", "PopStyle", "UINavigationMode::Explicit"):
            self.assertIn(api, guide)


if __name__ == "__main__":
    unittest.main()

"""Regression guards for opt-in low-resolution world rendering and dropdown UI.

SDL device creation, actual nearest-neighbour output and controller events are
also exercised by the native integration smoke test on supported CI hosts.
"""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class InternalRenderAndDropdownTests(unittest.TestCase):
    def test_public_render_api_is_documented(self):
        header = read("engine/include/RenderManager.h")
        for method, params in (
            ("SetRenderResolution", ("width", "height")),
            ("GetRenderResolution", ()),
            ("DisplayToRender", ("displayPoint",)),
        ):
            match = re.search(r"(/\*\*.*?\*/)[\s]*[\w: ]+\b" + method + r"\(", header, re.DOTALL)
            self.assertIsNotNone(match, method)
            comment = match.group(1)
            self.assertIn("@brief", comment)
            self.assertIn("@return", comment)
            for param in params:
                self.assertIn("@param " + param, comment)

    def test_world_is_rendered_to_target_then_upscaled_before_ui(self):
        render = read("engine/src/RenderManager.cpp")
        engine = read("engine/src/Engine.cpp")
        header = read("engine/include/Engine.h")
        self.assertIn("SDL_TEXTUREACCESS_TARGET, width, height", render)
        self.assertIn("SDL_ScaleModeNearest", render)
        self.assertIn("SDL_SetRenderTarget(mRenderer, mRenderTarget)", render)
        self.assertIn("SDL_RenderCopy(mRenderer, mRenderTarget, nullptr, &dst)", render)
        self.assertIn("mFrameOpen", render)
        self.assertIn("ApplyPendingRenderTarget_();", render)
        self.assertIn("uiStartIndex", header)
        self.assertIn("render->mReusableSnapshot.uiStartIndex", engine)
        self.assertIn("render->BeginScreenPass_();", engine)
        self.assertIn("if (index == snap.uiStartIndex)", render)

    def test_input_projection_uses_virtual_world_resolution(self):
        self.assertIn("DisplayToRender(pixel)", read("engine/src/InputManager.cpp"))
        self.assertIn("GetRenderResolution()", read("engine/src/Camera2D.cpp"))
        self.assertIn("GetRenderResolution()", read("engine/src/Scene.cpp"))

    def test_public_dropdown_api_and_cancel_are_documented(self):
        header = read("engine/include/UIManager.h")
        for method, params in (
            ("Dropdown", ("id", "r", "selectedIndex", "options", "maxVisibleItems")),
            ("DropdownConsumedCancel", ()),
        ):
            match = re.search(r"(/\*\*.*?\*/)[\s]*[\w: ]+\b" + method + r"\(", header, re.DOTALL)
            self.assertIsNotNone(match, method)
            comment = match.group(1)
            self.assertIn("@brief", comment)
            self.assertIn("@return", comment)
            for param in params:
                self.assertIn("@param " + param, comment)

    def test_dropdown_renders_as_overlay_and_consumes_cancel(self):
        implementation = read("engine/src/UIManager.cpp")
        self.assertIn("mDropdownOverlayCmds", implementation)
        self.assertIn("mDropdownCancelledThisFrame = true", implementation)
        self.assertIn("mDropdownNavigationStep", implementation)
        self.assertIn("GetMouseWheel().y", implementation)
        self.assertIn("mDropdownSuppressMouse", implementation)

    def test_monitor_query_and_user_documentation(self):
        window = read("engine/src/WindowManager.cpp")
        self.assertIn("SDL_GetDesktopDisplayMode", window)
        docs = read("docs/reference/game/RENDERING_AND_UI.md")
        self.assertIn("SetRenderResolution", docs)
        self.assertIn("UIManager::Dropdown", docs)


if __name__ == "__main__":
    unittest.main()

"""Regression checks for text caret timing and rounded dropdown highlights."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def source(path):
    """Read the exact production source used by the native build."""
    return (ROOT / path).read_text(encoding="utf-8")


class UiVisualCorrectionsTests(unittest.TestCase):
    def test_native_caret_timing_and_popup_geometry(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("C++20 compiler unavailable")
        code = r'''
#include "XEngine/detail/UIVisualGeometry.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
using namespace xengine::detail::ui;

static bool near_(float a, float b) { return std::fabs(a - b) < 0.001f; }
int main() {
    assert(CaretVisible(1000, 1000));
    assert(CaretVisible(1499, 1000));
    assert(!CaretVisible(1500, 1000));
    assert(!CaretVisible(1999, 1000));
    assert(CaretVisible(2000, 1000));
    // Unsigned 32-bit SDL_GetTicks wraps without changing the phase.
    const std::uint32_t reset = std::numeric_limits<std::uint32_t>::max() - 99;
    assert(CaretVisible(reset + 499u, reset));
    assert(!CaretVisible(reset + 500u, reset));
    assert(CaretVisible(reset + 1000u, reset));

    // The rectangular clips must tile the popup interior without gaps and
    // share one radius/shape, independent of which row is selected/hovered.
    for (float scale : {0.75f, 1.f, 1.25f, 1.5f, 2.f}) {
        const Rect popup{10.25f * scale, 20.25f * scale,
                         120.f * scale, 128.f * scale};
        const float rowHeight = 32.f * scale;
        const float border = 1.4f * scale;
        const float radius = 16.f * scale;  // px style
        auto first = DropdownRowClip(popup, rowHeight, 0, 4, radius, border);
        assert(first.valid && first.radius > 0.f);
        assert(near_(first.clip.y, first.shape.y));
        float lastBottom = first.clip.y + first.clip.h;
        for (int row = 1; row < 4; ++row) {
            const auto next = DropdownRowClip(popup, rowHeight, row, 4, radius, border);
            assert(next.valid);
            assert(near_(next.clip.y, lastBottom));
            assert(near_(next.clip.x, first.clip.x));
            assert(near_(next.clip.w, first.clip.w));
            assert(near_(next.radius, first.radius));
            lastBottom = next.clip.y + next.clip.h;
        }
        assert(near_(lastBottom, first.shape.y + first.shape.h));
        // Percent style is resolved against the *whole popup*, never each row.
        const float percentRadius = 0.3f * std::min(popup.w, popup.h);
        const auto percent = DropdownRowClip(popup, rowHeight, 2, 4,
                                             percentRadius, border);
        assert(percent.valid && percent.radius >= 0.f);
        // A popup opening upwards changes only the Y origin.
        Rect upward = popup;
        upward.y -= 150.f;
        const auto upper = DropdownRowClip(upward, rowHeight, 3, 4, radius, border);
        assert(upper.valid && near_(upper.clip.h, first.clip.h));
    }
    // One visible row retains both ends of the common rounded silhouette.
    const auto only = DropdownRowClip({4, 8, 120, 32}, 32, 0, 1, 12, 1);
    assert(only.valid && near_(only.clip.y, only.shape.y));
    assert(near_(only.clip.h, only.shape.h));
    assert(near_(only.radius, 11.f));
    // Zero radius makes every visible row square, even with a border.
    assert(near_(DropdownRowClip({4, 8, 120, 96}, 32, 1, 3, 0, 2).radius, 0.f));
    assert(!DropdownRowClip({0, 0, 1, 1}, 1, 0, 1, 1, 3).valid);
    assert(!DropdownRowClip({0, 0, 10, 10}, 10, 1, 1, 1, 0).valid);
}
'''
        with tempfile.TemporaryDirectory(prefix="xengine_ui_visual_") as directory:
            path = Path(directory)
            program = path / "ui_visual.cpp"
            executable = path / "ui_visual"
            program.write_text(code, encoding="utf-8")
            result = subprocess.run(
                [compiler, "-std=c++20", "-Wall", "-Wextra", "-pedantic",
                 "-I", str(ROOT / "engine/include"), str(program), "-o", str(executable)],
                capture_output=True, text=True, timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_selection_and_caret_do_not_inherit_panel_style(self):
        ui = source("engine/src/UIManager.cpp")
        text = ui.split("bool UIManager::TextInput(const char* id, const Rect& r,", 1)[1]
        text = text.split("void UIManager::ProgressBar(", 1)[0]
        self.assertIn("PushPanel_(r, mBtnDefaultColor, outline, true);", text)
        self.assertIn("PushSolidRect_({r.x + pad + left", text)
        self.assertIn("mActiveStyle.textSelection", text)
        self.assertIn("PushSolidRect_(Rect{x, r.y + pad, 2.f, h}", text)
        self.assertNotIn("Panel(", text)
        self.assertIn("CaretVisible(", text)
        self.assertIn("SDL_GetTicks(), mTextCaretResetTick", text)
        self.assertIn("mTextCursorByte != oldCursor", text)
        self.assertIn("mTextSelectionAnchorByte != oldAnchor", text)
        self.assertIn("changed || hitByMouse", text)
        self.assertIn("ResetTextCaretBlink_();", ui)
        self.assertIn("mTextCaretResetTick = SDL_GetTicks();", ui)

    def test_dropdown_uses_shared_rounded_clip_then_last_border(self):
        ui = source("engine/src/UIManager.cpp")
        drop = ui.split("bool UIManager::Dropdown(const char* id, const Rect& r,", 1)[1]
        drop = drop.split("bool UIManager::TextInput(", 1)[0]
        self.assertIn("DropdownRowClip(", drop)
        self.assertIn("PushClippedRoundedFill_(geometry.shape, geometry.clip,", drop)
        self.assertNotIn("Panel(row,", drop)
        self.assertLess(drop.index("PushPanel_(popup, mActiveStyle.dropdownBackground"),
                        drop.index("PushClippedRoundedFill_(geometry.shape"))
        self.assertLess(drop.index("PushClippedRoundedFill_(geometry.shape"),
                        drop.index("PushPanel_(popup, Color{0, 0, 0, 0}, mFocusColor, false)"))
        for preserved in ("GetMouseWheel().y", "mDropdownNavigationStep",
                          "mDropdownPressedRow", "mDropdownOverlayCmds.push_back",
                          "mDropdownFirstVisible", "mTopPointerId"):
            self.assertIn(preserved, drop)

    def test_immediate_and_recorded_renderer_restore_clip_state(self):
        ui = source("engine/src/UIManager.cpp")
        renderer = source("engine/src/RenderManager.cpp")
        header = source("engine/include/Engine.h")
        self.assertIn("case CmdType::ClippedRoundedFill:", ui)
        self.assertIn("DrawClippedRoundedFillScreen_(c.r, c.clip", ui)
        self.assertIn("command.hasScreenClip = true;", renderer)
        self.assertIn("c.hasScreenClip ? &c.screenClip : nullptr", renderer)
        self.assertIn("if (snapRecord) {", renderer)
        self.assertIn("SDL_RenderIsClipEnabled(mRenderer)", renderer)
        self.assertIn("SDL_RenderSetClipRect(mRenderer, &clipped)", renderer)
        self.assertIn("SDL_RenderSetClipRect(mRenderer, hadClip ? &oldClip : nullptr)", renderer)
        self.assertIn("Rect screenClip{};", header)
        self.assertIn("bool hasScreenClip = false;", header)


if __name__ == "__main__":
    unittest.main()

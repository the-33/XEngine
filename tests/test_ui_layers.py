"""UI layer order, equal-layer overlap and popup interaction regression tests."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "engine/include/UIManager.h"
SOURCE = ROOT / "engine/src/UIManager.cpp"


class UiLayerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.source = SOURCE.read_text(encoding="utf-8")

    def test_public_scoped_and_absolute_layers(self):
        self.assertIn("void SetLayer(int layer) noexcept { mLayer = layer; }", self.header)
        self.assertIn("int GetLayer() const noexcept { return mLayer; }", self.header)
        self.assertIn("void PushLayer(int layer) noexcept;", self.header)
        self.assertIn("void PopLayer() noexcept;", self.header)
        self.assertIn("mLayerStack.push_back(mLayer);", self.source)
        self.assertIn("mLayer = mLayerStack.back();", self.source)

    def test_draw_commands_inherit_layer_and_stable_sort(self):
        self.assertIn("command.layer = mLayer;", self.source)
        self.assertIn("std::stable_sort(mCmds.begin(), mCmds.end()", self.source)
        self.assertIn("return a.layer < b.layer;", self.source)
        self.assertIn("QueueCmd_(std::move(cmd));", self.source)

    def test_overlapping_pointer_targets_use_completed_previous_frame(self):
        self.assertIn("mHitRegionsPrevious.swap(mHitRegionsCurrent);", self.source)
        self.assertIn("mHitRegionsCurrent.push_back({id, r, mLayer, false});", self.source)
        self.assertIn("IsAbove(hit.layer, sequence,", self.source)
        self.assertIn("(mTopPointerId.empty() || mTopPointerId == id)", self.source)
        self.assertIn("mLayer = 0;", self.source)

    def test_popup_uses_same_layer_order_for_render_and_hits(self):
        self.assertIn("mOpenDropdownLayer = mLayer;", self.source)
        self.assertIn("mHitRegionsCurrent.push_back({id, popup, mLayer, true});", self.source)
        self.assertIn("std::numeric_limits<std::size_t>::max()", self.source)
        self.assertIn("mLayer <= mOpenDropdownLayer", self.source)

    def test_native_layer_priority_same_layer_last_wins(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        code = r'''
#include "XEngine/detail/UILayerOrder.h"
#include <cassert>
#include <cstddef>
int main() {
    using xengine::detail::ui::IsAbove;
    // A higher layer wins even when the lower-layer control was drawn later.
    assert(IsAbove(10, 0u, 0, 20u));
    assert(!IsAbove(-1, 500u, 0, 0u));
    // Equal layer: the last submitted control owns the overlap.
    assert(IsAbove(0, 2u, 0, 1u));
    assert(!IsAbove(0, 1u, 0, 2u));
    assert(!IsAbove(0, 2u, 0, 2u));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            src = path / "layer_order.cpp"
            binary = path / "layer_order"
            src.write_text(code, encoding="utf-8")
            result = subprocess.run(
                [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ROOT / "engine/include"), str(src), "-o", str(binary)],
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            run = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)


if __name__ == "__main__":
    unittest.main()

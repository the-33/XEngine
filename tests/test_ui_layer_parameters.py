"""Compile the real inline per-element UI layer overloads without SDL2."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "engine/include/UIManager.h"


class UiLayerParametersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text(encoding="utf-8")

    def test_coverage_for_all_immediate_mode_element_types(self):
        wrappers = self.header.split("/** @name Individual UI layers", 1)[1].split(
            "/** @} */", 1
        )[0]
        for widget in (
            "Button", "InvisibleButton", "ImageButton", "Checkbox", "Slider",
            "Toggle", "Dropdown", "TextInput", "Label", "Panel", "Line",
            "Image", "ProgressBar",
        ):
            self.assertIn(f" {widget}(", wrappers, widget)
        self.assertIn("const UIRectTransform& layout, int layer", wrappers)
        self.assertIn("const Rect& r, int layer", wrappers)
        self.assertIn("std::forward<Args>(args)...", wrappers)

    def test_scope_restores_previous_layer_after_invocation(self):
        self.assertIn("struct RestoreLayer", self.header)
        self.assertIn("~RestoreLayer() { owner.PopLayer(); }", self.header)
        self.assertIn("return std::forward<Fn>(fn)();", self.header)

    def test_native_actual_header_templates_dispatch_and_restore(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("C++20 compiler unavailable")

        # Compile the *verbatim* production inline templates inside a minimal
        # stand-in UIManager. Actual SDL-backed methods cannot be linked here.
        start = self.header.index("    template <typename Fn>\n    auto InvokeOnLayer_")
        invoke = self.header[start:self.header.index("    void PushPanel_", start)]
        start = self.header.index("    /** @name Individual UI layers")
        wrappers = self.header[start:self.header.index("    /** @} */", start) + len("    /** @} */")]
        code = r'''
#include <cassert>
#include <string>
#include <utility>
#include <vector>
struct Rect {};
struct Vec2 { float x = 0.f, y = 0.f; };
struct UIRectTransform {};
struct Texture {};
struct UIStyle {};
struct UIButtonColors {};
struct Color { int r = 0, g = 0, b = 0, a = 255; };
enum class AlignH { Center };
enum class AlignV { Middle };
class UIManager {
    int layer = 0;
    std::vector<int> stack;
public:
    int drawnLayer = -999;
    void PushLayer(int value) noexcept { stack.push_back(layer); layer = value; }
    void PopLayer() noexcept { layer = stack.back(); stack.pop_back(); }
    void SetLayer(int value) noexcept { layer = value; }
    int GetLayer() const noexcept { return layer; }
    bool Button(const char*, const Rect&, const std::string& = "") noexcept {
        drawnLayer = layer; return true;
    }
    bool Button(const char*, const UIRectTransform&, const std::string& = "",
                const Rect* = nullptr) noexcept {
        drawnLayer = layer; return true;
    }
    bool Button(const char*, const Rect&, const UIStyle&,
                const std::string& = "") noexcept {
        drawnLayer = layer; return true;
    }
    bool InvisibleButton(const char*, const Rect&) noexcept {
        drawnLayer = layer; return true;
    }
    bool ImageButton(const char*, const Rect&, const Texture&) noexcept {
        drawnLayer = layer; return true;
    }
    bool Checkbox(const char*, const Rect&, bool*, const std::string& = "") noexcept {
        drawnLayer = layer; return true;
    }
    bool Slider(const char*, const Rect&, float*, float = 0.f,
                float = 1.f, float = 0.05f) noexcept {
        drawnLayer = layer; return true;
    }
    bool Dropdown(const char*, const UIRectTransform&, int*,
                  const std::vector<std::string>&, int = 6,
                  const Rect* = nullptr) noexcept {
        drawnLayer = layer; return true;
    }
    bool Toggle(const char*, const Rect&, bool*, const std::string& = "") noexcept {
        drawnLayer = layer; return true;
    }
    bool TextInput(const char*, const Rect&, std::string*,
                   const std::string& = "") noexcept {
        drawnLayer = layer; return true;
    }
    void Panel(const Rect&, Color) noexcept { drawnLayer = layer; }
    void Panel(const Rect&, const UIStyle&, bool = true) noexcept {
        drawnLayer = layer;
    }
    void Line(const UIRectTransform&, Color, float = 1.f,
              const Rect* = nullptr) noexcept { drawnLayer = layer; }
    void Line(Vec2, Vec2, Color, float = 1.f) noexcept { drawnLayer = layer; }
    void Label(const std::string&, Rect, AlignH, AlignV,
               float = -1.f) noexcept { drawnLayer = layer; }
    void Label(const std::string&, float, float) noexcept { drawnLayer = layer; }
    void Image(const Texture&, const Rect&) noexcept { drawnLayer = layer; }
    void ProgressBar(float, const Rect&) noexcept { drawnLayer = layer; }
'''+ invoke + wrappers + r'''
};
int main() {
    UIManager ui;
    Rect r;
    UIRectTransform layout;
    ui.SetLayer(3);
    assert(ui.Button("play", r, 10, "Play"));
    assert(ui.drawnLayer == 10 && ui.GetLayer() == 3);
    assert(ui.Button("anchored", layout, 12, "Next", &r));
    assert(ui.drawnLayer == 12 && ui.GetLayer() == 3);
    UIStyle style;
    assert(ui.Button("styled", r, 13, style, "Styled"));
    assert(ui.drawnLayer == 13 && ui.GetLayer() == 3);
    assert(ui.InvisibleButton("hotspot", r, 14));
    assert(ui.drawnLayer == 14 && ui.GetLayer() == 3);
    Texture texture;
    assert(ui.ImageButton("image_button", r, 11, texture));
    assert(ui.drawnLayer == 11 && ui.GetLayer() == 3);
    bool checked = false;
    assert(ui.Checkbox("check", r, 7, &checked));
    assert(ui.drawnLayer == 7 && ui.GetLayer() == 3);
    assert(ui.Toggle("toggle", r, 8, &checked));
    assert(ui.drawnLayer == 8 && ui.GetLayer() == 3);
    std::string name;
    assert(ui.TextInput("name", r, 9, &name));
    assert(ui.drawnLayer == 9 && ui.GetLayer() == 3);

    float volume = 0.5f;
    assert(ui.Slider("volume", r, -5, &volume, 0.f, 1.f, 0.05f));
    assert(ui.drawnLayer == -5 && ui.GetLayer() == 3);
    int selected = 0;
    std::vector<std::string> items{"A", "B"};
    assert(ui.Dropdown("resolution", layout, 15, &selected, items, 6, &r));
    assert(ui.drawnLayer == 15 && ui.GetLayer() == 3);

    ui.Panel(r, -2, Color{30, 30, 30, 255});
    assert(ui.drawnLayer == -2 && ui.GetLayer() == 3);
    ui.Panel(r, 6, style);
    assert(ui.drawnLayer == 6 && ui.GetLayer() == 3);
    ui.Line(layout, 1, Color{255, 255, 255, 255}, 2.f, &r);
    assert(ui.drawnLayer == 1 && ui.GetLayer() == 3);
    ui.Line(Vec2{}, Vec2{}, 4, Color{255, 255, 255, 255}, 2.f);
    assert(ui.drawnLayer == 4 && ui.GetLayer() == 3);
    ui.Label("Title", r, 8);
    assert(ui.drawnLayer == 8 && ui.GetLayer() == 3);
    ui.Label("XY", 20.f, 50.f, 2);
    assert(ui.drawnLayer == 2 && ui.GetLayer() == 3);
    ui.Image(texture, r, 17);
    assert(ui.drawnLayer == 17 && ui.GetLayer() == 3);
    ui.ProgressBar(0.5f, r, 19);
    assert(ui.drawnLayer == 19 && ui.GetLayer() == 3);

    ui.PushLayer(20);
    ui.Button("nested", r, 25, "Nested");
    assert(ui.drawnLayer == 25 && ui.GetLayer() == 20);
    ui.PopLayer();
    assert(ui.GetLayer() == 3);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            file = Path(tmp) / "inline_layers.cpp"
            executable = Path(tmp) / "inline_layers"
            file.write_text(code, encoding="utf-8")
            build = subprocess.run(
                [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                 str(file), "-o", str(executable)],
                capture_output=True, text=True,
            )
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)


if __name__ == "__main__":
    unittest.main()

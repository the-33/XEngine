"""Compile and exercise the real fluent UI header without requiring SDL2 SDK headers.

The fake UIManager only replaces SDL-dependent drawing calls; every widget,
canvas, setter, registry lookup and deferred event comes from UIWidgets.h.
"""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "engine/include/UIWidgets.h"

MANAGER = r'''
#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
struct Color { int r=0,g=0,b=0,a=255; };
struct Vec2 { float x=0, y=0; };
struct Rect { float x=0,y=0,w=0,h=0; };
struct Texture {};
struct UIRectTransform {
    Vec2 anchorMin{}, anchorMax{}, pivot{}, anchoredPosition{}, sizeDelta{};
};
struct UICanvas {};
enum class AlignH {Left, Center, Right};
enum class AlignV {Top, Middle, Bottom};
enum class UICornerRadiusUnit {Pixels, Percent};
struct UIButtonColors { Color normal{},hover{},active{},outline{}; };
struct UIWidgetInteraction {bool hovered=false,held=false,clicked=false,pointerDown=false,pointerUp=false;};
class UIWidgetCanvas;
class UIManager {
public:
    ~UIManager() noexcept;
    UIWidgetCanvas& CreateCanvas(const std::string&);
    UIWidgetCanvas* FindCanvas(std::string_view) noexcept;
    const UIWidgetCanvas* FindCanvas(std::string_view) const noexcept;
    bool DestroyCanvas(std::string_view) noexcept;
    void ClearWidgetCanvases() noexcept;
    void ClearSceneWidgetCanvases() noexcept;
    void DrawWidgetCanvases_() noexcept;
    std::unordered_map<std::string,std::unique_ptr<UIWidgetCanvas>> mWidgetCanvases;
    std::vector<std::string> mWidgetCanvasOrder;
    std::unordered_map<std::string,UIWidgetInteraction> mWidgetInteractions;
    UICanvas mCanvas{};
    template<typename T> T* Find(std::string_view) noexcept;
    template<typename T> const T* Find(std::string_view) const noexcept;
    struct UIStyle {
        Color text{}, btn{}, btnHot{}, btnActive{},outline{},focus{};
        Color checkboxCheck{},checkboxBackground{};
        Color sliderTrack{},sliderFill{},sliderHandle{},sliderTrackOutline{},sliderHandleOutline{};
        Color toggleOn{},toggleOff{};
        Color placeholder{},textSelection{};
        Color dropdownBackground{},dropdownSelected{},dropdownHovered{},dropdownArrow{};
        Color progressFill{},progressBack{},progressOutline{};
        float borderWidth=1, padding=0, sliderHandleDiameter=1,sliderTrackThickness=1;
        bool sliderRoundElements=false,sliderShowBackground=true;
        bool toggleShowBackground=true,toggleRoundElements=false,toggleTrackOnlyHit=false,toggleShowStateText=true;
        std::string defaultFontKey;
        UICornerRadiusUnit cornerRadiusUnit = UICornerRadiusUnit::Pixels;
        float radius[4]{};
        UIStyle& SetCornerRadius(float a) {radius[0]=radius[1]=radius[2]=radius[3]=a;return *this;}
        UIStyle& SetCornerRadius(float a,float b) {radius[0]=radius[1]=a; radius[2]=radius[3]=b;return *this;}
        UIStyle& SetCornerRadius(float a,float b,float c,float d) {radius[0]=a;radius[1]=b;radius[2]=c;radius[3]=d;return *this;}
    };
    static UIManager& GetInstance() {static UIManager self; return self;}
    UIStyle GetStyle() const {return {};}
    UICanvas GetCanvas() const {return {};}
    void SetCanvas(const UICanvas&) {}
    void PushLayer(int) {}
    void PopLayer() {}
    UIWidgetInteraction interaction{};
    bool click=false,sliderChanged=false,textChanged=false;
    bool editing=false,submitted=false,open=false;
    int absolute=0,anchored=0;
    UIWidgetInteraction GetWidgetInteraction(std::string_view) const {return interaction;}
    bool IsFocused(const char*) const {return false;}
    bool IsTextEditing(std::string_view) const {return editing;}
    bool WasTextSubmitted(std::string_view) const {return submitted;}
    bool IsDropdownOpen(std::string_view) const {return open;}
    bool Button(const char*, Rect, const UIStyle&, const std::string&) {++absolute;return click;}
    bool Button(const char*, UIRectTransform, const UIStyle&,const std::string&, const Rect*) {++anchored;return click;}
    bool ImageButton(const char*,Rect,const Texture&,const UIStyle&,const Rect*,const UIButtonColors&,bool) {return click;}
    bool ImageButton(const char*,UIRectTransform,const Texture&,const UIStyle&,const Rect*,const UIButtonColors&,bool,const Rect*) {return click;}
    void Panel(Rect,const UIStyle&,bool) {}
    void Panel(UIRectTransform,const UIStyle&,bool,const Rect*) {}
    void Label(const std::string&,Rect,const UIStyle&,AlignH,AlignV,float) {}
    void Label(const std::string&,UIRectTransform,const UIStyle&,AlignH,AlignV,float,const Rect*) {}
    bool Checkbox(const char*,Rect,bool*,const UIStyle&,const std::string&) {return false;}
    bool Checkbox(const char*,UIRectTransform,bool*,const UIStyle&,const std::string&,const Rect*) {return false;}
    bool Slider(const char*,Rect,float* value,const UIStyle&,float,float,float) {
        if (sliderChanged) {*value=0.75f;return true;} return false;
    }
    bool Slider(const char*,UIRectTransform,float* value,const UIStyle&,float,float,float,const Rect*) {
        if (sliderChanged) {*value=0.75f;return true;} return false;
    }
    bool Toggle(const char*,Rect,bool*,const UIStyle&,const std::string&) {return false;}
    bool Toggle(const char*,UIRectTransform,bool*,const UIStyle&,const std::string&,const Rect*) {return false;}
    bool TextInput(const char*,Rect,std::string* value,const UIStyle&,const std::string&,size_t) {
        if (textChanged) {*value="Edited";return true;} return false;
    }
    bool TextInput(const char*,UIRectTransform,std::string* value,const UIStyle&,const std::string&,size_t,const Rect*) {
        if (textChanged) {*value="Edited";return true;} return false;
    }
    bool Dropdown(const char*,Rect,int*,const std::vector<std::string>&,const UIStyle&,int) {return false;}
    bool Dropdown(const char*,UIRectTransform,int*,const std::vector<std::string>&,const UIStyle&,int,const Rect*) {return false;}
    void ProgressBar(float,Rect,const UIStyle&) {}
    void ProgressBar(float,UIRectTransform,const UIStyle&,const Rect*) {}
    void Image(const Texture&,Rect,const Rect*,Color,bool,bool) {}
    void Image(const Texture&,UIRectTransform,const Rect*,Color,bool,bool,const Rect*) {}
    void Line(Vec2,Vec2,Color,float) {}
    void Line(UIRectTransform,Color,float,const Rect*) {}
    Rect Resolve(const UIRectTransform&, const Rect*) const {return {};}
    bool InvisibleButton(const char*,Rect) {return click;}
};
'''

CHECK = r'''
#include <cassert>
#include <type_traits>
#include "UIWidgets_test.h"
static_assert(std::is_same_v<decltype(std::declval<UIButton&>().SetCornerRadius(1.f).SetText("x")),UIButton&>);
static_assert(std::is_same_v<decltype(std::declval<UIDropdown&>().SetCornerRadius(1.f,2.f).SetOptions({})),UIDropdown&>);
static_assert(std::is_same_v<decltype(std::declval<UITextInput&>().SetCornerRadius(1.f,2.f,3.f,4.f).SetPlaceholder("x")),UITextInput&>);
static_assert(std::is_same_v<decltype(std::declval<UISlider&>().SetColor({}).SetStep(0.1f)),UISlider&>);
template <typename T> concept HasRadius = requires(T& w) { w.SetCornerRadius(1.f); };
template <typename T> concept HasBorder = requires(T& w) { w.SetBorderWidth(2.f); };
static_assert(!HasRadius<UILabel>);
static_assert(!HasBorder<UIImage>);
int main() {
    auto& ui = UIManager::GetInstance();
    UIWidgetCanvas canvas("MainMenu", {});
    int clicks=0, enters=0;
    auto& button = canvas.Create<UIButton>("play", Rect{1,2,3,4});
    button.SetText("Play").SetCornerRadius(12.f, 0.f).SetLayer(7)
          .OnClick([&] {++clicks;}).OnHoverEnter([&] {++enters;});
    assert(canvas.Find<UIButton>("play")==&button);
    assert(canvas.Find<UISlider>("play")==nullptr);
    assert(button.RuntimeId()=="MainMenu/play");
    assert(button.GetStyle().radius[0]==12.f && button.GetStyle().radius[2]==0.f);
    bool duplicate=false;
    try {canvas.Create<UIButton>("play", Rect{});}catch(const std::invalid_argument&){duplicate=true;}
    assert(duplicate);
    UIWidgetEvents events;
    ui.click=true;ui.interaction.hovered=true;
    canvas.Submit(ui,events);
    assert(clicks==0 && enters==0 && events.size()==2);
    for(auto& e:events)e();
    assert(clicks==1 && enters==1);
    events.clear();ui.click=false;
    canvas.Submit(ui,events);
    assert(events.empty()); // no duplicate hover event
    button.SetLayout({});
    canvas.Submit(ui,events);
    assert(ui.anchored>=1);
    auto& slider=canvas.Create<UISlider>("volume", Rect{});
    int changed=0;float latest=0;
    slider.SetRange(0.f,1.f).SetStep(0.05f).OnValueChanged([&](float value){++changed;latest=value;});
    ui.sliderChanged=true;
    events.clear();canvas.Submit(ui,events);
    assert(changed==0);
    for(auto& e:events)e();
    assert(changed==1 && latest==0.75f);
    slider.SetValueWithoutNotify(0.2f);assert(changed==1);
    slider.SetValue(0.4f);assert(changed==2);
    auto& field=canvas.Create<UITextInput>("name", Rect{});
    int edits=0, submits=0;
    field.OnEndEdit([&](const std::string&){++edits;})
         .OnSubmit([&](const std::string&){++submits;});
    ui.editing=true;ui.sliderChanged=false;events.clear();canvas.Submit(ui,events);
    ui.editing=false;ui.submitted=true;events.clear();canvas.Submit(ui,events);
    for(auto& e:events)e();
    assert(edits==1 && submits==1);
    assert(canvas.Remove("play"));assert(canvas.Find<UIButton>("play")==nullptr);
    canvas.Clear();assert(canvas.Find<UISlider>("volume")==nullptr);
    auto& live=ui.CreateCanvas("Settings");
    assert(&live==&ui.CreateCanvas("Settings"));
    auto& title=live.Create<UILabel>("title",Rect{});
    live.Create<UILine>("divider", Rect{0,0,100,3}).SetColor({255,255,255,255}).SetThickness(2.f);
    live.Create<UIInvisibleButton>("hit", Rect{0,0,20,20}).OnClick([&] {++clicks;});
    assert(ui.Find<UILabel>("Settings/title")==&title);
    assert(ui.Find<UIButton>("Settings/title")==nullptr);
    ui.DrawWidgetCanvases_();
    assert(ui.DestroyCanvas("Settings"));
    assert(ui.Find<UILabel>("Settings/title")==nullptr);
    auto& temp=ui.CreateCanvas("Temp");
    auto& saved=ui.CreateCanvas("Persistent");
    saved.SetPersistent(true);
    (void) temp;
    ui.ClearSceneWidgetCanvases();
    assert(ui.FindCanvas("Temp")==nullptr && ui.FindCanvas("Persistent")!=nullptr);
    ui.ClearWidgetCanvases();
    assert(ui.FindCanvas("Persistent")==nullptr);
    return 0;
}
'''


class UIWidgetCanvasTests(unittest.TestCase):
    def test_find_doxygen_docs_exist_only_at_declarations(self):
        """Avoid double @param tags when Doxygen merges declaration and definition."""
        manager = (ROOT / "engine/include/UIManager.h").read_text(encoding="utf-8")
        implementation = HEADER.read_text(encoding="utf-8")
        self.assertEqual(manager.count("@param path Qualified widget identifier."), 2)
        self.assertEqual(len(re.findall(
            r"\bUIManager::Find\(std::string_view path\)(?: const)? noexcept",
            implementation,
        )), 2)
        self.assertNotRegex(
            implementation,
            r"/\*\*(?:(?!/\*|\*/).)*@param\s+path(?:(?!/\*|\*/).)*\*/"
            r"\s*template\s*<typename\s+T>\s*(?:const\s+)?T\*\s*UIManager::Find",
        )

    def test_draw_content_doxygen_parameter_names_match_signatures(self):
        """Doxygen treats a documented but unnamed argument as an error."""
        source = HEADER.read_text(encoding="utf-8")
        declarations = list(re.finditer(
            r"/\*\*((?:(?!/\*|\*/).)*)\*/\s*(?:virtual\s+)?void\s+DrawContent\((.*?)\)",
            source, re.DOTALL,
        ))
        self.assertGreaterEqual(len(declarations), 13)
        for declaration in declarations:
            comment, signature = declaration.groups()
            documented = re.findall(r"@param\s+(\w+)", comment)
            signature = re.sub(r"\[\[.*?\]\]\s*", "", signature)
            named = [
                parameter.split()[-1].lstrip("&*")
                for parameter in signature.split(",")
            ]
            self.assertEqual(documented, named, f"Doxygen mismatch in DrawContent({signature})")

    def test_real_header_compiles_and_widgets_dispatch_events(self):
        compiler = shutil.which("g++")
        if not compiler:
            self.skipTest("g++ not installed")
        with tempfile.TemporaryDirectory(prefix="xengine-ui-widgets-") as td:
            folder = Path(td)
            (folder / "UIManagerStub.h").write_text(MANAGER, encoding="utf-8")
            (folder / "UIWidgets_test.h").write_text(
                HEADER.read_text(encoding="utf-8").replace(
                    '#include "UIManager.h"', '#include "UIManagerStub.h"'),
                encoding="utf-8")
            (folder / "check.cpp").write_text(CHECK, encoding="utf-8")
            (folder / "UIWidgets_impl.cpp").write_text(
                (ROOT / "engine/src/UIWidgets.cpp").read_text(encoding="utf-8").replace(
                    '#include "UIWidgets.h"', '#include "UIWidgets_test.h"'),
                encoding="utf-8")
            build = subprocess.run(
                [compiler, "-std=c++20", "-Wall", "-Wextra", "-pedantic-errors",
                 "-I", str(folder), str(folder / "check.cpp"),
                 str(folder / "UIWidgets_impl.cpp"), "-o", str(folder / "check")],
                capture_output=True, text=True, timeout=45,
            )
            self.assertEqual(build.returncode, 0, build.stderr)
            result = subprocess.run([str(folder / "check")], capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_engine_integration_and_classic_api_remain(self):
        manager = (ROOT / "engine/include/UIManager.h").read_text(encoding="utf-8")
        cpp = (ROOT / "engine/src/UIManager.cpp").read_text(encoding="utf-8")
        extra = (ROOT / "engine/src/UIWidgets.cpp").read_text(encoding="utf-8")
        scene = (ROOT / "engine/src/SceneManager.cpp").read_text(encoding="utf-8")
        self.assertIn("DrawWidgetCanvases_();", cpp)
        self.assertIn("UIWidgetInteraction UIManager::GetWidgetInteraction", cpp)
        self.assertIn("void UIManager::ClearSceneWidgetCanvases()", extra)
        self.assertIn("ui->ClearSceneWidgetCanvases();", scene)
        self.assertIn("bool Button(const char* id, const Rect& r, const std::string& text", manager)


if __name__ == "__main__":
    unittest.main()

#pragma once

#include <vector>
#include <string>

#include "Singleton.h"
#include "BaseTypes.h"
#include "Assets.h"
#include "Engine.h"

enum class AlignH { Left, Center, Right };
enum class AlignV { Top, Middle, Bottom };

struct UIButtonColors {
    Color normal;
    Color hover;
    Color active;
    Color outline;
};

class UIManager : public Singleton<UIManager>
{
    friend class Singleton<UIManager>;
    friend class Engine;

private:
    // --- Estilo global overrideable ---
    struct UIStyle {
        Color text{ 240,240,240,255 };
        Color btn{ 60,60,60,255 };
        Color btnHot{ 80,80,80,255 };
        Color btnActive{ 40,40,40,255 };
        Color outline{ 200,200,200,255 };
        float padding = 6.f;
        std::string defaultFontKey = "ui_default"; // <-- NO const, así puedes cambiarlo
    };

    enum class CmdType { Rect, Text, Image, TextInRect };

    struct DrawCmd {
        CmdType type = CmdType::Rect;

        Rect r{};
        Color color{ 255,255,255,255 };

        // Text
        std::string text;
        Font* font = nullptr;

        // Rect
        bool filled = true;

        // Image
        const Texture* tex = nullptr;
        Rect src{};
        bool hasSrc = false;
        bool flipX = false;
        bool flipY = false;

        //Text in rect
        AlignH hAlign = AlignH::Left;
        AlignV vAlign = AlignV::Top;
        float padding = 0.f;
    };

    struct ButtonState {
        bool hovered = false;
        bool held = false;     // active + mouse down
        bool pressed = false;  // click (release on hovered while active)
    };

    std::vector<DrawCmd> mCmds;

    Color mTextDefaultColor = { 240,240,240,255 };
    Color mBtnDefaultColor = { 60,60,60,255 };
    Color mBtnHotDefaultColor = { 80,80,80,255 };
    Color mBtnActiveDefaultColor = { 40,40,40,255 };
    Color mBtnOutlineDefaultColor = { 200,200,200,255 };
    float mBtnDefaultPadding = 6.f;

    Font* mDefaultFont = nullptr;

    // Helpers internos
    static bool PointInRect_(const Vec2I& p, const Rect& r) noexcept;
    ButtonState ButtonBehavior_(const char* id, const Rect& r) noexcept;

    void PushPanel_(const Rect& r, Color fill, Color outline, bool filled = true) noexcept;
    void PushText_(const std::string& text, const Rect& r, Font* font, Color color) noexcept;
    void PushImage_(const Texture& tex, const Rect& dst, const Rect* srcPixels, Color tint, bool flipX, bool flipY) noexcept;
    void PushTextInRect_(const Rect& r, Font* font, const std::string& text, Color color, AlignH h, AlignV v, float padding) noexcept;

    bool ButtonImpl_(const char* id, const Rect& r, Font* font, const UIButtonColors& colors, const std::string& text) noexcept;

    bool Init(const UIStyle& defaultStyle) noexcept;
    void Shutdown() noexcept;

    void Begin() noexcept;
    void End() noexcept;

public:
    // --------------------------
    // Buttons
    // --------------------------
    bool Button(const char* id, const Rect& r, const std::string& text = "") noexcept; // default font + default colors
    bool Button(const char* id, const Rect& r, const Font& font, const std::string& text = "") noexcept; // specified font + default colors
    bool Button(const char* id, const Rect& r, const UIButtonColors& colors, const std::string& text = "") noexcept; // default font + specified colors
    bool Button(const char* id, const Rect& r, const Font& font, const UIButtonColors& colors, const std::string& text = "") noexcept; // specified font + specified colors

    bool Button(const char* id, const Rect& r, const std::string& text, AlignH hAlign, AlignV vAlign, float textPadding = -1.f) noexcept;
    bool Button(const char* id, const Rect& r, const Font& font, const std::string& text, AlignH hAlign, AlignV vAlign, float textPadding = -1.f) noexcept;
    bool Button(const char* id, const Rect& r, const UIButtonColors& colors, const std::string& text, AlignH hAlign, AlignV vAlign, float textPadding = -1.f) noexcept;
    bool Button(const char* id, const Rect& r, const Font& font, const UIButtonColors& colors, const std::string& text, AlignH hAlign, AlignV vAlign, float textPadding = -1.f) noexcept;


    bool InvisibleButton(const char* id, const Rect& r) noexcept; // solo interacción, no dibuja

    // ImageButton: usa UIButtonColors como "tints" (normal/hover/active) y outline como marco
    bool ImageButton(const char* id,
        const Rect& r,
        const Texture& tex,
        const Rect* srcPixels = nullptr,
        const UIButtonColors& tints = UIButtonColors{
            {255,255,255,255}, {230,230,230,255}, {200,200,200,255}, {200,200,200,0}
        },
        bool drawFrame = true) noexcept;

    // --------------------------
    // Text utils
    // --------------------------
    void Label(const std::string& text, float x, float y) noexcept;
    void Label(const std::string& text, float x, float y, const Font& font) noexcept;
    void Label(const std::string& text, float x, float y, const Color& color) noexcept;
    void Label(const std::string& text, float x, float y, const Font& font, const Color& color) noexcept;

    void Label(const std::string& text, Rect rect, AlignH hAlign, AlignV vAlign, float padding = -1.f) noexcept;
    void Label(const std::string& text, Rect rect, const Font& font, AlignH hAlign, AlignV vAlign, float padding = -1.f) noexcept;
    void Label(const std::string& text, Rect rect, const Color& color, AlignH hAlign, AlignV vAlign, float padding = -1.f) noexcept;
    void Label(const std::string& text, Rect rect, const Font& font, const Color& color, AlignH hAlign, AlignV vAlign, float padding = -1.f) noexcept;

    // --------------------------
    // Panels
    // --------------------------
    void Panel(const Rect& r, Color fill, Color outline, bool filled = true) noexcept;
    void Panel(const Rect& r, Color fill) noexcept; // sin outline (rápido)

    // --------------------------
    // Images/Textures UI
    // --------------------------
    void Image(const Texture& tex,
        const Rect& dstScreen,
        const Rect* srcPixels = nullptr,
        Color tint = { 255,255,255,255 },
        bool flipX = false,
        bool flipY = false) noexcept;

    // --------------------------
    // Utilidades para construir widgets
    // --------------------------

    bool Checkbox(const char* id, const Rect& r, bool* value, const std::string& label = "") noexcept;

    void ProgressBar(float value01,
        const Rect& r,
        Color back = { 40,40,40,255 },
        Color fill = { 120,200,120,255 },
        Color outline = { 200,200,200,255 }) noexcept;
};
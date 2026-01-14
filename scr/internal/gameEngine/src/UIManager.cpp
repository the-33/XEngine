#include "UIManager.h"

#include <cstring>
#include <string>
#include <algorithm>   // std::clamp

#include "RenderManager.h"
#include "InputManager.h"
#include "AssetManager.h"
#include "ErrorHandler.h"

// Estado global IMGUI (tu estilo actual)
static const char* gHotID = nullptr;
static const char* gActiveID = nullptr;
static bool        gMouseDown = false;
static bool        gMousePressed = false;
static bool        gMouseReleased = false;
static Vec2I        gMousePos{ 0,0 };

bool UIManager::Init(const UIStyle& defaultStyle) noexcept
{
    mTextDefaultColor = defaultStyle.text;
    mBtnDefaultColor = defaultStyle.btn;
    mBtnHotDefaultColor = defaultStyle.btnHot;
    mBtnActiveDefaultColor = defaultStyle.btnActive;
    mBtnOutlineDefaultColor = defaultStyle.outline;
    mBtnDefaultPadding = defaultStyle.padding;

    mDefaultFont = AssetManager::GetInstancePtr()->GetFontByKey(defaultStyle.defaultFontKey);
    if (!mDefaultFont) {
        LogError("UIManager warning",
            std::string("Init(): Could not find font asset '") + defaultStyle.defaultFontKey +
            "'. Labels/Buttons without explicit font will not show text.");
    }

    gHotID = nullptr;
    gActiveID = nullptr;
    gMouseDown = gMousePressed = gMouseReleased = false;
    gMousePos = { 0,0 };
    mCmds.clear();

    return true;
}

void UIManager::Shutdown() noexcept
{
    mCmds.clear();
    mDefaultFont = nullptr;

    gHotID = nullptr;
    gActiveID = nullptr;
    gMouseDown = gMousePressed = gMouseReleased = false;
    gMousePos = { 0,0 };
}

void UIManager::Begin() noexcept
{
    gHotID = nullptr;

    if (auto* input = InputManager::GetInstancePtr()) {
        gMousePos = input->GetMousePos();
        gMouseDown = input->MouseDown(SDL_BUTTON_LEFT);
        gMousePressed = input->MousePressed(SDL_BUTTON_LEFT);
        gMouseReleased = input->MouseReleased(SDL_BUTTON_LEFT);
    }
    else {
        gMousePos = { 0,0 };
        gMouseDown = gMousePressed = gMouseReleased = false;
    }

    mCmds.clear();
}

// --------------------------
// Helpers
// --------------------------
bool UIManager::PointInRect_(const Vec2I& p, const Rect& r) noexcept
{
    return (p.x >= r.x && p.x <= r.x + r.w &&
        p.y >= r.y && p.y <= r.y + r.h);
}

UIManager::ButtonState UIManager::ButtonBehavior_(const char* id, const Rect& r) noexcept
{
    ButtonState st{};
    if (!id) return st;

    st.hovered = PointInRect_(gMousePos, r);
    if (st.hovered) gHotID = id;

    if (st.hovered && gMousePressed)
        gActiveID = id;

    st.held = (gActiveID == id && gMouseDown);

    // Click = release mientras está activo y encima
    if (gMouseReleased && gActiveID == id && st.hovered) {
        st.pressed = true;
        gActiveID = nullptr;
    }

    // Release fuera: suelta igualmente
    if (gMouseReleased && gActiveID == id && !st.hovered) {
        gActiveID = nullptr;
    }

    return st;
}

void UIManager::PushPanel_(const Rect& r, Color fill, Color outline, bool filled) noexcept
{
    // Fondo
    if (filled) {
        DrawCmd a;
        a.type = CmdType::Rect;
        a.r = r;
        a.color = fill;
        a.filled = true;
        mCmds.push_back(std::move(a));
    }

    // Contorno
    DrawCmd b;
    b.type = CmdType::Rect;
    b.r = r;
    b.color = outline;
    b.filled = false;
    mCmds.push_back(std::move(b));
}

void UIManager::PushText_(const std::string& text, const Rect& r, Font* font, Color color) noexcept
{
    if (!font || text.empty()) return;

    DrawCmd c;
    c.type = CmdType::Text;
    c.r = r;
    c.color = color;
    c.text = text;
    c.font = font;
    c.filled = false;
    mCmds.push_back(std::move(c));
}

void UIManager::PushImage_(const Texture& tex, const Rect& dst, const Rect* srcPixels, Color tint, bool flipX, bool flipY) noexcept
{
    DrawCmd c;
    c.type = CmdType::Image;
    c.r = dst;
    c.color = tint;
    c.tex = &tex;
    c.flipX = flipX;
    c.flipY = flipY;

    if (srcPixels) {
        c.src = *srcPixels;
        c.hasSrc = true;
    }
    mCmds.push_back(std::move(c));
}

void UIManager::PushTextInRect_(const Rect& r, Font* font, const std::string& text,
    Color color, AlignH h, AlignV v, float padding) noexcept
{
    if (!font || text.empty()) return;

    DrawCmd cmd;
    cmd.type = CmdType::TextInRect;
    cmd.r = r;
    cmd.color = color;
    cmd.text = text;
    cmd.font = font;
    cmd.filled = false;
    cmd.hAlign = h;
    cmd.vAlign = v;
    cmd.padding = padding;

    mCmds.push_back(std::move(cmd));
}

// --------------------------
// BUTTONS (re-implementados sin UB de Font nullptr)
// --------------------------
bool UIManager::ButtonImpl_(const char* id, const Rect& r, Font* font, const UIButtonColors& colors, const std::string& text) noexcept
{
    if (!id || !RenderManager::GetInstancePtr()) return false;

    auto st = ButtonBehavior_(id, r);

    Color fill = colors.normal;
    if (st.held)        fill = colors.active;
    else if (st.hovered) fill = colors.hover;

    PushPanel_(r, fill, colors.outline, /*filled*/true);

    // Texto centrado
    Font* use = font ? font : mDefaultFont;
    const std::string label = (!text.empty()) ? text : std::string(id);

    if (use && !label.empty())
    {
        PushTextInRect_(r, use, label, mTextDefaultColor,
            AlignH::Center, AlignV::Middle, mBtnDefaultPadding);
    }

    return st.pressed;
}

bool UIManager::Button(const char* id, const Rect& r, const std::string& text) noexcept
{
    return ButtonImpl_(id, r, mDefaultFont,
        UIButtonColors{ mBtnDefaultColor, mBtnHotDefaultColor, mBtnActiveDefaultColor, mBtnOutlineDefaultColor },
        text);
}

bool UIManager::Button(const char* id, const Rect& r, const Font& font, const std::string& text) noexcept
{
    return ButtonImpl_(id, r, const_cast<Font*>(&font),
        UIButtonColors{ mBtnDefaultColor, mBtnHotDefaultColor, mBtnActiveDefaultColor, mBtnOutlineDefaultColor },
        text);
}

bool UIManager::Button(const char* id, const Rect& r, const UIButtonColors& colors, const std::string& text) noexcept
{
    return ButtonImpl_(id, r, mDefaultFont, colors, text);
}

bool UIManager::Button(const char* id, const Rect& r, const Font& font, const UIButtonColors& colors, const std::string& text) noexcept
{
    return ButtonImpl_(id, r, const_cast<Font*>(&font), colors, text);
}

// --------------------------
// LABELS
// --------------------------
void UIManager::Label(const std::string& text, float x, float y) noexcept
{
    if (!mDefaultFont) return;
    Label(text, x, y, *mDefaultFont, mTextDefaultColor);
}

void UIManager::Label(const std::string& text, float x, float y, const Font& font) noexcept
{
    Label(text, x, y, font, mTextDefaultColor);
}

void UIManager::Label(const std::string& text, float x, float y, const Color& color) noexcept
{
    if (!mDefaultFont) return;
    Label(text, x, y, *mDefaultFont, color);
}

void UIManager::Label(const std::string& text, float x, float y, const Font& font, const Color& color) noexcept
{
    if (!RenderManager::GetInstancePtr() || text.empty()) return;

    Font* f = const_cast<Font*>(&font);
    if (!f) f = mDefaultFont;
    if (!f) return;

    Rect r{ x, y, 0, 0 };
    PushText_(text, r, f, color);
}

void UIManager::Label(const std::string& text, Rect rect, AlignH hAlign, AlignV vAlign, float padding) noexcept
{
    if (!mDefaultFont) return;
    Label(text, rect, *mDefaultFont, mTextDefaultColor, hAlign, vAlign, padding);
}

void UIManager::Label(const std::string& text, Rect rect, const Font& font, AlignH hAlign, AlignV vAlign, float padding) noexcept
{
    Label(text, rect, font, mTextDefaultColor, hAlign, vAlign, padding);
}

void UIManager::Label(const std::string& text, Rect rect, const Color& color, AlignH hAlign, AlignV vAlign, float padding) noexcept
{
    if (!mDefaultFont) return;
    Label(text, rect, *mDefaultFont, color, hAlign, vAlign, padding);
}

void UIManager::Label(const std::string& text, Rect rect, const Font& font, const Color& color, AlignH hAlign, AlignV vAlign, float padding) noexcept
{
    if (!RenderManager::GetInstancePtr() || text.empty()) return;

    Font* f = const_cast<Font*>(&font);
    if (!f) f = mDefaultFont;
    if (!f) return;

    PushTextInRect_(rect, f, text, color, hAlign, vAlign, padding);
}

// --------------------------
// NUEVO: PANELS
// --------------------------
void UIManager::Panel(const Rect& r, Color fill, Color outline, bool filled) noexcept
{
    PushPanel_(r, fill, outline, filled);
}

void UIManager::Panel(const Rect& r, Color fill) noexcept
{
    // panel rápido sin outline (o outline transparente)
    DrawCmd c;
    c.type = CmdType::Rect;
    c.r = r;
    c.color = fill;
    c.filled = true;
    mCmds.push_back(std::move(c));
}

// --------------------------
// NUEVO: IMAGES
// --------------------------
void UIManager::Image(const Texture& tex, const Rect& dst, const Rect* srcPixels, Color tint, bool flipX, bool flipY) noexcept
{
    if (!RenderManager::GetInstancePtr()) return;
    PushImage_(tex, dst, srcPixels, tint, flipX, flipY);
}

bool UIManager::ImageButton(const char* id, const Rect& r, const Texture& tex, const Rect* srcPixels,
    const UIButtonColors& tints, bool drawFrame) noexcept
{
    if (!id || !RenderManager::GetInstancePtr()) return false;

    auto st = ButtonBehavior_(id, r);

    Color tint = tints.normal;
    if (st.held)        tint = tints.active;
    else if (st.hovered) tint = tints.hover;

    if (drawFrame) {
        // Marco (y si quieres, puedes meter un panel tenue detrás)
        PushPanel_(r, Color{ 0,0,0,0 }, tints.outline, /*filled*/false);
    }

    PushImage_(tex, r, srcPixels, tint, /*flipX*/false, /*flipY*/false);

    return st.pressed;
}

bool UIManager::Button(const char* id, const Rect& r, const std::string& text, AlignH hAlign, AlignV vAlign, float textPadding) noexcept
{
    UIButtonColors colors{ mBtnDefaultColor, mBtnHotDefaultColor, mBtnActiveDefaultColor, mBtnOutlineDefaultColor };

    const std::string safeText = (mDefaultFont ? text : std::string());

    return Button(id, r, *mDefaultFont,
        colors, safeText, hAlign, vAlign, textPadding);
}

bool UIManager::Button(const char* id, const Rect& r, const Font& font, const std::string& text, AlignH hAlign, AlignV vAlign, float textPadding) noexcept
{
    UIButtonColors colors{ mBtnDefaultColor, mBtnHotDefaultColor, mBtnActiveDefaultColor, mBtnOutlineDefaultColor };

    return Button(id, r, font,
    colors, text, hAlign, vAlign, textPadding);
}

bool UIManager::Button(const char* id, const Rect& r, const UIButtonColors& colors, const std::string& text, AlignH hAlign, AlignV vAlign, float textPadding) noexcept
{
    const std::string safeText = (mDefaultFont ? text : std::string());

    return Button(id, r, *mDefaultFont,
        colors, safeText, hAlign, vAlign, textPadding);
}

bool UIManager::Button(const char* id, const Rect& r,
    const Font& font, const UIButtonColors& colors,
    const std::string& text,
    AlignH hAlign, AlignV vAlign, float textPadding) noexcept
{
    RenderManager* render = RenderManager::GetInstancePtr();
    if (!id || !render) return false;

    const bool hovered =
        (gMousePos.x >= r.x && gMousePos.x <= r.x + r.w &&
            gMousePos.y >= r.y && gMousePos.y <= r.y + r.h);

    if (hovered) gHotID = id;
    if (hovered && gMousePressed) gActiveID = id;

    bool clicked = false;
    if (gMouseReleased && gActiveID == id && hovered) { clicked = true; gActiveID = nullptr; }
    if (gMouseReleased && gActiveID == id && !hovered) { gActiveID = nullptr; }

    Color fill = colors.normal;
    if (gActiveID == id && gMouseDown) fill = colors.active;
    else if (hovered)                 fill = colors.hover;

    PushPanel_(r, fill, colors.outline);

    // Texto con alineación
    Font* use = const_cast<Font*>(&font);
    if (!use) use = mDefaultFont;
    const std::string label = !text.empty() ? text : std::string(id ? id : "");

    if (use && !label.empty())
    {
        const float pad = (textPadding >= 0.f) ? textPadding : mBtnDefaultPadding;
        PushTextInRect_(r, use, label, mTextDefaultColor, hAlign, vAlign, pad);
    }

    return clicked;
}

// --------------------------
// NUEVO: building blocks
// --------------------------
bool UIManager::InvisibleButton(const char* id, const Rect& r) noexcept
{
    if (!id) return false;
    auto st = ButtonBehavior_(id, r);
    return st.pressed;
}

bool UIManager::Checkbox(const char* id, const Rect& r, bool* value, const std::string& label) noexcept
{
    if (!id || !value) return false;

    // r = área total clicable (incluye texto). Caja = cuadrado de lado r.h a la izquierda
    const float boxSize = r.h;
    Rect box{ r.x, r.y, boxSize, boxSize };

    // Interacción: click en todo r
    const bool toggled = InvisibleButton(id, r);
    if (toggled) *value = !*value;

    // Dibujo caja
    Panel(box, Color{ 20,20,20,255 }, mBtnOutlineDefaultColor, /*filled*/true);

    if (*value) {
        const float pad = std::max(2.f, mBtnDefaultPadding * 0.35f);
        Rect inner{ box.x + pad, box.y + pad, box.w - pad * 2.f, box.h - pad * 2.f };
        Panel(inner, Color{ 180,180,180,255 });
    }

    // Texto a la derecha
    if (!label.empty()) {
        const float tx = r.x + boxSize + mBtnDefaultPadding;
        const float ty = r.y + (r.h * 0.5f); // lo centramos usando TextCentered en un rect
        Rect tr{ tx, r.y, r.w - (boxSize + mBtnDefaultPadding), r.h };
        Label(label, tr, AlignH::Center, AlignV::Middle);
    }

    return toggled;
}

void UIManager::ProgressBar(float value01, const Rect& r, Color back, Color fill, Color outline) noexcept
{
    value01 = std::clamp(value01, 0.f, 1.f);

    // fondo + outline
    PushPanel_(r, back, outline, /*filled*/true);

    // relleno
    Rect fr = r;
    fr.w = r.w * value01;
    Panel(fr, fill);
}

// --------------------------
// END (Flush draw-list)
// --------------------------
void UIManager::End() noexcept
{
    RenderManager* render = RenderManager::GetInstancePtr();
    if (!render) { mCmds.clear(); return; }

    for (const auto& c : mCmds) {
        switch (c.type) {
        case CmdType::Rect:
            render->DrawRectScreen(c.r, c.color, c.filled);
            break;

        case CmdType::Text:
            if (c.font) render->DrawTextScreen(*c.font, c.text, c.r.x, c.r.y, c.color);
            break;
        case CmdType::Image:
            if (c.tex) {
                render->DrawImageScreen(*c.tex, c.r,
                    c.hasSrc ? &c.src : nullptr,
                    c.color, c.flipX, c.flipY);
            }
            break;
        case CmdType::TextInRect:
            if (c.font && !c.text.empty()) {
                int tw = 0, th = 0;
                auto* ttf = c.font->GetSDL();
                if (ttf && TTF_SizeUTF8(ttf, c.text.c_str(), &tw, &th) == 0) {

                    float x = c.r.x;
                    float y = c.r.y;

                    // Horizontal
                    switch (c.hAlign) {
                    case AlignH::Left:   x = c.r.x + c.padding; break;
                    case AlignH::Center: x = c.r.x + (c.r.w - tw) * 0.5f; break;
                    case AlignH::Right:  x = c.r.x + c.r.w - tw - c.padding; break;
                    }

                    // Vertical
                    switch (c.vAlign) {
                    case AlignV::Top:    y = c.r.y + c.padding; break;
                    case AlignV::Middle: y = c.r.y + (c.r.h - th) * 0.5f; break;
                    case AlignV::Bottom: y = c.r.y + c.r.h - th - c.padding; break;
                    }

                    render->DrawTextScreen(*c.font, c.text, x, y, c.color);
                }
            }
            break;
        }
    }

    mCmds.clear();

    if (gMouseReleased && gActiveID && !gMouseDown)
        gActiveID = nullptr;
}
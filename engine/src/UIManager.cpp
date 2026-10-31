#include "UIManager.h"

#include <cstring>
#include <string>
#include <optional>
#include <algorithm>   // std::clamp
#include <cmath>
#include <string_view>

#include "RenderManager.h"
#include "InputManager.h"
#include "WindowManager.h"
#include "AssetManager.h"
#include "ErrorHandler.h"
#include "SDL.h"
#include "SDL_ttf.h"

// State global IMGUI (tu estilo actual)
static std::string gHotID;
static std::optional<std::string> gActiveID;
static bool        gMouseDown = false;
static bool        gMousePressed = false;
static bool        gMouseReleased = false;
static Vec2I        gMousePos{ 0,0 };

namespace {
struct ScopedCanvasTextScale {
    float& current;
    float previous;
    ScopedCanvasTextScale(float& slot, float scale) noexcept
        : current(slot), previous(slot) { current = scale; }
    ~ScopedCanvasTextScale() { current = previous; }
};

size_t NextUtf8_(std::string_view text, size_t pos) noexcept {
    if (pos >= text.size()) return text.size();
    ++pos;
    while (pos < text.size() &&
           (static_cast<unsigned char>(text[pos]) & 0xc0u) == 0x80u) ++pos;
    return pos;
}

size_t PrevUtf8_(std::string_view text, size_t pos) noexcept {
    pos = std::min(pos, text.size());
    if (pos == 0) return 0;
    --pos;
    while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xc0u) == 0x80u) --pos;
    return pos;
}

size_t Utf8Count_(std::string_view text) noexcept {
    size_t count = 0;
    for (size_t at = 0; at < text.size(); at = NextUtf8_(text, at)) ++count;
    return count;
}

bool InsertUtf8_(std::string& text, size_t& cursor, std::string_view input,
                 size_t maxCharacters) {
    bool changed = false;
    size_t total = Utf8Count_(text);
    for (size_t at = 0; at < input.size() && total < maxCharacters;) {
        const size_t next = NextUtf8_(input, at);
        const unsigned char first = static_cast<unsigned char>(input[at]);
        if (first >= 0x20u && first != 0x7fu) {
            text.insert(cursor, input.substr(at, next - at));
            cursor += next - at;
            ++total;
            changed = true;
        }
        at = next;
    }
    return changed;
}

int TextWidth_(TTF_Font* font, std::string_view value) noexcept {
    if (!font || value.empty()) return 0;
    int width = 0;
    // TTF_SizeUTF8 needs a NUL-terminated string; the temporary owns it.
    const std::string measured(value);
    return TTF_SizeUTF8(font, measured.c_str(), &width, nullptr) == 0 ? width : 0;
}
}

bool UIManager::Init(const UIStyle& defaultStyle) noexcept
{
    mTextDefaultColor = defaultStyle.text;
    mBtnDefaultColor = defaultStyle.btn;
    mBtnHotDefaultColor = defaultStyle.btnHot;
    mBtnActiveDefaultColor = defaultStyle.btnActive;
    mBtnOutlineDefaultColor = defaultStyle.outline;
    mBtnDefaultPadding = defaultStyle.padding;

    mDefaultFont = AssetManager::GetInstancePtr()->GetFontByKey(defaultStyle.defaultFontKey);
    if (!mDefaultFont && !defaultStyle.defaultFontKey.empty()) {
        LogError("UIManager warning",
            std::string("Init(): Could not find font asset '") + defaultStyle.defaultFontKey +
            "'. Labels/Buttons without explicit font will not show text.");
    }

    gHotID.clear();
    gActiveID.reset();
    gMouseDown = gMousePressed = gMouseReleased = false;
    gMousePos = { 0,0 };
    mCmds.clear();
    mFocusableCurrent.clear();
    mFocusablePrevious.clear();
    mFocusedId.clear();
    StopEditing_();
    mNavigationEnabled = true;
    mFocusColor = {105, 205, 255, 255};
    mNavigationPlayer = 0;
    mLastNavigationDirection = 0;
    mSliderNavigationStep = 0;
    mNextNavigationTick = 0;
    mNavigationInitialDelayMs = 300;
    mNavigationRepeatDelayMs = 110;

    return true;
}

void UIManager::Shutdown() noexcept
{
    StopEditing_();
    mFocusableCurrent.clear();
    mFocusablePrevious.clear();
    mFocusedId.clear();
    mSliderNavigationStep = 0;
    mCmds.clear();
    mDefaultFont = nullptr;

    gHotID.clear();
    gActiveID.reset();
    gMouseDown = gMousePressed = gMouseReleased = false;
    gMousePos = { 0,0 };
}

void UIManager::Begin() noexcept
{
    gHotID.clear();
    mTextScale = 1.f;
    mFocusablePrevious.swap(mFocusableCurrent);
    mFocusableCurrent.clear();
    mMouseHitSelectable = false;
    mSubmitPressed = false;
    mCancelPressed = false;
    mSliderNavigationStep = 0;

    if (auto* input = InputManager::GetInstancePtr()) {
        gMousePos = input->GetMousePos();
        if (auto* window = WindowManager::GetInstancePtr()) {
            const Vec2I logical = window->GetSize();
            const Vec2I drawable = window->GetDrawableSize();
            if (logical.x > 0 && logical.y > 0 &&
                drawable.x > 0 && drawable.y > 0) {
                gMousePos.x = static_cast<int>(std::lround(
                    static_cast<double>(gMousePos.x) * drawable.x / logical.x));
                gMousePos.y = static_cast<int>(std::lround(
                    static_cast<double>(gMousePos.y) * drawable.y / logical.y));
            }
        }
        gMouseDown = input->MouseDown(MouseButton::Left);
        gMousePressed = input->MousePressed(MouseButton::Left);
        gMouseReleased = input->MouseReleased(MouseButton::Left);

        if (!mFocusedId.empty()) {
            const bool stillPresent = std::any_of(mFocusablePrevious.begin(),
                mFocusablePrevious.end(), [this](const Focusable& f) {
                    return f.id == mFocusedId;
                });
            if (!stillPresent) ClearFocus();
        }
        if (mFocusedId.empty() && !mFocusablePrevious.empty())
            mFocusedId = mFocusablePrevious.front().id;

        if (mNavigationEnabled) {
            const bool uiActions = input->IsActionMapEnabled("UI", mNavigationPlayer);
            const InputAction* submit = uiActions ? input->FindAction("UI/Submit") : nullptr;
            const InputAction* cancel = uiActions ? input->FindAction("UI/Cancel") : nullptr;
            mSubmitPressed = (submit && submit->WasPressedThisFrame(mNavigationPlayer)) ||
                (mNavigationPlayer == 0 && input->KeyPressed(Key::KEY_RETURN));
            mCancelPressed = (cancel && cancel->WasPressedThisFrame(mNavigationPlayer)) ||
                (mNavigationPlayer == 0 && input->KeyPressed(Key::ESCAPE));

            const bool tab = mNavigationPlayer == 0 && input->KeyPressed(Key::TAB);
            if (tab) {
                StopEditing_();
                const int step = (SDL_GetModState() & KMOD_SHIFT) ? -1 : 1;
                MoveFocus_(step, false);
                mSubmitPressed = false;
            } else if (!mEditingId.empty()) {
                if (mSubmitPressed || mCancelPressed) {
                    StopEditing_();
                    mSubmitPressed = false;
                }
            } else {
                Vec2 direction{};
                if (uiActions) {
                    if (const InputAction* navigate = input->FindAction("UI/Navigate"))
                        direction = navigate->ReadVec2(mNavigationPlayer);
                }
                if (mNavigationPlayer == 0 &&
                    std::abs(direction.x) < 0.5f && std::abs(direction.y) < 0.5f) {
                    direction.x = float(input->KeyDown(Key::RIGHT)) -
                                  float(input->KeyDown(Key::LEFT));
                    direction.y = float(input->KeyDown(Key::DOWN)) -
                                  float(input->KeyDown(Key::UP));
                }
                int step = 0;
                if (std::abs(direction.x) >= 0.5f || std::abs(direction.y) >= 0.5f) {
                    step = std::abs(direction.x) > std::abs(direction.y)
                        ? (direction.x > 0.f ? 1 : -1)
                        : (direction.y > 0.f ? 2 : -2);
                }
                const uint32_t now = SDL_GetTicks();
                if (step != 0 && (step != mLastNavigationDirection ||
                    SDL_TICKS_PASSED(now, mNextNavigationTick))) {
                    const auto focused = std::find_if(mFocusablePrevious.begin(),
                        mFocusablePrevious.end(), [this](const Focusable& widget) {
                            return widget.id == mFocusedId;
                        });
                    if (std::abs(step) == 1 && focused != mFocusablePrevious.end() &&
                        focused->horizontalAdjust)
                        mSliderNavigationStep = step;
                    else
                        MoveFocus_(step, true);
                    mNextNavigationTick = now + (step == mLastNavigationDirection ?
                        mNavigationRepeatDelayMs : mNavigationInitialDelayMs);
                }
                mLastNavigationDirection = step;
            }
        }
    }
    else {
        gMousePos = { 0,0 };
        gMouseDown = gMousePressed = gMouseReleased = false;
        ClearFocus();
    }

    mCmds.clear();
}

// --------------------------
// Responsive canvas and anchors
// --------------------------
Rect UIManager::CanvasRect() const noexcept
{
    const auto* window = WindowManager::GetInstancePtr();
    if (!window) return {};
    const Vec2I size = window->GetDrawableSize();
    if (size.x <= 0 || size.y <= 0) return {};
    return {0.f, 0.f, static_cast<float>(size.x), static_cast<float>(size.y)};
}

float UIManager::CanvasScale() const noexcept
{
    return mCanvas.ScaleFor(CanvasRect().Size());
}

Rect UIManager::Resolve(const UIRectTransform& layout, const Rect* parent) const noexcept
{
    const Rect canvas = CanvasRect();
    return layout.Resolve(parent ? *parent : canvas, mCanvas.ScaleFor(canvas.Size()));
}

bool UIManager::Button(const char* id, const UIRectTransform& layout,
                       const std::string& text, const Rect* parent) noexcept
{
    const ScopedCanvasTextScale scale(mTextScale, CanvasScale());
    return Button(id, Resolve(layout, parent), text);
}

bool UIManager::Button(const char* id, const UIRectTransform& layout,
                       const Font& font, const std::string& text,
                       const Rect* parent) noexcept
{
    const ScopedCanvasTextScale scale(mTextScale, CanvasScale());
    return Button(id, Resolve(layout, parent), font, text);
}

bool UIManager::Button(const char* id, const UIRectTransform& layout,
                       const UIButtonColors& colors, const std::string& text,
                       const Rect* parent) noexcept
{
    const ScopedCanvasTextScale scale(mTextScale, CanvasScale());
    return Button(id, Resolve(layout, parent), colors, text);
}

bool UIManager::Button(const char* id, const UIRectTransform& layout,
                       const Font& font, const UIButtonColors& colors,
                       const std::string& text, const Rect* parent) noexcept
{
    const ScopedCanvasTextScale scale(mTextScale, CanvasScale());
    return Button(id, Resolve(layout, parent), font, colors, text);
}

bool UIManager::InvisibleButton(const char* id, const UIRectTransform& layout,
                                const Rect* parent) noexcept
{
    return InvisibleButton(id, Resolve(layout, parent));
}

bool UIManager::ImageButton(const char* id, const UIRectTransform& layout,
                            const Texture& tex, const Rect* srcPixels,
                            const UIButtonColors& tints, bool drawFrame,
                            const Rect* parent) noexcept
{
    return ImageButton(id, Resolve(layout, parent), tex, srcPixels, tints, drawFrame);
}

void UIManager::Label(const std::string& text, const UIRectTransform& layout,
                      AlignH hAlign, AlignV vAlign, float padding,
                      const Rect* parent) noexcept
{
    const ScopedCanvasTextScale scale(mTextScale, CanvasScale());
    Label(text, Resolve(layout, parent), hAlign, vAlign, padding);
}

void UIManager::Label(const std::string& text, const UIRectTransform& layout,
                      const Font& font, const Color& color,
                      AlignH hAlign, AlignV vAlign, float padding,
                      const Rect* parent) noexcept
{
    const ScopedCanvasTextScale scale(mTextScale, CanvasScale());
    Label(text, Resolve(layout, parent), font, color, hAlign, vAlign, padding);
}

void UIManager::Panel(const UIRectTransform& layout, Color fill, Color outline,
                      bool filled, const Rect* parent) noexcept
{
    Panel(Resolve(layout, parent), fill, outline, filled);
}

void UIManager::Panel(const UIRectTransform& layout, Color fill,
                      const Rect* parent) noexcept
{
    Panel(Resolve(layout, parent), fill);
}

void UIManager::Image(const Texture& tex, const UIRectTransform& layout,
                      const Rect* srcPixels, Color tint, bool flipX, bool flipY,
                      const Rect* parent) noexcept
{
    Image(tex, Resolve(layout, parent), srcPixels, tint, flipX, flipY);
}

bool UIManager::Checkbox(const char* id, const UIRectTransform& layout,
                         bool* value, const std::string& label,
                         const Rect* parent) noexcept
{
    const ScopedCanvasTextScale scale(mTextScale, CanvasScale());
    return Checkbox(id, Resolve(layout, parent), value, label);
}

bool UIManager::Slider(const char* id, const UIRectTransform& layout,
                       float* value, float minValue, float maxValue, float step,
                       const Rect* parent) noexcept
{
    const ScopedCanvasTextScale scale(mTextScale, CanvasScale());
    return Slider(id, Resolve(layout, parent), value, minValue, maxValue, step);
}

bool UIManager::Toggle(const char* id, const UIRectTransform& layout,
                       bool* value, const std::string& label,
                       const Rect* parent) noexcept
{
    const ScopedCanvasTextScale scale(mTextScale, CanvasScale());
    return Toggle(id, Resolve(layout, parent), value, label);
}

bool UIManager::TextInput(const char* id, const UIRectTransform& layout,
                          std::string* value, const std::string& placeholder,
                          size_t maxCharacters, const Rect* parent) noexcept
{
    const ScopedCanvasTextScale scale(mTextScale, CanvasScale());
    return TextInput(id, Resolve(layout, parent), value, placeholder, maxCharacters);
}

void UIManager::ProgressBar(float value01, const UIRectTransform& layout,
                            Color back, Color fill, Color outline,
                            const Rect* parent) noexcept
{
    ProgressBar(value01, Resolve(layout, parent), back, fill, outline);
}

// --------------------------
// Helpers
// --------------------------
bool UIManager::PointInRect_(const Vec2I& p, const Rect& r) noexcept
{
    return (p.x >= r.x && p.x <= r.x + r.w &&
        p.y >= r.y && p.y <= r.y + r.h);
}

bool UIManager::Focus(const char* id) noexcept
{
    if (!id || !*id) return false;
    if (mFocusedId != id) StopEditing_();
    mFocusedId = id;
    return true;
}

void UIManager::SetNavigationRepeat(uint32_t initialMs, uint32_t repeatMs) noexcept
{
    mNavigationInitialDelayMs = std::max(1u, initialMs);
    mNavigationRepeatDelayMs = std::max(1u, repeatMs);
}

void UIManager::ClearFocus() noexcept
{
    StopEditing_();
    mFocusedId.clear();
}

bool UIManager::IsFocused(const char* id) const noexcept
{
    return id && !mFocusedId.empty() && mFocusedId == id;
}

void UIManager::RegisterFocusable_(const char* id, const Rect& r, bool horizontalAdjust) noexcept
{
    if (!id || !*id) return;
    const auto existing = std::find_if(mFocusableCurrent.begin(), mFocusableCurrent.end(),
        [id](const Focusable& item) { return item.id == id; });
    if (existing == mFocusableCurrent.end()) mFocusableCurrent.push_back({id, r, horizontalAdjust});
    else existing->horizontalAdjust = existing->horizontalAdjust || horizontalAdjust;
    if (mFocusedId.empty()) mFocusedId = id;
}

void UIManager::MoveFocus_(int direction, bool spatial) noexcept
{
    if (mFocusablePrevious.empty() || direction == 0) return;
    auto current = std::find_if(mFocusablePrevious.begin(), mFocusablePrevious.end(),
        [this](const Focusable& item) { return item.id == mFocusedId; });
    if (current == mFocusablePrevious.end()) {
        Focus(mFocusablePrevious.front().id.c_str());
        return;
    }

    if (!spatial) {
        const auto count = static_cast<int>(mFocusablePrevious.size());
        const auto index = static_cast<int>(std::distance(mFocusablePrevious.begin(), current));
        Focus(mFocusablePrevious[(index + (direction > 0 ? 1 : -1) + count) % count].id.c_str());
        return;
    }

    const float cx = current->bounds.x + current->bounds.w * 0.5f;
    const float cy = current->bounds.y + current->bounds.h * 0.5f;
    float bestScore = 1e30f;
    const Focusable* best = nullptr;
    for (const Focusable& candidate : mFocusablePrevious) {
        if (candidate.id == current->id) continue;
        const float dx = candidate.bounds.x + candidate.bounds.w * 0.5f - cx;
        const float dy = candidate.bounds.y + candidate.bounds.h * 0.5f - cy;
        const float forward = direction == 1 ? dx : direction == -1 ? -dx :
                              direction == 2 ? dy : -dy;
        if (forward <= 0.5f) continue;
        const float side = std::abs(direction == 1 || direction == -1 ? dy : dx);
        const float score = forward + side * 2.5f;
        if (score < bestScore) {
            bestScore = score;
            best = &candidate;
        }
    }
    if (best) Focus(best->id.c_str());
}

void UIManager::StartEditing_(const char* id) noexcept
{
    if (!id || !*id) return;
    Focus(id);
    if (!SDL_IsTextInputActive()) {
        SDL_StartTextInput();
        mTextInputStarted = true;
    }
    mEditingId = id;
}

void UIManager::StopEditing_() noexcept
{
    if (mTextInputStarted) SDL_StopTextInput();
    mTextInputStarted = false;
    mEditingId.clear();
    mTextCursorByte = 0;
}

UIManager::ButtonState UIManager::ButtonBehavior_(const char* id, const Rect& r) noexcept
{
    ButtonState st{};
    if (!id) return st;

    RegisterFocusable_(id, r);

    st.hovered = PointInRect_(gMousePos, r);
    if (st.hovered) gHotID = id;

    if (st.hovered && gMousePressed) {
        mMouseHitSelectable = true;
        Focus(id);
        gActiveID = id;
    }

    const bool active = gActiveID && *gActiveID == id;
    st.held = (active && gMouseDown);

    // Click = release while active and hovered
    if (gMouseReleased && active && st.hovered) {
        st.pressed = true;
        gActiveID.reset();
    }

    // Release fuera: suelta igualmente
    if (gMouseReleased && active && !st.hovered) {
        gActiveID.reset();
    }

    if (mNavigationEnabled && mSubmitPressed && mEditingId.empty() && IsFocused(id))
        st.pressed = true;

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
    c.textScale = mTextScale;
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
    cmd.textScale = mTextScale;
    cmd.filled = false;
    cmd.hAlign = h;
    cmd.vAlign = v;
    cmd.padding = padding * mTextScale;

    mCmds.push_back(std::move(cmd));
}

// --------------------------
// BUTTONS (reimplemented without undefined behavior from null Font)
// --------------------------
bool UIManager::ButtonImpl_(const char* id, const Rect& r, Font* font, const UIButtonColors& colors, const std::string& text) noexcept
{
    if (!id || !RenderManager::GetInstancePtr()) return false;

    auto st = ButtonBehavior_(id, r);

    Color fill = colors.normal;
    if (st.held)        fill = colors.active;
    else if (st.hovered) fill = colors.hover;

    PushPanel_(r, fill, IsFocused(id) ? mFocusColor : colors.outline,
               /*filled*/true);

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
    // fast panel without an outline (or with a transparent outline)
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

    if (drawFrame || IsFocused(id)) {
        // Frame (an optional subtle panel can be drawn behind it)
        PushPanel_(r, Color{ 0,0,0,0 },
                   IsFocused(id) ? mFocusColor : tints.outline,
                   /*filled*/false);
    }

    PushImage_(tex, r, srcPixels, tint, /*flipX*/false, /*flipY*/false);

    return st.pressed;
}

bool UIManager::Button(const char* id, const Rect& r, const std::string& text, AlignH hAlign, AlignV vAlign, float textPadding) noexcept
{
    UIButtonColors colors{ mBtnDefaultColor, mBtnHotDefaultColor, mBtnActiveDefaultColor, mBtnOutlineDefaultColor };

    const std::string safeText = (mDefaultFont ? text : std::string());

    if (!mDefaultFont)
        return ButtonImpl_(id, r, nullptr, colors, safeText);
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

    if (!mDefaultFont)
        return ButtonImpl_(id, r, nullptr, colors, safeText);
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

    const ButtonState state = ButtonBehavior_(id, r);

    Color fill = colors.normal;
    if (state.held) fill = colors.active;
    else if (state.hovered) fill = colors.hover;

    PushPanel_(r, fill, IsFocused(id) ? mFocusColor : colors.outline);

    // Aligned text
    Font* use = const_cast<Font*>(&font);
    if (!use) use = mDefaultFont;
    const std::string label = !text.empty() ? text : std::string(id ? id : "");

    if (use && !label.empty())
    {
        const float pad = (textPadding >= 0.f) ? textPadding : mBtnDefaultPadding;
        PushTextInRect_(r, use, label, mTextDefaultColor, hAlign, vAlign, pad);
    }

    return state.pressed;
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

    // r = entire clickable area (including text). Box = left-side square of size r.h
    const float boxSize = r.h;
    Rect box{ r.x, r.y, boxSize, boxSize };

    // Interaction: click anywhere within r
    const bool toggled = InvisibleButton(id, r);
    if (toggled) *value = !*value;

    // Dibujo caja
    Panel(box, Color{ 20,20,20,255 },
          IsFocused(id) ? mFocusColor : mBtnOutlineDefaultColor,
          /*filled*/true);

    if (*value) {
        const float pad = std::max(2.f, mBtnDefaultPadding * 0.35f);
        Rect inner{ box.x + pad, box.y + pad, box.w - pad * 2.f, box.h - pad * 2.f };
        Panel(inner, Color{ 180,180,180,255 });
    }

    // Text on the right
    if (!label.empty()) {
        const float tx = r.x + boxSize + mBtnDefaultPadding;
        const float ty = r.y + (r.h * 0.5f); // lo centramos usando TextCentered en un rect
        Rect tr{ tx, r.y, r.w - (boxSize + mBtnDefaultPadding), r.h };
        Label(label, tr, AlignH::Center, AlignV::Middle);
    }

    return toggled;
}

bool UIManager::Slider(const char* id, const Rect& r, float* value,
                       float minValue, float maxValue, float step) noexcept
{
    if (!id || !*id || !value || r.w <= 14.f || r.h <= 8.f ||
        !std::isfinite(minValue) || !std::isfinite(maxValue) ||
        !std::isfinite(step) || maxValue <= minValue || step <= 0.f)
        return false;

    // The navigation list is rebuilt every frame. Register the slider as an
    // adjustable control before the shared button behavior registers its ID.
    RegisterFocusable_(id, r, true);
    const bool wasDragging = gActiveID && *gActiveID == id;
    const ButtonState state = ButtonBehavior_(id, r);

    const float oldValue = *value;
    float next = std::isfinite(oldValue) ? std::clamp(oldValue, minValue, maxValue)
                                         : minValue;
    const float radius = std::min(9.f, r.h * 0.24f);
    const float left = r.x + radius + 3.f;
    const float trackWidth = std::max(1.f, r.w - 2.f * (radius + 3.f));

    if (state.held || (wasDragging && gMouseReleased)) {
        const float fraction = std::clamp((static_cast<float>(gMousePos.x) - left) /
                                          trackWidth, 0.f, 1.f);
        next = minValue + (maxValue - minValue) * fraction;
        if (fraction >= 1.f) next = maxValue;
        else if (fraction <= 0.f) next = minValue;
        else next = std::clamp(minValue + std::round((next - minValue) / step) * step,
                               minValue, maxValue);
    } else if (mNavigationEnabled && IsFocused(id)) {
        const int direction = mSliderNavigationStep != 0 ? mSliderNavigationStep :
                              (mSubmitPressed && mEditingId.empty() ? 1 : 0);
        if (direction != 0)
            next = std::clamp(next + static_cast<float>(direction) * step,
                              minValue, maxValue);
    }
    *value = next;

    const float fraction = std::clamp((next - minValue) / (maxValue - minValue), 0.f, 1.f);
    PushPanel_(r, state.held ? mBtnActiveDefaultColor :
                  (state.hovered ? mBtnHotDefaultColor : mBtnDefaultColor),
               IsFocused(id) ? mFocusColor : mBtnOutlineDefaultColor);
    const Rect track{left, r.y + r.h * 0.5f - 4.f, trackWidth, 8.f};
    PushPanel_(track, {38, 48, 60, 255}, {91, 116, 139, 255});
    if (fraction > 0.f)
        PushPanel_({track.x, track.y, track.w * fraction, track.h},
                   {86, 179, 216, 255}, {86, 179, 216, 255});
    PushPanel_({left + trackWidth * fraction - radius, r.y + r.h * 0.5f - radius,
                radius * 2.f, radius * 2.f},
               {232, 241, 249, 255}, {61, 92, 113, 255});
    return !std::isfinite(oldValue) || std::fabs(next - oldValue) > 0.000001f;
}

bool UIManager::Toggle(const char* id, const Rect& r, bool* value,
                       const std::string& label) noexcept
{
    if (!id || !*id || !value || r.w <= 20.f || r.h <= 12.f) return false;
    const ButtonState state = ButtonBehavior_(id, r);
    if (state.pressed) *value = !*value;

    PushPanel_(r, state.held ? mBtnActiveDefaultColor :
                  (state.hovered ? mBtnHotDefaultColor : mBtnDefaultColor),
               IsFocused(id) ? mFocusColor : mBtnOutlineDefaultColor);
    const float margin = std::min(7.f, r.h * 0.16f);
    const float switchWidth = std::min(r.w - margin * 2.f, r.h * 1.5f);
    const Rect track{r.x + margin, r.y + margin, switchWidth, r.h - margin * 2.f};
    PushPanel_(track, *value ? Color{55, 169, 136, 255} : Color{79, 91, 107, 255},
               mBtnOutlineDefaultColor);
    const float inset = std::min(3.f, track.h * 0.12f);
    const float knobWidth = std::max(4.f, track.w * 0.42f);
    PushPanel_({*value ? track.x + track.w - knobWidth - inset : track.x + inset,
                track.y + inset, knobWidth, track.h - 2.f * inset},
               {244, 248, 250, 255}, {244, 248, 250, 255});
    const Rect textArea{track.x + track.w + margin, r.y,
                        std::max(0.f, r.w - switchWidth - 3.f * margin), r.h};
    if (textArea.w > 6.f)
        Label(label.empty() ? (*value ? "ON" : "OFF") : label, textArea,
              AlignH::Left, AlignV::Middle, 0.f);
    return state.pressed;
}

bool UIManager::TextInput(const char* id, const Rect& r, std::string* value,
                          const std::string& placeholder, size_t maxCharacters) noexcept
{
    if (!id || !*id || !value || !RenderManager::GetInstancePtr()) return false;
    const ButtonState state = ButtonBehavior_(id, r);
    const bool hitByMouse = state.hovered && gMousePressed;
    if (hitByMouse || (mNavigationEnabled && mSubmitPressed && IsFocused(id) &&
                       mEditingId.empty())) {
        StartEditing_(id);
        mTextCursorByte = value->size();
    }

    bool changed = false;
    const bool editing = mEditingId == id;
    if (editing) {
        auto* input = InputManager::GetInstancePtr();
        mTextCursorByte = std::min(mTextCursorByte, value->size());
        // The caller may replace the value between frames; restore a UTF-8 boundary.
        while (mTextCursorByte > 0 && mTextCursorByte < value->size() &&
               (static_cast<unsigned char>((*value)[mTextCursorByte]) & 0xc0u) == 0x80u)
            --mTextCursorByte;

        if (input) {
            const SDL_Keymod modifiers = SDL_GetModState();
            const bool ctrl = (modifiers & KMOD_CTRL) != 0;

            if (input->KeyPressed(Key::HOME)) mTextCursorByte = 0;
            if (input->KeyPressed(Key::END)) mTextCursorByte = value->size();
            if (input->KeyPressed(Key::LEFT))
                mTextCursorByte = PrevUtf8_(*value, mTextCursorByte);
            if (input->KeyPressed(Key::RIGHT))
                mTextCursorByte = NextUtf8_(*value, mTextCursorByte);

            if (input->KeyPressed(Key::BACKSPACE) && mTextCursorByte > 0) {
                const size_t from = PrevUtf8_(*value, mTextCursorByte);
                value->erase(from, mTextCursorByte - from);
                mTextCursorByte = from;
                changed = true;
            }
            if (input->KeyPressed(Key::DEL) && mTextCursorByte < value->size()) {
                const size_t to = NextUtf8_(*value, mTextCursorByte);
                value->erase(mTextCursorByte, to - mTextCursorByte);
                changed = true;
            }

            if (ctrl && input->KeyPressed(Key::V)) {
                char* clipboard = SDL_GetClipboardText();
                if (clipboard) {
                    changed |= InsertUtf8_(*value, mTextCursorByte, clipboard, maxCharacters);
                    SDL_free(clipboard);
                }
            }
            // Do not treat Ctrl+V as a literal 'v'; AltGr must still type normally.
            if (!ctrl || (modifiers & KMOD_ALT))
                changed |= InsertUtf8_(*value, mTextCursorByte,
                                       input->GetTextInput(), maxCharacters);
        }
    }

    const float drawScale = std::max(0.001f, mTextScale);
    const float pad = std::max(3.f, mBtnDefaultPadding * drawScale);
    const Color outline = IsFocused(id) ? mFocusColor : mBtnOutlineDefaultColor;
    PushPanel_(r, mBtnDefaultColor, outline, true);

    const bool empty = value->empty();
    const bool showPlaceholder = empty && !editing;
    const std::string& full = showPlaceholder ? placeholder : *value;
    size_t from = 0;
    size_t to = full.size();
    TTF_Font* ttf = mDefaultFont ? mDefaultFont->GetSDL() : nullptr;
    const float available = std::max(0.f, r.w - pad * 2.f);
    if (ttf && !showPlaceholder && !empty &&
        TextWidth_(ttf, full) * drawScale > available) {
        // Keep the caret in view without changing the caller's actual text.
        from = editing ? std::min(mTextCursorByte, full.size()) : full.size();
        to = from;
        while (from > 0) {
            const size_t previous = PrevUtf8_(full, from);
            if (TextWidth_(ttf, std::string_view(full).substr(previous, to - previous)) *
                drawScale > available) break;
            from = previous;
        }
        while (to < full.size()) {
            const size_t next = NextUtf8_(full, to);
            if (TextWidth_(ttf, std::string_view(full).substr(from, next - from)) *
                drawScale > available) break;
            to = next;
        }
    }
    if (mDefaultFont && !full.empty()) {
        const Color textColor = showPlaceholder ? Color{160, 160, 160, 255} : mTextDefaultColor;
        PushTextInRect_(r, mDefaultFont, full.substr(from, to - from), textColor,
                        AlignH::Left, AlignV::Middle, pad / drawScale);
    }
    if (editing) {
        const size_t caret = std::clamp(mTextCursorByte, from, to);
        const float offset = ttf ? TextWidth_(ttf,
            std::string_view(full).substr(from, caret - from)) * drawScale : 0.f;
        const float x = std::clamp(r.x + pad + offset, r.x + pad,
                                   std::max(r.x + pad, r.x + r.w - pad - 2.f));
        const float h = std::max(2.f, r.h - pad * 2.f);
        Panel(Rect{x, r.y + pad, 2.f, h}, Color{235, 235, 235, 255});
    }
    return changed;
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

    if (gMousePressed && !mMouseHitSelectable) ClearFocus();

    for (const auto& c : mCmds) {
        switch (c.type) {
        case CmdType::Rect:
            render->DrawRectScreen(c.r, c.color, c.filled);
            break;

        case CmdType::Text:
            if (c.font) render->DrawTextScreen(*c.font, c.text, c.r.x, c.r.y,
                                               c.color, c.textScale);
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
                    const float textWidth = static_cast<float>(tw) * c.textScale;
                    const float textHeight = static_cast<float>(th) * c.textScale;

                    float x = c.r.x;
                    float y = c.r.y;

                    // Horizontal
                    switch (c.hAlign) {
                    case AlignH::Left:   x = c.r.x + c.padding; break;
                    case AlignH::Center: x = c.r.x + (c.r.w - textWidth) * 0.5f; break;
                    case AlignH::Right:  x = c.r.x + c.r.w - textWidth - c.padding; break;
                    }

                    // Vertical
                    switch (c.vAlign) {
                    case AlignV::Top:    y = c.r.y + c.padding; break;
                    case AlignV::Middle: y = c.r.y + (c.r.h - textHeight) * 0.5f; break;
                    case AlignV::Bottom: y = c.r.y + c.r.h - textHeight - c.padding; break;
                    }

                    render->DrawTextScreen(*c.font, c.text, x, y,
                                           c.color, c.textScale);
                }
            }
            break;
        }
    }

    mCmds.clear();

    if (gMouseReleased && gActiveID && !gMouseDown)
        gActiveID.reset();
}

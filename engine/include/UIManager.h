#pragma once
/** @file UIManager.h
 * @brief Immediate-mode buttons, controls and screen-space drawing.
 */

#include <vector>
#include <string>

#include "Singleton.h"
#include "BaseTypes.h"
#include "Assets.h"
#include "Engine.h"

/** @brief Available align h modes for UIManager.h.
 */
enum class AlignH {
    Left,   ///< Align content against the left edge.
    Center, ///< Center content horizontally.
    Right   ///< Align content against the right edge.
};
/** @brief Available align v modes for UIManager.h.
 */
enum class AlignV {
    Top,    ///< Align content against the top edge.
    Middle, ///< Center content vertically.
    Bottom  ///< Align content against the bottom edge.
};

/** @brief Colors used for the normal, hover, active and outlined button states.
 * @ingroup xengine_render
 */
struct UIButtonColors {
    /** @brief Button color when not hovered or pressed.
     */
    Color normal;
    /** @brief Button color while the pointer is over it.
     */
    Color hover;
    /** @brief Button color while the button is pressed or active.
     */
    Color active;
    /** @brief Color of the button border.
     */
    Color outline;
};

/** @brief Immediate-mode widgets and draw commands in window screen coordinates.
 *
 * Call widgets every frame from the UI phase; their rectangles use the same
 * coordinates as InputManager's mouse position. Buttons activate on *release*
 * inside the rectangle after a press began inside it. UI returns true only on
 * that activation frame, and queued drawings are flushed at the end of UI.
 *
 * @warning Widget IDs are compared by `const char*` pointer identity, not by
 *          string contents. Use stable string literals or another pointer
 *          with stable identity across frames; transient `c_str()` addresses
 *          can break press/release tracking.
 * @ingroup xengine_render
 */
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
        std::string defaultFontKey = "ui_default"; // <-- Not const; this may be changed
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
    /** @brief Queues a styled button and reports a completed left-mouse click.
     * @return True on release inside the button if the same widget received the press.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param text Text to render or serialize.
     */
    bool Button(const char* id, const Rect& r, const std::string& text = "") noexcept; // default font + default colors
    /** @brief Draws and evaluates an immediate-mode button with a stable widget ID.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param font Font used for text drawing.
     * @param text Text to render or serialize.
     */
    bool Button(const char* id, const Rect& r, const Font& font, const std::string& text = "") noexcept; // specified font + default colors
    /** @brief Draws and evaluates an immediate-mode button with a stable widget ID.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param colors UI colors used for the control states.
     * @param text Text to render or serialize.
     */
    bool Button(const char* id, const Rect& r, const UIButtonColors& colors, const std::string& text = "") noexcept; // default font + specified colors
    /** @brief Draws and evaluates an immediate-mode button with a stable widget ID.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param font Font used for text drawing.
     * @param colors UI colors used for the control states.
     * @param text Text to render or serialize.
     */
    bool Button(const char* id, const Rect& r, const Font& font, const UIButtonColors& colors, const std::string& text = "") noexcept; // specified font + specified colors

    /** @brief Draws and evaluates an immediate-mode button with a stable widget ID.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param text Text to render or serialize.
     * @param hAlign Horizontal UI alignment.
     * @param vAlign Vertical UI alignment.
     * @param textPadding Spacing around the displayed text.
     */
    bool Button(const char* id, const Rect& r, const std::string& text, AlignH hAlign, AlignV vAlign, float textPadding = -1.f) noexcept;
    /** @brief Draws and evaluates an immediate-mode button with a stable widget ID.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param font Font used for text drawing.
     * @param text Text to render or serialize.
     * @param hAlign Horizontal UI alignment.
     * @param vAlign Vertical UI alignment.
     * @param textPadding Spacing around the displayed text.
     */
    bool Button(const char* id, const Rect& r, const Font& font, const std::string& text, AlignH hAlign, AlignV vAlign, float textPadding = -1.f) noexcept;
    /** @brief Draws and evaluates an immediate-mode button with a stable widget ID.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param colors UI colors used for the control states.
     * @param text Text to render or serialize.
     * @param hAlign Horizontal UI alignment.
     * @param vAlign Vertical UI alignment.
     * @param textPadding Spacing around the displayed text.
     */
    bool Button(const char* id, const Rect& r, const UIButtonColors& colors, const std::string& text, AlignH hAlign, AlignV vAlign, float textPadding = -1.f) noexcept;
    /** @brief Draws and evaluates an immediate-mode button with a stable widget ID.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param font Font used for text drawing.
     * @param colors UI colors used for the control states.
     * @param text Text to render or serialize.
     * @param hAlign Horizontal UI alignment.
     * @param vAlign Vertical UI alignment.
     * @param textPadding Spacing around the displayed text.
     */
    bool Button(const char* id, const Rect& r, const Font& font, const UIButtonColors& colors, const std::string& text, AlignH hAlign, AlignV vAlign, float textPadding = -1.f) noexcept;


    /** @brief Evaluates a clickable rectangle without enqueuing any drawing.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @return True on a matching left-button press/release inside the rectangle.
     */
    bool InvisibleButton(const char* id, const Rect& r) noexcept; // input only; does not draw

    // ImageButton: use UIButtonColors as tints (normal/hover/active), and outline as a border
    /** @brief Queues a texture button tinted for normal/hover/pressed state.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param tex Texture to draw or cache.
     * @param srcPixels Optional source rectangle in texture pixels; null uses the full texture.
     * @param tints Optional per-item color multipliers.
     * @param drawFrame Whether to draw the outline frame (texture is always queued).
     * @return True only on a completed left-mouse click.
     */
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
    /** @brief Draws screen-space text using the selected font, color and optional alignment.
     * @param text Text to render or serialize.
     * @param x Horizontal coordinate or component.
     * @param y Vertical coordinate or component.
     */
    void Label(const std::string& text, float x, float y) noexcept;
    /** @brief Draws screen-space text using the selected font, color and optional alignment.
     * @param text Text to render or serialize.
     * @param x Horizontal coordinate or component.
     * @param y Vertical coordinate or component.
     * @param font Font used for text drawing.
     */
    void Label(const std::string& text, float x, float y, const Font& font) noexcept;
    /** @brief Draws screen-space text using the selected font, color and optional alignment.
     * @param text Text to render or serialize.
     * @param x Horizontal coordinate or component.
     * @param y Vertical coordinate or component.
     * @param color RGBA drawing color.
     */
    void Label(const std::string& text, float x, float y, const Color& color) noexcept;
    /** @brief Draws screen-space text using the selected font, color and optional alignment.
     * @param text Text to render or serialize.
     * @param x Horizontal coordinate or component.
     * @param y Vertical coordinate or component.
     * @param font Font used for text drawing.
     * @param color RGBA drawing color.
     */
    void Label(const std::string& text, float x, float y, const Font& font, const Color& color) noexcept;

    /** @brief Draws screen-space text using the selected font, color and optional alignment.
     * @param text Text to render or serialize.
     * @param rect Rectangle in the coordinate space specified by the method.
     * @param hAlign Horizontal UI alignment.
     * @param vAlign Vertical UI alignment.
     * @param padding Extra spacing inside the control or rectangle.
     */
    void Label(const std::string& text, Rect rect, AlignH hAlign, AlignV vAlign, float padding = -1.f) noexcept;
    /** @brief Draws screen-space text using the selected font, color and optional alignment.
     * @param text Text to render or serialize.
     * @param rect Rectangle in the coordinate space specified by the method.
     * @param font Font used for text drawing.
     * @param hAlign Horizontal UI alignment.
     * @param vAlign Vertical UI alignment.
     * @param padding Extra spacing inside the control or rectangle.
     */
    void Label(const std::string& text, Rect rect, const Font& font, AlignH hAlign, AlignV vAlign, float padding = -1.f) noexcept;
    /** @brief Draws screen-space text using the selected font, color and optional alignment.
     * @param text Text to render or serialize.
     * @param rect Rectangle in the coordinate space specified by the method.
     * @param color RGBA drawing color.
     * @param hAlign Horizontal UI alignment.
     * @param vAlign Vertical UI alignment.
     * @param padding Extra spacing inside the control or rectangle.
     */
    void Label(const std::string& text, Rect rect, const Color& color, AlignH hAlign, AlignV vAlign, float padding = -1.f) noexcept;
    /** @brief Draws screen-space text using the selected font, color and optional alignment.
     * @param text Text to render or serialize.
     * @param rect Rectangle in the coordinate space specified by the method.
     * @param font Font used for text drawing.
     * @param color RGBA drawing color.
     * @param hAlign Horizontal UI alignment.
     * @param vAlign Vertical UI alignment.
     * @param padding Extra spacing inside the control or rectangle.
     */
    void Label(const std::string& text, Rect rect, const Font& font, const Color& color, AlignH hAlign, AlignV vAlign, float padding = -1.f) noexcept;

    // --------------------------
    // Panels
    // --------------------------
    /** @brief Draws a UI panel rectangle with optional outline and fill.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param fill Fill color or fill-mode setting.
     * @param outline Border or outline color.
     * @param filled Whether the rectangle is filled.
     */
    void Panel(const Rect& r, Color fill, Color outline, bool filled = true) noexcept;
    /** @brief Draws a UI panel rectangle with optional outline and fill.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param fill Fill color or fill-mode setting.
     */
    void Panel(const Rect& r, Color fill) noexcept; // no outline (fast)

    // --------------------------
    // Images/Textures UI
    // --------------------------
    /** @brief Draws an image in screen-space UI coordinates.
     * @param tex Texture to draw or cache.
     * @param dstScreen Destination rectangle in screen pixels.
     * @param srcPixels Optional source rectangle in texture pixels; null uses the full texture.
     * @param tint RGBA tint multiplier used for drawing.
     * @param flipX Whether to mirror the image horizontally.
     * @param flipY Whether to mirror the image vertically.
     */
    void Image(const Texture& tex,
        const Rect& dstScreen,
        const Rect* srcPixels = nullptr,
        Color tint = { 255,255,255,255 },
        bool flipX = false,
        bool flipY = false) noexcept;

    // --------------------------
    // Widget construction helpers
    // --------------------------

    /** @brief Queues a checkbox and toggles a caller-owned bool on release inside its bounds.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param value Non-owning pointer to the boolean changed on activation; null is ignored.
     * @param label Text shown to the user.
     * @return True if the checkbox was toggled this frame.
     */
    bool Checkbox(const char* id, const Rect& r, bool* value, const std::string& label = "") noexcept;

    /** @brief Draws a progress indicator with a normalized value from 0 to 1.
     * @param value01 Normalized value in the interval from 0 to 1.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param back Background RGBA color of the bar.
     * @param fill Fill color or fill-mode setting.
     * @param outline Border or outline color.
     */
    void ProgressBar(float value01,
        const Rect& r,
        Color back = { 40,40,40,255 },
        Color fill = { 120,200,120,255 },
        Color outline = { 200,200,200,255 }) noexcept;
};

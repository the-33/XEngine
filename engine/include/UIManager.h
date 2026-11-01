#pragma once
/** @file UIManager.h
 * @brief Immediate-mode buttons, controls and screen-space drawing.
 */

#include <vector>
#include <string>
#include <unordered_map>

#include "Singleton.h"
#include "BaseTypes.h"
#include "Assets.h"
#include "UIRectTransform.h"
#include "InputActions.h"
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

/** @brief Policy used when moving focus from an interactive UI control. */
enum class UINavigationMode {
    Automatic, ///< Choose the nearest control in the requested direction.
    Explicit,  ///< Follow the direction-specific IDs supplied by the caller.
    None       ///< Ignore directional navigation from this control.
};

/** @brief Optional directional neighbours for a focusable UI control. */
struct UINavigation {
    UINavigationMode mode = UINavigationMode::Automatic; ///< Directional focus policy.
    std::string up;    ///< ID to focus on Up; empty means no transition.
    std::string down;  ///< ID to focus on Down; empty means no transition.
    std::string left;  ///< ID to focus on Left; empty means no transition.
    std::string right; ///< ID to focus on Right; empty means no transition.
};

/** @brief Units used to interpret UIStyle::cornerRadius. */
enum class UICornerRadiusUnit {
    Pixels,  ///< Drawable UI pixels, scaled by UICanvas for anchored widgets.
    Percent  ///< Percentage of the shortest side of the resolved widget.
};

/** @brief Immediate-mode widgets and draw commands in drawable screen pixels.
 *
 * Call widgets every frame from the UI phase; their rectangles use the SDL
 * renderer's drawable pixel coordinates. Logical mouse positions are scaled
 * to these coordinates on HiDPI displays. Buttons activate on *release*
 * inside the rectangle after a press began inside it. UI returns true only on
 * that activation frame, and queued drawings are flushed at the end of UI.
 *
 * Widget IDs are compared by their text content and copied while held, so
 * a temporary `std::string::c_str()` is safe for press/release tracking as
 * long as the identifier text remains consistent between frames.
 * @ingroup xengine_render
 */
class UIManager : public Singleton<UIManager>
{
    friend class Singleton<UIManager>;
    friend class Engine;

private:
public:
    /** @brief Reusable appearance for built-in UI controls.
     * Values are applied by SetStyle or temporarily by PushStyle/PopStyle.
     * Corner radius can be in pixels or a percentage of the shortest side;
     * border thickness and padding are in drawable UI pixels. Anchored
     * controls scale pixel measurements with the active canvas.
     */
    struct UIStyle {
        Color text{240, 240, 240, 255};              ///< Foreground text color.
        Color btn{60, 60, 60, 255};                 ///< Normal control background.
        Color btnHot{80, 80, 80, 255};              ///< Hovered control background.
        Color btnActive{40, 40, 40, 255};           ///< Pressed control background.
        Color outline{200, 200, 200, 255};          ///< Unfocused border color.
        float padding = 6.f;                        ///< Internal text padding.
        std::string defaultFontKey = "ui_default";  ///< Font asset key for text.
        float cornerRadius = 0.f;                   ///< Corner radius value in cornerRadiusUnit units.
        float borderWidth = 1.f;                    ///< Thickness of panel/control borders.
        UICornerRadiusUnit cornerRadiusUnit = UICornerRadiusUnit::Pixels; ///< Unit for cornerRadius.
        Color focus{105, 205, 255, 255};           ///< Focus outline color.
        Color sliderTrack{38, 48, 60, 255};        ///< Slider track background.
        Color sliderTrackOutline{91, 116, 139, 255}; ///< Slider track border.
        Color sliderFill{86, 179, 216, 255};       ///< Slider progress color.
        Color sliderHandle{232, 241, 249, 255};    ///< Slider handle fill.
        Color sliderHandleOutline{61, 92, 113, 255}; ///< Slider handle border.
        Color toggleOn{55, 169, 136, 255};         ///< Enabled toggle track.
        Color toggleOff{79, 91, 107, 255};         ///< Disabled toggle track.
        Color dropdownBackground{22, 30, 42, 250}; ///< Expanded dropdown background.
        Color dropdownSelected{59, 105, 130, 255}; ///< Selected dropdown row.
        Color dropdownHovered{60, 72, 87, 255};    ///< Hovered dropdown row.
        Color dropdownArrow{240, 240, 240, 255};   ///< Dropdown chevron color.
        Color checkboxBackground{20, 20, 20, 255}; ///< Checkbox empty fill.
        Color checkboxCheck{180, 180, 180, 255};   ///< Checkbox checked fill.
        Color placeholder{160, 160, 160, 255};     ///< Text input placeholder color.
        Color textSelection{66, 125, 185, 255};    ///< Text input selection background.
    };

private:

    enum class CmdType { Rect, RoundedPanel, Line, Text, Image, TextInRect };

    struct DrawCmd {
        CmdType type = CmdType::Rect;

        Rect r{};
        Color color{ 255,255,255,255 };

        // Text
        std::string text;
        Font* font = nullptr;
        float textScale = 1.f;

        // Rect
        bool filled = true;
        Color outline{0, 0, 0, 0};
        float cornerRadius = 0.f;
        float borderWidth = 1.f;
        Vec2 from{};
        Vec2 to{};
        float lineWidth = 1.f;

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

    struct Focusable {
        std::string id; ///< Stable identifier of the interactive widget.
        Rect bounds{};  ///< Drawable-pixel rectangle used for directional navigation.
        bool horizontalAdjust = false; ///< Left/right adjusts this widget instead of moving focus.
    };

    std::vector<Focusable> mFocusableCurrent;
    std::vector<Focusable> mFocusablePrevious;
    std::string mFocusedId;
    std::string mPendingFocusId;
    std::string mEditingId;
    bool mAutoSelectFirst = true;
    std::unordered_map<std::string, UINavigation> mNavigationRules;
    PlayerId mNavigationPlayer = 0;
    bool mNavigationEnabled = true;
    Color mFocusColor{105, 205, 255, 255};
    bool mSubmitPressed = false;
    bool mCancelPressed = false;
    bool mTextInputStarted = false;
    bool mMouseHitSelectable = false;
    int mLastNavigationDirection = 0;
    int mSliderNavigationStep = 0;
    // Only one dropdown popup is open at a time; options and selection are
    // caller-owned, as with other immediate-mode controls.
    std::string mOpenDropdownId;
    std::vector<DrawCmd> mDropdownOverlayCmds;
    int mDropdownHighlighted = 0;
    int mDropdownFirstVisible = 0;
    int mDropdownPressedRow = -1;
    int mDropdownNavigationStep = 0;
    bool mDropdownSubmitPressed = false;
    bool mDropdownCancelledThisFrame = false;
    bool mDropdownSeenLastFrame = false;
    bool mDropdownSuppressMouse = false;
    uint32_t mNextNavigationTick = 0;
    uint32_t mNavigationInitialDelayMs = 300;
    uint32_t mNavigationRepeatDelayMs = 110;
    size_t mTextCursorByte = 0;
    size_t mTextSelectionAnchorByte = 0;
    bool mTextMouseSelecting = false;

    void RegisterFocusable_(const char* id, const Rect& r, bool horizontalAdjust = false) noexcept;
    void MoveFocus_(int direction, bool spatial) noexcept;
    void StartEditing_(const char* id) noexcept;
    void StopEditing_() noexcept;

    std::vector<DrawCmd> mCmds;

    Color mTextDefaultColor = { 240,240,240,255 };
    Color mBtnDefaultColor = { 60,60,60,255 };
    Color mBtnHotDefaultColor = { 80,80,80,255 };
    Color mBtnActiveDefaultColor = { 40,40,40,255 };
    Color mBtnOutlineDefaultColor = { 200,200,200,255 };
    float mBtnDefaultPadding = 6.f;

    UIStyle mBaseStyle{};
    UIStyle mActiveStyle{};
    std::vector<UIStyle> mStyleStack;

    Font* mDefaultFont = nullptr;
    UICanvas mCanvas{};
    float mTextScale = 1.f;

    // Helpers internos
    static bool PointInRect_(const Vec2I& p, const Rect& r) noexcept;
    ButtonState ButtonBehavior_(const char* id, const Rect& r) noexcept;

    void PushPanel_(const Rect& r, Color fill, Color outline, bool filled = true) noexcept;
    void PushText_(const std::string& text, const Rect& r, Font* font, Color color) noexcept;
    void PushImage_(const Texture& tex, const Rect& dst, const Rect* srcPixels, Color tint, bool flipX, bool flipY) noexcept;
    void PushTextInRect_(const Rect& r, Font* font, const std::string& text, Color color, AlignH h, AlignV v, float padding) noexcept;
    void PushChevron_(const Rect& r, bool upward) noexcept;
    void ApplyStyle_(const UIStyle& style) noexcept;

    bool ButtonImpl_(const char* id, const Rect& r, Font* font, const UIButtonColors& colors, const std::string& text) noexcept;

    bool Init(const UIStyle& defaultStyle) noexcept;
    void Shutdown() noexcept;

    void Begin() noexcept;
    void End() noexcept;

public:
    /** @brief Set the default appearance of subsequent controls.
     * @param style Appearance to use outside temporary style scopes.
     */
    void SetStyle(const UIStyle& style) noexcept;
    /** @brief Return the appearance currently used for drawing. */
    const UIStyle& GetStyle() const noexcept { return mActiveStyle; }
    /** @brief Temporarily override all UI appearance fields.
     * Pair each call with PopStyle, including conditional drawing paths.
     * @param style Appearance used until the matching PopStyle.
     */
    void PushStyle(const UIStyle& style) noexcept;
    /** @brief Restore the appearance preceding the most recent PushStyle. */
    void PopStyle() noexcept;

    // --------------------------
    // Keyboard/controller navigation and editable fields
    // --------------------------
    /** @brief Enable or disable automatic UI focus navigation (enabled by default).
     * Mouse interaction remains available when navigation is disabled.
     */
    void SetNavigationEnabled(bool enabled) noexcept { mNavigationEnabled = enabled; }
    /** @brief Whether automatic keyboard/controller UI navigation is enabled. */
    bool NavigationEnabled() const noexcept { return mNavigationEnabled; }
    /** @brief Select which logical player owns UI navigation and submit actions. */
    void SetNavigationPlayer(PlayerId player) noexcept { mNavigationPlayer = player; }
    /** @brief Read the player currently controlling UI navigation. */
    PlayerId NavigationPlayer() const noexcept { return mNavigationPlayer; }
    /** @brief Set the outline color for focused UI controls. */
    void SetFocusColor(Color color) noexcept;
    /** @brief Read the current UI focus outline color. */
    Color FocusColor() const noexcept { return mFocusColor; }
    /** @brief Configure delays in milliseconds before and during held-direction repeats. */
    void SetNavigationRepeat(uint32_t initialMs, uint32_t repeatMs) noexcept;
    /** @brief Control whether an empty focus automatically selects the first widget.
     * Disable it to allow a menu to remain initially unselected. Directional
     * input or Tab can still select the first eligible widget.
     * @param enabled Whether first-widget selection is automatic.
     */
    void SetAutoSelectFirst(bool enabled) noexcept;
    /** @brief Whether empty focus selects the first available widget. */
    bool AutoSelectFirst() const noexcept { return mAutoSelectFirst; }
    /** @brief Request the initial focused widget of an upcoming panel.
     * The requested ID receives focus when its widget is next drawn.
     * @param id Widget ID, or null/empty to remove the pending request.
     */
    void SetInitialFocus(const char* id) noexcept;
    /** @brief Configure directional navigation for one widget ID.
     * Rules persist across frames until cleared; missing explicit neighbours
     * do not move focus. An ID with mode None can still be clicked or focused
     * programmatically.
     * @param id Unique focusable widget ID.
     * @param navigation Directional navigation mode and optional neighbours.
     */
    void SetNavigation(const char* id, const UINavigation& navigation) noexcept;
    /** @brief Remove the navigation override for a specific widget ID.
     * @param id Widget identifier to reset to automatic navigation.
     */
    void ClearNavigation(const char* id) noexcept;
    /** @brief Remove every explicit UI navigation override. */
    void ClearAllNavigation() noexcept;
    /** @brief Focus a widget by stable ID; it must be drawn on the current UI frame. */
    bool Focus(const char* id) noexcept;
    /** @brief Clear widget focus and end text editing if active.
     * If automatic first-selection is enabled, focus returns to the first
     * eligible widget on the next frame.
     */
    void ClearFocus() noexcept;
    /** @brief Stable ID of the focused widget, or an empty string. */
    const std::string& FocusedId() const noexcept { return mFocusedId; }
    /** @brief Whether the widget identified by ID has UI focus. */
    bool IsFocused(const char* id) const noexcept;
    /** @brief Whether a UI text field currently consumes text input. */
    bool IsEditingText() const noexcept { return !mEditingId.empty(); }

    /** @brief Whether Escape/UI Cancel was used to dismiss an open dropdown
     * during the current input frame. Parent menus should not close too.
     * @return True only on the frame when the dropdown consumed Cancel.
     */
    bool DropdownConsumedCancel() const noexcept { return mDropdownCancelledThisFrame; }

    /** @brief One-choice dropdown with mouse, keyboard and gamepad navigation.
     * The closed field opens on click/UI Submit; while open, up/down chooses
     * an item, Submit commits and Cancel dismisses without changing the value.
     * Mouse wheel scrolls long lists. The popup is overlaid above later widgets
     * and constrained to the drawable canvas. The caller owns the option list.
     * @param id Stable identifier unique to this UI panel.
     * @param r Closed field rectangle in drawable-pixel coordinates.
     * @param selectedIndex Pointer to caller-owned selected option index.
     * @param options Display labels; must remain available throughout this call.
     * @param maxVisibleItems Maximum number of popup rows, clamped to 1..12.
     * @return True only when the selected index changes.
     */
    bool Dropdown(const char* id, const Rect& r, int* selectedIndex,
                  const std::vector<std::string>& options, int maxVisibleItems = 6) noexcept;

    /** @brief Anchored dropdown with the same interaction and ownership rules.
     * @param id Stable identifier unique to this UI panel.
     * @param layout Responsive rectangle of the closed field.
     * @param selectedIndex Pointer to caller-owned selected option index.
     * @param options Display labels for each available choice.
     * @param maxVisibleItems Maximum number of visible popup rows.
     * @param parent Optional parent rectangle for anchored layout.
     * @return True only when selection changes.
     */
    bool Dropdown(const char* id, const UIRectTransform& layout, int* selectedIndex,
                  const std::vector<std::string>& options, int maxVisibleItems = 6,
                  const Rect* parent = nullptr) noexcept;

    /** @brief Editable single-line UTF-8 field, using caller-owned text.
     * Mouse click or UI/Submit begins editing; Enter confirms and Escape cancels
     * editing without discarding existing text. Permite colocar el cursor con
     * el ratón, seleccionar arrastrando o con Mayús+flechas y usar Ctrl+A/C/X/V.
     * Las teclas de edición admiten repetición. Returns true if the text changes.
     * @param id Unique stable widget identifier.
     * @param r Field rectangle in drawable screen pixels.
     * @param value Non-null string owned by the game.
     * @param placeholder Display text shown while the field is empty.
     * @param maxCharacters Maximum UTF-8 code points accepted.
     */
    bool TextInput(const char* id, const Rect& r, std::string* value,
                   const std::string& placeholder = "", size_t maxCharacters = 64) noexcept;
    /** @brief Responsive anchored version of TextInput. */
    bool TextInput(const char* id, const UIRectTransform& layout,
                   std::string* value, const std::string& placeholder = "",
                   size_t maxCharacters = 64, const Rect* parent = nullptr) noexcept;

    // --------------------------
    // Responsive canvas and anchored UI
    // --------------------------
    /** @brief Sets the scale policy used by anchored UI rectangles.
     * Existing Rect-based widgets continue to use raw drawable pixels.
     */
    void SetCanvas(const UICanvas& canvas) noexcept { mCanvas = canvas; }
    /** @brief Returns the current scale policy for anchored UI. */
    const UICanvas& GetCanvas() const noexcept { return mCanvas; }
    /** @brief Returns the current full-screen canvas in drawable pixels. */
    Rect CanvasRect() const noexcept;
    /** @brief Returns the current canvas scale (1 for constant pixel size). */
    float CanvasScale() const noexcept;
    /** @brief Resolves an anchored rectangle relative to the canvas or a parent.
     * Recomputed from the current drawable size on each call; changes to
     * resolution, fullscreen mode and aspect ratio require no manual updates.
     * @param layout Normalized anchors, pivot, offset and size delta.
     * @param parent Optional already-resolved parent rectangle; null means canvas.
     */
    Rect Resolve(const UIRectTransform& layout, const Rect* parent = nullptr) const noexcept;

    /** @brief Anchored button using the same hit testing and drawing as Button(Rect). */
    bool Button(const char* id, const UIRectTransform& layout,
                const std::string& text = "", const Rect* parent = nullptr) noexcept;
    /** @brief Anchored button with an explicit font. */
    bool Button(const char* id, const UIRectTransform& layout, const Font& font,
                const std::string& text = "", const Rect* parent = nullptr) noexcept;
    /** @brief Anchored button with explicit colors. */
    bool Button(const char* id, const UIRectTransform& layout, const UIButtonColors& colors,
                const std::string& text = "", const Rect* parent = nullptr) noexcept;
    /** @brief Anchored button with explicit font and colors. */
    bool Button(const char* id, const UIRectTransform& layout, const Font& font,
                const UIButtonColors& colors, const std::string& text = "",
                const Rect* parent = nullptr) noexcept;
    /** @brief Invisible anchored button. */
    bool InvisibleButton(const char* id, const UIRectTransform& layout,
                         const Rect* parent = nullptr) noexcept;
    /** @brief Textured anchored button. */
    bool ImageButton(const char* id, const UIRectTransform& layout, const Texture& tex,
                     const Rect* srcPixels = nullptr,
                     const UIButtonColors& tints = UIButtonColors{
                         {255,255,255,255}, {230,230,230,255},
                         {200,200,200,255}, {200,200,200,0}},
                     bool drawFrame = true, const Rect* parent = nullptr) noexcept;
    /** @brief Draws text inside an anchored rectangle. */
    void Label(const std::string& text, const UIRectTransform& layout,
               AlignH hAlign = AlignH::Center, AlignV vAlign = AlignV::Middle,
               float padding = -1.f, const Rect* parent = nullptr) noexcept;
    /** @brief Draws styled text inside an anchored rectangle. */
    void Label(const std::string& text, const UIRectTransform& layout,
               const Font& font, const Color& color,
               AlignH hAlign = AlignH::Center, AlignV vAlign = AlignV::Middle,
               float padding = -1.f, const Rect* parent = nullptr) noexcept;
    /** @brief Draws a colored panel from an anchored rectangle. */
    void Panel(const UIRectTransform& layout, Color fill, Color outline,
               bool filled = true, const Rect* parent = nullptr) noexcept;
    /** @brief Draws an outline-free anchored panel. */
    void Panel(const UIRectTransform& layout, Color fill,
               const Rect* parent = nullptr) noexcept;
    /** @brief Draws a texture inside an anchored rectangle. */
    void Image(const Texture& tex, const UIRectTransform& layout,
               const Rect* srcPixels = nullptr, Color tint = {255,255,255,255},
               bool flipX = false, bool flipY = false,
               const Rect* parent = nullptr) noexcept;
    /** @brief Draws an anchored checkbox and updates a caller-owned value. */
    bool Checkbox(const char* id, const UIRectTransform& layout, bool* value,
                  const std::string& label = "", const Rect* parent = nullptr) noexcept;
    /** @brief Draws a native slider using a responsive anchored rectangle.
     * The game owns the value. A mouse drag, horizontal navigation or Submit
     * changes it; vertical navigation and Tab continue to move focus.
     * @param id Unique stable widget identifier.
     * @param layout Responsive anchors and size for the whole slider.
     * @param value Non-null pointer to the float value owned by the caller.
     * @param minValue Inclusive finite minimum (less than maxValue).
     * @param maxValue Inclusive finite maximum (greater than minValue).
     * @param step Positive finite increment for navigation and mouse snapping.
     * @param parent Optional resolved parent rectangle; null uses the canvas.
     * @return True only if the caller-owned value changed this frame.
     */
    bool Slider(const char* id, const UIRectTransform& layout, float* value,
                float minValue = 0.f, float maxValue = 1.f, float step = 0.05f,
                const Rect* parent = nullptr) noexcept;
    /** @brief Draws a native ON/OFF switch using responsive anchors.
     * Supports mouse clicks and keyboard/controller UI Submit activation.
     * @param id Unique stable widget identifier.
     * @param layout Responsive anchors and size for the whole switch.
     * @param value Non-null pointer to the boolean owned by the caller.
     * @param label Optional text displayed beside the switch, or ON/OFF when empty.
     * @param parent Optional resolved parent rectangle; null uses the canvas.
     * @return True only if the caller-owned boolean changed this frame.
     */
    bool Toggle(const char* id, const UIRectTransform& layout, bool* value,
                const std::string& label = "", const Rect* parent = nullptr) noexcept;
    /** @brief Draws an anchored progress bar. */
    void ProgressBar(float value01, const UIRectTransform& layout,
                     Color back = {40,40,40,255}, Color fill = {120,200,120,255},
                     Color outline = {200,200,200,255},
                     const Rect* parent = nullptr) noexcept;

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

    /** @brief Draws an interactive, game-state-owned slider.
     * Mouse press/drag changes the value (including when dragged outside the bounds).
     * When focused, UI/Navigate left/right or keyboard arrows change it by step;
     * UI/Submit (Enter or controller South) increases it by step. Up/down and Tab
     * continue to navigate between widgets. The pointer is never stored.
     * @param id Unique stable ID that must be reused each frame.
     * @param r Entire hit rectangle in drawable screen pixels.
     * @param value Non-null pointer to the current value, owned by the caller.
     * @param minValue Inclusive finite minimum (must be below maxValue).
     * @param maxValue Inclusive finite maximum.
     * @param step Positive finite increment, also used for mouse snap intervals.
     * @return True only when the value changes this frame.
     */
    bool Slider(const char* id, const Rect& r, float* value,
                float minValue = 0.f, float maxValue = 1.f, float step = 0.05f) noexcept;

    /** @brief Draws a native ON/OFF switch with mouse, keyboard and gamepad support.
     * The switch toggles on click/release or UI/Submit, can receive focus and
     * optionally displays a custom label in place of its ON/OFF state text.
     * @param id Unique stable ID that must be reused each frame.
     * @param r Entire hit rectangle in drawable screen pixels.
     * @param value Non-null pointer to a caller-owned boolean.
     * @param label Optional custom text displayed next to the switch.
     * @return True only when the boolean changes this frame.
     */
    bool Toggle(const char* id, const Rect& r, bool* value,
                const std::string& label = "") noexcept;

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

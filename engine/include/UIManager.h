#pragma once
/** @file UIManager.h
 * @brief Immediate-mode buttons, controls and screen-space drawing.
 */

#include <vector>
#include <string>
#include <unordered_map>
#include <utility>

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
    /** @brief Grant Singleton< UIManager > access to the internal state of UIManager.
     */
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
    /** @brief Permit the singleton base to construct and release the UIManager manager.
     */
    friend class Singleton<UIManager>;
    /** @brief Grant Engine access to the internal state of UIManager.
     */
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
        bool sliderShowBackground = true;           ///< Draw the rectangular slider container.
        bool sliderRoundElements = false;           ///< Round the slider track and thumb into capsules/circles.
        float sliderTrackThickness = 8.f;           ///< Track height in drawable UI pixels.
        float sliderHandleDiameter = 18.f;          ///< Thumb diameter in drawable UI pixels.
        Color toggleOn{55, 169, 136, 255};         ///< Enabled toggle track.
        Color toggleOff{79, 91, 107, 255};         ///< Disabled toggle track.
        bool toggleShowBackground = true;           ///< Draw the rectangular toggle container.
        bool toggleRoundElements = false;           ///< Round the switch track and thumb.
        bool toggleTrackOnlyHit = false;            ///< Restrict pointer interaction to the switch itself.
        bool toggleShowStateText = true;            ///< Draw ON/OFF text when the label is empty.
        Color dropdownBackground{22, 30, 42, 250}; ///< Expanded dropdown background.
        Color dropdownSelected{59, 105, 130, 255}; ///< Selected dropdown row.
        Color dropdownHovered{60, 72, 87, 255};    ///< Hovered dropdown row.
        Color dropdownArrow{240, 240, 240, 255};   ///< Dropdown chevron color.
        Color checkboxBackground{20, 20, 20, 255}; ///< Checkbox empty fill.
        Color checkboxCheck{180, 180, 180, 255};   ///< Checkbox checked fill.
        Color placeholder{160, 160, 160, 255};     ///< Text input placeholder color.
        Color textSelection{66, 125, 185, 255};    ///< Text input selection background.
        Color progressBack{40, 40, 40, 255};       ///< Progress bar background.
        Color progressFill{120, 200, 120, 255};    ///< Progress bar filled portion.
        Color progressOutline{200, 200, 200, 255}; ///< Progress bar outline.
    };

private:

    /** @brief Available cmd type values used by UIManager.
     */
    enum class CmdType {
        Rect,         ///< Filled or outlined rectangular primitive.
        RoundedPanel, ///< Panel with rounded corners and an optional border.
        Line,         ///< Screen-space line with configurable thickness.
        Text,         ///< Text positioned at a screen-space origin.
        Image,        ///< Textured rectangle.
        TextInRect    ///< Text aligned within a rectangular region.
    };

    /** @brief Draw cmd data and utilities used by UIManager.
     */
    struct DrawCmd {
        /** @brief Configured type value used by DrawCmd.
         */
        CmdType type = CmdType::Rect;
        int layer = 0; ///< Higher layers render above lower layers.

        /** @brief Configured r value used by DrawCmd.
         */
        Rect r{};
        /** @brief Configured color value used by DrawCmd.
         */
        Color color{ 255,255,255,255 };

        // Text
        /** @brief Configured text value used by DrawCmd.
         */
        std::string text;
        /** @brief Configured font value used by DrawCmd.
         */
        Font* font = nullptr;
        /** @brief Configured text scale value used by DrawCmd.
         */
        float textScale = 1.f;

        // Rect
        /** @brief Configured filled value used by DrawCmd.
         */
        bool filled = true;
        /** @brief Configured outline value used by DrawCmd.
         */
        Color outline{0, 0, 0, 0};
        /** @brief Configured corner radius value used by DrawCmd.
         */
        float cornerRadius = 0.f;
        /** @brief Configured border width value used by DrawCmd.
         */
        float borderWidth = 1.f;
        /** @brief Configured from value used by DrawCmd.
         */
        Vec2 from{};
        /** @brief Configured to value used by DrawCmd.
         */
        Vec2 to{};
        /** @brief Configured line width value used by DrawCmd.
         */
        float lineWidth = 1.f;

        // Image
        /** @brief Configured tex value used by DrawCmd.
         */
        const Texture* tex = nullptr;
        /** @brief Configured src value used by DrawCmd.
         */
        Rect src{};
        /** @brief Configured has src value used by DrawCmd.
         */
        bool hasSrc = false;
        /** @brief Configured flip x value used by DrawCmd.
         */
        bool flipX = false;
        /** @brief Configured flip y value used by DrawCmd.
         */
        bool flipY = false;

        //Text in rect
        /** @brief Configured h align value used by DrawCmd.
         */
        AlignH hAlign = AlignH::Left;
        /** @brief Configured v align value used by DrawCmd.
         */
        AlignV vAlign = AlignV::Top;
        /** @brief Configured padding value used by DrawCmd.
         */
        float padding = 0.f;
    };

    /** @brief Runtime state of an input device or interactive control.
     */
    struct ButtonState {
        /** @brief Configured hovered value used by ButtonState.
         */
        bool hovered = false;
        /** @brief Configured held value used by ButtonState.
         */
        bool held = false;     // active + mouse down
        /** @brief Configured pressed value used by ButtonState.
         */
        bool pressed = false;  // click (release on hovered while active)
    };

    /** @brief Bounds and interaction metadata of an interactive UI element.
     */
    struct Focusable {
        std::string id; ///< Stable identifier of the interactive widget.
        Rect bounds{};  ///< Drawable-pixel rectangle used for directional navigation.
        bool horizontalAdjust = false; ///< Left/right adjusts this widget instead of moving focus.
        /** @brief Configured layer value used by Focusable.
         */
        int layer = 0;
    };

    /** @brief Bounds and interaction metadata of an interactive UI element.
     */
    struct HitRegion {
        /** @brief Configured id value used by HitRegion.
         */
        std::string id;
        /** @brief Configured bounds value used by HitRegion.
         */
        Rect bounds{};
        /** @brief Configured layer value used by HitRegion.
         */
        int layer = 0;
        bool overlay = false; ///< Popup commands flush last within their layer.
    };

    /** @brief Stored focusable current value used by UIManager.
     */
    std::vector<Focusable> mFocusableCurrent;
    /** @brief Stored focusable previous value used by UIManager.
     */
    std::vector<Focusable> mFocusablePrevious;
    /** @brief Stored hit regions current value used by UIManager.
     */
    std::vector<HitRegion> mHitRegionsCurrent;
    /** @brief Stored hit regions previous value used by UIManager.
     */
    std::vector<HitRegion> mHitRegionsPrevious;
    /** @brief Stored top pointer id value used by UIManager.
     */
    std::string mTopPointerId;
    /** @brief Stored top pointer layer value used by UIManager.
     */
    int mTopPointerLayer = 0;
    /** @brief Stored focused id value used by UIManager.
     */
    std::string mFocusedId;
    /** @brief Stored pending focus id value used by UIManager.
     */
    std::string mPendingFocusId;
    /** @brief Stored editing id value used by UIManager.
     */
    std::string mEditingId;
    /** @brief Stored auto select first value used by UIManager.
     */
    bool mAutoSelectFirst = true;
    /** @brief Stored navigation rules value used by UIManager.
     */
    std::unordered_map<std::string, UINavigation> mNavigationRules;
    /** @brief Stored navigation player value used by UIManager.
     */
    PlayerId mNavigationPlayer = 0;
    /** @brief Stored navigation enabled value used by UIManager.
     */
    bool mNavigationEnabled = true;
    /** @brief Stored focus color value used by UIManager.
     */
    Color mFocusColor{105, 205, 255, 255};
    /** @brief Stored submit pressed value used by UIManager.
     */
    bool mSubmitPressed = false;
    /** @brief Stored cancel pressed value used by UIManager.
     */
    bool mCancelPressed = false;
    /** @brief Stored text input started value used by UIManager.
     */
    bool mTextInputStarted = false;
    /** @brief Stored mouse hit selectable value used by UIManager.
     */
    bool mMouseHitSelectable = false;
    /** @brief Stored last navigation direction value used by UIManager.
     */
    int mLastNavigationDirection = 0;
    /** @brief Stored slider navigation step value used by UIManager.
     */
    int mSliderNavigationStep = 0;
    // Only one dropdown popup is open at a time; options and selection are
    // caller-owned, as with other immediate-mode controls.
    /** @brief Stored open dropdown id value used by UIManager.
     */
    std::string mOpenDropdownId;
    /** @brief Stored open dropdown layer value used by UIManager.
     */
    int mOpenDropdownLayer = 0;
    /** @brief Stored dropdown overlay cmds value used by UIManager.
     */
    std::vector<DrawCmd> mDropdownOverlayCmds;
    /** @brief Stored dropdown highlighted value used by UIManager.
     */
    int mDropdownHighlighted = 0;
    /** @brief Stored dropdown first visible value used by UIManager.
     */
    int mDropdownFirstVisible = 0;
    /** @brief Stored dropdown pressed row value used by UIManager.
     */
    int mDropdownPressedRow = -1;
    /** @brief Stored dropdown navigation step value used by UIManager.
     */
    int mDropdownNavigationStep = 0;
    /** @brief Stored dropdown submit pressed value used by UIManager.
     */
    bool mDropdownSubmitPressed = false;
    /** @brief Stored dropdown cancelled this frame value used by UIManager.
     */
    bool mDropdownCancelledThisFrame = false;
    /** @brief Stored dropdown seen last frame value used by UIManager.
     */
    bool mDropdownSeenLastFrame = false;
    /** @brief Stored dropdown suppress mouse value used by UIManager.
     */
    bool mDropdownSuppressMouse = false;
    /** @brief Stored next navigation tick value used by UIManager.
     */
    uint32_t mNextNavigationTick = 0;
    /** @brief Stored navigation initial delay ms value used by UIManager.
     */
    uint32_t mNavigationInitialDelayMs = 300;
    /** @brief Stored navigation repeat delay ms value used by UIManager.
     */
    uint32_t mNavigationRepeatDelayMs = 110;
    /** @brief Stored text cursor byte value used by UIManager.
     */
    size_t mTextCursorByte = 0;
    /** @brief Stored text selection anchor byte value used by UIManager.
     */
    size_t mTextSelectionAnchorByte = 0;
    /** @brief Stored text mouse selecting value used by UIManager.
     */
    bool mTextMouseSelecting = false;

    /** @brief Register focusable in UIManager.
     * @param id Stable identifier associated with the requested object or control.
     * @param r Target rectangle in the relevant coordinate space.
     * @param horizontalAdjust Horizontal adjust supplied to UIManager.
     */
    void RegisterFocusable_(const char* id, const Rect& r, bool horizontalAdjust = false) noexcept;
    /** @brief Move focus in UIManager.
     * @param direction Direction supplied to UIManager.
     * @param spatial Spatial supplied to UIManager.
     */
    void MoveFocus_(int direction, bool spatial) noexcept;
    /** @brief Start editing in UIManager.
     * @param id Stable identifier associated with the requested object or control.
     */
    void StartEditing_(const char* id) noexcept;
    /** @brief Stop editing in UIManager.
     */
    void StopEditing_() noexcept;

    /** @brief Stored cmds value used by UIManager.
     */
    std::vector<DrawCmd> mCmds;
    /** @brief Stored layer value used by UIManager.
     */
    int mLayer = 0;
    /** @brief Stored layer stack value used by UIManager.
     */
    std::vector<int> mLayerStack;

    /** @brief Stored text default color value used by UIManager.
     */
    Color mTextDefaultColor = { 240,240,240,255 };
    /** @brief Stored btn default color value used by UIManager.
     */
    Color mBtnDefaultColor = { 60,60,60,255 };
    /** @brief Stored btn hot default color value used by UIManager.
     */
    Color mBtnHotDefaultColor = { 80,80,80,255 };
    /** @brief Stored btn active default color value used by UIManager.
     */
    Color mBtnActiveDefaultColor = { 40,40,40,255 };
    /** @brief Stored btn outline default color value used by UIManager.
     */
    Color mBtnOutlineDefaultColor = { 200,200,200,255 };
    /** @brief Stored btn default padding value used by UIManager.
     */
    float mBtnDefaultPadding = 6.f;

    /** @brief Stored base style value used by UIManager.
     */
    UIStyle mBaseStyle{};
    /** @brief Stored active style value used by UIManager.
     */
    UIStyle mActiveStyle{};
    /** @brief Stored style stack value used by UIManager.
     */
    std::vector<UIStyle> mStyleStack;

    /** @brief Stored default font value used by UIManager.
     */
    Font* mDefaultFont = nullptr;
    /** @brief Stored canvas value used by UIManager.
     */
    UICanvas mCanvas{};
    /** @brief Stored text scale value used by UIManager.
     */
    float mTextScale = 1.f;

    // Helpers internos
    /** @brief Point in rect in UIManager.
     * @param p P supplied to UIManager.
     * @param r Target rectangle in the relevant coordinate space.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    static bool PointInRect_(const Vec2I& p, const Rect& r) noexcept;
    /** @brief Button behavior in UIManager.
     * @param id Stable identifier associated with the requested object or control.
     * @param r Target rectangle in the relevant coordinate space.
     * @return Result of button behavior in UIManager.
     */
    ButtonState ButtonBehavior_(const char* id, const Rect& r) noexcept;
    /** @brief Queue cmd in UIManager.
     * @param command Command supplied to UIManager.
     */
    void QueueCmd_(DrawCmd command) noexcept;

    /** @brief Invoke a UI operation on a temporary layer and restore the prior layer.
     * @tparam Fn Callable type whose result is forwarded to the caller.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param fn Callable invoked by the operation.
     * @return The value returned by the invoked callback, if any.
     */
    template <typename Fn>
    auto InvokeOnLayer_(int layer, Fn&& fn) noexcept -> decltype(std::forward<Fn>(fn)())
    {
        PushLayer(layer);
        // Restore the surrounding layer even for early returns from a widget.
        struct RestoreLayer {
            UIManager& owner;
            ~RestoreLayer() { owner.PopLayer(); }
        } restore{*this};
        return std::forward<Fn>(fn)();
    }

    /** @brief Push panel in UIManager.
     * @param r Target rectangle in the relevant coordinate space.
     * @param fill Fill supplied to UIManager.
     * @param outline Outline supplied to UIManager.
     * @param filled Filled supplied to UIManager.
     * @param radiusOverride Radius override supplied to UIManager.
     */
    void PushPanel_(const Rect& r, Color fill, Color outline, bool filled = true,
                    float radiusOverride = -1.f) noexcept;
    /** @brief Push text in UIManager.
     * @param text UTF-8 text supplied to the operation.
     * @param r Target rectangle in the relevant coordinate space.
     * @param font Font asset used to measure or render text.
     * @param color RGBA color used by the operation.
     */
    void PushText_(const std::string& text, const Rect& r, Font* font, Color color) noexcept;
    /** @brief Push image in UIManager.
     * @param tex Texture asset to read or draw.
     * @param dst Dst supplied to UIManager.
     * @param srcPixels Src pixels supplied to UIManager.
     * @param tint RGBA tint applied to the rendered sprite or text.
     * @param flipX Flip x supplied to UIManager.
     * @param flipY Flip y supplied to UIManager.
     */
    void PushImage_(const Texture& tex, const Rect& dst, const Rect* srcPixels, Color tint, bool flipX, bool flipY) noexcept;
    /** @brief Push text in rect in UIManager.
     * @param r Target rectangle in the relevant coordinate space.
     * @param font Font asset used to measure or render text.
     * @param text UTF-8 text supplied to the operation.
     * @param color RGBA color used by the operation.
     * @param h Height of the requested region.
     * @param v Input vector or value to process.
     * @param padding Padding supplied to UIManager.
     */
    void PushTextInRect_(const Rect& r, Font* font, const std::string& text, Color color, AlignH h, AlignV v, float padding) noexcept;
    /** @brief Push chevron in UIManager.
     * @param r Target rectangle in the relevant coordinate space.
     * @param upward Upward supplied to UIManager.
     */
    void PushChevron_(const Rect& r, bool upward) noexcept;
    /** @brief Apply style in UIManager.
     * @param style Appearance and layout options for this control.
     */
    void ApplyStyle_(const UIStyle& style) noexcept;

    /** @brief Button impl in UIManager.
     * @param id Stable identifier associated with the requested object or control.
     * @param r Target rectangle in the relevant coordinate space.
     * @param font Font asset used to measure or render text.
     * @param colors Colors supplied to UIManager.
     * @param text UTF-8 text supplied to the operation.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool ButtonImpl_(const char* id, const Rect& r, Font* font, const UIButtonColors& colors, const std::string& text) noexcept;

    /** @brief Initialize the UIManager subsystem.
     * @param defaultStyle Default style supplied to UIManager.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Init(const UIStyle& defaultStyle) noexcept;
    /** @brief Release resources managed by UIManager.
     */
    void Shutdown() noexcept;

    /** @brief Begin in UIManager.
     */
    void Begin() noexcept;
    /** @brief End in UIManager.
     */
    void End() noexcept;

public:
    /** @brief Set the numeric UI layer for subsequent elements this frame.
     * Higher values render on top and take mouse priority; at equal layers,
     * the last submitted interactive element wins overlapping hit tests.
     * A new UI frame begins at layer zero.
     * @param layer Absolute layer value (negative values are allowed).
     */
    void SetLayer(int layer) noexcept { mLayer = layer; }
    /** @brief Get the current layer for subsequent UI elements.
     * @return Requested get layer value or reference.
     */
    int GetLayer() const noexcept { return mLayer; }
    /** @brief Push a layer, retaining the previous value for PopLayer.
     * @param layer Absolute layer value to use within this scope.
     */
    void PushLayer(int layer) noexcept;
    /** @brief Restore the layer from the most recent PushLayer. */
    void PopLayer() noexcept;


    /** @brief Set the default appearance of subsequent controls.
     * @param style Appearance to use outside temporary style scopes.
     */
    void SetStyle(const UIStyle& style) noexcept;
    /** @brief Return the appearance currently used for drawing.
     * @return The appearance currently used for drawing.
     */
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
     * @param enabled Whether the corresponding feature should be active.
     */
    void SetNavigationEnabled(bool enabled) noexcept { mNavigationEnabled = enabled; }
    /** @brief Whether automatic keyboard/controller UI navigation is enabled.
     * @return True if automatic keyboard/controller UI navigation is enabled; otherwise false.
     */
    bool NavigationEnabled() const noexcept { return mNavigationEnabled; }
    /** @brief Select which logical player owns UI navigation and submit actions.
     * @param player Logical player whose input is queried or updated.
     */
    void SetNavigationPlayer(PlayerId player) noexcept { mNavigationPlayer = player; }
    /** @brief Read the player currently controlling UI navigation.
     * @return Result of navigation player in UIManager.
     */
    PlayerId NavigationPlayer() const noexcept { return mNavigationPlayer; }
    /** @brief Set the outline color for focused UI controls.
     * @param color RGBA color used by the operation.
     */
    void SetFocusColor(Color color) noexcept;
    /** @brief Read the current UI focus outline color.
     * @return Result of focus color in UIManager.
     */
    Color FocusColor() const noexcept { return mFocusColor; }
    /** @brief Configure delays in milliseconds before and during held-direction repeats.
     * @param initialMs Delay before the first repeated input event, in milliseconds.
     * @param repeatMs Delay between subsequent repeated input events, in milliseconds.
     */
    void SetNavigationRepeat(uint32_t initialMs, uint32_t repeatMs) noexcept;
    /** @brief Control whether an empty focus automatically selects the first widget.
     * Disable it to allow a menu to remain initially unselected. Directional
     * input or Tab can still select the first eligible widget.
     * @param enabled Whether first-widget selection is automatic.
     */
    void SetAutoSelectFirst(bool enabled) noexcept;
    /** @brief Whether empty focus selects the first available widget.
     * @return True if empty focus selects the first available widget; otherwise false.
     */
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
    /** @brief Focus a widget by stable ID; it must be drawn on the current UI frame.
     * @param id Stable identifier associated with the requested object or control.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Focus(const char* id) noexcept;
    /** @brief Clear widget focus and end text editing if active.
     * If automatic first-selection is enabled, focus returns to the first
     * eligible widget on the next frame.
     */
    void ClearFocus() noexcept;
    /** @brief Stable ID of the focused widget, or an empty string.
     * @return Result of focused id in UIManager.
     */
    const std::string& FocusedId() const noexcept { return mFocusedId; }
    /** @brief Whether the widget identified by ID has UI focus.
     * @param id Stable identifier associated with the requested object or control.
     * @return True if the widget identified by ID has UI focus; otherwise false.
     */
    bool IsFocused(const char* id) const noexcept;
    /** @brief Whether a UI text field currently consumes text input.
     * @return True if a UI text field currently consumes text input; otherwise false.
     */
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
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool TextInput(const char* id, const Rect& r, std::string* value,
                   const std::string& placeholder = "", size_t maxCharacters = 64) noexcept;
    /** @brief Responsive anchored version of TextInput.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param value Value to store, serialize, compare or transform.
     * @param placeholder Placeholder supplied to UIManager.
     * @param maxCharacters Max characters supplied to UIManager.
     * @param parent Optional parent region or object against which this item is resolved.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool TextInput(const char* id, const UIRectTransform& layout,
                   std::string* value, const std::string& placeholder = "",
                   size_t maxCharacters = 64, const Rect* parent = nullptr) noexcept;

    // --------------------------
    // Responsive canvas and anchored UI
    // --------------------------
    /** @brief Sets the scale policy used by anchored UI rectangles.
     * Existing Rect-based widgets continue to use raw drawable pixels.
     * @param canvas Drawable viewport against which the element is validated or resolved.
     */
    void SetCanvas(const UICanvas& canvas) noexcept { mCanvas = canvas; }
    /** @brief Returns the current scale policy for anchored UI.
     * @return The current scale policy for anchored ui.
     */
    const UICanvas& GetCanvas() const noexcept { return mCanvas; }
    /** @brief Returns the current full-screen canvas in drawable pixels.
     * @return The current full-screen canvas in drawable pixels.
     */
    Rect CanvasRect() const noexcept;
    /** @brief Returns the current canvas scale (1 for constant pixel size).
     * @return The current canvas scale (1 for constant pixel size).
     */
    float CanvasScale() const noexcept;
    /** @brief Resolves an anchored rectangle relative to the canvas or a parent.
     * Recomputed from the current drawable size on each call; changes to
     * resolution, fullscreen mode and aspect ratio require no manual updates.
     * @param layout Normalized anchors, pivot, offset and size delta.
     * @param parent Optional already-resolved parent rectangle; null means canvas.
     * @return Requested resolve value or reference.
     */
    Rect Resolve(const UIRectTransform& layout, const Rect* parent = nullptr) const noexcept;

    /** @brief Anchored button using the same hit testing and drawing as Button(Rect).
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param text UTF-8 text supplied to the operation.
     * @param parent Optional parent region or object against which this item is resolved.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Button(const char* id, const UIRectTransform& layout,
                const std::string& text = "", const Rect* parent = nullptr) noexcept;
    /** @brief Anchored button with an explicit font.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param font Font asset used to measure or render text.
     * @param text UTF-8 text supplied to the operation.
     * @param parent Optional parent region or object against which this item is resolved.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Button(const char* id, const UIRectTransform& layout, const Font& font,
                const std::string& text = "", const Rect* parent = nullptr) noexcept;
    /** @brief Anchored button with explicit colors.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param colors Colors supplied to UIManager.
     * @param text UTF-8 text supplied to the operation.
     * @param parent Optional parent region or object against which this item is resolved.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Button(const char* id, const UIRectTransform& layout, const UIButtonColors& colors,
                const std::string& text = "", const Rect* parent = nullptr) noexcept;
    /** @brief Anchored button with explicit font and colors.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param font Font asset used to measure or render text.
     * @param colors Colors supplied to UIManager.
     * @param text UTF-8 text supplied to the operation.
     * @param parent Optional parent region or object against which this item is resolved.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Button(const char* id, const UIRectTransform& layout, const Font& font,
                const UIButtonColors& colors, const std::string& text = "",
                const Rect* parent = nullptr) noexcept;
    /** @brief Invisible anchored button.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param parent Optional parent region or object against which this item is resolved.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool InvisibleButton(const char* id, const UIRectTransform& layout,
                         const Rect* parent = nullptr) noexcept;
    /** @brief Textured anchored button.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param tex Texture asset to read or draw.
     * @param srcPixels Src pixels supplied to UIManager.
     * @param tints Tints supplied to UIManager.
     * @param drawFrame Draw frame supplied to UIManager.
     * @param parent Optional parent region or object against which this item is resolved.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool ImageButton(const char* id, const UIRectTransform& layout, const Texture& tex,
                     const Rect* srcPixels = nullptr,
                     const UIButtonColors& tints = UIButtonColors{
                         {255,255,255,255}, {230,230,230,255},
                         {200,200,200,255}, {200,200,200,0}},
                     bool drawFrame = true, const Rect* parent = nullptr) noexcept;
    /** @brief Draws text inside an anchored rectangle.
     * @param text UTF-8 text supplied to the operation.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param hAlign H align supplied to UIManager.
     * @param vAlign V align supplied to UIManager.
     * @param padding Padding supplied to UIManager.
     * @param parent Optional parent region or object against which this item is resolved.
     */
    void Label(const std::string& text, const UIRectTransform& layout,
               AlignH hAlign = AlignH::Center, AlignV vAlign = AlignV::Middle,
               float padding = -1.f, const Rect* parent = nullptr) noexcept;
    /** @brief Draws styled text inside an anchored rectangle.
     * @param text UTF-8 text supplied to the operation.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param font Font asset used to measure or render text.
     * @param color RGBA color used by the operation.
     * @param hAlign H align supplied to UIManager.
     * @param vAlign V align supplied to UIManager.
     * @param padding Padding supplied to UIManager.
     * @param parent Optional parent region or object against which this item is resolved.
     */
    void Label(const std::string& text, const UIRectTransform& layout,
               const Font& font, const Color& color,
               AlignH hAlign = AlignH::Center, AlignV vAlign = AlignV::Middle,
               float padding = -1.f, const Rect* parent = nullptr) noexcept;
    /** @brief Draws a colored panel from an anchored rectangle.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param fill Fill supplied to UIManager.
     * @param outline Outline supplied to UIManager.
     * @param filled Filled supplied to UIManager.
     * @param parent Optional parent region or object against which this item is resolved.
     */
    void Panel(const UIRectTransform& layout, Color fill, Color outline,
               bool filled = true, const Rect* parent = nullptr) noexcept;
    /** @brief Draws an outline-free anchored panel.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param fill Fill supplied to UIManager.
     * @param parent Optional parent region or object against which this item is resolved.
     */
    void Panel(const UIRectTransform& layout, Color fill,
               const Rect* parent = nullptr) noexcept;
    /** @brief Draws a canvas-scaled line along an anchored rectangle's long axis.
     * A wide rectangle produces a horizontal line through its center; a tall
     * rectangle produces a vertical line. The line thickness, measured in
     * reference-canvas pixels, scales with `UICanvas` and is rasterized to
     * physical pixels by the renderer.
     * @param layout Rectangle defining the line's endpoints and orientation.
     * @param color RGBA line color.
     * @param thickness Positive line width in reference-canvas pixels.
     * @param parent Optional already-resolved parent rectangle.
     */
    void Line(const UIRectTransform& layout, Color color, float thickness = 1.f,
              const Rect* parent = nullptr) noexcept;
    /** @brief Draws a texture inside an anchored rectangle.
     * @param tex Texture asset to read or draw.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param srcPixels Src pixels supplied to UIManager.
     * @param tint RGBA tint applied to the rendered sprite or text.
     * @param flipX Flip x supplied to UIManager.
     * @param flipY Flip y supplied to UIManager.
     * @param parent Optional parent region or object against which this item is resolved.
     */
    void Image(const Texture& tex, const UIRectTransform& layout,
               const Rect* srcPixels = nullptr, Color tint = {255,255,255,255},
               bool flipX = false, bool flipY = false,
               const Rect* parent = nullptr) noexcept;
    /** @brief Draws an anchored checkbox and updates a caller-owned value.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param value Value to store, serialize, compare or transform.
     * @param label Label supplied to UIManager.
     * @param parent Optional parent region or object against which this item is resolved.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
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
    /** @brief Draws an anchored progress bar.
     * @param value01 Normalized value between zero and one.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param back Back supplied to UIManager.
     * @param fill Fill supplied to UIManager.
     * @param outline Outline supplied to UIManager.
     * @param parent Optional parent region or object against which this item is resolved.
     */
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
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Button(const char* id, const Rect& r, const Font& font, const std::string& text = "") noexcept; // specified font + default colors
    /** @brief Draws and evaluates an immediate-mode button with a stable widget ID.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param colors UI colors used for the control states.
     * @param text Text to render or serialize.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Button(const char* id, const Rect& r, const UIButtonColors& colors, const std::string& text = "") noexcept; // default font + specified colors
    /** @brief Draws and evaluates an immediate-mode button with a stable widget ID.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param font Font used for text drawing.
     * @param colors UI colors used for the control states.
     * @param text Text to render or serialize.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Button(const char* id, const Rect& r, const Font& font, const UIButtonColors& colors, const std::string& text = "") noexcept; // specified font + specified colors

    /** @brief Draws and evaluates an immediate-mode button with a stable widget ID.
     * @param id Stable identifier chosen by the caller.
     * @param r Rectangle in the coordinate space specified by the method.
     * @param text Text to render or serialize.
     * @param hAlign Horizontal UI alignment.
     * @param vAlign Vertical UI alignment.
     * @param textPadding Spacing around the displayed text.
     * @return True if the operation succeeded or its condition holds; otherwise false.
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
     * @return True if the operation succeeded or its condition holds; otherwise false.
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
     * @return True if the operation succeeded or its condition holds; otherwise false.
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
     * @return True if the operation succeeded or its condition holds; otherwise false.
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
    /** @brief Queues a screen-space UI line in the same draw order as widgets.
     * Unlike `Renderer->DrawLineScreen`, this does not bypass UIManager's
     * immediate-mode command queue.
     * @param start Starting point in drawable UI pixels.
     * @param end Ending point in drawable UI pixels.
     * @param color RGBA line color.
     * @param thickness Positive line width in drawable pixels.
     */
    void Line(Vec2 start, Vec2 end, Color color, float thickness = 1.f) noexcept;

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
    // One-call style overrides use an internal scope: they never change the
    // appearance of subsequent controls or the global default style.

    /** @brief Draw a button using a style only for this call.
     * @param id Stable widget identifier.
     * @param r Rectangle in drawable screen pixels.
     * @param style Temporary appearance for the button.
     * @param text Visible button caption; empty uses the identifier.
     * @return True when the button is activated.
     */
    bool Button(const char* id, const Rect& r, const UIStyle& style,
                const std::string& text = "") noexcept;

    /** @brief Draw an anchored button with a temporary individual style.
     * @param id Stable widget identifier.
     * @param layout Anchors, offsets, pivot and size.
     * @param style Appearance of this button only.
     * @param text Visible caption; empty uses the identifier.
     * @param parent Optional resolved parent rectangle.
     * @return True when the button is activated.
     */
    bool Button(const char* id, const UIRectTransform& layout, const UIStyle& style,
                const std::string& text = "", const Rect* parent = nullptr) noexcept;

    /** @brief Draw an aligned button with an individual style.
     * @param id Stable widget identifier.
     * @param r Screen-space rectangle.
     * @param style Temporary appearance for this button.
     * @param text Button caption.
     * @param hAlign Horizontal text alignment.
     * @param vAlign Vertical text alignment.
     * @param textPadding Text inset, or negative for the style default.
     * @return True when activated.
     */
    bool Button(const char* id, const Rect& r, const UIStyle& style,
                const std::string& text, AlignH hAlign, AlignV vAlign,
                float textPadding = -1.f) noexcept;

    /** @brief Draw a textured button with its own frame style.
     * @param id Stable widget identifier.
     * @param r Screen-space rectangle.
     * @param tex Texture to display.
     * @param style Appearance of the optional focus/frame outline.
     * @param srcPixels Optional source rectangle in texture pixels.
     * @param tints Normal, hover, pressed and outline tints.
     * @param drawFrame Whether to draw the outline frame.
     * @return True when activated.
     */
    bool ImageButton(const char* id, const Rect& r, const Texture& tex,
                     const UIStyle& style, const Rect* srcPixels = nullptr,
                     const UIButtonColors& tints = UIButtonColors{
                         {255,255,255,255}, {230,230,230,255},
                         {200,200,200,255}, {200,200,200,0}},
                     bool drawFrame = true) noexcept;

    /** @brief Draw an anchored textured button with an individual frame style.
     * @param id Stable widget identifier.
     * @param layout Anchored rectangle.
     * @param tex Texture to display.
     * @param style Appearance of the optional focus/frame outline.
     * @param srcPixels Optional source rectangle in texture pixels.
     * @param tints Normal, hover, pressed and outline tints.
     * @param drawFrame Whether to draw the outline frame.
     * @param parent Optional resolved parent rectangle.
     * @return True when activated.
     */
    bool ImageButton(const char* id, const UIRectTransform& layout,
                     const Texture& tex, const UIStyle& style,
                     const Rect* srcPixels = nullptr,
                     const UIButtonColors& tints = UIButtonColors{
                         {255,255,255,255}, {230,230,230,255},
                         {200,200,200,255}, {200,200,200,0}},
                     bool drawFrame = true, const Rect* parent = nullptr) noexcept;

    /** @brief Draw text with the font, color and padding of one UI style.
     * @param text Text to display.
     * @param r Screen-space rectangle.
     * @param style Individual appearance for this label.
     * @param hAlign Horizontal text alignment.
     * @param vAlign Vertical text alignment.
     * @param padding Padding, or negative for the style default.
     */
    void Label(const std::string& text, const Rect& r, const UIStyle& style,
               AlignH hAlign = AlignH::Center, AlignV vAlign = AlignV::Middle,
               float padding = -1.f) noexcept;

    /** @brief Draw anchored text with a temporary individual style.
     * @param text Text to display.
     * @param layout Anchored rectangle.
     * @param style Individual appearance for this label.
     * @param hAlign Horizontal text alignment.
     * @param vAlign Vertical text alignment.
     * @param padding Padding, or negative for the style default.
     * @param parent Optional resolved parent rectangle.
     */
    void Label(const std::string& text, const UIRectTransform& layout,
               const UIStyle& style, AlignH hAlign = AlignH::Center,
               AlignV vAlign = AlignV::Middle, float padding = -1.f,
               const Rect* parent = nullptr) noexcept;

    /** @brief Draw a panel using only the colors and geometry of its style.
     * @param r Screen-space rectangle.
     * @param style Background, outline, border width and corner radius.
     * @param filled Whether the panel has an interior fill.
     */
    void Panel(const Rect& r, const UIStyle& style, bool filled = true) noexcept;

    /** @brief Draw a panel using explicit colors and an individual geometry style.
     * @param r Screen-space rectangle.
     * @param style Border width, corner radius and other UI appearance.
     * @param fill Panel fill color.
     * @param outline Panel outline color.
     * @param filled Whether the panel has an interior fill.
     */
    void Panel(const Rect& r, const UIStyle& style, Color fill, Color outline,
               bool filled = true) noexcept;

    /** @brief Draw an anchored panel with colors from its individual style.
     * @param layout Anchored rectangle.
     * @param style Background, outline, border width and corner radius.
     * @param filled Whether the panel has an interior fill.
     * @param parent Optional resolved parent rectangle.
     */
    void Panel(const UIRectTransform& layout, const UIStyle& style,
               bool filled = true, const Rect* parent = nullptr) noexcept;

    /** @brief Draw an anchored panel with explicit colors and its own style.
     * @param layout Anchored rectangle.
     * @param style Border width, corner radius and other UI appearance.
     * @param fill Panel fill color.
     * @param outline Panel outline color.
     * @param filled Whether the panel has an interior fill.
     * @param parent Optional resolved parent rectangle.
     */
    void Panel(const UIRectTransform& layout, const UIStyle& style, Color fill,
               Color outline, bool filled = true, const Rect* parent = nullptr) noexcept;

    /** @brief Draw a checkbox with an individual style.
     * @param id Stable widget identifier.
     * @param r Screen-space hit rectangle.
     * @param value Caller-owned boolean to toggle.
     * @param style Individual appearance for this checkbox.
     * @param label Optional caption.
     * @return True when its value changes.
     */
    bool Checkbox(const char* id, const Rect& r, bool* value,
                  const UIStyle& style, const std::string& label = "") noexcept;

    /** @brief Draw an anchored checkbox with an individual style.
     * @param id Stable widget identifier.
     * @param layout Anchored hit rectangle.
     * @param value Caller-owned boolean to toggle.
     * @param style Individual appearance for this checkbox.
     * @param label Optional caption.
     * @param parent Optional resolved parent rectangle.
     * @return True when its value changes.
     */
    bool Checkbox(const char* id, const UIRectTransform& layout, bool* value,
                  const UIStyle& style, const std::string& label = "",
                  const Rect* parent = nullptr) noexcept;

    /** @brief Draw a slider with an individual style.
     * @param id Stable widget identifier.
     * @param r Screen-space hit rectangle.
     * @param value Caller-owned float to edit.
     * @param style Individual slider appearance.
     * @param minValue Inclusive minimum.
     * @param maxValue Inclusive maximum.
     * @param step Positive value increment.
     * @return True when the value changes.
     */
    bool Slider(const char* id, const Rect& r, float* value, const UIStyle& style,
                float minValue = 0.f, float maxValue = 1.f,
                float step = 0.05f) noexcept;

    /** @brief Draw an anchored slider with an individual style.
     * @param id Stable widget identifier.
     * @param layout Anchored hit rectangle.
     * @param value Caller-owned float to edit.
     * @param style Individual slider appearance.
     * @param minValue Inclusive minimum.
     * @param maxValue Inclusive maximum.
     * @param step Positive value increment.
     * @param parent Optional resolved parent rectangle.
     * @return True when the value changes.
     */
    bool Slider(const char* id, const UIRectTransform& layout, float* value,
                const UIStyle& style, float minValue = 0.f,
                float maxValue = 1.f, float step = 0.05f,
                const Rect* parent = nullptr) noexcept;

    /** @brief Draw a toggle with an individual style.
     * @param id Stable widget identifier.
     * @param r Screen-space hit rectangle.
     * @param value Caller-owned boolean to toggle.
     * @param style Individual toggle appearance.
     * @param label Optional caption.
     * @return True when its value changes.
     */
    bool Toggle(const char* id, const Rect& r, bool* value,
                const UIStyle& style, const std::string& label = "") noexcept;

    /** @brief Draw an anchored toggle with an individual style.
     * @param id Stable widget identifier.
     * @param layout Anchored hit rectangle.
     * @param value Caller-owned boolean to toggle.
     * @param style Individual toggle appearance.
     * @param label Optional caption.
     * @param parent Optional resolved parent rectangle.
     * @return True when its value changes.
     */
    bool Toggle(const char* id, const UIRectTransform& layout, bool* value,
                const UIStyle& style, const std::string& label = "",
                const Rect* parent = nullptr) noexcept;

    /** @brief Draw an editable single-line text field with an individual style.
     * @param id Stable widget identifier.
     * @param r Screen-space hit rectangle.
     * @param value Caller-owned UTF-8 string.
     * @param style Individual text field appearance.
     * @param placeholder Hint text displayed when empty.
     * @param maxCharacters Maximum UTF-8 code points.
     * @return True when the string changes.
     */
    bool TextInput(const char* id, const Rect& r, std::string* value,
                   const UIStyle& style, const std::string& placeholder = "",
                   size_t maxCharacters = 64) noexcept;

    /** @brief Draw an anchored text field with an individual style.
     * @param id Stable widget identifier.
     * @param layout Anchored hit rectangle.
     * @param value Caller-owned UTF-8 string.
     * @param style Individual text field appearance.
     * @param placeholder Hint text displayed when empty.
     * @param maxCharacters Maximum UTF-8 code points.
     * @param parent Optional resolved parent rectangle.
     * @return True when the string changes.
     */
    bool TextInput(const char* id, const UIRectTransform& layout,
                   std::string* value, const UIStyle& style,
                   const std::string& placeholder = "", size_t maxCharacters = 64,
                   const Rect* parent = nullptr) noexcept;

    /** @brief Draw a dropdown with an individual style.
     * @param id Stable widget identifier.
     * @param r Screen-space hit rectangle.
     * @param selectedIndex Caller-owned selected option index.
     * @param options Display labels of the choices.
     * @param style Individual dropdown field and popup appearance.
     * @param maxVisibleItems Maximum number of visible popup rows.
     * @return True when the selection changes.
     */
    bool Dropdown(const char* id, const Rect& r, int* selectedIndex,
                  const std::vector<std::string>& options, const UIStyle& style,
                  int maxVisibleItems = 6) noexcept;

    /** @brief Draw an anchored dropdown with an individual style.
     * @param id Stable widget identifier.
     * @param layout Anchored hit rectangle.
     * @param selectedIndex Caller-owned selected option index.
     * @param options Display labels of the choices.
     * @param style Individual dropdown field and popup appearance.
     * @param maxVisibleItems Maximum number of visible popup rows.
     * @param parent Optional resolved parent rectangle.
     * @return True when the selection changes.
     */
    bool Dropdown(const char* id, const UIRectTransform& layout,
                  int* selectedIndex, const std::vector<std::string>& options,
                  const UIStyle& style, int maxVisibleItems = 6,
                  const Rect* parent = nullptr) noexcept;

    /** @brief Draw a progress bar using the colors and geometry of its style.
     * @param value01 Progress fraction between zero and one.
     * @param r Screen-space rectangle.
     * @param style Individual background, fill, outline and corner appearance.
     */
    void ProgressBar(float value01, const Rect& r, const UIStyle& style) noexcept;

    /** @brief Draw a progress bar with explicit colors and an individual style.
     * @param value01 Progress fraction between zero and one.
     * @param r Screen-space rectangle.
     * @param style Individual corner radius and border appearance.
     * @param back Background color.
     * @param fill Filled progress color.
     * @param outline Border color.
     */
    void ProgressBar(float value01, const Rect& r, const UIStyle& style,
                     Color back, Color fill, Color outline) noexcept;

    /** @brief Draw an anchored progress bar using colors from its style.
     * @param value01 Progress fraction between zero and one.
     * @param layout Anchored rectangle.
     * @param style Individual background, fill, outline and corner appearance.
     * @param parent Optional resolved parent rectangle.
     */
    void ProgressBar(float value01, const UIRectTransform& layout,
                     const UIStyle& style, const Rect* parent = nullptr) noexcept;

    /** @brief Draw an anchored progress bar with explicit colors and a style.
     * @param value01 Progress fraction between zero and one.
     * @param layout Anchored rectangle.
     * @param style Individual corner radius and border appearance.
     * @param back Background color.
     * @param fill Filled progress color.
     * @param outline Border color.
     * @param parent Optional resolved parent rectangle.
     */
    void ProgressBar(float value01, const UIRectTransform& layout,
                     const UIStyle& style, Color back, Color fill, Color outline,
                     const Rect* parent = nullptr) noexcept;

    /** @name Individual UI layers
     * Specify an absolute numeric layer immediately after an element's
     * rectangle (after both endpoints for a line). This works with all
     * existing styled, anchored and screen-space widget arguments without
     * modifying their original overloads or default values. For example:
     * @code{.cpp}
     * UI->Button("play", rect, 10, "Play");
     * UI->Slider("volume", rect, 5, &volume, 0.f, 1.f, 0.05f);
     * UI->Panel(rect, -2, Color{30, 30, 30, 255});
     * UI->Line(start, end, 10, Color{255, 255, 255, 255}, 2.f);
     * @endcode
     * The previous layer is restored after each call. Omitting the argument
     * uses the current layer set by SetLayer/PushLayer (zero by default).
     * @{ */
    template <typename... Args>
    /** @brief Draw a button on the specified numeric layer.
     * @tparam Args Additional arguments accepted by the ordinary Button overload.
     * @param id Stable identifier used to track button interactions across frames.
     * @param r Screen-space button rectangle in drawable pixels.
     * @param layer Layer used for drawing and pointer priority during this call.
     * @param args Additional button arguments, such as caption and style.
     * @return True only on the frame the button is activated.
     */
    bool Button(const char* id, const Rect& r, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return Button(id, r, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Button in UIManager.
     * @tparam Args Template type or value used by Button.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Button(const char* id, const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return Button(id, layout, std::forward<Args>(args)...); }); }

    template <typename... Args>
    /** @brief Invisible button in UIManager.
     * @tparam Args Template type or value used by InvisibleButton.
     * @param id Stable identifier associated with the requested object or control.
     * @param r Target rectangle in the relevant coordinate space.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool InvisibleButton(const char* id, const Rect& r, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return InvisibleButton(id, r, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Invisible button in UIManager.
     * @tparam Args Template type or value used by InvisibleButton.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool InvisibleButton(const char* id, const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return InvisibleButton(id, layout, std::forward<Args>(args)...); }); }

    template <typename... Args>
    /** @brief Image button in UIManager.
     * @tparam Args Template type or value used by ImageButton.
     * @param id Stable identifier associated with the requested object or control.
     * @param r Target rectangle in the relevant coordinate space.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool ImageButton(const char* id, const Rect& r, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return ImageButton(id, r, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Image button in UIManager.
     * @tparam Args Template type or value used by ImageButton.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool ImageButton(const char* id, const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return ImageButton(id, layout, std::forward<Args>(args)...); }); }

    template <typename... Args>
    /** @brief Checkbox in UIManager.
     * @tparam Args Template type or value used by Checkbox.
     * @param id Stable identifier associated with the requested object or control.
     * @param r Target rectangle in the relevant coordinate space.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Checkbox(const char* id, const Rect& r, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return Checkbox(id, r, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Checkbox in UIManager.
     * @tparam Args Template type or value used by Checkbox.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Checkbox(const char* id, const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return Checkbox(id, layout, std::forward<Args>(args)...); }); }

    template <typename... Args>
    /** @brief Slider in UIManager.
     * @tparam Args Template type or value used by Slider.
     * @param id Stable identifier associated with the requested object or control.
     * @param r Target rectangle in the relevant coordinate space.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Slider(const char* id, const Rect& r, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return Slider(id, r, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Slider in UIManager.
     * @tparam Args Template type or value used by Slider.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Slider(const char* id, const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return Slider(id, layout, std::forward<Args>(args)...); }); }

    template <typename... Args>
    /** @brief Toggle in UIManager.
     * @tparam Args Template type or value used by Toggle.
     * @param id Stable identifier associated with the requested object or control.
     * @param r Target rectangle in the relevant coordinate space.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Toggle(const char* id, const Rect& r, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return Toggle(id, r, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Toggle in UIManager.
     * @tparam Args Template type or value used by Toggle.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Toggle(const char* id, const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return Toggle(id, layout, std::forward<Args>(args)...); }); }

    template <typename... Args>
    /** @brief Dropdown in UIManager.
     * @tparam Args Template type or value used by Dropdown.
     * @param id Stable identifier associated with the requested object or control.
     * @param r Target rectangle in the relevant coordinate space.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Dropdown(const char* id, const Rect& r, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return Dropdown(id, r, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Dropdown in UIManager.
     * @tparam Args Template type or value used by Dropdown.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Dropdown(const char* id, const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return Dropdown(id, layout, std::forward<Args>(args)...); }); }

    template <typename... Args>
    /** @brief Text input in UIManager.
     * @tparam Args Template type or value used by TextInput.
     * @param id Stable identifier associated with the requested object or control.
     * @param r Target rectangle in the relevant coordinate space.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool TextInput(const char* id, const Rect& r, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return TextInput(id, r, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Text input in UIManager.
     * @tparam Args Template type or value used by TextInput.
     * @param id Stable identifier associated with the requested object or control.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool TextInput(const char* id, const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { return InvokeOnLayer_(layer, [&] { return TextInput(id, layout, std::forward<Args>(args)...); }); }

    template <typename... Args>
    /** @brief Label in UIManager.
     * @tparam Args Template type or value used by Label.
     * @param text UTF-8 text supplied to the operation.
     * @param r Target rectangle in the relevant coordinate space.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     */
    void Label(const std::string& text, Rect r, int layer, Args&&... args) noexcept
    {
        InvokeOnLayer_(layer, [&] {
            if constexpr (sizeof...(Args) == 0)
                Label(text, r, AlignH::Center, AlignV::Middle);
            else
                Label(text, r, std::forward<Args>(args)...);
        });
    }
    template <typename... Args>
    /** @brief Label in UIManager.
     * @tparam Args Template type or value used by Label.
     * @param text UTF-8 text supplied to the operation.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     */
    void Label(const std::string& text, const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { InvokeOnLayer_(layer, [&] { Label(text, layout, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Label in UIManager.
     * @tparam Args Template type or value used by Label.
     * @param text UTF-8 text supplied to the operation.
     * @param x Horizontal coordinate or component.
     * @param y Vertical coordinate or component.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     */
    void Label(const std::string& text, float x, float y, int layer, Args&&... args) noexcept
    { InvokeOnLayer_(layer, [&] { Label(text, x, y, std::forward<Args>(args)...); }); }

    template <typename... Args>
    /** @brief Panel in UIManager.
     * @tparam Args Template type or value used by Panel.
     * @param r Target rectangle in the relevant coordinate space.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     */
    void Panel(const Rect& r, int layer, Args&&... args) noexcept
    { InvokeOnLayer_(layer, [&] { Panel(r, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Panel in UIManager.
     * @tparam Args Template type or value used by Panel.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     */
    void Panel(const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { InvokeOnLayer_(layer, [&] { Panel(layout, std::forward<Args>(args)...); }); }

    template <typename... Args>
    /** @brief Line in UIManager.
     * @tparam Args Template type or value used by Line.
     * @param start Starting position or iterator of the requested range.
     * @param end Ending position or iterator of the requested range.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     */
    void Line(Vec2 start, Vec2 end, int layer, Args&&... args) noexcept
    { InvokeOnLayer_(layer, [&] { Line(start, end, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Line in UIManager.
     * @tparam Args Template type or value used by Line.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     */
    void Line(const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { InvokeOnLayer_(layer, [&] { Line(layout, std::forward<Args>(args)...); }); }

    template <typename... Args>
    /** @brief Image in UIManager.
     * @tparam Args Template type or value used by Image.
     * @param tex Texture asset to read or draw.
     * @param r Target rectangle in the relevant coordinate space.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     */
    void Image(const Texture& tex, const Rect& r, int layer, Args&&... args) noexcept
    { InvokeOnLayer_(layer, [&] { Image(tex, r, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Image in UIManager.
     * @tparam Args Template type or value used by Image.
     * @param tex Texture asset to read or draw.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     */
    void Image(const Texture& tex, const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { InvokeOnLayer_(layer, [&] { Image(tex, layout, std::forward<Args>(args)...); }); }

    template <typename... Args>
    /** @brief Progress bar in UIManager.
     * @tparam Args Template type or value used by ProgressBar.
     * @param value01 Normalized value between zero and one.
     * @param r Target rectangle in the relevant coordinate space.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     */
    void ProgressBar(float value01, const Rect& r, int layer, Args&&... args) noexcept
    { InvokeOnLayer_(layer, [&] { ProgressBar(value01, r, std::forward<Args>(args)...); }); }
    template <typename... Args>
    /** @brief Progress bar in UIManager.
     * @tparam Args Template type or value used by ProgressBar.
     * @param value01 Normalized value between zero and one.
     * @param layout Anchored UI layout definition to resolve against the active canvas.
     * @param layer Absolute drawing and interaction layer; larger values appear above smaller ones.
     * @param args Additional arguments forwarded to the corresponding overload.
     */
    void ProgressBar(float value01, const UIRectTransform& layout, int layer, Args&&... args) noexcept
    { InvokeOnLayer_(layer, [&] { ProgressBar(value01, layout, std::forward<Args>(args)...); }); }
    /** @} */
};

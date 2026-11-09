#pragma once
/** @file UIWidgets.h
 * @brief Scene-friendly persistent UI elements, fluent configuration and canvas events.
 * @details These widgets submit the existing immediate-mode UIManager commands once
 * per frame. The classic UIManager::Button/Slider/... API remains available.
 * @ingroup xengine_render
 */

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "UIManager.h"

/** @brief Deferred callback queue; dispatched once all registered widgets are submitted. */
using UIWidgetEvents = std::vector<std::function<void()>>;

class UILabel;
class UIImage;
class UIPanel;
class UIProgressBar;
class UIImageButton;
class UILine;
class UIInvisibleButton;

/** @brief Common identity, geometry, style, visibility and interaction state of retained UI widgets.
 * @details Concrete widgets use UIWidgetFluent to return their own type from
 * setters. The owning UIWidgetCanvas preserves their addresses until removal.
 * This is a rendering descriptor, not a GameObject or a new SDL resource.
 * @ingroup xengine_render
 */
class UIWidget {
public:
    /** @brief Release a polymorphic UI element. */
    virtual ~UIWidget() = default;

    /** @brief Retrieve the canvas-local identifier.
     * @return Stable identifier used for typed canvas lookups.
     */
    const std::string& Key() const noexcept { return mKey; }
    /** @brief Retrieve the fully qualified immediate-mode widget identifier.
     * @return Canvas key and element key separated by '/'.
     */
    const std::string& RuntimeId() const noexcept { return mRuntimeId; }
    /** @brief Whether this element is submitted by its canvas.
     * @return True if visible.
     */
    bool IsVisible() const noexcept { return mVisible; }
    /** @brief Return the active local style configuration.
     * @return Const reference to this element's style.
     */
    const UIManager::UIStyle& GetStyle() const noexcept { return mStyle; }
    /** @brief Return its numeric render/interaction layer.
     * @return Absolute layer value.
     */
    int GetLayer() const noexcept { return mLayer; }

    /** @brief Submit one visible widget, recording its pending interaction callbacks.
     * @param ui Active immediate-mode manager.
     * @param events Queue for callbacks to run after all widgets have been traversed.
     */
    void Submit(UIManager& ui, UIWidgetEvents& events) {
        if (!mVisible) {
            Suspend(events);
            return;
        }
        ui.PushLayer(mLayer);
        DrawContent(ui, events);
        ui.PopLayer();

        const UIWidgetInteraction interaction = ui.GetWidgetInteraction(mRuntimeId);
        const bool focused = ui.IsFocused(mRuntimeId.c_str());
        if (interaction.hovered != mWasHovered) {
            const auto& fn = interaction.hovered ? mOnHoverEnter : mOnHoverExit;
            if (fn) events.push_back(fn);
        }
        if (focused != mWasFocused) {
            const auto& fn = focused ? mOnFocus : mOnBlur;
            if (fn) events.push_back(fn);
        }
        if (interaction.pointerDown && mOnPointerDown) events.push_back(mOnPointerDown);
        if (interaction.pointerUp && mOnPointerUp) events.push_back(mOnPointerUp);
        mWasHovered = interaction.hovered;
        mWasFocused = focused;
    }

    /** @brief Emit one-time pointer/focus exit notifications without drawing.
     * @param events Deferred callback queue.
     */
    void Suspend(UIWidgetEvents& events) {
        if (mWasHovered && mOnHoverExit) events.push_back(mOnHoverExit);
        if (mWasFocused && mOnBlur) events.push_back(mOnBlur);
        mWasHovered = mWasFocused = false;
    }

protected:
    /** @brief Construct a widget positioned in absolute drawable pixels.
     * @param key Canvas-local identifier.
     * @param rect Absolute drawable rectangle.
     */
    UIWidget(std::string key, Rect rect) : mKey(std::move(key)), mRect(rect) {}
    /** @brief Construct a widget using responsive layout anchors.
     * @param key Canvas-local identifier.
     * @param layout Anchor and reference-canvas offsets.
     */
    UIWidget(std::string key, const UIRectTransform& layout)
        : mKey(std::move(key)), mLayout(layout) {}

    /** @brief Submit a concrete widget's existing UIManager command(s).
     * @param ui Active immediate-mode UI manager.
     * @param events Deferred callbacks receiving value snapshots.
     */
    virtual void DrawContent(UIManager& ui, UIWidgetEvents& events) = 0;
    /** @brief Get a pointer to the optional parent, whose lifetime is owned here.
     * @return Stored rectangle or nullptr for the current canvas viewport.
     */
    const Rect* Parent() const noexcept { return mParent ? &*mParent : nullptr; }
    /** @brief Assign the canvas namespace to this widget's runtime ID.
     * @param canvasKey Nonempty containing canvas identifier.
     */
    void SetCanvasKey(const std::string& canvasKey) { mRuntimeId = canvasKey + "/" + mKey; }

    std::string mKey; ///< Canvas-local name, never changed after insertion.
    std::string mRuntimeId; ///< Qualified UIManager focus and hit-test ID.
    Rect mRect{}; ///< Absolute geometry when mLayout is absent.
    std::optional<UIRectTransform> mLayout; ///< Anchored geometry if configured.
    std::optional<Rect> mParent; ///< Optional resolved parent copied by value.
    UIManager::UIStyle mStyle{}; ///< Individual colors, font and border settings.
    int mLayer = 0; ///< Render and hit-test ordering.
    bool mVisible = true; ///< Whether to submit and interact this frame.
    bool mWasHovered = false; ///< Previous submitted pointer hover state.
    bool mWasFocused = false; ///< Previous submitted keyboard focus state.
    std::function<void()> mOnHoverEnter; ///< Enter callback.
    std::function<void()> mOnHoverExit; ///< Exit callback.
    std::function<void()> mOnFocus; ///< Focus callback.
    std::function<void()> mOnBlur; ///< Focus-loss callback.
    std::function<void()> mOnPointerDown; ///< Pointer-down callback.
    std::function<void()> mOnPointerUp; ///< Pointer-up callback.

    /** @brief Permit the owning canvas to assign stable runtime widget IDs. */
    friend class UIWidgetCanvas;
};

/** @brief CRTP fluent setters shared by every UIWidget subclass.
 * @tparam Derived Concrete widget type returned by configuration calls.
 * @details Common properties change actual UIStyle fields; the drawing
 * back-end decides which style fields apply to its primitive. For textures,
 * use the subtype's SetTint/SetTexture instead of panel-only appearance.
 */
template <typename Derived>
class UIWidgetFluent : public UIWidget {
public:
    /** @brief Whether this concrete type has no rendered style surface. */
    static constexpr bool kNoStyle = std::is_same_v<Derived, UILine> ||
                                     std::is_same_v<Derived, UIInvisibleButton>;
    /** @brief Replace every appearance property.
     * @param style Complete UIManager style.
     * @return This concrete widget.
     */
    Derived& SetStyle(const UIManager::UIStyle& style)
        requires (!kNoStyle)
    { mStyle = style; return Self(); }
    /** @brief Change the widget's normal background color.
     * @param color RGBA background color.
     * @return This concrete widget.
     */
    Derived& SetColor(Color color)
        requires (!std::is_same_v<Derived, UIInvisibleButton>)
    { mStyle.btn = color; return Self(); }
    /** @brief Change its hovered background color.
     * @param color RGBA hover color.
     * @return This concrete widget.
     */
    Derived& SetHoverColor(Color color)
        requires (!std::is_same_v<Derived, UILabel> && !std::is_same_v<Derived, UIImage> &&
                  !std::is_same_v<Derived, UIPanel> && !std::is_same_v<Derived, UIProgressBar> &&
                  !std::is_same_v<Derived, UIImageButton> && !kNoStyle)
    { mStyle.btnHot = color; return Self(); }
    /** @brief Change its pressed background color.
     * @param color RGBA pressed color.
     * @return This concrete widget.
     */
    Derived& SetPressedColor(Color color)
        requires (!std::is_same_v<Derived, UILabel> && !std::is_same_v<Derived, UIImage> &&
                  !std::is_same_v<Derived, UIPanel> && !std::is_same_v<Derived, UIProgressBar> &&
                  !std::is_same_v<Derived, UIImageButton> && !kNoStyle)
    { mStyle.btnActive = color; return Self(); }
    /** @brief Change text foreground color.
     * @param color RGBA text color.
     * @return This concrete widget.
     */
    Derived& SetTextColor(Color color)
        requires (!std::is_same_v<Derived, UIImage> && !std::is_same_v<Derived, UIPanel> &&
                  !std::is_same_v<Derived, UIProgressBar> && !std::is_same_v<Derived, UIImageButton> && !kNoStyle)
    { mStyle.text = color; return Self(); }
    /** @brief Change unselected outline color.
     * @param color RGBA outline color.
     * @return This concrete widget.
     */
    Derived& SetBorderColor(Color color)
        requires (!std::is_same_v<Derived, UILabel> && !std::is_same_v<Derived, UIImage> && !kNoStyle)
    { mStyle.outline = color; return Self(); }
    /** @brief Change outline thickness.
     * @param width Nonnegative width in reference-canvas pixels.
     * @return This concrete widget.
     */
    Derived& SetBorderWidth(float width)
        requires (!std::is_same_v<Derived, UILabel> && !std::is_same_v<Derived, UIImage> && !kNoStyle)
    { mStyle.borderWidth = std::max(0.f, width); return Self(); }
    /** @brief Round every supported corner equally.
     * @param radius Radius in the configured corner-radius unit.
     * @return This concrete widget.
     */
    Derived& SetCornerRadius(float radius)
        requires (!std::is_same_v<Derived, UILabel> && !std::is_same_v<Derived, UIImage> && !kNoStyle)
    { mStyle.SetCornerRadius(radius); return Self(); }
    /** @brief Round top and bottom pairs independently.
     * @param top Radius of both upper corners.
     * @param bottom Radius of both lower corners.
     * @return This concrete widget.
     */
    Derived& SetCornerRadius(float top, float bottom)
        requires (!std::is_same_v<Derived, UILabel> && !std::is_same_v<Derived, UIImage> && !kNoStyle)
    { mStyle.SetCornerRadius(top, bottom); return Self(); }
    /** @brief Round each corner independently in clockwise order.
     * @param topLeft Upper-left radius.
     * @param topRight Upper-right radius.
     * @param bottomRight Lower-right radius.
     * @param bottomLeft Lower-left radius.
     * @return This concrete widget.
     */
    Derived& SetCornerRadius(float topLeft, float topRight, float bottomRight, float bottomLeft)
        requires (!std::is_same_v<Derived, UILabel> && !std::is_same_v<Derived, UIImage> && !kNoStyle) {
        mStyle.SetCornerRadius(topLeft, topRight, bottomRight, bottomLeft); return Self();
    }
    /** @brief Choose pixels or percentage units for corner radii.
     * @param unit Corner-radius interpretation.
     * @return This concrete widget.
     */
    Derived& SetCornerRadiusUnit(UICornerRadiusUnit unit)
        requires (!std::is_same_v<Derived, UILabel> && !std::is_same_v<Derived, UIImage> && !kNoStyle)
    { mStyle.cornerRadiusUnit = unit; return Self(); }
    /** @brief Set inner text padding.
     * @param padding Nonnegative reference-canvas pixel inset.
     * @return This concrete widget.
     */
    Derived& SetPadding(float padding)
        requires (!kNoStyle)
    { mStyle.padding = std::max(0.f, padding); return Self(); }
    /** @brief Select the font asset used by this widget.
     * @param fontKey Registered AssetManager font key.
     * @return This concrete widget.
     */
    Derived& SetFont(const std::string& fontKey)
        requires (!std::is_same_v<Derived, UIImage> && !std::is_same_v<Derived, UIPanel> &&
                  !std::is_same_v<Derived, UIProgressBar> && !std::is_same_v<Derived, UIImageButton> && !kNoStyle)
    { mStyle.defaultFontKey = fontKey; return Self(); }
    /** @brief Configure its focus border color.
     * @param color RGBA focus color.
     * @return This concrete widget.
     */
    Derived& SetFocusColor(Color color)
        requires (!kNoStyle && !std::is_same_v<Derived, UIPanel> &&
                  !std::is_same_v<Derived, UILabel> && !std::is_same_v<Derived, UIImage> &&
                  !std::is_same_v<Derived, UIProgressBar>)
    { mStyle.focus = color; return Self(); }
    /** @brief Set absolute screen-space bounds, disabling anchored layout.
     * @param rect Drawable-pixel rectangle.
     * @return This concrete widget.
     */
    Derived& SetRect(Rect rect) { mRect = rect; mLayout.reset(); return Self(); }
    /** @brief Assign responsive geometry.
     * @param layout Reference-canvas anchor and size configuration.
     * @return This concrete widget.
     */
    Derived& SetLayout(const UIRectTransform& layout) { mLayout = layout; return Self(); }
    /** @brief Set the position in the active geometry mode.
     * @param position Top-left drawable pixels for Rect; anchored pivot offset for UIRectTransform.
     * @return This concrete widget.
     */
    Derived& SetPosition(Vec2 position) {
        if (mLayout) mLayout->anchoredPosition = position;
        else { mRect.x = position.x; mRect.y = position.y; }
        return Self();
    }
    /** @brief Set dimensions in the active geometry mode.
     * @param size Drawable pixels for Rect; reference-canvas sizeDelta for anchored layout.
     * @return This concrete widget.
     */
    Derived& SetSize(Vec2 size) {
        if (mLayout) mLayout->sizeDelta = size;
        else { mRect.w = size.x; mRect.h = size.y; }
        return Self();
    }
    /** @brief Set the normalized anchor rectangle, enabling anchored geometry.
     * @param minimum Normalized top-left anchors.
     * @param maximum Normalized bottom-right anchors.
     * @return This concrete widget.
     */
    Derived& SetAnchors(Vec2 minimum, Vec2 maximum) {
        if (!mLayout) mLayout = UIRectTransform{};
        mLayout->anchorMin = minimum; mLayout->anchorMax = maximum;
        return Self();
    }
    /** @brief Set the normalized pivot for anchored geometry.
     * @param pivot Normalized relative pivot.
     * @return This concrete widget.
     */
    Derived& SetPivot(Vec2 pivot) {
        if (!mLayout) mLayout = UIRectTransform{};
        mLayout->pivot = pivot; return Self();
    }
    /** @brief Set a copied parent rectangle for anchored geometry.
     * @param parent Resolved parent, or nullptr to use the canvas viewport.
     * @return This concrete widget.
     */
    Derived& SetParent(const Rect* parent) { if (parent) mParent = *parent; else mParent.reset(); return Self(); }
    /** @brief Set the absolute interaction/render layer.
     * @param layer Larger integers appear above smaller ones.
     * @return This concrete widget.
     */
    Derived& SetLayer(int layer) { mLayer = layer; return Self(); }
    /** @brief Show or hide the widget without destroying state.
     * @param visible Whether the canvas submits it.
     * @return This concrete widget.
     */
    Derived& SetVisible(bool visible) { mVisible = visible; return Self(); }
    /** @brief Listen for pointer hover entering the widget.
     * @param callback Invoked once on an enter transition.
     * @return This concrete widget.
     */
    Derived& OnHoverEnter(std::function<void()> callback) { mOnHoverEnter = std::move(callback); return Self(); }
    /** @brief Listen for pointer hover leaving the widget.
     * @param callback Invoked once on a leave transition.
     * @return This concrete widget.
     */
    Derived& OnHoverExit(std::function<void()> callback) { mOnHoverExit = std::move(callback); return Self(); }
    /** @brief Listen for keyboard/gamepad focus entering the widget.
     * @param callback Invoked once on a focus transition.
     * @return This concrete widget.
     */
    Derived& OnFocus(std::function<void()> callback) { mOnFocus = std::move(callback); return Self(); }
    /** @brief Listen for focus leaving the widget.
     * @param callback Invoked once on a blur transition.
     * @return This concrete widget.
     */
    Derived& OnBlur(std::function<void()> callback) { mOnBlur = std::move(callback); return Self(); }
    /** @brief Listen for a left-pointer press starting on this control.
     * @param callback Invoked on the initial press, not on click completion.
     * @return This concrete widget.
     */
    Derived& OnPointerDown(std::function<void()> callback) { mOnPointerDown = std::move(callback); return Self(); }
    /** @brief Listen for left-pointer release after a press on this control.
     * @param callback Invoked even when released outside its bounds.
     * @return This concrete widget.
     */
    Derived& OnPointerUp(std::function<void()> callback) { mOnPointerUp = std::move(callback); return Self(); }

public:
    /** @brief Construct a fluent element with absolute geometry.
     * @param key Canvas-local identifier.
     * @param rect Drawable-pixel bounds.
     */
    UIWidgetFluent(std::string key, Rect rect) : UIWidget(std::move(key), rect) {}
    /** @brief Construct a fluent element with anchored geometry.
     * @param key Canvas-local identifier.
     * @param layout Anchor configuration.
     */
    UIWidgetFluent(std::string key, const UIRectTransform& layout) : UIWidget(std::move(key), layout) {}
protected:
    /** @brief Cast the fluent base to the concrete widget type.
     * @return Reference to this concrete object.
     */
    Derived& Self() noexcept { return static_cast<Derived&>(*this); }
};

/** @brief Retained text button with completed-click and pointer callbacks. */
class UIButton : public UIWidgetFluent<UIButton> {
public:
    using UIWidgetFluent<UIButton>::UIWidgetFluent;
    /** @brief Set the button caption.
     * @param text UTF-8 caption.
     * @return This button.
     */
    UIButton& SetText(std::string text) { mText = std::move(text); return *this; }
    /** @brief Read the button caption.
     * @return Current UTF-8 caption.
     */
    const std::string& GetText() const noexcept { return mText; }
    /** @brief Set the activation callback (release after press, or UI Submit).
     * @param callback Invoked once per completed click.
     * @return This button.
     */
    UIButton& OnClick(std::function<void()> callback) { mOnClick = std::move(callback); return *this; }

protected:
    /** @brief Submit the styled button and defer click notification.
     * @param ui Active UI manager.
     * @param events Deferred event queue.
     */
    void DrawContent(UIManager& ui, UIWidgetEvents& events) override {
        const bool activated = mLayout
            ? ui.Button(mRuntimeId.c_str(), *mLayout, mStyle, mText, Parent())
            : ui.Button(mRuntimeId.c_str(), mRect, mStyle, mText);
        if (activated && mOnClick) events.push_back(mOnClick);
    }
private:
    std::string mText; ///< Button label.
    std::function<void()> mOnClick; ///< Activation delegate.
};

/** @brief Retained image button with per-state texture tints. */
class UIImageButton : public UIWidgetFluent<UIImageButton> {
public:
    using UIWidgetFluent<UIImageButton>::UIWidgetFluent;
    /** @brief Set the base image tint.
     * @param color RGBA tint.
     * @return This image button.
     */
    UIImageButton& SetColor(Color color) { return SetTint(color); }
    /** @brief Set the nonowning texture to render.
     * @param texture AssetManager-owned texture, which must outlive the button.
     * @return This image button.
     */
    UIImageButton& SetTexture(const Texture& texture) { mTexture = &texture; return *this; }
    /** @brief Set the normal texture tint.
     * @param color RGBA texture tint.
     * @return This image button.
     */
    UIImageButton& SetTint(Color color) { mTints.normal = color; return *this; }
    /** @brief Set the hover texture tint.
     * @param color RGBA texture tint.
     * @return This image button.
     */
    UIImageButton& SetHoverTint(Color color) { mTints.hover = color; return *this; }
    /** @brief Set the pressed texture tint.
     * @param color RGBA texture tint.
     * @return This image button.
     */
    UIImageButton& SetPressedTint(Color color) { mTints.active = color; return *this; }
    /** @brief Set the image-frame outline color.
     * @param color RGBA border tint.
     * @return This image button.
     */
    UIImageButton& SetFrameColor(Color color) { mTints.outline = color; return *this; }
    /** @brief Set the native image frame's outline tint.
     * @param color RGBA outline color.
     * @return This image button.
     */
    UIImageButton& SetBorderColor(Color color) { return SetFrameColor(color); }
    /** @brief Set the optional texture subrectangle.
     * @param pixels Source pixels, or nullptr for the full image.
     * @return This image button.
     */
    UIImageButton& SetSourceRect(const Rect* pixels) { mSource = pixels ? std::optional<Rect>(*pixels) : std::nullopt; return *this; }
    /** @brief Show or hide the image button's frame.
     * @param visible Whether its frame is drawn.
     * @return This image button.
     */
    UIImageButton& SetDrawFrame(bool visible) { mDrawFrame = visible; return *this; }
    /** @brief Set the image button activation callback.
     * @param callback Invoked after completed click or UI Submit.
     * @return This image button.
     */
    UIImageButton& OnClick(std::function<void()> callback) { mOnClick = std::move(callback); return *this; }
protected:
    /** @brief Submit the textured button with its own frame palette.
     * @param ui Active UI manager.
     * @param events Deferred event queue.
     */
    void DrawContent(UIManager& ui, UIWidgetEvents& events) override {
        if (!mTexture) return;
        const Rect* source = mSource ? &*mSource : nullptr;
        const bool activated = mLayout
            ? ui.ImageButton(mRuntimeId.c_str(), *mLayout, *mTexture, mStyle, source,
                             mTints, mDrawFrame, Parent())
            : ui.ImageButton(mRuntimeId.c_str(), mRect, *mTexture, mStyle, source,
                             mTints, mDrawFrame);
        if (activated && mOnClick) events.push_back(mOnClick);
    }
private:
    const Texture* mTexture = nullptr; ///< Nonowning texture asset.
    std::optional<Rect> mSource; ///< Optional copied source pixels.
    UIButtonColors mTints{{255,255,255,255}, {230,230,230,255},
                          {200,200,200,255}, {200,200,200,0}}; ///< Texture state colors.
    bool mDrawFrame = true; ///< Frame visibility.
    std::function<void()> mOnClick; ///< Click delegate.
};

/** @brief Retained solid or outlined decorative panel. */
class UIPanel : public UIWidgetFluent<UIPanel> {
public:
    using UIWidgetFluent<UIPanel>::UIWidgetFluent;
    /** @brief Set whether to fill the panel interior.
     * @param filled Whether the panel is solid.
     * @return This panel.
     */
    UIPanel& SetFilled(bool filled) { mFilled = filled; return *this; }
protected:
    /** @brief Submit the filled or outlined panel.
     * @param ui Active UI manager.
     * @param events Unused event queue; panels are decorative.
     */
    void DrawContent(UIManager& ui, [[maybe_unused]] UIWidgetEvents& events) override {
        if (mLayout) ui.Panel(*mLayout, mStyle, mFilled, Parent());
        else ui.Panel(mRect, mStyle, mFilled);
    }
private:
    bool mFilled = true; ///< Fill toggle.
};

/** @brief Retained UI text label with text-specific alignment and color. */
class UILabel : public UIWidgetFluent<UILabel> {
public:
    using UIWidgetFluent<UILabel>::UIWidgetFluent;
    /** @brief Set displayed UTF-8 text.
     * @param text Label content.
     * @return This label.
     */
    UILabel& SetText(std::string text) { mText = std::move(text); return *this; }
    /** @brief Set label text color (labels have no background).
     * @param color RGBA text color.
     * @return This label.
     */
    UILabel& SetColor(Color color) { mStyle.text = color; return *this; }
    /** @brief Set horizontal and vertical alignment.
     * @param horizontal Left, center or right.
     * @param vertical Top, middle or bottom.
     * @return This label.
     */
    UILabel& SetAlignment(AlignH horizontal, AlignV vertical) {
        mHorizontal = horizontal; mVertical = vertical; return *this;
    }
    /** @brief Set text inset (negative means the style default).
     * @param padding Text inset in UI pixels.
     * @return This label.
     */
    UILabel& SetTextPadding(float padding) { mTextPadding = padding; return *this; }
protected:
    /** @brief Submit the aligned label with local text appearance.
     * @param ui Active UI manager.
     * @param events Unused event queue; labels are decorative.
     */
    void DrawContent(UIManager& ui, [[maybe_unused]] UIWidgetEvents& events) override {
        if (mLayout)
            ui.Label(mText, *mLayout, mStyle, mHorizontal, mVertical, mTextPadding, Parent());
        else ui.Label(mText, mRect, mStyle, mHorizontal, mVertical, mTextPadding);
    }
private:
    std::string mText; ///< Displayed label text.
    AlignH mHorizontal = AlignH::Center; ///< Horizontal alignment.
    AlignV mVertical = AlignV::Middle; ///< Vertical alignment.
    float mTextPadding = -1.f; ///< Text inset or style default.
};

/** @brief Retained checkbox owning its boolean state. */
class UICheckbox : public UIWidgetFluent<UICheckbox> {
public:
    using UIWidgetFluent<UICheckbox>::UIWidgetFluent;
    /** @brief Set checkbox background color (the primary fill of this control).
     * @param color RGBA box fill.
     * @return This checkbox.
     */
    UICheckbox& SetColor(Color color) { return SetBoxColor(color); }
    /** @brief Set optional checkbox label.
     * @param label UTF-8 caption.
     * @return This checkbox.
     */
    UICheckbox& SetLabel(std::string label) { mLabel = std::move(label); return *this; }
    /** @brief Set the color of the selected check mark.
     * @param color RGBA check color.
     * @return This checkbox.
     */
    UICheckbox& SetCheckColor(Color color) { mStyle.checkboxCheck = color; return *this; }
    /** @brief Set the unselected checkbox fill.
     * @param color RGBA empty-box color.
     * @return This checkbox.
     */
    UICheckbox& SetBoxColor(Color color) { mStyle.checkboxBackground = color; return *this; }
    /** @brief Set the checked value and notify the listener on a change.
     * @param value New checkbox state.
     * @return This checkbox.
     */
    UICheckbox& SetValue(bool value) { if (mValue != value) { mValue = value; if (mOnChanged) mOnChanged(value); } return *this; }
    /** @brief Set the checked state without raising OnValueChanged.
     * @param value New checkbox state.
     * @return This checkbox.
     */
    UICheckbox& SetValueWithoutNotify(bool value) { mValue = value; return *this; }
    /** @brief Read the current checkbox state.
     * @return True when checked.
     */
    bool GetValue() const noexcept { return mValue; }
    /** @brief Set the checkbox change listener.
     * @param callback Receives the new checked state.
     * @return This checkbox.
     */
    UICheckbox& OnValueChanged(std::function<void(bool)> callback) { mOnChanged = std::move(callback); return *this; }
protected:
    /** @brief Submit the checkbox and defer changed-value notification.
     * @param ui Active UI manager.
     * @param events Deferred event queue.
     */
    void DrawContent(UIManager& ui, UIWidgetEvents& events) override {
        const bool changed = mLayout
            ? ui.Checkbox(mRuntimeId.c_str(), *mLayout, &mValue, mStyle, mLabel, Parent())
            : ui.Checkbox(mRuntimeId.c_str(), mRect, &mValue, mStyle, mLabel);
        if (changed && mOnChanged) {
            auto callback = mOnChanged; const bool snapshot = mValue;
            events.push_back([callback, snapshot] { callback(snapshot); });
        }
    }
private:
    std::string mLabel; ///< Checkbox caption.
    bool mValue = false; ///< Owned checked state.
    std::function<void(bool)> mOnChanged; ///< Change callback.
};

/** @brief Retained range slider with owned numeric value and typed change event. */
class UISlider : public UIWidgetFluent<UISlider> {
public:
    using UIWidgetFluent<UISlider>::UIWidgetFluent;
    /** @brief Configure the allowed slider range.
     * @param minimum Inclusive lower endpoint.
     * @param maximum Inclusive upper endpoint, strictly larger than minimum.
     * @return This slider.
     * @throws std::invalid_argument If the interval is nonfinite or empty.
     */
    UISlider& SetRange(float minimum, float maximum) {
        if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum >= maximum)
            throw std::invalid_argument("UISlider: minimum must be less than maximum");
        mMin = minimum; mMax = maximum;
        mValue = std::clamp(mValue, mMin, mMax);
        return *this;
    }
    /** @brief Set the slider's input and snapping increment.
     * @param step Positive finite step.
     * @return This slider.
     * @throws std::invalid_argument If step is invalid.
     */
    UISlider& SetStep(float step) {
        if (!std::isfinite(step) || step <= 0.f)
            throw std::invalid_argument("UISlider: step must be positive");
        mStep = step; return *this;
    }
    /** @brief Change the slider value, notifying if it changed.
     * @param value New value, clamped to the configured range.
     * @return This slider.
     */
    UISlider& SetValue(float value) {
        const float next = std::isfinite(value) ? std::clamp(value, mMin, mMax) : mValue;
        if (next != mValue) { mValue = next; if (mOnChanged) mOnChanged(mValue); }
        return *this;
    }
    /** @brief Set slider value without invoking the change event.
     * @param value New value, clamped to the current range.
     * @return This slider.
     */
    UISlider& SetValueWithoutNotify(float value) {
        if (std::isfinite(value)) mValue = std::clamp(value, mMin, mMax);
        return *this;
    }
    /** @brief Read the current slider value.
     * @return Value in the configured range.
     */
    float GetValue() const noexcept { return mValue; }
    /** @brief Configure the slider's track color.
     * @param color RGBA track color.
     * @return This slider.
     */
    UISlider& SetTrackColor(Color color) { mStyle.sliderTrack = color; return *this; }
    /** @brief Configure the filled portion's color.
     * @param color RGBA fill color.
     * @return This slider.
     */
    UISlider& SetFillColor(Color color) { mStyle.sliderFill = color; return *this; }
    /** @brief Configure the handle's fill color.
     * @param color RGBA handle color.
     * @return This slider.
     */
    UISlider& SetHandleColor(Color color) { mStyle.sliderHandle = color; return *this; }
    /** @brief Configure the track outline color.
     * @param color RGBA outline color.
     * @return This slider.
     */
    UISlider& SetTrackBorderColor(Color color) { mStyle.sliderTrackOutline = color; return *this; }
    /** @brief Configure the handle outline color.
     * @param color RGBA outline color.
     * @return This slider.
     */
    UISlider& SetHandleBorderColor(Color color) { mStyle.sliderHandleOutline = color; return *this; }
    /** @brief Set the handle diameter in UI pixels.
     * @param diameter Nonnegative diameter.
     * @return This slider.
     */
    UISlider& SetHandleDiameter(float diameter) { mStyle.sliderHandleDiameter = std::max(0.f, diameter); return *this; }
    /** @brief Set the track thickness in UI pixels.
     * @param thickness Nonnegative track thickness.
     * @return This slider.
     */
    UISlider& SetTrackThickness(float thickness) { mStyle.sliderTrackThickness = std::max(0.f, thickness); return *this; }
    /** @brief Configure rounded track and handle shapes.
     * @param rounded Whether to render circular/capsule elements.
     * @return This slider.
     */
    UISlider& SetRoundElements(bool rounded) { mStyle.sliderRoundElements = rounded; return *this; }
    /** @brief Toggle drawing the outer slider background.
     * @param visible Whether to draw the background.
     * @return This slider.
     */
    UISlider& SetShowBackground(bool visible) { mStyle.sliderShowBackground = visible; return *this; }
    /** @brief Subscribe to user or programmatic value changes.
     * @param callback Receives the updated slider value.
     * @return This slider.
     */
    UISlider& OnValueChanged(std::function<void(float)> callback) { mOnChanged = std::move(callback); return *this; }
    /** @brief Subscribe to the beginning of a pointer drag.
     * @param callback Callback invoked on pointer down.
     * @return This slider.
     */
    UISlider& OnDragStart(std::function<void()> callback) { mDragStart = std::move(callback); return *this; }
    /** @brief Subscribe to the end of a pointer drag.
     * @param callback Callback invoked on pointer up.
     * @return This slider.
     */
    UISlider& OnDragEnd(std::function<void()> callback) { mDragEnd = std::move(callback); return *this; }
protected:
    /** @brief Submit the slider, changed value and pointer drag transitions.
     * @param ui Active UI manager.
     * @param events Deferred event queue.
     */
    void DrawContent(UIManager& ui, UIWidgetEvents& events) override {
        const bool changed = mLayout
            ? ui.Slider(mRuntimeId.c_str(), *mLayout, &mValue, mStyle, mMin, mMax, mStep, Parent())
            : ui.Slider(mRuntimeId.c_str(), mRect, &mValue, mStyle, mMin, mMax, mStep);
        if (changed && mOnChanged) {
            auto callback = mOnChanged; const float snapshot = mValue;
            events.push_back([callback, snapshot] { callback(snapshot); });
        }
        const UIWidgetInteraction interaction = ui.GetWidgetInteraction(mRuntimeId);
        if (interaction.pointerDown && mDragStart) events.push_back(mDragStart);
        if (interaction.pointerUp && mDragEnd) events.push_back(mDragEnd);
    }
private:
    float mValue = 0.f; ///< Current bounded value.
    float mMin = 0.f; ///< Minimum.
    float mMax = 1.f; ///< Maximum.
    float mStep = 0.05f; ///< Input step.
    std::function<void(float)> mOnChanged; ///< Value change delegate.
    std::function<void()> mDragStart; ///< Initial drag delegate.
    std::function<void()> mDragEnd; ///< Drag-end delegate.
};

/** @brief Retained on/off switch owning its state. */
class UIToggle : public UIWidgetFluent<UIToggle> {
public:
    using UIWidgetFluent<UIToggle>::UIWidgetFluent;
    /** @brief Set the optional state label.
     * @param label Text shown beside the track.
     * @return This toggle.
     */
    UIToggle& SetLabel(std::string label) { mLabel = std::move(label); return *this; }
    /** @brief Set enabled-track color.
     * @param color RGBA enabled color.
     * @return This toggle.
     */
    UIToggle& SetOnColor(Color color) { mStyle.toggleOn = color; return *this; }
    /** @brief Set disabled-track color.
     * @param color RGBA disabled color.
     * @return This toggle.
     */
    UIToggle& SetOffColor(Color color) { mStyle.toggleOff = color; return *this; }
    /** @brief Toggle the background rectangle.
     * @param visible Whether to draw the outer background.
     * @return This toggle.
     */
    UIToggle& SetShowBackground(bool visible) { mStyle.toggleShowBackground = visible; return *this; }
    /** @brief Round the native track and thumb.
     * @param rounded Whether to use capsule and circle shapes.
     * @return This toggle.
     */
    UIToggle& SetRoundElements(bool rounded) { mStyle.toggleRoundElements = rounded; return *this; }
    /** @brief Limit pointer hits to the switch track, rather than its whole label.
     * @param trackOnly Whether to restrict the hit area.
     * @return This toggle.
     */
    UIToggle& SetTrackOnlyHit(bool trackOnly) { mStyle.toggleTrackOnlyHit = trackOnly; return *this; }
    /** @brief Show ON/OFF text when no label was configured.
     * @param visible Whether state labels are enabled.
     * @return This toggle.
     */
    UIToggle& SetShowStateText(bool visible) { mStyle.toggleShowStateText = visible; return *this; }
    /** @brief Set checked state and notify on change.
     * @param value New toggle value.
     * @return This toggle.
     */
    UIToggle& SetValue(bool value) { if (mValue != value) { mValue = value; if (mOnChanged) mOnChanged(value); } return *this; }
    /** @brief Set checked state without sending OnValueChanged.
     * @param value New toggle value.
     * @return This toggle.
     */
    UIToggle& SetValueWithoutNotify(bool value) { mValue = value; return *this; }
    /** @brief Read checked state.
     * @return True when toggled on.
     */
    bool GetValue() const noexcept { return mValue; }
    /** @brief Register the checked-state change callback.
     * @param callback Receives the updated state.
     * @return This toggle.
     */
    UIToggle& OnValueChanged(std::function<void(bool)> callback) { mOnChanged = std::move(callback); return *this; }
protected:
    /** @brief Submit the toggle and defer changed-value notification.
     * @param ui Active UI manager.
     * @param events Deferred event queue.
     */
    void DrawContent(UIManager& ui, UIWidgetEvents& events) override {
        const bool changed = mLayout
            ? ui.Toggle(mRuntimeId.c_str(), *mLayout, &mValue, mStyle, mLabel, Parent())
            : ui.Toggle(mRuntimeId.c_str(), mRect, &mValue, mStyle, mLabel);
        if (changed && mOnChanged) {
            auto callback = mOnChanged; const bool snapshot = mValue;
            events.push_back([callback, snapshot] { callback(snapshot); });
        }
    }
private:
    bool mValue = false; ///< Current on/off state.
    std::string mLabel; ///< Optional caption.
    std::function<void(bool)> mOnChanged; ///< Change delegate.
};

/** @brief Retained UTF-8 text field with change, completion and submit events. */
class UITextInput : public UIWidgetFluent<UITextInput> {
public:
    using UIWidgetFluent<UITextInput>::UIWidgetFluent;
    /** @brief Set the empty-input hint.
     * @param placeholder Placeholder text.
     * @return This field.
     */
    UITextInput& SetPlaceholder(std::string placeholder) { mPlaceholder = std::move(placeholder); return *this; }
    /** @brief Set the hint foreground color.
     * @param color RGBA hint color.
     * @return This field.
     */
    UITextInput& SetPlaceholderColor(Color color) { mStyle.placeholder = color; return *this; }
    /** @brief Set the selected text background color.
     * @param color RGBA selection fill.
     * @return This field.
     */
    UITextInput& SetSelectionColor(Color color) { mStyle.textSelection = color; return *this; }
    /** @brief Set the maximum UTF-8 code point count.
     * @param count Maximum characters accepted during editing.
     * @return This field.
     */
    UITextInput& SetMaxCharacters(size_t count) { mMaxCharacters = count; return *this; }
    /** @brief Set text and notify listeners when changed.
     * @param value UTF-8 content; programmatic content is not silently truncated.
     * @return This field.
     */
    UITextInput& SetValue(std::string value) {
        if (mValue != value) { mValue = std::move(value); if (mOnChanged) mOnChanged(mValue); }
        return *this;
    }
    /** @brief Replace text without raising the value-change event.
     * @param value UTF-8 content.
     * @return This field.
     */
    UITextInput& SetValueWithoutNotify(std::string value) { mValue = std::move(value); return *this; }
    /** @brief Read the current field content.
     * @return Text owned by this widget.
     */
    const std::string& GetValue() const noexcept { return mValue; }
    /** @brief Register a per-keystroke or programmatic text-change event.
     * @param callback Receives the new text by const reference.
     * @return This field.
     */
    UITextInput& OnValueChanged(std::function<void(const std::string&)> callback) { mOnChanged = std::move(callback); return *this; }
    /** @brief Register an editing-completed event.
     * @param callback Receives text after focus/keyboard editing ends.
     * @return This field.
     */
    UITextInput& OnEndEdit(std::function<void(const std::string&)> callback) { mOnEndEdit = std::move(callback); return *this; }
    /** @brief Register an explicit Enter/UI Submit completion event.
     * @param callback Receives the submitted text.
     * @return This field.
     */
    UITextInput& OnSubmit(std::function<void(const std::string&)> callback) { mOnSubmit = std::move(callback); return *this; }
protected:
    /** @brief Submit text editing and defer change, completion and submit events.
     * @param ui Active UI manager.
     * @param events Deferred event queue.
     */
    void DrawContent(UIManager& ui, UIWidgetEvents& events) override {
        const bool changed = mLayout
            ? ui.TextInput(mRuntimeId.c_str(), *mLayout, &mValue, mStyle, mPlaceholder,
                           mMaxCharacters, Parent())
            : ui.TextInput(mRuntimeId.c_str(), mRect, &mValue, mStyle, mPlaceholder, mMaxCharacters);
        if (changed && mOnChanged) {
            auto callback = mOnChanged; const std::string snapshot = mValue;
            events.push_back([callback, snapshot] { callback(snapshot); });
        }
        const bool editing = ui.IsTextEditing(mRuntimeId);
        if (mWasEditing && !editing) {
            const std::string snapshot = mValue;
            if (mOnEndEdit) { auto callback = mOnEndEdit; events.push_back([callback, snapshot] { callback(snapshot); }); }
            if (ui.WasTextSubmitted(mRuntimeId) && mOnSubmit) {
                auto callback = mOnSubmit; events.push_back([callback, snapshot] { callback(snapshot); });
            }
        }
        mWasEditing = editing;
    }
private:
    std::string mValue; ///< Owned current UTF-8 string.
    std::string mPlaceholder; ///< Empty field hint.
    size_t mMaxCharacters = 64; ///< Editing limit.
    bool mWasEditing = false; ///< Previous editing state.
    std::function<void(const std::string&)> mOnChanged; ///< Change delegate.
    std::function<void(const std::string&)> mOnEndEdit; ///< Completion delegate.
    std::function<void(const std::string&)> mOnSubmit; ///< Submit delegate.
};

/** @brief Retained dropdown storing its options and selected index. */
class UIDropdown : public UIWidgetFluent<UIDropdown> {
public:
    using UIWidgetFluent<UIDropdown>::UIWidgetFluent;
    /** @brief Replace the displayed options.
     * @param options UTF-8 option captions.
     * @return This dropdown.
     */
    UIDropdown& SetOptions(std::vector<std::string> options) { mOptions = std::move(options); return *this; }
    /** @brief Set the selected index and notify on change.
     * @param index Zero-based option index, or -1 for no selection.
     * @return This dropdown.
     */
    UIDropdown& SetSelectedIndex(int index) {
        if (mSelected != index) { mSelected = index; if (mOnChanged) mOnChanged(index); }
        return *this;
    }
    /** @brief Set the selection without notifying listeners.
     * @param index Zero-based selection index, or -1.
     * @return This dropdown.
     */
    UIDropdown& SetSelectedIndexWithoutNotify(int index) { mSelected = index; return *this; }
    /** @brief Read the current selected index.
     * @return Zero-based selected option or -1 for none.
     */
    int GetSelectedIndex() const noexcept { return mSelected; }
    /** @brief Set maximum visible popup rows.
     * @param rows Positive visible row count (UIManager internally clamps to 12).
     * @return This dropdown.
     */
    UIDropdown& SetMaxVisibleItems(int rows) { mMaxVisibleItems = std::clamp(rows, 1, 12); return *this; }
    /** @brief Set popup background color.
     * @param color RGBA popup fill.
     * @return This dropdown.
     */
    UIDropdown& SetPopupColor(Color color) { mStyle.dropdownBackground = color; return *this; }
    /** @brief Set selected-row color.
     * @param color RGBA selected row color.
     * @return This dropdown.
     */
    UIDropdown& SetSelectedColor(Color color) { mStyle.dropdownSelected = color; return *this; }
    /** @brief Set hovered-row color.
     * @param color RGBA hover row color.
     * @return This dropdown.
     */
    UIDropdown& SetRowHoverColor(Color color) { mStyle.dropdownHovered = color; return *this; }
    /** @brief Set the arrow glyph color.
     * @param color RGBA arrow color.
     * @return This dropdown.
     */
    UIDropdown& SetArrowColor(Color color) { mStyle.dropdownArrow = color; return *this; }
    /** @brief Register a selection-change callback.
     * @param callback Receives the zero-based selected index.
     * @return This dropdown.
     */
    UIDropdown& OnValueChanged(std::function<void(int)> callback) { mOnChanged = std::move(callback); return *this; }
    /** @brief Register a popup-open callback.
     * @param callback Invoked when the dropdown becomes expanded.
     * @return This dropdown.
     */
    UIDropdown& OnOpen(std::function<void()> callback) { mOnOpen = std::move(callback); return *this; }
    /** @brief Register a popup-close callback.
     * @param callback Invoked when the dropdown becomes collapsed.
     * @return This dropdown.
     */
    UIDropdown& OnClose(std::function<void()> callback) { mOnClose = std::move(callback); return *this; }
protected:
    /** @brief Submit dropdown rows and defer selection/open-state events.
     * @param ui Active UI manager.
     * @param events Deferred event queue.
     */
    void DrawContent(UIManager& ui, UIWidgetEvents& events) override {
        const bool changed = mLayout
            ? ui.Dropdown(mRuntimeId.c_str(), *mLayout, &mSelected, mOptions, mStyle,
                          mMaxVisibleItems, Parent())
            : ui.Dropdown(mRuntimeId.c_str(), mRect, &mSelected, mOptions, mStyle, mMaxVisibleItems);
        if (changed && mOnChanged) {
            auto callback = mOnChanged; const int snapshot = mSelected;
            events.push_back([callback, snapshot] { callback(snapshot); });
        }
        const bool open = ui.IsDropdownOpen(mRuntimeId);
        if (open != mWasOpen) {
            const auto& callback = open ? mOnOpen : mOnClose;
            if (callback) events.push_back(callback);
        }
        mWasOpen = open;
    }
private:
    std::vector<std::string> mOptions; ///< Owned dropdown captions.
    int mSelected = -1; ///< Current selection.
    int mMaxVisibleItems = 6; ///< Popup row limit.
    bool mWasOpen = false; ///< Previous popup state.
    std::function<void(int)> mOnChanged; ///< Value-change callback.
    std::function<void()> mOnOpen; ///< Popup-open callback.
    std::function<void()> mOnClose; ///< Popup-close callback.
};

/** @brief Retained progress meter owning a normalized fill value. */
class UIProgressBar : public UIWidgetFluent<UIProgressBar> {
public:
    using UIWidgetFluent<UIProgressBar>::UIWidgetFluent;
    /** @brief Set the progress fill (the primary color of a progress bar).
     * @param color RGBA progress color.
     * @return This progress bar.
     */
    UIProgressBar& SetColor(Color color) { return SetFillColor(color); }
    /** @brief Set the normalized fill fraction.
     * @param value Normalized fraction, clamped to [0,1].
     * @return This progress bar.
     */
    UIProgressBar& SetValue(float value) { if (std::isfinite(value)) mValue = std::clamp(value, 0.f, 1.f); return *this; }
    /** @brief Read the current normalized fill.
     * @return Fraction between 0 and 1.
     */
    float GetValue() const noexcept { return mValue; }
    /** @brief Set progress foreground fill.
     * @param color RGBA foreground color.
     * @return This progress bar.
     */
    UIProgressBar& SetFillColor(Color color) { mStyle.progressFill = color; return *this; }
    /** @brief Set progress background fill.
     * @param color RGBA background color.
     * @return This progress bar.
     */
    UIProgressBar& SetBackgroundColor(Color color) { mStyle.progressBack = color; return *this; }
    /** @brief Set progress outline color.
     * @param color RGBA outline color.
     * @return This progress bar.
     */
    UIProgressBar& SetProgressBorderColor(Color color) { mStyle.progressOutline = color; return *this; }
    /** @brief Set the progress-meter outline rather than a button outline.
     * @param color RGBA progress outline.
     * @return This progress bar.
     */
    UIProgressBar& SetBorderColor(Color color) { return SetProgressBorderColor(color); }
protected:
    /** @brief Submit the normalized filled progress bar.
     * @param ui Active UI manager.
     * @param events Unused event queue; progress bars are decorative.
     */
    void DrawContent(UIManager& ui, [[maybe_unused]] UIWidgetEvents& events) override {
        if (mLayout) ui.ProgressBar(mValue, *mLayout, mStyle, Parent());
        else ui.ProgressBar(mValue, mRect, mStyle);
    }
private:
    float mValue = 0.f; ///< Normalized fill.
};

/** @brief Retained image view with source cropping, tint and flipping.
 * @note Plain textures are rectangular: border and corner rounding are
 * intentionally not offered, because UIManager::Image cannot clip a texture
 * to a curved silhouette. Use an UIImageButton frame for framed images.
 */
class UIImage : public UIWidgetFluent<UIImage> {
public:
    using UIWidgetFluent<UIImage>::UIWidgetFluent;
    /** @brief Select a nonowning AssetManager texture.
     * @param texture Texture which must outlive this view.
     * @return This image.
     */
    UIImage& SetTexture(const Texture& texture) { mTexture = &texture; return *this; }
    /** @brief Change texture tint.
     * @param color RGBA tint applied to the image.
     * @return This image.
     */
    UIImage& SetColor(Color color) { mTint = color; return *this; }
    /** @brief Change texture tint.
     * @param color RGBA tint applied to the image.
     * @return This image.
     */
    UIImage& SetTint(Color color) { return SetColor(color); }
    /** @brief Set source pixels, or reset to entire texture.
     * @param pixels Optional source rectangle.
     * @return This image.
     */
    UIImage& SetSourceRect(const Rect* pixels) { mSource = pixels ? std::optional<Rect>(*pixels) : std::nullopt; return *this; }
    /** @brief Set horizontal and vertical texture mirroring.
     * @param horizontal Flip horizontally.
     * @param vertical Flip vertically.
     * @return This image.
     */
    UIImage& SetFlip(bool horizontal, bool vertical) { mFlipX = horizontal; mFlipY = vertical; return *this; }
protected:
    /** @brief Submit the tinted texture without introducing an extra render path.
     * @param ui Active UI manager.
     * @param events Unused event queue; images are decorative.
     */
    void DrawContent(UIManager& ui, [[maybe_unused]] UIWidgetEvents& events) override {
        if (!mTexture) return;
        const Rect* source = mSource ? &*mSource : nullptr;
        if (mLayout) ui.Image(*mTexture, *mLayout, source, mTint, mFlipX, mFlipY, Parent());
        else ui.Image(*mTexture, mRect, source, mTint, mFlipX, mFlipY);
    }
private:
    const Texture* mTexture = nullptr; ///< Nonowning texture asset.
    std::optional<Rect> mSource; ///< Optional copied source rectangle.
    Color mTint{255,255,255,255}; ///< Image tint.
    bool mFlipX = false; ///< Horizontal mirroring.
    bool mFlipY = false; ///< Vertical mirroring.
};

/** @brief Retained canvas-scaled UI line with width and tint.
 * @details A wide Rect produces a horizontal center line; a tall Rect a
 * vertical center line, matching UIManager::Line(UIRectTransform).
 * Curved corners and borders are not applicable to bare lines.
 */
class UILine : public UIWidgetFluent<UILine> {
public:
    using UIWidgetFluent<UILine>::UIWidgetFluent;
    /** @brief Configure line stroke color.
     * @param color RGBA stroke color.
     * @return This line.
     */
    UILine& SetColor(Color color) { mStroke = color; return *this; }
    /** @brief Configure positive line width.
     * @param width Stroke thickness in UI pixels.
     * @return This line.
     */
    UILine& SetThickness(float width) { mThickness = std::max(0.f, width); return *this; }
protected:
    /** @brief Submit the line through the existing UIManager::Line API.
     * @param ui Active UI manager.
     * @param events Unused event queue; lines are decorative.
     */
    void DrawContent(UIManager& ui, [[maybe_unused]] UIWidgetEvents& events) override {
        if (mLayout) {
            ui.Line(*mLayout, mStroke, mThickness, Parent());
        } else if (mRect.w >= mRect.h) {
            const float middle = mRect.y + mRect.h * 0.5f;
            ui.Line({mRect.x, middle}, {mRect.x + mRect.w, middle}, mStroke, mThickness);
        } else {
            const float middle = mRect.x + mRect.w * 0.5f;
            ui.Line({middle, mRect.y}, {middle, mRect.y + mRect.h}, mStroke, mThickness);
        }
    }
private:
    Color mStroke{255,255,255,255}; ///< Line foreground.
    float mThickness = 1.f; ///< Stroke width.
};

/** @brief Invisible hit target with click/pointer/focus events and no visual styling.
 * @details Useful for custom-drawn UI hotspots. Only geometry, layer,
 * visibility and event properties apply; no invisible border/color setters.
 */
class UIInvisibleButton : public UIWidgetFluent<UIInvisibleButton> {
public:
    using UIWidgetFluent<UIInvisibleButton>::UIWidgetFluent;
    /** @brief Register click completion callback.
     * @param callback Invoked on release inside or UI Submit.
     * @return This invisible button.
     */
    UIInvisibleButton& OnClick(std::function<void()> callback) {
        mOnClick = std::move(callback); return *this;
    }
protected:
    /** @brief Submit only the hit-test region.
     * @param ui Active UI manager.
     * @param events Deferred event queue.
     */
    void DrawContent(UIManager& ui, UIWidgetEvents& events) override {
        const Rect bounds = mLayout ? ui.Resolve(*mLayout, Parent()) : mRect;
        if (ui.InvisibleButton(mRuntimeId.c_str(), bounds) && mOnClick)
            events.push_back(mOnClick);
    }
private:
    std::function<void()> mOnClick; ///< Click completion listener.
};

/** @brief Owns a set of keyed, persistent UI widgets and their canvas scaling.
 * @details UIManager::Begin submits every visible canvas once per frame.
 * Individual widget addresses are stable until Remove, Clear or canvas
 * destruction; never retain pointers across a canvas or scene unload.
 * @ingroup xengine_render
 */
class UIWidgetCanvas {
public:
    /** @brief Construct a named canvas with an initial scaling configuration.
     * @param key Globally unique identifier.
     * @param configuration Canvas scaler copied from UIManager.
     */
    UIWidgetCanvas(std::string key, UICanvas configuration)
        : mKey(std::move(key)), mCanvas(configuration) {}
    /** @brief Get the canvas registry identifier.
     * @return Stable global canvas key.
     */
    const std::string& Key() const noexcept { return mKey; }
    /** @brief Register a new typed element owned by this canvas.
     * @tparam T Concrete UIWidget type (UIButton, UIDropdown, etc.).
     * @tparam Args Geometry constructor parameter types.
     * @param key Unique local identifier (slashes are reserved for lookup paths).
     * @param args Element constructor arguments, normally Rect or UIRectTransform.
     * @return Stable reference to the new concrete widget.
     * @throws std::invalid_argument For empty, invalid or duplicate keys.
     */
    template <typename T, typename... Args>
    T& Create(const std::string& key, Args&&... args) {
        static_assert(std::is_base_of_v<UIWidget, T>, "T must derive from UIWidget");
        if (key.empty() || key.find('/') != std::string::npos || mElements.contains(key))
            throw std::invalid_argument("UIWidgetCanvas::Create: invalid or duplicate key: " + key);
        auto widget = std::make_unique<T>(key, std::forward<Args>(args)...);
        widget->SetCanvasKey(mKey);
        if constexpr (requires(T& object, const UIManager::UIStyle& style) { object.SetStyle(style); })
            widget->SetStyle(UIManager::GetInstance().GetStyle());
        T& reference = *widget;
        mElements.emplace(key, std::move(widget));
        mDrawOrder.push_back(key);
        return reference;
    }
    /** @brief Look up an existing widget by key and required concrete type.
     * @tparam T Expected UIWidget subclass (or UIWidget for untyped access).
     * @param key Canvas-local element key.
     * @return Nonowning pointer or nullptr if absent or of another type.
     */
    template <typename T = UIWidget>
    T* Find(std::string_view key) noexcept {
        static_assert(std::is_base_of_v<UIWidget, T>, "T must derive from UIWidget");
        const auto it = mElements.find(std::string(key));
        return it == mElements.end() ? nullptr : dynamic_cast<T*>(it->second.get());
    }
    /** @brief Look up an existing widget without modifying its canvas.
     * @tparam T Expected UIWidget subclass (or UIWidget for untyped access).
     * @param key Canvas-local element key.
     * @return Const nonowning pointer or nullptr if absent or of another type.
     */
    template <typename T = UIWidget>
    const T* Find(std::string_view key) const noexcept {
        static_assert(std::is_base_of_v<UIWidget, T>, "T must derive from UIWidget");
        const auto it = mElements.find(std::string(key));
        return it == mElements.end() ? nullptr : dynamic_cast<const T*>(it->second.get());
    }
    /** @brief Remove an element and invalidate references to it.
     * @param key Element key.
     * @return True if an element was removed.
     */
    bool Remove(std::string_view key) {
        const std::string name(key);
        if (mElements.erase(name) == 0) return false;
        mDrawOrder.erase(std::remove(mDrawOrder.begin(), mDrawOrder.end(), name), mDrawOrder.end());
        return true;
    }
    /** @brief Remove all elements and invalidate their pointers. */
    void Clear() { mElements.clear(); mDrawOrder.clear(); }
    /** @brief Set canvas-wide visibility, preserving its elements and values.
     * @param visible Whether to submit this canvas.
     * @return This canvas.
     */
    UIWidgetCanvas& SetVisible(bool visible) { mVisible = visible; return *this; }
    /** @brief Return canvas-wide visibility.
     * @return True when this canvas is submitted.
     */
    bool IsVisible() const noexcept { return mVisible; }
    /** @brief Keep the canvas registered across active scene changes.
     * @param persistent Whether it survives scene replacement.
     * @return This canvas.
     */
    UIWidgetCanvas& SetPersistent(bool persistent) { mPersistent = persistent; return *this; }
    /** @brief Return the persistence policy.
     * @return True when it survives scene replacement.
     */
    bool IsPersistent() const noexcept { return mPersistent; }
    /** @brief Assign a per-canvas resolution and anchor scaling configuration.
     * @param canvas Existing UICanvas scaling settings.
     * @return This canvas.
     */
    UIWidgetCanvas& SetScaler(const UICanvas& canvas) { mCanvas = canvas; return *this; }
    /** @brief Get per-canvas scale settings.
     * @return Current UICanvas scaler.
     */
    const UICanvas& GetScaler() const noexcept { return mCanvas; }
    /** @brief Submit all owned widgets in creation order without invoking callbacks yet.
     * @param ui Active immediate-mode UIManager.
     * @param events Deferred callbacks shared across all canvases this frame.
     */
    void Submit(UIManager& ui, UIWidgetEvents& events) {
        if (!mVisible) {
            for (const auto& key : mDrawOrder) {
                const auto it = mElements.find(key);
                if (it != mElements.end()) it->second->Suspend(events);
            }
            return;
        }
        const UICanvas previous = ui.GetCanvas();
        ui.SetCanvas(mCanvas);
        for (const auto& key : mDrawOrder) {
            const auto it = mElements.find(key);
            if (it != mElements.end()) it->second->Submit(ui, events);
        }
        ui.SetCanvas(previous);
    }
private:
    std::string mKey; ///< Registry key.
    UICanvas mCanvas{}; ///< Local resolution scaling settings.
    bool mVisible = true; ///< Canvas visibility.
    bool mPersistent = false; ///< Scene replacement policy.
    std::unordered_map<std::string, std::unique_ptr<UIWidget>> mElements; ///< Owned elements.
    std::vector<std::string> mDrawOrder; ///< Stable insertion order.
};

// API documentation belongs to the declaration in UIManager.h. Doxygen
// merges declaration and definition comments, duplicating @param otherwise.
template <typename T>
T* UIManager::Find(std::string_view path) noexcept {
    const size_t slash = path.find('/');
    if (slash == std::string_view::npos) return nullptr;
    auto* canvas = FindCanvas(path.substr(0, slash));
    return canvas ? canvas->Find<T>(path.substr(slash + 1)) : nullptr;
}

// The const overload is documented at its declaration in UIManager.h.
template <typename T>
const T* UIManager::Find(std::string_view path) const noexcept {
    const size_t slash = path.find('/');
    if (slash == std::string_view::npos) return nullptr;
    const auto* canvas = FindCanvas(path.substr(0, slash));
    return canvas ? canvas->Find<T>(path.substr(slash + 1)) : nullptr;
}

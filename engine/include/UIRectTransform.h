#pragma once
/** @file UIRectTransform.h
 * @brief Resolution-independent UI anchors and optional canvas scaling.
 * @ingroup xengine_render
 */

#include <algorithm>
#include <cmath>

#include "BaseTypes.h"

/** @brief Selects how a UI canvas scales its pixel offsets and element sizes.
 * @ingroup xengine_render
 */
enum class UICanvasScaleMode {
    ConstantPixelSize,   ///< Keep offsets and fixed sizes in drawable pixels.
    ScaleWithScreenSize  ///< Scale offsets and sizes from a reference resolution.
};

/** @brief Canvas settings shared by anchored UI widgets.
 *
 * Anchors always use the current drawable dimensions. This scaler affects
 * anchoredPosition and sizeDelta, not the normalized anchor positions.
 * Anchored widgets scale their rendered text as well as their rectangles;
 * the underlying Font asset is not recreated at a different raster size.
 * @ingroup xengine_render
 */
struct UICanvas {
    /** @brief Determines whether anchored UI sizes use fixed pixels or a reference-resolution scale. */
    UICanvasScaleMode scaleMode = UICanvasScaleMode::ConstantPixelSize;
    /** @brief Reference drawable resolution in pixels for ScaleWithScreenSize. */
    Vec2 referenceResolution{1920.f, 1080.f};
    float matchWidthOrHeight = 0.5f; ///< 0 = width, 1 = height; logarithmic blend.

    /** @brief Returns the pixel scale for the current drawable dimensions. */
    float ScaleFor(Vec2 drawableSize) const noexcept
    {
        if (scaleMode == UICanvasScaleMode::ConstantPixelSize ||
            !std::isfinite(referenceResolution.x) ||
            !std::isfinite(referenceResolution.y) ||
            referenceResolution.x <= 0.f || referenceResolution.y <= 0.f ||
            !std::isfinite(drawableSize.x) || !std::isfinite(drawableSize.y) ||
            drawableSize.x <= 0.f || drawableSize.y <= 0.f)
            return 1.f;

        const float match = std::isfinite(matchWidthOrHeight)
            ? std::clamp(matchWidthOrHeight, 0.f, 1.f) : 0.5f;
        const float widthRatio = drawableSize.x / referenceResolution.x;
        const float heightRatio = drawableSize.y / referenceResolution.y;
        const float scale = std::pow(widthRatio, 1.f - match) * std::pow(heightRatio, match);
        return std::isfinite(scale) && scale > 0.f ? scale : 1.f;
    }
};

/** @brief Unity-style anchor rectangle for XEngine's immediate-mode UI.
 *
 * Anchor coordinates are normalized within the parent: (0,0) is its top-left
 * and (1,1) its bottom-right, following SDL's downward-positive Y axis.
 * For a fixed anchor, sizeDelta is the pixel size; for stretched anchors it
 * is an offset added to the anchor span. anchoredPosition offsets the pivot
 * from the point interpolated between anchorMin and anchorMax by pivot.
 *
 * All offsets are scaled by the canvas scale, while anchor fractions are not.
 * Call Resolve() each frame or pass this object directly to UIManager.
 * @ingroup xengine_render
 */
struct UIRectTransform {
    /** @brief Normalized top-left anchor position within the parent rectangle. */
    Vec2 anchorMin{0.5f, 0.5f};
    /** @brief Normalized bottom-right anchor position within the parent rectangle. */
    Vec2 anchorMax{0.5f, 0.5f};
    /** @brief Normalized pivot of the resolved UI rectangle, measured from its top-left. */
    Vec2 pivot{0.5f, 0.5f};
    /** @brief Pixel offset of the pivot relative to the interpolated anchor reference. */
    Vec2 anchoredPosition{0.f, 0.f};
    /** @brief Pixel size added to the anchor span; equals the size for fixed anchors. */
    Vec2 sizeDelta{100.f, 100.f};

    /** @brief Keeps a fixed-sized element attached to a normalized parent point. */
    static constexpr UIRectTransform Anchored(Vec2 anchor, Vec2 size,
                                              Vec2 offset = {0.f, 0.f},
                                              Vec2 elementPivot = {0.5f, 0.5f}) noexcept
    {
        return {anchor, anchor, elementPivot, offset, size};
    }

    /** @brief Fixed size at a positive inset from the parent's top-left. */
    static constexpr UIRectTransform TopLeft(Vec2 size, Vec2 inset = {0.f, 0.f}) noexcept
    {
        return Anchored({0.f, 0.f}, size, inset, {0.f, 0.f});
    }

    /** @brief Fixed size at a positive inset from the parent's top-right. */
    static constexpr UIRectTransform TopRight(Vec2 size, Vec2 inset = {0.f, 0.f}) noexcept
    {
        return Anchored({1.f, 0.f}, size, {-inset.x, inset.y}, {1.f, 0.f});
    }

    /** @brief Fixed size at a positive inset from the parent's bottom-left. */
    static constexpr UIRectTransform BottomLeft(Vec2 size, Vec2 inset = {0.f, 0.f}) noexcept
    {
        return Anchored({0.f, 1.f}, size, {inset.x, -inset.y}, {0.f, 1.f});
    }

    /** @brief Fixed size at a positive inset from the parent's bottom-right. */
    static constexpr UIRectTransform BottomRight(Vec2 size, Vec2 inset = {0.f, 0.f}) noexcept
    {
        return Anchored({1.f, 1.f}, size, {-inset.x, -inset.y}, {1.f, 1.f});
    }

    /** @brief Fixed-size element centered in its parent. */
    static constexpr UIRectTransform Center(Vec2 size, Vec2 offset = {0.f, 0.f}) noexcept
    {
        return Anchored({0.5f, 0.5f}, size, offset);
    }

    /** @brief Stretches to all four parent edges with positive pixel insets. */
    static constexpr UIRectTransform Stretch(float left, float top,
                                              float right, float bottom) noexcept
    {
        return {{0.f, 0.f}, {1.f, 1.f}, {0.5f, 0.5f},
                {(left - right) * 0.5f, (top - bottom) * 0.5f},
                {-(left + right), -(top + bottom)}};
    }

    /** @brief Resolves a UI rectangle in drawable pixels, relative to a parent rectangle.
     * @param parent Parent bounds in drawable pixels (the canvas for root elements).
     * @param scale Canvas scale for anchoredPosition and sizeDelta.
     */
    Rect Resolve(const Rect& parent, float scale = 1.f) const noexcept
    {
        if (!std::isfinite(parent.x) || !std::isfinite(parent.y) ||
            !std::isfinite(parent.w) || !std::isfinite(parent.h) ||
            parent.w <= 0.f || parent.h <= 0.f)
            return {parent.x, parent.y, 0.f, 0.f};
        if (!std::isfinite(scale) || scale <= 0.f) scale = 1.f;

        const float anchorWidth = (anchorMax.x - anchorMin.x) * parent.w;
        const float anchorHeight = (anchorMax.y - anchorMin.y) * parent.h;
        const float width = std::max(0.f, anchorWidth + sizeDelta.x * scale);
        const float height = std::max(0.f, anchorHeight + sizeDelta.y * scale);
        const float anchorX = parent.x + (anchorMin.x * parent.w) + anchorWidth * pivot.x;
        const float anchorY = parent.y + (anchorMin.y * parent.h) + anchorHeight * pivot.y;
        return {anchorX + anchoredPosition.x * scale - width * pivot.x,
                anchorY + anchoredPosition.y * scale - height * pivot.y,
                width, height};
    }
};

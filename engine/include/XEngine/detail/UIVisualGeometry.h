#pragma once
/** @file UIVisualGeometry.h
 * @brief SDL-independent timing and physical-pixel geometry for editable UI widgets.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "BaseTypes.h"
#include "XEngine/detail/UICornerRadii.h"

/** @brief Internal UI timing and clipped-popup geometry helpers. */
namespace xengine::detail::ui {

/** @brief Duration of the visible and hidden halves of the text caret cycle. */
inline constexpr std::uint32_t kCaretHalfPeriodMs = 500;

/** @brief Determine whether an editing caret is in its visible half-cycle.
 * Unsigned subtraction also handles SDL's 32-bit millisecond tick wraparound.
 * @param now Current monotonic tick in milliseconds.
 * @param reset Tick when editing began or the caret last moved/changed.
 * @return True for the first 500 ms of each 1000 ms interval.
 */
constexpr bool CaretVisible(std::uint32_t now, std::uint32_t reset) noexcept {
    return ((now - reset) % (2u * kCaretHalfPeriodMs)) < kCaretHalfPeriodMs;
}

/** @brief Physical-pixel area of one dropdown highlight within its rounded popup.
 * The highlight draws the *whole* rounded interior, clipped to this row. The
 * shared silhouette prevents rounded inner row corners, border overlap and
 * gaps even when a large percentage radius extends across several rows.
 */
struct DropdownHighlightClip {
    Rect shape{}; ///< Inset rounded interior of the popup, in drawable pixels.
    Rect clip{};  ///< Pixel-aligned rectangular clip for just this row.
    float radius = 0.f; ///< Interior radius after accounting for the outline.
    CornerRadii radii{}; ///< Four interior radii after border inset and fitting.
    bool valid = false; ///< Whether the intersection has positive area.
};

/** @brief Resolve the shared popup silhouette and one row's clip region.
 * @param popup Popup rectangle in physical drawable coordinates.
 * @param rowHeight Height of one visible row in drawable pixels.
 * @param rowIndex Zero-based row position *within the visible popup*.
 * @param rowCount Number of visible rows, independent of scroll position.
 * @param outerRadii Already resolved per-corner style radii in physical pixels.
 * @param borderWidth Style outline width in physical pixels.
 * @return Snapped silhouette and clip; invalid when the geometry is empty.
 */
inline DropdownHighlightClip DropdownRowClip(const Rect& popup, float rowHeight,
                                            int rowIndex, int rowCount,
                                            CornerRadii outerRadii, float borderWidth) noexcept {
    DropdownHighlightClip result{};
    if (rowCount <= 0 || rowIndex < 0 || rowIndex >= rowCount ||
        !std::isfinite(popup.x) || !std::isfinite(popup.y) ||
        !std::isfinite(popup.w) || !std::isfinite(popup.h) ||
        !std::isfinite(rowHeight) || rowHeight <= 0.f ||
        popup.w <= 0.f || popup.h <= 0.f) return result;

    const float left = std::round(popup.x);
    const float top = std::round(popup.y);
    const float right = std::max(left + 1.f, std::round(popup.x + popup.w));
    const float bottom = std::max(top + 1.f, std::round(popup.y + popup.h));
    const float halfSide = std::min(right - left, bottom - top) * 0.5f;
    const float stroke = std::isfinite(borderWidth) && borderWidth > 0.f
        ? std::clamp(std::max(1.f, std::round(borderWidth)), 0.f, halfSide) : 0.f;
    result.shape = {left + stroke, top + stroke,
                    right - left - 2.f * stroke, bottom - top - 2.f * stroke};
    if (result.shape.w <= 0.f || result.shape.h <= 0.f) return result;
    outerRadii = FitCornerRadii(outerRadii, right - left, bottom - top);
    result.radii = FitCornerRadii(OffsetCornerRadii(outerRadii, -stroke),
                                  result.shape.w, result.shape.h);
    result.radius = result.radii.topLeft;

    const float rowTop = std::round(popup.y + rowHeight * rowIndex);
    const float rowBottom = std::round(popup.y + rowHeight * (rowIndex + 1));
    const float clipTop = std::max(result.shape.y, rowTop);
    const float clipBottom = std::min(result.shape.y + result.shape.h, rowBottom);
    result.clip = {result.shape.x, clipTop, result.shape.w,
                   std::max(0.f, clipBottom - clipTop)};
    result.valid = result.clip.h > 0.f;
    return result;
}

/** @brief Compatibility overload for a popup with the same radius on all corners.
 * @param popup Popup rectangle in physical drawable coordinates.
 * @param rowHeight Height of one visible row in drawable pixels.
 * @param rowIndex Zero-based row position within the visible popup.
 * @param rowCount Number of visible rows.
 * @param outerRadius Uniform outer radius in drawable pixels.
 * @param borderWidth Border width in drawable pixels.
 * @return Pixel-aligned shared popup silhouette and row clip.
 */
inline DropdownHighlightClip DropdownRowClip(const Rect& popup, float rowHeight,
                                            int rowIndex, int rowCount,
                                            float outerRadius, float borderWidth) noexcept {
    return DropdownRowClip(popup, rowHeight, rowIndex, rowCount,
                           UniformRadii(outerRadius), borderWidth);
}

} // namespace xengine::detail::ui

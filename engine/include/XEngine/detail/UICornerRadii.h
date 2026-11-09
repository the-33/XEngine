#pragma once
/** @file UICornerRadii.h
 * @brief Independent UI corner radii and side-length normalization without SDL dependencies.
 */

#include <algorithm>
#include <cmath>
#include "BaseTypes.h"

/** @brief Private UI corner geometry shared by immediate and recorded renderers. */
namespace xengine::detail::ui {

/** @brief Four radii in clockwise order, starting at the upper-left corner. */
struct CornerRadii {
    float topLeft = 0.f; ///< Upper-left radius in physical pixels.
    float topRight = 0.f; ///< Upper-right radius in physical pixels.
    float bottomRight = 0.f; ///< Lower-right radius in physical pixels.
    float bottomLeft = 0.f; ///< Lower-left radius in physical pixels.
};

/** @brief Construct four matching radii.
 * @param radius Radius shared by all corners in pixels.
 * @return Four identical radii.
 */
constexpr CornerRadii UniformRadii(float radius) noexcept {
    return {radius, radius, radius, radius};
}

/** @brief Ensure that adjacent corner arcs fit their shared rectangle sides.
 * Follows the CSS-style proportional reduction rule rather than clamping each
 * corner independently, preserving relative proportions for uneven radii.
 * @param radii Requested physical radii.
 * @param width Width of the rectangle in physical pixels.
 * @param height Height of the rectangle in physical pixels.
 * @return Nonnegative radii that never overlap along a side.
 */
inline CornerRadii FitCornerRadii(CornerRadii radii, float width, float height) noexcept {
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.f || height <= 0.f)
        return {};
    const auto safe = [](float value) noexcept {
        return std::isfinite(value) ? std::max(0.f, value) : 0.f;
    };
    radii.topLeft = safe(radii.topLeft);
    radii.topRight = safe(radii.topRight);
    radii.bottomRight = safe(radii.bottomRight);
    radii.bottomLeft = safe(radii.bottomLeft);
    float factor = 1.f;
    const auto fitSide = [&factor](float available, float first, float second) noexcept {
        if (first + second > available)
            factor = std::min(factor, available / (first + second));
    };
    fitSide(width, radii.topLeft, radii.topRight);
    fitSide(width, radii.bottomLeft, radii.bottomRight);
    fitSide(height, radii.topLeft, radii.bottomLeft);
    fitSide(height, radii.topRight, radii.bottomRight);
    radii.topLeft *= factor;
    radii.topRight *= factor;
    radii.bottomRight *= factor;
    radii.bottomLeft *= factor;
    return radii;
}

/** @brief Offset each fitted radius for a concentric outline or inset fill.
 * @param radii Outer corner radii.
 * @param delta Added radius, negative for an inset contour.
 * @return Four nonnegative offset radii.
 */
inline CornerRadii OffsetCornerRadii(CornerRadii radii, float delta) noexcept {
    radii.topLeft = std::max(0.f, radii.topLeft + delta);
    radii.topRight = std::max(0.f, radii.topRight + delta);
    radii.bottomRight = std::max(0.f, radii.bottomRight + delta);
    radii.bottomLeft = std::max(0.f, radii.bottomLeft + delta);
    return radii;
}

/** @brief Test whether at least one corner has a positive radius.
 * @param radii Corner radii to inspect.
 * @return True when any of the four corners is rounded.
 */
constexpr bool HasRoundedCorner(const CornerRadii& radii) noexcept {
    return radii.topLeft > 0.f || radii.topRight > 0.f ||
           radii.bottomRight > 0.f || radii.bottomLeft > 0.f;
}

} // namespace xengine::detail::ui

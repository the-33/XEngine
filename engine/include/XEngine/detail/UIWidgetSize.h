#pragma once
/** @file UIWidgetSize.h
 * @brief Minimum screen-space bounds for canvas-scaled immediate-mode widgets.
 */

#include <algorithm>
#include <cmath>

namespace xengine::detail::ui
{
    /** @brief Whether a control has enough physical pixels to draw and hit-test.
     * @details UIManager's legacy minimums are expressed in reference-canvas
     * pixels. Comparing them directly to scaled screen coordinates hides
     * dropdowns and toggles at small drawable sizes. Preserve those minimums
     * at 1:1 scale while allowing proportional downscaling, with a two-pixel
     * absolute floor to reject degenerate bounds.
     * @param width Requested width in the function's coordinate units.
     * @param height Requested height in the function's coordinate units.
     * @param canvasScale Scale from reference UI units to drawable screen pixels.
     * @param minReferenceWidth Minimum control width in reference UI units.
     * @param minReferenceHeight Minimum control height in reference UI units.
     * @return True if a control has enough physical pixels to draw and hit-test; otherwise false.
     */
    inline bool ControlHasRoom(float width, float height, float canvasScale,
                               float minReferenceWidth, float minReferenceHeight) noexcept
    {
        if (!std::isfinite(width) || !std::isfinite(height) ||
            width <= 0.f || height <= 0.f) return false;
        const float scale = std::isfinite(canvasScale) && canvasScale > 0.f
            ? canvasScale : 1.f;
        return width > std::max(2.f, minReferenceWidth * scale) &&
               height > std::max(2.f, minReferenceHeight * scale);
    }
}

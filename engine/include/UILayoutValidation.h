#pragma once
/** @file UILayoutValidation.h
 * @brief Explicit, exception-based preflight checks for resize-sensitive UI.
 */

#include "BaseTypes.h"
#include "XEngine/detail/UIWidgetSize.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

/** @brief A required UI element cannot be displayed inside the requested canvas.
 * @details Games may catch this exception around a resize transaction, restore
 * the previous window/render resolution and show a nonfatal settings warning.
 * The normal immediate-mode draw methods remain noexcept; only explicitly
 * requested preflight validation throws.
 * @ingroup xengine_render
 */
class UILayoutException final : public std::runtime_error {
public:
    /** @brief Available reason values used by UILayoutException.
     */
    enum class Reason {
        InvalidCanvas, ///< Drawable canvas has invalid or unsupported dimensions.
        TooSmall,      ///< The element cannot meet its minimum visible size.
        Clipped        ///< Some of the element falls outside the drawable canvas.
    };

    /** @brief Construct a UILayoutException instance.
     * @param element Element supplied to UILayoutException.
     * @param reason Specific category identifying the failure.
     * @param requested Requested viewport dimensions in drawable pixels.
     */
    UILayoutException(std::string element, Reason reason, Vec2I requested)
        : std::runtime_error("UI element '" + element + "' cannot fit in " +
                             std::to_string(requested.x) + "x" +
                             std::to_string(requested.y) + " drawable pixels"),
          mElement(std::move(element)), mReason(reason), mRequested(requested) {}

    /** @brief Element id in UILayoutException.
     * @return Result of element id in UILayoutException.
     */
    const std::string& ElementId() const noexcept { return mElement; }
    /** @brief Failure reason in UILayoutException.
     * @return Result of failure reason in UILayoutException.
     */
    Reason FailureReason() const noexcept { return mReason; }
    /** @brief Requested drawable size in UILayoutException.
     * @return Result of requested drawable size in UILayoutException.
     */
    Vec2I RequestedDrawableSize() const noexcept { return mRequested; }

private:
    /** @brief Stored element value used by UILayoutException.
     */
    std::string mElement;
    /** @brief Stored reason value used by UILayoutException.
     */
    Reason mReason;
    /** @brief Stored requested value used by UILayoutException.
     */
    Vec2I mRequested;
};

/** @brief Stateless validation of caller-declared UI rectangles before resize.
 * @details All required elements must be submitted by the game. This API does
 * not guess the layout of elements the game has not created yet, and does not
 * change the window or intercept draw calls automatically. The visibility
 * threshold uses the same snapped physical pixels as rounded UI rendering,
 * allowing a positive fractional extent that rasterizes to one pixel.
 * @ingroup xengine_render
 */
namespace UILayoutValidation {
    /** @brief Require visible in UILayoutValidation.
     * @param elementId Stable identifier used to identify a failing UI element.
     * @param bounds Bounding rectangle of the object or region.
     * @param canvas Drawable viewport against which the element is validated or resolved.
     * @param minWidth Minimum required width in drawable pixels.
     * @param minHeight Minimum required height in drawable pixels.
     */
    inline void RequireVisible(const char* elementId, Rect bounds,
                               Rect canvas, float minWidth = 1.f,
                               float minHeight = 1.f)
    {
        const std::string element = elementId && *elementId ? elementId : "<unnamed>";
        const auto dimension = [](float pixels) noexcept {
            return std::isfinite(pixels) && pixels > 0.f && pixels <= 16384.f
                ? static_cast<int>(pixels) : 0;
        };
        const Vec2I requested{dimension(canvas.w), dimension(canvas.h)};
        if (!std::isfinite(canvas.x) || !std::isfinite(canvas.y) ||
            !std::isfinite(canvas.w) || !std::isfinite(canvas.h) ||
            canvas.w <= 0.f || canvas.h <= 0.f ||
            canvas.w > 16384.f || canvas.h > 16384.f)
            throw UILayoutException(element, UILayoutException::Reason::InvalidCanvas,
                                    requested);
        if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
            !std::isfinite(bounds.w) || !std::isfinite(bounds.h) ||
            bounds.w <= 0.f || bounds.h <= 0.f)
            throw UILayoutException(element, UILayoutException::Reason::TooSmall,
                                    requested);

        // Match RenderManager::DrawRoundedRectScreenImmediate_: UI shapes snap
        // both edges to physical pixels and preserve at least one pixel on
        // each axis. Testing pre-snap fractions would falsely reject visible
        // 0.75px dividers, and tolerating raw edges could accept clipped ones.
        const float left = std::round(bounds.x);
        const float top = std::round(bounds.y);
        const float right = std::max(left + 1.f,
                                     std::round(bounds.x + bounds.w));
        const float bottom = std::max(top + 1.f,
                                      std::round(bounds.y + bounds.h));
        if (right - left < std::max(1.f, minWidth) ||
            bottom - top < std::max(1.f, minHeight))
            throw UILayoutException(element, UILayoutException::Reason::TooSmall,
                                    requested);

        const float canvasLeft = std::round(canvas.x);
        const float canvasTop = std::round(canvas.y);
        const float canvasRight = std::round(canvas.x + canvas.w);
        const float canvasBottom = std::round(canvas.y + canvas.h);
        if (left < canvasLeft || top < canvasTop ||
            right > canvasRight || bottom > canvasBottom)
            throw UILayoutException(element, UILayoutException::Reason::Clipped,
                                    requested);
    }

    /** @brief Require control visible in UILayoutValidation.
     * @param elementId Stable identifier used to identify a failing UI element.
     * @param bounds Bounding rectangle of the object or region.
     * @param canvas Drawable viewport against which the element is validated or resolved.
     * @param canvasScale Scale from reference UI units to drawable screen pixels.
     * @param minReferenceWidth Minimum control width in reference UI units.
     * @param minReferenceHeight Minimum control height in reference UI units.
     */
    inline void RequireControlVisible(const char* elementId, Rect bounds,
                                      Rect canvas, float canvasScale,
                                      float minReferenceWidth,
                                      float minReferenceHeight)
    {
        RequireVisible(elementId, bounds, canvas);
        if (!xengine::detail::ui::ControlHasRoom(
                bounds.w, bounds.h, canvasScale,
                minReferenceWidth, minReferenceHeight))
            throw UILayoutException(elementId ? elementId : "<unnamed>",
                                    UILayoutException::Reason::TooSmall,
                                    {static_cast<int>(canvas.w),
                                     static_cast<int>(canvas.h)});
    }
}

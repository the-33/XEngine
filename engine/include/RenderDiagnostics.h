#pragma once
/** @file RenderDiagnostics.h
 * @brief Public renderer diagnostics and CPU preparation metrics.
 */
// Deterministic adaptive-render selection, independent of SDL and testable on its own.
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>

/** @namespace RenderDiagnostics
 * @brief Public, SDL-independent workload estimates and adaptive-render selection helpers.
 * @ingroup xengine_render
 * @note The values drive performance heuristics, not game-save compatibility.
 */
namespace RenderDiagnostics {
/** @brief Default minimum number of eligible commands before parallel preparation is selected.
 */
inline constexpr std::size_t kParallelThreshold = 1536;
/** @brief Available path modes for RenderDiagnostics.
 */
enum class Path : std::uint8_t {
    Disabled = 0,       ///< Parallel preparation is disabled by settings.
    NoWorkers = 1,      ///< Parallel preparation was requested but no workers exist.
    BelowThreshold = 2, ///< Previous-frame workload is too small to parallelize.
    Parallel = 3,       ///< Parallel preparation is selected.
    ForcedImmediate = 4 ///< Benchmark-only forced immediate path; distinct from below threshold.
};
/** @brief Converts a diagnostic enum value into a stable, human-readable identifier.
 * @param path Diagnostic route value to convert into a readable identifier.
 */
inline constexpr const char* Name(Path path) noexcept {
    switch (path) {
        case Path::Disabled: return "disabled";
        case Path::NoWorkers: return "no_workers";
        case Path::BelowThreshold: return "below_threshold";
        case Path::Parallel: return "parallel";
        case Path::ForcedImmediate: return "forced_immediate";
    }
    return "unknown";
}
/** @brief Selects a serial or parallel rendering-preparation route from last-frame workload statistics.
 * @param threading Whether multithreaded execution is enabled.
 * @param requested Whether the user requested parallel processing.
 * @param workers Number of available preparation worker threads.
 * @param previousEligible Number of eligible commands measured in the previous frame.
 * @param threshold Cutoff that activates or deactivates the selection policy.
 */
inline constexpr Path Select(bool threading, bool requested, unsigned workers,
                             std::size_t previousEligible,
                             std::size_t threshold = kParallelThreshold) noexcept {
    if (!threading || !requested) return Path::Disabled;
    if (!workers) return Path::NoWorkers;
    if (previousEligible < threshold) return Path::BelowThreshold;
    return Path::Parallel;
}
// The adaptive policy estimates worker-preparable CPU work, not GPU draw cost.
// All values use integer fixed-point (256 units = one plain world command).
// Weights are preliminary and must be calibrated against actual frame timings.
/** @brief Fixed-point weight of one plain world command (256 units).
 */
inline constexpr std::uint64_t kUnit = 256;
/** @brief Additional weight assigned to a three-axis sprite tilt.
 */
inline constexpr std::uint64_t kXYZTiltExtra = 192;
/** @brief Additional weight assigned to a rotated sprite.
 */
inline constexpr std::uint64_t kRotationExtra = 64;
/** @brief Additional weight assigned to a custom sprite offset.
 */
inline constexpr std::uint64_t kOffsetExtra = 64;
/** @brief Hysteresis exit threshold as a percentage of the entry threshold.
 */
inline constexpr std::uint64_t kExitPercent = 85; // 15% hysteresis below entry
// Offscreen geometry still requires projection/culling CPU, but does not
// generate an SDL texture submission. Experimental 25% preparation weight.
/** @brief Weight percentage retained for projected sprites known to be offscreen.
 */
inline constexpr std::uint64_t kOffscreenPercent = 25;

/** @brief World-space viewport center and dimensions used for visibility estimates.
 */
struct View {
    /** @brief X coordinate of the viewport center. Y coordinate of the viewport center. Viewport width in world units. Viewport height in world units.
     */
    float centerX = 0.f, centerY = 0.f, width = 0.f, height = 0.f;
};
// Compute viewport edges once per frame, not for every submitted sprite.
/** @brief Cached viewport edges, prevalidated for conservative culling estimates.
 */
struct ViewBounds {
    /** @brief Cached left viewport edge. Cached right viewport edge. Cached top viewport edge. Cached bottom viewport edge.
     */
    float left = 0.f, right = 0.f, top = 0.f, bottom = 0.f;
    /** @brief True if the viewport is finite with positive dimensions.
     */
    bool valid = false;
};
/** @brief Builds finite viewport edges from a camera center and extents.
 * @param v Value, vector or input to this operation.
 */
inline ViewBounds MakeViewBounds(const View& v) noexcept {
    if (!(std::isfinite(v.centerX) && std::isfinite(v.centerY) &&
          std::isfinite(v.width) && std::isfinite(v.height) &&
          v.width > 0.f && v.height > 0.f)) return {};
    return {v.centerX - v.width * .5f, v.centerX + v.width * .5f,
            v.centerY - v.height * .5f, v.centerY + v.height * .5f, true};
}
// Strictly conservative: a square enclosing every possible X/Y projection,
// Z rotation, scaled local offset and custom pivot. This never controls actual
// rendering/culling; it only changes the NEXT frame's selector estimate.
// Invalid/nonfinite geometry is conservatively considered potentially visible.
/** @brief Conservatively checks whether sprite geometry cannot intersect the viewport.
 * @param v Value, vector or input to this operation.
 * @param x Horizontal coordinate or component.
 * @param y Vertical coordinate or component.
 * @param w Width or horizontal extent.
 * @param h Height or vertical extent.
 * @param scaleX Horizontal sprite scale.
 * @param scaleY Vertical sprite scale.
 * @param offsetX Horizontal sprite local offset.
 * @param offsetY Vertical sprite local offset.
 * @param pivotX Horizontal normalized rotation pivot.
 * @param pivotY Vertical normalized rotation pivot.
 */
inline bool DefinitelyOffscreenSprite(const ViewBounds& v,
    float x, float y, float w, float h, float scaleX, float scaleY,
    float offsetX, float offsetY, float pivotX, float pivotY) noexcept {
    if (!v.valid || !std::isfinite(x) || !std::isfinite(y)) return false;
    // With nonnegative extent, an anchor inside both viewport axes cannot
    // satisfy any offscreen inequality. Common visible sprites avoid all
    // per-sprite width/scale/pivot/offset calculations.
    if (x >= v.left && x <= v.right && y >= v.top && y <= v.bottom)
        return false;
    if (!(std::isfinite(w) &&
          std::isfinite(h) && std::isfinite(scaleX) && std::isfinite(scaleY) &&
          std::isfinite(offsetX) && std::isfinite(offsetY) &&
          std::isfinite(pivotX) && std::isfinite(pivotY))) return false;
    // L1-bound is deliberately larger than the circumscribed circle. It also
    // encloses arbitrary pivots outside [0,1] and all rotations Z.
    // Most untransformed benchmark sprites (notably culling) have a centered
    // pivot and zero local offset. The expression below is algebraically and
    // floating-point equivalent to the general bound for those exact values,
    // but avoids four fabs, two subtractions and two max operations.
    // Keep the original conservative expression for all custom geometry.
    const float extent = (pivotX == .5f && pivotY == .5f &&
                          offsetX == 0.f && offsetY == 0.f)
        ? (std::fabs(w * scaleX) * .5f + std::fabs(h * scaleY) * .5f)
        : (std::fabs(w * scaleX) *
               std::max(std::fabs(pivotX), std::fabs(1.f - pivotX)) +
           std::fabs(h * scaleY) *
               std::max(std::fabs(pivotY), std::fabs(1.f - pivotY)) +
           std::fabs(offsetX * scaleX) + std::fabs(offsetY * scaleY));
    if (!std::isfinite(extent)) return false;
    return x + extent < v.left || x - extent > v.right ||
           y + extent < v.top || y - extent > v.bottom;
}
/** @brief Conservatively checks whether sprite geometry cannot intersect the viewport.
 * @param v Value, vector or input to this operation.
 * @param x Horizontal coordinate or component.
 * @param y Vertical coordinate or component.
 * @param w Width or horizontal extent.
 * @param h Height or vertical extent.
 * @param scaleX Horizontal sprite scale.
 * @param scaleY Vertical sprite scale.
 * @param offsetX Horizontal sprite local offset.
 * @param offsetY Vertical sprite local offset.
 * @param pivotX Horizontal normalized rotation pivot.
 * @param pivotY Vertical normalized rotation pivot.
 */
inline bool DefinitelyOffscreenSprite(const View& v,
    float x, float y, float w, float h, float scaleX, float scaleY,
    float offsetX, float offsetY, float pivotX, float pivotY) noexcept {
    return DefinitelyOffscreenSprite(MakeViewBounds(v), x, y, w, h,
                                     scaleX, scaleY, offsetX, offsetY, pivotX, pivotY);
}
/** @brief Calculates estimated effective rendering work for the specified draw call.
 * @param units Estimated number of rendering-work units.
 * @param definitelyOffscreen Whether the geometry is known to be outside the current view.
 */
inline constexpr std::uint64_t EffectiveUnits(std::uint64_t units,
                                              bool definitelyOffscreen) noexcept {
    return definitelyOffscreen ? units * kOffscreenPercent / 100 : units;
}
// Only the comparison against the entry/exit boundaries matters. After
// reaching the higher entry boundary, more exact visibility scores cannot
// change either decision (including after a hysteresis transition).
/** @brief Returns the threshold at which the selected rendering route may change.
 * @param threshold Cutoff that activates or deactivates the selection policy.
 */
inline constexpr std::uint64_t EntryBoundary(std::size_t threshold) noexcept {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    return threshold > maximum / kUnit ? maximum :
           static_cast<std::uint64_t>(threshold) * kUnit;
}
/** @brief Accumulates estimated rendering units subject to the configured cap.
 * @param current Current value before the requested transition.
 * @param next Candidate state or value for this operation.
 * @param boundary Threshold boundary for the strategy decision.
 */
inline constexpr std::uint64_t CappedEffectiveSum(std::uint64_t current,
    std::uint64_t next, std::uint64_t boundary) noexcept {
    return current >= boundary ? boundary :
           current + std::min(next, boundary - current);
}

/** @brief Available selection modes for RenderDiagnostics.
 */
enum class Selection : std::uint8_t {
    Fixed,    ///< Use the fixed eligible-command threshold.
    Adaptive  ///< Use weighted work estimates and hysteresis.
};
/** @brief Converts a diagnostic enum value into a stable, human-readable identifier.
 * @param mode Selected operation, input or render mode.
 */
inline constexpr const char* Name(Selection mode) noexcept {
    return mode == Selection::Adaptive ? "adaptive" : "fixed";
}
// Benchmark-only ablation controls. Auto preserves production behavior:
// fixed scores weighted units; adaptive also estimates offscreen sprites.
// No graphics, physics or runtime configuration files depend on these enums.
/** @brief Available work estimate modes for RenderDiagnostics.
 */
enum class WorkEstimate : std::uint8_t {
    Off = 0,        ///< Disable additional work weighting for benchmark comparison.
    Weighted = 1,   ///< Weight the CPU preparation cost of command kinds.
    Visibility = 2, ///< Also estimate conservative offscreen visibility.
    Auto = 3        ///< Let the active policy choose an estimation mode.
};
/** @brief Available route override modes for RenderDiagnostics.
 */
enum class RouteOverride : std::uint8_t {
    Auto = 0,     ///< Use the engine's normal render-route selection.
    Immediate = 1, ///< Force immediate rendering for benchmark comparisons.
    Recorded = 2 ///< Force the recorded-command rendering path.
};
/** @brief Estimates rendering work attributable to a sprite.
 * @param tilted Whether the geometry has a non-axis-aligned orientation.
 * @param rotated Whether to include rotation in the conversion.
 * @param offset Offset applied to the current position or geometry.
 */
inline constexpr std::uint64_t SpriteUnits(bool tilted, bool rotated, bool offset) noexcept {
    return kUnit + (tilted ? kXYZTiltExtra : 0) +
           (rotated ? kRotationExtra : 0) + (offset ? kOffsetExtra : 0);
}
/** @brief Estimates rendering work attributable to a rectangle.
 * @param rotated Whether to include rotation in the conversion.
 */
inline constexpr std::uint64_t RectangleUnits(bool rotated) noexcept {
    return kUnit + (rotated ? kRotationExtra : 0);
}
/** @brief Chooses a rendering strategy from workload measurements and configured thresholds.
 * @param threading Whether multithreaded execution is enabled.
 * @param requested Whether the user requested parallel processing.
 * @param workers Number of available preparation worker threads.
 * @param previousUnits Work-unit estimate from the preceding frame.
 * @param threshold Cutoff that activates or deactivates the selection policy.
 * @param previouslyParallel Whether the previous renderer route used parallel work.
 */
inline constexpr Path SelectAdaptive(bool threading, bool requested, unsigned workers,
                                    std::uint64_t previousUnits, std::size_t threshold,
                                    bool previouslyParallel) noexcept {
    if (!threading || !requested) return Path::Disabled;
    if (!workers) return Path::NoWorkers;
    const auto boundary = EntryBoundary(threshold);
    const auto exitBoundary = boundary / 100 * kExitPercent +
                              (boundary % 100) * kExitPercent / 100;
    return previousUnits >= (previouslyParallel ? exitBoundary : boundary)
        ? Path::Parallel : Path::BelowThreshold;
}
} // namespace RenderDiagnostics

/** @var float RenderDiagnostics::View::centerY
 * @brief Vertical center of the camera view in world units.
 */
/** @var float RenderDiagnostics::View::width
 * @brief Positive world-space width of the camera view.
 */
/** @var float RenderDiagnostics::View::height
 * @brief Positive world-space height of the camera view.
 */
/** @var float RenderDiagnostics::ViewBounds::right
 * @brief Right edge of the viewport in world coordinates.
 */
/** @var float RenderDiagnostics::ViewBounds::top
 * @brief Top edge of the viewport in world coordinates.
 */
/** @var float RenderDiagnostics::ViewBounds::bottom
 * @brief Bottom edge of the viewport in world coordinates.
 */

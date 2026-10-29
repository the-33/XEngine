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
/** @brief Reason/route chosen for CPU-side render preparation this frame.
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
 * @param threshold Eligible-command count required for parallel work.
 * @return `Disabled`/`NoWorkers` if parallelism is unavailable; otherwise
 *         `Parallel` at or above the fixed threshold, `BelowThreshold` below it.
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
    float centerX = 0.f; ///< Horizontal center in world units.
    float centerY = 0.f; ///< Vertical center in world units.
    float width = 0.f;   ///< Visible world-space width, before the bounding check.
    float height = 0.f;  ///< Visible world-space height, before the bounding check.
};
// Compute viewport edges once per frame, not for every submitted sprite.
/** @brief Cached viewport edges, prevalidated for conservative culling estimates.
 */
struct ViewBounds {
    float left = 0.f;   ///< Minimum visible world X.
    float right = 0.f;  ///< Maximum visible world X.
    float top = 0.f;    ///< Minimum visible world Y.
    float bottom = 0.f; ///< Maximum visible world Y.
    /** @brief True if the viewport is finite with positive dimensions.
     */
    bool valid = false;
};
/** @brief Validates a world-space view and precomputes its four edges.
 * @param v Camera center and positive world-space dimensions.
 * @return Valid bounding edges, or an invalid zero-initialized result when
 *         any input is nonfinite or a dimension is nonpositive.
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
/** @brief Conservatively rejects sprites known to be entirely outside a valid view.
 * @details Tests a deliberately generous rotational bound. A false result does
 * **not** mean the sprite is visible; it merely cannot be proven offscreen.
 * Used for next-frame work estimation, not actual draw-call culling.
 * @param v Validated viewport bounds; invalid bounds never reject a sprite.
 * @param x Sprite anchor X in world units.
 * @param y Sprite anchor Y in world units.
 * @param w Sprite width before scale.
 * @param h Sprite height before scale.
 * @param scaleX Horizontal sprite scale.
 * @param scaleY Vertical sprite scale.
 * @param offsetX Horizontal local offset before scale.
 * @param offsetY Vertical local offset before scale.
 * @param pivotX Normalized horizontal pivot (values outside [0,1] are allowed).
 * @param pivotY Normalized vertical pivot (values outside [0,1] are allowed).
 * @return True only when the conservative bound lies completely outside.
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
/** @brief DefinitelyOffscreenSprite overload accepting camera dimensions.
 * @param v World-space viewport, validated by MakeViewBounds().
 * @param x Sprite anchor X in world units.
 * @param y Sprite anchor Y in world units.
 * @param w Sprite width before scale.
 * @param h Sprite height before scale.
 * @param scaleX Horizontal sprite scale.
 * @param scaleY Vertical sprite scale.
 * @param offsetX Horizontal local offset before scale.
 * @param offsetY Vertical local offset before scale.
 * @param pivotX Normalized horizontal rotation pivot.
 * @param pivotY Normalized vertical rotation pivot.
 * @return True only when the sprite is definitely outside the view.
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
/** @brief Converts a command-count threshold into fixed-point work units.
 * @param threshold Number of baseline world commands (each worth kUnit).
 * @return Saturates at uint64_t maximum on multiplication overflow.
 */
inline constexpr std::uint64_t EntryBoundary(std::size_t threshold) noexcept {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    return threshold > maximum / kUnit ? maximum :
           static_cast<std::uint64_t>(threshold) * kUnit;
}
/** @brief Adds estimated work units, saturating at a decision boundary.
 * @param current Running accumulated work units.
 * @param next Units contributed by the next candidate command.
 * @param boundary Maximum units that affect the selection decision.
 */
inline constexpr std::uint64_t CappedEffectiveSum(std::uint64_t current,
    std::uint64_t next, std::uint64_t boundary) noexcept {
    return current >= boundary ? boundary :
           current + std::min(next, boundary - current);
}

/** @brief Fixed command-count or adaptive weighted-work selection policy.
 */
enum class Selection : std::uint8_t {
    Fixed,    ///< Use the fixed eligible-command threshold.
    Adaptive  ///< Use weighted work estimates and hysteresis.
};
/** @brief Converts a diagnostic enum value into a stable, human-readable identifier.
 * @param mode Selection policy whose stable text label is requested.
 */
inline constexpr const char* Name(Selection mode) noexcept {
    return mode == Selection::Adaptive ? "adaptive" : "fixed";
}
// Benchmark-only ablation controls. Auto preserves production behavior:
// fixed scores weighted units; adaptive also estimates offscreen sprites.
// No graphics, physics or runtime configuration files depend on these enums.
/** @brief Benchmark control over CPU work estimation precision.
 */
enum class WorkEstimate : std::uint8_t {
    Off = 0,        ///< Disable additional work weighting for benchmark comparison.
    Weighted = 1,   ///< Weight the CPU preparation cost of command kinds.
    Visibility = 2, ///< Also estimate conservative offscreen visibility.
    Auto = 3        ///< Let the active policy choose an estimation mode.
};
/** @brief Benchmark override of the renderer command submission route.
 */
enum class RouteOverride : std::uint8_t {
    Auto = 0,     ///< Use the engine's normal render-route selection.
    Immediate = 1, ///< Force immediate rendering for benchmark comparisons.
    Recorded = 2 ///< Force the recorded-command rendering path.
};
/** @brief Estimates rendering work attributable to a sprite.
 * @param tilted Whether an X/Y sprite tilt requires extra preparation.
 * @param rotated Whether the sprite has a nonzero Z rotation.
 * @param offset Whether the sprite has a non-default local offset.
 */
inline constexpr std::uint64_t SpriteUnits(bool tilted, bool rotated, bool offset) noexcept {
    return kUnit + (tilted ? kXYZTiltExtra : 0) +
           (rotated ? kRotationExtra : 0) + (offset ? kOffsetExtra : 0);
}
/** @brief Estimates rendering work attributable to a rectangle.
 * @param rotated Whether the rectangle requires rotation preparation.
 */
inline constexpr std::uint64_t RectangleUnits(bool rotated) noexcept {
    return kUnit + (rotated ? kRotationExtra : 0);
}
/** @brief Chooses a rendering strategy from workload measurements and configured thresholds.
 * @param threading Whether multithreaded execution is enabled.
 * @param requested Whether the user requested parallel processing.
 * @param workers Number of available preparation worker threads.
 * @param previousUnits Work-unit estimate from the preceding frame.
 * @param threshold Equivalent number of baseline world commands required to
 *                  enter parallel mode; converted to work units internally.
 * @param previouslyParallel Whether the previous renderer route used parallel work.
 * @return Uses the full entry boundary when starting parallel work and an
 *         85%-of-entry exit boundary when already parallel (hysteresis).
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

#pragma once
// Deterministic adaptive-render selection, independent of SDL and testable on its own.
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>

namespace RenderDiagnostics {
inline constexpr std::size_t kParallelThreshold = 1536;
enum class Path : std::uint8_t {
    Disabled = 0, NoWorkers = 1, BelowThreshold = 2, Parallel = 3,
    ForcedImmediate = 4 // benchmark-only; do not mislabel a forced path as below-threshold
};
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
inline constexpr std::uint64_t kUnit = 256;
inline constexpr std::uint64_t kXYZTiltExtra = 192;
inline constexpr std::uint64_t kRotationExtra = 64;
inline constexpr std::uint64_t kOffsetExtra = 64;
inline constexpr std::uint64_t kExitPercent = 85; // 15% hysteresis below entry
// Offscreen geometry still requires projection/culling CPU, but does not
// generate an SDL texture submission. Experimental 25% preparation weight.
inline constexpr std::uint64_t kOffscreenPercent = 25;

struct View {
    float centerX = 0.f, centerY = 0.f, width = 0.f, height = 0.f;
};
// Compute viewport edges once per frame, not for every submitted sprite.
struct ViewBounds {
    float left = 0.f, right = 0.f, top = 0.f, bottom = 0.f;
    bool valid = false;
};
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
inline bool DefinitelyOffscreenSprite(const View& v,
    float x, float y, float w, float h, float scaleX, float scaleY,
    float offsetX, float offsetY, float pivotX, float pivotY) noexcept {
    return DefinitelyOffscreenSprite(MakeViewBounds(v), x, y, w, h,
                                     scaleX, scaleY, offsetX, offsetY, pivotX, pivotY);
}
inline constexpr std::uint64_t EffectiveUnits(std::uint64_t units,
                                              bool definitelyOffscreen) noexcept {
    return definitelyOffscreen ? units * kOffscreenPercent / 100 : units;
}
// Only the comparison against the entry/exit boundaries matters. After
// reaching the higher entry boundary, more exact visibility scores cannot
// change either decision (including after a hysteresis transition).
inline constexpr std::uint64_t EntryBoundary(std::size_t threshold) noexcept {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    return threshold > maximum / kUnit ? maximum :
           static_cast<std::uint64_t>(threshold) * kUnit;
}
inline constexpr std::uint64_t CappedEffectiveSum(std::uint64_t current,
    std::uint64_t next, std::uint64_t boundary) noexcept {
    return current >= boundary ? boundary :
           current + std::min(next, boundary - current);
}

enum class Selection : std::uint8_t { Fixed, Adaptive };
inline constexpr const char* Name(Selection mode) noexcept {
    return mode == Selection::Adaptive ? "adaptive" : "fixed";
}
// Benchmark-only ablation controls. Auto preserves production behavior:
// fixed scores weighted units; adaptive also estimates offscreen sprites.
// No graphics, physics or runtime configuration files depend on these enums.
enum class WorkEstimate : std::uint8_t { Off = 0, Weighted = 1, Visibility = 2, Auto = 3 };
enum class RouteOverride : std::uint8_t { Auto = 0, Immediate = 1, Recorded = 2 };
inline constexpr std::uint64_t SpriteUnits(bool tilted, bool rotated, bool offset) noexcept {
    return kUnit + (tilted ? kXYZTiltExtra : 0) +
           (rotated ? kRotationExtra : 0) + (offset ? kOffsetExtra : 0);
}
inline constexpr std::uint64_t RectangleUnits(bool rotated) noexcept {
    return kUnit + (rotated ? kRotationExtra : 0);
}
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

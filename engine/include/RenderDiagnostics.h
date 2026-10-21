#pragma once
// Deterministic adaptive-render selection, independent of SDL and testable on its own.
#include <cstddef>
#include <cstdint>

namespace RenderDiagnostics {
inline constexpr std::size_t kParallelThreshold = 384;
enum class Path : std::uint8_t {
    Disabled = 0, NoWorkers = 1, BelowThreshold = 2, Parallel = 3
};
inline constexpr const char* Name(Path path) noexcept {
    switch (path) {
        case Path::Disabled: return "disabled";
        case Path::NoWorkers: return "no_workers";
        case Path::BelowThreshold: return "below_threshold";
        case Path::Parallel: return "parallel";
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
} // namespace RenderDiagnostics

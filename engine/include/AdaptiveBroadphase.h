#pragma once
// Online selection of spatial broadphase vs all-pairs based on *measured* CPU
// cost (grid construction + candidate evaluation) for comparable workloads.
// Allocation-free; no SDL dependency. Calibration and periodic probes allow
// adapting to different occupancy patterns without an arbitrary object cutoff.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

class AdaptiveBroadphase final {
public:
    // One bucket per power-of-two collider-count range, and one per worker mode.
    // Separate buckets ensure observations from different scales are not mixed.
    void Reset() noexcept { mBuckets = {}; }

    bool Choose(std::size_t colliders, bool parallelWorkers) noexcept {
        Bucket& b = Get(colliders, parallelWorkers);
        // Interleave both algorithms while calibrating; avoid temporal bias.
        if (b.brute.samples < kInitialSamples || b.grid.samples < kInitialSamples)
            return b.brute.samples > b.grid.samples;
        ++b.calls;
        // Every 48 passes, remeasure the currently losing strategy. This
        // adapts to moving/clustering objects without frequent mode switches.
        if (b.calls % kProbeInterval == 0) return !b.preferGrid;
        return b.preferGrid;
    }

    void Observe(std::size_t colliders, bool parallelWorkers, bool usedGrid,
                 float gridBuildAndCandidateMs) noexcept {
        if (!std::isfinite(gridBuildAndCandidateMs) || gridBuildAndCandidateMs < 0.f) return;
        Bucket& b = Get(colliders, parallelWorkers);
        Estimate& e = usedGrid ? b.grid : b.brute;
        if (e.samples == 0) e.mean = gridBuildAndCandidateMs;
        else e.mean += kAlpha * (gridBuildAndCandidateMs - e.mean);
        ++e.samples;
        if (b.brute.samples < kInitialSamples || b.grid.samples < kInitialSamples) return;
        // Hysteresis: challenger must beat incumbent by at least 8%.
        // This avoids oscillating on timing noise close to the crossover.
        if (b.preferGrid) {
            if (b.brute.mean < b.grid.mean * (1.f - kHysteresis)) b.preferGrid = false;
        } else if (b.grid.mean < b.brute.mean * (1.f - kHysteresis)) {
            b.preferGrid = true;
        }
    }

private:
    static constexpr unsigned kInitialSamples = 6;
    static constexpr unsigned kProbeInterval = 48;
    static constexpr float kAlpha = 0.125f;
    static constexpr float kHysteresis = 0.08f;
    struct Estimate { float mean = 0.f; unsigned samples = 0; };
    struct Bucket { Estimate brute, grid; unsigned calls = 0; bool preferGrid = false; };
    std::array<Bucket, 64> mBuckets{};

    Bucket& Get(std::size_t n, bool parallelWorkers) noexcept {
        unsigned bucket = 0;
        while (n > 1 && bucket < 31) { n >>= 1; ++bucket; }
        return mBuckets[bucket + (parallelWorkers ? 32u : 0u)];
    }
};


// Four measured strategies: all-pairs and three spatial grids. Samples are
// segregated by collider scale, worker availability and coarse occupancy.
// A conservative incumbent/challenger policy prevents switching on a single
// lucky timing sample and progressively reduces probes in stable scenes.
class AdaptiveCellBroadphase final {
public:
    static constexpr unsigned kStrategies = 4;
    static constexpr unsigned kInitialSamples = 5;
    static constexpr unsigned kProbeInterval = 24; // first post-calibration probe
    static constexpr unsigned kMaxProbeInterval = 192;
    static constexpr unsigned kConfirmationInterval = 8;
    static constexpr unsigned kConfirmations = 3;
    static constexpr float kHysteresis = 0.12f;
    // Concentration distinguishes two spatial layouts with the same number of
    // colliders and similar global AABB occupancy. Contexts are *not* cutoffs
    // for choosing an algorithm: each context measures all four strategies.
    static unsigned ConcentrationClass(const std::array<unsigned, 16>& bins) noexcept {
        double count = 0., sumSquares = 0.;
        for (unsigned v : bins) { count += v; sumSquares += double(v) * double(v); }
        if (sumSquares == 0.) return 0u;
        const double effectiveBins = count * count / sumSquares;
        return effectiveBins <= 3. ? 0u : (effectiveBins <= 8. ? 1u : 2u);
    }
    struct Choice { unsigned strategy = 0; bool probe = false; };

    void Reset() noexcept { mBuckets = {}; }

    Choice Choose(std::size_t colliders, bool workers, unsigned densityClass) noexcept {
        Bucket& b = Get(colliders, workers, densityClass);
        // Calibration runs in round-robin order to limit time-dependent bias.
        for (unsigned i = 0; i < kStrategies; ++i) {
            const unsigned candidate = (b.calibrationCursor + i) % kStrategies;
            if (b.cost[candidate].samples < kInitialSamples) {
                b.calibrationCursor = (candidate + 1) % kStrategies;
                return {candidate, true};
            }
        }
        ++b.calls;
        if (b.cooldown > 0) --b.cooldown;
        if (b.pending != kStrategies) {
            if (++b.sinceProbe >= kConfirmationInterval) {
                b.sinceProbe = 0;
                return {b.pending, true};
            }
            return {b.winner, false};
        }
        if (++b.sinceProbe >= b.probeInterval) {
            b.sinceProbe = 0;
            // Rotate through *other* strategies, not just the one most
            // frequently winning prior benchmarks.
            for (unsigned step = 0; step < kStrategies; ++step) {
                const unsigned candidate = (b.probeCursor + step) % kStrategies;
                if (candidate != b.winner) {
                    b.probeCursor = (candidate + 1) % kStrategies;
                    return {candidate, true};
                }
            }
        }
        return {b.winner, false};
    }

    void Observe(std::size_t colliders, bool workers, unsigned densityClass,
                 unsigned strategy, float milliseconds) noexcept {
        if (strategy >= kStrategies || !std::isfinite(milliseconds) || milliseconds < 0.f) return;
        Bucket& b = Get(colliders, workers, densityClass);
        Estimate& e = b.cost[strategy];
        // Compare a fresh probe against costs observed for the incumbent.
        // The trimmed mean uses recent samples and is less sensitive to
        // transient scheduler jitter than an EWMA of all historic timings.
        const float previous = b.cost[b.winner].RobustCost();
        e.Add(milliseconds);
        for (const auto& candidate : b.cost)
            if (candidate.samples < kInitialSamples) return;
        if (!b.calibrated) {
            b.calibrated = true;
            unsigned best = 0;
            for (unsigned i = 1; i < kStrategies; ++i)
                if (b.cost[i].RobustCost() < b.cost[best].RobustCost()) best = i;
            b.winner = b.cost[best].RobustCost() < b.cost[0].RobustCost() * (1.f - kHysteresis)
                       ? best : 0;
            return;
        }
        if (strategy == b.winner) {
            // Sustained changes in winner cost merit rapid re-probing, even
            // if the (collider count, worker, occupancy) bucket is unchanged.
            // Sub-25-microsecond fluctuations can be enormous in relative
            // terms for nearly empty scenes. Require a meaningful *absolute*
            // regression as well as a relative one before waking up probes.
            if (previous > 0.f &&
                milliseconds > previous * 1.30f &&
                milliseconds - previous > 0.025f) {
                if (++b.driftSamples >= 3) {
                    b.probeInterval = kProbeInterval;
                    b.quietProbes = 0;
                    b.sinceProbe = b.probeInterval - 1;
                    b.driftSamples = 0;
                }
            } else b.driftSamples = 0;
            return;
        }

        // Challenger confirmation is based on fresh, *independent* probes,
        // rather than stale timing history that may describe a former scene.
        if (b.cooldown == 0 && previous > 0.f &&
            milliseconds < previous * (1.f - kHysteresis)) {
            if (b.pending != strategy) { b.pending = strategy; b.confirmations = 1; }
            else ++b.confirmations;
            if (b.confirmations >= kConfirmations) {
                b.winner = strategy;
                b.pending = kStrategies;
                b.confirmations = 0;
                b.cooldown = 64;
                b.probeInterval = kProbeInterval;
                b.quietProbes = 0;
                b.driftSamples = 0;
            }
        } else {
            b.pending = kStrategies;
            b.confirmations = 0;
            // After repeatedly unproductive probes, exponentially back off.
            // Never stop checking: moving objects can change the winner.
            if (++b.quietProbes >= 3) {
                b.probeInterval = std::min(kMaxProbeInterval, b.probeInterval * 2);
                b.quietProbes = 0;
            }
        }
    }

    static float CellSize(unsigned strategy, float reference = 2.f) noexcept {
        if (strategy == 1) return std::max(0.05f, reference * 0.5f);
        if (strategy == 3) return std::min(4096.f, reference * 2.f);
        return reference;
    }

private:
    // Nine recent observations per strategy. Drop the largest and smallest
    // samples so a brief OS scheduling spike cannot become the new baseline.
    struct Estimate {
        std::array<float, 9> recent{};
        unsigned samples = 0;
        void Add(float x) noexcept { recent[samples % recent.size()] = x; ++samples; }
        float RobustCost() const noexcept {
            const unsigned count = std::min(samples, static_cast<unsigned>(recent.size()));
            if (!count) return 0.f;
            std::array<float, 9> values = recent;
            std::sort(values.begin(), values.begin() + count);
            const unsigned trim = count >= 5 ? 1u : 0u;
            float sum = 0.f;
            for (unsigned i = trim; i < count - trim; ++i) sum += values[i];
            return sum / static_cast<float>(count - 2 * trim);
        }
    };
    struct Bucket {
        std::array<Estimate, kStrategies> cost{};
        unsigned calibrationCursor = 0, probeCursor = 0, winner = 0;
        unsigned calls = 0, sinceProbe = 0, probeInterval = kProbeInterval;
        unsigned pending = kStrategies, confirmations = 0;
        unsigned quietProbes = 0, driftSamples = 0, cooldown = 0;
        bool calibrated = false;
    };
    // Three global occupancy classes x three spatial concentration classes.
    std::array<Bucket, 32 * 2 * 9> mBuckets{};
    Bucket& Get(std::size_t n, bool workers, unsigned density) noexcept {
        unsigned bin = 0;
        while (n > 1 && bin < 31) { n >>= 1; ++bin; }
        return mBuckets[bin + 32u * (workers ? 1u : 0u) + 64u * std::min(density, 8u)];
    }
};

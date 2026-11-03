#pragma once
/** @file AdaptiveBroadphase.h
 * @brief Timing-driven, allocation-free choice of collision broad-phase algorithms.
 */
// Online selection of spatial broadphase vs all-pairs based on *measured* CPU
// cost (grid construction + candidate evaluation) for comparable workloads.
// Allocation-free; no SDL dependency. Calibration and periodic probes allow
// adapting to different occupancy patterns without an arbitrary object cutoff.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace xengine::detail {

/** @brief Learns measured grid-versus-brute-force cost by collider-count bucket.
 * @note Buckets also distinguish serial and parallel candidate evaluation.
 * Calibration alternates both algorithms and probes a losing strategy
 * periodically; an 8% hysteresis reduces switches caused by noise.
 */
class AdaptiveBroadphase final {
public:
    // One bucket per power-of-two collider-count range, and one per worker mode.
    // Separate buckets ensure observations from different scales are not mixed.
    /** @brief Reset the AdaptiveBroadphase state.
     */
    void Reset() noexcept { mBuckets = {}; }

    /** @brief Chooses whether to use grid broad phase on the current pass.
     * @return True for grid, false for brute force. During calibration this
     *         intentionally probes both candidates rather than always using
     *         the current winner.
     * @param colliders Collider collection to query or use in collision detection.
     * @param parallelWorkers Number of workers available for parallel processing.
     */
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

    /** @brief Records the observed preparation/candidate cost in milliseconds.
     * @note Invalid negative or non-finite measurements are ignored.
     * @param colliders Collider collection to query or use in collision detection.
     * @param parallelWorkers Number of workers available for parallel processing.
     * @param usedGrid Whether to used grid for this operation.
     * @param gridBuildAndCandidateMs Grid build and candidate ms in milliseconds.
     */
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
    /** @brief Configured initial samples value used by AdaptiveBroadphase.
     */
    static constexpr unsigned kInitialSamples = 6;
    /** @brief Configured probe interval value used by AdaptiveBroadphase.
     */
    static constexpr unsigned kProbeInterval = 48;
    /** @brief Configured alpha value used by AdaptiveBroadphase.
     */
    static constexpr float kAlpha = 0.125f;
    /** @brief Configured hysteresis value used by AdaptiveBroadphase.
     */
    static constexpr float kHysteresis = 0.08f;
    /** @brief Stored mean value used by Estimate.
     */
    struct Estimate { float mean = 0.f; unsigned samples = 0; };
    /** @brief Configured brute value used by Bucket.
     */
    struct Bucket { Estimate brute, grid; unsigned calls = 0; bool preferGrid = false; };
    /** @brief Stored buckets value used by AdaptiveBroadphase.
     */
    std::array<Bucket, 64> mBuckets{};

    /** @brief Retrieve the value from AdaptiveBroadphase.
     * @param n N supplied to AdaptiveBroadphase.
     * @param parallelWorkers Number of workers available for parallel processing.
     * @return Requested get value or reference.
     */
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
/** @brief Adapts between brute force and three grid sizes using measured costs.
 * @note Uses collider scale, worker mode and spatial concentration to keep
 * independent measurements; choices are not fixed collider-count thresholds.
 */
class AdaptiveCellBroadphase final {
public:
    /** @brief Configured strategies value used by AdaptiveCellBroadphase.
     */
    static constexpr unsigned kStrategies = 4;
    /** @brief Configured initial samples value used by AdaptiveCellBroadphase.
     */
    static constexpr unsigned kInitialSamples = 5;
    /** @brief Configured probe interval value used by AdaptiveCellBroadphase.
     */
    static constexpr unsigned kProbeInterval = 24; // first post-calibration probe
    /** @brief Configured max probe interval value used by AdaptiveCellBroadphase.
     */
    static constexpr unsigned kMaxProbeInterval = 192;
    /** @brief Configured confirmation interval value used by AdaptiveCellBroadphase.
     */
    static constexpr unsigned kConfirmationInterval = 8;
    /** @brief Configured confirmations value used by AdaptiveCellBroadphase.
     */
    static constexpr unsigned kConfirmations = 3;
    /** @brief Configured hysteresis value used by AdaptiveCellBroadphase.
     */
    static constexpr float kHysteresis = 0.12f;
    // Concentration distinguishes two spatial layouts with the same number of
    // colliders and similar global AABB occupancy. Contexts are *not* cutoffs
    // for choosing an algorithm: each context measures all four strategies.
    /** @brief Concentration class in AdaptiveCellBroadphase.
     * @param bins Bins supplied to AdaptiveCellBroadphase.
     * @return Result of concentration class in AdaptiveCellBroadphase.
     */
    static unsigned ConcentrationClass(const std::array<unsigned, 16>& bins) noexcept {
        double count = 0., sumSquares = 0.;
        for (unsigned v : bins) { count += v; sumSquares += double(v) * double(v); }
        if (sumSquares == 0.) return 0u;
        const double effectiveBins = count * count / sumSquares;
        return effectiveBins <= 3. ? 0u : (effectiveBins <= 8. ? 1u : 2u);
    }
    /** @brief Configured strategy value used by Choice.
     */
    struct Choice { unsigned strategy = 0; bool probe = false; };

    /** @brief Reset the AdaptiveCellBroadphase state.
     */
    void Reset() noexcept { mBuckets = {}; }

    /** @brief Choose in AdaptiveCellBroadphase.
     * @param colliders Collider collection to query or use in collision detection.
     * @param workers Workers supplied to AdaptiveCellBroadphase.
     * @param densityClass Density class supplied to AdaptiveCellBroadphase.
     * @return Result of choose in AdaptiveCellBroadphase.
     */
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

    /** @brief Observe in AdaptiveCellBroadphase.
     * @param colliders Collider collection to query or use in collision detection.
     * @param workers Workers supplied to AdaptiveCellBroadphase.
     * @param densityClass Density class supplied to AdaptiveCellBroadphase.
     * @param strategy Strategy supplied to AdaptiveCellBroadphase.
     * @param milliseconds Milliseconds supplied to AdaptiveCellBroadphase.
     */
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

    /** @brief Cell size in AdaptiveCellBroadphase.
     * @param strategy Strategy supplied to AdaptiveCellBroadphase.
     * @param reference Reference supplied to AdaptiveCellBroadphase.
     * @return Result of cell size in AdaptiveCellBroadphase.
     */
    static float CellSize(unsigned strategy, float reference = 2.f) noexcept {
        if (strategy == 1) return std::max(0.05f, reference * 0.5f);
        if (strategy == 3) return std::min(4096.f, reference * 2.f);
        return reference;
    }

private:
    // Nine recent observations per strategy. Drop the largest and smallest
    // samples so a brief OS scheduling spike cannot become the new baseline.
    /** @brief Recorded statistics and decisions for adaptive algorithm selection.
     */
    struct Estimate {
        /** @brief Configured recent value used by Estimate.
         */
        std::array<float, 9> recent{};
        /** @brief Configured samples value used by Estimate.
         */
        unsigned samples = 0;
        /** @brief Add in Estimate.
         * @param x Horizontal coordinate or component.
         */
        void Add(float x) noexcept { recent[samples % recent.size()] = x; ++samples; }
        /** @brief Robust cost in Estimate.
         * @return Result of robust cost in Estimate.
         */
        float RobustCost() const noexcept {
            const unsigned count = std::min(samples, static_cast<unsigned>(recent.size()));
            if (!count) return 0.f;
            std::array<float, 9> values = recent;
            // At most nine samples: an insertion sort is small, deterministic
            // and avoids GCC 16's false-positive -Warray-bounds diagnostic
            // in std::sort's heap implementation for this bounded subrange.
            for (unsigned i = 1; i < count; ++i) {
                const float current = values[i];
                unsigned j = i;
                while (j > 0 && values[j - 1] > current) {
                    values[j] = values[j - 1];
                    --j;
                }
                values[j] = current;
            }
            const unsigned trim = count >= 5 ? 1u : 0u;
            float sum = 0.f;
            for (unsigned i = trim; i < count - trim; ++i) sum += values[i];
            return sum / static_cast<float>(count - 2 * trim);
        }
    };
    /** @brief Recorded statistics and decisions for adaptive algorithm selection.
     */
    struct Bucket {
        /** @brief Configured cost value used by Bucket.
         */
        std::array<Estimate, kStrategies> cost{};
        /** @brief Configured calibration cursor value used by Bucket.
         */
        unsigned calibrationCursor = 0, probeCursor = 0, winner = 0;
        /** @brief Configured calls value used by Bucket.
         */
        unsigned calls = 0, sinceProbe = 0, probeInterval = kProbeInterval;
        /** @brief Configured pending value used by Bucket.
         */
        unsigned pending = kStrategies, confirmations = 0;
        /** @brief Configured quiet probes value used by Bucket.
         */
        unsigned quietProbes = 0, driftSamples = 0, cooldown = 0;
        /** @brief Configured calibrated value used by Bucket.
         */
        bool calibrated = false;
    };
    // Three global occupancy classes x three spatial concentration classes.
    /** @brief Stored buckets value used by AdaptiveCellBroadphase.
     */
    std::array<Bucket, 32 * 2 * 9> mBuckets{};
    /** @brief Retrieve the value from AdaptiveCellBroadphase.
     * @param n N supplied to AdaptiveCellBroadphase.
     * @param workers Workers supplied to AdaptiveCellBroadphase.
     * @param density Density supplied to AdaptiveCellBroadphase.
     * @return Requested get value or reference.
     */
    Bucket& Get(std::size_t n, bool workers, unsigned density) noexcept {
        unsigned bin = 0;
        while (n > 1 && bin < 31) { n >>= 1; ++bin; }
        return mBuckets[bin + 32u * (workers ? 1u : 0u) + 64u * std::min(density, 8u)];
    }
};

} // namespace xengine::detail
/** @brief Type alias for adaptive broadphase used by XEngine/detail/AdaptiveBroadphase.h.
 */
using AdaptiveBroadphase = xengine::detail::AdaptiveBroadphase;
/** @brief Type alias for adaptive cell broadphase used by XEngine/detail/AdaptiveBroadphase.h.
 */
using AdaptiveCellBroadphase = xengine::detail::AdaptiveCellBroadphase;

/** @name Co-declared engine member descriptions
 * @brief Members sharing declarations with other fields.
 * @{ */
/** @var Estimate xengine::detail::AdaptiveBroadphase::Bucket::brute
 * @brief Brute used by Bucket.
 */

/** @var Estimate xengine::detail::AdaptiveBroadphase::Bucket::grid
 * @brief Grid used by Bucket.
 */

/** @var unsigned xengine::detail::AdaptiveBroadphase::Bucket::calls
 * @brief Recorded calls used to compare algorithm costs.
 */

/** @var bool xengine::detail::AdaptiveBroadphase::Bucket::preferGrid
 * @brief Prefer grid used by Bucket.
 */

/** @var unsigned xengine::detail::AdaptiveCellBroadphase::Bucket::probeCursor
 * @brief Probe cursor used by Bucket.
 */

/** @var unsigned xengine::detail::AdaptiveCellBroadphase::Bucket::winner
 * @brief Index identifying the selected broad-phase winner.
 */

/** @var unsigned xengine::detail::AdaptiveCellBroadphase::Bucket::sinceProbe
 * @brief Since probe used by Bucket.
 */

/** @var unsigned xengine::detail::AdaptiveCellBroadphase::Bucket::probeInterval
 * @brief Probe interval used by Bucket.
 */

/** @var unsigned xengine::detail::AdaptiveCellBroadphase::Bucket::confirmations
 * @brief Confirmations used by Bucket.
 */

/** @var unsigned xengine::detail::AdaptiveCellBroadphase::Bucket::driftSamples
 * @brief Drift samples used by Bucket.
 */

/** @var unsigned xengine::detail::AdaptiveCellBroadphase::Bucket::cooldown
 * @brief Cooldown used by Bucket.
 */

/** @var unsigned xengine::detail::AdaptiveCellBroadphase::Choice::strategy
 * @brief Index identifying the selected broad-phase strategy.
 */

/** @var bool xengine::detail::AdaptiveCellBroadphase::Choice::probe
 * @brief Probe used by Choice.
 */

/** @var float xengine::detail::AdaptiveBroadphase::Estimate::mean
 * @brief Recorded mean used to compare algorithm costs.
 */

/** @var unsigned xengine::detail::AdaptiveBroadphase::Estimate::samples
 * @brief Recorded samples used to compare algorithm costs.
 */
/** @} */

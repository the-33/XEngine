#pragma once
/** @file SpatialContextClassifier.h
 * @brief Stable context bucketing used for adaptive collision broadphase measurements.
 */
// Stable, allocation-free spatial context classification for adaptive broadphase.
// The 3x3 context chooses an independent measurement bucket; it never directly
// selects an algorithm. Margin-based hysteresis and two observations prevent
// jitter around density / concentration boundaries from resetting calibration.
#include <cmath>
#include <cstddef>

/** @brief Classifies collision workloads into one of nine measurement contexts.
 * @details Contexts combine three bins of average occupancy and three bins of
 * effective occupied-cell concentration. Collider-count scale and worker
 * availability reset stability evidence, but are not part of the nine-valued
 * identifier. A 15% margin and two agreeing observations suppress rapid
 * transitions near thresholds. The result selects a *calibration bucket*;
 * it does not itself choose a collision algorithm.
 */
class SpatialContextClassifier final {
public:
    /** @brief Forget the previous bucket and hysteresis state.
     */
    void Reset() noexcept { *this = {}; }

    /** @brief Observe workload metrics and return the stabilized bucket (0..8).
     * @param occupancy Average occupancy, classified at 0.1 and 1.0.
     * @param effectiveBins Effective number of occupied spatial bins, classified
     *                      at 3 and 8; nonfinite or negative values become zero.
     * @param colliderCount Used to detect a change of order-of-magnitude scale.
     * @param workers Whether parallel collision-processing workers are available.
     * @return `occupancyBin * 3 + concentrationBin`, after hysteresis.
     */
    unsigned Observe(double occupancy, double effectiveBins,
                     std::size_t colliderCount, bool workers) noexcept {
        if (!std::isfinite(occupancy) || occupancy < 0.) occupancy = 0.;
        if (!std::isfinite(effectiveBins) || effectiveBins < 0.) effectiveBins = 0.;
        const unsigned scale = Scale(colliderCount);
        // A count/worker change invalidates the previous stability evidence.
        if (!mInitialized || scale != mScale || workers != mWorkers) {
            mScale = scale;
            mWorkers = workers;
            mOccupancy = RawBin(occupancy, 0.1, 1.0);
            mConcentration = RawBin(effectiveBins, 3., 8.);
            mInitialized = true;
            mPending = 9;
            mPendingFrames = 0;
            return mOccupancy * 3u + mConcentration;
        }
        const unsigned proposedOccupancy = HystereticBin(
            occupancy, mOccupancy, 0.1, 1.0, 0.15);
        const unsigned proposedConcentration = HystereticBin(
            effectiveBins, mConcentration, 3., 8., 0.15);
        const unsigned proposal = proposedOccupancy * 3u + proposedConcentration;
        const unsigned current = mOccupancy * 3u + mConcentration;
        if (proposal == current) {
            mPending = 9;
            mPendingFrames = 0;
            return current;
        }
        if (proposal != mPending) {
            mPending = proposal;
            mPendingFrames = 1;
        } else if (++mPendingFrames >= 2) {
            mOccupancy = proposedOccupancy;
            mConcentration = proposedConcentration;
            mPending = 9;
            mPendingFrames = 0;
        }
        return mOccupancy * 3u + mConcentration;
    }

    /** @brief Computes Simpson's effective number of bins: (sum counts)^2 / sum(count^2).
     * @param bins Non-null count array when count is nonzero.
     * @param count Number of entries in bins.
     * @return Zero for an empty/zero-filled histogram; otherwise an effective
     *         number of occupied bins, at least one when particles exist.
     */
    static double EffectiveBins(const unsigned* bins, std::size_t count) noexcept {
        double sum = 0., squares = 0.;
        for (std::size_t i = 0; i < count; ++i) {
            const double value = bins[i];
            sum += value;
            squares += value * value;
        }
        return squares > 0. ? sum * sum / squares : 0.;
    }

private:
    static unsigned RawBin(double value, double low, double high) noexcept {
        return value >= high ? 2u : (value >= low ? 1u : 0u);
    }
    static unsigned HystereticBin(double value, unsigned current,
                                  double low, double high, double margin) noexcept {
        const double thresholds[2]{low, high};
        while (current < 2 && value > thresholds[current] * (1. + margin)) ++current;
        while (current > 0 && value < thresholds[current - 1] * (1. - margin)) --current;
        return current;
    }
    static unsigned Scale(std::size_t colliders) noexcept {
        unsigned scale = 0;
        while (colliders > 1 && scale < 31) { colliders >>= 1; ++scale; }
        return scale;
    }
    unsigned mScale = 0, mOccupancy = 0, mConcentration = 0;
    unsigned mPending = 9, mPendingFrames = 0;
    bool mWorkers = false, mInitialized = false;
};

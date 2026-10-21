#pragma once
// Stable, allocation-free spatial context classification for adaptive broadphase.
// The 3x3 context chooses an independent measurement bucket; it never directly
// selects an algorithm. Margin-based hysteresis and two observations prevent
// jitter around density / concentration boundaries from resetting calibration.
#include <cmath>
#include <cstddef>

class SpatialContextClassifier final {
public:
    void Reset() noexcept { *this = {}; }

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

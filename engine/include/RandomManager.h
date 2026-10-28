#pragma once
/** @file RandomManager.h
 * @brief Seeded random values, ranges and circle/sphere samples.
 */

#include <random>
#include <cstdint>
#include <chrono>

#include "Singleton.h"
#include "BaseTypes.h"
#include "Engine.h"

/** @brief Seeded pseudo-random sampling for gameplay and deterministic tests.
 * @ingroup xengine_services
 */
class RandomManager final : public Singleton<RandomManager>
{
    friend class Singleton<RandomManager>;
    friend class Engine;

public:
    /** @brief Initializes the random-number generator.
     * @param seed Seed that determines the pseudo-random number sequence.
     */
    bool Init(uint64_t* seed = nullptr) noexcept;

    /** @brief Reseeds the pseudo-random generator for reproducible future samples.
     * @param seed Seed that determines the pseudo-random number sequence.
     */
    void SetSeed(uint64_t seed) noexcept;
    /** @brief Returns the seed of RandomManager.
     */
    uint64_t GetSeed() noexcept;

    /** @brief Returns a uniformly distributed float in the half-open interval [0, 1).
     */
    float Value() noexcept;                  // [0,1)
    /** @brief Samples a random Boolean value.
     */
    bool  Bool() noexcept;
    /** @brief Samples a random sign, negative or positive.
     */
    int   Sign() noexcept;

    /** @brief Samples a value from the specified inclusive/exclusive or inclusive float range.
     * @param minInclusive Inclusive lower bound of the requested random interval.
     * @param maxExclusive Exclusive upper bound of the requested random interval.
     */
    int   Range(int minInclusive, int maxExclusive) noexcept;
    /** @brief Samples a value from the specified inclusive/exclusive or inclusive float range.
     * @param minInclusive Inclusive lower bound of the requested random interval.
     * @param maxInclusive Inclusive upper bound of the requested random interval.
     */
    float Range(float minInclusive, float maxInclusive) noexcept;

    /** @brief Samples a random position inside the unit circle.
     */
    Vec2  InsideUnitCircle() noexcept;
    /** @brief Samples a random position along the unit circle.
     */
    Vec2  OnUnitCircle() noexcept;

    /** @brief Samples a random position inside the unit sphere.
     */
    Vec3  InsideUnitSphere() noexcept;
    /** @brief Samples a random position on the unit sphere.
     */
    Vec3  OnUnitSphere() noexcept;

private:
    RandomManager() = default;
    ~RandomManager() = default;

    RandomManager(const RandomManager&) = delete;
    RandomManager& operator=(const RandomManager&) = delete;
    RandomManager(RandomManager&&) = delete;
    RandomManager& operator=(RandomManager&&) = delete;

    uint64_t DefaultSeed() noexcept;

    uint64_t mSeed = 0;
    std::mt19937_64 mRng;
};
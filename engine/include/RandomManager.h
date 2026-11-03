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
    /** @brief Permit the singleton base to construct and release the RandomManager manager.
     */
    friend class Singleton<RandomManager>;
    /** @brief Grant Engine access to the internal state of RandomManager.
     */
    friend class Engine;

public:
    /** @brief Initializes the random-number generator.
     * @param seed Seed that determines the pseudo-random number sequence.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Init(uint64_t* seed = nullptr) noexcept;

    /** @brief Reseeds the pseudo-random generator for reproducible future samples.
     * @param seed Seed that determines the pseudo-random number sequence.
     */
    void SetSeed(uint64_t seed) noexcept;
    /** @brief Returns the active generator seed, or 0 if no manager exists.
     * @return The active generator seed, or 0 if no manager exists.
     */
    uint64_t GetSeed() noexcept;

    /** @brief Returns a uniformly distributed float in the half-open interval [0, 1).
     * @return A uniformly distributed float in the half-open interval [0, 1).
     */
    float Value() noexcept;                  // [0,1)
    /** @brief Samples a random Boolean value.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool  Bool() noexcept;
    /** @brief Samples a random sign, negative or positive.
     * @return Result of sign in RandomManager.
     */
    int   Sign() noexcept;

    /** @brief Samples an integer from [minInclusive, maxExclusive).
     * @param minInclusive Inclusive lower bound of the requested random interval.
     * @param maxExclusive Exclusive upper bound of the requested random interval.
     * @return Result of range in RandomManager.
     */
    int   Range(int minInclusive, int maxExclusive) noexcept;
    /** @brief Samples a floating-point value using std::uniform_real_distribution.
     * @param minInclusive Inclusive lower bound of the requested random interval.
     * @param maxInclusive Named upper bound (the standard distribution is normally half-open).
     * @note Pass the lower bound before the upper bound.
     * @return Result of range in RandomManager.
     */
    float Range(float minInclusive, float maxInclusive) noexcept;

    /** @brief Samples a random position inside the unit circle.
     * @return Result of inside unit circle in RandomManager.
     */
    Vec2  InsideUnitCircle() noexcept;
    /** @brief Samples a random position along the unit circle.
     * @return Result of on unit circle in RandomManager.
     */
    Vec2  OnUnitCircle() noexcept;

    /** @brief Samples a random position inside the unit sphere.
     * @return Result of inside unit sphere in RandomManager.
     */
    Vec3  InsideUnitSphere() noexcept;
    /** @brief Samples a random position on the unit sphere.
     * @return Result of on unit sphere in RandomManager.
     */
    Vec3  OnUnitSphere() noexcept;

private:
    /** @brief Construct a RandomManager instance.
     */
    RandomManager() = default;
    /** @brief Destroy a RandomManager instance and release owned resources.
     */
    ~RandomManager() = default;

    /** @brief Construct a RandomManager instance.
     * @param other Source object whose copy, move or equality is being requested.
     */
    RandomManager(const RandomManager& other) = delete;
    /** @brief Implement the operator= operation for RandomManager.
     * @return Result of applying the overloaded operator.
     * @param other Source object whose copy, move or equality is being requested.
     */
    RandomManager& operator=(const RandomManager& other) = delete;
    /** @brief Construct a RandomManager instance.
     * @param other Source object whose copy, move or equality is being requested.
     */
    RandomManager(RandomManager&& other) = delete;
    /** @brief Implement the operator= operation for RandomManager.
     * @return Result of applying the overloaded operator.
     * @param other Source object whose copy, move or equality is being requested.
     */
    RandomManager& operator=(RandomManager&& other) = delete;

    /** @brief Default seed in RandomManager.
     * @return Result of default seed in RandomManager.
     */
    uint64_t DefaultSeed() noexcept;

    /** @brief Stored seed value used by RandomManager.
     */
    uint64_t mSeed = 0;
    /** @brief Stored rng value used by RandomManager.
     */
    std::mt19937_64 mRng;
};

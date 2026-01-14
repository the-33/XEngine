#pragma once

#include <random>
#include <cstdint>
#include <chrono>

#include "Singleton.h"
#include "BaseTypes.h"
#include "Engine.h"

class RandomManager final : public Singleton<RandomManager>
{
    friend class Singleton<RandomManager>;
    friend class Engine;

public:
    bool Init(uint64_t* seed = nullptr) noexcept;

    void SetSeed(uint64_t seed) noexcept;
    uint64_t GetSeed() noexcept;

    float Value() noexcept;                  // [0,1)
    bool  Bool() noexcept;
    int   Sign() noexcept;

    int   Range(int minInclusive, int maxExclusive) noexcept;
    float Range(float minInclusive, float maxInclusive) noexcept;

    Vec2  InsideUnitCircle() noexcept;
    Vec2  OnUnitCircle() noexcept;

    Vec3  InsideUnitSphere() noexcept;
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
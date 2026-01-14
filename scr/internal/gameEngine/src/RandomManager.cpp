#include "RandomManager.h"

#include <cmath>

#include "TimeManager.h"

bool RandomManager::Init(uint64_t* seed) noexcept
{
    if (seed)
    {
        SetSeed(*seed);
        return true;
    }

    auto* time = TimeManager::GetInstancePtr();

    uint64_t s = 0;

    if (time)
    {
        s = time->highResTimeStamp;
    }
    else
    {
        LogError("RandomManager error", "Init(): TimeManager was not initialisated, aborting.");
        return false;
    }

    SetSeed(s);
    return true;
}

void RandomManager::SetSeed(uint64_t seed) noexcept
{
    mSeed = seed;
    mRng.seed(seed);
}

uint64_t RandomManager::GetSeed() noexcept
{
    if (auto* r = GetInstancePtr())
        return r->mSeed;
    return 0;
}

uint64_t RandomManager::DefaultSeed() noexcept
{
    return static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now()
        .time_since_epoch().count()
        );
}

// -------------------------------------------------
// Básicos
// -------------------------------------------------
float RandomManager::Value() noexcept
{
    static std::uniform_real_distribution<float> dist(0.f, 1.f);
    return dist(GetInstance().mRng);
}

bool RandomManager::Bool() noexcept
{
    static std::bernoulli_distribution dist(0.5);
    return dist(GetInstance().mRng);
}

int RandomManager::Sign() noexcept
{
    return Bool() ? 1 : -1;
}

// -------------------------------------------------
// Range
// -------------------------------------------------
int RandomManager::Range(int minInclusive, int maxExclusive) noexcept
{
    std::uniform_int_distribution<int> dist(minInclusive, maxExclusive - 1);
    return dist(GetInstance().mRng);
}

float RandomManager::Range(float minInclusive, float maxInclusive) noexcept
{
    std::uniform_real_distribution<float> dist(minInclusive, maxInclusive);
    return dist(GetInstance().mRng);
}

// -------------------------------------------------
// Geometría 2D
// -------------------------------------------------
Vec2 RandomManager::OnUnitCircle() noexcept
{
    const float a = Range(0.f, 2.f * 3.14159265359f);
    return { std::cos(a), std::sin(a) };
}

Vec2 RandomManager::InsideUnitCircle() noexcept
{
    while (true)
    {
        Vec2 p{
            Range(-1.f, 1.f),
            Range(-1.f, 1.f)
        };
        if (math::LengthSq(p) <= 1.f)
            return p;
    }
}

// -------------------------------------------------
// Geometría 3D
// -------------------------------------------------
Vec3 RandomManager::OnUnitSphere() noexcept
{
    const float z = Range(-1.f, 1.f);
    const float a = Range(0.f, 2.f * 3.14159265359f);
    const float r = std::sqrt(1.f - z * z);

    return {
        r * std::cos(a),
        z,
        r * std::sin(a)
    };
}

Vec3 RandomManager::InsideUnitSphere() noexcept
{
    while (true)
    {
        Vec3 p{
            Range(-1.f, 1.f),
            Range(-1.f, 1.f),
            Range(-1.f, 1.f)
        };
        if (math::LengthSq(p) <= 1.f)
            return p;
    }
}
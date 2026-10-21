#pragma once

namespace MyPrincessRun
{
    // Shared state for the current attempt. Only one copy exists in the executable.
    inline float gSpeedMultiplier = 1.f;
    inline float gScore = 0.f;
    inline float gScreenBorderLeftWorld = 0.f;
    inline float gScreenBorderRightWorld = 0.f;

    inline void ResetGameState() noexcept
    {
        gSpeedMultiplier = 1.f;
        gScore = 0.f;
    }
}

#include "TimeManager.h"

#include <algorithm>

#include "SDL.h"

// Local helpers for high-resolution timing in seconds
static inline double NowSecondsHR() noexcept {
    const Uint64 counter = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    return (freq > 0.0 ? static_cast<double>(counter) / freq : 0.0);
}

bool TimeManager::Init(float fixedDt_) noexcept {
    fixedDt = (fixedDt_ > 0.f ? fixedDt_ : 1.f / 60.f);
    timeScale_ = 1.f;
    delta = 0.f;

    const double now = NowSecondsHR();
    startTicks = now;
    prevTicks = now;

    return true;
}

void TimeManager::Tick() noexcept {
    const double now = NowSecondsHR();

    double raw = now - prevTicks;   // real seconds since the last frame
    if (raw < 0.0) raw = 0.0;       // in case the clock moves backwards

    prevTicks = now;

    // Optional: cap delta time to avoid large jumps after breakpoints or Alt-Tab.
    // Adjust this cap as needed (0.25s = approximately 4 FPS minimum).
    const float unclampedDt = static_cast<float>(raw);
    const float cappedDt = std::min(unclampedDt, 0.25f);

    delta = cappedDt * std::max(0.f, timeScale_); // dt escalado
}

double TimeManager::SinceStart() const noexcept {
    // Real elapsed time since Init() (unscaled)
    return NowSecondsHR() - startTicks;
}

void TimeManager::SetTimeScale(float s) noexcept {
    // Evitamos negativos
    timeScale_ = std::max(0.f, s);
}

uint64_t TimeManager::GetHighResTimestamp() const noexcept
{
    return SDL_GetPerformanceCounter();
}

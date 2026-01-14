#include "TimeManager.h"

#include <algorithm>

#include "SDL.h"

// Helpers locales para tiempo en segundos (alta resolución)
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

    double raw = now - prevTicks;   // segundos reales desde el último frame
    if (raw < 0.0) raw = 0.0;       // por si el reloj retrocede

    prevTicks = now;

    // (Opcional) Limitar DT para evitar saltos enormes tras un breakpoint o alt-tab.
    // Ajusta este tope a tus necesidades (0.25s = ~4 FPS mínimo).
    const float unclampedDt = static_cast<float>(raw);
    const float cappedDt = std::min(unclampedDt, 0.25f);

    delta = cappedDt * std::max(0.f, timeScale_); // dt escalado
}

double TimeManager::SinceStart() const noexcept {
    // Tiempo real desde Init() (no escalado)
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

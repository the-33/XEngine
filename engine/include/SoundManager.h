#pragma once

#include <vector>

#include "Singleton.h"
#include "Assets.h"
#include "Engine.h"

// Forward declarations
class TimeManager;

class SoundManager : public Singleton<SoundManager>
{
    friend class Singleton<SoundManager>;
    friend class Engine;

private:
    struct Config {
        int   sfxChannels = 32;   // number of sound-effect channels
        float masterVolume = 1.0f; // 0..1, afecta a TODO (music + sfx)
        bool  enableDucking = false; // optional: reduce music volume while playing SFX
        float duckVolume = 0.6f;  // music volume while SFX are active
        float duckAttackSec = 0.02f; // tiempos
        float duckReleaseSec = 0.25f;
    };

    SoundManager() = default;
    ~SoundManager() = default;
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    int   mSfxChannels = 32;
    float mMasterVolume = 1.0f;
    bool  mEnableDucking = false;
    float mDuckVolume = 0.6f;
    float mDuckAttackSec = 0.02f;
    float mDuckReleaseSec = 0.25f;

    std::vector<bool> mChannelBusy;

    TimeManager* mTimeManager = nullptr;

    bool Init(const Config& cfg) noexcept;
    void Shutdown() noexcept;
    void Update() noexcept; // when handling ducking/fades

public:
    // --- Volumen global ---
    void  SetMasterVolume(float v01) noexcept; // 0..1
    float MasterVolume() const noexcept;

    // --- Music (no separate music volume: master volume is used) ---
    bool PlayMusic(const Music* music, int loops = -1, float gain01 = 1.0f) noexcept;
    void StopMusic() noexcept;
    bool IsMusicPlaying() const noexcept;

    // --- SFX (automatic channel management) ---
    // Return the assigned channel for optional stopping/querying; use of the result is NOT required.
    int  PlaySFX(const SoundEffect* sfx, int loops = 0, float gain01 = 1.0f) noexcept;
    void StopAllSFX() noexcept;
    void StopSFXChannel(int channel) noexcept; // opcional
    bool IsAnySFXPlaying() const noexcept;

    // --- Config en runtime ---
    void SetSFXChannelCount(int count) noexcept;  // resize the channel pool if supported by the backend
    int  SFXChannelCount() const noexcept;
};
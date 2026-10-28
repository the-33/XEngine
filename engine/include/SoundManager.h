#pragma once
/** @file SoundManager.h
 * @brief Music and sound-effect playback, volume and channel management.
 */

#include <vector>

#include "Singleton.h"
#include "Assets.h"
#include "Engine.h"

// Forward declarations
class TimeManager;

/** @brief Plays and controls music and SFX with a shared master volume.
 * @ingroup xengine_services
 */
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
    /** @brief Sets common SFX/music volume using a normalized value from 0 to 1.
     * @param v01 Normalized volume from 0 to 1.
     */
    void  SetMasterVolume(float v01) noexcept; // 0..1
    /** @brief Gets or updates the global audio volume multiplier.
     */
    float MasterVolume() const noexcept;

    // --- Music (no separate music volume: master volume is used) ---
    /** @brief Starts background music; a negative loop count repeats indefinitely.
     * @param music Loaded music resource; must remain valid during use.
     * @param loops Additional playback repetitions; a negative music value requests indefinite playback.
     * @param gain01 Normalized gain multiplier from 0 to 1.
     */
    bool PlayMusic(const Music* music, int loops = -1, float gain01 = 1.0f) noexcept;
    /** @brief Stops music playback.
     */
    void StopMusic() noexcept;
    /** @brief Reports whether music playing is currently true.
     */
    bool IsMusicPlaying() const noexcept;

    // --- SFX (automatic channel management) ---
    // Return the assigned channel for optional stopping/querying; use of the result is NOT required.
    /** @brief Plays an effect and returns its assigned mixer channel, or a failure indicator.
     * @param sfx Loaded sound effect; must remain valid during use.
     * @param loops Additional playback repetitions; a negative music value requests indefinite playback.
     * @param gain01 Normalized gain multiplier from 0 to 1.
     */
    int  PlaySFX(const SoundEffect* sfx, int loops = 0, float gain01 = 1.0f) noexcept;
    /** @brief Stops all currently playing sound effects.
     */
    void StopAllSFX() noexcept;
    /** @brief Stops sound effects on the specified audio channel.
     * @param channel Mixer channel index previously returned by playback.
     */
    void StopSFXChannel(int channel) noexcept; // opcional
    /** @brief Reports whether any sfxplaying is currently true.
     */
    bool IsAnySFXPlaying() const noexcept;

    // --- Config en runtime ---
    /** @brief Changes the sfxchannel count of SoundManager.
     * @param count Number of elements or bits to read/write, as stated by the method.
     */
    void SetSFXChannelCount(int count) noexcept;  // resize the channel pool if supported by the backend
    /** @brief Returns the number of sound-effect playback channels.
     */
    int  SFXChannelCount() const noexcept;
};
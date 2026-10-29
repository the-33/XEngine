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

/** @brief Plays SDL_mixer music/SFX using a shared normalized master volume.
 *
 * Music uses one playback stream; SFX use numbered mixer channels. Volumes
 * are perceptually transformed before being sent to SDL_mixer. Optionally,
 * playing SFX ducks the music volume over configured attack/release times.
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
    /** @brief Returns the current normalized master volume (does not modify it).
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
    /** @brief True if SDL_mixer reports music currently playing.
     */
    bool IsMusicPlaying() const noexcept;

    // --- SFX (automatic channel management) ---
    // Return the assigned channel for optional stopping/querying; use of the result is NOT required.
    /** @brief Plays an effect and returns its assigned mixer channel, or a failure indicator.
     * @param sfx Loaded sound effect; must remain valid during use.
     * @param loops Additional playback repetitions; negative values repeat indefinitely.
     * @param gain01 Normalized gain multiplier from 0 to 1.
     * @return Assigned channel index, or -1 when playback fails.
     */
    int  PlaySFX(const SoundEffect* sfx, int loops = 0, float gain01 = 1.0f) noexcept;
    /** @brief Stops all currently playing sound effects.
     */
    void StopAllSFX() noexcept;
    /** @brief Stops sound effects on the specified audio channel.
     * @param channel Mixer channel index previously returned by playback.
     */
    void StopSFXChannel(int channel) noexcept; // opcional
    /** @brief True if one or more mixer channels are currently playing an effect.
     */
    bool IsAnySFXPlaying() const noexcept;

    // --- Config en runtime ---
    /** @brief Resizes the SFX channel pool, stopping currently playing effects.
     * @param count Requested number of mixer channels; negatives become zero.
     */
    void SetSFXChannelCount(int count) noexcept;  // resize the channel pool if supported by the backend
    /** @brief Returns the number of sound-effect playback channels.
     */
    int  SFXChannelCount() const noexcept;
};

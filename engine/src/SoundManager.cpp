#include "SoundManager.h"

#include <algorithm>
#include <cmath>

#include "TimeManager.h"
#include "ErrorHandler.h"
#include "SDL_mixer.h"

static inline int ToMixVol(float v01) // Use a logarithmic scale for better perceived volume
{
    v01 = std::clamp(v01, 0.0f, 1.0f);
    float perceptual = std::pow(v01, 2.2f);
    return int(perceptual * MIX_MAX_VOLUME + 0.5f);
}

static inline float Lerp(float a, float b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return a + (b - a) * t;
}

struct Runtime {
    float musicVol01 = 1.0f;
    float musicGain = 1.0f;
    std::vector<float> chGain;   // gain per SFX channel (0..1)
    bool  inited = false;
};
Runtime& RT() { static Runtime r; return r; }

static inline void ReapplySFXVolumes(float master01) {
    auto& rt = RT();
    const int n = (int)rt.chGain.size();
    for (int i = 0; i < n; ++i) {
        if (Mix_Playing(i)) {
            const float v = std::clamp(master01 * rt.chGain[i], 0.0f, 1.0f);
            Mix_Volume(i, ToMixVol(v));
        }
    }
}

bool SoundManager::Init(const Config& cfg) noexcept
{
    mTimeManager = TimeManager::GetInstancePtr();

    if (mTimeManager == nullptr) return false;

    // Open the audio device (typical parameters)
    const int frequency = 44100;
    const Uint16 format = MIX_DEFAULT_FORMAT;
    const int channels = 2; // Stereo
    const int chunksize = 2048;

    // Engine::StartImpl() already opens SDL_mixer. Avoid incrementing its
    // internal open counter twice: shutdown calls Mix_CloseAudio() once.
    if (Mix_QuerySpec(nullptr, nullptr, nullptr) == 0) {
        if (Mix_OpenAudio(frequency, format, channels, chunksize) != 0) {
            LogError("SoundManager::Init()", "Mix_OpenAudio failed.", Mix_GetError());
            return false;
        }
    }

    // Retained for compatibility with additional codecs
    (void)Mix_Init(0);

    // Copy configuration into the class PRIVATE MEMBERS
    mSfxChannels = std::max(0, cfg.sfxChannels);
    mMasterVolume = std::clamp(cfg.masterVolume, 0.0f, 1.0f);
    mEnableDucking = cfg.enableDucking;
    mDuckVolume = std::clamp(cfg.duckVolume, 0.0f, 1.0f);
    mDuckAttackSec = std::max(0.0f, cfg.duckAttackSec);
    mDuckReleaseSec = std::max(0.0f, cfg.duckReleaseSec);

    // Allocate SFX channels and store the actual returned channel count in sfxChannels
    mSfxChannels = Mix_AllocateChannels(mSfxChannels);
    RT().chGain.assign(mSfxChannels, 1.0f);

    // Aplicamos volumen maestro
    Mix_Volume(-1, ToMixVol(mMasterVolume));    // all SFX channels
    Mix_VolumeMusic(ToMixVol(mMasterVolume));   // music

    auto& rt = RT();
    rt.musicVol01 = mMasterVolume;
    rt.musicGain = 1.f;
    rt.inited = true;

    return true;
}

void SoundManager::Shutdown() noexcept
{
    // Stop all sounds before closing the audio device
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
    RT() = Runtime{};
}

void SoundManager::Update() noexcept
{
    auto& rt = RT();

    // dt from TimeManager
    float dtSec = mTimeManager->deltaTime;

    const bool anySfx = Mix_Playing(-1) > 0;

    // Objetivo y constante de tiempo
    const float base = mEnableDucking && anySfx ? mDuckVolume : mMasterVolume;
    const float target = std::clamp(base * rt.musicGain, 0.0f, 1.0f);
    const float tau = mEnableDucking ? (anySfx ? mDuckAttackSec : mDuckReleaseSec) : 0.0f;

    float next = target;
    if (tau > 0.0f) {
        const float alpha = 1.0f - std::exp(-dtSec / tau); // time-based exponential smoothing
        next = Lerp(rt.musicVol01, target, alpha);
    }
    rt.musicVol01 = next;

    // Apply smoothed volume to music
    Mix_VolumeMusic(ToMixVol(rt.musicVol01));

    ReapplySFXVolumes(mMasterVolume);
}

void SoundManager::SetMasterVolume(float v01) noexcept
{
    mMasterVolume = std::clamp(v01, 0.0f, 1.0f);
    Mix_Volume(-1, ToMixVol(mMasterVolume)); // SFX

    // Without ducking, synchronize music immediately.
    if (!mEnableDucking) {
        RT().musicVol01 = mMasterVolume * RT().musicGain;
        Mix_VolumeMusic(ToMixVol(RT().musicVol01));
    }

    ReapplySFXVolumes(mMasterVolume);
}

float SoundManager::MasterVolume() const noexcept
{
	return mMasterVolume;
}

bool SoundManager::PlayMusic(const Music* music, int loops, float gain01) noexcept
{
    if (!music) return false;
    Mix_Music* mm = music->GetSDL();
    if (!mm) return false;

    if (Mix_PlayMusic(mm, loops) != 0) {
        LogError("SoundManager::PlayMusic()", "Mix_PlayMusic failed.", Mix_GetError());
        return false;
    }

    RT().musicGain = std::clamp(gain01, 0.0f, 1.0f);
    // Apply a reasonable value immediately; Update() smooths it later.
    const bool anySfx = Mix_Playing(-1) > 0;
    const float targetBase = mEnableDucking && anySfx ? mDuckVolume : mMasterVolume;
    const float target = std::clamp(targetBase * RT().musicGain, 0.0f, 1.0f);
    RT().musicVol01 = target;
    Mix_VolumeMusic(ToMixVol(RT().musicVol01));
    return true;
}

void SoundManager::StopMusic() noexcept
{
    Mix_HaltMusic();
}

bool SoundManager::IsMusicPlaying() const noexcept
{
    return Mix_PlayingMusic() == 1;
}

int SoundManager::PlaySFX(const SoundEffect* sfx, int loops, float gain01) noexcept
{
    if (!sfx) return -1;
    Mix_Chunk* ch = sfx->GetSDL();
    if (!ch) return -1;

    const int channel = Mix_PlayChannel(-1, ch, loops);
    if (channel == -1) {
        LogError("SoundManager::PlaySFX()", "Mix_PlayChannel failed.", Mix_GetError());
        return -1;
    }

    auto& rt = RT(); // where chGain[channel] is stored if used
    if (channel >= 0 && channel < (int)rt.chGain.size())
        rt.chGain[channel] = std::clamp(gain01, 0.0f, 1.0f);

    // Simple mix: master * one-shot gain (SFX are short-lived).
    const float v = std::clamp(mMasterVolume * rt.chGain[channel], 0.0f, 1.0f);
    Mix_Volume(channel, ToMixVol(v));
    return channel;
}

void SoundManager::StopAllSFX() noexcept
{
    Mix_HaltChannel(-1);
}

void SoundManager::StopSFXChannel(int channel) noexcept
{
    if (channel >= 0) Mix_HaltChannel(channel);
}

bool SoundManager::IsAnySFXPlaying() const noexcept
{
    return Mix_Playing(-1) > 0;
}

void SoundManager::SetSFXChannelCount(int count) noexcept
{
    StopAllSFX();
    mSfxChannels = Mix_AllocateChannels(std::max(0, count));
    RT().chGain.assign(mSfxChannels, 1.0f);
    // Reapply master volume to all newly (re)allocated SFX channels.
    Mix_Volume(-1, ToMixVol(mMasterVolume));
}

int SoundManager::SFXChannelCount() const noexcept
{
    return mSfxChannels;
}

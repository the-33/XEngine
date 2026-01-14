#include "SoundManager.h"

#include <algorithm>
#include <cmath>

#include "TimeManager.h"
#include "ErrorHandler.h"

static inline int ToMixVol(float v01) // Usa una escala logaritmica para mejorar la percepcion del volumen
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
    std::vector<float> chGain;   // ganancia por canal SFX (0..1)
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

    // Abrimos el dispositivo de audio (valores típicos)
    const int frequency = 44100;
    const Uint16 format = MIX_DEFAULT_FORMAT;
    const int channels = 2; // Stereo
    const int chunksize = 2048;

    if (Mix_OpenAudio(frequency, format, channels, chunksize) != 0) {
        LogError("SoundManager::Init()", "Mix_OpenAudio failed.", Mix_GetError());
        return false;
    }

    // En el caso de usar codecs extra (Dejar por compatibilidad)
    (void)Mix_Init(0);

    // Copiamos configuración a los ATRIBUTOS PRIVADOS de la clase
    mSfxChannels = std::max(0, cfg.sfxChannels);
    mMasterVolume = std::clamp(cfg.masterVolume, 0.0f, 1.0f);
    mEnableDucking = cfg.enableDucking;
    mDuckVolume = std::clamp(cfg.duckVolume, 0.0f, 1.0f);
    mDuckAttackSec = std::max(0.0f, cfg.duckAttackSec);
    mDuckReleaseSec = std::max(0.0f, cfg.duckReleaseSec);

    // Reservamos canales SFX y guardamos el valor real devuelto en sfxChannels
    mSfxChannels = Mix_AllocateChannels(mSfxChannels);
    RT().chGain.assign(mSfxChannels, 1.0f);

    // Aplicamos volumen maestro
    Mix_Volume(-1, ToMixVol(mMasterVolume));    // todos los SFX
    Mix_VolumeMusic(ToMixVol(mMasterVolume));   // música

    auto& rt = RT();
    rt.musicVol01 = mMasterVolume;
    rt.musicGain = 1.f;
    rt.inited = true;

    return true;
}

void SoundManager::Shutdown() noexcept
{
    // Paramos todo antes de cerrar audio
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
    RT() = Runtime{};
}

void SoundManager::Update() noexcept
{
    auto& rt = RT();

    // dt desde tu TimeManager
    float dtSec = mTimeManager->deltaTime;

    const bool anySfx = Mix_Playing(-1) > 0;

    // Objetivo y constante de tiempo
    const float base = mEnableDucking && anySfx ? mDuckVolume : mMasterVolume;
    const float target = std::clamp(base * rt.musicGain, 0.0f, 1.0f);
    const float tau = mEnableDucking ? (anySfx ? mDuckAttackSec : mDuckReleaseSec) : 0.0f;

    float next = target;
    if (tau > 0.0f) {
        const float alpha = 1.0f - std::exp(-dtSec / tau); // suavizado exponencial por tiempo
        next = Lerp(rt.musicVol01, target, alpha);
    }
    rt.musicVol01 = next;

    // Aplica el volumen suavizado a la música
    Mix_VolumeMusic(ToMixVol(rt.musicVol01));

    ReapplySFXVolumes(mMasterVolume);
}

void SoundManager::SetMasterVolume(float v01) noexcept
{
    mMasterVolume = std::clamp(v01, 0.0f, 1.0f);
    Mix_Volume(-1, ToMixVol(mMasterVolume)); // SFX

    // Si no hay ducking, sincroniza música al instante.
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
    // Aplica inmediatamente algo razonable; Update() lo suaviza después.
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

    auto& rt = RT(); // donde guardas chGain[channel] si lo usas
    if (channel >= 0 && channel < (int)rt.chGain.size())
        rt.chGain[channel] = std::clamp(gain01, 0.0f, 1.0f);

    // Composición simple: master * ganancia puntual. (SFX son de vida corta.)
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
    // Reaplicamos volumen maestro a todos los canales SFX recién (re)alocados.
    Mix_Volume(-1, ToMixVol(mMasterVolume));
}

int SoundManager::SFXChannelCount() const noexcept
{
    return mSfxChannels;
}

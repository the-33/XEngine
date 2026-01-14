#pragma once

#include <unordered_map>
#include <memory>
#include <string>

#include "Singleton.h"
#include "Assets.h"
#include "Engine.h"

class AssetManager final : public Singleton<AssetManager>
{
	friend class Singleton<AssetManager>;
	friend class Engine;

private:
	// caches por ruta (o por clave lógica)
	std::unordered_map<std::string, std::unique_ptr<Texture>>    mTextures;
	std::unordered_map<std::string, std::unique_ptr<Font>>       mFonts;
	std::unordered_map<std::string, std::unique_ptr<SoundEffect>> mSfx;
	std::unordered_map<std::string, std::unique_ptr<Music>>       mMusic;

	std::string basePath = "";

	double memoryUsed = 0;

	AssetManager() = default;
	~AssetManager() = default;
	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;

	// Inicialización (para setear paths base, etc.)
	bool Init(const std::string& assetsFolderPath) noexcept;
	void Shutdown() noexcept;

	Font* GetEngineDefaultFont(bool bold) noexcept;

public:
	// TEXTURAS
	Texture* LoadTexture(const std::string& path, float pixelsPerUnit = 100.f) noexcept;
	Texture* LoadTexture(const std::string& path, const std::string& key, float pixelsPerUnit = 100.f) noexcept;
	Texture* GetTexture(const std::string& path) const noexcept;
	Texture* GetTextureByKey(const std::string& key) const noexcept;

	// FUENTES
	Font* LoadFont(const std::string& path, int ptSize) noexcept;
	Font* LoadFont(const std::string& path, const std::string& key, int ptSize) noexcept;
	Font* GetFont(const std::string& path, int ptSize) const noexcept;
	Font* GetFontByKey(const std::string& key) const noexcept;

	// SONIDO (SFX)
	SoundEffect* LoadSFX(const std::string& path) noexcept;
	SoundEffect* LoadSFX(const std::string& path, const std::string& key) noexcept;
	SoundEffect* GetSFX(const std::string& path) const noexcept;
	SoundEffect* GetSFXByKey(const std::string& key) const noexcept;

	// MÚSICA (stream)
	Music* LoadMusic(const std::string& path) noexcept;
	Music* LoadMusic(const std::string& path, const std::string& key) noexcept;
	Music* GetMusic(const std::string& path) const noexcept;
	Music* GetMusicByKey(const std::string& key) const noexcept;
};
#pragma once
/** @file AssetManager.h
 * @brief Cached texture, font, sound-effect and music loading.
 */

#include <unordered_map>
#include <memory>
#include <string>

#include "Singleton.h"
#include "Assets.h"
#include "Engine.h"

/** @brief Loads and caches textures, fonts, sound effects and music assets.
 * @ingroup xengine_services
 */
class AssetManager final : public Singleton<AssetManager>
{
	friend class Singleton<AssetManager>;
	friend class Engine;

private:
	// caches by path (or logical key)
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

	// Initialization (sets base paths, etc.)
	bool Init(const std::string& assetsFolderPath) noexcept;
	void Shutdown() noexcept;

	Font* GetEngineDefaultFont(bool bold) noexcept;

public:
	// TEXTURAS
	/** @brief Loads/caches a texture; optional key and pixels-per-unit control how it is accessed and projected.
	 * @param path Resource path relative to the game assets directory.
	 * @param pixelsPerUnit Texture pixels represented by one world-space unit.
	 */
	Texture* LoadTexture(const std::string& path, float pixelsPerUnit = 100.f) noexcept;
	/** @brief Loads/caches a texture; optional key and pixels-per-unit control how it is accessed and projected.
	 * @param path Resource path relative to the game assets directory.
	 * @param key Application-defined lookup key.
	 * @param pixelsPerUnit Texture pixels represented by one world-space unit.
	 */
	Texture* LoadTexture(const std::string& path, const std::string& key, float pixelsPerUnit = 100.f) noexcept;
	/** @brief Returns the texture of AssetManager.
	 * @param path Resource path relative to the game assets directory.
	 */
	Texture* GetTexture(const std::string& path) const noexcept;
	/** @brief Returns the texture by key of AssetManager.
	 * @param key Application-defined lookup key.
	 */
	Texture* GetTextureByKey(const std::string& key) const noexcept;

	// FUENTES
	/** @brief Loads/caches a font of the requested point size, optionally under a logical key.
	 * @param path Resource path relative to the game assets directory.
	 * @param ptSize Font point size.
	 */
	Font* LoadFont(const std::string& path, int ptSize) noexcept;
	/** @brief Loads/caches a font of the requested point size, optionally under a logical key.
	 * @param path Resource path relative to the game assets directory.
	 * @param key Application-defined lookup key.
	 * @param ptSize Font point size.
	 */
	Font* LoadFont(const std::string& path, const std::string& key, int ptSize) noexcept;
	/** @brief Returns the font of AssetManager.
	 * @param path Resource path relative to the game assets directory.
	 * @param ptSize Font point size.
	 */
	Font* GetFont(const std::string& path, int ptSize) const noexcept;
	/** @brief Returns the font by key of AssetManager.
	 * @param key Application-defined lookup key.
	 */
	Font* GetFontByKey(const std::string& key) const noexcept;

	// SONIDO (SFX)
	/** @brief Loads/caches an SDL_mixer sound effect from the assets directory.
	 * @param path Resource path relative to the game assets directory.
	 */
	SoundEffect* LoadSFX(const std::string& path) noexcept;
	/** @brief Loads/caches an SDL_mixer sound effect from the assets directory.
	 * @param path Resource path relative to the game assets directory.
	 * @param key Application-defined lookup key.
	 */
	SoundEffect* LoadSFX(const std::string& path, const std::string& key) noexcept;
	/** @brief Returns the sfx of AssetManager.
	 * @param path Resource path relative to the game assets directory.
	 */
	SoundEffect* GetSFX(const std::string& path) const noexcept;
	/** @brief Finds a previously loaded sound effect by its resource key.
	 * @param key Application-defined lookup key.
	 */
	SoundEffect* GetSFXByKey(const std::string& key) const noexcept;

	// MUSIC (stream)
	/** @brief Loads/caches streamed music from the assets directory.
	 * @param path Resource path relative to the game assets directory.
	 */
	Music* LoadMusic(const std::string& path) noexcept;
	/** @brief Loads/caches streamed music from the assets directory.
	 * @param path Resource path relative to the game assets directory.
	 * @param key Application-defined lookup key.
	 */
	Music* LoadMusic(const std::string& path, const std::string& key) noexcept;
	/** @brief Returns the music of AssetManager.
	 * @param path Resource path relative to the game assets directory.
	 */
	Music* GetMusic(const std::string& path) const noexcept;
	/** @brief Returns the music by key of AssetManager.
	 * @param key Application-defined lookup key.
	 */
	Music* GetMusicByKey(const std::string& key) const noexcept;
};
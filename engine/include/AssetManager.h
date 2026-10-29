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

/** @brief Loads and owns cached textures, fonts, sound effects and music.
 *
 * Paths are relative to the configured assets root. Returned pointers are
 * non-owning and become invalid on cache shutdown. Loading may return nullptr
 * when an asset is missing, invalid, or its SDL subsystem is unavailable.
 * A resource is owned once under its canonical path (and font size for fonts).
 * Explicit keys are aliases to that resource: keyed and path-based lookups
 * return the same pointer. Reusing a key for a different asset is rejected.
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
	// Logical-key aliases point to canonical cache keys, never owning duplicates.
	std::unordered_map<std::string, std::string> mTextureAliases;
	std::unordered_map<std::string, std::string> mFontAliases;
	std::unordered_map<std::string, std::string> mSfxAliases;
	std::unordered_map<std::string, std::string> mMusicAliases;

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
	/** @brief Loads or returns a cached texture under its relative path.
	 * @param path Resource path relative to the game assets directory.
	 * @param pixelsPerUnit Texture pixels per world unit; values <= 0 reset to 100.
	 * @return Cached non-owning pointer, or nullptr if loading fails.
	 */
	Texture* LoadTexture(const std::string& path, float pixelsPerUnit = 100.f) noexcept;
	/** @brief Loads/reuses a texture and registers a custom lookup alias.
	 * @param path Resource path relative to the game assets directory.
	 * @param key Application-defined lookup key.
	 * @param pixelsPerUnit Texture pixels represented by one world-space unit.
	 * @return The same texture as LoadTexture(path), or nullptr if loading fails
	 *         or `key` already identifies a different texture.
	 */
	Texture* LoadTexture(const std::string& path, const std::string& key, float pixelsPerUnit = 100.f) noexcept;
	/** @brief Finds a texture previously cached using its resource path.
	 * @param path Resource path relative to the game assets directory.
	 */
	Texture* GetTexture(const std::string& path) const noexcept;
	/** @brief Finds a texture stored under a custom key; returns nullptr if missing.
	 * @param key Application-defined lookup key.
	 */
	Texture* GetTextureByKey(const std::string& key) const noexcept;

	// FUENTES
	/** @brief Loads/caches a font of the requested point size, optionally under a logical key.
	 * @param path Resource path relative to the game assets directory.
	 * @param ptSize Font point size.
	 */
	Font* LoadFont(const std::string& path, int ptSize) noexcept;
	/** @brief Loads/reuses a font and assigns an alias for its path/size pair.
	 * @param path Resource path relative to the game assets directory.
	 * @param key Application-defined lookup key.
	 * @param ptSize Font point size.
	 * @return Existing or new font; nullptr if the key was bound to another
	 *         font or the requested font could not be loaded.
	 */
	Font* LoadFont(const std::string& path, const std::string& key, int ptSize) noexcept;
	/** @brief Finds a font loaded by path and point size, or nullptr.
	 * @param path Resource path relative to the game assets directory.
	 * @param ptSize Font point size.
	 */
	Font* GetFont(const std::string& path, int ptSize) const noexcept;
	/** @brief Finds a font stored under a custom key, or nullptr.
	 * @param key Application-defined lookup key.
	 */
	Font* GetFontByKey(const std::string& key) const noexcept;

	// SONIDO (SFX)
	/** @brief Loads/caches an SDL_mixer sound effect from the assets directory.
	 * @param path Resource path relative to the game assets directory.
	 */
	SoundEffect* LoadSFX(const std::string& path) noexcept;
	/** @brief Loads/reuses an SDL_mixer effect and assigns a logical key alias.
	 * @param path Resource path relative to the game assets directory.
	 * @param key Application-defined lookup key.
	 * @return Cached effect, or nullptr on failure/key conflict.
	 */
	SoundEffect* LoadSFX(const std::string& path, const std::string& key) noexcept;
	/** @brief Finds a sound effect previously loaded by its relative path.
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
	/** @brief Loads/reuses streamed music and assigns a logical key alias.
	 * @param path Resource path relative to the game assets directory.
	 * @param key Application-defined lookup key.
	 * @return Cached music, or nullptr on failure/key conflict.
	 */
	Music* LoadMusic(const std::string& path, const std::string& key) noexcept;
	/** @brief Finds music previously loaded by its relative path.
	 * @param path Resource path relative to the game assets directory.
	 */
	Music* GetMusic(const std::string& path) const noexcept;
	/** @brief Finds music stored under a custom key, or nullptr.
	 * @param key Application-defined lookup key.
	 */
	Music* GetMusicByKey(const std::string& key) const noexcept;
};

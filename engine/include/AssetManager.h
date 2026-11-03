#pragma once
/** @file AssetManager.h
 * @brief Cached texture, font, sound-effect and music loading.
 */

#include <unordered_map>
#include <memory>
#include <string>

#include "Singleton.h"
#include "Assets.h"
#include "Animation.h"
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
	/** @brief Permit the singleton base to construct and release the AssetManager manager.
	 */
	friend class Singleton<AssetManager>;
	/** @brief Grant Engine access to the internal state of AssetManager.
	 */
	friend class Engine;

private:
	// caches by path (or logical key)
	/** @brief Stored textures value used by AssetManager.
	 */
	std::unordered_map<std::string, std::unique_ptr<Texture>>    mTextures;
	/** @brief Stored fonts value used by AssetManager.
	 */
	std::unordered_map<std::string, std::unique_ptr<Font>>       mFonts;
	/** @brief Stored sfx value used by AssetManager.
	 */
	std::unordered_map<std::string, std::unique_ptr<SoundEffect>> mSfx;
	/** @brief Stored music value used by AssetManager.
	 */
	std::unordered_map<std::string, std::unique_ptr<Music>>       mMusic;
	/** @brief Stored animations value used by AssetManager.
	 */
	std::unordered_map<std::string, std::unique_ptr<AnimationClip>> mAnimations;
	/** @brief Stored animator controllers value used by AssetManager.
	 */
	std::unordered_map<std::string, std::unique_ptr<AnimatorController>> mAnimatorControllers;
	// Logical-key aliases point to canonical cache keys, never owning duplicates.
	/** @brief Stored texture aliases value used by AssetManager.
	 */
	std::unordered_map<std::string, std::string> mTextureAliases;
	/** @brief Stored font aliases value used by AssetManager.
	 */
	std::unordered_map<std::string, std::string> mFontAliases;
	/** @brief Stored sfx aliases value used by AssetManager.
	 */
	std::unordered_map<std::string, std::string> mSfxAliases;
	/** @brief Stored music aliases value used by AssetManager.
	 */
	std::unordered_map<std::string, std::string> mMusicAliases;

	/** @brief Configured base path value used by AssetManager.
	 */
	std::string basePath = "";

	/** @brief Stored memory used value used by AssetManager.
	 */
	double memoryUsed = 0;

	/** @brief Construct a AssetManager instance.
	 */
	AssetManager() = default;
	/** @brief Destroy a AssetManager instance and release owned resources.
	 */
	~AssetManager() = default;
	/** @brief Construct a AssetManager instance.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	AssetManager(const AssetManager& other) = delete;
	/** @brief Implement the operator= operation for AssetManager.
	 * @return Result of applying the overloaded operator.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	AssetManager& operator=(const AssetManager& other) = delete;

	// Initialization (sets base paths, etc.)
	/** @brief Initialize the AssetManager subsystem.
	 * @param assetsFolderPath Filesystem assets folder path used by the operation.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool Init(const std::string& assetsFolderPath) noexcept;
	/** @brief Release resources managed by AssetManager.
	 */
	void Shutdown() noexcept;

	/** @brief Retrieve the engine default font from AssetManager.
	 * @param bold Bold supplied to AssetManager.
	 * @return Requested get engine default font value or reference.
	 */
	Font* GetEngineDefaultFont(bool bold) noexcept;

public:
	/** @brief Load and cache a validated JSON AnimationClip by assets-relative path.
	 * @param path Path relative to the initialized assets root.
	 * @return Shared immutable clip, or nullptr on malformed/missing input.
	 */
	const AnimationClip* LoadAnimation(const std::string& path) noexcept;
	/** @brief Load and cache a validated AnimatorController by assets-relative path.
	 * @param path Path relative to the initialized assets root.
	 * @return Shared immutable controller, or nullptr on malformed/missing input.
	 */
	const AnimatorController* LoadAnimatorController(const std::string& path) noexcept;
	/** @brief Locate a cached texture's original assets-relative path, if known.
	 * @param texture Non-owning texture pointer.
	 * @return Relative path or empty string for an unknown texture.
	 */
	std::string FindTexturePath(const Texture* texture) const;
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
	 * @return Requested get texture value or reference.
	 */
	Texture* GetTexture(const std::string& path) const noexcept;
	/** @brief Finds a texture stored under a custom key; returns nullptr if missing.
	 * @param key Application-defined lookup key.
	 * @return Requested get texture by key value or reference.
	 */
	Texture* GetTextureByKey(const std::string& key) const noexcept;

	// FUENTES
	/** @brief Loads/caches a font of the requested point size, optionally under a logical key.
	 * @param path Resource path relative to the game assets directory.
	 * @param ptSize Font point size.
	 * @return Result of load font in AssetManager.
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
	 * @return Requested get font value or reference.
	 */
	Font* GetFont(const std::string& path, int ptSize) const noexcept;
	/** @brief Finds a font stored under a custom key, or nullptr.
	 * @param key Application-defined lookup key.
	 * @return Requested get font by key value or reference.
	 */
	Font* GetFontByKey(const std::string& key) const noexcept;

	// SONIDO (SFX)
	/** @brief Loads/caches an SDL_mixer sound effect from the assets directory.
	 * @param path Resource path relative to the game assets directory.
	 * @return Result of load sfx in AssetManager.
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
	 * @return Requested get sfx value or reference.
	 */
	SoundEffect* GetSFX(const std::string& path) const noexcept;
	/** @brief Finds a previously loaded sound effect by its resource key.
	 * @param key Application-defined lookup key.
	 * @return Requested get sfx by key value or reference.
	 */
	SoundEffect* GetSFXByKey(const std::string& key) const noexcept;

	// MUSIC (stream)
	/** @brief Loads/caches streamed music from the assets directory.
	 * @param path Resource path relative to the game assets directory.
	 * @return Result of load music in AssetManager.
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
	 * @return Requested get music value or reference.
	 */
	Music* GetMusic(const std::string& path) const noexcept;
	/** @brief Finds music stored under a custom key, or nullptr.
	 * @param key Application-defined lookup key.
	 * @return Requested get music by key value or reference.
	 */
	Music* GetMusicByKey(const std::string& key) const noexcept;
};

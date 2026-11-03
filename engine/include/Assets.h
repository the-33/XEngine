#pragma once
/** @file Assets.h
 * @brief Move-only wrappers for SDL texture, font and audio resources.
 */

// Use SDL's own declarations for opaque resource types. The internal struct
// tags for TTF_Font and Mix_Music differ across SDL2 versions/platforms.
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"
#include <string>

// =========================
// Texture
// =========================

/** @brief Move-only SDL texture wrapper; exposes dimensions and pixels-per-unit metadata.
 * @ingroup xengine_services
 */
class Texture
{
public:
	/** @brief Constructs a Texture instance.
	 */
	Texture() = default;
	/** @brief Releases the Texture instance and any owned resources.
	 */
	~Texture() noexcept;                     // libera SDL_Texture*
	/** @brief Constructs a Texture instance.
	 * @param other Other supplied to Texture.
	 */
	Texture(Texture&& other) noexcept;    // move-only
	/** @brief Implements operator= for this value or property type.
	 * @param other Other supplied to Texture.
	 * @return Result of applying the overloaded operator.
	 */
	Texture& operator=(Texture&& other) noexcept;
	/** @brief Constructs a Texture instance.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	Texture(const Texture& other) = delete;
	/** @brief Implements operator= for this value or property type.
	 * @return Result of applying the overloaded operator.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	Texture& operator=(const Texture& other) = delete;

	// Internal access (RenderManager only)
	/** @brief Returns the underlying SDL_Texture handle without transferring ownership.
	 * @return The underlying sdl_texture handle without transferring ownership.
	 */
	SDL_Texture* GetSDL() const noexcept;

	// Useful metadata (filled in during loading with SDL_QueryTexture / SDL_image)
	/** @brief Returns the resource width, in pixels.
	 * @return The resource width, in pixels.
	 */
	int  Width()  const noexcept;
	/** @brief Returns the resource height, in pixels.
	 * @return The resource height, in pixels.
	 */
	int  Height() const noexcept;

	/** @brief Returns the texture's pixels-per-world-unit scale.
	 * @return The texture's pixels-per-world-unit scale.
	 */
	float PixelsPerUnit() const noexcept { return pixelsPerUnit; }
	/** @brief Sets texture scale metadata, falling back to 100 for nonpositive input.
	 * @param ppu Pixels per world unit.
	 */
	void  SetPixelsPerUnit(float ppu) noexcept { pixelsPerUnit = (ppu > 0.f ? ppu : 100.f); }

private:
	/** @brief Grant AssetManager access to the internal state of Texture.
	 */
	friend class AssetManager;   // assign the pointer when loading
	/** @brief Grant RenderManager access to the internal state of Texture.
	 */
	friend class RenderManager;  // for drawing
	/** @brief Stored texture value used by Texture.
	 */
	SDL_Texture* mTexture = nullptr;
	/** @brief Stored w value used by Texture.
	 */
	int mW = 0, mH = 0;
	/** @brief Configured pixels per unit value used by Texture.
	 */
	float pixelsPerUnit = 100.f;
};

// =========================
// Font
// =========================

/** @brief Move-only SDL_ttf font wrapper owned by the asset cache.
 * @ingroup xengine_services
 */
class Font
{
public:
	/** @brief Constructs a Font instance.
	 */
	Font() = default;
	/** @brief Releases the Font instance and any owned resources.
	 */
	~Font() noexcept;                      // libera TTF_Font*
	/** @brief Constructs a Font instance.
	 * @param other Other supplied to Font.
	 */
	Font(Font&& other) noexcept;
	/** @brief Implements operator= for this value or property type.
	 * @param other Other supplied to Font.
	 * @return Result of applying the overloaded operator.
	 */
	Font& operator=(Font&& other) noexcept;
	/** @brief Constructs a Font instance.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	Font(const Font& other) = delete;
	/** @brief Implements operator= for this value or property type.
	 * @return Result of applying the overloaded operator.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	Font& operator=(const Font& other) = delete;

	/** @brief Returns the underlying TTF_Font handle without transferring ownership.
	 * @return The underlying ttf_font handle without transferring ownership.
	 */
	TTF_Font* GetSDL() const noexcept;

private:
	/** @brief Grant AssetManager access to the internal state of Font.
	 */
	friend class AssetManager;
	/** @brief Grant RenderManager access to the internal state of Font.
	 */
	friend class RenderManager;
	/** @brief Stored font value used by Font.
	 */
	TTF_Font* mFont = nullptr;
	/** @brief Stored point size value used by Font.
	 */
	int mPointSize = 0;
	std::string mSourcePath; ///< Source file for crisp screen-sized font variants.
};

// =========================
// SoundEffect (SFX)
// =========================

/** @brief Move-only SDL_mixer sound effect resource.
 * @ingroup xengine_services
 */
class SoundEffect
{
public:
	/** @brief Constructs a SoundEffect instance.
	 */
	SoundEffect() = default;
	/** @brief Releases the SoundEffect instance and any owned resources.
	 */
	~SoundEffect() noexcept;                         // libera Mix_Chunk*
	/** @brief Constructs a SoundEffect instance.
	 * @param other Other supplied to SoundEffect.
	 */
	SoundEffect(SoundEffect&& other) noexcept;
	/** @brief Implements operator= for this value or property type.
	 * @param other Other supplied to SoundEffect.
	 * @return Result of applying the overloaded operator.
	 */
	SoundEffect& operator=(SoundEffect&& other) noexcept;
	/** @brief Constructs a SoundEffect instance.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	SoundEffect(const SoundEffect& other) = delete;
	/** @brief Implements operator= for this value or property type.
	 * @return Result of applying the overloaded operator.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	SoundEffect& operator=(const SoundEffect& other) = delete;

	/** @brief Returns the underlying Mix_Chunk handle without transferring ownership.
	 * @return The underlying mix_chunk handle without transferring ownership.
	 */
	Mix_Chunk* GetSDL() const noexcept;

private:
	/** @brief Grant AssetManager access to the internal state of SoundEffect.
	 */
	friend class AssetManager;
	/** @brief Grant SoundManager access to the internal state of SoundEffect.
	 */
	friend class SoundManager;
	/** @brief Stored chunk value used by SoundEffect.
	 */
	Mix_Chunk* mChunk = nullptr;
};

// =========================
// Music
// =========================

/** @brief Move-only streamed music resource managed by SDL_mixer.
 * @ingroup xengine_services
 */
class Music
{
public:
	/** @brief Constructs a Music instance.
	 */
	Music() = default;
	/** @brief Releases the Music instance and any owned resources.
	 */
	~Music() noexcept;                               // libera Mix_Music*
	/** @brief Constructs a Music instance.
	 * @param other Other supplied to Music.
	 */
	Music(Music&& other) noexcept;
	/** @brief Implements operator= for this value or property type.
	 * @param other Other supplied to Music.
	 * @return Result of applying the overloaded operator.
	 */
	Music& operator=(Music&& other) noexcept;
	/** @brief Constructs a Music instance.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	Music(const Music& other) = delete;
	/** @brief Implements operator= for this value or property type.
	 * @return Result of applying the overloaded operator.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	Music& operator=(const Music& other) = delete;

	/** @brief Returns the underlying Mix_Music handle without transferring ownership.
	 * @return The underlying mix_music handle without transferring ownership.
	 */
	Mix_Music* GetSDL() const noexcept;

private:
	/** @brief Grant AssetManager access to the internal state of Music.
	 */
	friend class AssetManager;
	/** @brief Grant SoundManager access to the internal state of Music.
	 */
	friend class SoundManager;
	/** @brief Stored music value used by Music.
	 */
	Mix_Music* mMusic = nullptr;
};

/** @name Co-declared engine member descriptions
 * @brief Members sharing declarations with other fields.
 * @{ */
/** @var int Texture::mH
 * @brief Stored h state owned by Texture.
 */
/** @} */

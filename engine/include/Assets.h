#pragma once
/** @file Assets.h
 * @brief Move-only wrappers for SDL texture, font and audio resources.
 */

// Use SDL's own declarations for opaque resource types. The internal struct
// tags for TTF_Font and Mix_Music differ across SDL2 versions/platforms.
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"

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
	 */
	Texture(Texture&&) noexcept;    // move-only
	/** @brief Implements operator= for this value or property type.
	 */
	Texture& operator=(Texture&&) noexcept;
	/** @brief Constructs a Texture instance.
	 */
	Texture(const Texture&) = delete;
	/** @brief Implements operator= for this value or property type.
	 */
	Texture& operator=(const Texture&) = delete;

	// Internal access (RenderManager only)
	/** @brief Returns the sdl of Texture.
	 */
	SDL_Texture* GetSDL() const noexcept;

	// Useful metadata (filled in during loading with SDL_QueryTexture / SDL_image)
	/** @brief Returns the resource width, in pixels.
	 */
	int  Width()  const noexcept;
	/** @brief Returns the resource height, in pixels.
	 */
	int  Height() const noexcept;

	/** @brief Returns or updates the texture’s pixels-per-world-unit scale.
	 */
	float PixelsPerUnit() const noexcept { return pixelsPerUnit; }
	/** @brief Changes the pixels per unit of Texture.
	 * @param ppu Pixels per world unit.
	 */
	void  SetPixelsPerUnit(float ppu) noexcept { pixelsPerUnit = (ppu > 0.f ? ppu : 100.f); }

private:
	friend class AssetManager;   // assign the pointer when loading
	friend class RenderManager;  // for drawing
	SDL_Texture* mTexture = nullptr;
	int mW = 0, mH = 0;
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
	 */
	Font(Font&&) noexcept;
	/** @brief Implements operator= for this value or property type.
	 */
	Font& operator=(Font&&) noexcept;
	/** @brief Constructs a Font instance.
	 */
	Font(const Font&) = delete;
	/** @brief Implements operator= for this value or property type.
	 */
	Font& operator=(const Font&) = delete;

	/** @brief Returns the sdl of Font.
	 */
	TTF_Font* GetSDL() const noexcept;

private:
	friend class AssetManager;
	TTF_Font* mFont = nullptr;
	int mPointSize = 0;
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
	 */
	SoundEffect(SoundEffect&&) noexcept;
	/** @brief Implements operator= for this value or property type.
	 */
	SoundEffect& operator=(SoundEffect&&) noexcept;
	/** @brief Constructs a SoundEffect instance.
	 */
	SoundEffect(const SoundEffect&) = delete;
	/** @brief Implements operator= for this value or property type.
	 */
	SoundEffect& operator=(const SoundEffect&) = delete;

	/** @brief Returns the sdl of SoundEffect.
	 */
	Mix_Chunk* GetSDL() const noexcept;

private:
	friend class AssetManager;
	friend class SoundManager;
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
	 */
	Music(Music&&) noexcept;
	/** @brief Implements operator= for this value or property type.
	 */
	Music& operator=(Music&&) noexcept;
	/** @brief Constructs a Music instance.
	 */
	Music(const Music&) = delete;
	/** @brief Implements operator= for this value or property type.
	 */
	Music& operator=(const Music&) = delete;

	/** @brief Returns the sdl of Music.
	 */
	Mix_Music* GetSDL() const noexcept;

private:
	friend class AssetManager;
	friend class SoundManager;
	Mix_Music* mMusic = nullptr;
};
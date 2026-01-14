#pragma once

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"

// =========================
// Texture
// =========================

class Texture
{
public:
	Texture() = default;
	~Texture() noexcept;                     // libera SDL_Texture*
	Texture(Texture&&) noexcept;    // move-only
	Texture& operator=(Texture&&) noexcept;
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	// Acceso interno (solo para RenderManager)
	SDL_Texture* GetSDL() const noexcept;

	// Metadatos útiles (rellenar al cargar con SDL_QueryTexture / SDL_image)
	int  Width()  const noexcept;
	int  Height() const noexcept;

	float PixelsPerUnit() const noexcept { return pixelsPerUnit; }
	void  SetPixelsPerUnit(float ppu) noexcept { pixelsPerUnit = (ppu > 0.f ? ppu : 100.f); }

private:
	friend class AssetManager;   // para asignar el puntero al cargar
	friend class RenderManager;  // para dibujar
	SDL_Texture* mTexture = nullptr;
	int mW = 0, mH = 0;
	float pixelsPerUnit = 100.f;
};

// =========================
// Font
// =========================

class Font
{
public:
	Font() = default;
	~Font() noexcept;                      // libera TTF_Font*
	Font(Font&&) noexcept;
	Font& operator=(Font&&) noexcept;
	Font(const Font&) = delete;
	Font& operator=(const Font&) = delete;

	TTF_Font* GetSDL() const noexcept;

private:
	friend class AssetManager;
	TTF_Font* mFont = nullptr;
	int mPointSize = 0;          // opcional, por si quieres guardarlo
};

// =========================
// SoundEffect (SFX)
// =========================

class SoundEffect
{
public:
	SoundEffect() = default;
	~SoundEffect() noexcept;                         // libera Mix_Chunk*
	SoundEffect(SoundEffect&&) noexcept;
	SoundEffect& operator=(SoundEffect&&) noexcept;
	SoundEffect(const SoundEffect&) = delete;
	SoundEffect& operator=(const SoundEffect&) = delete;

	Mix_Chunk* GetSDL() const noexcept;

private:
	friend class AssetManager;
	friend class SoundManager;
	Mix_Chunk* mChunk = nullptr;
};

// =========================
// Music
// =========================

class Music
{
public:
	Music() = default;
	~Music() noexcept;                               // libera Mix_Music*
	Music(Music&&) noexcept;
	Music& operator=(Music&&) noexcept;
	Music(const Music&) = delete;
	Music& operator=(const Music&) = delete;

	Mix_Music* GetSDL() const noexcept;

private:
	friend class AssetManager;
	friend class SoundManager;
	Mix_Music* mMusic = nullptr;
};
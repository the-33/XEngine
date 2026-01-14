#include "Assets.h"

// =========================
// Texture
// =========================

Texture::~Texture() noexcept
{
	if (mTexture) {
		SDL_DestroyTexture(mTexture);
		mTexture = nullptr;
	}
	mW = mH = 0;
}

Texture::Texture(Texture&& other) noexcept
{
	mTexture = other.mTexture;
	mW = other.mW;
	mH = other.mH;
	pixelsPerUnit = other.pixelsPerUnit;

	other.mTexture = nullptr;
	other.mW = other.mH = 0;
	other.pixelsPerUnit = 100.f;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
	if (this != &other) {
		// libera lo que tengamos
		if (mTexture) {
			SDL_DestroyTexture(mTexture);
		}
		// mueve
		mTexture = other.mTexture;
		mW = other.mW;
		mH = other.mH;
		pixelsPerUnit = other.pixelsPerUnit;

		// deja el origen en estado nulo
		other.mTexture = nullptr;
		other.mW = other.mH = 0;
		other.pixelsPerUnit = 100.f;
	}
	return *this;
}

SDL_Texture* Texture::GetSDL() const noexcept
{
	return mTexture;
}

int Texture::Width() const noexcept
{
	return mW;
}

int Texture::Height() const noexcept
{
	return mH;
}

// =========================
// Font
// =========================

Font::~Font() noexcept
{
	if (mFont) {
		TTF_CloseFont(mFont);
		mFont = nullptr;
	}
	mPointSize = 0;
}

Font::Font(Font&& other) noexcept
{
	mFont = other.mFont;
	mPointSize = other.mPointSize;

	other.mFont = nullptr;
	other.mPointSize = 0;
}

Font& Font::operator=(Font&& other) noexcept
{
	if (this != &other) {
		if (mFont) {
			TTF_CloseFont(mFont);
		}
		mFont = other.mFont;
		mPointSize = other.mPointSize;

		other.mFont = nullptr;
		other.mPointSize = 0;
	}
	return *this;
}

TTF_Font* Font::GetSDL() const noexcept
{
	return mFont;
}

// =========================
// SoundEffect (SFX)
// =========================

SoundEffect::~SoundEffect() noexcept
{
	if (mChunk) {
		Mix_FreeChunk(mChunk);
		mChunk = nullptr;
	}
}

SoundEffect::SoundEffect(SoundEffect&& other) noexcept
{
	mChunk = other.mChunk;
	other.mChunk = nullptr;
}

SoundEffect& SoundEffect::operator=(SoundEffect&& other) noexcept
{
	if (this != &other) {
		if (mChunk) {
			Mix_FreeChunk(mChunk);
		}
		mChunk = other.mChunk;
		other.mChunk = nullptr;
	}
	return *this;
}

Mix_Chunk* SoundEffect::GetSDL() const noexcept
{
	return mChunk;
}

// =========================
// Music
// =========================

Music::~Music() noexcept
{
	if (mMusic) {
		Mix_FreeMusic(mMusic);
		mMusic = nullptr;
	}
}

Music::Music(Music&& other) noexcept
{
	mMusic = other.mMusic;
	other.mMusic = nullptr;
}

Music& Music::operator=(Music&& other) noexcept
{
	if (this != &other) {
		if (mMusic) {
			Mix_FreeMusic(mMusic);
		}
		mMusic = other.mMusic;
		other.mMusic = nullptr;
	}
	return *this;
}

Mix_Music* Music::GetSDL() const noexcept
{
	return mMusic;
}

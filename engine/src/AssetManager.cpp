#include "AssetManager.h"

#include <filesystem>

#include "ErrorHandler.h"
#include "RenderManager.h"
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"


#define IsDir IsDirectoryValid
#define IsFile IsFileValid

namespace fs = std::filesystem;

// -----------------------
// Helpers internos
// -----------------------

inline std::string MakeFontKey(const std::string& path, int ptSize) {
    return path + "#" + std::to_string(ptSize);
}

inline bool IsFileValid(const std::string& path) noexcept
{
    if (path.empty())
        return false;

    try {
        return fs::exists(path) && fs::is_regular_file(path);
    }
    catch (...) {
        return false;
    }
}

inline bool IsDirectoryValid(const std::string& path) noexcept
{
    if (path.empty())
        return false;

    try {
        // exists() checks for existence
        // is_directory() checks that it is a directory
        return fs::exists(path) && fs::is_directory(path);
    }
    catch (...) {
        // May fail due to permissions or other system errors
        return false;
    }
}

// -----------------------
// Lifecycle
// -----------------------

bool AssetManager::Init(const std::string& assetsFolderPath) noexcept
{
    if (!IsDir(assetsFolderPath))
    {
        std::string wd = std::filesystem::current_path().string();
        LogError("AssetManager warning", "Init(): Assets folder path does not exist or is not accessible.\nXEngine is running in: '" + wd + "'");
        return false;
    }

    basePath = assetsFolderPath;

    while (basePath.back() == '/' || basePath.back() == '\\')
		basePath.pop_back();

    // Invalidate font-backed cached draw data before replacing font handles.
    if (auto* render = RenderManager::GetInstancePtr()) render->ClearTextCache_();
    // Clear any remaining state in case of reinitialization
    mTextures.clear();
    mFonts.clear();
    mSfx.clear();
    mMusic.clear();
    mTextureAliases.clear();
    mFontAliases.clear();
    mSfxAliases.clear();
    mMusicAliases.clear();
    memoryUsed = 0;

    // SDL_image
    const int wantImg = IMG_INIT_PNG | IMG_INIT_JPG;
    int gotImg = IMG_Init(wantImg);
    if ((gotImg & wantImg) != wantImg) {
        LogError("AssetManager::Init()", "SDL_InitSubSystem(IMG_Init) failed.", IMG_GetError());
        return false;
    }

    // SDL_ttf
    if (TTF_WasInit() == 0) {
        if (TTF_Init() != 0) {
            LogError("AssetManager::Init()", "SDL_InitSubSystem(TTF_Init) failed.", TTF_GetError());
            return false;
        }
    }

    return true;
}

void AssetManager::Shutdown() noexcept
{
    if (auto* render = RenderManager::GetInstancePtr()) render->ClearTextCache_();
    mTextures.clear();
    mFonts.clear();
    mSfx.clear();
    mMusic.clear();
    mTextureAliases.clear();
    mFontAliases.clear();
    mSfxAliases.clear();
    mMusicAliases.clear();
    memoryUsed = 0;
}

Font* AssetManager::GetEngineDefaultFont(bool bold) noexcept
{
    // Prefer game-provided font instead of embedding copyrighted font data.
    (void)bold;
    return GetFontByKey("ui_default");
}

// =======================
// TEXTURAS
// =======================

// The cache owns one resource per path; custom keys are aliases only.

#include <SDL.h>

size_t GetTextureMemoryBytes(const Texture* t)
{
    if (!t)
        return 0;

    SDL_Texture* sdl = t->GetSDL();
    if (!sdl)
        return 0;

    Uint32 format = 0;
    int access = 0;
    int w = 0, h = 0;

    if (SDL_QueryTexture(sdl, &format, &access, &w, &h) != 0)
        return 0;

    SDL_PixelFormat* pf = SDL_AllocFormat(format);
    if (!pf)
        return 0;

    const size_t bytesPerPixel = pf->BytesPerPixel;

    SDL_FreeFormat(pf);

    return size_t(w) * size_t(h) * bytesPerPixel;
}

Texture* AssetManager::LoadTexture(const std::string& path, float pixelsPerUnit) noexcept
{
    if (pixelsPerUnit <= 0)
    {
        LogError("AssetManager warning", 
            "LoadTexture(): Pixels per unit (PPP) can not be set to less than 1. Setting PPP to default value (100.0)");
        pixelsPerUnit = 100.f;
    }

    std::string fullPath = basePath + "/" + path;
    // Cache hit
    auto it = mTextures.find(fullPath);
    if (it != mTextures.end())
        return it->second.get();

    if (!IsFileValid(fullPath)) {
        LogError("AssetManager warning", "LoadTexture(): Texture path does not exist or is not accessible.");
        return nullptr;
    }

    SDL_Renderer* renderer = RenderManager::GetInstancePtr()
        ? RenderManager::GetInstance().SDL()
        : nullptr;

    if (!renderer) {
        LogError("AssetManager warning", "LoadTexture(): Renderer not avaliable to load '" + fullPath + "'.");
        return nullptr;
    }

    // Load directly to texture using SDL_image
    SDL_Texture* sdlTex = IMG_LoadTexture(renderer, fullPath.c_str());
    if (!sdlTex) {
        LogError("AssetManager::LoadTexture()", "IMG_LoadTexture failed for '" + fullPath + "'.", IMG_GetError());
        return nullptr;
    }

    // Crear Texture y rellenar datos
    auto up = std::make_unique<Texture>();
    up->mTexture = sdlTex;

    // Rellenamos ancho y alto reales
    int w = 0, h = 0;
    if (SDL_QueryTexture(sdlTex, nullptr, nullptr, &w, &h) == 0) {
        up->mW = w;
        up->mH = h;
    }
    else {
        up->mW = up->mH = 0;
        LogError("AssetManager::LoadTexture()", "SDL_QueryTexture failed for '" + fullPath + "'.", SDL_GetError());
        return nullptr;
    }

    up->SetPixelsPerUnit(pixelsPerUnit);

    // Cachear y devolver
    Texture* raw = up.get();
    memoryUsed += GetTextureMemoryBytes(raw)/(1024*2);
    mTextures.emplace(fullPath, std::move(up));
    return raw;
}

Texture* AssetManager::LoadTexture(const std::string& path, const std::string& key, float pixelsPerUnit) noexcept
{
    const std::string fullPath = basePath + "/" + path;
    if (const auto alias = mTextureAliases.find(key); alias != mTextureAliases.end() &&
        alias->second != fullPath) return nullptr;

    Texture* texture = LoadTexture(path, pixelsPerUnit);
    if (texture) mTextureAliases[key] = fullPath;
    return texture;
}

Texture* AssetManager::GetTexture(const std::string& path) const noexcept
{
    std::string fullPath = basePath + "/" + path;

    if (auto it = mTextures.find(fullPath); it != mTextures.end())
        return it->second.get();
    return nullptr;
}

Texture* AssetManager::GetTextureByKey(const std::string& key) const noexcept
{
    if (const auto alias = mTextureAliases.find(key); alias != mTextureAliases.end()) {
        if (const auto it = mTextures.find(alias->second); it != mTextures.end())
            return it->second.get();
    }
    if (auto it = mTextures.find(key); it != mTextures.end())
        return it->second.get();
    return nullptr;
}

// =======================
// FUENTES
// =======================

Font* AssetManager::LoadFont(const std::string& path, int ptSize) noexcept
{
    if (ptSize <= 0)
    {
        LogError("AssetManager warning", "LoadFont(): ptSize can not be set to less than 1.");
        return nullptr;
    }

    std::string fullPath = basePath + "/" + path;
    const std::string key = MakeFontKey(fullPath, ptSize);

    // Cache hit
    if (auto it = mFonts.find(key); it != mFonts.end())
        return it->second.get();

    if (!IsFileValid(fullPath)) {
        LogError("AssetManager warning", "LoadFont(): Font path does not exist or is not accessible.");
        return nullptr;
    }

    TTF_Font* ttf = TTF_OpenFont(fullPath.c_str(), ptSize);
    if (!ttf) {
        LogError("AssetManager::LoadFont()", "TTF_OpenFont failed for '" + fullPath + "' (" + std::to_string(ptSize) + " pt).", TTF_GetError());
        return nullptr;
    }

    auto up = std::make_unique<Font>();
    up->mFont = ttf;
    up->mPointSize = ptSize;

    Font* raw = up.get();
    try {
        memoryUsed += static_cast<double>(fs::file_size(fullPath)) / (1024 * 2);
    } catch (...) {
        // Font is still usable if filesystem metadata cannot be read.
    }
    mFonts.emplace(key, std::move(up));
    return raw;
}

Font* AssetManager::LoadFont(const std::string& path, const std::string& key, int ptSize) noexcept
{
    const std::string fullPath = basePath + "/" + path;
    const std::string canonical = MakeFontKey(fullPath, ptSize);
    if (const auto alias = mFontAliases.find(key); alias != mFontAliases.end() &&
        alias->second != canonical) return nullptr;

    Font* font = LoadFont(path, ptSize);
    if (font) mFontAliases[key] = canonical;
    return font;
}

Font* AssetManager::GetFont(const std::string& path, int ptSize) const noexcept
{
    std::string fullPath = basePath + "/" + path;
    const std::string key = MakeFontKey(fullPath, ptSize);

    if (auto it = mFonts.find(key); it != mFonts.end())
        return it->second.get();
    return nullptr;
}

Font* AssetManager::GetFontByKey(const std::string& key) const noexcept
{
    if (const auto alias = mFontAliases.find(key); alias != mFontAliases.end()) {
        if (const auto it = mFonts.find(alias->second); it != mFonts.end())
            return it->second.get();
    }
    if (auto it = mFonts.find(key); it != mFonts.end())
        return it->second.get();
    return nullptr;
}

// =======================
// SONIDO (SFX)
// =======================

SoundEffect* AssetManager::LoadSFX(const std::string& path) noexcept
{
    std::string fullPath = basePath + "/" + path;

    // Cache hit
    if (auto it = mSfx.find(fullPath); it != mSfx.end())
        return it->second.get();

    if (!IsFileValid(fullPath)) {
        LogError("AssetManager warning", "LoadSFX(): SFX path does not exist or is not accessible.");
        return nullptr;
    }

    Mix_Chunk* chunk = Mix_LoadWAV(fullPath.c_str());
    if (!chunk) {
        LogError("AssetManager::LoadSFX()", "Mix_LoadWAV failed for '" + fullPath + "'.", Mix_GetError());
        return nullptr;
    }

    auto up = std::make_unique<SoundEffect>();
    up->mChunk = chunk;

    SoundEffect* raw = up.get();
    memoryUsed += static_cast<double>(chunk->alen) / (1024 * 2);
    mSfx.emplace(fullPath, std::move(up));
    return raw;
}

size_t GetSFXMemoryBytes(const SoundEffect* s) {
    Mix_Chunk* c = s->GetSDL();
    if (!c) return 0;
    return size_t(c->alen); // size in bytes
}

SoundEffect* AssetManager::LoadSFX(const std::string& path, const std::string& key) noexcept
{
    const std::string fullPath = basePath + "/" + path;
    if (const auto alias = mSfxAliases.find(key); alias != mSfxAliases.end() &&
        alias->second != fullPath) return nullptr;

    SoundEffect* sfx = LoadSFX(path);
    if (sfx) mSfxAliases[key] = fullPath;
    return sfx;
}

SoundEffect* AssetManager::GetSFX(const std::string& path) const noexcept
{
    std::string fullPath = basePath + "/" + path;

    if (auto it = mSfx.find(fullPath); it != mSfx.end())
        return it->second.get();
    return nullptr;
}

SoundEffect* AssetManager::GetSFXByKey(const std::string& key) const noexcept
{
    if (const auto alias = mSfxAliases.find(key); alias != mSfxAliases.end()) {
        if (const auto it = mSfx.find(alias->second); it != mSfx.end())
            return it->second.get();
    }
    if (auto it = mSfx.find(key); it != mSfx.end())
        return it->second.get();
    return nullptr;
}

// =======================
// MUSICA (stream)
// =======================

Music* AssetManager::LoadMusic(const std::string& path) noexcept
{
    std::string fullPath = basePath + "/" + path;

    // Cache hit
    if (auto it = mMusic.find(fullPath); it != mMusic.end())
        return it->second.get();

    if (!IsFileValid(fullPath)) {
        LogError("AssetManager warning", "LoadMusic(): Music path does not exist or is not accessible.");
        return nullptr;
    }

    Mix_Music* mus = Mix_LoadMUS(fullPath.c_str());
    if (!mus) {
        LogError("AssetManager::LoadMusic()", "Mix_LoadMUS failed for '" + fullPath + "'.", Mix_GetError());
        return nullptr;
    }

    auto up = std::make_unique<Music>();
    up->mMusic = mus;

    Music* raw = up.get();
    mMusic.emplace(fullPath, std::move(up));
    return raw;
}

Music* AssetManager::LoadMusic(const std::string& path, const std::string& key) noexcept
{
    const std::string fullPath = basePath + "/" + path;
    if (const auto alias = mMusicAliases.find(key); alias != mMusicAliases.end() &&
        alias->second != fullPath) return nullptr;

    Music* music = LoadMusic(path);
    if (music) mMusicAliases[key] = fullPath;
    return music;
}

Music* AssetManager::GetMusic(const std::string& path) const noexcept
{
    std::string fullPath = basePath + "/" + path;

    if (auto it = mMusic.find(fullPath); it != mMusic.end())
        return it->second.get();
    return nullptr;
}

Music* AssetManager::GetMusicByKey(const std::string& key) const noexcept
{
    if (const auto alias = mMusicAliases.find(key); alias != mMusicAliases.end()) {
        if (const auto it = mMusic.find(alias->second); it != mMusic.end())
            return it->second.get();
    }
    if (auto it = mMusic.find(key); it != mMusic.end())
        return it->second.get();
    return nullptr;
}

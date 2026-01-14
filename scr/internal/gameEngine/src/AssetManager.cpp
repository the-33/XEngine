#include "AssetManager.h"

#include <filesystem>

#include "ErrorHandler.h"
#include "RenderManager.h"
#include "ConsoleFonts.h"


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
        // exists() -> que exista
        // is_directory() -> que realmente sea carpeta
        return fs::exists(path) && fs::is_directory(path);
    }
    catch (...) {
        // Puede fallar por permisos u otros errores del sistema
        return false;
    }
}

// -----------------------
// Ciclo de vida
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

    // Vacía cualquier resto por si re-inicializamos
    mTextures.clear();
    mFonts.clear();
    mSfx.clear();
    mMusic.clear();

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
    mTextures.clear();
    mFonts.clear();
    mSfx.clear();
    mMusic.clear();
}

Font* AssetManager::GetEngineDefaultFont(bool bold = false) noexcept
{
    static Font* sEngineFontNormal = nullptr;
    static Font* sEngineFontBold = nullptr;
    if (!bold && sEngineFontNormal) return sEngineFontNormal;
    if (bold && sEngineFontBold) return sEngineFontBold;

    SDL_RWops* rw = SDL_RWFromConstMem(
        (!bold) ? JetBrainsMono_Regular_ttf : JetBrainsMono_Bold_ttf,
        (!bold) ? JetBrainsMono_Regular_ttf_len : JetBrainsMono_Bold_ttf_len
    );

    if (!rw)
    {
        LogError("AssetManager", "Failed to create RWops for embedded font");
        return nullptr;
    }

    TTF_Font* ttf = TTF_OpenFontRW(rw, 1, 14); // tamaño fijo
    if (!ttf)
    {
        LogError("AssetManager", "Failed to load embedded font");
        return nullptr;
    }

    if (!bold && sEngineFontNormal)
    {
        sEngineFontNormal = new Font();
        sEngineFontNormal->mFont = ttf;
        sEngineFontNormal->mPointSize = 10;
        return sEngineFontNormal;
    }
    else
    {
        sEngineFontBold = new Font();
        sEngineFontBold->mFont = ttf;
        sEngineFontBold->mPointSize = 10;
        return sEngineFontBold;
    }
}

// =======================
// TEXTURAS
// =======================

// LA IDEA ES DOCUMENTAR TODAS LAS FUNCIONES DEL MOTOR QUE SEAN PUBLICAS PARA EL USUARIO
// TENGO QUE REHACERLO CON UN MAPA DE KEYS Y PATHS PARA QUE FUNCIONE TANTO CON KEY COMO
// SIN KEY BIEN, AHORA MISMO LOS QUE GUARDAN TEXTURAS CON KEY NO HACEN BIEN EL CACHE HIT

/*  
    @brief Loads a texture into the texture manager for further use
    @param path The path of the texture relative to the assets folder, format: "path/to/the/texture.fileType"
    @return A unique pointer to the loaded texture or nullptr if loading failed
*/  

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

    // Carga con SDL_image directamente a textura
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

    // Carga con SDL_image directamente a textura
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
    memoryUsed += GetTextureMemoryBytes(raw) / (1024 * 2);
    mTextures.emplace(key, std::move(up));
    return raw;
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
    mFonts.emplace(key, std::move(up));
    return raw;
}

Font* AssetManager::LoadFont(const std::string& path, const std::string& key, int ptSize) noexcept
{
    if (ptSize <= 0)
    {
        LogError("AssetManager warning", "LoadFont(): ptSize can not be set to less than 1.");
        return nullptr;
    }

    std::string fullPath = basePath + "/" + path;

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

    float fontSize = 0.f;

    try {
        fontSize = (float) fs::file_size(fullPath);
    }
    catch (...) {
        fontSize = 0.f;
    }

    Font* raw = up.get();
    memoryUsed += fontSize / (1024 * 2);
    mFonts.emplace(key, std::move(up));
    return raw;
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
    mSfx.emplace(fullPath, std::move(up));
    return raw;
}

size_t GetSFXMemoryBytes(const SoundEffect* s) {
    Mix_Chunk* c = s->GetSDL();
    if (!c) return 0;
    return size_t(c->alen); // tamaño en bytes
}

SoundEffect* AssetManager::LoadSFX(const std::string& path, const std::string& key) noexcept
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
    memoryUsed += GetSFXMemoryBytes(raw) / (1024*2);
    mSfx.emplace(key, std::move(up));
    return raw;
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
    mMusic.emplace(key, std::move(up));
    return raw;
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
    if (auto it = mMusic.find(key); it != mMusic.end())
        return it->second.get();
    return nullptr;
}
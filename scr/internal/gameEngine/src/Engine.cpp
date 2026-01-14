#include "Engine.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include <sstream>
#include <cstdlib> // Para system()
#include <inttypes.h> // Para uint64_t

#ifdef _WIN32
#define CLEAR_SCREEN "cls"
#else
#define CLEAR_SCREEN "clear"
#endif

#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// SDL
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#include "SDL_mixer.h"

// Core
#include "ErrorHandler.h"
#include "Singleton.h"
#include "Property.h"
#include "Assets.h"

// Components
#include "Component.h"
#include "Behaviour.h"
#include "Transform.h"
#include "SpriteRenderer.h"
#include "Collider2D.h"
#include "GameObject.h"
#include "Rigidbody2D.h"

// Managers
#include "WindowManager.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "AssetManager.h"
#include "SoundManager.h"
#include "RenderManager.h"
#include "CollisionManager.h"
#include "UIManager.h"
#include "SceneManager.h"
#include "PhysicsManager.h"
#include "RandomManager.h"

// Scene
#include "Camera2D.h"
#include "Scene.h"

using json = nlohmann::json;

Engine::Engine() noexcept
{
	bool success = false;

	success = WindowManager::CreateSingleton();
	if (success) success = TimeManager::CreateSingleton();
	if (success) success = RandomManager::CreateSingleton();
	if (success) success = InputManager::CreateSingleton();
	if (success) success = AssetManager::CreateSingleton();
	if (success) success = SoundManager::CreateSingleton();
	if (success) success = RenderManager::CreateSingleton();
	if (success) success = CollisionManager::CreateSingleton();
    if (success) success = PhysicsManager::CreateSingleton();
	if (success) success = UIManager::CreateSingleton();
	if (success) success = SceneManager::CreateSingleton();

	if (!success)
	{
		bool stopDestroying = false;
		if (WindowManager::GetInstancePtr() != nullptr) WindowManager::DestroySingleton();
		else stopDestroying = true;
		if (!stopDestroying && TimeManager::GetInstancePtr() != nullptr) TimeManager::DestroySingleton();
		else stopDestroying = true;
        if (!stopDestroying && RandomManager::GetInstancePtr() != nullptr) RandomManager::DestroySingleton();
        else stopDestroying = true;
		if (!stopDestroying && InputManager::GetInstancePtr() != nullptr) InputManager::DestroySingleton();
		else stopDestroying = true;
		if (!stopDestroying && AssetManager::GetInstancePtr() != nullptr) AssetManager::DestroySingleton();
		else stopDestroying = true;
		if (!stopDestroying && SoundManager::GetInstancePtr() != nullptr) SoundManager::DestroySingleton();
		else stopDestroying = true;
		if (!stopDestroying && RenderManager::GetInstancePtr() != nullptr) RenderManager::DestroySingleton();
		else stopDestroying = true;
		if (!stopDestroying && CollisionManager::GetInstancePtr() != nullptr) CollisionManager::DestroySingleton();
        else stopDestroying = true;
        if (!stopDestroying && PhysicsManager::GetInstancePtr() != nullptr) PhysicsManager::DestroySingleton();
		else stopDestroying = true;
		if (!stopDestroying && UIManager::GetInstancePtr() != nullptr) UIManager::DestroySingleton();
		else stopDestroying = true;
		if (!stopDestroying && SceneManager::GetInstancePtr() != nullptr) SceneManager::DestroySingleton();

		LogError("Engine Error", "Engine(): Failed to create one or more managers.");
	}
}

Engine::~Engine()
{
    // Si alguien no llamó a Stop(), lo llamamos
    if (mStarted)
        StopImpl();

    // Destruir singletons en orden inverso de dependencias
    if (SceneManager::GetInstancePtr())     SceneManager::DestroySingleton();
    if (UIManager::GetInstancePtr())        UIManager::DestroySingleton();
    if (PhysicsManager::GetInstancePtr())   PhysicsManager::DestroySingleton();
    if (CollisionManager::GetInstancePtr()) CollisionManager::DestroySingleton();
    if (RenderManager::GetInstancePtr())    RenderManager::DestroySingleton();
    if (SoundManager::GetInstancePtr())     SoundManager::DestroySingleton();
    if (AssetManager::GetInstancePtr())     AssetManager::DestroySingleton();
    if (InputManager::GetInstancePtr())     InputManager::DestroySingleton();
    if (RandomManager::GetInstancePtr())    RandomManager::DestroySingleton();
    if (TimeManager::GetInstancePtr())      TimeManager::DestroySingleton();
    if (WindowManager::GetInstancePtr())    WindowManager::DestroySingleton();
}

bool Engine::StartImpl(const std::string configPath) noexcept
{
    if (mStarted)
    {
        LogError("Engine Warning", "Start(): Engine was already started.");
        return false;
    }

    printf("Starting XEngine...\n");
    printf("------------------------------\n\n");
    printf("Loading configuration...\n");
    Config cfg;
    if (!LoadConfigFromFile((configPath == "") ? kDefaultConfigPath : configPath, cfg))
        LogError("Engine warning", "Start(): Could not read config file, loading default values.");
    printf("Configuration loaded.\n\n");

    mCfg = cfg;
    mFixedDt = mCfg.time.fixedDt;
    mLogStats = mCfg.logStats;
    mLogErrors = mCfg.logErrors;
    mAccumulator = 0.f;

    printf("Initializing SDL... ");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0)
    {
        LogError("Engine::Start()", "SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) failed.", SDL_GetError());
        return false;
    }
    printf("OK\n");

    printf("Initializing SDL_image... ");
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if ((IMG_Init(imgFlags) & imgFlags) != imgFlags)
    {
        LogError("Engine::Start()", "IMG_Init(imgFlags) failed.", IMG_GetError());
        return false;
    }
    printf("OK\n");

    printf("Initializing SDL_ttf... ");
    if (TTF_Init() != 0)
    {
        LogError("Engine::Start()", "TTF_Init() failed.", TTF_GetError());
        return false;
    }
    printf("OK\n");

    printf("Initializing SDL_mixer... ");
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        LogError("Engine::Start()", "Mix_OpenAudio failed.", Mix_GetError());
        return false;
    }
    printf("OK\n\n");

    auto* win = WindowManager::GetInstancePtr();
    auto* time = TimeManager::GetInstancePtr();
    auto* random = RandomManager::GetInstancePtr();
    auto* input = InputManager::GetInstancePtr();
    auto* assets = AssetManager::GetInstancePtr();
    auto* sound = SoundManager::GetInstancePtr();
    auto* render = RenderManager::GetInstancePtr();
    auto* collision = CollisionManager::GetInstancePtr();
    auto* physics = PhysicsManager::GetInstancePtr();
    auto* ui = UIManager::GetInstancePtr();
    auto* scenes = SceneManager::GetInstancePtr();

    if (!win || !time || !random || !input || !assets || !sound ||
        !render || !collision || !physics || !ui || !scenes)
    {
        LogError("Engine error", "Start(): One or more managers are null, check Engine::Engine()");
        return false;
    }

    bool ok = true;

    printf("Initializing WindowManager... ");
    {
        WindowManager::Config cfg{
        mCfg.window.width,
        mCfg.window.height,
        mCfg.window.fullscreen,
        mCfg.window.title,
        };

        ok = win->Init(cfg);
        if (!ok)
        {
            LogError("Engine Start", "WindowManager::Init() failed.");
            return false;
        }
    }
    printf("OK\n");

    printf("Initializing RenderManager... ");
    {
        RenderManager::Config cfg{
            mCfg.renderer.hardwareAcceleration,
            mCfg.renderer.vsync,
            mCfg.renderer.bgColor
        };

        ok = render->Init(cfg);
        if (!ok)
        {
            LogError("Engine Start", "RenderManager::Init() failed.");
            return false;
        }
    }
    printf("OK\n");

    printf("Initializing TimeManager... ");
    ok = time->Init(mCfg.time.fixedDt);
    if (!ok)
    {
        LogError("Engine Start", "TimeManager::Init() failed.");
        return false;
    }
    printf("OK\n");

    printf("Initializing RandomManager... ");
    random->Init();
    printf("OK\n");

    printf("Initializing InputManager... ");
    {
        InputManager::Config cfg{
            mCfg.input.startMouseLocked,
            mCfg.input.startMouseHidden,
            mCfg.input.startTextInput,
        };

        ok = input->Init(cfg);
        if (!ok)
        {
            LogError("Engine Start", "InputManager::Init() failed.");
            return false;
        }
    }
    printf("OK\n");

    printf("Initializing AssetManager... ");
    ok = assets->Init(mCfg.assets.assetsFolderPath);
    if (!ok)
    {
        LogError("Engine Start", "AssetManager::Init() failed.");
        return false;
    }
    printf("OK\n");

    printf(" * Loading default font asset to AssetManager... ");
    assets->LoadFont(cfg.assets.defaultFontRelativePath, cfg.uiStyle.defaultFontKey, cfg.uiStyle.defaultFontptSize);
    printf("OK\n");

    printf("Initializing SoundManager... ");
    {
        SoundManager::Config cfg{
            mCfg.sound.sfxChannels,
            mCfg.sound.masterVolume,
            mCfg.sound.enableDucking,
            mCfg.sound.duckVolume,
            mCfg.sound.duckAttackSec,
            mCfg.sound.duckReleaseSec,
        };

        ok = sound->Init(cfg);
        if (!ok)
        {
            LogError("Engine Start", "SoundManager::Init() failed.");
            return false;
        }
    }
    printf("OK\n");

    printf("Initializing CollisionManager... ");
    ok = collision->Init();
    if (!ok)
    {
        LogError("Engine Start", "CollisionManager::Init() failed.");
        return false;
    }
    printf("OK\n");

    printf("Initializing PhysicsManager... ");
    {
        PhysicsManager::Config cfg
        {
            mCfg.physics.baseGravity,
            mCfg.physics.penetrationSlop,
            mCfg.physics.penetrationPercent,
            mCfg.physics.maxSubsteps,
            mCfg.physics.ccdMinSizeFactor,
        };

        ok = physics->Init(cfg);
        if (!ok)
        {
            LogError("Engine Start", "PhysicsManager::Init() failed.");
            return false;
        }
    }
    printf("OK\n");

    printf("Initializing UIManager... ");
    {
        UIManager::UIStyle style{
            mCfg.uiStyle.text,
            mCfg.uiStyle.btn,
            mCfg.uiStyle.btnHot,
            mCfg.uiStyle.btnActive,
            mCfg.uiStyle.outline,
            mCfg.uiStyle.padding,
            mCfg.uiStyle.defaultFontKey,
        };

        ok = ui->Init(style);
        if (!ok)
        {
            LogError("Engine Start", "UIManager::Init() failed.");
            return false;
        }
    }
    printf("OK\n");

    printf("Initializing SceneManager... ");
    ok = scenes->Init();
    if (!ok)
    {
        LogError("Engine Start", "SceneManager::Init() failed.");
        return false;
    }
    printf("OK\n");

    SafeToQuit = false;
    mStarted = true;
    printf("\nXEngine started successfully.\n\n");

    return true;
}

int Engine::RunSingleThreadingImpl() noexcept
{
    if (!mStarted) return 0;

    auto* time = TimeManager::GetInstancePtr();
	auto* win = WindowManager::GetInstancePtr();
    RenderManager::GetInstancePtr()->renderBeginTime = (float) time->timeSinceStart;

    mRunning = true;

    // ============================================================
    // BUCLE PRINCIPAL
    // ============================================================
    while(mRunning && !win->ShouldClose())
    {
        SafeToQuit = false;
        // ------------------------------------------------------------
        // 1) TimeManager -> calcular deltaTime y fixed dt
        // ------------------------------------------------------------
		FrameTick_();

        // ------------------------------------------------------------
        // 2) InputManager frame update
        // ------------------------------------------------------------
		PollInput_();

        // ------------------------------------------------------------
        // 3) FixedUpdate (pueden ejecutarse varias veces)
        // ------------------------------------------------------------
        float fixedUpdateStart = (float)time->timeSinceStart;
        fixedUpdatesDoneThisFrame = 0;
        lastAccumulator = mAccumulator;
        while (mAccumulator >= mFixedDt)
        {
			DoFixedUpdates_(mFixedDt);
            fixedUpdatesDoneThisFrame++;
            mAccumulator -= mFixedDt;
        }
        mFixedUpdateTime = (float)time->timeSinceStart - fixedUpdateStart;

        // ------------------------------------------------------------
        // 4) Update variable
        // ------------------------------------------------------------
		DoUpdate_(time->deltaTime);

        // ------------------------------------------------------------
        // 5) Render
        // ------------------------------------------------------------
        
		DoRender_();

        // ------------------------------------------------------------
        // 6) Limpieza de objetos marcados para destruir
        // ------------------------------------------------------------
        ManageSceneQueues_();

        // ------------------------------------------------------------
		// 7) Aplicar cambios de escena pendientes
        // ------------------------------------------------------------
		ManageSceneChanges_();
        SafeToQuit = true;
    }

    SafeToQuit = true;
    StopImpl();
    return 0;
}

int Engine::RunMultiThreadingImpl() noexcept
{
    LogError("Engine::Run()", "Multi-threading is not implemented yet.");
    return 0;
}

void Engine::StopImpl() noexcept
{
    if (!mStarted) return;
    mRunning = false;
    if (!SafeToQuit) return;

    auto* win = WindowManager::GetInstancePtr();
    auto* input = InputManager::GetInstancePtr();
    auto* assets = AssetManager::GetInstancePtr();
    auto* sound = SoundManager::GetInstancePtr();
    auto* render = RenderManager::GetInstancePtr();
    auto* collision = CollisionManager::GetInstancePtr();
    auto* physics = PhysicsManager::GetInstancePtr();
    auto* ui = UIManager::GetInstancePtr();
    auto* scenes = SceneManager::GetInstancePtr();

    printf("Shutting down SceneManager... ");
    if (scenes) { scenes->Shutdown(); }
    printf("OK\n");

    printf("Shutting down UIManager... ");
    if (ui) { ui->Shutdown(); }
    printf("OK\n");

    printf("Shutting down PhysicsManager... ");
    if (physics) { physics->Shutdown(); }
    printf("OK\n");

    printf("Shutting down CollisionManager... ");
    if (collision) { collision->Shutdown(); }
    printf("OK\n");

    printf("Shutting down SoundManager... ");
    if (sound) { sound->Shutdown(); }
    printf("OK\n");

    printf("Shutting down AssetManager... ");
    if (assets) { assets->Shutdown(); }
    printf("OK\n");

    printf("Shutting down InputManager... ");
    if (input) { input->Shutdown(); }
    printf("OK\n");

    printf("Shutting down RenderManager... ");
    if (render) { render->Shutdown(); }
    printf("OK\n");

    printf("Shutting down WindowManager... ");
    if (win) { win->Shutdown(); }
    printf("OK\n");

    printf("Shutting down SDL_mixer... ");
    Mix_CloseAudio();
    printf("OK\n");

    printf("Shutting down SDL_ttf... ");
    TTF_Quit();
    printf("OK\n");

    printf("Shutting down SDL_image... ");
    IMG_Quit();
    printf("OK\n");

    printf("Shutting down SDL... ");
    SDL_Quit();
    printf("OK\n");

    mStarted = false;
    DestroySingleton();
}

bool Engine::ApplyStartingScene() const noexcept
{
    if (!Scenes()->mHasPending)
    {
        LogError("Engine::ApplyStartingScene()", "No pending scene to apply at start. Engine will shut down.");
		return false;
    }

    Scenes()->ApplyPendingScene();
	return true;
}

Camera2D* Engine::GetCamera() noexcept { return SceneManager::GetInstancePtr()->GetActive()->GetCamera(); }

inline float Engine::GetFPS() noexcept
{
    if (!IsRunning()) return 0.f;
    if (auto* render = RenderManager::GetInstancePtr())
        return 1 / (render->renderTime);
    return 0.f;
}

inline float Engine::GetAspectRatio() noexcept
{
    if (!IsRunning()) return 0.f;
    Vec2I res = WindowManager::GetInstancePtr()->GetDrawableSize();
    float aspect = (float)res.x / (float)res.y;

    GetInstancePtr()->mAspectRatio = aspect;

    return aspect;
}

// Helper para sacar el aspect ratio como fraccion
int gcd(int a, int b) {
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

inline Vec2I Engine::GetAspectRatioAsFraction() noexcept
{
    if (!IsRunning()) return Vec2I::Zero();
    Vec2I res = WindowManager::GetInstancePtr()->GetDrawableSize();
    float aspect = (float)res.x / (float)res.y;

    if (aspect == GetInstancePtr()->aspectRatio) return GetInstancePtr()->mAspectRatioAsFraction;

    int w = res.x;
    int h = res.y;

    int g = gcd(w, h);
    
    Vec2I aspectAsFr = Vec2I(w / g, h / g);

    GetInstancePtr()->mAspectRatio = aspect;
    GetInstancePtr()->mAspectRatioAsFraction = aspectAsFr;

    return aspectAsFr;
}

float Engine::GetFPS_() const noexcept { return Engine::GetFPS(); }
float Engine::GetAspectRatio_() const noexcept { return Engine::GetAspectRatio(); }
Vec2I Engine::GetAspectRatioAsFraction_() const noexcept { return Engine::GetAspectRatioAsFraction(); }

static inline std::string Trim(const std::string& s)
{
    auto first = std::find_if_not(s.begin(), s.end(), ::isspace);
    if (first == s.end()) return {};
    auto last = std::find_if_not(s.rbegin(), s.rend(), ::isspace).base();
    return std::string(first, last);
}

static inline bool ParseBool(const std::string& s)
{
    std::string v;
    v.reserve(s.size());
    std::transform(s.begin(), s.end(), std::back_inserter(v),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return (v == "1" || v == "true" || v == "yes" || v == "on");
}

static inline Color ParseColor(const std::string& s)
{
    // Espera "r,g,b,a"
    std::istringstream iss(s);
    std::string part;
    int r = 0, g = 0, b = 0, a = 255;

    if (std::getline(iss, part, ',')) r = std::stoi(part);
    if (std::getline(iss, part, ',')) g = std::stoi(part);
    if (std::getline(iss, part, ',')) b = std::stoi(part);
    if (std::getline(iss, part, ',')) a = std::stoi(part);

    return Color{
        static_cast<uint8_t>(std::clamp(r, 0, 255)),
        static_cast<uint8_t>(std::clamp(g, 0, 255)),
        static_cast<uint8_t>(std::clamp(b, 0, 255)),
        static_cast<uint8_t>(std::clamp(a, 0, 255))
    };
}

/*
-----TEMPLATE DE FICHERO DE CONFIGURACIÓN-----

{
  "Engine": {
    "multiThreading": false,
    "logStats": false,
    "logErrors": true,
  },
  "Window": {
    "width": 1280,
    "height": 720,
    "fullscreen": false,
    "title": "My Awesome Game"
  },
  "Renderer": {
    "vsync": true,
    "hardwareAcceleration": true,
    "bgColor": [0, 0, 0, 255]
  },
  "Input": {
    "startTextInput": false,
    "startMouseLocked": false,
    "startMouseHidden": false
  },
  "Time": {
    "fixedDt": 0.0166667
  },
  "Assets": {
    "assetsFolderPath": "./assets/",
    "defaultFontRelativePath": "fonts/ui_default.ttf"
  },
  "Sound": {
    "sfxChannels": 32,
    "masterVolume": 1.0,
    "enableDucking": false,
    "duckVolume": 0.6,
    "duckAttackSec": 0.02,
    "duckReleaseSec": 0.25
  },
  "UIStyle": {
    "text":    [240, 240, 240, 255],
    "btn":     [60, 60, 60, 255],
    "btnHot":  [80, 80, 80, 255],
    "btnActive":[40, 40, 40, 255],
    "outline": [200, 200, 200, 255],
    "padding": 6.0,
    "defaultFontKey": "ui_default"
	"defaultFontPtSize": 24
  },
  "Physics": {
    "baseGravity":     [0.0, 9.81],
    "penetrationSlop": 0.01,
    "penetrationPercent": 0.8,
    "maxSubsteps":     8,
    "ccdMinSizeFactor":0.5
  }
}

*/

static Color JsonColorOrDefault(const json& j, const Color& def) noexcept
{
    Color c = def;

    if (!j.is_array() || j.size() < 4)
        return c;

    // Cast a int -> uint8_t con clamp básico
    auto to_u8 = [](const json& v, uint8_t fallback) -> uint8_t
        {
            if (!v.is_number_integer()) return fallback;
            int x = v.get<int>();
            if (x < 0) x = 0;
            if (x > 255) x = 255;
            return static_cast<uint8_t>(x);
        };

    c.r = to_u8(j[0], def.r);
    c.g = to_u8(j[1], def.g);
    c.b = to_u8(j[2], def.b);
    c.a = to_u8(j[3], def.a);
    return c;
}

static Vec2 JsonVec2OrDefault(const json& j, const Vec2& def) noexcept
{
    if (!j.is_array() || j.size() < 2) return def;

    auto to_f = [](const json& v, float fallback) -> float
        {
            if (!v.is_number()) return fallback;      // acepta int o float
            return v.get<float>();
        };

    Vec2 out = def;
    out.x = to_f(j[0], def.x);
    out.y = to_f(j[1], def.y);
    return out;
}

static std::pair<int, int> ByteToLineCol(const std::string& text, std::size_t bytePos) noexcept
{
    int line = 1;
    int col = 1;

    const std::size_t n = (bytePos < text.size()) ? bytePos : text.size();
    for (std::size_t i = 0; i < n; ++i)
    {
        if (text[i] == '\n') { ++line; col = 1; }
        else { ++col; }
    }
    return { line, col };
}

bool Engine::LoadConfigFromFile(const std::string& path, Config& out) noexcept
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        file = std::ifstream("./XEngine_CONFIG.json", std::ios::binary);
    }

    if (!file.is_open()) return false;

    // Leer TODO el fichero (para poder calcular línea/columna)
    std::ostringstream oss;
    oss << file.rdbuf();
    const std::string text = oss.str();

    try
    {
        json j = json::parse(text); // lanza json::parse_error si el JSON está mal

        // ===========================
        //  Engine
        // ===========================
        if (auto it = j.find("Engine"); it != j.end() && it->is_object())
        {
            const json& eng = *it;
            if (eng.contains("multiThreading") && eng["multiThreading"].is_boolean())
                out.multiThreading = eng["multiThreading"].get<bool>();
            if (eng.contains("logStats") && eng["logStats"].is_boolean())
                out.logStats = eng["logStats"].get<bool>();
            if (eng.contains("logErrors") && eng["logErrors"].is_boolean())
                out.logErrors = eng["logErrors"].get<bool>();
        }

        // ===========================
        //  Window
        // ===========================
        if (auto it = j.find("Window"); it != j.end() && it->is_object())
        {
            const json& win = *it;

            if (win.contains("width") && win["width"].is_number_integer())
                out.window.width = win["width"].get<int>();
            if (win.contains("height") && win["height"].is_number_integer())
                out.window.height = win["height"].get<int>();
            if (win.contains("fullscreen") && win["fullscreen"].is_boolean())
                out.window.fullscreen = win["fullscreen"].get<bool>();

            if (win.contains("title") && win["title"].is_string())
            {
                static std::string windowTitleStorage;
                windowTitleStorage = win["title"].get<std::string>();
                out.window.title = windowTitleStorage.c_str();
            }
        }

        // ===========================
        //  Renderer
        // ===========================
        if (auto it = j.find("Renderer"); it != j.end() && it->is_object())
        {
            const json& r = *it;

            if (r.contains("vsync") && r["vsync"].is_boolean())
                out.renderer.vsync = r["vsync"].get<bool>();
            if (r.contains("hardwareAcceleration") && r["hardwareAcceleration"].is_boolean())
                out.renderer.hardwareAcceleration = r["hardwareAcceleration"].get<bool>();
            if (r.contains("bgColor"))
                out.renderer.bgColor = JsonColorOrDefault(r["bgColor"], out.renderer.bgColor);
        }

        // ===========================
        //  Input
        // ===========================
        if (auto it = j.find("Input"); it != j.end() && it->is_object())
        {
            const json& in = *it;

            if (in.contains("startTextInput") && in["startTextInput"].is_boolean())
                out.input.startTextInput = in["startTextInput"].get<bool>();
            if (in.contains("startMouseLocked") && in["startMouseLocked"].is_boolean())
                out.input.startMouseLocked = in["startMouseLocked"].get<bool>();
            if (in.contains("startMouseHidden") && in["startMouseHidden"].is_boolean())
                out.input.startMouseHidden = in["startMouseHidden"].get<bool>();
        }

        // ===========================
        //  Time
        // ===========================
        if (auto it = j.find("Time"); it != j.end() && it->is_object())
        {
            const json& t = *it;

            if (t.contains("fixedDt") && t["fixedDt"].is_number())
                out.time.fixedDt = t["fixedDt"].get<float>();
        }

        // ===========================
        //  Assets
        // ===========================
        if (auto it = j.find("Assets"); it != j.end() && it->is_object())
        {
            const json& a = *it;

            if (a.contains("assetsFolderPath") && a["assetsFolderPath"].is_string())
                out.assets.assetsFolderPath = a["assetsFolderPath"].get<std::string>();

            if (a.contains("defaultFontRelativePath") && a["defaultFontRelativePath"].is_string())
                out.assets.defaultFontRelativePath = a["defaultFontRelativePath"].get<std::string>();
        }

        // ===========================
        //  Sound
        // ===========================
        if (auto it = j.find("Sound"); it != j.end() && it->is_object())
        {
            const json& s = *it;

            if (s.contains("sfxChannels") && s["sfxChannels"].is_number_integer())
                out.sound.sfxChannels = s["sfxChannels"].get<int>();
            if (s.contains("masterVolume") && s["masterVolume"].is_number())
                out.sound.masterVolume = s["masterVolume"].get<float>();
            if (s.contains("enableDucking") && s["enableDucking"].is_boolean())
                out.sound.enableDucking = s["enableDucking"].get<bool>();
            if (s.contains("duckVolume") && s["duckVolume"].is_number())
                out.sound.duckVolume = s["duckVolume"].get<float>();
            if (s.contains("duckAttackSec") && s["duckAttackSec"].is_number())
                out.sound.duckAttackSec = s["duckAttackSec"].get<float>();
            if (s.contains("duckReleaseSec") && s["duckReleaseSec"].is_number())
                out.sound.duckReleaseSec = s["duckReleaseSec"].get<float>();
        }

        // ===========================
        //  UIStyle
        // ===========================
        if (auto it = j.find("UIStyle"); it != j.end() && it->is_object())
        {
            const json& ui = *it;

            if (ui.contains("text"))      out.uiStyle.text = JsonColorOrDefault(ui["text"], out.uiStyle.text);
            if (ui.contains("btn"))       out.uiStyle.btn = JsonColorOrDefault(ui["btn"], out.uiStyle.btn);
            if (ui.contains("btnHot"))    out.uiStyle.btnHot = JsonColorOrDefault(ui["btnHot"], out.uiStyle.btnHot);
            if (ui.contains("btnActive")) out.uiStyle.btnActive = JsonColorOrDefault(ui["btnActive"], out.uiStyle.btnActive);
            if (ui.contains("outline"))   out.uiStyle.outline = JsonColorOrDefault(ui["outline"], out.uiStyle.outline);

            if (ui.contains("padding") && ui["padding"].is_number())
                out.uiStyle.padding = ui["padding"].get<float>();
            if (ui.contains("defaultFontKey") && ui["defaultFontKey"].is_string())
                out.uiStyle.defaultFontKey = ui["defaultFontKey"].get<std::string>();
            if (ui.contains("defaultFontPtSize") && ui["defaultFontPtSize"].is_number())
                out.uiStyle.defaultFontptSize = ui["defaultFontPtSize"].get<int>();
        }

        // ===========================
        //  Physics 
        // ===========================
        if (auto it = j.find("Physics"); it != j.end() && it->is_object())
        {
            const json& p = *it;

            // baseGravity: [x,y]
            if (p.contains("baseGravity"))
                out.physics.baseGravity = JsonVec2OrDefault(p["baseGravity"], out.physics.baseGravity);

            if (p.contains("penetrationSlop") && p["penetrationSlop"].is_number())
                out.physics.penetrationSlop = p["penetrationSlop"].get<float>();

            if (p.contains("penetrationPercent") && p["penetrationPercent"].is_number())
                out.physics.penetrationPercent = p["penetrationPercent"].get<float>();

            if (p.contains("maxSubsteps") && p["maxSubsteps"].is_number_integer())
                out.physics.maxSubsteps = p["maxSubsteps"].get<int>();

            if (p.contains("ccdMinSizeFactor") && p["ccdMinSizeFactor"].is_number())
                out.physics.ccdMinSizeFactor = p["ccdMinSizeFactor"].get<float>();
        }
    }
    catch (const json::parse_error& e)
    {
        // e.byte = posición aproximada del fallo en bytes dentro del string
        const auto [line, col] = ByteToLineCol(text, e.byte);
        LogError("Engine::LoadConfigFromFile",
            (std::string("JSON parse error at ") + path +
                " line " + std::to_string(line) +
                ", col " + std::to_string(col) +
                ": " + e.what()).c_str());
        return false;
    }
    catch (const json::exception& e)
    {
        // Type errors / out_of_range / etc.
        LogError("Engine::LoadConfigFromFile",
            (std::string("JSON error in ") + path + ": " + e.what()).c_str());
        return false;
    }
    catch (...)
    {
        LogError("Engine::LoadConfigFromFile",
            (std::string("Unknown error parsing JSON config: ") + path).c_str());
        return false;
    }

    return true;
}

void Engine::FrameTick_() noexcept
{
	if (!mRunning) return;

    auto* time = TimeManager::GetInstancePtr();

    time->Tick();
    mAccumulator += time->deltaTime;
}

void Engine::PollInput_() noexcept
{
	if (!mRunning) return;

    auto* time = TimeManager::GetInstancePtr();
    float pollStart = (float) time->timeSinceStart;

    auto* input = InputManager::GetInstancePtr();

	input->Update();
    if (mLogStats && input && input->KeyPressed(SDL_SCANCODE_F3))
    {
        mShowStatsOverlay = !mShowStatsOverlay;
        mStatsNextRefresh = 0.0;
    }
    UIManager::GetInstancePtr()->Begin();

    mPollInputTime = (float) time->timeSinceStart - pollStart;
}

void Engine::DoFixedUpdates_(float dt) noexcept
{
	if (!mRunning) return;

    auto* time = TimeManager::GetInstancePtr();

    // Callback de usuario por frame (si está configurado)
    if (mOnFixedUpdate)
        mOnFixedUpdate(mFixedDt);
    // FixedUpdate de la escena activa
    SceneManager::GetInstancePtr()->FixedUpdate(mFixedDt);

    PhysicsManager::GetInstancePtr()->Step(mFixedDt);
    CollisionManager::GetInstancePtr()->DetectAndDispatch();
}

void Engine::DoUpdate_(float dt) noexcept
{
	if (!mRunning) return;

    auto* time = TimeManager::GetInstancePtr();
    auto* scenes = SceneManager::GetInstancePtr();
    float updateStart = (float) time->timeSinceStart;

	// Callback de usuario por frame (si está configurado)
    if (mOnUpdate)
        mOnUpdate(dt);
	scenes->Update(dt);

	mUpdateTime = (float) time->timeSinceStart - updateStart;
}

void Engine::DoRender_() noexcept
{
	if (!mRunning) return;

    auto* time = TimeManager::GetInstancePtr();
    float pollStart = (float) time->timeSinceStart;

	auto* scenes = SceneManager::GetInstancePtr();
	auto* render = RenderManager::GetInstancePtr();
	auto* ui = UIManager::GetInstancePtr();

    render->nDrawCallsThisFrame = 0;
    render->nDebugDrawCallsThisFrame = 0;
    render->nUIDrawCallsThisFrame = 0;
    render->nRenderedSpritesThisFrame = 0;

    render->Begin(*(scenes->GetActive()->GetCamera()));

    scenes->Render();      // Render de todos los GameObjects y Gizmos

    ui->End();
    if (mShowStatsOverlay)
    {
        DrawStatsOverlay_();
        ui->End();
    }

    render->End();         // Window present

	mRenderTime = (float) time->timeSinceStart - pollStart;
}

static std::string SPrintf_(const char* fmt, ...) noexcept
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return std::string(buf);
}

static inline double SafePct_(double part, double total) noexcept
{
    if (!(total > 1e-12)) return 0.0;
    return (part * 100.0) / total;
}

void Engine::DrawStatsOverlay_() noexcept
{
    auto* win = WindowManager::GetInstancePtr();
    auto* time = TimeManager::GetInstancePtr();
    auto* assets = AssetManager::GetInstancePtr();
    auto* render = RenderManager::GetInstancePtr();
    auto* collision = CollisionManager::GetInstancePtr();
    auto* scenes = SceneManager::GetInstancePtr();
    auto* ui = UIManager::GetInstancePtr();

    if (!win || !time || !assets || !render || !collision || !scenes || !ui) return;

    // refresca texto a 4 Hz (evita gastar TTF cada frame generando strings)
    const double now = time->timeSinceStart;
    if (now >= mStatsNextRefresh)
    {
        mStatsNextRefresh = now + 0.25; // 4 veces por segundo
        mStatsLines.clear();
        mStatsLines.reserve(64);

        Vec2I res = win->GetDrawableSize();
        Vec2I aspectAsFr = GetAspectRatioAsFraction();

        const double dt = (double)time->deltaTime;

        const float fpsNow = GetFPS();
        averageFps += fpsNow;
        averageFps *= 0.5f;

        averageFixedUpdatesPerFrame += fixedUpdatesDoneThisFrame;
        averageFixedUpdatesPerFrame = (int)std::ceil(((float)averageFixedUpdatesPerFrame) * 0.5f);

        blockSizes.clear();

        mStatsLines.push_back("=== XEngine Stats ===");
        mStatsLines.push_back(SPrintf_("Time Since Start:                   %.1f sec", time->SinceStart()));
        mStatsLines.push_back(SPrintf_("FPS:                                %.2f fps", fpsNow));
        mStatsLines.push_back(SPrintf_("Average fps:                        %.2f fps", averageFps));
        mStatsLines.push_back(SPrintf_("Delta Time (limited to 0.25):       %.6f sec", dt));
        mStatsLines.push_back(SPrintf_("Fixed Dt:                           %.6f sec", (double)mFixedDt));
        mStatsLines.push_back(SPrintf_("Accumulator this frame:             %.6f sec", (double)mAccumulator));
        mStatsLines.push_back(SPrintf_("Fixed updates this frame:           %d fixed updates", fixedUpdatesDoneThisFrame));
        mStatsLines.push_back(SPrintf_("Average Fixed Updates per frame:    %d fixed updates", averageFixedUpdatesPerFrame));
        mStatsLines.push_back("");

        blockSizes.push_back(9);

        if (!mCfg.multiThreading)
        {
            mStatsLines.push_back("Frame Time:");
            mStatsLines.push_back(SPrintf_(" * Input Update:     %.6f sec (%.1f%%)", (double)mPollInputTime, SafePct_((double)mPollInputTime, dt)));
            mStatsLines.push_back(SPrintf_(" * Fixed Update:     %.6f sec (%.1f%%)", (double)mFixedUpdateTime, SafePct_((double)mFixedUpdateTime, dt)));
            mStatsLines.push_back(SPrintf_(" * Update:           %.6f sec (%.1f%%)", (double)mUpdateTime, SafePct_((double)mUpdateTime, dt)));
            mStatsLines.push_back(SPrintf_(" * Render:           %.6f sec (%.1f%%)", (double)mRenderTime, SafePct_((double)mRenderTime, dt)));
        }
        else
        {
            mStatsLines.push_back("Thread Time:");
            mStatsLines.push_back(SPrintf_(" * Input Update:     %.6f sec", (double)mPollInputTime));
            mStatsLines.push_back(SPrintf_(" * Fixed Update:     %.6f sec", (double)mFixedUpdateTime));
            mStatsLines.push_back(SPrintf_(" * Update:           %.6f sec", (double)mUpdateTime));
            mStatsLines.push_back(SPrintf_(" * Render:           %.6f sec", (double)mRenderTime));
        }
        mStatsLines.push_back("");

        blockSizes.push_back(5);

        mStatsLines.push_back("=== Renderer Stats ===");
        mStatsLines.push_back(SPrintf_("Resolution:                      %u X %u px", (unsigned)res.x, (unsigned)res.y));
        mStatsLines.push_back(SPrintf_("Aspect Ratio:                    %u : %u", (unsigned)aspectAsFr.x, (unsigned)aspectAsFr.y));
        mStatsLines.push_back("Draw Calls (per frame):");
        mStatsLines.push_back(SPrintf_(" * World Draw Calls:             %d calls", render->nDrawCallsThisFrame));
        mStatsLines.push_back(SPrintf_(" * UI Draw Calls:                %d calls", render->nUIDrawCallsThisFrame));
        mStatsLines.push_back(SPrintf_(" * Debug Draw Calls:             %d calls", render->nDebugDrawCallsThisFrame));
        mStatsLines.push_back(SPrintf_("Sprites Rendered (per frame):    %d sprites", render->nRenderedSpritesThisFrame));
        mStatsLines.push_back("");

        blockSizes.push_back(8);

        mStatsLines.push_back("=== Scene Stats ===");
        mStatsLines.push_back(SPrintf_("Current Scene ID:               \"%s\"", scenes->mActiveId.c_str()));
        mStatsLines.push_back(SPrintf_("Game Objects In Scene:          %" PRIu64 " objects", (std::uint64_t)scenes->GetActive()->mEntities.size()));
        mStatsLines.push_back(SPrintf_("GOs Instantiated This Frame:    %" PRIu64 " objects", (std::uint64_t)scenes->GetActive()->mNew.size()));
        mStatsLines.push_back(SPrintf_("GOs Destroyed This Frame:       %" PRIu64 " objects", (std::uint64_t)scenes->GetActive()->mDestroyQueue.size()));
        mStatsLines.push_back("");

        blockSizes.push_back(5);

        mStatsLines.push_back("=== Collision Stats ===");
        mStatsLines.push_back(SPrintf_("Registered Colliders:              %" PRIu64 " colliders", (std::uint64_t)collision->mRegisteredColliders.size()));
        mStatsLines.push_back(SPrintf_("Broadphase Checks (per frame):     %d checks", collision->nBroadphaseTestsThisFrame));
        mStatsLines.push_back(SPrintf_("Narrowphase Checks (per frame):    %d checks", collision->nNarrowphaseTestsThisFrame));
        mStatsLines.push_back(SPrintf_("Contacts Built This Frame:         %" PRIu64 " contacts", (std::uint64_t)collision->nContactsBuiltThisFrame));
        mStatsLines.push_back(SPrintf_("Active Pairs:                      %" PRIu64 " pairs", (std::uint64_t)collision->mCurrPairs.size()));
        mStatsLines.push_back("");

        blockSizes.push_back(6);

        mStatsLines.push_back("=== Assets Stats ===");
        mStatsLines.push_back(SPrintf_("Textures Loaded:                %" PRIu64 " textures", (std::uint64_t)assets->mTextures.size()));
        mStatsLines.push_back(SPrintf_("Fonts Loaded:                   %" PRIu64 " fonts", (std::uint64_t)assets->mFonts.size()));
        mStatsLines.push_back(SPrintf_("SFX Loaded:                     %" PRIu64 " SFX", (std::uint64_t)assets->mSfx.size()));
        mStatsLines.push_back(SPrintf_("Musics Loaded:                  %" PRIu64 " musics", (std::uint64_t)assets->mMusic.size()));
        mStatsLines.push_back(SPrintf_("Total Memory Used By Assets:    %.2f MB", (double)assets->memoryUsed));
        mStatsLines.push_back("");

        blockSizes.push_back(6);

        std::uint64_t bodiesRegistered = (std::uint64_t)physics->mBodies.size();
        std::uint64_t bodiesActive = 0;
        std::uint64_t bodiesDynamic = 0, bodiesKinematic = 0, bodiesStatic = 0;
        std::uint64_t bodiesCCD = 0;
        std::uint64_t bodiesWithConstraints = 0;
        std::uint64_t bodiesFreezeRot = 0, bodiesFreezePos = 0;

        for (auto& [rb, active] : physics->mBodies)
        {
            if (!rb) continue;

            if (active) bodiesActive++;

            switch (rb->bodyType)
            {
            case RigidBody2D::BodyType::Dynamic:   bodiesDynamic++; break;
            case RigidBody2D::BodyType::Kinematic: bodiesKinematic++; break;
            case RigidBody2D::BodyType::Static:    bodiesStatic++; break;
            }

            if (rb->bodyType == RigidBody2D::BodyType::Dynamic &&
                rb->collisionDetection == RigidBody2D::CollisionDetection::Continuous)
            {
                bodiesCCD++;
            }
        }

        mStatsLines.push_back("=== Physics Stats ===");
        mStatsLines.push_back(SPrintf_("Physics Step Time:                %.6f sec", physics->stepTimeSec));
        mStatsLines.push_back(SPrintf_(" * Integrate Time:                %.6f sec (%.1f%%)", physics->integrateTimeSec, SafePct_(physics->integrateTimeSec, physics->stepTimeSec)));
        mStatsLines.push_back(SPrintf_(" * BuildContacts Time:            %.6f sec (%.1f%%)", physics->buildContactsTimeSec, SafePct_(physics->buildContactsTimeSec, physics->stepTimeSec)));
        mStatsLines.push_back(SPrintf_(" * Solve Time:                    %.6f sec (%.1f%%)", physics->solveTimeSec, SafePct_(physics->solveTimeSec, physics->stepTimeSec)));
        mStatsLines.push_back(SPrintf_("Registered Bodies:                %" PRIu64 " bodies", bodiesRegistered));
        mStatsLines.push_back(SPrintf_("Active Bodies:                    %" PRIu64 " bodies", bodiesActive));
        mStatsLines.push_back(SPrintf_("Body Types:                       dyn %" PRIu64 " / kin %" PRIu64 " / static %" PRIu64, bodiesDynamic, bodiesKinematic, bodiesStatic));
        mStatsLines.push_back(SPrintf_("CCD Enabled (Dynamic):            %" PRIu64 " bodies", bodiesCCD));
        mStatsLines.push_back(SPrintf_("Gravity:                          [%.3f, %.3f] m/s^2", (double)physics->gravity.x, (double)physics->gravity.y));
        mStatsLines.push_back(SPrintf_("Max Substeps:                     %d substeps", physics->maxSubsteps));
        mStatsLines.push_back(SPrintf_("CCD Min Size Factor:              %.3f units", (double)physics->ccdMinSizeFactor));
        mStatsLines.push_back(SPrintf_("Substeps This Frame:              %d substeps", physics->nSubstepsThisFrame));
        mStatsLines.push_back(SPrintf_("Solver Iterations:                %d interactions", physics->solverIterations));
        mStatsLines.push_back(SPrintf_("Contacts Processed This Frame:    %" PRIu64 " contacts", (std::uint64_t)physics->nContactsProcessedThisFrame));

        blockSizes.push_back(15);
    }

    Font* consoleFontBold = assets->GetEngineDefaultFont(true);
    Font* consoleFontNormal = assets->GetEngineDefaultFont(false);

    if (win->GetDrawableSize() == screenSize)
    {
        for (int i = 0; i < mStatsLines.size(); i++)
        {
            Vec2 coords = mStatsLinesCoords[i].coords;
            bool title = mStatsLinesCoords[i].isTitle;
            ui->Label(mStatsLines[i], coords.x, coords.y, title ? *consoleFontBold : *consoleFontNormal, title ? Color::White() : Color::Green());
        }
    }
    else
    {
        screenSize = win->GetDrawableSize();
        mStatsLinesCoords.clear();
        statsPanelRect = Rect(0.f, 0.f, 0.f, 0.f);

        // --------- DIBUJO ----------
        // Panel semitransparente arriba-izq
        const float pad = 10.0f;
        const float x = pad + 12;
        const float y = pad + 6;

        const float lineH = 18.0f;
        const float charW = 8.235294f;

        int biggestLineSizeInLastRow = 0;
        int biggestBlockSize = 0;

        float panelH = 0.f;

        for (const auto& s : blockSizes)
            biggestBlockSize = std::max(biggestBlockSize, s);

        // Texto
        float cy = y;
        float cx = x;

        if (biggestBlockSize * lineH < (float)win->GetDrawableSize().y - lineH - pad)
        {
            int lineIndex = 0;
            int blockIndex = 0;
            bool firstBlock = true;

            for (int i = 0; i < mStatsLines.size(); i++)
            {
                auto line = mStatsLines[i];

                if (lineIndex == 0 && !firstBlock)
                {
                    ui->Label(line, cx, cy, *consoleFontNormal);
                    mStatsLinesCoords.push_back({ Vec2(cx, cy), false });
                    cy += lineH;
                    line = mStatsLines[++i];

                    if (cy + (blockSizes[blockIndex] * lineH) >= (float)win->GetDrawableSize().y - lineH - pad)
                    {
                        cy = y;
                        cx += biggestLineSizeInLastRow * charW + 40.f;
                        biggestLineSizeInLastRow = 0;
                    }
                }

                ui->Label(line, cx, cy, lineIndex == 0 ? *consoleFontBold : *consoleFontNormal, lineIndex == 0 ? Color::White() : Color::Green());
                mStatsLinesCoords.push_back({ Vec2(cx, cy), lineIndex == 0 });
                cy += lineH;
                biggestLineSizeInLastRow = std::max(biggestLineSizeInLastRow, (int)line.size());

                panelH = std::max(cy, panelH);

                if (++lineIndex >= blockSizes[blockIndex])
                {
                    lineIndex = 0;
                    if (firstBlock) firstBlock = false;
                    if (++blockIndex >= blockSizes.size()) break;
                }
            }
        }
        else
        {
            for (const auto& line : mStatsLines)
            {
                if (cy >= (float)win->GetDrawableSize().y - lineH - pad)
                {
                    cy = y;
                    cx += biggestLineSizeInLastRow * charW + pad;
                    biggestLineSizeInLastRow = 0;
                }
                ui->Label(line, cx, cy, *consoleFontNormal, Color::Green());
                mStatsLinesCoords.push_back({ Vec2(cx, cy), true });
                cy += lineH;
                biggestLineSizeInLastRow = std::max(biggestLineSizeInLastRow, (int)line.size());
                panelH = std::max(cy, panelH);
            }
        }

        const float panelW = cx + biggestLineSizeInLastRow * charW + pad;
        panelH += pad;

        statsPanelRect = Rect(6, 6, panelW, panelH);
    }

    //Fondo
    render->DrawRectScreen(statsPanelRect, Color::Black(200), true);
    // Borde
    render->DrawRectScreen(statsPanelRect, Color::White(100), false);
}

void Engine::ManageSceneQueues_() noexcept
{
	if (!mRunning) return;
	SceneManager::GetInstancePtr()->GetActive()->FlushDestroyQueue();
    SceneManager::GetInstancePtr()->GetActive()->ProcessNewObjects();
}

void Engine::ManageSceneChanges_() noexcept
{
	if (!mRunning) return;
	SceneManager::GetInstancePtr()->ApplyPendingScene();
}

WindowManager* Engine::Window() noexcept { return WindowManager::GetInstancePtr(); }
TimeManager* Engine::Time() noexcept { return TimeManager::GetInstancePtr(); }
InputManager* Engine::Input() noexcept { return InputManager::GetInstancePtr(); }
AssetManager* Engine::Assets() noexcept { return AssetManager::GetInstancePtr(); }
SoundManager* Engine::Sound() noexcept { return SoundManager::GetInstancePtr(); }
UIManager* Engine::UI() noexcept { return UIManager::GetInstancePtr(); }
SceneManager* Engine::Scenes() noexcept { return SceneManager::GetInstancePtr(); }
PhysicsManager* Engine::Physics() noexcept { return PhysicsManager::GetInstancePtr(); }
RandomManager* Engine::Random() noexcept { return RandomManager::GetInstancePtr(); }
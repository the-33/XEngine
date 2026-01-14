#pragma once

#include <functional>
#include <string>

#include "BaseTypes.h"
#include "Property.h"
#include "Singleton.h"

// forward declarations
class WindowManager;
class TimeManager;
class InputManager;
class AssetManager;
class SoundManager;
class RenderManager;
class CollisionManager;
class UIManager;
class SceneManager;
class PhysicsManager;
class RandomManager;
class Camera2D;
class Collider2D;

class Engine : public Singleton<Engine>
{
    friend class Singleton<Engine>;
private:
    struct Config
    {
        bool multiThreading = false;
		bool logStats = false;
        bool logErrors = true;

        struct Window
        {
            int width = 1280;
            int height = 720;
            bool fullscreen = false;
            const char* title = "Game";

			Window() noexcept = default;
        } window;

        struct Renderer
        {
            bool vsync = true;
            bool hardwareAcceleration = true;
            Color bgColor = { 0, 0, 0, 255 };

			Renderer() noexcept = default;
        } renderer;

        struct Input
        {
            bool startTextInput = false;
            bool startMouseLocked = false;
            bool startMouseHidden = false;

			Input() noexcept = default;
        } input;

        struct Time
        {
            float fixedDt = 1.0f / 60.0f;

			Time() noexcept = default;
        } time;

        struct Assets
        {
            std::string assetsFolderPath = "./assets/";
            std::string defaultFontRelativePath = "fonts/ui_default.ttf";

			Assets() noexcept = default;
        } assets;

        struct Sound {
            int   sfxChannels = 32;
            float masterVolume = 1.0f;
            bool  enableDucking = false;
            float duckVolume = 0.6f;
            float duckAttackSec = 0.02f;
            float duckReleaseSec = 0.25f;

			Sound() noexcept = default;
        } sound;

        struct UIStyle
        {
            Color text{ 240,240,240,255 };
            Color btn{ 60,60,60,255 };
            Color btnHot{ 80,80,80,255 };
            Color btnActive{ 40,40,40,255 };
            Color outline{ 200,200,200,255 };
            float padding = 6.f;
            std::string defaultFontKey = "ui_default";
            int defaultFontptSize = 24;

			UIStyle() noexcept = default;

            UIStyle& operator= (const UIStyle& other) noexcept
            {
                text = other.text;
                btn = other.btn;
                btnHot = other.btnHot;
                btnActive = other.btnActive;
                outline = other.outline;
                padding = other.padding;
                defaultFontKey = other.defaultFontKey;
                return *this;
            }
        } uiStyle;

        struct Physics
        {
            Vec2 baseGravity = { 0.f, 9.81f };
            float penetrationSlop = 0.01f;
            float penetrationPercent = 0.8f;
            int maxSubsteps = 8;
            float ccdMinSizeFactor = 0.5f;
        } physics;

        Config() noexcept = default;
    };

    friend class Collider2D;

	const std::string kDefaultConfigPath = "./XEngineConfig.json";
    static bool LoadConfigFromFile(const std::string& path, Config& out) noexcept;

    void FrameTick_() noexcept;
    void PollInput_() noexcept;
    void DoFixedUpdates_(float dt) noexcept;
    void DoUpdate_(float dt) noexcept;
    void DoRender_() noexcept;

	void DrawStatsOverlay_() noexcept;
    bool mShowStatsOverlay = false;

    double mStatsNextRefresh = 0.0;
    std::vector<std::string> mStatsLines;
    std::vector<int> blockSizes;

    struct lineCoord
    {
        Vec2 coords = Vec2::Zero();
        bool isTitle = false;
    };

    std::vector<lineCoord> mStatsLinesCoords;
    Vec2I screenSize = Vec2I::Zero();
    Rect statsPanelRect = Rect(0.f, 0.f, 0.f, 0.f);

    void ManageSceneQueues_() noexcept;
    void ManageSceneChanges_() noexcept;

    Config mCfg;

    bool mLogStats = true;
    bool mLogErrors = true;

    float mFixedDt = 1/60.f;
    bool mStarted = false;
    bool mRunning = false;
    bool SafeToQuit = true;
    float mAccumulator = 0.f;

    float mPollInputTime = 0.f;
	float mUpdateTime = 0.f;
	float mFixedUpdateTime = 0.f;
	float mRenderTime = 0.f;
    int fixedUpdatesDoneThisFrame = 0;
    int averageFixedUpdatesPerFrame = 0;
    float lastAccumulator = 0.f;
    float averageFps = 0.f;

    float mAspectRatio = 0.f;
    Vec2I  mAspectRatioAsFraction = Vector2::Zero;

    std::function<void(Engine&)> mOnInit;
    std::function<void(float)> mOnUpdate;
    std::function<void(float)> mOnFixedUpdate;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Engine() noexcept;
    ~Engine();

    bool StartImpl(const std::string cfg) noexcept;      // inicializa singletons
	int RunSingleThreadingImpl() noexcept;               // bucle principal singlethread
	int RunMultiThreadingImpl() noexcept;                // bucle principal multithread
    void StopImpl() noexcept;                            // marca fin de loop

    bool ApplyStartingScene() const noexcept;

public:
    // --- API estática de alto nivel ---
    static bool Start(const std::string& configPath = "./") noexcept
    {
        if (!CreateSingleton()) return false;
        return GetInstance().StartImpl(configPath);
    }

    static int Run() noexcept
    {
        if (auto* e = GetInstancePtr())
        {
            // Callback de inicialización de usuario (si la hay)
            if (e->mOnInit)
                e->mOnInit(*e);

            if (!(e->ApplyStartingScene())) Stop();

            if (!e->mCfg.multiThreading)
                return e->RunSingleThreadingImpl();
            else
				return e->RunMultiThreadingImpl();
        }
        return -1;
    }

    static void Stop() noexcept
    {
        if (auto* e = GetInstancePtr())
        {
            e->StopImpl();
            DestroySingleton();    // aquí destruyes el Engine y, en tu Stop, apagas SDL, singletons, etc.
        }
    }

    static void SetInitCallback(std::function<void(Engine&)> cb) noexcept 
    { 
        if (auto* e = GetInstancePtr())
            e->mOnInit = std::move(cb); 
    }
    static void SetUpdateCallback(std::function<void(float)> cb) noexcept 
    { 
        if (auto* e = GetInstancePtr())
            e->mOnUpdate = std::move(cb);
    }
    static void SetFixedUpdateCallback(std::function<void(float)> cb) noexcept 
    { 
        if (auto* e = GetInstancePtr())
            e->mOnFixedUpdate = std::move(cb);
    }

private:
    static bool IsRunning() noexcept
    {
        if (auto* e = GetInstancePtr())
            return e->mRunning;
        return false;
    }

    static WindowManager* Window() noexcept;
    static TimeManager* Time() noexcept;
    static InputManager* Input() noexcept;
    static AssetManager* Assets() noexcept;
    static SoundManager* Sound() noexcept;
    static UIManager* UI() noexcept;
    static SceneManager* Scenes() noexcept;
    static PhysicsManager* Physics() noexcept;
    static RandomManager* Random() noexcept;

    static Camera2D* GetCamera() noexcept;

    static inline float FixedDelta() noexcept 
    {
        if (auto* e = GetInstancePtr())
            return e->mFixedDt;
        return 0.f;
    }

    static bool GetLogErrors() noexcept
    {
        if (auto* e = GetInstancePtr())
            return e->mLogErrors;
        return false;
    }

    static inline float GetFPS() noexcept;
    static inline float GetAspectRatio() noexcept;
    static inline Vec2I GetAspectRatioAsFraction() noexcept;

private:
    // -------- Property getters (NO estáticos) --------
    bool IsRunning_() const noexcept { return mRunning; }

    WindowManager* Window_()  const noexcept { return Engine::Window(); }
    TimeManager* Time_()    const noexcept { return Engine::Time(); }
    InputManager* Input_()   const noexcept { return Engine::Input(); }
    AssetManager* Assets_()  const noexcept { return Engine::Assets(); }
    SoundManager* Sound_()   const noexcept { return Engine::Sound(); }
    UIManager* UI_()      const noexcept { return Engine::UI(); }
    SceneManager* Scenes_()  const noexcept { return Engine::Scenes(); }
    PhysicsManager* Physics_() const noexcept { return Engine::Physics(); }
    RandomManager* Random_() const noexcept { return Engine::Random(); }

    Camera2D* Camera_() const noexcept { return Engine::GetCamera(); }

    float FixedDelta_() const noexcept { return mFixedDt; }
    bool  LogErrors_()  const noexcept { return mLogErrors; }

    float GetFPS_() const noexcept;
    float GetAspectRatio_() const noexcept;
    Vec2I GetAspectRatioAsFraction_() const noexcept;

public:
    // ======================================================
    // PropertyROs (lectura)
    // ======================================================

    using IsRunningProperty = PropertyRO<Engine, bool, &Engine::IsRunning_>;
    IsRunningProperty isRunning{ this };

    using WindowProperty = PropertyRO<Engine, WindowManager*, &Engine::Window_>;
    WindowProperty window{ this };

    using TimeProperty = PropertyRO<Engine, TimeManager*, &Engine::Time_>;
    TimeProperty time{ this };

    using InputProperty = PropertyRO<Engine, InputManager*, &Engine::Input_>;
    InputProperty input{ this };

    using AssetsProperty = PropertyRO<Engine, AssetManager*, &Engine::Assets_>;
    AssetsProperty assets{ this };

    using SoundProperty = PropertyRO<Engine, SoundManager*, &Engine::Sound_>;
    SoundProperty sound{ this };

    using UIProperty = PropertyRO<Engine, UIManager*, &Engine::UI_>;
    UIProperty ui{ this };

    using ScenesProperty = PropertyRO<Engine, SceneManager*, &Engine::Scenes_>;
    ScenesProperty scenes{ this };

    using PhysicsProperty = PropertyRO<Engine, PhysicsManager*, &Engine::Physics_>;
    PhysicsProperty physics{ this };

    using RandomProperty = PropertyRO<Engine, RandomManager*, &Engine::Random_>;
    RandomProperty random{ this };

    using CameraProperty = PropertyRO<Engine, Camera2D*, &Engine::Camera_>;
    CameraProperty camera{ this };

    using FixedDeltaProperty = PropertyRO<Engine, float, &Engine::FixedDelta_>;
    FixedDeltaProperty fixedDelta{ this };

    using LogErrorsProperty = PropertyRO<Engine, bool, &Engine::LogErrors_>;
    LogErrorsProperty logErrors{ this };

    using FPSProperty = PropertyRO<Engine, float, &Engine::GetFPS_>;
    FPSProperty fps{ this };

    using AspectRatioProperty = PropertyRO<Engine, float, &Engine::GetAspectRatio_>;
    AspectRatioProperty aspectRatio{ this };

    using AspectRatioFractionProperty = PropertyRO<Engine, Vec2I, &Engine::GetAspectRatioAsFraction_>;
    AspectRatioFractionProperty aspectRatioFraction{ this };
};
#pragma once
/** @file Engine.h
 * @brief Engine startup, global services and game-loop state.
 */

#include <functional>
#include <array>
#include <vector>
#include <cstdint>
#include <string>

#include "BaseTypes.h"
#include "SpriteProjection.h"
#include "Property.h"
#include "Singleton.h"
#include "Assets.h"
#include "StatsOverlayLayout.h"
#include "RenderDiagnostics.h"
#include "BuildConfig.h"
#include "Platform.h"
#include "SDL_stdinc.h"

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
struct SDL_mutex;

/** @brief Immutable-style camera parameters copied into a render snapshot.
 * @note Units are world-space distances except for the dimensionless zoom.
 */
struct CameraState
{
    /** @brief Camera center X in world coordinates.
     */
    float centerX = 0.f;
    /** @brief Camera center Y in world coordinates.
     */
    float centerY = 0.f;
    /** @brief Unzoomed horizontal world-space view span.
     */
    float baseViewW = 0.f;
    /** @brief Unzoomed vertical world-space view span.
     */
    float baseViewH = 0.f;
    /** @brief Camera zoom multiplier.
     */
    float zoom = 1.f;
};

/** @brief CPU-side render command that can target world or screen coordinates.
 * @warning Texture and font pointers are non-owning. Do not free those assets
 * until the recorded frame has finished using them.
 */
struct DrawCmd
{
    /** @brief Available type modes for DrawCmd.
     */
    enum class Type
    {
        WorldTex,    ///< Texture rendered in camera-projected world coordinates.
        WorldRect,   ///< Filled or outlined rectangle in world coordinates.
        WorldLine,   ///< Line segment in world coordinates.

        ScreenRect,  ///< Rectangle in window screen pixels.
        ScreenImage, ///< Image in window screen pixels without camera projection.
        ScreenText,  ///< Text in window screen pixels.
    };

    /** @brief Command kind: world texture, world rectangle/line, or screen rectangle/image/text.
     */
    Type type;

    // Common
    /** @brief RGBA tint or primitive color.
     */
    Color color{ 255,255,255,255 };

    // WorldTex
    /** @brief Referenced texture; its lifetime must cover rendering of this command.
     */
    const Texture* tex = nullptr;
    /** @brief Destination rectangle measured in world-space units.
     */
    Rect dstWorld{};
    /** @brief Source rectangle measured in source-texture pixels.
     */
    Rect srcPx{};
    /** @brief Whether a custom texture source rectangle is set.
     */
    bool hasSrc = false;
    // Immutable CPU-only sprite pose; projected by workers if populated.
    /** @brief Whether CPU sprite projection is deferred to the preparation stage.
     */
    bool hasDeferredSpritePose = false;
    /** @brief Index into the immutable sprite-pose sidecar of the snapshot.
     */
    std::uint32_t spritePoseIndex = 0;
    /** @brief World texture rotation in degrees.
     */
    float rotationDeg = 0.f;
    /** @brief Normalized sprite rotation pivot.
     */
    Vec2  pivot01{ 0.5f,0.5f };
    /** @brief Flip the sprite horizontally.
     */
    bool flipX = false;
    /** @brief Flip the sprite vertically.
     */
    bool flipY = false;

    // WorldRect
    /** @brief World-space rectangle for the rectangular primitive command.
     */
    Rect worldRect{};
    /** @brief Whether rectangle primitives have a filled interior.
     */
    bool filled = true;

    // WorldLine
    /** @brief World-space X/Y coordinates of both endpoints for `WorldLine` commands.
     */
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;

    /** @brief Marks a command as debug drawing for render diagnostics.
     */
    bool debugDraw = false; // Keeps debug statistics intact when recording commands.

    // Optional CPU-only precomputed geometry for the recorded render path.
    /** @brief Whether the command already has precomputed screen-space geometry.
     */
    bool hasPreparedGeometry = false;
    /** @brief Visibility flag from the CPU render-preparation stage.
     */
    bool preparedVisible = true;
    /** @brief Precomputed screen-space destination geometry.
     */
    Rect preparedScreen{};
    /** @brief Precomputed screen-space rotation pivot.
     */
    Vec2 preparedPivot{};

    // ScreenRect / ScreenImage / ScreenText
    /** @brief Destination rectangle in screen pixels.
     */
    Rect dstScreen{};

    // ScreenImage uses tex + srcPx/hasSrc + flips + tint in color
    // ScreenText
    /** @brief Font used by screen-space text commands.
     */
    const Font* font = nullptr;
    /** @brief UTF-8 string used by text commands.
     */
    std::string text;
    /** @brief Screen-space text origin in drawable pixels (`textX`, `textY`).
     */
    float textX = 0.f, textY = 0.f; // px
};

/** @var float DrawCmd::y1
 * @brief First line endpoint's Y coordinate (world space).
 */
/** @var float DrawCmd::x2
 * @brief Second line endpoint's X coordinate (world space).
 */
/** @var float DrawCmd::y2
 * @brief Second line endpoint's Y coordinate (world space).
 */
/** @var float DrawCmd::textY
 * @brief Screen-space text position on the Y axis, in pixels.
 */

/** @brief Captured camera, window and render commands for one frame.
 * @note Game code normally uses RenderManager rather than constructing snapshots.
 */
struct Snapshot
{
    /** @brief Whether this recorded render snapshot is initialized.
     */
    bool valid = false;
    /** @brief Recorded logical window width.
     */
    int winW = 0;
    /** @brief Recorded logical window height.
     */
    int winH = 0;

    /** @brief Recorded camera parameters for this frame.
     */
    CameraState cam;
    /** @brief Ordered CPU-side draw commands.
     */
    std::vector<DrawCmd> cmds;
    // Compact sidecar: avoids inflating every command (UI, world rect, etc).
    /** @brief Compact per-sprite projection metadata referenced by draw commands.
     */
    std::vector<SpriteProjection::Pose> spritePoses;
};

/** @brief Central engine lifecycle and access to active managers, camera and timing.
 * @ingroup xengine_core
 */
class Engine : public Singleton<Engine>
{
    friend class Singleton<Engine>;
private:
    struct Config
    {
        bool multiThreading = false;
        bool profileCsv = false;
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

        struct Collision
        {
            std::string broadphase = "bruteforce";
            float cellSize = 2.f;
            bool autoCellSize = false;
        } collision;

        struct Renderer
        {
            bool vsync = true;
            bool hardwareAcceleration = true;
            // Backend selected for the current platform (default: SDL chooses).
            std::string backend = "auto";
            bool backendFallback = true;
            bool parallelPreparation = true;
            std::size_t parallelMinCommands = RenderDiagnostics::kParallelThreshold;
            RenderDiagnostics::Selection parallelSelection = RenderDiagnostics::Selection::Fixed;
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
    static bool LoadConfigFromJson(const std::string& text, const std::string& origin, Config& out) noexcept;

    void FrameTick_() noexcept;
    void PollInput_() noexcept;
    void DoFixedUpdates_(float dt) noexcept;
    void DoUpdate_(float dt) noexcept;
    void DoRender_() noexcept;


	void DrawStatsOverlay_() noexcept;
    bool mShowStatsOverlay = false;

    double mStatsNextRefresh = 0.0;
    std::vector<std::string> mStatsLines;
    // On-screen profiler pages are rebuilt only when values or viewport change.
    struct StatsDisplayRow
    {
        std::string label;
        std::string value;
        bool heading = false;
        // Text clipping and metrics are cached at the overlay's 4 Hz refresh.
        std::string visibleLabel;
        std::string visibleValue;
        float visibleValueWidth = 0.f;
    };
    std::vector<StatsDisplayRow> mStatsDisplayRows;
    XEngineStatsOverlay::Layout mStatsLayout;
    std::size_t mStatsPage = 0;
    Vec2I mStatsViewport = Vec2I::Zero();
    float mStatsColumnWidth = 0.f;
    float mStatsLineHeight = 0.f;

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
    // 0: regular real-time accumulator. Nonzero: exactly N fixed steps per frame.
    // Intended for controlled benchmarks, independent of frame wall time.
    unsigned mDeterministicFixedStepsPerFrame = 0;


    float mPollInputTime = 0.f;
	float mUpdateTime = 0.f;
	float mFixedUpdateTime = 0.f;
    float mRenderTime = 0.f;
    float mRenderRecordTime = 0.f;
    float mRenderPrepareTime = 0.f;
    float mRenderSubmitTime = 0.f;
    float mRenderPresentTime = 0.f;
    float mRenderWorkerWaitTime = 0.f;
    float mRenderWorldTime = 0.f;
    float mRenderUiTime = 0.f;
    unsigned mLastRenderEligiblePrevious = 0;
    std::uint64_t mLastRenderWeightedPrevious = 0;
    std::uint64_t mLastRenderEffectivePrevious = 0;
    bool mPreviousExecutedParallel = false;
    bool mExecutedPathPrimed = false;
    unsigned mRenderExecutedSwitched = 0;
    unsigned mRenderParallelExecuted = 0;
    unsigned mRenderParallelFallback = 0;
    bool mPreviousRenderParallel = false;
    bool mRenderPathPrimed = false;
    unsigned mRenderPathSwitched = 0;
    RenderDiagnostics::Path mLastRenderPath = RenderDiagnostics::Path::Disabled;
    RenderDiagnostics::WorkEstimate mWorkEstimateProbe = RenderDiagnostics::WorkEstimate::Auto;
    RenderDiagnostics::RouteOverride mRouteProbe = RenderDiagnostics::RouteOverride::Auto;
    RenderDiagnostics::WorkEstimate mWorkEstimateActive = RenderDiagnostics::WorkEstimate::Weighted;
    float mRenderSelectionTime = 0.f;
    float mPhysicsTime = 0.f;
    float mCollisionTime = 0.f;
    // Per-frame PhysicsManager::Step totals (each Step resets its own counters).
    double mFramePhysicsStepSec = 0.0;
    double mFramePhysicsIntegrateSec = 0.0;
    double mFramePhysicsBuildContactsSec = 0.0;
    double mFramePhysicsSolveSec = 0.0;
    std::uint64_t mFramePhysicsContactsProcessed = 0;
    unsigned mFramePhysicsSubsteps = 0;

    float mFrameTime = 0.f;
    std::array<float, 512> mFrameHistory{};
    std::size_t mFrameHistoryCount = 0;
    std::size_t mFrameHistoryIndex = 0;
    struct FrameProfile
    {
        float frameMs = 0.f;
        float inputMs = 0.f;
        float fixedMs = 0.f;
        float updateMs = 0.f;
        float renderMs = 0.f;
        float renderRecordMs = 0.f;
        float renderPrepareMs = 0.f;
        float renderSubmitMs = 0.f;
        float renderPresentMs = 0.f;
        float renderSdlPresentMs = 0.f;
        float renderWorkerWaitMs = 0.f;
        unsigned renderPreparedCommands = 0;
        unsigned renderCulledCommands = 0;
        unsigned renderWorkers = 0;
        unsigned renderEligiblePrevious = 0, renderEligibleCurrent = 0;
        unsigned renderTotalRecorded = 0, renderImmediateCulled = 0;
        unsigned renderBackgroundDraws = 0, renderWorldDraws = 0;
        unsigned renderDebugDraws = 0, renderUiDraws = 0;
        unsigned renderTintCacheHits = 0, renderTintUpdates = 0;
        unsigned renderPathReason = 0;
        std::uint64_t renderWeightedPrevious = 0, renderWeightedCurrent = 0;
        unsigned renderPathSwitched = 0;
        unsigned renderSelectionMode = 0;
        std::uint64_t renderEffectivePrevious = 0, renderEffectiveCurrent = 0;
        unsigned renderEstimatedOffscreen = 0;
        unsigned renderVisibilitySkipped = 0;
        unsigned renderParallelExecuted = 0, renderParallelFallback = 0;
        unsigned renderExecutedSwitched = 0;
        unsigned renderWorkEstimateMode = 0;
        unsigned renderRouteOverride = 0;
        float renderSelectionMs = 0.f;
        float renderBackgroundMs = 0.f, renderWorldMs = 0.f;
        float renderDebugMs = 0.f, renderUiMs = 0.f;
        float renderSpriteSubmitMs = 0.f;
        float physicsMs = 0.f;
        float collisionsMs = 0.f;
        float collisionSnapshotMs = 0.f;
        float collisionCandidatesMs = 0.f;
        float collisionMergeMs = 0.f;
        float collisionGridBuildMs = 0.f;
        float collisionAutoSelectorMs = 0.f;
        unsigned collisionBroadphaseMode = 0;
        unsigned collisionGridPasses = 0;
        unsigned collisionBruteforcePasses = 0;
        float collisionGridCellSizeLast = 0.f;
        unsigned collisionAutoCalibrationPasses = 0;
        unsigned collisionAutoContextLast = 0;
        unsigned collisionAutoContextTransitions = 0;
        unsigned collisionAutoCellHalfPasses = 0;
        unsigned collisionAutoCellBasePasses = 0;
        unsigned collisionAutoCellDoublePasses = 0;
        std::uint64_t collisionContactSignature = 0;
        int fixedSteps = 0;
        std::uint64_t broadphaseTests = 0;
        std::uint64_t narrowphaseTests = 0;
        std::uint64_t contactsBuilt = 0;
        unsigned collisionPasses = 0;
        unsigned collisionParallelPasses = 0;
        double physicsStepMs = 0.0;
        double physicsIntegrateMs = 0.0;
        double physicsBuildContactsMs = 0.0;
        double physicsSolveMs = 0.0;
        std::uint64_t physicsContactsProcessed = 0;
        unsigned physicsSubsteps = 0;
        unsigned deterministicStepsPerFrame = 0;
        unsigned workers = 0;
        unsigned collisionParallelSteps = 0;
    };
    std::vector<FrameProfile> mProfiles;
    void WriteProfiles_() noexcept;
    int fixedUpdatesDoneThisFrame = 0;
    unsigned mCollisionParallelStepsThisFrame = 0;
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

    bool StartImpl(const std::string cfg, bool inlineJson = false) noexcept;      // inicializa singletons
	int RunSingleThreadingImpl() noexcept;               // bucle principal singlethread
	int RunMultiThreadingImpl() noexcept;                // bucle principal multithread
    void StopImpl() noexcept;                            // marks end of loop

    bool ApplyStartingScene() const noexcept;

public:
    // --- High-level static API ---
    /** @brief Creates/initializes the engine singleton from a JSON config file.
     * @param configPath Path to a JSON file, not the directory containing it.
     * @return True when all managers initialize; false for invalid existing
     *         configuration or an SDL/service initialization failure.
     * @note A missing *file* falls back to defaults. The current default
     *       argument `"./"` names an existing directory and is not a valid
     *       JSON config file; pass an actual filename or use StartFromJson().
     */
    static bool Start(const std::string& configPath = "./") noexcept
    {
        if (!CreateSingleton()) return false;
        return GetInstance().StartImpl(configPath);
    }

    /** @brief Creates/initializes the engine from an inline UTF-8 JSON object.
     * @param jsonText Serialized configuration in UTF-8 JSON form.
     * @return False when JSON is invalid or any manager fails to initialize.
     */
    static bool StartFromJson(const std::string& jsonText) noexcept
    {
        if (!CreateSingleton()) return false;
        return GetInstance().StartImpl(jsonText, true);
    }

    /** @brief Calls the init callback, applies the pending starting scene, and runs the main loop.
     * @return -1 if no engine instance exists; otherwise the loop's return status.
     * @note Register a starting scene before Run(). The `multiThreading` option
     *       currently runs the same main-thread game loop with parallel collision
     *       candidate evaluation, not a separate SDL render thread.
     */
    static int Run() noexcept
    {
        if (auto* e = GetInstancePtr())
        {
            // Optional user initialization callback
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

    /** @brief Marks the engine as stopped and invokes singleton destruction.
     * @warning Unlike a deferred quit request, this method calls
     *          `DestroySingleton()` immediately. Calling it from a gameplay
     *          callback while a frame is executing can invalidate live engine
     *          pointers; prefer letting the window-close event end the loop.
     */
    static void Stop() noexcept
    {
        if (auto* e = GetInstancePtr())
        {
            e->StopImpl();
            DestroySingleton();    // Destroy the Engine here; Stop shuts down SDL, singletons, etc.
        }
    }

    // Benchmark-only override: set before Run(). 0 restores normal real-time
    // physics; 1..16 executes exactly that many fixed updates per rendered frame.
    // Does not alter the threading mode or production game configurations.
    /** @brief Sets a benchmark-only count of fixed updates per rendered frame.
     * @param steps 1..16 fixed steps each frame; 0 restores the real-time accumulator.
     * @return False unless the engine is started but not yet running, or if steps > 16.
     * @note This alters simulation cadence, not thread mode or production settings.
     */
    static bool ConfigureDeterministicFixedSteps(unsigned steps) noexcept
    {
        auto* engine = GetInstancePtr();
        if (!engine || !engine->mStarted || engine->mRunning || steps > 16u)
            return false;
        engine->mDeterministicFixedStepsPerFrame = steps;
        engine->mAccumulator = 0.f;
        return true;
    }

    // Only the dedicated benchmark calls this after StartFromJson and before
    // Run. Auto/Auto preserves every game/config default. A forced render path
    // allows equivalent workloads to compare the cost of weight/visibility
    // scoring without changing whether workers are used.
    /** @brief Overrides render-route and workload-estimate diagnostics before Run().
     * @param estimate Work-estimate strategy used when evaluating frame work.
     * @param route Forced renderer route or Auto for normal route selection.
     * @return False if the engine is not initialized, already running, or an
     *         adaptive selector would be requested without a work estimate.
     */
    static bool ConfigureRenderSelectorProbe(RenderDiagnostics::WorkEstimate estimate,
                                             RenderDiagnostics::RouteOverride route) noexcept
    {
        auto* engine = GetInstancePtr();
        if (!engine || !engine->mStarted || engine->mRunning) return false;
        if (estimate == RenderDiagnostics::WorkEstimate::Off &&
            route == RenderDiagnostics::RouteOverride::Auto &&
            engine->mCfg.renderer.parallelSelection == RenderDiagnostics::Selection::Adaptive)
            return false; // Adaptive needs prior-frame scores to choose a path.
        engine->mWorkEstimateProbe = estimate;
        engine->mRouteProbe = route;
        return true;
    }

    /** @brief Registers a callback invoked once at the beginning of Run().
     * @param cb Function receiving the active Engine; replaces any prior callback.
     */
    static void SetInitCallback(std::function<void(Engine&)> cb) noexcept 
    { 
        if (auto* e = GetInstancePtr())
            e->mOnInit = std::move(cb); 
    }
    /** @brief Sets the per-frame update callback.
     * @param cb Function receiving frame delta in scaled seconds.
     */
    static void SetUpdateCallback(std::function<void(float)> cb) noexcept 
    { 
        if (auto* e = GetInstancePtr())
            e->mOnUpdate = std::move(cb);
    }
    /** @brief Sets the callback invoked for each fixed simulation step.
     * @param cb Function receiving the configured fixed timestep in seconds.
     */
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
    // -------- Property getters (NON-static) --------
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

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `is running`; use the corresponding instance member in gameplay code.
     */
    using IsRunningProperty = PropertyRO<Engine, bool, &Engine::IsRunning_>;
    /** @endcond */
    /** @brief Read-only flag that reports whether the engine loop is active.
     */
    IsRunningProperty isRunning{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `window`; use the corresponding instance member in gameplay code.
     */
    using WindowProperty = PropertyRO<Engine, WindowManager*, &Engine::Window_>;
    /** @endcond */
    /** @brief Read-only pointer to the active window manager.
     */
    WindowProperty window{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `time`; use the corresponding instance member in gameplay code.
     */
    using TimeProperty = PropertyRO<Engine, TimeManager*, &Engine::Time_>;
    /** @endcond */
    /** @brief Read-only pointer to the time manager.
     */
    TimeProperty time{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `input`; use the corresponding instance member in gameplay code.
     */
    using InputProperty = PropertyRO<Engine, InputManager*, &Engine::Input_>;
    /** @endcond */
    /** @brief Read-only pointer to the input manager.
     */
    InputProperty input{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `assets`; use the corresponding instance member in gameplay code.
     */
    using AssetsProperty = PropertyRO<Engine, AssetManager*, &Engine::Assets_>;
    /** @endcond */
    /** @brief Read-only pointer to the asset cache.
     */
    AssetsProperty assets{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `sound`; use the corresponding instance member in gameplay code.
     */
    using SoundProperty = PropertyRO<Engine, SoundManager*, &Engine::Sound_>;
    /** @endcond */
    /** @brief Read-only pointer to the sound manager.
     */
    SoundProperty sound{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Public type alias for the uiproperty accessor.
     */
    using UIProperty = PropertyRO<Engine, UIManager*, &Engine::UI_>;
    /** @endcond */
    /** @brief Read-only pointer to the immediate-mode UI manager.
     */
    UIProperty ui{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `scenes`; use the corresponding instance member in gameplay code.
     */
    using ScenesProperty = PropertyRO<Engine, SceneManager*, &Engine::Scenes_>;
    /** @endcond */
    /** @brief Read-only pointer to the scene manager.
     */
    ScenesProperty scenes{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `physics`; use the corresponding instance member in gameplay code.
     */
    using PhysicsProperty = PropertyRO<Engine, PhysicsManager*, &Engine::Physics_>;
    /** @endcond */
    /** @brief Read-only pointer to the physics manager.
     */
    PhysicsProperty physics{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `random`; use the corresponding instance member in gameplay code.
     */
    using RandomProperty = PropertyRO<Engine, RandomManager*, &Engine::Random_>;
    /** @endcond */
    /** @brief Read-only pointer to the random-number manager.
     */
    RandomProperty random{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `camera`; use the corresponding instance member in gameplay code.
     */
    using CameraProperty = PropertyRO<Engine, Camera2D*, &Engine::Camera_>;
    /** @endcond */
    /** @brief Read-only pointer to the current scene camera.
     */
    CameraProperty camera{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `fixed delta`; use the corresponding instance member in gameplay code.
     */
    using FixedDeltaProperty = PropertyRO<Engine, float, &Engine::FixedDelta_>;
    /** @endcond */
    /** @brief Fixed simulation timestep in seconds, exposed read-only.
     */
    FixedDeltaProperty fixedDelta{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `log errors`; use the corresponding instance member in gameplay code.
     */
    using LogErrorsProperty = PropertyRO<Engine, bool, &Engine::LogErrors_>;
    /** @endcond */
    /** @brief Flag controlling engine error logging.
     */
    LogErrorsProperty logErrors{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Public type alias for the fpsproperty accessor.
     */
    using FPSProperty = PropertyRO<Engine, float, &Engine::GetFPS_>;
    /** @endcond */
    /** @brief Read-only frame-rate value from engine timing statistics.
     */
    FPSProperty fps{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `aspect ratio`; use the corresponding instance member in gameplay code.
     */
    using AspectRatioProperty = PropertyRO<Engine, float, &Engine::GetAspectRatio_>;
    /** @endcond */
    /** @brief Read-only width-to-height ratio of the active window.
     */
    AspectRatioProperty aspectRatio{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `aspect ratio fraction`; use the corresponding instance member in gameplay code.
     */
    using AspectRatioFractionProperty = PropertyRO<Engine, Vec2I, &Engine::GetAspectRatioAsFraction_>;
    /** @endcond */
    /** @brief Read-only integer representation of the current aspect ratio.
     */
    AspectRatioFractionProperty aspectRatioFraction{ this };
};

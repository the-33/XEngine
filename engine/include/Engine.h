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
#include "XEngine/detail/UICornerRadii.h"
#include "XEngine/detail/SpriteProjection.h"
#include "Property.h"
#include "Singleton.h"
#include "Assets.h"
#include "XEngine/detail/StatsOverlayLayout.h"
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
        ScreenRoundedRect, ///< Rounded rectangle with optional outline in screen pixels.
        ScreenLine,  ///< Screen-space line segment with configurable thickness.
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
    /** @brief Endpoints for WorldLine (world units) or ScreenLine (screen pixels).
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

    // ScreenRect / ScreenRoundedRect / ScreenLine / ScreenImage / ScreenText
    /** @brief Destination rectangle in screen pixels.
     */
    Rect dstScreen{};
    /** @brief Outer border color used for a rounded screen rectangle. */
    Color screenOutline{0, 0, 0, 0};
    /** @brief Corner radius for a screen-space rounded rectangle, in pixels. */
    float screenCornerRadius = 0.f;
    /** @brief Optional independent rounded corners for recorded screen draws. */
    xengine::detail::ui::CornerRadii screenCornerRadii{};
    /** @brief Whether to use screenCornerRadii instead of the uniform radius. */
    bool hasScreenCornerRadii = false;
    /** @brief Outline thickness for a rounded rectangle or screen line, in pixels. */
    float screenStrokeWidth = 1.f;
    /** @brief Optional pixel-aligned clip for a rounded screen-space fill. */
    Rect screenClip{};
    /** @brief Whether the rounded screen-space draw uses its own clip rectangle. */
    bool hasScreenClip = false;

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
    /** @brief Optional texture-space scaling for screen text (1 = original font size). */
    float textScale = 1.f;
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
    /** @brief First command belonging to the native-resolution UI overlay.
     * World commands before this index render at the internal resolution;
     * commands from here onward render after world upscaling.
     */
    std::size_t uiStartIndex = 0;
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
    /** @brief Permit the singleton base to construct and release the Engine manager.
     */
    friend class Singleton<Engine>;
private:
    /** @brief Internal configuration values for Engine.
     */
    struct Config
    {
        /** @brief Stored multi threading value used by Config.
         */
        bool multiThreading = false;
        /** @brief Configured profile csv value used by Config.
         */
        bool profileCsv = false;
		/** @brief Configured log stats value used by Config.
		 */
		bool logStats = false;
        /** @brief Configured log errors value used by Config.
         */
        bool logErrors = true;

        /** @brief Window data and utilities used by Engine::Config.
         */
        struct Window
        {
            /** @brief Configured width value used by Window.
             */
            int width = 1280;
            /** @brief Configured height value used by Window.
             */
            int height = 720;
            /** @brief Configured fullscreen value used by Window.
             */
            bool fullscreen = false;
            /** @brief Configured title value used by Window.
             */
            const char* title = "Game";

			/** @brief Construct a Window instance.
			 */
			Window() noexcept = default;
        /** @brief Configured window value used by Config.
         */
        } window;

        /** @brief Collision data and utilities used by Engine::Config.
         */
        struct Collision
        {
            /** @brief Configured broadphase value used by Collision.
             */
            std::string broadphase = "bruteforce";
            /** @brief Configured cell size value used by Collision.
             */
            float cellSize = 2.f;
            /** @brief Configured auto cell size value used by Collision.
             */
            bool autoCellSize = false;
        /** @brief Configured collision value used by Config.
         */
        } collision;

        /** @brief Renderer data and utilities used by Engine::Config.
         */
        struct Renderer
        {
            /** @brief Configured vsync value used by Renderer.
             */
            bool vsync = true;
            /** @brief Configured hardware acceleration value used by Renderer.
             */
            bool hardwareAcceleration = true;
            // Backend selected for the current platform (default: SDL chooses).
            /** @brief Configured backend value used by Renderer.
             */
            std::string backend = "auto";
            /** @brief Configured backend fallback value used by Renderer.
             */
            bool backendFallback = true;
            /** @brief Configured parallel preparation value used by Renderer.
             */
            bool parallelPreparation = true;
            /** @brief Configured parallel min commands value used by Renderer.
             */
            std::size_t parallelMinCommands = RenderDiagnostics::kParallelThreshold;
            /** @brief Configured parallel selection value used by Renderer.
             */
            RenderDiagnostics::Selection parallelSelection = RenderDiagnostics::Selection::Fixed;
            /** @brief Configured bg color value used by Renderer.
             */
            Color bgColor = { 0, 0, 0, 255 };

			/** @brief Construct a Renderer instance.
			 */
			Renderer() noexcept = default;
        /** @brief Configured renderer value used by Config.
         */
        } renderer;

        /** @brief Input data and utilities used by Engine::Config.
         */
        struct Input
        {
            /** @brief Configured start text input value used by Input.
             */
            bool startTextInput = false;
            /** @brief Configured start mouse locked value used by Input.
             */
            bool startMouseLocked = false;
            /** @brief Configured start mouse hidden value used by Input.
             */
            bool startMouseHidden = false;

			/** @brief Construct a Input instance.
			 */
			Input() noexcept = default;
        /** @brief Configured input value used by Config.
         */
        } input;

        /** @brief Time data and utilities used by Engine::Config.
         */
        struct Time
        {
            /** @brief Configured fixed dt value used by Time.
             */
            float fixedDt = 1.0f / 60.0f;

			/** @brief Construct a Time instance.
			 */
			Time() noexcept = default;
        /** @brief Configured time value used by Config.
         */
        } time;

        /** @brief Assets data and utilities used by Engine::Config.
         */
        struct Assets
        {
            /** @brief Configured assets folder path value used by Assets.
             */
            std::string assetsFolderPath = "./assets/";
            /** @brief Configured default font relative path value used by Assets.
             */
            std::string defaultFontRelativePath = "fonts/ui_default.ttf";

			/** @brief Construct a Assets instance.
			 */
			Assets() noexcept = default;
        /** @brief Configured assets value used by Config.
         */
        } assets;

        /** @brief Sound data and utilities used by Engine::Config.
         */
        struct Sound {
            /** @brief Configured sfx channels value used by Sound.
             */
            int   sfxChannels = 32;
            /** @brief Stored master volume value used by Sound.
             */
            float masterVolume = 1.0f;
            /** @brief Configured enable ducking value used by Sound.
             */
            bool  enableDucking = false;
            /** @brief Configured duck volume value used by Sound.
             */
            float duckVolume = 0.6f;
            /** @brief Configured duck attack sec value used by Sound.
             */
            float duckAttackSec = 0.02f;
            /** @brief Configured duck release sec value used by Sound.
             */
            float duckReleaseSec = 0.25f;

			/** @brief Construct a Sound instance.
			 */
			Sound() noexcept = default;
        /** @brief Configured sound value used by Config.
         */
        } sound;

        /** @brief Ui style data and utilities used by Engine::Config.
         */
        struct UIStyle
        {
            /** @brief Configured text value used by UIStyle.
             */
            Color text{ 240,240,240,255 };
            /** @brief Configured btn value used by UIStyle.
             */
            Color btn{ 60,60,60,255 };
            /** @brief Configured btn hot value used by UIStyle.
             */
            Color btnHot{ 80,80,80,255 };
            /** @brief Configured btn active value used by UIStyle.
             */
            Color btnActive{ 40,40,40,255 };
            /** @brief Configured outline value used by UIStyle.
             */
            Color outline{ 200,200,200,255 };
            /** @brief Configured padding value used by UIStyle.
             */
            float padding = 6.f;
            /** @brief Configured default font key value used by UIStyle.
             */
            std::string defaultFontKey = "ui_default";
            float cornerRadius = 0.f; ///< Default UI corner radius in cornerRadiusUnit units.
            float borderWidth = 1.f;  ///< Default UI border thickness in pixels.
            std::string cornerRadiusUnit = "pixels"; ///< Unit of cornerRadius: pixels or percent.
            bool useIndividualCornerRadii = false; ///< Whether the config uses a two/four-value radius array.
            xengine::detail::ui::CornerRadii individualCornerRadii{}; ///< Parsed clockwise radii in cornerRadiusUnit units.
            /** @brief Configured default fontpt size value used by UIStyle.
             */
            int defaultFontptSize = 24;

			/** @brief Construct a UIStyle instance.
			 */
			UIStyle() noexcept = default;

            /** @brief Implement the operator= operation for UIStyle.
             * @param other Other supplied to UIStyle.
             * @return Result of applying the overloaded operator.
             */
            UIStyle& operator= (const UIStyle& other) noexcept
            {
                text = other.text;
                btn = other.btn;
                btnHot = other.btnHot;
                btnActive = other.btnActive;
                outline = other.outline;
                padding = other.padding;
                defaultFontKey = other.defaultFontKey;
                cornerRadius = other.cornerRadius;
                borderWidth = other.borderWidth;
                cornerRadiusUnit = other.cornerRadiusUnit;
                useIndividualCornerRadii = other.useIndividualCornerRadii;
                individualCornerRadii = other.individualCornerRadii;
                defaultFontptSize = other.defaultFontptSize;
                return *this;
            }
        /** @brief Configured ui style value used by Config.
         */
        } uiStyle;

        /** @brief Physics data and utilities used by Engine::Config.
         */
        struct Physics
        {
            /** @brief Configured base gravity value used by Physics.
             */
            Vec2 baseGravity = { 0.f, 9.81f };
            /** @brief Configured penetration slop value used by Physics.
             */
            float penetrationSlop = 0.01f;
            /** @brief Configured penetration percent value used by Physics.
             */
            float penetrationPercent = 0.8f;
            /** @brief Stored max substeps value used by Physics.
             */
            int maxSubsteps = 8;
            /** @brief Configured ccd min size factor value used by Physics.
             */
            float ccdMinSizeFactor = 0.5f;
        /** @brief Configured physics value used by Config.
         */
        } physics;

        /** @brief Construct a Config instance.
         */
        Config() noexcept = default;
    };

    /** @brief Grant Collider2D access to the internal state of Engine.
     */
    friend class Collider2D;

	/** @brief Configured default config path value used by Engine.
	 */
	const std::string kDefaultConfigPath = "./XEngineConfig.json";
    /** @brief Load config from file in Engine.
     * @param path Filesystem path identifying the resource.
     * @param out Caller-provided output destination.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    static bool LoadConfigFromFile(const std::string& path, Config& out) noexcept;
    /** @brief Load config from json in Engine.
     * @param text UTF-8 text supplied to the operation.
     * @param origin Origin supplied to Engine.
     * @param out Caller-provided output destination.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    static bool LoadConfigFromJson(const std::string& text, const std::string& origin, Config& out) noexcept;

    /** @brief Frame tick in Engine.
     */
    void FrameTick_() noexcept;
    /** @brief Poll input in Engine.
     */
    void PollInput_() noexcept;
    /** @brief Do fixed updates in Engine.
     * @param dt Elapsed simulation time for the update, in seconds.
     */
    void DoFixedUpdates_(float dt) noexcept;
    /** @brief Do update in Engine.
     * @param dt Elapsed simulation time for the update, in seconds.
     */
    void DoUpdate_(float dt) noexcept;
    /** @brief Do render in Engine.
     */
    void DoRender_() noexcept;


	/** @brief Draw stats overlay in Engine.
	 */
	void DrawStatsOverlay_() noexcept;
    // F3 controls Debug collider draws independently of the F2 stats-panel toggle.
    /** @brief Stored show debug draws value used by Engine.
     */
    bool mShowDebugDraws = false;
    /** @brief Stored show stats overlay value used by Engine.
     */
    bool mShowStatsOverlay = false;

    /** @brief Stored stats next refresh value used by Engine.
     */
    double mStatsNextRefresh = 0.0;
    /** @brief Stored stats lines value used by Engine.
     */
    std::vector<std::string> mStatsLines;
    // On-screen profiler pages are rebuilt only when values or viewport change.
    /** @brief Stats display row data and utilities used by Engine.
     */
    struct StatsDisplayRow
    {
        /** @brief Configured label value used by StatsDisplayRow.
         */
        std::string label;
        /** @brief Configured value value used by StatsDisplayRow.
         */
        std::string value;
        /** @brief Configured heading value used by StatsDisplayRow.
         */
        bool heading = false;
        // Text clipping and metrics are cached at the overlay's 4 Hz refresh.
        /** @brief Configured visible label value used by StatsDisplayRow.
         */
        std::string visibleLabel;
        /** @brief Configured visible value value used by StatsDisplayRow.
         */
        std::string visibleValue;
        /** @brief Configured visible value width value used by StatsDisplayRow.
         */
        float visibleValueWidth = 0.f;
    };
    /** @brief Stored stats display rows value used by Engine.
     */
    std::vector<StatsDisplayRow> mStatsDisplayRows;
    /** @brief Stored stats layout value used by Engine.
     */
    XEngineStatsOverlay::Layout mStatsLayout;
    /** @brief Stored stats page value used by Engine.
     */
    std::size_t mStatsPage = 0;
    /** @brief Stored stats viewport value used by Engine.
     */
    Vec2I mStatsViewport = Vec2I::Zero();
    /** @brief Stored stats column width value used by Engine.
     */
    float mStatsColumnWidth = 0.f;
    /** @brief Stored stats held column width value used by Engine.
     */
    float mStatsHeldColumnWidth = 0.f;
    /** @brief Stored stats line height value used by Engine.
     */
    float mStatsLineHeight = 0.f;

    /** @brief Manage scene queues in Engine.
     */
    void ManageSceneQueues_() noexcept;
    /** @brief Manage scene changes in Engine.
     */
    void ManageSceneChanges_() noexcept;

    /** @brief Stored cfg value used by Engine.
     */
    Config mCfg;

    /** @brief Stored log stats value used by Engine.
     */
    bool mLogStats = true;
    /** @brief Stored log errors value used by Engine.
     */
    bool mLogErrors = true;

    /** @brief Stored fixed dt value used by Engine.
     */
    float mFixedDt = 1/60.f;
    /** @brief Stored started value used by Engine.
     */
    bool mStarted = false;
    /** @brief Stored running value used by Engine.
     */
    bool mRunning = false;
    /** @brief Stored shutting down value used by Engine.
     */
    bool mShuttingDown = false;
    /** @brief Configured safe to quit value used by Engine.
     */
    bool SafeToQuit = true;
    /** @brief Stored accumulator value used by Engine.
     */
    float mAccumulator = 0.f;
    // 0: regular real-time accumulator. Nonzero: exactly N fixed steps per frame.
    // Intended for controlled benchmarks, independent of frame wall time.
    /** @brief Stored deterministic fixed steps per frame value used by Engine.
     */
    unsigned mDeterministicFixedStepsPerFrame = 0;


    /** @brief Stored poll input time value used by Engine.
     */
    float mPollInputTime = 0.f;
	/** @brief Stored update time value used by Engine.
	 */
	float mUpdateTime = 0.f;
	/** @brief Stored fixed update time value used by Engine.
	 */
	float mFixedUpdateTime = 0.f;
    /** @brief Stored render time value used by Engine.
     */
    float mRenderTime = 0.f;
    /** @brief Stored render record time value used by Engine.
     */
    float mRenderRecordTime = 0.f;
    /** @brief Stored render prepare time value used by Engine.
     */
    float mRenderPrepareTime = 0.f;
    /** @brief Stored render submit time value used by Engine.
     */
    float mRenderSubmitTime = 0.f;
    /** @brief Stored render present time value used by Engine.
     */
    float mRenderPresentTime = 0.f;
    /** @brief Stored render worker wait time value used by Engine.
     */
    float mRenderWorkerWaitTime = 0.f;
    /** @brief Stored render world time value used by Engine.
     */
    float mRenderWorldTime = 0.f;
    /** @brief Stored render ui time value used by Engine.
     */
    float mRenderUiTime = 0.f;
    /** @brief Stored last render eligible previous value used by Engine.
     */
    unsigned mLastRenderEligiblePrevious = 0;
    /** @brief Stored last render weighted previous value used by Engine.
     */
    std::uint64_t mLastRenderWeightedPrevious = 0;
    /** @brief Stored last render effective previous value used by Engine.
     */
    std::uint64_t mLastRenderEffectivePrevious = 0;
    /** @brief Stored previous executed parallel value used by Engine.
     */
    bool mPreviousExecutedParallel = false;
    /** @brief Stored executed path primed value used by Engine.
     */
    bool mExecutedPathPrimed = false;
    /** @brief Stored render executed switched value used by Engine.
     */
    unsigned mRenderExecutedSwitched = 0;
    /** @brief Stored render parallel executed value used by Engine.
     */
    unsigned mRenderParallelExecuted = 0;
    /** @brief Stored render parallel fallback value used by Engine.
     */
    unsigned mRenderParallelFallback = 0;
    /** @brief Stored previous render parallel value used by Engine.
     */
    bool mPreviousRenderParallel = false;
    /** @brief Stored render path primed value used by Engine.
     */
    bool mRenderPathPrimed = false;
    /** @brief Stored render path switched value used by Engine.
     */
    unsigned mRenderPathSwitched = 0;
    /** @brief Stored last render path value used by Engine.
     */
    RenderDiagnostics::Path mLastRenderPath = RenderDiagnostics::Path::Disabled;
    /** @brief Stored work estimate probe value used by Engine.
     */
    RenderDiagnostics::WorkEstimate mWorkEstimateProbe = RenderDiagnostics::WorkEstimate::Auto;
    /** @brief Stored route probe value used by Engine.
     */
    RenderDiagnostics::RouteOverride mRouteProbe = RenderDiagnostics::RouteOverride::Auto;
    /** @brief Stored work estimate active value used by Engine.
     */
    RenderDiagnostics::WorkEstimate mWorkEstimateActive = RenderDiagnostics::WorkEstimate::Weighted;
    /** @brief Stored render selection time value used by Engine.
     */
    float mRenderSelectionTime = 0.f;
    /** @brief Stored physics time value used by Engine.
     */
    float mPhysicsTime = 0.f;
    /** @brief Stored collision time value used by Engine.
     */
    float mCollisionTime = 0.f;
    // Per-frame PhysicsManager::Step totals (each Step resets its own counters).
    /** @brief Stored frame physics step sec value used by Engine.
     */
    double mFramePhysicsStepSec = 0.0;
    /** @brief Stored frame physics integrate sec value used by Engine.
     */
    double mFramePhysicsIntegrateSec = 0.0;
    /** @brief Stored frame physics build contacts sec value used by Engine.
     */
    double mFramePhysicsBuildContactsSec = 0.0;
    /** @brief Stored frame physics solve sec value used by Engine.
     */
    double mFramePhysicsSolveSec = 0.0;
    /** @brief Stored frame physics contacts processed value used by Engine.
     */
    std::uint64_t mFramePhysicsContactsProcessed = 0;
    /** @brief Stored frame physics substeps value used by Engine.
     */
    unsigned mFramePhysicsSubsteps = 0;

    /** @brief Stored frame time value used by Engine.
     */
    float mFrameTime = 0.f;
    /** @brief Stored frame history value used by Engine.
     */
    std::array<float, 512> mFrameHistory{};
    /** @brief Stored frame history count value used by Engine.
     */
    std::size_t mFrameHistoryCount = 0;
    /** @brief Stored frame history index value used by Engine.
     */
    std::size_t mFrameHistoryIndex = 0;
    /** @brief Frame profile data and utilities used by Engine.
     */
    struct FrameProfile
    {
        /** @brief Configured frame ms value used by FrameProfile.
         */
        float frameMs = 0.f;
        /** @brief Configured input ms value used by FrameProfile.
         */
        float inputMs = 0.f;
        /** @brief Configured fixed ms value used by FrameProfile.
         */
        float fixedMs = 0.f;
        /** @brief Configured update ms value used by FrameProfile.
         */
        float updateMs = 0.f;
        /** @brief Configured render ms value used by FrameProfile.
         */
        float renderMs = 0.f;
        /** @brief Configured render record ms value used by FrameProfile.
         */
        float renderRecordMs = 0.f;
        /** @brief Configured render prepare ms value used by FrameProfile.
         */
        float renderPrepareMs = 0.f;
        /** @brief Configured render submit ms value used by FrameProfile.
         */
        float renderSubmitMs = 0.f;
        /** @brief Configured render present ms value used by FrameProfile.
         */
        float renderPresentMs = 0.f;
        /** @brief Configured render sdl present ms value used by FrameProfile.
         */
        float renderSdlPresentMs = 0.f;
        /** @brief Configured render worker wait ms value used by FrameProfile.
         */
        float renderWorkerWaitMs = 0.f;
        /** @brief Configured render prepared commands value used by FrameProfile.
         */
        unsigned renderPreparedCommands = 0;
        /** @brief Configured render culled commands value used by FrameProfile.
         */
        unsigned renderCulledCommands = 0;
        /** @brief Configured render workers value used by FrameProfile.
         */
        unsigned renderWorkers = 0;
        /** @brief Configured render eligible previous value used by FrameProfile.
         */
        unsigned renderEligiblePrevious = 0, renderEligibleCurrent = 0;
        /** @brief Configured render total recorded value used by FrameProfile.
         */
        unsigned renderTotalRecorded = 0, renderImmediateCulled = 0;
        /** @brief Configured render background draws value used by FrameProfile.
         */
        unsigned renderBackgroundDraws = 0, renderWorldDraws = 0;
        /** @brief Configured render debug draws value used by FrameProfile.
         */
        unsigned renderDebugDraws = 0, renderUiDraws = 0;
        /** @brief Configured render tint cache hits value used by FrameProfile.
         */
        unsigned renderTintCacheHits = 0, renderTintUpdates = 0;
        /** @brief Configured render path reason value used by FrameProfile.
         */
        unsigned renderPathReason = 0;
        /** @brief Configured render weighted previous value used by FrameProfile.
         */
        std::uint64_t renderWeightedPrevious = 0, renderWeightedCurrent = 0;
        /** @brief Configured render path switched value used by FrameProfile.
         */
        unsigned renderPathSwitched = 0;
        /** @brief Configured render selection mode value used by FrameProfile.
         */
        unsigned renderSelectionMode = 0;
        /** @brief Configured render effective previous value used by FrameProfile.
         */
        std::uint64_t renderEffectivePrevious = 0, renderEffectiveCurrent = 0;
        /** @brief Configured render estimated offscreen value used by FrameProfile.
         */
        unsigned renderEstimatedOffscreen = 0;
        /** @brief Configured render visibility skipped value used by FrameProfile.
         */
        unsigned renderVisibilitySkipped = 0;
        /** @brief Configured render parallel executed value used by FrameProfile.
         */
        unsigned renderParallelExecuted = 0, renderParallelFallback = 0;
        /** @brief Configured render executed switched value used by FrameProfile.
         */
        unsigned renderExecutedSwitched = 0;
        /** @brief Configured render work estimate mode value used by FrameProfile.
         */
        unsigned renderWorkEstimateMode = 0;
        /** @brief Configured render route override value used by FrameProfile.
         */
        unsigned renderRouteOverride = 0;
        /** @brief Configured render selection ms value used by FrameProfile.
         */
        float renderSelectionMs = 0.f;
        /** @brief Configured render background ms value used by FrameProfile.
         */
        float renderBackgroundMs = 0.f, renderWorldMs = 0.f;
        /** @brief Configured render debug ms value used by FrameProfile.
         */
        float renderDebugMs = 0.f, renderUiMs = 0.f;
        /** @brief Configured render sprite submit ms value used by FrameProfile.
         */
        float renderSpriteSubmitMs = 0.f;
        /** @brief Configured physics ms value used by FrameProfile.
         */
        float physicsMs = 0.f;
        /** @brief Configured collisions ms value used by FrameProfile.
         */
        float collisionsMs = 0.f;
        /** @brief Configured collision snapshot ms value used by FrameProfile.
         */
        float collisionSnapshotMs = 0.f;
        /** @brief Configured collision candidates ms value used by FrameProfile.
         */
        float collisionCandidatesMs = 0.f;
        /** @brief Configured collision merge ms value used by FrameProfile.
         */
        float collisionMergeMs = 0.f;
        /** @brief Configured collision grid build ms value used by FrameProfile.
         */
        float collisionGridBuildMs = 0.f;
        /** @brief Configured collision auto selector ms value used by FrameProfile.
         */
        float collisionAutoSelectorMs = 0.f;
        /** @brief Configured collision broadphase mode value used by FrameProfile.
         */
        unsigned collisionBroadphaseMode = 0;
        /** @brief Configured collision grid passes value used by FrameProfile.
         */
        unsigned collisionGridPasses = 0;
        /** @brief Configured collision bruteforce passes value used by FrameProfile.
         */
        unsigned collisionBruteforcePasses = 0;
        /** @brief Configured collision grid cell size last value used by FrameProfile.
         */
        float collisionGridCellSizeLast = 0.f;
        /** @brief Configured collision auto calibration passes value used by FrameProfile.
         */
        unsigned collisionAutoCalibrationPasses = 0;
        /** @brief Configured collision auto context last value used by FrameProfile.
         */
        unsigned collisionAutoContextLast = 0;
        /** @brief Configured collision auto context transitions value used by FrameProfile.
         */
        unsigned collisionAutoContextTransitions = 0;
        /** @brief Configured collision auto cell half passes value used by FrameProfile.
         */
        unsigned collisionAutoCellHalfPasses = 0;
        /** @brief Configured collision auto cell base passes value used by FrameProfile.
         */
        unsigned collisionAutoCellBasePasses = 0;
        /** @brief Configured collision auto cell double passes value used by FrameProfile.
         */
        unsigned collisionAutoCellDoublePasses = 0;
        /** @brief Configured collision contact signature value used by FrameProfile.
         */
        std::uint64_t collisionContactSignature = 0;
        /** @brief Configured fixed steps value used by FrameProfile.
         */
        int fixedSteps = 0;
        /** @brief Configured broadphase tests value used by FrameProfile.
         */
        std::uint64_t broadphaseTests = 0;
        /** @brief Configured narrowphase tests value used by FrameProfile.
         */
        std::uint64_t narrowphaseTests = 0;
        /** @brief Configured contacts built value used by FrameProfile.
         */
        std::uint64_t contactsBuilt = 0;
        /** @brief Configured collision passes value used by FrameProfile.
         */
        unsigned collisionPasses = 0;
        /** @brief Configured collision parallel passes value used by FrameProfile.
         */
        unsigned collisionParallelPasses = 0;
        /** @brief Configured physics step ms value used by FrameProfile.
         */
        double physicsStepMs = 0.0;
        /** @brief Configured physics integrate ms value used by FrameProfile.
         */
        double physicsIntegrateMs = 0.0;
        /** @brief Configured physics build contacts ms value used by FrameProfile.
         */
        double physicsBuildContactsMs = 0.0;
        /** @brief Configured physics solve ms value used by FrameProfile.
         */
        double physicsSolveMs = 0.0;
        /** @brief Configured physics contacts processed value used by FrameProfile.
         */
        std::uint64_t physicsContactsProcessed = 0;
        /** @brief Configured physics substeps value used by FrameProfile.
         */
        unsigned physicsSubsteps = 0;
        /** @brief Configured deterministic steps per frame value used by FrameProfile.
         */
        unsigned deterministicStepsPerFrame = 0;
        /** @brief Configured workers value used by FrameProfile.
         */
        unsigned workers = 0;
        /** @brief Configured collision parallel steps value used by FrameProfile.
         */
        unsigned collisionParallelSteps = 0;
    };
    /** @brief Stored profiles value used by Engine.
     */
    std::vector<FrameProfile> mProfiles;
    /** @brief Write profiles in Engine.
     */
    void WriteProfiles_() noexcept;
    /** @brief Configured fixed updates done this frame value used by Engine.
     */
    int fixedUpdatesDoneThisFrame = 0;
    /** @brief Stored collision parallel steps this frame value used by Engine.
     */
    unsigned mCollisionParallelStepsThisFrame = 0;
    /** @brief Configured average fixed updates per frame value used by Engine.
     */
    int averageFixedUpdatesPerFrame = 0;
    /** @brief Configured last accumulator value used by Engine.
     */
    float lastAccumulator = 0.f;
    /** @brief Configured average fps value used by Engine.
     */
    float averageFps = 0.f;

    /** @brief Stored aspect ratio value used by Engine.
     */
    float mAspectRatio = 0.f;
    /** @brief Stored aspect ratio as fraction value used by Engine.
     */
    Vec2I  mAspectRatioAsFraction = Vector2::Zero;

    /** @brief Stored on init value used by Engine.
     */
    std::function<void(Engine&)> mOnInit;
    /** @brief Stored on update value used by Engine.
     */
    std::function<void(float)> mOnUpdate;
    /** @brief Stored on fixed update value used by Engine.
     */
    std::function<void(float)> mOnFixedUpdate;

    /** @brief Construct a Engine instance.
     * @param other Source object whose copy, move or equality is being requested.
     */
    Engine(const Engine& other) = delete;
    /** @brief Implement the operator= operation for Engine.
     * @return Result of applying the overloaded operator.
     * @param other Source object whose copy, move or equality is being requested.
     */
    Engine& operator=(const Engine& other) = delete;

    /** @brief Construct a Engine instance.
     */
    Engine() noexcept;
    /** @brief Destroy a Engine instance and release owned resources.
     */
    ~Engine();

    /** @brief Start impl in Engine.
     * @param cfg Cfg supplied to Engine.
     * @param inlineJson Inline json supplied to Engine.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool StartImpl(const std::string cfg, bool inlineJson = false) noexcept;      // inicializa singletons
	/** @brief Run single threading impl in Engine.
	 * @return Result of run single threading impl in Engine.
	 */
	int RunSingleThreadingImpl() noexcept;               // bucle principal singlethread
	/** @brief Run multi threading impl in Engine.
	 * @return Result of run multi threading impl in Engine.
	 */
	int RunMultiThreadingImpl() noexcept;                // bucle principal multithread
    /** @brief Stop impl in Engine.
     */
    void StopImpl() noexcept;                            // marks end of loop

    /** @brief Apply starting scene in Engine.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
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
            // A Stop() request from the initialization callback must be
            // deferred until this entry point can safely destroy the engine.
            e->mRunning = true;
            e->SafeToQuit = false;
            // Optional user initialization callback
            if (e->mOnInit)
                e->mOnInit(*e);

            int result = 0;
            if (!e->mRunning)
            {
                // Initialization explicitly requested a clean exit.
                e->SafeToQuit = true;
            }
            else if (!e->ApplyStartingScene())
            {
                e->SafeToQuit = true;
                result = -1;
            }
            else
            {
                // Scene activation callbacks can request Stop() too.
                if (e->mRunning)
                    result = e->mCfg.multiThreading
                        ? e->RunMultiThreadingImpl()
                        : e->RunSingleThreadingImpl();
            }

            // All member functions and frame callbacks have now returned.
            e->SafeToQuit = true;
            e->StopImpl();
            DestroySingleton();
            return result;
        }
        return -1;
    }

    /** @brief Requests a safe engine shutdown.
     * @details During Run(), the request stops subsequent frames and defers
     * cleanup and singleton destruction until the loop has returned. Outside
     * Run(), the initialized engine is shut down and destroyed immediately.
     */
    static void Stop() noexcept
    {
        if (auto* e = GetInstancePtr())
        {
            if (e->mShuttingDown) return;
            if (e->mRunning || !e->SafeToQuit)
            {
                e->mRunning = false;
                return;
            }
            e->StopImpl();
            DestroySingleton();
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
    /** @brief Test whether Engine is running.
     * @return True when is running; otherwise false.
     */
    static bool IsRunning() noexcept
    {
        if (auto* e = GetInstancePtr())
            return e->mRunning;
        return false;
    }

    /** @brief Window in Engine.
     * @return Result of window in Engine.
     */
    static WindowManager* Window() noexcept;
    /** @brief Time in Engine.
     * @return Result of time in Engine.
     */
    static TimeManager* Time() noexcept;
    /** @brief Input in Engine.
     * @return Result of input in Engine.
     */
    static InputManager* Input() noexcept;
    /** @brief Assets in Engine.
     * @return Result of assets in Engine.
     */
    static AssetManager* Assets() noexcept;
    /** @brief Sound in Engine.
     * @return Result of sound in Engine.
     */
    static SoundManager* Sound() noexcept;
    /** @brief Ui in Engine.
     * @return Result of ui in Engine.
     */
    static UIManager* UI() noexcept;
    /** @brief Scenes in Engine.
     * @return Result of scenes in Engine.
     */
    static SceneManager* Scenes() noexcept;
    /** @brief Physics in Engine.
     * @return Result of physics in Engine.
     */
    static PhysicsManager* Physics() noexcept;
    /** @brief Random in Engine.
     * @return Result of random in Engine.
     */
    static RandomManager* Random() noexcept;

    /** @brief Returns the active scene camera, or nullptr while no scene is active.
     * @note A scene-builder callback runs before activation; use its Scene* to
     * access the scene being constructed rather than this global accessor.
     * @return The active scene camera, or nullptr while no scene is active.
     */
    static Camera2D* GetCamera() noexcept;

    /** @brief Fixed delta in Engine.
     * @return Result of fixed delta in Engine.
     */
    static inline float FixedDelta() noexcept 
    {
        if (auto* e = GetInstancePtr())
            return e->mFixedDt;
        return 0.f;
    }

    /** @brief Retrieve the log errors from Engine.
     * @return Requested get log errors value or reference.
     */
    static bool GetLogErrors() noexcept
    {
        if (auto* e = GetInstancePtr())
            return e->mLogErrors;
        return false;
    }

    /** @brief Retrieve the fps from Engine.
     * @return Requested get fps value or reference.
     */
    static inline float GetFPS() noexcept;
    /** @brief Retrieve the aspect ratio from Engine.
     * @return Requested get aspect ratio value or reference.
     */
    static inline float GetAspectRatio() noexcept;
    /** @brief Retrieve the aspect ratio as fraction from Engine.
     * @return Requested get aspect ratio as fraction value or reference.
     */
    static inline Vec2I GetAspectRatioAsFraction() noexcept;

private:
    // -------- Property getters (NON-static) --------
    /** @brief Test whether Engine is running.
     * @return True when is running; otherwise false.
     */
    bool IsRunning_() const noexcept { return mRunning; }

    /** @brief Window in Engine.
     * @return Result of window in Engine.
     */
    WindowManager* Window_()  const noexcept { return Engine::Window(); }
    /** @brief Time in Engine.
     * @return Result of time in Engine.
     */
    TimeManager* Time_()    const noexcept { return Engine::Time(); }
    /** @brief Input in Engine.
     * @return Result of input in Engine.
     */
    InputManager* Input_()   const noexcept { return Engine::Input(); }
    /** @brief Assets in Engine.
     * @return Result of assets in Engine.
     */
    AssetManager* Assets_()  const noexcept { return Engine::Assets(); }
    /** @brief Sound in Engine.
     * @return Result of sound in Engine.
     */
    SoundManager* Sound_()   const noexcept { return Engine::Sound(); }
    /** @brief Ui in Engine.
     * @return Result of ui in Engine.
     */
    UIManager* UI_()      const noexcept { return Engine::UI(); }
    /** @brief Scenes in Engine.
     * @return Result of scenes in Engine.
     */
    SceneManager* Scenes_()  const noexcept { return Engine::Scenes(); }
    /** @brief Physics in Engine.
     * @return Result of physics in Engine.
     */
    PhysicsManager* Physics_() const noexcept { return Engine::Physics(); }
    /** @brief Random in Engine.
     * @return Result of random in Engine.
     */
    RandomManager* Random_() const noexcept { return Engine::Random(); }

    /** @brief Camera in Engine.
     * @return Result of camera in Engine.
     */
    Camera2D* Camera_() const noexcept { return Engine::GetCamera(); }

    /** @brief Fixed delta in Engine.
     * @return Result of fixed delta in Engine.
     */
    float FixedDelta_() const noexcept { return mFixedDt; }
    /** @brief Log errors in Engine.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool  LogErrors_()  const noexcept { return mLogErrors; }

    /** @brief Retrieve the fps from Engine.
     * @return Requested get fps value or reference.
     */
    float GetFPS_() const noexcept;
    /** @brief Retrieve the aspect ratio from Engine.
     * @return Requested get aspect ratio value or reference.
     */
    float GetAspectRatio_() const noexcept;
    /** @brief Retrieve the aspect ratio as fraction from Engine.
     * @return Requested get aspect ratio as fraction value or reference.
     */
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

/** @name Co-declared engine member descriptions
 * @brief Members sharing declarations with other fields.
 * @{ */
/** @var struct Engine::Config::Window Engine::Config::window
 * @brief Configuration block for the window subsystem.
 */

/** @var struct Engine::Config::Collision Engine::Config::collision
 * @brief Configuration block for the collision subsystem.
 */

/** @var struct Engine::Config::Renderer Engine::Config::renderer
 * @brief Frame profiling metric for renderer, sampled by render diagnostics.
 */

/** @var struct Engine::Config::Input Engine::Config::input
 * @brief Configuration block for the input subsystem.
 */

/** @var struct Engine::Config::Time Engine::Config::time
 * @brief Configuration block for the time subsystem.
 */

/** @var struct Engine::Config::Assets Engine::Config::assets
 * @brief Configuration block for the assets subsystem.
 */

/** @var struct Engine::Config::Sound Engine::Config::sound
 * @brief Configuration block for the sound subsystem.
 */

/** @var struct Engine::Config::UIStyle Engine::Config::uiStyle
 * @brief Configuration block for the ui style subsystem.
 */

/** @var struct Engine::Config::Physics Engine::Config::physics
 * @brief Configuration block for the physics subsystem.
 */

/** @var unsigned Engine::FrameProfile::renderEligibleCurrent
 * @brief Frame profiling metric for render eligible current, sampled by render diagnostics.
 */

/** @var unsigned Engine::FrameProfile::renderImmediateCulled
 * @brief Frame profiling metric for render immediate culled, sampled by render diagnostics.
 */

/** @var unsigned Engine::FrameProfile::renderWorldDraws
 * @brief Frame profiling metric for render world draws, sampled by render diagnostics.
 */

/** @var unsigned Engine::FrameProfile::renderUiDraws
 * @brief Frame profiling metric for render ui draws, sampled by render diagnostics.
 */

/** @var unsigned Engine::FrameProfile::renderTintUpdates
 * @brief Frame profiling metric for render tint updates, sampled by render diagnostics.
 */

/** @var std::uint64_t Engine::FrameProfile::renderWeightedCurrent
 * @brief Frame profiling metric for render weighted current, sampled by render diagnostics.
 */

/** @var std::uint64_t Engine::FrameProfile::renderEffectiveCurrent
 * @brief Frame profiling metric for render effective current, sampled by render diagnostics.
 */

/** @var unsigned Engine::FrameProfile::renderParallelFallback
 * @brief Frame profiling metric for render parallel fallback, sampled by render diagnostics.
 */

/** @var float Engine::FrameProfile::renderWorldMs
 * @brief Frame profiling metric for render world ms, sampled by render diagnostics.
 */

/** @var float Engine::FrameProfile::renderUiMs
 * @brief Frame profiling metric for render ui ms, sampled by render diagnostics.
 */
/** @} */

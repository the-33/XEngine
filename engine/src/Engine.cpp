#include "Engine.h"

#include <fstream>
#include <filesystem>
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
#include <cstdarg>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cmath>

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
#include "KeyCodes.h"

// Components
#include "Component.h"
#include "Behaviour.h"
#include "Transform.h"
#include "SpriteRenderer.h"
#include "Collider2D.h"
#include "GameObject.h"
#include "RigidBody2D.h"

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

// Independent of TimeManager::Tick(), which is only updated once per frame.
// steady_clock measurements include VSync blocking time where applicable.
using ProfileClock = std::chrono::steady_clock;
static inline float ProfileElapsed_(ProfileClock::time_point start) noexcept
{
    return std::chrono::duration<float>(ProfileClock::now() - start).count();
}


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
    // Call Stop() if it was not called already
    if (mStarted)
        StopImpl();

    // Destroy singletons in reverse dependency order
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

bool Engine::StartImpl(const std::string configPath, bool inlineJson) noexcept
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
    const bool configLoaded = inlineJson
        ? LoadConfigFromJson(configPath, "embedded config", cfg)
        : LoadConfigFromFile((configPath == "") ? kDefaultConfigPath : configPath, cfg);
    if (!configLoaded)
    {
        if (inlineJson) return false;
        // Missing config may use legacy defaults, but an EXISTING malformed
        // runtime config must not silently discard an explicit renderer
        // preference (or any other setting).
        std::error_code statError;
        const std::string resolvedPath =
            configPath.empty() ? kDefaultConfigPath : configPath;
        if (std::filesystem::exists(resolvedPath, statError) || statError)
        {
            LogError("Engine::Start()", "Invalid/unreadable existing engine config: " +
                     resolvedPath + ". Refusing to silently load defaults.");
            return false;
        }
        LogError("Engine warning", "Start(): Could not read config file, loading default values.");
    }
    // Contract: cwd is the assets root in editor/debug and in shipping.
    cfg.assets.assetsFolderPath = "./";
#if defined(XENGINE_RELEASE)
    cfg.logStats = false;
#endif
    printf("Configuration loaded.\n\n");

    mCfg = cfg;
    mPreviousRenderParallel = false;
    mRenderPathPrimed = false;
    mLastRenderWeightedPrevious = 0;
    mFixedDt = mCfg.time.fixedDt;
    mLogStats = mCfg.logStats;
#if defined(XENGINE_RELEASE)
    mShowStatsOverlay = false;
#endif
    mLogErrors = mCfg.logErrors;
    mAccumulator = 0.f;
    mDeterministicFixedStepsPerFrame = 0;
    mProfiles.clear();
    mFrameHistoryCount = 0;
    mFrameHistoryIndex = 0;

    printf("Initializing SDL... ");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0)
    {
        LogError("Engine::Start()", "SDL_Init with video, audio, events and gamecontroller failed.", SDL_GetError());
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
            mCfg.renderer.bgColor,
            mCfg.renderer.backend,
            mCfg.renderer.backendFallback
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
    if (!cfg.assets.defaultFontRelativePath.empty())
    {
        assets->LoadFont(cfg.assets.defaultFontRelativePath, cfg.uiStyle.defaultFontKey, cfg.uiStyle.defaultFontptSize);
#if defined(XENGINE_DEBUG)
        // Dedicated compact profiler fonts; gameplay UI keeps its own font size.
        if (auto* font = assets->LoadFont(cfg.assets.defaultFontRelativePath, "xengine_profiler_14", 14))
            (void)font;
        if (auto* bold = assets->LoadFont(cfg.assets.defaultFontRelativePath, "xengine_profiler_bold_14", 14))
            if (bold->GetSDL()) TTF_SetFontStyle(bold->GetSDL(), TTF_STYLE_BOLD);
#endif
    }
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
    collision->ConfigureBroadphase(mCfg.collision.broadphase,
                                    mCfg.collision.cellSize, mCfg.collision.autoCellSize);
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

    SafeToQuit = true;
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
        const auto frameStart = ProfileClock::now();
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
        const auto fixedUpdateStart = ProfileClock::now();
        mPhysicsTime = 0.f;
        mCollisionTime = 0.f;
        mFramePhysicsStepSec = 0.0;
        mFramePhysicsIntegrateSec = 0.0;
        mFramePhysicsBuildContactsSec = 0.0;
        mFramePhysicsSolveSec = 0.0;
        mFramePhysicsContactsProcessed = 0;
        mFramePhysicsSubsteps = 0;
        fixedUpdatesDoneThisFrame = 0;
        mCollisionParallelStepsThisFrame = 0;
        CollisionManager::GetInstancePtr()->ResetFrameProfile_();
        lastAccumulator = mAccumulator;
        if (mDeterministicFixedStepsPerFrame != 0u)
        {
            // Equal workloads: no accumulated wall-clock debt, no catch-up.
            for (unsigned i = 0; i < mDeterministicFixedStepsPerFrame; ++i)
            {
                DoFixedUpdates_(mFixedDt);
                ++fixedUpdatesDoneThisFrame;
            }
        }
        else
        {
            while (mAccumulator >= mFixedDt)
            {
                DoFixedUpdates_(mFixedDt);
                ++fixedUpdatesDoneThisFrame;
                mAccumulator -= mFixedDt;
            }
        }
        mFixedUpdateTime = ProfileElapsed_(fixedUpdateStart);

        // ------------------------------------------------------------
        // 4) Update variable
        // ------------------------------------------------------------
		DoUpdate_(time->deltaTime);

        // ------------------------------------------------------------
        // 5) Render
        // ------------------------------------------------------------
        
		DoRender_();

        // ------------------------------------------------------------
        // 6) Clean up objects marked for destruction
        // ------------------------------------------------------------
        ManageSceneQueues_();

        // ------------------------------------------------------------
		// 7) Apply pending scene changes
        // ------------------------------------------------------------
		ManageSceneChanges_();
        mFrameTime = ProfileElapsed_(frameStart);
        mFrameHistory[mFrameHistoryIndex] = mFrameTime;
        mFrameHistoryIndex = (mFrameHistoryIndex + 1) % mFrameHistory.size();
        mFrameHistoryCount = std::min(mFrameHistoryCount + 1, mFrameHistory.size());
        if (mCfg.profileCsv && mProfiles.size() < 100000)
        {
            const auto* col = CollisionManager::GetInstancePtr();
            FrameProfile sample;
            sample.frameMs = mFrameTime * 1000.f;
            sample.inputMs = mPollInputTime * 1000.f;
            sample.fixedMs = mFixedUpdateTime * 1000.f;
            sample.updateMs = mUpdateTime * 1000.f;
            sample.renderMs = mRenderTime * 1000.f;
            sample.renderRecordMs = mRenderRecordTime * 1000.f;
            sample.renderPrepareMs = mRenderPrepareTime * 1000.f;
            sample.renderSubmitMs = mRenderSubmitTime * 1000.f;
            sample.renderPresentMs = mRenderPresentTime * 1000.f;
            sample.renderWorkerWaitMs = mRenderWorkerWaitTime * 1000.f;
            if (auto* r = RenderManager::GetInstancePtr())
            {
                sample.renderSdlPresentMs = r->mLastSdlPresentMs;
                sample.renderPreparedCommands = r->mLastPreparedCount;
                sample.renderCulledCommands = r->mLastCulledCount;
                sample.renderWorkers = r->mLastWorkers;
                sample.renderEligibleCurrent = r->mPublicWorldCommandsThisFrame;
                sample.renderTotalRecorded = r->mLastRecordedCommands;
                sample.renderImmediateCulled = r->mImmediateCulledCommands;
                sample.renderBackgroundDraws = r->mBackgroundDraws;
                sample.renderWorldDraws = r->mWorldDraws;
                sample.renderDebugDraws = r->mDebugDraws;
                sample.renderUiDraws = r->mUiDraws;
                sample.renderTintCacheHits = r->mTintCacheHits;
                sample.renderTintUpdates = r->mTintUpdates;
                sample.renderBackgroundMs = r->mBackgroundMs;
                sample.renderDebugMs = r->mDebugMs;
                sample.renderWorldMs = mRenderWorldTime * 1000.f;
                sample.renderUiMs = mRenderUiTime * 1000.f;
                sample.renderSpriteSubmitMs = r->mSpriteSubmitMs;
                sample.renderEligiblePrevious = mLastRenderEligiblePrevious;
                sample.renderPathReason = static_cast<unsigned>(mLastRenderPath);
                sample.renderWeightedPrevious = mLastRenderWeightedPrevious;
                sample.renderWeightedCurrent = r->mPublicWorldWorkUnitsThisFrame;
                sample.renderPathSwitched = mRenderPathSwitched;
                sample.renderSelectionMode = mCfg.renderer.parallelSelection == RenderDiagnostics::Selection::Adaptive ? 1u : 0u;
                sample.renderEffectivePrevious = mLastRenderEffectivePrevious;
                sample.renderEffectiveCurrent = r->mPublicEffectiveWorldWorkUnitsThisFrame;
                sample.renderEstimatedOffscreen = r->mEstimatedOffscreenSprites;
                sample.renderVisibilitySkipped = r->mSkippedVisibilitySprites;
                sample.renderParallelExecuted = mRenderParallelExecuted;
                sample.renderParallelFallback = mRenderParallelFallback;
                sample.renderExecutedSwitched = mRenderExecutedSwitched;
                sample.renderWorkEstimateMode = static_cast<unsigned>(mWorkEstimateActive);
                sample.renderRouteOverride = static_cast<unsigned>(mRouteProbe);
                sample.renderSelectionMs = mRenderSelectionTime * 1000.f;
            }
            sample.physicsMs = mPhysicsTime * 1000.f;
            sample.collisionsMs = mCollisionTime * 1000.f;
            sample.fixedSteps = fixedUpdatesDoneThisFrame;
            sample.collisionParallelSteps = mCollisionParallelStepsThisFrame;
            sample.physicsStepMs = mFramePhysicsStepSec * 1000.0;
            sample.physicsIntegrateMs = mFramePhysicsIntegrateSec * 1000.0;
            sample.physicsBuildContactsMs = mFramePhysicsBuildContactsSec * 1000.0;
            sample.physicsSolveMs = mFramePhysicsSolveSec * 1000.0;
            sample.physicsSubsteps = mFramePhysicsSubsteps;
            sample.physicsContactsProcessed = mFramePhysicsContactsProcessed;
            sample.deterministicStepsPerFrame = mDeterministicFixedStepsPerFrame;
            if (col)
            {
                sample.collisionSnapshotMs = col->mFrameSnapshotMs;
                sample.collisionGridBuildMs = col->mFrameGridBuildMs;
                sample.collisionAutoSelectorMs = col->mFrameAutoSelectorMs;
                sample.collisionBroadphaseMode = col->mAutoBroadphase ? 2u : (col->mSpatialBroadphase ? 1u : 0u);
                sample.collisionGridPasses = col->mFrameGridPasses;
                sample.collisionBruteforcePasses = col->mFrameBruteforcePasses;
                sample.collisionGridCellSizeLast = col->mFrameLastGridCellSize;
                sample.collisionAutoCalibrationPasses = col->mFrameAutoCalibrationPasses;
                sample.collisionAutoContextLast = col->mFrameAutoContextLast;
                sample.collisionAutoContextTransitions = col->mFrameAutoContextTransitions;
                sample.collisionAutoCellHalfPasses = col->mFrameAutoGridCell1Passes;
                sample.collisionAutoCellBasePasses = col->mFrameAutoGridCell2Passes;
                sample.collisionAutoCellDoublePasses = col->mFrameAutoGridCell4Passes;
                sample.collisionContactSignature = col->mFrameContactSignature;
                sample.collisionCandidatesMs = col->mFrameCandidateMs;
                sample.collisionMergeMs = col->mFrameMergeMs;
                sample.broadphaseTests = col->mFrameBroadphaseTests;
                sample.narrowphaseTests = col->mFrameNarrowphaseTests;
                sample.contactsBuilt = col->mFrameContactsBuilt;
                sample.collisionPasses = col->mFrameCollisionPasses;
                sample.collisionParallelPasses = col->mFrameParallelPasses;
                sample.workers = col->ParallelWorkerCount();
            }
            mProfiles.emplace_back(sample);
        }
        SafeToQuit = true;
    }

    SafeToQuit = true;
    WriteProfiles_();
    return 0;
}

// The old implementation rendered SDL on a worker thread and serialized
// gameplay behind a global mutex. Besides violating SDL's main-thread render
// contract on some platforms, snapshot copies outweighed useful concurrency.
// The new mode keeps the full lifecycle on the main thread and enables
// persistent workers only for independent collision geometry calculations.
int Engine::RunMultiThreadingImpl() noexcept
{
    auto* collision = CollisionManager::GetInstancePtr();
    if (!collision) return -1;
    collision->EnableParallelCollision(true);
    return RunSingleThreadingImpl();
}


void Engine::WriteProfiles_() noexcept
{
    if (!mCfg.profileCsv || mProfiles.empty()) return;
    const char* path = mCfg.multiThreading ? "xengine_profile_parallel.csv"
                                          : "xengine_profile_single.csv";
    std::ofstream out(path, std::ios::trunc);
    if (!out)
    {
        std::fprintf(stderr, "[PROFILE] Cannot write %s\n", path);
        return;
    }
    out << "frame,frame_ms,input_ms,fixed_ms,update_ms,render_present_ms,"
           "physics_ms,collision_ms,collision_snapshot_ms,collision_candidates_ms,"
           "collision_merge_ms,fixed_steps,broadphase_tests,narrowphase_tests,workers,"
           "render_record_ms,render_prepare_ms,render_submit_ms,render_only_present_ms,"
           "render_sdl_present_ms,"
           "render_worker_wait_ms,render_prepared_commands,render_culled_commands,render_workers,"
           "render_path_reason,render_eligible_previous,render_eligible_current,"
           "render_recorded_total,render_immediate_culled,render_background_draws,render_world_draws,"
           "render_debug_draws,render_ui_draws,render_background_ms,render_world_ms,"
           "render_debug_ms,render_ui_ms,render_sprite_submit_ms,"
           "render_tint_cache_hits,render_tint_updates,collision_parallel_steps,"
           "collision_passes,collision_parallel_passes,contacts_built,"
           "physics_step_ms,physics_integrate_ms,physics_build_contacts_ms,"
           "physics_solve_ms,physics_substeps,physics_contacts_processed,"
           "deterministic_steps_per_frame,collision_grid_build_ms,"
           "collision_broadphase_mode,collision_contact_signature,"
           "collision_grid_passes,collision_bruteforce_passes,"
           "collision_grid_cell_size_last,collision_auto_calibration_passes,"
           "collision_auto_cell_half_passes,collision_auto_cell_base_passes,"
           "collision_auto_cell_double_passes,collision_auto_selector_ms,"
           "collision_auto_context_last,collision_auto_context_transitions,"
           "render_weighted_units_previous,render_weighted_units_current,"
           "render_path_switched,render_selection_mode,"
           "render_effective_units_previous,render_effective_units_current,"
           "render_estimated_offscreen_sprites,render_visibility_skipped_sprites,render_parallel_executed,"
           "render_parallel_fallback,render_executed_switched,"
           "render_work_estimate_mode,render_route_override,render_selector_decision_ms\n";
    out << std::fixed << std::setprecision(5);
    for (std::size_t i = 0; i < mProfiles.size(); ++i)
    {
        const auto& r = mProfiles[i];
        out << i << ',' << r.frameMs << ',' << r.inputMs << ',' << r.fixedMs
            << ',' << r.updateMs << ',' << r.renderMs << ',' << r.physicsMs
            << ',' << r.collisionsMs << ',' << r.collisionSnapshotMs
            << ',' << r.collisionCandidatesMs << ',' << r.collisionMergeMs
            << ',' << r.fixedSteps << ',' << r.broadphaseTests
            << ',' << r.narrowphaseTests << ',' << r.workers
            << ',' << r.renderRecordMs << ',' << r.renderPrepareMs
            << ',' << r.renderSubmitMs << ',' << r.renderPresentMs
            << ',' << r.renderSdlPresentMs
            << ',' << r.renderWorkerWaitMs << ',' << r.renderPreparedCommands
            << ',' << r.renderCulledCommands << ',' << r.renderWorkers
            << ',' << r.renderPathReason << ',' << r.renderEligiblePrevious
            << ',' << r.renderEligibleCurrent << ',' << r.renderTotalRecorded
            << ',' << r.renderImmediateCulled
            << ',' << r.renderBackgroundDraws << ',' << r.renderWorldDraws
            << ',' << r.renderDebugDraws << ',' << r.renderUiDraws
            << ',' << r.renderBackgroundMs << ',' << r.renderWorldMs
            << ',' << r.renderDebugMs << ',' << r.renderUiMs
            << ',' << r.renderSpriteSubmitMs
            << ',' << r.renderTintCacheHits << ',' << r.renderTintUpdates
            << ',' << r.collisionParallelSteps
            << ',' << r.collisionPasses << ',' << r.collisionParallelPasses
            << ',' << r.contactsBuilt << ',' << r.physicsStepMs
            << ',' << r.physicsIntegrateMs << ',' << r.physicsBuildContactsMs
            << ',' << r.physicsSolveMs << ',' << r.physicsSubsteps
            << ',' << r.physicsContactsProcessed << ',' << r.deterministicStepsPerFrame
            << ',' << r.collisionGridBuildMs << ',' << r.collisionBroadphaseMode
            << ',' << r.collisionContactSignature
            << ',' << r.collisionGridPasses << ',' << r.collisionBruteforcePasses
            << ',' << r.collisionGridCellSizeLast << ',' << r.collisionAutoCalibrationPasses
            << ',' << r.collisionAutoCellHalfPasses << ',' << r.collisionAutoCellBasePasses
            << ',' << r.collisionAutoCellDoublePasses
            << ',' << r.collisionAutoSelectorMs
            << ',' << r.collisionAutoContextLast
            << ',' << r.collisionAutoContextTransitions
            << ',' << r.renderWeightedPrevious << ',' << r.renderWeightedCurrent
            << ',' << r.renderPathSwitched << ',' << r.renderSelectionMode
            << ',' << r.renderEffectivePrevious << ',' << r.renderEffectiveCurrent
            << ',' << r.renderEstimatedOffscreen << ',' << r.renderVisibilitySkipped
            << ',' << r.renderParallelExecuted
            << ',' << r.renderParallelFallback << ',' << r.renderExecutedSwitched
            << ',' << r.renderWorkEstimateMode << ',' << r.renderRouteOverride
            << ',' << r.renderSelectionMs
            << '\n';
    }
    std::printf("[PROFILE] Saved %zu frame samples: %s\n", mProfiles.size(), path);
    mProfiles.clear();
}

void Engine::StopImpl() noexcept
{
    if (!mStarted || mShuttingDown) return;
    mRunning = false;
    if (!SafeToQuit) return;
    mShuttingDown = true;

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
    mShuttingDown = false;
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

Camera2D* Engine::GetCamera() noexcept {
    auto* scenes = SceneManager::GetInstancePtr_NO_ERROR_MSG();
    if (!scenes) return nullptr;
    auto* active = scenes->GetActive();
    return active ? active->GetCamera() : nullptr;
}

inline float Engine::GetFPS() noexcept
{
    if (!IsRunning()) return 0.f;

    // Uses the most recently completed frame, measured by the same monotonic
    // wall clock in both execution modes. The old renderTime value is no
    // longer updated by RenderManager and used to cause infinite FPS.
    const float frameSeconds = GetInstancePtr()->mFrameTime;
    return frameSeconds > 0.f && std::isfinite(frameSeconds)
        ? 1.f / frameSeconds
        : 0.f;
}

inline float Engine::GetAspectRatio() noexcept
{
    if (!IsRunning()) return 0.f;
    Vec2I res = WindowManager::GetInstancePtr()->GetDrawableSize();
    float aspect = (float)res.x / (float)res.y;

    GetInstancePtr()->mAspectRatio = aspect;

    return aspect;
}

// Helper to express the aspect ratio as a fraction
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
    // Expects "r,g,b,a"
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
-----CONFIGURATION FILE TEMPLATE-----

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

    // Cast an int to uint8_t with basic clamping
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
            if (!v.is_number()) return fallback;      // accepts int or float
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
    if (!file.is_open()) return false;
    std::ostringstream oss;
    oss << file.rdbuf();
    return LoadConfigFromJson(oss.str(), path, out);
}

bool Engine::LoadConfigFromJson(const std::string& text, const std::string& origin, Config& out) noexcept
{
    try
    {
        json j = json::parse(text); // throws json::parse_error for invalid JSON

        // ===========================
        //  Engine
        // ===========================
        if (auto it = j.find("Engine"); it != j.end() && it->is_object())
        {
            const json& eng = *it;
            if (eng.contains("multiThreading") && eng["multiThreading"].is_boolean())
                out.multiThreading = eng["multiThreading"].get<bool>();
            if (eng.contains("profileCsv") && eng["profileCsv"].is_boolean())
                out.profileCsv = eng["profileCsv"].get<bool>();
            if (eng.contains("logStats") && eng["logStats"].is_boolean())
                out.logStats = eng["logStats"].get<bool>();
            if (eng.contains("logErrors") && eng["logErrors"].is_boolean())
                out.logErrors = eng["logErrors"].get<bool>();
        }

        // ===========================
        //  Collision broadphase
        // ===========================
        if (auto it = j.find("Collision"); it != j.end() && it->is_object())
        {
            const json& col = *it;
            if (col.contains("broadphase") && col["broadphase"].is_string()) {
                const auto mode = col["broadphase"].get<std::string>();
                if (mode != "bruteforce" && mode != "grid" && mode != "auto") return false;
                out.collision.broadphase = mode;
            }
            if (col.contains("cellSize")) {
                if (col["cellSize"].is_string()) {
                    if (col["cellSize"].get<std::string>() != "auto" ||
                        out.collision.broadphase != "auto") return false;
                    out.collision.autoCellSize = true;
                } else if (col["cellSize"].is_number()) {
                    const float v = col["cellSize"].get<float>();
                    if (!std::isfinite(v) || v < 0.05f || v > 4096.f) return false;
                    out.collision.cellSize = v;
                    out.collision.autoCellSize = false;
                } else return false;
            }
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
            if (r.contains("backend"))
            {
                // A single config file can travel with the game on Windows
                // and Linux. An omitted platform keeps SDL auto-selection.
                const json& platforms = r["backend"];
                if (!platforms.is_object()) return false;
                for (auto itBackend = platforms.begin(); itBackend != platforms.end(); ++itBackend)
                {
                    if (itBackend.key() != "windows" && itBackend.key() != "linux")
                        return false;
                    if (!itBackend.value().is_string()) return false;
                    const std::string value = itBackend.value().get<std::string>();
                    if (value.empty() || value.size() > 64u) return false;
#if defined(_WIN32)
                    if (itBackend.key() == "windows") out.renderer.backend = value;
#elif defined(__linux__)
                    if (itBackend.key() == "linux") out.renderer.backend = value;
#endif
                }
            }
            if (r.contains("backendFallback"))
            {
                if (!r["backendFallback"].is_boolean()) return false;
                out.renderer.backendFallback = r["backendFallback"].get<bool>();
            }
            if (r.contains("parallelPreparation") && r["parallelPreparation"].is_boolean())
                out.renderer.parallelPreparation = r["parallelPreparation"].get<bool>();
            // Global crossover reference; selection strategy is independent.
            if (r.contains("parallelMinCommands")) {
                if (!r["parallelMinCommands"].is_number_unsigned() &&
                    !r["parallelMinCommands"].is_number_integer()) return false;
                const auto value = r["parallelMinCommands"].get<std::int64_t>();
                if (value < 1 || value > 100000) return false;
                out.renderer.parallelMinCommands = static_cast<std::size_t>(value);
            }
            if (r.contains("parallelSelection")) {
                if (!r["parallelSelection"].is_string()) return false;
                const auto value = r["parallelSelection"].get<std::string>();
                if (value == "fixed") out.renderer.parallelSelection = RenderDiagnostics::Selection::Fixed;
                else if (value == "adaptive") out.renderer.parallelSelection = RenderDiagnostics::Selection::Adaptive;
                else return false;
            }
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
        // e.byte = approximate error byte offset within the string
        const auto [line, col] = ByteToLineCol(text, e.byte);
        LogError("Engine::LoadConfigFromFile",
            (std::string("JSON parse error at ") + origin +
                " line " + std::to_string(line) +
                ", col " + std::to_string(col) +
                ": " + e.what()).c_str());
        return false;
    }
    catch (const json::exception& e)
    {
        // Type errors / out_of_range / etc.
        LogError("Engine::LoadConfigFromFile",
            (std::string("JSON error in ") + origin + ": " + e.what()).c_str());
        return false;
    }
    catch (...)
    {
        LogError("Engine::LoadConfigFromFile",
            (std::string("Unknown error parsing JSON config: ") + origin).c_str());
        return false;
    }

    return true;
}

void Engine::FrameTick_() noexcept
{
	if (!mRunning) return;

    auto* time = TimeManager::GetInstancePtr();

    time->Tick();
    if (mDeterministicFixedStepsPerFrame != 0u)
    {
        // Use simulation time for variable Update callbacks as well, so motion
        // independent of CPU speed sees the same dt on every benchmark mode.
        time->delta = mFixedDt * static_cast<float>(mDeterministicFixedStepsPerFrame);
        mAccumulator = 0.f;
    }
    else
    {
        mAccumulator += time->deltaTime;
    }
}

void Engine::PollInput_() noexcept
{
	if (!mRunning) return;

    const auto pollStart = ProfileClock::now();

    auto* input = InputManager::GetInstancePtr();

	input->Update();
    if (XEngineBuild::IsDebug && mLogStats && input && input->KeyPressed(Key::F3))
    {
        mShowStatsOverlay = !mShowStatsOverlay;
        mStatsNextRefresh = 0.0;
        mStatsPage = 0;
    }
    if (XEngineBuild::IsDebug && mShowStatsOverlay && input &&
        input->KeyPressed(Key::F4) && !mStatsLayout.pages.empty())
    {
        mStatsPage = (mStatsPage + 1) % mStatsLayout.pages.size();
    }
    UIManager::GetInstancePtr()->Begin();

    mPollInputTime = ProfileElapsed_(pollStart);
}

void Engine::DoFixedUpdates_(float dt) noexcept
{
	if (!mRunning) return;

    // Optional per-frame user callback
    if (mOnFixedUpdate)
        mOnFixedUpdate(mFixedDt);
    // FixedUpdate of the active scene
    SceneManager::GetInstancePtr()->FixedUpdate(mFixedDt);

    const auto physicsStart = ProfileClock::now();
    auto* physics = PhysicsManager::GetInstancePtr();
    physics->Step(mFixedDt);
    mPhysicsTime += ProfileElapsed_(physicsStart);
    mFramePhysicsStepSec += physics->stepTimeSec;
    mFramePhysicsIntegrateSec += physics->integrateTimeSec;
    mFramePhysicsBuildContactsSec += physics->buildContactsTimeSec;
    mFramePhysicsSolveSec += physics->solveTimeSec;
    mFramePhysicsContactsProcessed += physics->nContactsProcessedThisFrame;
    mFramePhysicsSubsteps += static_cast<unsigned>(physics->nSubstepsThisFrame);

    const auto collisionStart = ProfileClock::now();
    auto* collision = CollisionManager::GetInstancePtr();
    collision->DetectAndDispatch();
    if (collision->mLastUsedParallelCandidates) ++mCollisionParallelStepsThisFrame;
    mCollisionTime += ProfileElapsed_(collisionStart);
}

void Engine::DoUpdate_(float dt) noexcept
{
	if (!mRunning) return;

    auto* scenes = SceneManager::GetInstancePtr();
    const auto updateStart = ProfileClock::now();

	// Optional per-frame user callback
    if (mOnUpdate)
        mOnUpdate(dt);
	scenes->Update(dt);

	mUpdateTime = ProfileElapsed_(updateStart);
}

void Engine::DoRender_() noexcept
{
    if (!mRunning) return;
    const auto renderStart = ProfileClock::now();
    auto* scenes = SceneManager::GetInstancePtr();
    auto* render = RenderManager::GetInstancePtr();
    auto* ui = UIManager::GetInstancePtr();
    Scene* active = scenes ? scenes->GetActive() : nullptr;
    if (render)
    {
        render->mLastSdlPresentMs = 0.f;
        render->mMeasureSdlPresent = mCfg.profileCsv;
    }
    if (!active || !active->GetCamera() || !render || !ui) return;

    mRenderRecordTime = 0.f;
    mRenderPrepareTime = 0.f;
    mRenderSubmitTime = 0.f;
    mRenderPresentTime = 0.f;
    mRenderWorkerWaitTime = 0.f;
    const auto* collision = CollisionManager::GetInstancePtr();
    ParallelExecutor* pool = collision ? collision->mParallelExecutor.get() : nullptr;
    // Use the old zero-allocation immediate path for small scenes.
    // Last frame's world command count is used to avoid snapshot overhead
    // when threading would almost certainly be slower.
    mLastRenderEligiblePrevious = render->mLastAttemptedWorldCommands;
    mLastRenderWeightedPrevious = render->mLastWorldWorkUnits;
    mLastRenderEffectivePrevious = render->mLastEffectiveWorldWorkUnits;
    const bool adaptive = mCfg.renderer.parallelSelection == RenderDiagnostics::Selection::Adaptive;
    const auto selectionStart = mCfg.profileCsv ? ProfileClock::now() : ProfileClock::time_point{};
    mLastRenderPath = adaptive
        ? RenderDiagnostics::SelectAdaptive(
            mCfg.multiThreading, mCfg.renderer.parallelPreparation,
            pool ? pool->WorkerCount() : 0, mLastRenderEffectivePrevious,
            mCfg.renderer.parallelMinCommands, mPreviousRenderParallel)
        : RenderDiagnostics::Select(
            mCfg.multiThreading, mCfg.renderer.parallelPreparation,
            pool ? pool->WorkerCount() : 0, mLastRenderEligiblePrevious,
            mCfg.renderer.parallelMinCommands);
    if (mCfg.profileCsv) mRenderSelectionTime = ProfileElapsed_(selectionStart);
    else mRenderSelectionTime = 0.f;
    if (mRouteProbe == RenderDiagnostics::RouteOverride::Immediate)
        mLastRenderPath = RenderDiagnostics::Path::ForcedImmediate;
    else if (mRouteProbe == RenderDiagnostics::RouteOverride::Recorded)
        mLastRenderPath = RenderDiagnostics::Path::Parallel;
    const bool record = mLastRenderPath == RenderDiagnostics::Path::Parallel;
    mRenderPathSwitched = mRenderPathPrimed && (record != mPreviousRenderParallel) ? 1u : 0u;
    mRenderPathPrimed = true;
    mPreviousRenderParallel = record;
    mRenderWorldTime = 0.f;
    mRenderUiTime = 0.f;
    render->mPublicWorldCommandsThisFrame = 0;
    render->mPublicWorldWorkUnitsThisFrame = 0;
    render->mPublicEffectiveWorldWorkUnitsThisFrame = 0;
    render->mEstimatedOffscreenSprites = 0;
    render->mSkippedVisibilitySprites = 0;
    mWorkEstimateActive = mWorkEstimateProbe == RenderDiagnostics::WorkEstimate::Auto
        ? (adaptive ? RenderDiagnostics::WorkEstimate::Visibility : RenderDiagnostics::WorkEstimate::Weighted)
        : mWorkEstimateProbe;
    render->mWorkEstimateMode = mWorkEstimateActive;
    render->mSaturateEffectiveWork = adaptive &&
        mWorkEstimateProbe == RenderDiagnostics::WorkEstimate::Auto;
    render->mSelectorEntryBoundary = RenderDiagnostics::EntryBoundary(
        mCfg.renderer.parallelMinCommands);
    render->mLastRecordedCommands = render->mImmediateCulledCommands = 0;
    render->mBackgroundDraws = render->mWorldDraws = 0;
    render->mDebugDraws = render->mUiDraws = 0;
    render->mTintCacheHits = render->mTintUpdates = 0;
    render->mBackgroundMs = render->mDebugMs = 0.f;
    render->mSpriteSubmitMs = 0.f;
    render->mMeasureSpriteSubmissions = mCfg.profileCsv;
    render->nDrawCallsThisFrame = 0;
    render->nDebugDrawCallsThisFrame = 0;
    render->nUIDrawCallsThisFrame = 0;
    render->nRenderedSpritesThisFrame = 0;
    render->mLastPreparedCount = 0;
    render->mLastCulledCount = 0;
    render->mLastWorkers = 0;
    if (record)
    {
        const auto recordStart = ProfileClock::now();
        render->BeginRecord(active->GetCamera(), render->mReusableSnapshot);
        const auto worldStart = ProfileClock::now();
        scenes->Render();
        mRenderWorldTime = ProfileElapsed_(worldStart);
        const auto uiStart = ProfileClock::now();
        render->mDrawPhase = RenderManager::DrawPhase::UI;
        ui->End();
        if (XEngineBuild::IsDebug && mShowStatsOverlay)
        {
            DrawStatsOverlay_();
            ui->End();
        }
        render->mDrawPhase = RenderManager::DrawPhase::World;
        mRenderUiTime = ProfileElapsed_(uiStart);
        render->EndRecord();
        render->mLastRecordedCommands = static_cast<unsigned>(render->mReusableSnapshot.cmds.size());
        mRenderRecordTime = ProfileElapsed_(recordStart);
        // The adaptive selector has already applied its weighted threshold;
        // do not reject valid complex workloads by raw command count again.
        render->PrepareSnapshot(render->mReusableSnapshot, pool,
                                adaptive || mRouteProbe == RenderDiagnostics::RouteOverride::Recorded
                                    ? 1u : mCfg.renderer.parallelMinCommands);
        mRenderPrepareTime = render->mLastPrepareMs / 1000.f;
        mRenderWorkerWaitTime = render->mLastWaitMs / 1000.f;
        const auto submitStart = ProfileClock::now();
        render->RenderSnapshot(render->mReusableSnapshot);
        mRenderSubmitTime = ProfileElapsed_(submitStart);
        const auto presentStart = ProfileClock::now();
        render->End();
        mRenderPresentTime = ProfileElapsed_(presentStart);
        render->mLastAttemptedWorldCommands = render->mPublicWorldCommandsThisFrame;
        render->mLastWorldWorkUnits = render->mPublicWorldWorkUnitsThisFrame;
        render->mLastEffectiveWorldWorkUnits = render->mPublicEffectiveWorldWorkUnitsThisFrame;
    }
    else
    {
        const auto submitStart = ProfileClock::now();
        render->Begin(*active->GetCamera());
        const auto worldStart = ProfileClock::now();
        scenes->Render();
        mRenderWorldTime = ProfileElapsed_(worldStart);
        const auto uiStart = ProfileClock::now();
        render->mDrawPhase = RenderManager::DrawPhase::UI;
        ui->End();
        if (XEngineBuild::IsDebug && mShowStatsOverlay)
        {
            DrawStatsOverlay_();
            ui->End();
        }
        render->mDrawPhase = RenderManager::DrawPhase::World;
        mRenderUiTime = ProfileElapsed_(uiStart);
        mRenderSubmitTime = ProfileElapsed_(submitStart);
        const auto presentStart = ProfileClock::now();
        render->End();
        mRenderPresentTime = ProfileElapsed_(presentStart);
        render->mLastAttemptedWorldCommands = render->mPublicWorldCommandsThisFrame;
        render->mLastWorldWorkUnits = render->mPublicWorldWorkUnitsThisFrame;
        render->mLastEffectiveWorldWorkUnits = render->mPublicEffectiveWorldWorkUnitsThisFrame;
    }
    // Selection can request recording but the CURRENT frame may fall below
    // the fixed dispatcher threshold (e.g. a sudden scene-load drop).
    // Do not confuse that fallback with worker execution in the profiler.
    mRenderParallelExecuted = render->mLastWorkers > 0 ? 1u : 0u;
    mRenderParallelFallback = (record && !mRenderParallelExecuted) ? 1u : 0u;
    mRenderExecutedSwitched = mExecutedPathPrimed &&
        (bool(mRenderParallelExecuted) != mPreviousExecutedParallel) ? 1u : 0u;
    mPreviousExecutedParallel = bool(mRenderParallelExecuted);
    mExecutedPathPrimed = true;
    mRenderTime = ProfileElapsed_(renderStart);
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

#if defined(XENGINE_DEBUG)
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

    // Refresh values and positions together: the text width is not constant.
    const double now = time->timeSinceStart;
    const Vec2I viewport = win->GetDrawableSize();
    const bool refreshed = now >= mStatsNextRefresh;
    if (refreshed)
    {
        mStatsNextRefresh = now + 0.25; // four times per second
        mStatsLines.clear();
        mStatsLines.reserve(96);

        Vec2I res = win->GetDrawableSize();
        Vec2I aspectAsFr = GetAspectRatioAsFraction();

        const double dt = (double)time->deltaTime;

        const float fpsNow = GetFPS();
        averageFps += fpsNow;
        averageFps *= 0.5f;

        averageFixedUpdatesPerFrame += fixedUpdatesDoneThisFrame;
        averageFixedUpdatesPerFrame = (int)std::ceil(((float)averageFixedUpdatesPerFrame) * 0.5f);


        mStatsLines.push_back("=== XEngine Stats ===");
        mStatsLines.push_back(SPrintf_("Time Since Start:                   %.1f sec", time->SinceStart()));
        mStatsLines.push_back(SPrintf_("FPS:                                %.2f fps", fpsNow));
        mStatsLines.push_back(SPrintf_("Average fps:                        %.2f fps", averageFps));
        mStatsLines.push_back(SPrintf_("Delta Time (limited to 0.25):       %.6f sec", dt));
        mStatsLines.push_back(SPrintf_("Fixed Dt:                           %.6f sec", (double)mFixedDt));
        mStatsLines.push_back(SPrintf_("Accumulator this frame:             %.6f sec", (double)mAccumulator));
        mStatsLines.push_back(SPrintf_("Fixed updates this frame:           %d fixed updates", fixedUpdatesDoneThisFrame));
        if (mDeterministicFixedStepsPerFrame)
            mStatsLines.push_back(SPrintf_("Deterministic fixed steps / frame:  %u", mDeterministicFixedStepsPerFrame));
        mStatsLines.push_back(SPrintf_("Average Fixed Updates per frame:    %d fixed updates", averageFixedUpdatesPerFrame));
        mStatsLines.push_back("");

        // Report the settings currently in use, not only the requested JSON
        // values. SDL can select a fallback renderer, and both automatic
        // broadphase and render preparation can change paths at runtime.
        SDL_RendererInfo rendererInfo{};
        const char* activeBackend = "unavailable";
        if (SDL_Renderer* sdlRenderer = render->SDL())
        {
            if (SDL_GetRendererInfo(sdlRenderer, &rendererInfo) == 0 &&
                rendererInfo.name && rendererInfo.name[0] != '\0')
                activeBackend = rendererInfo.name;
        }

        const unsigned cpuWorkers = collision->ParallelWorkerCount();
        const bool workersAvailable = mCfg.multiThreading && cpuWorkers > 0;
        const bool renderParallel = mLastRenderPath == RenderDiagnostics::Path::Parallel;
        const char* configuredBroadphase = collision->mAutoBroadphase ? "auto"
            : (collision->mSpatialBroadphase ? "grid" : "bruteforce");
        const char* activeBroadphase = "idle";
        if (collision->mFrameGridPasses > 0 && collision->mFrameBruteforcePasses > 0)
            activeBroadphase = "mixed";
        else if (collision->mFrameGridPasses > 0)
            activeBroadphase = "grid";
        else if (collision->mFrameBruteforcePasses > 0)
            activeBroadphase = "bruteforce";

        mStatsLines.push_back("=== Active Configuration ===");
        mStatsLines.push_back(SPrintf_("Multithreading: %s (%u workers; configured %s)",
            workersAvailable ? "available" : "off", cpuWorkers,
            mCfg.multiThreading ? "on" : "off"));
        mStatsLines.push_back(SPrintf_("Parallel work (last frame): render %s, collision %u passes",
            renderParallel ? "on" : "off", collision->mFrameParallelPasses));
        mStatsLines.push_back(SPrintf_("Render backend: %s (requested %s)",
            activeBackend, mCfg.renderer.backend.c_str()));
        mStatsLines.push_back(SPrintf_("Parallel preparation: %s (last path %s)",
            mCfg.renderer.parallelPreparation ? "on" : "off",
            RenderDiagnostics::Name(mLastRenderPath)));
        mStatsLines.push_back(SPrintf_("Render selection: %s",
            RenderDiagnostics::Name(mCfg.renderer.parallelSelection)));
        mStatsLines.push_back(SPrintf_("Min commands: %zu", mCfg.renderer.parallelMinCommands));
        mStatsLines.push_back(SPrintf_("Broadphase: %s (last frame %s)",
            configuredBroadphase, activeBroadphase));
        if (collision->mFrameLastGridCellSize > 0.f)
        {
            if (collision->mAutoGridCellSize)
                mStatsLines.push_back(SPrintf_("Cell size: auto (effective %.3f; base %.3f)",
                    (double)collision->mFrameLastGridCellSize,
                    (double)collision->mBroadphaseCellSize));
            else
                mStatsLines.push_back(SPrintf_("Cell size: %.3f (fixed)",
                    (double)collision->mBroadphaseCellSize));
        }
        else if (collision->mAutoGridCellSize)
            mStatsLines.push_back(SPrintf_("Cell size: auto (base %.3f; grid inactive)",
                (double)collision->mBroadphaseCellSize));
        else if (collision->mSpatialBroadphase || collision->mAutoBroadphase)
            mStatsLines.push_back(SPrintf_("Cell size: %.3f (grid inactive)",
                (double)collision->mBroadphaseCellSize));
        else
            mStatsLines.push_back(SPrintf_("Cell size: unused (configured %.3f)",
                (double)collision->mBroadphaseCellSize));
        mStatsLines.push_back("");


        mStatsLines.push_back(mCfg.multiThreading ? "Frame phases (parallel CPU workers):" : "Frame phases (single-threaded):");
        mStatsLines.push_back(SPrintf_(" * Input Update:      %.3f ms", (double)mPollInputTime * 1000.0));
        mStatsLines.push_back(SPrintf_(" * Fixed Update:      %.3f ms", (double)mFixedUpdateTime * 1000.0));
        mStatsLines.push_back(SPrintf_("   - Physics Step:    %.3f ms", (double)mPhysicsTime * 1000.0));
        mStatsLines.push_back(SPrintf_("   - Collision Events:%.3f ms", (double)mCollisionTime * 1000.0));
        mStatsLines.push_back(SPrintf_(" * Update:            %.3f ms", (double)mUpdateTime * 1000.0));
        mStatsLines.push_back(SPrintf_(" * Render + Present:  %.3f ms", (double)mRenderTime * 1000.0));
        mStatsLines.push_back(SPrintf_(" * Record:            %.3f ms", (double)mRenderRecordTime * 1000.0));
        mStatsLines.push_back(SPrintf_(" * CPU Prepare:       %.3f ms", (double)mRenderPrepareTime * 1000.0));
        mStatsLines.push_back(SPrintf_(" * SDL Submit:        %.3f ms", (double)mRenderSubmitTime * 1000.0));
        mStatsLines.push_back(SPrintf_(" * Present:           %.3f ms", (double)mRenderPresentTime * 1000.0));
        mStatsLines.push_back(SPrintf_(" * Worker Wait:       %.3f ms", (double)mRenderWorkerWaitTime * 1000.0));
        mStatsLines.push_back(SPrintf_(" * Render Prepared/Culled: %u / %u",
            render->mLastPreparedCount, render->mLastCulledCount));
        mStatsLines.push_back(SPrintf_(" * Immediate Culled: %u", render->mImmediateCulledCommands));
        mStatsLines.push_back(SPrintf_(" * Render selector:  %s (%s)",
            RenderDiagnostics::Name(mCfg.renderer.parallelSelection), RenderDiagnostics::Name(mLastRenderPath)));
        mStatsLines.push_back(SPrintf_(" * Work units prev/current: %llu / %llu",
            static_cast<unsigned long long>(mLastRenderWeightedPrevious),
            static_cast<unsigned long long>(render->mPublicWorldWorkUnitsThisFrame)));
        mStatsLines.push_back(SPrintf_(" * Eligible (prev/current): %u / %u",
            mLastRenderEligiblePrevious, render->mPublicWorldCommandsThisFrame));
        mStatsLines.push_back(SPrintf_(" * SDL calls: bg=%u world=%u debug=%u UI=%u",
            render->mBackgroundDraws, render->mWorldDraws,
            render->mDebugDraws, render->mUiDraws));
        if (mCfg.profileCsv)
            mStatsLines.push_back(SPrintf_(" * Sprite SDL submit: %.3f ms", (double)render->mSpriteSubmitMs));
        mStatsLines.push_back(SPrintf_(" * SDL tint: skipped=%u changed=%u",
            render->mTintCacheHits, render->mTintUpdates));
        mStatsLines.push_back(SPrintf_(" * BG / World / Debug / UI: %.3f / %.3f / %.3f / %.3f ms",
            (double)render->mBackgroundMs, (double)mRenderWorldTime * 1000.,
            (double)render->mDebugMs, (double)mRenderUiTime * 1000.));
        mStatsLines.push_back(SPrintf_(" * Frame Wall Time:   %.3f ms", (double)mFrameTime * 1000.0));
        if (mFrameHistoryCount)
        {
            // Sort once per overlay refresh (4 Hz), not on every frame.
            std::vector<float> samples(mFrameHistory.begin(),
                                       mFrameHistory.begin() + mFrameHistoryCount);
            std::sort(samples.begin(), samples.end());
            const auto percentile = [&samples](float q) -> float
            {
                const std::size_t idx = static_cast<std::size_t>(
                    q * static_cast<float>(samples.size() - 1));
                return samples[idx] * 1000.0f;
            };
            mStatsLines.push_back(SPrintf_(" * Frame p50 / p95:  %.3f / %.3f ms",
                 (double)percentile(0.50f), (double)percentile(0.95f)));
        }
        mStatsLines.push_back(SPrintf_(" * Coll. Snapshot:    %.3f ms", (double)collision->mFrameSnapshotMs));
        mStatsLines.push_back(SPrintf_(" * Coll. Candidates:  %.3f ms", (double)collision->mFrameCandidateMs));
        mStatsLines.push_back(SPrintf_(" * Coll. Merge:       %.3f ms", (double)collision->mFrameMergeMs));
        mStatsLines.push_back(SPrintf_(" * Grid Build:        %.3f ms (%s)",
            (double)collision->mFrameGridBuildMs,
            collision->mAutoBroadphase ? "auto" : (collision->mSpatialBroadphase ? "grid" : "bruteforce")));
        mStatsLines.push_back(SPrintf_(" * Auto grid / brute passes: %u / %u",
            collision->mFrameGridPasses, collision->mFrameBruteforcePasses));
        mStatsLines.push_back(SPrintf_(" * Coll. passes / parallel: %u / %u",
            collision->mFrameCollisionPasses, collision->mFrameParallelPasses));
        mStatsLines.push_back(SPrintf_(" * Collision Workers: %u", collision->ParallelWorkerCount()));
        mStatsLines.push_back("");


        mStatsLines.push_back("=== Renderer Stats ===");
        mStatsLines.push_back(SPrintf_("Resolution:                      %u X %u px", (unsigned)res.x, (unsigned)res.y));
        mStatsLines.push_back(SPrintf_("Aspect Ratio:                    %u : %u", (unsigned)aspectAsFr.x, (unsigned)aspectAsFr.y));
        mStatsLines.push_back("Draw Calls (per frame):");
        mStatsLines.push_back(SPrintf_(" * World Draw Calls:             %u calls", render->mWorldDraws));
        mStatsLines.push_back(SPrintf_(" * Background Draw Calls:        %u calls", render->mBackgroundDraws));
        mStatsLines.push_back(SPrintf_(" * UI Draw Calls:                %u calls", render->mUiDraws));
        mStatsLines.push_back(SPrintf_(" * Debug Draw Calls:             %u calls", render->mDebugDraws));
        mStatsLines.push_back(SPrintf_("Sprites Rendered (per frame):    %d sprites", render->nRenderedSpritesThisFrame));
        mStatsLines.push_back("");


        mStatsLines.push_back("=== Scene Stats ===");
        mStatsLines.push_back(SPrintf_("Current Scene ID:               \"%s\"", scenes->mActiveId.c_str()));
        mStatsLines.push_back(SPrintf_("Game Objects In Scene:          %" PRIu64 " objects", (std::uint64_t)scenes->GetActive()->mEntities.size()));
        mStatsLines.push_back(SPrintf_("GOs Instantiated This Frame:    %" PRIu64 " objects", (std::uint64_t)scenes->GetActive()->mNew.size()));
        mStatsLines.push_back(SPrintf_("GOs Destroyed This Frame:       %" PRIu64 " objects", (std::uint64_t)scenes->GetActive()->mDestroyQueue.size()));
        mStatsLines.push_back("");


        mStatsLines.push_back("=== Collision Stats ===");
        mStatsLines.push_back(SPrintf_("Registered Colliders:              %" PRIu64 " colliders", (std::uint64_t)collision->mRegisteredColliders.size()));
        mStatsLines.push_back(SPrintf_("Broadphase Checks (per frame):     %" PRIu64 " checks", collision->mFrameBroadphaseTests));
        mStatsLines.push_back(SPrintf_("Narrowphase Checks (per frame):    %" PRIu64 " checks", collision->mFrameNarrowphaseTests));
        mStatsLines.push_back(SPrintf_("Contacts Built This Frame:         %" PRIu64 " contacts", collision->mFrameContactsBuilt));
        mStatsLines.push_back(SPrintf_("Active Pairs:                      %" PRIu64 " pairs", (std::uint64_t)collision->mCurrInfo.size()));
        mStatsLines.push_back("");


        mStatsLines.push_back("=== Assets Stats ===");
        mStatsLines.push_back(SPrintf_("Textures Loaded:                %" PRIu64 " textures", (std::uint64_t)assets->mTextures.size()));
        mStatsLines.push_back(SPrintf_("Fonts Loaded:                   %" PRIu64 " fonts", (std::uint64_t)assets->mFonts.size()));
        mStatsLines.push_back(SPrintf_("SFX Loaded:                     %" PRIu64 " SFX", (std::uint64_t)assets->mSfx.size()));
        mStatsLines.push_back(SPrintf_("Musics Loaded:                  %" PRIu64 " musics", (std::uint64_t)assets->mMusic.size()));
        mStatsLines.push_back(SPrintf_("Total Memory Used By Assets:    %.2f MB", (double)assets->memoryUsed));
        mStatsLines.push_back("");


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
        mStatsLines.push_back(SPrintf_("Physics Step Time:                %.6f sec", mFramePhysicsStepSec));
        mStatsLines.push_back(SPrintf_(" * Integrate Time:                %.6f sec (%.1f%%)", mFramePhysicsIntegrateSec, SafePct_(mFramePhysicsIntegrateSec, mFramePhysicsStepSec)));
        mStatsLines.push_back(SPrintf_(" * BuildContacts Time:            %.6f sec (%.1f%%)", mFramePhysicsBuildContactsSec, SafePct_(mFramePhysicsBuildContactsSec, mFramePhysicsStepSec)));
        mStatsLines.push_back(SPrintf_(" * Solve Time:                    %.6f sec (%.1f%%)", mFramePhysicsSolveSec, SafePct_(mFramePhysicsSolveSec, mFramePhysicsStepSec)));
        mStatsLines.push_back(SPrintf_("Registered Bodies:                %" PRIu64 " bodies", bodiesRegistered));
        mStatsLines.push_back(SPrintf_("Active Bodies:                    %" PRIu64 " bodies", bodiesActive));
        mStatsLines.push_back(SPrintf_("Body Types:                       dyn %" PRIu64 " / kin %" PRIu64 " / static %" PRIu64, bodiesDynamic, bodiesKinematic, bodiesStatic));
        mStatsLines.push_back(SPrintf_("CCD Enabled (Dynamic):            %" PRIu64 " bodies", bodiesCCD));
        mStatsLines.push_back(SPrintf_("Gravity:                          [%.3f, %.3f] m/s^2", (double)physics->gravity.x, (double)physics->gravity.y));
        mStatsLines.push_back(SPrintf_("Max Substeps:                     %d substeps", physics->maxSubsteps));
        mStatsLines.push_back(SPrintf_("CCD Min Size Factor:              %.3f units", (double)physics->ccdMinSizeFactor));
        mStatsLines.push_back(SPrintf_("Substeps This Frame:              %u substeps", mFramePhysicsSubsteps));
        mStatsLines.push_back(SPrintf_("Solver Iterations:                %d interactions", physics->solverIterations));
        mStatsLines.push_back(SPrintf_("Contacts Processed This Frame:    %" PRIu64 " contacts", mFramePhysicsContactsProcessed));

    }

    Font* consoleFontBold = assets->GetFontByKey("xengine_profiler_bold_14");
    Font* consoleFontNormal = assets->GetFontByKey("xengine_profiler_14");
    if (!consoleFontBold) consoleFontBold = assets->GetEngineDefaultFont(true);
    if (!consoleFontNormal) consoleFontNormal = assets->GetEngineDefaultFont(false);
    if (!consoleFontBold || !consoleFontNormal || !consoleFontNormal->GetSDL() ||
        !consoleFontBold->GetSDL() || viewport.x < 90 || viewport.y < 100) return;

    const auto textWidth = [](TTF_Font* font, const std::string& text) noexcept -> float
    {
        if (!font || text.empty()) return 0.f;
        int width = 0;
        int height = 0;
        return TTF_SizeUTF8(font, text.c_str(), &width, &height) == 0
            ? static_cast<float>(width) : 0.f;
    };
    const auto trim = [](const std::string& text) -> std::string
    {
        constexpr const char* white = " \t\r\n";
        const std::size_t first = text.find_first_not_of(white);
        if (first == std::string::npos) return {};
        const std::size_t last = text.find_last_not_of(white);
        return text.substr(first, last - first + 1);
    };
    const auto fit = [&textWidth](const std::string& text, TTF_Font* font, float width) -> std::string
    {
        if (width <= 0.f) return {};
        if (textWidth(font, text) <= width) return text;
        const std::string suffix = "...";
        if (textWidth(font, suffix) > width) return {};
        std::string clipped = text;
        while (!clipped.empty())
        {
            // Remove one complete UTF-8 codepoint, not just the final byte.
            std::size_t cut = clipped.size() - 1;
            while (cut > 0 &&
                (static_cast<unsigned char>(clipped[cut]) & 0xC0u) == 0x80u)
                --cut;
            clipped.resize(cut);
            if (textWidth(font, clipped + suffix) <= width)
                return clipped + suffix;
        }
        return suffix;
    };

    constexpr float margin = 9.f;
    constexpr float horizontalPadding = 9.f;
    constexpr float valueGap = 10.f;
    constexpr float headerHeight = 32.f;
    constexpr float footerHeight = 26.f;
    constexpr float columnGap = 12.f;
    const float lineHeight = static_cast<float>(std::max(
        TTF_FontHeight(consoleFontNormal->GetSDL()),
        TTF_FontHeight(consoleFontBold->GetSDL()))) + 1.f;

    if (refreshed || mStatsViewport != viewport || mStatsLayout.pages.empty() ||
        mStatsLineHeight != lineHeight)
    {
        mStatsViewport = viewport;
        mStatsLineHeight = lineHeight;
        mStatsDisplayRows.clear();
        std::vector<std::vector<std::size_t>> blocks;
        std::vector<std::size_t> currentBlock;
        float desiredWidth = 0.f;

        for (const auto& raw : mStatsLines)
        {
            const std::string line = trim(raw);
            if (line.empty())
            {
                if (!currentBlock.empty())
                {
                    blocks.emplace_back(std::move(currentBlock));
                    currentBlock.clear();
                }
                continue;
            }

            const bool heading = currentBlock.empty();
            std::string left = line;
            std::string right;
            if (!heading)
            {
                const std::size_t separator = line.find(':');
                if (separator != std::string::npos)
                {
                    left = trim(line.substr(0, separator));
                    right = trim(line.substr(separator + 1));
                }
            }
            else if (left.size() >= 6 && left.compare(0, 3, "===") == 0 &&
                     left.compare(left.size() - 3, 3, "===") == 0)
            {
                left = trim(left.substr(3, left.size() - 6));
            }

            const float labelWidth = textWidth(heading ? consoleFontBold->GetSDL() :
                consoleFontNormal->GetSDL(), left);
            const float rightWidth = textWidth(consoleFontNormal->GetSDL(), right);
            desiredWidth = std::max(desiredWidth, labelWidth + rightWidth +
                (right.empty() ? 0.f : valueGap) + 2.f * horizontalPadding);

            currentBlock.push_back(mStatsDisplayRows.size());
            mStatsDisplayRows.push_back({std::move(left), std::move(right), heading});
        }
        if (!currentBlock.empty()) blocks.emplace_back(std::move(currentBlock));

        mStatsLayout = XEngineStatsOverlay::Pack(blocks,
            static_cast<float>(viewport.x), static_cast<float>(viewport.y),
            std::max(260.f, desiredWidth), lineHeight, margin, headerHeight,
            footerHeight, columnGap);
        mStatsColumnWidth = mStatsLayout.columnWidth;
        // Cache fitted strings: the F3 overlay must not perform expensive
        // font measurements every rendered frame (only at 4 Hz / on resize).
        const float maxTextWidth = std::max(0.f,
            mStatsColumnWidth - 2.f * horizontalPadding);
        for (auto& row : mStatsDisplayRows)
        {
            row.visibleValue = fit(row.value, consoleFontNormal->GetSDL(), maxTextWidth * 0.55f);
            row.visibleValueWidth = textWidth(consoleFontNormal->GetSDL(), row.visibleValue);
            const float remaining = maxTextWidth - (row.visibleValue.empty()
                ? 0.f : row.visibleValueWidth + valueGap);
            row.visibleLabel = fit(row.label,
                row.heading ? consoleFontBold->GetSDL() : consoleFontNormal->GetSDL(), remaining);
        }
        if (mStatsPage >= mStatsLayout.pages.size()) mStatsPage = 0;
    }

    if (mStatsLayout.pages.empty()) return;
    const auto& page = mStatsLayout.pages[mStatsPage];
    const float panelW = std::min(static_cast<float>(viewport.x) - margin,
        margin + static_cast<float>(std::max(1u, page.usedColumns)) * mStatsColumnWidth +
        static_cast<float>(std::max(1u, page.usedColumns) - 1u) * columnGap + margin);
    const float panelH = std::min(static_cast<float>(viewport.y) - margin,
        std::max(headerHeight + lineHeight + 18.f, page.usedHeight + footerHeight));

    // Background is submitted before the UI text is flushed by UIManager::End().
    render->DrawRectScreen(Rect(6.f, 6.f, panelW - 6.f, panelH - 6.f),
                           Color::Black(220), true);
    render->DrawRectScreen(Rect(6.f, 6.f, panelW - 6.f, panelH - 6.f),
                           Color::White(110), false);

    const std::string heading = SPrintf_("XEngine Profiler  |  Page %zu/%zu",
        mStatsPage + 1, mStatsLayout.pages.size());
    ui->Label(fit(heading, consoleFontBold->GetSDL(), panelW - 2.f * horizontalPadding),
              margin + horizontalPadding, margin + 2.f,
              *consoleFontBold, Color::White());

    for (const auto& pos : page.lines)
    {
        if (pos.line >= mStatsDisplayRows.size()) continue;
        const auto& line = mStatsDisplayRows[pos.line];
        const float x = pos.x + horizontalPadding;
        const Color labelColor = line.heading ? Color::White() : Color::Green();
        ui->Label(line.visibleLabel, x, pos.y,
                  line.heading ? *consoleFontBold : *consoleFontNormal, labelColor);
        if (!line.visibleValue.empty())
        {
            ui->Label(line.visibleValue,
                      pos.x + mStatsColumnWidth - horizontalPadding - line.visibleValueWidth,
                      pos.y, *consoleFontNormal, Color::White());
        }
    }

    const std::string footer = mStatsLayout.pages.size() > 1
        ? "F3: close  |  F4: next page"
        : "F3: close";
    ui->Label(fit(footer, consoleFontNormal->GetSDL(), panelW - 2.f * horizontalPadding),
        margin + horizontalPadding, panelH - lineHeight - 6.f,
        *consoleFontNormal, Color::White());
}

#else
void Engine::DrawStatsOverlay_() noexcept {}
#endif

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

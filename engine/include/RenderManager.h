#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include "ParallelExecutor.h"
#include "RenderDiagnostics.h"
#include "SpriteProjection.h"
#include "RenderPreparation.h"

#include "SDL.h"

#include "Singleton.h"
#include "BaseTypes.h"
#include "Assets.h"
#include "Engine.h"

// Forward declarations
class Camera2D;
class WindowManager;
class Collider2D;
class SpriteRenderer;
class UIManager;
class AssetManager;
struct SDL_Renderer;
struct SDL_FRect;

class RenderManager : public Singleton<RenderManager>
{
    friend class Singleton<RenderManager>;
    friend class Scene;
    friend class Collider2D;
	friend class SpriteRenderer;
	friend class UIManager;
	friend class AssetManager;
    friend class Font;  // Invalidate cached text before its TTF_Font is destroyed.
    friend class Texture; // Invalidate tint state before a texture is destroyed.
    friend class Engine;

private:
    WindowManager* mWindow = nullptr;
    const Camera2D* mActiveCam = nullptr;
    int mWinW = 0, mWinH = 0;
    bool mVSync = true;
    bool mAccelerated = true;

	float renderBeginTime = 0.f;
    float renderTime = 0.f;
	int nDrawCallsThisFrame = 0;
	int nUIDrawCallsThisFrame = 0;
	int nDebugDrawCallsThisFrame = 0;
    int nRenderedSpritesThisFrame = 0;

    RenderManager() = default;
    ~RenderManager() = default;
    RenderManager(const RenderManager&) = delete;
    RenderManager& operator=(const RenderManager&) = delete;

    struct DebugCmd {
        enum class Type { Line, RectI } type;
        Rect rect;
        float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        Color color;
        bool filled = false;
        float rotationDeg = 0.f;
    };

    std::vector<DebugCmd> mDebugCmds;

    struct Config {
        bool accelerated = true;
        bool vsync = true;
        Color bgColor = Color::White();
    };

    std::vector<SDL_Texture*> mTempTextTextures; // destroy at the end of the frame
    std::vector<SDL_Surface*> mTempTextSurfaces; // destroy at the end of the frame

    Snapshot* snapRecord = nullptr;
    Snapshot mReusableSnapshot;
    float mLastPrepareMs = 0.f;
    float mLastWaitMs = 0.f;
    unsigned mLastPreparedCount = 0;
    unsigned mLastCulledCount = 0;
    unsigned mLastWorkers = 0;
    unsigned mLastAttemptedWorldCommands = 0;
    unsigned mLastRecordedCommands = 0, mImmediateCulledCommands = 0;
    unsigned mPublicWorldCommandsThisFrame = 0;
    std::uint64_t mLastWorldWorkUnits = 0;
    std::uint64_t mPublicWorldWorkUnitsThisFrame = 0;
    std::uint64_t mLastEffectiveWorldWorkUnits = 0;
    std::uint64_t mPublicEffectiveWorldWorkUnitsThisFrame = 0;
    unsigned mEstimatedOffscreenSprites = 0;
    RenderDiagnostics::WorkEstimate mWorkEstimateMode = RenderDiagnostics::WorkEstimate::Weighted;
    RenderDiagnostics::View mSelectorView{};
    std::uint64_t mSpriteWorkOverride = 0;
    std::uint64_t mSpriteEffectiveWorkOverride = 0;
    enum class DrawPhase : std::uint8_t { World, Background, Debug, UI };
    DrawPhase mDrawPhase = DrawPhase::World;
    unsigned mBackgroundDraws = 0, mWorldDraws = 0;
    unsigned mDebugDraws = 0, mUiDraws = 0;
    unsigned mTintCacheHits = 0, mTintUpdates = 0;
    float mBackgroundMs = 0.f, mDebugMs = 0.f;
    float mSpriteSubmitMs = 0.f; // world textures only, measured when CSV profiling is enabled
    bool mMeasureSpriteSubmissions = false;
    // Comparison switch: both variants record commands; only workers mode
    // defers sprite XYZ projection until CPU geometry preparation.
    bool mDeferSpriteProjection = true;
    std::unordered_map<SDL_Texture*, Color> mTextureTintCache;
    void ApplyTextureTint_(SDL_Texture* texture, Color color) noexcept;
    void InvalidateTextureTint_(SDL_Texture* texture) noexcept;
    void CountSubmission_() noexcept;
    void PrepareSnapshot(Snapshot& snapshot, ParallelExecutor* workers,
                         std::size_t minCommands) noexcept;

    struct TextCacheEntry
    {
        SDL_Texture* texture = nullptr;
        int width = 0, height = 0;
        std::uint64_t lastUsed = 0;
    };
    std::unordered_map<std::string, TextCacheEntry> mTextCache;
    std::uint64_t mTextCacheFrame = 0;
    void ClearTextCache_() noexcept;

    void RenderSnapshot(const Snapshot& snap) noexcept;

    void BeginRecord(const Camera2D* cam, Snapshot& out) noexcept;

    void EndRecord() noexcept;

public:
    enum class BackgroundMode : uint8_t
    {
        None = 0,
        Stretch,
        Fit,
        Fill,
        Center,
        Repeat,
        RepeatScaled,
        TileWorldPhysical,
        TileWorldPhysicalScaled
    };

    struct BackgroundLayer
    {
        Texture* tex = nullptr;
        BackgroundMode mode = BackgroundMode::Stretch;

        int order = 0;
        float parallax = 0.f;
        bool parallaxY = true;

        Vec2 offsetPx{ 0.f, 0.f };

        Vec2 scale{ 1.f, 1.f };
        Color tint{ 255,255,255,255 };
        bool flipX = false;
        bool flipY = false;

        bool seamSafe = true;

        Vec2 worldOrigin{ 0.f, 0.f };
        Rect worldBounds{ 0.f,0.f,0.f,0.f };
        bool useWorldBounds = false;
    };

private:
    BackgroundMode mBgMode = BackgroundMode::None;
    Texture* mBackgroundTexture = nullptr;
    Color mBackgroundTint = Color::White();
    Color mClearColor = Color::White();

    std::vector<BackgroundLayer> mBgLayers; // parallax layers (fall back to mBackgroundTexture if empty)

    Vec2 mPrevCamPos{ 0.f, 0.f };
    bool mHasPrevCam = false;

    // Helpers internos
    void DrawBackground_(const Camera2D& cam) noexcept;
    static Rect ComputeFitRect_(float winW, float winH, float texW, float texH) noexcept;
    static Rect ComputeFillRect_(float winW, float winH, float texW, float texH) noexcept;
    static inline float WrapOffset_(float x, float period) noexcept;
    Vec2 GetCamPos_(const Camera2D& cam) const noexcept;

    bool Init(const Config& cfg) noexcept;
    void Shutdown() noexcept;

    void Begin(const Camera2D& cam) noexcept;
    void End() noexcept;

    void FlushDebug() noexcept;

    SDL_Renderer* SDL() const noexcept { return mRenderer; }

    static bool Intersects(const Rect& a, const Rect& b) noexcept;
    SDL_Renderer* mRenderer = nullptr;

    SDL_Texture* mWhite = nullptr;  // 1x1 texture used to fill rectangles

    void SetVSync(bool enabled) noexcept;
    bool IsVSync() const noexcept { return mVSync; }

public:
    // ===============================
    // PUBLIC API (normal)
    // - Si mRecording == true -> encola comando en snapshot
    // - Si mRecording == false -> llama a Immediate_
    // ===============================

    // Diagnostic A/B switch. `true` (production default) offloads sprite XYZ
    // projection with render preparation; `false` projects on the main thread.
    // This is not an additional renderer configuration field.
    void SetDeferredSpriteProjection(bool enabled) noexcept {
        mDeferSpriteProjection = enabled;
    }

    // Debug
    void DrawDebugLine(float x1, float y1, float x2, float y2, Color color) noexcept;
    void DrawDebugRect(const Rect& worldRect, Color color, bool filled = false) noexcept;

    // World to screen (through the camera)
    void DrawTexture(const Texture& tex,
        const Rect& dstWorld,
        const Rect* srcPixels = nullptr,
        float rotationDeg = 0.f,
        Vec2 pivot01 = { 0.5f, 0.5f },
        Color tint = { 255,255,255,255 },
        bool flipX = false,
        bool flipY = false) noexcept;

    void DrawRect(const Rect& worldRect, Color color, bool filled) noexcept;
    void DrawRect(const Rect& worldRect,
        float rotationDeg,
        Vec2 pivot01,
        Color color,
        bool filled = true) noexcept;

    void DrawLine(float x1, float y1, float x2, float y2, Color color) noexcept;

    // Pantalla (screen-space)
    void DrawRectScreen(const Rect& screenRect, Color color, bool filled) noexcept;

    void DrawImageScreen(const Texture& tex,
        const Rect& dstScreen,
        const Rect* srcPixels = nullptr,
        Color tint = { 255,255,255,255 },
        bool flipX = false,
        bool flipY = false) noexcept;

    void DrawTextScreen(const Font& font,
        const std::string& text,
        float x_px, float y_px,
        Color color) noexcept;


private:
    // ===============================
    // PRIVATE / INTERNAL (immediate)
    // - SOLO SDL draw calls
    // - NUNCA graban comandos
    // ===============================

    // Called exclusively by SpriteRenderer; all live-scene reads happen before
    // this method, and worker preparation sees only SpriteProjection::Pose.
    void DrawSprite_(const Texture& tex, const SpriteProjection::Pose& pose,
                     const Rect& srcPx, Color tint, bool flipX, bool flipY) noexcept;

    void DrawTexturePrepared_(const DrawCmd& cmd) noexcept;

    void DrawTextureImmediate_(const Texture& tex,
        const Rect& dstWorld,
        const Rect* srcPixels,
        float rotationDeg,
        Vec2 pivot01,
        Color tint,
        bool flipX,
        bool flipY) noexcept;

    void DrawRectImmediate_(const Rect& worldRect,
        float rotationDeg,
        Vec2 pivot01,
        Color color,
        bool filled) noexcept;

    void DrawLineImmediate_(float x1, float y1, float x2, float y2, Color color) noexcept;

    void DrawRectScreenImmediate_(const Rect& screenRect, Color color, bool filled) noexcept;

    void DrawImageScreenImmediate_(const Texture& tex,
        const Rect& dstScreen,
        const Rect* srcPixels,
        Color tint,
        bool flipX,
        bool flipY) noexcept;

    void DrawTextScreenImmediate_(const Font& font,
        const std::string& text,
        float x_px, float y_px,
        Color color) noexcept;

	// Helpers internos
    static inline SDL_FRect WorldRectToScreen(const Camera2D& cam, 
        int winW, 
        int winH, 
        const Rect& wrect) noexcept;

public:
        void SetBackground(Texture* tex, BackgroundMode mode = BackgroundMode::Stretch, Color tint = { 255,255,255,255 }) noexcept;
        void ClearBackground() noexcept;

        void ClearParallax() noexcept;
        void AddParallaxLayer(const BackgroundLayer& layer) noexcept;
};

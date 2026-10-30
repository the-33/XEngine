#pragma once
/** @file RenderManager.h
 * @brief World-space and screen-space rendering and backgrounds.
 */

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include "XEngine/detail/ParallelExecutor.h"
#include "RenderDiagnostics.h"
#include "XEngine/detail/SpriteProjection.h"
#include "XEngine/detail/RenderPreparation.h"

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

/** @brief Submits world-space and screen-space draw commands to the SDL renderer.
 *
 * World commands use the active Camera2D; screen commands use drawable pixels
 * and bypass camera projection. During command recording, draws are enqueued
 * and later prepared/submitted; referenced Texture/Font assets must remain alive
 * through that frame.
 * @ingroup xengine_render
 */
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
    // Preserve the actually initialized SDL renderer through VSync recreation.
    int mPreferredRendererDriverIndex = -1;
    std::string mRequestedRendererBackend = "auto";
    std::string mFirstChoiceRendererBackend = "auto";
    struct BackendCandidate
    {
        int index = -1;
        std::string name;
    };
    std::vector<BackendCandidate> mRendererCandidates;
    bool ResolveRendererBackend_(const std::string& requested,
                                 bool allowFallback, bool strictHint) noexcept;
    bool CreateInitialRenderer_(std::uint32_t flags) noexcept;
    void CreateWhiteTexture_() noexcept;

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
        std::string backend = "auto";
        bool backendFallback = true;
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
    unsigned mSkippedVisibilitySprites = 0;
    RenderDiagnostics::WorkEstimate mWorkEstimateMode = RenderDiagnostics::WorkEstimate::Weighted;
    RenderDiagnostics::ViewBounds mSelectorViewBounds{};
    // Active only for actual automatic adaptive selection, not a forced
    // scoring-cost probe. The published effective count is then capped.
    bool mSaturateEffectiveWork = false;
    std::uint64_t mSelectorEntryBoundary = 0;
    std::uint64_t mSpriteWorkOverride = 0;
    std::uint64_t mSpriteEffectiveWorkOverride = 0;
    bool NeedsVisibilityEstimate_() const noexcept {
        return mWorkEstimateMode == RenderDiagnostics::WorkEstimate::Visibility &&
               (!mSaturateEffectiveWork ||
                mPublicEffectiveWorldWorkUnitsThisFrame < mSelectorEntryBoundary);
    }
    void AccumulateEffectiveWork_(std::uint64_t units) noexcept {
        if (mSaturateEffectiveWork)
            mPublicEffectiveWorldWorkUnitsThisFrame = RenderDiagnostics::CappedEffectiveSum(
                mPublicEffectiveWorldWorkUnitsThisFrame, units, mSelectorEntryBoundary);
        else
            mPublicEffectiveWorldWorkUnitsThisFrame += units;
    }
    enum class DrawPhase : std::uint8_t { World, Background, Debug, UI };
    DrawPhase mDrawPhase = DrawPhase::World;
    unsigned mBackgroundDraws = 0, mWorldDraws = 0;
    unsigned mDebugDraws = 0, mUiDraws = 0;
    unsigned mTintCacheHits = 0, mTintUpdates = 0;
    float mBackgroundMs = 0.f, mDebugMs = 0.f;
    float mSpriteSubmitMs = 0.f; // world textures only, measured when CSV profiling is enabled
    bool mMeasureSpriteSubmissions = false;
    // Exact SDL_RenderPresent duration; End() also includes texture cleanup.
    float mLastSdlPresentMs = 0.f;
    bool mMeasureSdlPresent = false;
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
    // One-shot read-only diagnostics for benchmarks. These are the SDL objects
    // actually created (including any fallback), not the requested JSON flags.
    /** @brief Read-only snapshot of the actual SDL renderer, window and driver.
     */
    struct BackendDiagnostics {
        /** @brief True if SDL returned renderer details successfully.
         */
        bool rendererInfoAvailable = false;
        /** @brief Name of the initialized SDL rendering backend.
         */
        std::string rendererName;
        /** @brief Name of the active SDL video driver.
         */
        std::string videoDriver;
        /** @brief SDL runtime version used for these diagnostics.
         */
        std::string sdlVersion;
        /** @brief Renderer-driver preference supplied to SDL. Configured SDL vertical-sync driver hint.
         */
        std::string renderDriverHint, renderVsyncHint;
        /** @brief SDL error string when renderer information could not be queried.
         */
        std::string rendererInfoError;
        /** @brief Backend capability and configuration flags.
         */
        std::uint32_t rendererFlags = 0;
        /** @brief Whether SDL reports a hardware-accelerated renderer. Whether the SDL renderer uses a software implementation.
         */
        bool accelerated = false, software = false;
        /** @brief Whether SDL reports synchronization with display refresh. Whether rendering to texture targets is supported.
         */
        bool vsync = false, targetTexture = false;
        /** @brief SDL flags attached to the active window.
         */
        std::uint32_t windowFlags = 0;
        /** @brief Reported width of the renderer output, in pixels. Reported height of renderer output, in pixels.
         */
        int rendererOutputWidth = 0, rendererOutputHeight = 0;
        /** @brief Maximum reported texture width supported by the renderer. Maximum reported texture height supported by the renderer.
         */
        int maxTextureWidth = 0, maxTextureHeight = 0;
        /** @brief Logical width of the active SDL window. Logical height of the active SDL window.
         */
        int windowWidth = 0, windowHeight = 0;
        /** @brief Index of the display containing the window. Refresh rate of the selected display, in hertz.
         */
        int displayIndex = -1, displayRefreshHz = 0;
    };

    /** @brief Queries the SDL backend capabilities and current renderer settings.
     */
    BackendDiagnostics QueryBackendDiagnostics() const;

    /** @brief Placement and tiling policy for background imagery.
     */
    enum class BackgroundMode : uint8_t
    {
        None = 0,              ///< Do not draw a background texture.
        Stretch,               ///< Stretch the image to exactly cover the window.
        Fit,                   ///< Preserve aspect ratio with the entire image visible; may leave bars.
        Fill,                  ///< Preserve aspect ratio and cover the window; may crop edges.
        Center,                ///< Draw at native size centered in the window.
        Repeat,                ///< Tile the texture in screen-space coordinates.
        RepeatScaled,          ///< Repeat tiles with the configured background scale.
        TileWorldPhysical,     ///< Tile across world space using physical texture dimensions.
        TileWorldPhysicalScaled ///< World-space tiling with an additional scale factor.
    };

    /** @brief One ordered, optionally parallax-scrolling background layer.
     */
    struct BackgroundLayer
    {
        /** @brief Texture displayed by this background layer.
         */
        Texture* tex = nullptr;
        /** @brief How the layer image is positioned, stretched or repeated.
         */
        BackgroundMode mode = BackgroundMode::Stretch;

        /** @brief Layer ordering relative to other backgrounds.
         */
        int order = 0;
        /** @brief Horizontal camera-motion parallax factor.
         */
        float parallax = 0.f;
        /** @brief Vertical camera-motion parallax factor.
         */
        bool parallaxY = true;

        /** @brief Additional image offset measured in pixels.
         */
        Vec2 offsetPx{ 0.f, 0.f };

        /** @brief Additional background image scale factor.
         */
        Vec2 scale{ 1.f, 1.f };
        /** @brief Color multiplier applied to the background image.
         */
        Color tint{ 255,255,255,255 };
        /** @brief Flip background image horizontally.
         */
        bool flipX = false;
        /** @brief Flip background image vertically.
         */
        bool flipY = false;

        /** @brief Apply tile placement designed to avoid visible seams.
         */
        bool seamSafe = true;

        /** @brief World-space origin used when anchoring this layer.
         */
        Vec2 worldOrigin{ 0.f, 0.f };
        /** @brief Optional world-space bounds for this background.
         */
        Rect worldBounds{ 0.f,0.f,0.f,0.f };
        /** @brief Whether to constrain the background to world bounds.
         */
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
    /** @brief Toggles CPU-side deferred sprite projection used for diagnostics, not a renderer backend.
     * @param enabled Whether to enable the named feature.
     */
    void SetDeferredSpriteProjection(bool enabled) noexcept {
        mDeferSpriteProjection = enabled;
    }

    // Debug
    /** @brief Queues a line in the renderer’s diagnostic overlay.
     * @param x1 First X coordinate.
     * @param y1 First Y coordinate.
     * @param x2 Second X coordinate.
     * @param y2 Second Y coordinate.
     * @param color RGBA drawing color.
     */
    void DrawDebugLine(float x1, float y1, float x2, float y2, Color color) noexcept;
    /** @brief Queues a rectangle in the renderer’s diagnostic overlay.
     * @param worldRect Rectangle in world coordinates.
     * @param color RGBA drawing color.
     * @param filled Whether the rectangle is filled.
     */
    void DrawDebugRect(const Rect& worldRect, Color color, bool filled = false) noexcept;

    // World to screen (through the camera)
    /** @brief Renders a texture in world coordinates, projected through the current camera.
     * @param tex Texture to draw or cache.
     * @param dstWorld Destination rectangle in world coordinates.
     * @param srcPixels Optional source rectangle in texture pixels; null uses the full texture.
     * @param rotationDeg Rotation angle in degrees.
     * @param pivot01 Normalized rotation pivot within the rectangle.
     * @param tint RGBA tint multiplier used for drawing.
     * @param flipX Whether to mirror the image horizontally.
     * @param flipY Whether to mirror the image vertically.
     */
    void DrawTexture(const Texture& tex,
        const Rect& dstWorld,
        const Rect* srcPixels = nullptr,
        float rotationDeg = 0.f,
        Vec2 pivot01 = { 0.5f, 0.5f },
        Color tint = { 255,255,255,255 },
        bool flipX = false,
        bool flipY = false) noexcept;

    /** @brief Draws an unrotated rectangle in world coordinates.
     * @param worldRect Rectangle in world coordinates.
     * @param color RGBA drawing color.
     * @param filled Whether the rectangle is filled.
     */
    void DrawRect(const Rect& worldRect, Color color, bool filled) noexcept;
    /** @brief Draws a world-space rectangle, optionally with rotation and pivot.
     * @param worldRect Rectangle in world coordinates.
     * @param rotationDeg Rotation angle in degrees.
     * @param pivot01 Normalized rotation pivot within the rectangle.
     * @param color RGBA drawing color.
     * @param filled Whether the rectangle is filled.
     */
    void DrawRect(const Rect& worldRect,
        float rotationDeg,
        Vec2 pivot01,
        Color color,
        bool filled = true) noexcept;

    /** @brief Draws a line between two world-space points through the camera.
     * @param x1 First X coordinate.
     * @param y1 First Y coordinate.
     * @param x2 Second X coordinate.
     * @param y2 Second Y coordinate.
     * @param color RGBA drawing color.
     */
    void DrawLine(float x1, float y1, float x2, float y2, Color color) noexcept;

    // Pantalla (screen-space)
    /** @brief Draws a screen-space rectangle without applying the camera transform.
     * @param screenRect Rectangle in screen coordinates.
     * @param color RGBA drawing color.
     * @param filled Whether the rectangle is filled.
     */
    void DrawRectScreen(const Rect& screenRect, Color color, bool filled) noexcept;

    /** @brief Draws a texture in screen pixels without applying the camera transform.
     * @param tex Texture to draw or cache.
     * @param dstScreen Destination rectangle in screen pixels.
     * @param srcPixels Optional source rectangle in texture pixels; null uses the full texture.
     * @param tint RGBA tint multiplier used for drawing.
     * @param flipX Whether to mirror the image horizontally.
     * @param flipY Whether to mirror the image vertically.
     */
    void DrawImageScreen(const Texture& tex,
        const Rect& dstScreen,
        const Rect* srcPixels = nullptr,
        Color tint = { 255,255,255,255 },
        bool flipX = false,
        bool flipY = false) noexcept;

    /** @brief Draws text in screen-pixel coordinates using the supplied font.
     * @param font Font used for text drawing.
     * @param text Text to render or serialize.
     * @param x_px Horizontal screen coordinate in pixels.
     * @param y_px Vertical screen coordinate in pixels.
     * @param color RGBA drawing color.
     * @param scale Positive size multiplier applied after font rasterization.
     */
    void DrawTextScreen(const Font& font,
        const std::string& text,
        float x_px, float y_px,
        Color color, float scale = 1.f) noexcept;


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
        Color color, float scale) noexcept;

	// Helpers internos
    static inline SDL_FRect WorldRectToScreen(const Camera2D& cam, 
        int winW, 
        int winH, 
        const Rect& wrect) noexcept;

public:
        /** @brief Sets a single background texture and its placement mode.
         * @param tex Non-owning texture pointer; keep the asset alive while used.
         * @param mode Screen- or world-space placement/tiling strategy.
         * @param tint RGBA tint multiplier used for drawing.
         */
        void SetBackground(Texture* tex, BackgroundMode mode = BackgroundMode::Stretch, Color tint = { 255,255,255,255 }) noexcept;
        /** @brief Clears the single background texture and disables its mode.
         */
        void ClearBackground() noexcept;

        /** @brief Removes all background parallax layers.
         */
        void ClearParallax() noexcept;
        /** @brief Appends a background layer that is sorted by its `order` when rendered.
         * @param layer Layer configuration copied into the manager.
         */
        void AddParallaxLayer(const BackgroundLayer& layer) noexcept;
};

/** @var std::string RenderManager::BackendDiagnostics::renderVsyncHint
 * @brief SDL render-VSync hint used when creating the renderer.
 */
/** @var bool RenderManager::BackendDiagnostics::software
 * @brief True when the active SDL renderer uses a software backend.
 */
/** @var bool RenderManager::BackendDiagnostics::targetTexture
 * @brief True if the renderer supports render-to-texture targets.
 */
/** @var int RenderManager::BackendDiagnostics::rendererOutputHeight
 * @brief Active renderer output height in drawable pixels.
 */
/** @var int RenderManager::BackendDiagnostics::maxTextureHeight
 * @brief Maximum texture height reported by SDL for the active backend.
 */
/** @var int RenderManager::BackendDiagnostics::windowHeight
 * @brief Actual logical SDL window height.
 */
/** @var int RenderManager::BackendDiagnostics::displayRefreshHz
 * @brief Display refresh frequency in hertz, when reported by the platform.
 */

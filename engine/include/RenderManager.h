/** @brief Grant Singleton< RenderManager > access to the internal state of RenderManager.
 */
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
    /** @brief Permit the singleton base to construct and release the RenderManager manager.
     */
    friend class Singleton<RenderManager>;
    /** @brief Grant Scene access to the internal state of RenderManager.
     */
    friend class Scene;
    /** @brief Grant Collider2D access to the internal state of RenderManager.
     */
    friend class Collider2D;
	/** @brief Grant SpriteRenderer access to the internal state of RenderManager.
	 */
	friend class SpriteRenderer;
	/** @brief Grant UIManager access to the internal state of RenderManager.
	 */
	friend class UIManager;
	/** @brief Grant AssetManager access to the internal state of RenderManager.
	 */
	friend class AssetManager;
    /** @brief Grant Font access to the internal state of RenderManager.
     */
    friend class Font;  // Invalidate cached text before its TTF_Font is destroyed.
    /** @brief Grant Texture access to the internal state of RenderManager.
     */
    friend class Texture; // Invalidate tint state before a texture is destroyed.
    /** @brief Grant Engine access to the internal state of RenderManager.
     */
    friend class Engine;

private:
    /** @brief Stored window value used by RenderManager.
     */
    WindowManager* mWindow = nullptr;
    /** @brief Stored active cam value used by RenderManager.
     */
    const Camera2D* mActiveCam = nullptr;
    /** @brief Stored win w value used by RenderManager.
     */
    int mWinW = 0, mWinH = 0;
    /** @brief Stored v sync value used by RenderManager.
     */
    bool mVSync = true;
    /** @brief Stored accelerated value used by RenderManager.
     */
    bool mAccelerated = true;
    // Preserve the actually initialized SDL renderer through VSync recreation.
    /** @brief Stored preferred renderer driver index value used by RenderManager.
     */
    int mPreferredRendererDriverIndex = -1;
    /** @brief Stored requested renderer backend value used by RenderManager.
     */
    std::string mRequestedRendererBackend = "auto";
    /** @brief Stored first choice renderer backend value used by RenderManager.
     */
    std::string mFirstChoiceRendererBackend = "auto";
    /** @brief Backend candidate data and utilities used by RenderManager.
     */
    struct BackendCandidate
    {
        /** @brief Configured index value used by BackendCandidate.
         */
        int index = -1;
        /** @brief Configured name value used by BackendCandidate.
         */
        std::string name;
    };
    /** @brief Stored renderer candidates value used by RenderManager.
     */
    std::vector<BackendCandidate> mRendererCandidates;
    /** @brief Retrieve the olve renderer backend from RenderManager.
     * @param requested Requested viewport dimensions in drawable pixels.
     * @param allowFallback Whether to allow fallback for this operation.
     * @param strictHint Strict hint supplied to RenderManager.
     * @return Requested resolve renderer backend value or reference.
     */
    bool ResolveRendererBackend_(const std::string& requested,
                                 bool allowFallback, bool strictHint) noexcept;
    /** @brief Create initial renderer in RenderManager.
     * @param flags Bitmask of options controlling the operation.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool CreateInitialRenderer_(std::uint32_t flags) noexcept;
    /** @brief Create white texture in RenderManager.
     */
    void CreateWhiteTexture_() noexcept;
    SDL_Texture* mRenderTarget = nullptr; ///< Optional low-resolution world render target.
    SDL_Texture* mPendingRenderTarget = nullptr; ///< Validated target waiting for frame completion.
    /** @brief Stored render width value used by RenderManager.
     */
    int mRenderWidth = 0, mRenderHeight = 0;
    /** @brief Stored pending width value used by RenderManager.
     */
    int mPendingWidth = 0, mPendingHeight = 0;
    /** @brief Stored has pending render target value used by RenderManager.
     */
    bool mHasPendingRenderTarget = false;
    /** @brief Stored frame open value used by RenderManager.
     */
    bool mFrameOpen = false;
    /** @brief Stored world target active value used by RenderManager.
     */
    bool mWorldTargetActive = false;
    /** @brief Commit render target in RenderManager.
     * @param target Target path or object for this operation.
     * @param width Requested width in the function's coordinate units.
     * @param height Requested height in the function's coordinate units.
     */
    void CommitRenderTarget_(SDL_Texture* target, int width, int height) noexcept;
    /** @brief Apply pending render target in RenderManager.
     */
    void ApplyPendingRenderTarget_() noexcept;
    /** @brief Presentation rect in RenderManager.
     * @return Result of presentation rect in RenderManager.
     */
    RectI PresentationRect_() const noexcept;
    /** @brief Begin screen pass in RenderManager.
     */
    void BeginScreenPass_() noexcept;
    /** @brief Draw deferred debug on screen in RenderManager.
     */
    void DrawDeferredDebugOnScreen_() noexcept;

	/** @brief Configured render begin time value used by RenderManager.
	 */
	float renderBeginTime = 0.f;
    /** @brief Configured render time value used by RenderManager.
     */
    float renderTime = 0.f;
	/** @brief Configured n draw calls this frame value used by RenderManager.
	 */
	int nDrawCallsThisFrame = 0;
	/** @brief Configured n ui draw calls this frame value used by RenderManager.
	 */
	int nUIDrawCallsThisFrame = 0;
	/** @brief Configured n debug draw calls this frame value used by RenderManager.
	 */
	int nDebugDrawCallsThisFrame = 0;
    /** @brief Configured n rendered sprites this frame value used by RenderManager.
     */
    int nRenderedSpritesThisFrame = 0;

    /** @brief Construct a RenderManager instance.
     */
    RenderManager() = default;
    /** @brief Destroy a RenderManager instance and release owned resources.
     */
    ~RenderManager() = default;
    /** @brief Construct a RenderManager instance.
     * @param other Source object whose copy, move or equality is being requested.
     */
    RenderManager(const RenderManager& other) = delete;
    /** @brief Implement the operator= operation for RenderManager.
     * @return Result of applying the overloaded operator.
     * @param other Source object whose copy, move or equality is being requested.
     */
    RenderManager& operator=(const RenderManager& other) = delete;

    /** @brief Debug cmd data and utilities used by RenderManager.
     */
    struct DebugCmd {
        /** @brief Available type values used by DebugCmd.
         */
        enum class Type {
            Line, ///< Debug line between world-space endpoints.
            RectI ///< Debug rectangle, optionally filled.
        } type;
        /** @brief Configured rect value used by DebugCmd.
         */
        Rect rect;
        /** @brief Configured x1 value used by DebugCmd.
         */
        float x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        /** @brief Configured color value used by DebugCmd.
         */
        Color color;
        /** @brief Configured filled value used by DebugCmd.
         */
        bool filled = false;
        /** @brief Configured rotation deg value used by DebugCmd.
         */
        float rotationDeg = 0.f;
    };

    /** @brief Stored debug cmds value used by RenderManager.
     */
    std::vector<DebugCmd> mDebugCmds;
    std::vector<DebugCmd> mDeferredDebugCmds; ///< World gizmos awaiting native-pixel output.

    /** @brief Internal configuration values for RenderManager.
     */
    struct Config {
        /** @brief Configured accelerated value used by Config.
         */
        bool accelerated = true;
        /** @brief Configured vsync value used by Config.
         */
        bool vsync = true;
        /** @brief Configured bg color value used by Config.
         */
        Color bgColor = Color::White();
        /** @brief Configured backend value used by Config.
         */
        std::string backend = "auto";
        /** @brief Configured backend fallback value used by Config.
         */
        bool backendFallback = true;
    };

    /** @brief Stored temp text textures value used by RenderManager.
     */
    std::vector<SDL_Texture*> mTempTextTextures; // destroy at the end of the frame
    /** @brief Stored temp text surfaces value used by RenderManager.
     */
    std::vector<SDL_Surface*> mTempTextSurfaces; // destroy at the end of the frame

    /** @brief Configured snap record value used by RenderManager.
     */
    Snapshot* snapRecord = nullptr;
    /** @brief Stored reusable snapshot value used by RenderManager.
     */
    Snapshot mReusableSnapshot;
    /** @brief Stored last prepare ms value used by RenderManager.
     */
    float mLastPrepareMs = 0.f;
    /** @brief Stored last wait ms value used by RenderManager.
     */
    float mLastWaitMs = 0.f;
    /** @brief Stored last prepared count value used by RenderManager.
     */
    unsigned mLastPreparedCount = 0;
    /** @brief Stored last culled count value used by RenderManager.
     */
    unsigned mLastCulledCount = 0;
    /** @brief Stored last workers value used by RenderManager.
     */
    unsigned mLastWorkers = 0;
    /** @brief Stored last attempted world commands value used by RenderManager.
     */
    unsigned mLastAttemptedWorldCommands = 0;
    /** @brief Stored last recorded commands value used by RenderManager.
     */
    unsigned mLastRecordedCommands = 0, mImmediateCulledCommands = 0;
    /** @brief Stored public world commands this frame value used by RenderManager.
     */
    unsigned mPublicWorldCommandsThisFrame = 0;
    /** @brief Stored last world work units value used by RenderManager.
     */
    std::uint64_t mLastWorldWorkUnits = 0;
    /** @brief Stored public world work units this frame value used by RenderManager.
     */
    std::uint64_t mPublicWorldWorkUnitsThisFrame = 0;
    /** @brief Stored last effective world work units value used by RenderManager.
     */
    std::uint64_t mLastEffectiveWorldWorkUnits = 0;
    /** @brief Stored public effective world work units this frame value used by RenderManager.
     */
    std::uint64_t mPublicEffectiveWorldWorkUnitsThisFrame = 0;
    /** @brief Stored estimated offscreen sprites value used by RenderManager.
     */
    unsigned mEstimatedOffscreenSprites = 0;
    /** @brief Stored skipped visibility sprites value used by RenderManager.
     */
    unsigned mSkippedVisibilitySprites = 0;
    /** @brief Stored work estimate mode value used by RenderManager.
     */
    RenderDiagnostics::WorkEstimate mWorkEstimateMode = RenderDiagnostics::WorkEstimate::Weighted;
    /** @brief Stored selector view bounds value used by RenderManager.
     */
    RenderDiagnostics::ViewBounds mSelectorViewBounds{};
    // Active only for actual automatic adaptive selection, not a forced
    // scoring-cost probe. The published effective count is then capped.
    /** @brief Stored saturate effective work value used by RenderManager.
     */
    bool mSaturateEffectiveWork = false;
    /** @brief Stored selector entry boundary value used by RenderManager.
     */
    std::uint64_t mSelectorEntryBoundary = 0;
    /** @brief Stored sprite work override value used by RenderManager.
     */
    std::uint64_t mSpriteWorkOverride = 0;
    /** @brief Stored sprite effective work override value used by RenderManager.
     */
    std::uint64_t mSpriteEffectiveWorkOverride = 0;
    /** @brief Needs visibility estimate in RenderManager.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool NeedsVisibilityEstimate_() const noexcept {
        return mWorkEstimateMode == RenderDiagnostics::WorkEstimate::Visibility &&
               (!mSaturateEffectiveWork ||
                mPublicEffectiveWorldWorkUnitsThisFrame < mSelectorEntryBoundary);
    }
    /** @brief Accumulate effective work in RenderManager.
     * @param units Units supplied to RenderManager.
     */
    void AccumulateEffectiveWork_(std::uint64_t units) noexcept {
        if (mSaturateEffectiveWork)
            mPublicEffectiveWorldWorkUnitsThisFrame = RenderDiagnostics::CappedEffectiveSum(
                mPublicEffectiveWorldWorkUnitsThisFrame, units, mSelectorEntryBoundary);
        else
            mPublicEffectiveWorldWorkUnitsThisFrame += units;
    }
    /** @brief Available draw phase values used by RenderManager.
     */
    enum class DrawPhase : std::uint8_t {
        World,      ///< Normal world rendering stage.
        Background, ///< Background draw commands.
        Debug,      ///< Collider and diagnostic overlay commands.
        UI          ///< Native-resolution user interface commands.
    };
    /** @brief Stored draw phase value used by RenderManager.
     */
    DrawPhase mDrawPhase = DrawPhase::World;
    /** @brief Stored background draws value used by RenderManager.
     */
    unsigned mBackgroundDraws = 0, mWorldDraws = 0;
    /** @brief Stored debug draws value used by RenderManager.
     */
    unsigned mDebugDraws = 0, mUiDraws = 0;
    /** @brief Stored tint cache hits value used by RenderManager.
     */
    unsigned mTintCacheHits = 0, mTintUpdates = 0;
    /** @brief Stored background ms value used by RenderManager.
     */
    float mBackgroundMs = 0.f, mDebugMs = 0.f;
    /** @brief Stored sprite submit ms value used by RenderManager.
     */
    float mSpriteSubmitMs = 0.f; // world textures only, measured when CSV profiling is enabled
    /** @brief Stored measure sprite submissions value used by RenderManager.
     */
    bool mMeasureSpriteSubmissions = false;
    // Exact SDL_RenderPresent duration; End() also includes texture cleanup.
    /** @brief Stored last sdl present ms value used by RenderManager.
     */
    float mLastSdlPresentMs = 0.f;
    /** @brief Stored measure sdl present value used by RenderManager.
     */
    bool mMeasureSdlPresent = false;
    // Comparison switch: both variants record commands; only workers mode
    // defers sprite XYZ projection until CPU geometry preparation.
    /** @brief Stored defer sprite projection value used by RenderManager.
     */
    bool mDeferSpriteProjection = true;
    /** @brief Stored texture tint cache value used by RenderManager.
     */
    std::unordered_map<SDL_Texture*, Color> mTextureTintCache;
    /** @brief Apply texture tint in RenderManager.
     * @param texture Texture asset to read, render or update.
     * @param color RGBA color used by the operation.
     */
    void ApplyTextureTint_(SDL_Texture* texture, Color color) noexcept;
    /** @brief Invalidate texture tint in RenderManager.
     * @param texture Texture asset to read, render or update.
     */
    void InvalidateTextureTint_(SDL_Texture* texture) noexcept;
    /** @brief Count submission in RenderManager.
     */
    void CountSubmission_() noexcept;
    /** @brief Prepare snapshot in RenderManager.
     * @param snapshot Snapshot supplied to RenderManager.
     * @param workers Workers supplied to RenderManager.
     * @param minCommands Min commands supplied to RenderManager.
     */
    void PrepareSnapshot(Snapshot& snapshot, ParallelExecutor* workers,
                         std::size_t minCommands) noexcept;

    /** @brief Text cache entry data and utilities used by RenderManager.
     */
    struct TextCacheEntry
    {
        /** @brief Configured texture value used by TextCacheEntry.
         */
        SDL_Texture* texture = nullptr;
        /** @brief Configured width value used by TextCacheEntry.
         */
        int width = 0, height = 0;
        /** @brief Configured last used value used by TextCacheEntry.
         */
        std::uint64_t lastUsed = 0;
    };
    /** @brief Stored text cache value used by RenderManager.
     */
    std::unordered_map<std::string, TextCacheEntry> mTextCache;
    std::unordered_map<std::string, TTF_Font*> mScaledFonts; ///< Screen-sized font variants.
    /** @brief Stored text cache frame value used by RenderManager.
     */
    std::uint64_t mTextCacheFrame = 0;
    /** @brief Clear text cache in RenderManager.
     */
    void ClearTextCache_() noexcept;
    /** @brief Font for screen scale in RenderManager.
     * @param font Font asset used to measure or render text.
     * @param scale Scaling factor applied to the coordinates or dimensions.
     * @return Result of font for screen scale in RenderManager.
     */
    TTF_Font* FontForScreenScale_(const Font& font, float scale) noexcept;

    /** @brief Render snapshot in RenderManager.
     * @param snap Snap supplied to RenderManager.
     */
    void RenderSnapshot(const Snapshot& snap) noexcept;

    /** @brief Begin record in RenderManager.
     * @param cam Cam supplied to RenderManager.
     * @param out Caller-provided output destination.
     */
    void BeginRecord(const Camera2D* cam, Snapshot& out) noexcept;

    /** @brief End record in RenderManager.
     */
    void EndRecord() noexcept;

public:
    /** @brief Changes the resolution used to draw the world, independently of the monitor.
     * The world is rendered to an offscreen texture and enlarged with nearest-neighbour
     * sampling to fit the real display, preserving aspect ratio with black bars.
     * UIManager controls are then rendered at the display's native resolution.
     * Unlike changing an exclusive fullscreen video mode, this works with any
     * valid render-target size supported by the current SDL renderer.
     * @param width Internal world-render width in pixels, or zero to disable.
     * @param height Internal world-render height in pixels, or zero to disable.
     * @return True if the request was accepted and the texture allocated;
     *         false if dimensions are invalid or the renderer cannot create it.
     * @note Pass (0, 0) to draw directly to the display again. Requests made
     *       during a frame take effect after its presentation, not mid-frame.
     */
    bool SetRenderResolution(int width, int height) noexcept;

    /** @brief Returns the world render size, or the display pixel size when unset.
     * @return Width and height in pixels used for world-space projection.
     */
    Vec2I GetRenderResolution() const noexcept;

    /** @brief Converts a display pixel into internal world-render coordinates.
     * Coordinates outside the letterboxed picture remain outside the render area.
     * @param displayPoint Pixel position in the SDL renderer's output.
     * @return The corresponding pixel position in the world render target.
     */
    Vec2 DisplayToRender(Vec2 displayPoint) const noexcept;

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
     * @return Result of query backend diagnostics in RenderManager.
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
    /** @brief Stored bg mode value used by RenderManager.
     */
    BackgroundMode mBgMode = BackgroundMode::None;
    /** @brief Stored background texture value used by RenderManager.
     */
    Texture* mBackgroundTexture = nullptr;
    /** @brief Stored background tint value used by RenderManager.
     */
    Color mBackgroundTint = Color::White();
    /** @brief Stored clear color value used by RenderManager.
     */
    Color mClearColor = Color::White();

    /** @brief Stored bg layers value used by RenderManager.
     */
    std::vector<BackgroundLayer> mBgLayers; // parallax layers (fall back to mBackgroundTexture if empty)

    /** @brief Stored prev cam pos value used by RenderManager.
     */
    Vec2 mPrevCamPos{ 0.f, 0.f };
    /** @brief Stored has prev cam value used by RenderManager.
     */
    bool mHasPrevCam = false;

    // Helpers internos
    /** @brief Draw background in RenderManager.
     * @param cam Cam supplied to RenderManager.
     */
    void DrawBackground_(const Camera2D& cam) noexcept;
    /** @brief Compute fit rect in RenderManager.
     * @param winW Win w supplied to RenderManager.
     * @param winH Win h supplied to RenderManager.
     * @param texW Tex w supplied to RenderManager.
     * @param texH Tex h supplied to RenderManager.
     * @return Result of compute fit rect in RenderManager.
     */
    static Rect ComputeFitRect_(float winW, float winH, float texW, float texH) noexcept;
    /** @brief Compute fill rect in RenderManager.
     * @param winW Win w supplied to RenderManager.
     * @param winH Win h supplied to RenderManager.
     * @param texW Tex w supplied to RenderManager.
     * @param texH Tex h supplied to RenderManager.
     * @return Result of compute fill rect in RenderManager.
     */
    static Rect ComputeFillRect_(float winW, float winH, float texW, float texH) noexcept;
    /** @brief Wrap offset in RenderManager.
     * @param x Horizontal coordinate or component.
     * @param period Period supplied to RenderManager.
     * @return Result of wrap offset in RenderManager.
     */
    static inline float WrapOffset_(float x, float period) noexcept;
    /** @brief Retrieve the cam pos from RenderManager.
     * @param cam Cam supplied to RenderManager.
     * @return Requested get cam pos value or reference.
     */
    Vec2 GetCamPos_(const Camera2D& cam) const noexcept;

    /** @brief Initialize the RenderManager subsystem.
     * @param cfg Cfg supplied to RenderManager.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Init(const Config& cfg) noexcept;
    /** @brief Release resources managed by RenderManager.
     */
    void Shutdown() noexcept;

    /** @brief Begin in RenderManager.
     * @param cam Cam supplied to RenderManager.
     */
    void Begin(const Camera2D& cam) noexcept;
    /** @brief End in RenderManager.
     */
    void End() noexcept;

    /** @brief Flush debug in RenderManager.
     */
    void FlushDebug() noexcept;

    /** @brief Sdl in RenderManager.
     * @return Result of sdl in RenderManager.
     */
    SDL_Renderer* SDL() const noexcept { return mRenderer; }

    /** @brief Intersects in RenderManager.
     * @param a First input operand.
     * @param b Second input operand.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    static bool Intersects(const Rect& a, const Rect& b) noexcept;
    /** @brief Stored renderer value used by RenderManager.
     */
    SDL_Renderer* mRenderer = nullptr;

    /** @brief Stored white value used by RenderManager.
     */
    SDL_Texture* mWhite = nullptr;  // 1x1 texture used to fill rectangles

    /** @brief Set v sync in RenderManager.
     * @param enabled Whether the corresponding feature should be active.
     */
    void SetVSync(bool enabled) noexcept;
    /** @brief Test whether RenderManager is v sync.
     * @return True when is v sync; otherwise false.
     */
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

    /** @brief Draw a filled or outlined rounded rectangle in screen pixels.
     * The radius is clamped to half the shortest side and the border stays
     * inside the supplied rectangle; zero radius produces square corners.
     * @param screenRect Destination rectangle in drawable pixels.
     * @param fill Interior color (ignored when filled is false).
     * @param outline Border color; alpha zero disables the border.
     * @param cornerRadius Corner radius in pixels.
     * @param borderWidth Border thickness in pixels.
     * @param filled Whether to draw the interior.
     */
    void DrawRoundedRectScreen(const Rect& screenRect, Color fill, Color outline,
                               float cornerRadius, float borderWidth = 1.f,
                               bool filled = true) noexcept;

    /** @brief Draw a screen-space line independent of the active camera.
     * @param x1 Starting X coordinate in drawable pixels.
     * @param y1 Starting Y coordinate in drawable pixels.
     * @param x2 Ending X coordinate in drawable pixels.
     * @param y2 Ending Y coordinate in drawable pixels.
     * @param color RGBA line color.
     * @param thickness Positive line width in drawable pixels.
     */
    void DrawLineScreen(float x1, float y1, float x2, float y2,
                        Color color, float thickness = 1.f) noexcept;

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
     * @param scale Positive size multiplier; glyphs are rasterized at their
     *              final size if the original font file is available.
     */
    void DrawTextScreen(const Font& font,
        const std::string& text,
        float x_px, float y_px,
        Color color, float scale = 1.f) noexcept;

    /** @brief Measures UTF-8 text using the same scaled font as DrawTextScreen.
     * @param font Source font loaded by AssetManager.
     * @param text UTF-8 text to measure.
     * @param scale Positive UI text scale.
     * @return The rendered width and height in physical output pixels.
     */
    Vec2I MeasureTextScreen(const Font& font, const std::string& text,
                            float scale = 1.f) noexcept;


private:
    // ===============================
    // PRIVATE / INTERNAL (immediate)
    // - SOLO SDL draw calls
    // - NUNCA graban comandos
    // ===============================

    // Called exclusively by SpriteRenderer; all live-scene reads happen before
    // this method, and worker preparation sees only SpriteProjection::Pose.
    /** @brief Draw sprite in RenderManager.
     * @param tex Texture asset to read or draw.
     * @param pose Pose supplied to RenderManager.
     * @param srcPx Src px supplied to RenderManager.
     * @param tint RGBA tint applied to the rendered sprite or text.
     * @param flipX Flip x supplied to RenderManager.
     * @param flipY Flip y supplied to RenderManager.
     */
    void DrawSprite_(const Texture& tex, const SpriteProjection::Pose& pose,
                     const Rect& srcPx, Color tint, bool flipX, bool flipY) noexcept;

    /** @brief Draw texture prepared in RenderManager.
     * @param cmd Cmd supplied to RenderManager.
     */
    void DrawTexturePrepared_(const DrawCmd& cmd) noexcept;

    /** @brief Draw texture immediate in RenderManager.
     * @param tex Texture asset to read or draw.
     * @param dstWorld Dst world supplied to RenderManager.
     * @param srcPixels Src pixels supplied to RenderManager.
     * @param rotationDeg Rotation deg supplied to RenderManager.
     * @param pivot01 Pivot01 supplied to RenderManager.
     * @param tint RGBA tint applied to the rendered sprite or text.
     * @param flipX Flip x supplied to RenderManager.
     * @param flipY Flip y supplied to RenderManager.
     */
    void DrawTextureImmediate_(const Texture& tex,
        const Rect& dstWorld,
        const Rect* srcPixels,
        float rotationDeg,
        Vec2 pivot01,
        Color tint,
        bool flipX,
        bool flipY) noexcept;

    /** @brief Draw rect immediate in RenderManager.
     * @param worldRect World rect supplied to RenderManager.
     * @param rotationDeg Rotation deg supplied to RenderManager.
     * @param pivot01 Pivot01 supplied to RenderManager.
     * @param color RGBA color used by the operation.
     * @param filled Filled supplied to RenderManager.
     */
    void DrawRectImmediate_(const Rect& worldRect,
        float rotationDeg,
        Vec2 pivot01,
        Color color,
        bool filled) noexcept;

    /** @brief Draw line immediate in RenderManager.
     * @param x1 X1 supplied to RenderManager.
     * @param y1 Y1 supplied to RenderManager.
     * @param x2 X2 supplied to RenderManager.
     * @param y2 Y2 supplied to RenderManager.
     * @param color RGBA color used by the operation.
     */
    void DrawLineImmediate_(float x1, float y1, float x2, float y2, Color color) noexcept;

    /** @brief Draw rect screen immediate in RenderManager.
     * @param screenRect Screen rect supplied to RenderManager.
     * @param color RGBA color used by the operation.
     * @param filled Filled supplied to RenderManager.
     */
    void DrawRectScreenImmediate_(const Rect& screenRect, Color color, bool filled) noexcept;

    /** @brief Draw rounded rect screen immediate in RenderManager.
     * @param screenRect Screen rect supplied to RenderManager.
     * @param fill Fill supplied to RenderManager.
     * @param outline Outline supplied to RenderManager.
     * @param cornerRadius Corner radius supplied to RenderManager.
     * @param borderWidth Border width supplied to RenderManager.
     * @param filled Filled supplied to RenderManager.
     */
    void DrawRoundedRectScreenImmediate_(const Rect& screenRect, Color fill,
        Color outline, float cornerRadius, float borderWidth, bool filled) noexcept;

    /** @brief Draw line screen immediate in RenderManager.
     * @param x1 X1 supplied to RenderManager.
     * @param y1 Y1 supplied to RenderManager.
     * @param x2 X2 supplied to RenderManager.
     * @param y2 Y2 supplied to RenderManager.
     * @param color RGBA color used by the operation.
     * @param thickness Thickness supplied to RenderManager.
     */
    void DrawLineScreenImmediate_(float x1, float y1, float x2, float y2,
        Color color, float thickness) noexcept;

    /** @brief Draw image screen immediate in RenderManager.
     * @param tex Texture asset to read or draw.
     * @param dstScreen Dst screen supplied to RenderManager.
     * @param srcPixels Src pixels supplied to RenderManager.
     * @param tint RGBA tint applied to the rendered sprite or text.
     * @param flipX Flip x supplied to RenderManager.
     * @param flipY Flip y supplied to RenderManager.
     */
    void DrawImageScreenImmediate_(const Texture& tex,
        const Rect& dstScreen,
        const Rect* srcPixels,
        Color tint,
        bool flipX,
        bool flipY) noexcept;

    /** @brief Draw text screen immediate in RenderManager.
     * @param font Font asset used to measure or render text.
     * @param text UTF-8 text supplied to the operation.
     * @param x_px X px supplied to RenderManager.
     * @param y_px Y px supplied to RenderManager.
     * @param color RGBA color used by the operation.
     * @param scale Scaling factor applied to the coordinates or dimensions.
     */
    void DrawTextScreenImmediate_(const Font& font,
        const std::string& text,
        float x_px, float y_px,
        Color color, float scale) noexcept;

	// Helpers internos
    /** @brief World rect to screen in RenderManager.
     * @param cam Cam supplied to RenderManager.
     * @param winW Win w supplied to RenderManager.
     * @param winH Win h supplied to RenderManager.
     * @param wrect Wrect supplied to RenderManager.
     * @return Result of world rect to screen in RenderManager.
     */
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

/** @name Co-declared engine member descriptions
 * @brief Members sharing declarations with other fields.
 * @{ */
/** @var enum RenderManager::DebugCmd::Type RenderManager::DebugCmd::type
 * @brief Type used by DebugCmd.
 */

/** @var float RenderManager::DebugCmd::y1
 * @brief Y1 coordinate of the line endpoint.
 */

/** @var float RenderManager::DebugCmd::x2
 * @brief X2 coordinate of the line endpoint.
 */

/** @var float RenderManager::DebugCmd::y2
 * @brief Y2 coordinate of the line endpoint.
 */

/** @var int RenderManager::mWinH
 * @brief Stored win h state owned by RenderManager.
 */

/** @var int RenderManager::mRenderHeight
 * @brief Stored render height state owned by RenderManager.
 */

/** @var int RenderManager::mPendingHeight
 * @brief Stored pending height state owned by RenderManager.
 */

/** @var unsigned RenderManager::mImmediateCulledCommands
 * @brief Stored immediate culled commands state owned by RenderManager.
 */

/** @var unsigned RenderManager::mWorldDraws
 * @brief Stored world draws state owned by RenderManager.
 */

/** @var unsigned RenderManager::mUiDraws
 * @brief Stored ui draws state owned by RenderManager.
 */

/** @var unsigned RenderManager::mTintUpdates
 * @brief Stored tint updates state owned by RenderManager.
 */

/** @var float RenderManager::mDebugMs
 * @brief Stored debug ms state owned by RenderManager.
 */

/** @var int RenderManager::TextCacheEntry::height
 * @brief Camera height in the projection coordinate space.
 */
/** @} */

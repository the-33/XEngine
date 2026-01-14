#pragma once

#include <vector>
#include <string>

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

class RenderManager : public Singleton<RenderManager>
{
    friend class Singleton<RenderManager>;
    friend class Scene;
    friend class Collider2D;
	friend class SpriteRenderer;
	friend class UIManager;
	friend class AssetManager;
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

    std::vector<BackgroundLayer> mBgLayers; // parallax layers (si está vacío usa mBackgroundTexture)

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

    SDL_Texture* mWhite = nullptr;  // 1x1 para rellenar rects

    void DrawDebugLine(float x1, float y1, float x2, float y2, Color color) noexcept;
    void DrawDebugRect(const Rect& worldRect, Color color, bool filled = false) noexcept;

    void SetVSync(bool enabled) noexcept;
    bool IsVSync() const noexcept { return mVSync; }

    // Dibujo en coordenadas de MUNDO (la cámara hace world->screen)
    void DrawTexture(const Texture& tex,
        const Rect& dstWorld,
        const Rect* srcPixels = nullptr,
        float rotationDeg = 0.f,
        Vec2 pivot01 = { 0.5f, 0.5f },
        Color tint = { 255,255,255,255 },
        bool flipX = false,
        bool flipY = false) noexcept;

    void DrawRect(const Rect& worldRect, const  Color color, const  bool filled) noexcept;
    void DrawRect(const Rect& worldRect, const  float rotationDeg, const  Vec2 pivot01, const  Color color, const  bool filled = true) noexcept;

    void DrawLine(float x1, float y1, float x2, float y2, Color color) noexcept;

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
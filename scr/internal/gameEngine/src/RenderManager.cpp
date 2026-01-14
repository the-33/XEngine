#include "RenderManager.h"

#include <cmath>

#include "SDL.h"
#include "WindowManager.h"
#include "Camera2D.h"
#include "ErrorHandler.h"
#include "TimeManager.h"

static inline bool SDL_Intersect(const SDL_Rect& a, const SDL_Rect& b, SDL_Rect& out) {
    const int x1 = std::max(a.x, b.x);
    const int y1 = std::max(a.y, b.y);
    const int x2 = std::min(a.x + a.w, b.x + b.w);
    const int y2 = std::min(a.y + a.h, b.y + b.h);
    out.x = x1; out.y = y1;
    out.w = std::max(0, x2 - x1);
    out.h = std::max(0, y2 - y1);
    return out.w > 0 && out.h > 0;
}

bool RenderManager::Init(const Config& cfg) noexcept
{
    mWindow = WindowManager::GetInstancePtr();

    if (mWindow == nullptr) return false;

    mAccelerated = cfg.accelerated;
    mVSync = cfg.vsync;
    mClearColor = cfg.bgColor;

    // Crear renderer de SDL
    Uint32 flags = 0;
    if (mAccelerated) flags |= SDL_RENDERER_ACCELERATED;
    if (mVSync)       flags |= SDL_RENDERER_PRESENTVSYNC;

    mRenderer = SDL_CreateRenderer(mWindow->SDL(), -1, flags);
    if (!mRenderer) {
        // Fallback sin aceleración/VSYNC
        mRenderer = SDL_CreateRenderer(mWindow->SDL(), -1, 0);
    }

    if (!mRenderer)
    {
        LogError("RenderManager::Init()", "SDL_CreateRenderer(mWindow->SDL(), -1, 0) failed.", SDL_GetError());
        return false;
    }

    // Mezcla alfa para DrawRect/DrawLine y texturas con alpha
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    const Vec2I drawable = mWindow->GetDrawableSize();
    mWinW = drawable.x; mWinH = drawable.y;

    // 1x1 blanco
    mWhite = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, 1, 1);
    if (mWhite) {
        SDL_SetTextureBlendMode(mWhite, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(mRenderer, mWhite);
        SDL_SetRenderDrawColor(mRenderer, 255, 255, 255, 255);
        SDL_RenderClear(mRenderer);
        SDL_SetRenderTarget(mRenderer, nullptr);
    }

    return true;
}

void RenderManager::Shutdown() noexcept
{
    if (mWhite) { SDL_DestroyTexture(mWhite); mWhite = nullptr; }
    if (mRenderer) { SDL_DestroyRenderer(mRenderer); mRenderer = nullptr; }
    mActiveCam = nullptr;
    mWindow = nullptr;
    mWinW = mWinH = 0;
}

void RenderManager::Begin(const Camera2D& cam) noexcept
{
    mActiveCam = &cam;

    if (mWindow) {
        const Vec2I drawable = mWindow->GetDrawableSize();
        mWinW = drawable.x; mWinH = drawable.y;
    }

    if (mRenderer) {
        SDL_SetRenderDrawColor(mRenderer, mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a);
        SDL_RenderClear(mRenderer);
    }

    DrawBackground_(cam);
}

void RenderManager::End() noexcept
{
    if (!mRenderer) return;

    // Delega el "present" en WindowManager para centralizar peculiaridades por backend
    if (mWindow) mWindow->Present(mRenderer);
    else         SDL_RenderPresent(mRenderer);

    mActiveCam = nullptr;
	renderTime = (float) TimeManager::GetInstancePtr()->timeSinceStart - renderBeginTime;
    renderBeginTime = (float) TimeManager::GetInstancePtr()->timeSinceStart;
}

void RenderManager::SetVSync(bool enabled) noexcept
{
    if (mVSync == enabled) return;
    mVSync = enabled;

    // SDL no permite cambiar VSYNC del renderer ya creado en todas las plataformas.
    // La opción robusta: recrear el renderer respetando aceleración. Basicamente seria hacer un shutdown y un innit con un nuevo valor para vsync
    if (!mWindow || !mRenderer) return;

    SDL_DestroyRenderer(mRenderer);
    mRenderer = nullptr;

    Uint32 flags = 0;
    if (mAccelerated) flags |= SDL_RENDERER_ACCELERATED;
    if (mVSync)       flags |= SDL_RENDERER_PRESENTVSYNC;

    mRenderer = SDL_CreateRenderer(mWindow->SDL(), -1, flags);
    if (!mRenderer) mRenderer = SDL_CreateRenderer(mWindow->SDL(), -1, 0);
    if (mRenderer) SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
}

inline SDL_FRect RenderManager::WorldRectToScreen(const Camera2D& cam, int winW, int winH, const Rect& wrect) noexcept
{
    Vec2 s1 = cam.WorldToScreen(wrect.x, wrect.y, winW, winH);
    Vec2 s2 = cam.WorldToScreen(wrect.x + wrect.w, wrect.y + wrect.h, winW, winH);

    SDL_FRect r;
    r.x = std::min(s1.x, s2.x);
    r.y = std::min(s1.y, s2.y);
    r.w = std::fabs(s2.x - s1.x);
    r.h = std::fabs(s2.y - s1.y);
    return r;
}

static inline Rect RotatedAABB_(const SDL_FRect& dst, const SDL_FPoint& center, float deg) noexcept
{
    const float rad = deg * 3.14159265f / 180.f;
    const float c = std::cos(rad), s = std::sin(rad);

    const float cx = dst.x + center.x;
    const float cy = dst.y + center.y;

    const float xs[4] = { -center.x,        dst.w - center.x, dst.w - center.x, -center.x };
    const float ys[4] = { -center.y,       -center.y,       dst.h - center.y,  dst.h - center.y };

    float minx = 1e30f, miny = 1e30f;
    float maxx = -1e30f, maxy = -1e30f;

    for (int i = 0; i < 4; ++i) {
        const float rx = xs[i] * c - ys[i] * s;
        const float ry = xs[i] * s + ys[i] * c;
        const float px = cx + rx;
        const float py = cy + ry;
        if (px < minx) minx = px; if (px > maxx) maxx = px;
        if (py < miny) miny = py; if (py > maxy) maxy = py;
    }
    return Rect{ minx, miny, maxx - minx, maxy - miny };
}

void RenderManager::DrawTexture(const Texture& tex,
    const Rect& dstWorld,
    const Rect* srcPixels,
    float rotationDeg,
    Vec2 pivot01,
    Color tint,
    bool flipX,
    bool flipY) noexcept
{
    if ((tint.r < 0 || tint.r > 255) || (tint.g < 0 || tint.g > 255) || (tint.b < 0 || tint.b > 255) || (tint.a < 0 || tint.a > 255))
    {
        LogError("RenderManager warning", "DrawTexture(): Tint color not valid.");
        return;
    }

    if (!mRenderer || !mActiveCam || tex.GetSDL() == nullptr) return;
    nDrawCallsThisFrame++;

    // 1) Mundo -> Pantalla
    SDL_FRect dst = WorldRectToScreen(*mActiveCam, mWinW, mWinH, dstWorld);

    // 2) Centro de rotación (pivot) en píxeles del rect en pantalla
    SDL_FPoint center{ dst.w * pivot01.x, dst.h * pivot01.y };

    // 3) Culling exacto del quad rotado usando el helper
    Rect aabbRot = RotatedAABB_(dst, center, rotationDeg);
    Rect viewport{ 0.f, 0.f, (float)mWinW, (float)mWinH };
    if (!Intersects(aabbRot, viewport)) return;

    // 4) Región fuente (en píxeles de la textura)
    SDL_Rect sdlSrc;
    const SDL_Rect* pSrc = nullptr;
    if (srcPixels) {
        sdlSrc.x = (int)srcPixels->x;
        sdlSrc.y = (int)srcPixels->y;
        sdlSrc.w = (int)(srcPixels->w == 0 ? tex.Width() : srcPixels->w);
        sdlSrc.h = (int)(srcPixels->h == 0 ? tex.Height() : srcPixels->h);
        pSrc = &sdlSrc;
    }

    // 5) Modulación de color/alpha
    SDL_SetTextureColorMod(tex.GetSDL(), tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(tex.GetSDL(), tint.a);

    // 6) Flip + Draw
    SDL_RendererFlip sdlFlip = SDL_FLIP_NONE;
    if (flipX) sdlFlip = (SDL_RendererFlip)(sdlFlip | SDL_FLIP_HORIZONTAL);
    if (flipY) sdlFlip = (SDL_RendererFlip)(sdlFlip | SDL_FLIP_VERTICAL);

    SDL_RenderCopyExF(mRenderer, tex.GetSDL(), pSrc, &dst, rotationDeg, &center, sdlFlip);
    nRenderedSpritesThisFrame++;
}

void RenderManager::DrawRect(const Rect& worldRect, const Color color, const bool filled) noexcept
{
    DrawRect(worldRect, /*rotationDeg=*/0.f, /*pivot01=*/{ 0.5f, 0.5f }, color, filled);
}

void RenderManager::DrawTextScreen(const Font& font, const std::string& text, float x, float y, Color color) noexcept
{
    if (!mRenderer || !font.GetSDL() || text.empty()) return;

    Vec2I wS = mWindow->GetDrawableSize();
    if (wS.x <= 0 || wS.y <= 0) return;

    int tw = 0, th = 0;
    if (TTF_SizeUTF8(font.GetSDL(), text.c_str(), &tw, &th) == 0) {
        if ((int)x >= wS.x || (int)y >= wS.y || (int)x + tw <= 0 || (int)y + th <= 0)
            return;
    }

    SDL_Surface* surf = TTF_RenderUTF8_Blended(font.GetSDL(), text.c_str(), SDL_Color{ color.r,color.g,color.b,color.a });
    if (!surf) return;

    SDL_Texture* tx = SDL_CreateTextureFromSurface(mRenderer, surf);
    SDL_Rect dst{ (int)x, (int)y, surf->w, surf->h };
    SDL_FreeSurface(surf);
    if (!tx) return;

    SDL_RenderCopy(mRenderer, tx, nullptr, &dst);
    SDL_DestroyTexture(tx);
    nUIDrawCallsThisFrame++;
}

void RenderManager::DrawRect(const Rect& worldRect,
    const float rotationDeg,
    const Vec2 pivot01,
    const Color color,
    const bool filled) noexcept
{
    if (!mRenderer || !mActiveCam) return;

    if ((color.r < 0 || color.r > 255) || (color.g < 0 || color.g > 255) || (color.b < 0 || color.b > 255) || (color.a < 0 || color.a > 255))
    {
        LogError("RenderManager warning", "DrawRect(): Color not valid.");
        return;
    }

    // 1) Mundo -> Pantalla (AABB sin rotar del rect)
    SDL_FRect dst = WorldRectToScreen(*mActiveCam, mWinW, mWinH, worldRect);

    // 2) Centro de rotación (pivot) en píxeles
    SDL_FPoint center{ dst.w * pivot01.x, dst.h * pivot01.y };

    // 3) Culling con AABB del quad rotado (exacto)
    Rect aabbRot = RotatedAABB_(dst, center, rotationDeg);
    Rect viewport{ 0.f, 0.f, (float)mWinW, (float)mWinH };
    if (!Intersects(aabbRot, viewport)) return;

    // 4) Dibujo
    if (filled) {
        // Relleno: usar la 1x1 blanca + modulación de color
        if (!mWhite) return;
        SDL_SetTextureColorMod(mWhite, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(mWhite, color.a);

        SDL_RenderCopyExF(mRenderer, mWhite, nullptr, &dst, rotationDeg, &center, SDL_FLIP_NONE);

    }
    else {
        // Borde: rotar 4 esquinas y trazar líneas
        const float rad = rotationDeg * 3.14159265f / 180.f;
        const float c = std::cos(rad), s = std::sin(rad);
        const float cx = dst.x + center.x, cy = dst.y + center.y;

        SDL_FPoint p[5];
        const float xs[4] = { -center.x,        dst.w - center.x, dst.w - center.x, -center.x };
        const float ys[4] = { -center.y,       -center.y,       dst.h - center.y,  dst.h - center.y };
        for (int i = 0; i < 4; ++i) {
            const float rx = xs[i] * c - ys[i] * s;
            const float ry = xs[i] * s + ys[i] * c;
            p[i].x = cx + rx;
            p[i].y = cy + ry;
        }
        p[4] = p[0]; // cerrar

        SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(mRenderer, color.r, color.g, color.b, color.a);

        SDL_RenderDrawLinesF(mRenderer, p, 5);
    }
	nDrawCallsThisFrame++;
}

void RenderManager::DrawLine(float x1, float y1, float x2, float y2, Color color) noexcept
{
    if ((color.r < 0 || color.r > 255) || (color.g < 0 || color.g > 255) || (color.b < 0 || color.b > 255) || (color.a < 0 || color.a > 255))
    {
        LogError("RenderManager warning", "DrawLine(): Color not valid.");
        return;
    }

    if (!mRenderer || !mActiveCam) return;

    Vec2 s1 = mActiveCam->WorldToScreen(x1, y1, mWinW, mWinH);
    Vec2 s2 = mActiveCam->WorldToScreen(x2, y2, mWinW, mWinH);

    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(mRenderer, color.r, color.g, color.b, color.a);

    SDL_RenderDrawLineF(mRenderer, s1.x, s1.y, s2.x, s2.y);
	nDrawCallsThisFrame++;
}

void RenderManager::DrawRectScreen(const Rect& r, Color color, bool filled) noexcept 
{
    if ((color.r < 0 || color.r > 255) || (color.g < 0 || color.g > 255) || (color.b < 0 || color.b > 255) || (color.a < 0 || color.a > 255))
    {
        LogError("RenderManager warning", "DrawRectScreen(): Color not valid.");
        return;
    }

    if (!mRenderer || !mWindow) return;

    Vec2I wS = mWindow->GetDrawableSize();
    if (wS.x <= 0 || wS.y <= 0) return;

    SDL_Rect sr{ (int)std::floor(r.x), (int)std::floor(r.y),
                 (int)std::ceil(r.w), (int)std::ceil(r.h) };
    if (sr.w <= 0 || sr.h <= 0) return;
    
    SDL_Rect screen{ 0, 0, wS.x, wS.y };
    SDL_Rect clipped;

    if (!SDL_Intersect(sr, screen, clipped)) return;

    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(mRenderer, color.r, color.g, color.b, color.a);
    if (filled) SDL_RenderFillRect(mRenderer, &clipped);
    else        SDL_RenderDrawRect(mRenderer, &clipped);
	nUIDrawCallsThisFrame++;
}

void RenderManager::DrawImageScreen(const Texture& tex, const Rect& dst, const Rect* src, Color tint, bool flipX, bool flipY) noexcept
{
    if ((tint.r < 0 || tint.r > 255) || (tint.g < 0 || tint.g > 255) || (tint.b < 0 || tint.b > 255) || (tint.a < 0 || tint.a > 255))
    {
        LogError("RenderManager warning", "DrawImageScreen(): Tint not valid.");
        return;
    }

    if (!mRenderer || !tex.GetSDL()) return;

    int texW = tex.Width();
    int texH = tex.Height();
    if (texW <= 0 || texH <= 0) return;

    SDL_Rect s{};
    if (src) {
        // Clamp >= 0
        int sx = std::max(0, (int)src->x);
        int sy = std::max(0, (int)src->y);
        int sw = std::max(0, (int)src->w);
        int sh = std::max(0, (int)src->h);

        // Limitar a tamaño de textura
        if (sx >= texW || sy >= texH || sw <= 0 || sh <= 0) return;
        if (sx + sw > texW) sw = texW - sx;
        if (sy + sh > texH) sh = texH - sy;

        s = { sx, sy, sw, sh };
    }
    else {
        s = { 0, 0, texW, texH };
    }

    Vec2I wS = mWindow->GetDrawableSize();
    if (wS.x <= 0 || wS.y <= 0) return;

    SDL_Rect d{ (int)std::floor(dst.x), (int)std::floor(dst.y),
                (int)std::ceil(dst.w), (int)std::ceil(dst.h) };
    if (d.w <= 0 || d.h <= 0) return;

    SDL_Rect screen{ 0, 0, wS.x, wS.y };
    SDL_Rect di; // dst intersectado
    if (!SDL_Intersect(d, screen, di)) return;

    const float sx = (float)d.w / (float)s.w;
    const float sy = (float)d.h / (float)s.h;

    const int cutL = di.x - d.x;
    const int cutT = di.y - d.y;

    SDL_Rect sAdj = s;
    sAdj.x += (int)std::floor(cutL / sx);
    sAdj.y += (int)std::floor(cutT / sy);

    sAdj.w -= (int)std::floor((d.w - di.w) / sx);
    sAdj.h -= (int)std::floor((d.h - di.h) / sy);

    if (sAdj.x < 0) sAdj.x = 0;
    if (sAdj.y < 0) sAdj.y = 0;
    if (sAdj.x + sAdj.w > texW) sAdj.w = texW - sAdj.x;
    if (sAdj.y + sAdj.h > texH) sAdj.h = texH - sAdj.y;
    if (sAdj.w <= 0 || sAdj.h <= 0) return;

    SDL_Rect dFinal = di;

    SDL_SetTextureColorMod(tex.GetSDL(), tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(tex.GetSDL(), tint.a);
    const SDL_RendererFlip f = static_cast<SDL_RendererFlip>(
        (flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE) |
        (flipY ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE)
        );

    SDL_RenderCopyEx(mRenderer, tex.GetSDL(), &sAdj, &dFinal, 0.0, nullptr, f);
	nUIDrawCallsThisFrame++;
}

bool RenderManager::Intersects(const Rect& a, const Rect& b) noexcept
{
    const float ax2 = a.x + a.w;
    const float ay2 = a.y + a.h;
    const float bx2 = b.x + b.w;
    const float by2 = b.y + b.h;

    if (ax2 <= b.x) return false;
    if (bx2 <= a.x) return false;
    if (ay2 <= b.y) return false;
    if (by2 <= a.y) return false;
    return true;
}   

void RenderManager::DrawDebugLine(float x1, float y1, float x2, float y2, Color color) noexcept
{
    mDebugCmds.push_back(DebugCmd{ DebugCmd::Type::Line, {}, x1, y1, x2, y2, color, false });
}

void RenderManager::DrawDebugRect(const Rect& worldRect, const Color color, bool filled) noexcept
{
    DebugCmd cmd;
    cmd.type = DebugCmd::Type::RectI;
    cmd.rect = worldRect;
    cmd.color = color;
    cmd.filled = filled;
    mDebugCmds.push_back(cmd);
}

void RenderManager::FlushDebug() noexcept
{
    if (!mRenderer || !mActiveCam) return;
    nDebugDrawCallsThisFrame = 0;

    // Dibuja los comandos debug al final (siempre arriba)
    for (const auto& c : mDebugCmds) {
        switch (c.type) {
        case DebugCmd::Type::Line:
            DrawLine(c.x1, c.y1, c.x2, c.y2, c.color);
            break;
        case DebugCmd::Type::RectI:
            DrawRect(c.rect, c.color, c.filled);
            break;
        }
        nDebugDrawCallsThisFrame++;
    }
    mDebugCmds.clear();
}

void RenderManager::SetBackground(Texture* tex, BackgroundMode mode, Color tint) noexcept
{
    mBackgroundTexture = tex;
    mBgMode = (tex ? mode : BackgroundMode::None);
    mBackgroundTint = tint;
}

void RenderManager::ClearBackground() noexcept
{
    mBackgroundTexture = nullptr;
    mBgMode = BackgroundMode::None;
    mBackgroundTint = { 255,255,255,255 };
}

void RenderManager::ClearParallax() noexcept
{
    mBgLayers.clear();
    mHasPrevCam = false;
}

void RenderManager::AddParallaxLayer(const BackgroundLayer& layer) noexcept
{
    if (!layer.tex) return;
    mBgLayers.push_back(layer);
}

Rect RenderManager::ComputeFitRect_(float winW, float winH, float texW, float texH) noexcept
{
    if (texW <= 0.f || texH <= 0.f || winW <= 0.f || winH <= 0.f) return { 0,0,0,0 };

    const float sx = winW / texW;
    const float sy = winH / texH;
    const float s = (sx < sy) ? sx : sy;

    const float w = texW * s;
    const float h = texH * s;
    return Rect{ (winW - w) * 0.5f, (winH - h) * 0.5f, w, h };
}

Rect RenderManager::ComputeFillRect_(float winW, float winH, float texW, float texH) noexcept
{
    if (texW <= 0.f || texH <= 0.f || winW <= 0.f || winH <= 0.f) return { 0,0,0,0 };

    const float sx = winW / texW;
    const float sy = winH / texH;
    const float s = (sx > sy) ? sx : sy;

    const float w = texW * s;
    const float h = texH * s;
    return Rect{ (winW - w) * 0.5f, (winH - h) * 0.5f, w, h };
}

inline float RenderManager::WrapOffset_(float x, float period) noexcept
{
    if (period <= 0.f) return 0.f;
    // wrap a [0, period)
    float r = std::fmod(x, period);
    if (r < 0.f) r += period;
    return r;
}

// ADAPTAR A LA CÁMARA USADA
Vec2 RenderManager::GetCamPos_(const Camera2D& cam) const noexcept
{
    return cam.GetCenter();
}

void RenderManager::DrawBackground_(const Camera2D& cam) noexcept
{
    if (!mRenderer) return;
    if (mWinW <= 0 || mWinH <= 0) return;

    const bool useLayers = !mBgLayers.empty();
    const Vec2 camPos = GetCamPos_(cam);

    // ------------------------------------------------------------
    // 1) Tilemap físico en MUNDO (quieto, player pasa por encima)
    // ------------------------------------------------------------
    auto drawPhysicalWorldTiles = [&](Texture* tex,
        Vec2 worldOrigin,
        Vec2 scale,
        Color tint,
        bool flipX, bool flipY,
        bool seamSafe,
        bool useBounds,
        Rect boundsWorld) noexcept
        {
            if (!tex || !tex->GetSDL() || !mActiveCam) return;

            float ppu = tex->pixelsPerUnit;
            if (ppu <= 0.f) ppu = 1.f;

            // Tamaño del tile en WORLD units
            const float tileW = (tex->Width() / ppu) * scale.x;
            const float tileH = (tex->Height() / ppu) * scale.y;
            if (tileW <= 0.f || tileH <= 0.f) return;

            // Viewport aproximado en mundo (si existe ScreenToWorld, esta es una aproximación)
            Vec2 w0 = cam.ScreenToWorld(0.f, 0.f, mWinW, mWinH);
            Vec2 w1 = cam.ScreenToWorld((float)mWinW, (float)mWinH, mWinW, mWinH);

            // AABB en mundo
            float minx = std::min(w0.x, w1.x);
            float miny = std::min(w0.y, w1.y);
            float maxx = std::max(w0.x, w1.x);
            float maxy = std::max(w0.y, w1.y);

            Rect viewWorld{ minx, miny, maxx - minx, maxy - miny };


            Rect area = viewWorld;
            if (useBounds && boundsWorld.w > 0.f && boundsWorld.h > 0.f) {
                float ax1 = std::max(area.x, boundsWorld.x);
                float ay1 = std::max(area.y, boundsWorld.y);
                float ax2 = std::min(area.x + area.w, boundsWorld.x + boundsWorld.w);
                float ay2 = std::min(area.y + area.h, boundsWorld.y + boundsWorld.h);
                if (ax2 <= ax1 || ay2 <= ay1) return;
                area = Rect{ ax1, ay1, ax2 - ax1, ay2 - ay1 };
            }

            // SeamSafe: 1 tile extra alrededor del área visible
            if (seamSafe) {
                area.x -= tileW; area.y -= tileH;
                area.w += tileW * 2.f; area.h += tileH * 2.f;
            }

            // índices a dibujar
            const int x0 = (int)std::floor((area.x - worldOrigin.x) / tileW);
            const int y0 = (int)std::floor((area.y - worldOrigin.y) / tileH);
            const int x1 = (int)std::ceil(((area.x + area.w) - worldOrigin.x) / tileW);
            const int y1 = (int)std::ceil(((area.y + area.h) - worldOrigin.y) / tileH);

            for (int ty = y0; ty < y1; ++ty)
            {
                // Mirrored repeat en Y si flipY
                const bool fy = flipY ? ((ty & 1) != 0) : false;

                for (int tx = x0; tx < x1; ++tx)
                {
                    const bool fx = flipX ? ((tx & 1) != 0) : false;

                    Rect dstWorld{
                        worldOrigin.x + tx * tileW,
                        worldOrigin.y + ty * tileH,
                        tileW,
                        tileH
                    };

                    DrawTexture(*tex, dstWorld, nullptr, 0.f, { 0.5f,0.5f }, tint, fx, fy);
                }
            }
        };

    // ------------------------------------------------------------
    // 2) Wrap helper
    // ------------------------------------------------------------
    auto wrapOffset = [&](float x, float period) noexcept -> float
        {
            if (period <= 0.f) return 0.f;
            float r = std::fmod(x, period);
            if (r < 0.f) r += period;
            return r;
        };

    // ------------------------------------------------------------
    // 3) Tiling en SCREEN px con fix de flip (mirrored repeat)
    // ------------------------------------------------------------
    auto drawTiledScreen = [&](Texture* tex,
        float tileW, float tileH,
        Vec2 offsetPx,
        Color tint,
        bool flipX, bool flipY,
        bool seamSafe) noexcept
        {
            if (!tex || !tex->GetSDL()) return;
            if (tileW <= 0.f || tileH <= 0.f) return;

            const float winW = (float)mWinW;
            const float winH = (float)mWinH;

            // índice entero estable del tile (para paridad de flip)
            const int baseIx = (int)std::floor(offsetPx.x / tileW);
            const int baseIy = (int)std::floor(offsetPx.y / tileH);

            const float fracX = offsetPx.x - (float)baseIx * tileW; // [0..tileW) si offset normal
            const float fracY = offsetPx.y - (float)baseIy * tileH;

            const int extraX = seamSafe ? 1 : 0;
            const int extraY = seamSafe ? 1 : 0;

            const float startX = -fracX - (float)extraX * tileW;
            const float startY = -fracY - (float)extraY * tileH;

            const int nx = (int)std::ceil((winW - startX) / tileW) + 1;
            const int ny = (int)std::ceil((winH - startY) / tileH) + 1;

            for (int y = 0; y < ny; ++y)
            {
                const int tileIy = baseIy + (y - extraY);
                const bool fy = flipY ? ((tileIy & 1) != 0) : false;

                for (int x = 0; x < nx; ++x)
                {
                    const int tileIx = baseIx + (x - extraX);
                    const bool fx = flipX ? ((tileIx & 1) != 0) : false;

                    Rect r{ startX + x * tileW, startY + y * tileH, tileW, tileH };
                    DrawImageScreen(*tex, r, nullptr, tint, fx, fy);
                }
            }
        };

    // ------------------------------------------------------------
    // 4) Dibujo de un modo "simple" (no tiled físico)
    // ------------------------------------------------------------
    auto drawOne = [&](Texture* tex,
        BackgroundMode mode,
        Color tint,
        Vec2 offsetPx,
        Vec2 scale,
        bool flipX,
        bool flipY,
        bool seamSafe) noexcept
        {
            if (!tex || !tex->GetSDL()) return;

            const float winW = (float)mWinW;
            const float winH = (float)mWinH;

            const float tw = (float)tex->Width();
            const float th = (float)tex->Height();
            if (tw <= 0.f || th <= 0.f) return;

            Rect dst{ 0.f, 0.f, winW, winH };

            switch (mode)
            {
            case BackgroundMode::None:
                return;

            case BackgroundMode::Stretch:
                dst = Rect{ offsetPx.x, offsetPx.y, winW, winH };
                DrawImageScreen(*tex, dst, nullptr, tint, flipX, flipY);
                return;

            case BackgroundMode::Fit:
                dst = ComputeFitRect_(winW, winH, tw, th);
                dst.x += offsetPx.x; dst.y += offsetPx.y;
                DrawImageScreen(*tex, dst, nullptr, tint, flipX, flipY);
                return;

            case BackgroundMode::Fill:
                dst = ComputeFillRect_(winW, winH, tw, th);
                dst.x += offsetPx.x; dst.y += offsetPx.y;
                DrawImageScreen(*tex, dst, nullptr, tint, flipX, flipY);
                return;

            case BackgroundMode::Center:
                dst = Rect{ (winW - tw) * 0.5f + offsetPx.x, (winH - th) * 0.5f + offsetPx.y, tw, th };
                DrawImageScreen(*tex, dst, nullptr, tint, flipX, flipY);
                return;

            case BackgroundMode::Repeat:
            case BackgroundMode::RepeatScaled:
            {
                float tileW = tw;
                float tileH = th;

                if (mode == BackgroundMode::RepeatScaled) {
                    tileW = tw * scale.x;
                    tileH = th * scale.y;
                    if (tileW <= 0.f || tileH <= 0.f) return;
                }

                // Usar tiled screen con mirrored flip
                drawTiledScreen(tex, tileW, tileH, offsetPx, tint, flipX, flipY, seamSafe);
                return;
            }

            default:
                return;
            }
        };

    // ------------------------------------------------------------
    // Caso simple (sin layers)
    // ------------------------------------------------------------
    if (!useLayers)
    {
        if (mBackgroundTexture && mBgMode != BackgroundMode::None)
            drawOne(mBackgroundTexture, mBgMode, mBackgroundTint,
                Vec2{ 0.f, 0.f }, Vec2{ 1.f, 1.f }, false, false, true);
        return;
    }

    // ------------------------------------------------------------
    // Orden de capas
    // ------------------------------------------------------------
    std::vector<BackgroundLayer> layers = mBgLayers;
    std::sort(layers.begin(), layers.end(),
        [](const BackgroundLayer& a, const BackgroundLayer& b) { return a.order < b.order; });

    // ------------------------------------------------------------
    // Dibujo capas
    // ------------------------------------------------------------
    for (const auto& L : layers)
    {
        if (!L.tex || !L.tex->GetSDL()) continue;

        float ppu = L.tex->pixelsPerUnit;
        if (ppu <= 0.f) ppu = 1.f;

        // Parallax en WORLD units -> px (screen offset)
        const float parX = -camPos.x * L.parallax * ppu;
        const float parY = -camPos.y * (L.parallaxY ? L.parallax : 0.f) * ppu;

        // --------------------------------------------------------
        // TILING FÍSICO REAL (WORLD SPACE)
        // --------------------------------------------------------
        if (L.mode == BackgroundMode::TileWorldPhysical || L.mode == BackgroundMode::TileWorldPhysicalScaled)
        {
            Vec2 scl = (L.mode == BackgroundMode::TileWorldPhysicalScaled) ? L.scale : Vec2{ 1.f, 1.f };

            // Offset en mundo usado como origen
            Vec2 worldOrigin = L.offsetPx;

            // Bounds: deshabilitado por defecto
            const bool useBounds = false;
            Rect boundsWorld{ 0,0,0,0 };

            drawPhysicalWorldTiles(L.tex, worldOrigin, scl, L.tint, L.flipX, L.flipY, L.seamSafe, useBounds, boundsWorld);
            continue;
        }

        // --------------------------------------------------------
        // RESTO MODOS (screen-space): offsetPx en px
        // --------------------------------------------------------
        Vec2 offsetPx{ 0.f, 0.f };

        offsetPx = Vec2{ parX + L.offsetPx.x, parY + L.offsetPx.y };

        drawOne(L.tex, L.mode, L.tint, offsetPx, L.scale, L.flipX, L.flipY, L.seamSafe);
    }
}


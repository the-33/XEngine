#include "RenderManager.h"

#include <cmath>

#include "SDL.h"
#include "SDL_ttf.h"
#include "WindowManager.h"
#include "Camera2D.h"
#include "RenderPreparation.h"
#include <chrono>
#include <algorithm>
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

    // Create SDL renderer
    Uint32 flags = 0;
    if (mAccelerated) flags |= SDL_RENDERER_ACCELERATED;
    if (mVSync)       flags |= SDL_RENDERER_PRESENTVSYNC;

    mRenderer = SDL_CreateRenderer(mWindow->SDL(), -1, flags);
    if (!mRenderer) {
        // Fallback without acceleration/VSYNC
        mRenderer = SDL_CreateRenderer(mWindow->SDL(), -1, 0);
    }

    if (!mRenderer)
    {
        LogError("RenderManager::Init()", "SDL_CreateRenderer(mWindow->SDL(), -1, 0) failed.", SDL_GetError());
        return false;
    }

    // Alpha blending for DrawRect/DrawLine and alpha textures
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
    ClearTextCache_();
    mTextureTintCache.clear();
    if (mWhite) { SDL_DestroyTexture(mWhite); mWhite = nullptr; }
    if (mRenderer) { SDL_DestroyRenderer(mRenderer); mRenderer = nullptr; }
    mActiveCam = nullptr;
    mWindow = nullptr;
    mWinW = mWinH = 0;
}

void RenderManager::Begin(const Camera2D& cam) noexcept
{
    const auto phaseStart = std::chrono::steady_clock::now();
    mDrawPhase = DrawPhase::Background;
    mTextureTintCache.clear();
    ++mTextCacheFrame;
    mActiveCam = &cam;
    const Vec2 view = cam.GetViewBase();
    const float zoom = cam.GetZoom();
    const Vec2 center = cam.GetCenter();
    mSelectorView = {center.x, center.y, view.x / zoom, view.y / zoom};

    if (mWindow) {
        const Vec2I drawable = mWindow->GetDrawableSize();
        mWinW = drawable.x; mWinH = drawable.y;
    }

    if (mRenderer) {
        SDL_SetRenderDrawColor(mRenderer, mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a);
        SDL_RenderClear(mRenderer);
    }

    DrawBackground_(cam);
    mBackgroundMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - phaseStart).count();
    mDrawPhase = DrawPhase::World;
}

void RenderManager::End() noexcept
{
    if (!mRenderer) return;

    // Delegate present to WindowManager to centralize backend-specific behavior
    if (mWindow) mWindow->Present(mRenderer);
    else         SDL_RenderPresent(mRenderer);

    for (SDL_Texture* t : mTempTextTextures) SDL_DestroyTexture(t);
    mTempTextTextures.clear();
    while (mTextCache.size() > 128)
    {
        auto oldest = std::min_element(mTextCache.begin(), mTextCache.end(),
            [](const auto& a, const auto& b) {
                return a.second.lastUsed < b.second.lastUsed;
            });
        if (oldest == mTextCache.end()) break;
        SDL_DestroyTexture(oldest->second.texture);
        mTextCache.erase(oldest);
    }

	for (SDL_Surface* s : mTempTextSurfaces) SDL_FreeSurface(s);
	mTempTextSurfaces.clear();

    mActiveCam = nullptr;
    // Frame phase profiling is owned by Engine (steady_clock).
}

void RenderManager::SetVSync(bool enabled) noexcept
{
    if (mVSync == enabled) return;
    mVSync = enabled;

    // SDL cannot change VSYNC on an existing renderer on every platform.
    // Robust option: recreate the renderer with the requested acceleration and VSYNC settings (shutdown and reinitialize)
    if (!mWindow || !mRenderer) return;

    ClearTextCache_();
    mTextureTintCache.clear();
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

void RenderManager::InvalidateTextureTint_(SDL_Texture* texture) noexcept
{
    mTextureTintCache.erase(texture);
}

void RenderManager::CountSubmission_() noexcept
{
    switch (mDrawPhase)
    {
        case DrawPhase::Background: ++mBackgroundDraws; break;
        case DrawPhase::World: ++mWorldDraws; break;
        case DrawPhase::Debug: ++mDebugDraws; break;
        case DrawPhase::UI: ++mUiDraws; break;
    }
}

void RenderManager::ApplyTextureTint_(SDL_Texture* texture, Color color) noexcept
{
    if (!texture) return;
    const auto it = mTextureTintCache.find(texture);
    if (it != mTextureTintCache.end() &&
        it->second.r == color.r && it->second.g == color.g &&
        it->second.b == color.b && it->second.a == color.a)
    {
        ++mTintCacheHits;
        return;
    }
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture, color.a);
    mTextureTintCache[texture] = color;
    ++mTintUpdates;
}

void RenderManager::DrawTextureImmediate_(const Texture& tex,
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
    // 1) Mundo -> Pantalla
    SDL_FRect dst = WorldRectToScreen(*mActiveCam, mWinW, mWinH, dstWorld);

    // 2) Rotation center (pivot) in screen-space rectangle pixels
    SDL_FPoint center{ dst.w * pivot01.x, dst.h * pivot01.y };

    // 3) Precise rotated-quad culling using the helper
    Rect aabbRot = RotatedAABB_(dst, center, rotationDeg);
    Rect viewport{ 0.f, 0.f, (float)mWinW, (float)mWinH };
    if (!Intersects(aabbRot, viewport)) { ++mImmediateCulledCommands; return; }

    // 4) Source rectangle (texture pixels)
    SDL_Rect sdlSrc;
    const SDL_Rect* pSrc = nullptr;
    if (srcPixels) {
        sdlSrc.x = (int)srcPixels->x;
        sdlSrc.y = (int)srcPixels->y;
        sdlSrc.w = (int)(srcPixels->w == 0 ? tex.Width() : srcPixels->w);
        sdlSrc.h = (int)(srcPixels->h == 0 ? tex.Height() : srcPixels->h);
        pSrc = &sdlSrc;
    }

    // Time the actual world-texture SDL submission only when profiling is requested.
    const bool timed = mMeasureSpriteSubmissions && mDrawPhase == DrawPhase::World;
    const auto spriteStart = timed ? std::chrono::steady_clock::now()
                                   : std::chrono::steady_clock::time_point{};
    // 5) Color/alpha modulation
    ApplyTextureTint_(tex.GetSDL(), tint);

    // 6) Flip + Draw
    SDL_RendererFlip sdlFlip = SDL_FLIP_NONE;
    if (flipX) sdlFlip = (SDL_RendererFlip)(sdlFlip | SDL_FLIP_HORIZONTAL);
    if (flipY) sdlFlip = (SDL_RendererFlip)(sdlFlip | SDL_FLIP_VERTICAL);

    SDL_RenderCopyExF(mRenderer, tex.GetSDL(), pSrc, &dst, rotationDeg, &center, sdlFlip);
    if (timed) mSpriteSubmitMs += std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - spriteStart).count();
    ++nDrawCallsThisFrame;
    CountSubmission_();
    nRenderedSpritesThisFrame++;
}

void RenderManager::DrawTexturePrepared_(const DrawCmd& cmd) noexcept
{
    if (!mRenderer || !cmd.tex || !cmd.tex->GetSDL()) return;
    const Texture& tex = *cmd.tex;
    const Rect& d = cmd.preparedScreen;
    const SDL_FRect dst{d.x, d.y, d.w, d.h};
    const SDL_FPoint center{cmd.preparedPivot.x, cmd.preparedPivot.y};
    SDL_Rect sdlSrc{};
    const SDL_Rect* src = nullptr;
    if (cmd.hasSrc)
    {
        sdlSrc = {(int)cmd.srcPx.x, (int)cmd.srcPx.y,
                  (int)(cmd.srcPx.w == 0.f ? tex.Width() : cmd.srcPx.w),
                  (int)(cmd.srcPx.h == 0.f ? tex.Height() : cmd.srcPx.h)};
        src = &sdlSrc;
    }
    const bool timed = mMeasureSpriteSubmissions && mDrawPhase == DrawPhase::World;
    const auto spriteStart = timed ? std::chrono::steady_clock::now()
                                   : std::chrono::steady_clock::time_point{};
    ApplyTextureTint_(tex.GetSDL(), cmd.color);
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (cmd.flipX) flip = (SDL_RendererFlip)(flip | SDL_FLIP_HORIZONTAL);
    if (cmd.flipY) flip = (SDL_RendererFlip)(flip | SDL_FLIP_VERTICAL);
    SDL_RenderCopyExF(mRenderer, tex.GetSDL(), src, &dst, cmd.rotationDeg, &center, flip);
    if (timed) mSpriteSubmitMs += std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - spriteStart).count();
    ++nDrawCallsThisFrame;
    CountSubmission_();
    ++nRenderedSpritesThisFrame;
}

void RenderManager::PrepareSnapshot(Snapshot& snapshot, ParallelExecutor* workers,
                                    std::size_t minCommands) noexcept
{
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    mLastWaitMs = 0.f;
    mLastPreparedCount = 0;
    mLastCulledCount = 0;
    mLastWorkers = 0;
    if (!snapshot.valid) return;

    const RenderPreparation::Camera camera{
        snapshot.cam.centerX, snapshot.cam.centerY,
        snapshot.cam.baseViewW, snapshot.cam.baseViewH,
        snapshot.cam.zoom, snapshot.winW, snapshot.winH};
    // Worker callbacks access only immutable camera data and disjoint command slots.
    // In particular: no SDL, no asset access, no scene object or live Camera reads.
    const auto prepare = [&](std::size_t index) noexcept
    {
        DrawCmd& command = snapshot.cmds[index];
        if (command.type != DrawCmd::Type::WorldTex &&
            command.type != DrawCmd::Type::WorldRect) return;
        // All sprite geometry is projected from an immutable value snapshot.
        // No live scene objects, textures, rendering APIs or mutable camera access here.
        if (command.type == DrawCmd::Type::WorldTex && command.hasDeferredSpritePose) {
            const auto projected = SpriteProjection::Project(snapshot.spritePoses[command.spritePoseIndex]);
            command.dstWorld = {projected.x, projected.y, projected.w, projected.h};
        }
        const Rect& r = (command.type == DrawCmd::Type::WorldTex)
                          ? command.dstWorld : command.worldRect;
        const auto result = RenderPreparation::Prepare(camera,
            {r.x, r.y, r.w, r.h}, command.rotationDeg,
            command.pivot01.x, command.pivot01.y);
        command.hasPreparedGeometry = true;
        command.preparedVisible = result.visible;
        command.preparedScreen = {result.screen.x, result.screen.y,
                                  result.screen.w, result.screen.h};
        command.preparedPivot = {result.pivotX, result.pivotY};
    };
    if (workers && workers->WorkerCount() && snapshot.cmds.size() >= minCommands)
    {
        // Chunking limits atomic queue contention for high sprite counts.
        constexpr std::size_t chunkSize = 64;
        const auto chunks = (snapshot.cmds.size() + chunkSize - 1) / chunkSize;
        workers->Run(chunks, [&](std::size_t chunk) {
            const auto start = chunk * chunkSize;
            const auto end = std::min(start + chunkSize, snapshot.cmds.size());
            for (auto i = start; i < end; ++i) prepare(i);
        });
        mLastWaitMs = workers->LastWaitMs();
        mLastWorkers = workers->WorkerCount();
    }
    else
    {
        for (std::size_t i = 0; i < snapshot.cmds.size(); ++i) prepare(i);
    }
    for (const auto& cmd : snapshot.cmds)
    {
        if (cmd.hasPreparedGeometry)
        {
            ++mLastPreparedCount;
            if (!cmd.preparedVisible) ++mLastCulledCount;
        }
    }
    mLastPrepareMs = std::chrono::duration<float, std::milli>(
        Clock::now() - start).count();
}

void RenderManager::RenderSnapshot(const Snapshot& snap) noexcept
{
    if (!snap.valid) return;

    // Build a temporary camera using the captured state
    Camera2D camTmp;
    camTmp.mCenterX = snap.cam.centerX;
    camTmp.mCenterY = snap.cam.centerY;
    camTmp.mBaseViewWidth = snap.cam.baseViewW;
    camTmp.mBaseViewHeight = snap.cam.baseViewH;
    camTmp.mZoom = snap.cam.zoom;

    const Rect* pSrc = nullptr;

    Begin(camTmp);

    for (const auto& c : snap.cmds)
    {
        mDrawPhase = c.debugDraw ? DrawPhase::Debug :
            (c.type == DrawCmd::Type::ScreenRect ||
             c.type == DrawCmd::Type::ScreenImage ||
             c.type == DrawCmd::Type::ScreenText ? DrawPhase::UI : DrawPhase::World);
        switch (c.type)
        {
        case DrawCmd::Type::WorldTex:
            if (c.hasPreparedGeometry)
            {
                if (c.preparedVisible && c.tex) DrawTexturePrepared_(c);
            }
            else if (c.tex)
            {
                pSrc = c.hasSrc ? &c.srcPx : nullptr;
                DrawTextureImmediate_(*c.tex, c.dstWorld, pSrc, c.rotationDeg, c.pivot01, c.color, c.flipX, c.flipY);
            }
            break;

        case DrawCmd::Type::WorldRect:
            if (!c.hasPreparedGeometry || c.preparedVisible)
                DrawRectImmediate_(c.worldRect, c.rotationDeg, c.pivot01, c.color, c.filled);
            break;

        case DrawCmd::Type::WorldLine:
            DrawLineImmediate_(c.x1, c.y1, c.x2, c.y2, c.color);
            break;

        case DrawCmd::Type::ScreenRect:
            DrawRectScreenImmediate_(c.dstScreen, c.color, c.filled);
            break;

        case DrawCmd::Type::ScreenImage:
            pSrc = c.hasSrc ? &c.srcPx : nullptr;
            DrawImageScreenImmediate_(*c.tex, c.dstScreen, pSrc, c.color, c.flipX, c.flipY);
            break;

        case DrawCmd::Type::ScreenText:
            DrawTextScreenImmediate_(*c.font, c.text, c.textX, c.textY, c.color);
            
            break;
        }
    }
    mDrawPhase = DrawPhase::World;
}

void RenderManager::BeginRecord(const Camera2D* cam, Snapshot& out) noexcept
{
    out.valid = false;
    out.cmds.clear();
    out.spritePoses.clear();

    if (mWindow)
    {
        Vec2I d = mWindow->GetDrawableSize();
        out.winW = d.x;
        out.winH = d.y;
        mWinW = d.x;
        mWinH = d.y;
    }
    else
    {
        out.winW = mWinW;
        out.winH = mWinH;
    }

    if (!cam) return;

    // Copy plain camera state (thread-safe)
    out.cam.centerX = cam->mCenterX;
    out.cam.centerY = cam->mCenterY;
    out.cam.baseViewW = cam->mBaseViewWidth;
    out.cam.baseViewH = cam->mBaseViewHeight;
    out.cam.zoom = cam->mZoom;
    mSelectorView = {out.cam.centerX, out.cam.centerY,
                     out.cam.baseViewW / out.cam.zoom,
                     out.cam.baseViewH / out.cam.zoom};

    // activar recording
    snapRecord = &out;
}

void RenderManager::EndRecord() noexcept
{
    if (snapRecord) snapRecord->valid = true;
    snapRecord = nullptr;
}


void RenderManager::DrawSprite_(const Texture& tex, const SpriteProjection::Pose& pose,
                                const Rect& srcPx, Color tint,
                                bool flipX, bool flipY) noexcept
{
    if (snapRecord && mDeferSpriteProjection) {
        if (mDrawPhase != DrawPhase::Debug) {
            ++mPublicWorldCommandsThisFrame;
            const auto units = RenderDiagnostics::SpriteUnits(
                pose.rotationX != 0.f || pose.rotationY != 0.f,
                pose.rotationZ != 0.f,
                pose.offsetX != 0.f || pose.offsetY != 0.f);
            mPublicWorldWorkUnitsThisFrame += units;
            const bool offscreen = mEstimateVisibleWork &&
                RenderDiagnostics::DefinitelyOffscreenSprite(mSelectorView,
                    pose.x, pose.y, pose.width, pose.height,
                    pose.scaleX, pose.scaleY, pose.offsetX, pose.offsetY,
                    pose.pivotX, pose.pivotY);
            mPublicEffectiveWorldWorkUnitsThisFrame +=
                RenderDiagnostics::EffectiveUnits(units, offscreen);
            mEstimatedOffscreenSprites += offscreen ? 1u : 0u;
        }
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::WorldTex;
        c.tex = &tex;
        c.hasSrc = true;
        c.srcPx = srcPx;
        c.rotationDeg = pose.rotationZ;
        c.pivot01 = {pose.pivotX, pose.pivotY};
        c.color = tint;
        c.flipX = flipX;
        c.flipY = flipY;
        c.hasDeferredSpritePose = true;
        c.spritePoseIndex = static_cast<std::uint32_t>(snapRecord->spritePoses.size());
        snapRecord->spritePoses.push_back(pose);
        return;
    }
    // Immediate draw and A/B 'main' mode use the exact same pure CPU
    // projection as workers mode. This isolates deferral in the benchmark.
    const auto projected = SpriteProjection::Project(pose);
    const Rect dst{projected.x, projected.y, projected.w, projected.h};
    mSpriteWorkOverride = RenderDiagnostics::SpriteUnits(
        pose.rotationX != 0.f || pose.rotationY != 0.f,
        pose.rotationZ != 0.f,
        pose.offsetX != 0.f || pose.offsetY != 0.f);
    const bool offscreen = mEstimateVisibleWork &&
        RenderDiagnostics::DefinitelyOffscreenSprite(mSelectorView,
            pose.x, pose.y, pose.width, pose.height,
            pose.scaleX, pose.scaleY, pose.offsetX, pose.offsetY,
            pose.pivotX, pose.pivotY);
    mSpriteEffectiveWorkOverride = RenderDiagnostics::EffectiveUnits(mSpriteWorkOverride, offscreen);
    mEstimatedOffscreenSprites += offscreen ? 1u : 0u;
    DrawTexture(tex, dst, &srcPx, pose.rotationZ,
                {pose.pivotX, pose.pivotY}, tint, flipX, flipY);
    mSpriteWorkOverride = 0;
    mSpriteEffectiveWorkOverride = 0;
}

void RenderManager::DrawTexture(const Texture& tex, const Rect& dstWorld, const Rect* srcPixels, float rotationDeg, Vec2 pivot01, Color tint, bool flipX, bool flipY) noexcept
{
    if (mDrawPhase != DrawPhase::Debug) {
        ++mPublicWorldCommandsThisFrame;
        const auto units = mSpriteWorkOverride ? mSpriteWorkOverride :
            RenderDiagnostics::RectangleUnits(rotationDeg != 0.f);
        mPublicWorldWorkUnitsThisFrame += units;
        mPublicEffectiveWorldWorkUnitsThisFrame += mSpriteWorkOverride ?
            mSpriteEffectiveWorkOverride : units;
    }
	// --- RECORDING MODE ---
    if (snapRecord)
    {
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::WorldTex;
        c.tex = &tex;
        c.dstWorld = dstWorld;
        c.hasSrc = (srcPixels != nullptr);
        if (srcPixels) c.srcPx = *srcPixels;
        c.rotationDeg = rotationDeg;
        c.pivot01 = pivot01;
        c.color = tint;
        c.flipX = flipX;
        c.flipY = flipY;
		return;
    }

	// --- IMMEDIATE MODE ---
	DrawTextureImmediate_(tex, dstWorld, srcPixels, rotationDeg, pivot01, tint, flipX, flipY);
}

void RenderManager::DrawRect(const Rect& worldRect, const Color color, const bool filled) noexcept
{
    DrawRect(worldRect, /*rotationDeg=*/0.f, /*pivot01=*/{ 0.5f, 0.5f }, color, filled);
}

void RenderManager::DrawRect(const Rect& worldRect, float rotationDeg, Vec2 pivot01, Color color, bool filled) noexcept
{
    if (mDrawPhase != DrawPhase::Debug) {
        ++mPublicWorldCommandsThisFrame;
        const auto units = RenderDiagnostics::RectangleUnits(rotationDeg != 0.f);
        mPublicWorldWorkUnitsThisFrame += units;
        mPublicEffectiveWorldWorkUnitsThisFrame += units;
    }
    // --- RECORDING MODE ---
    if (snapRecord)
    {
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::WorldRect;
        c.debugDraw = (mDrawPhase == DrawPhase::Debug);
        c.color = color;
        c.worldRect = worldRect;
        c.filled = filled;

        // Reutilizamos estos campos aunque el comentario ponga "WorldTex"
        c.rotationDeg = rotationDeg;
        c.pivot01 = pivot01;

        return;
    }

    // --- IMMEDIATE MODE ---
    DrawRectImmediate_(worldRect, rotationDeg, pivot01, color, filled);
}

void RenderManager::DrawLine(float x1, float y1, float x2, float y2, Color color) noexcept
{
    // --- RECORDING MODE ---
    if (snapRecord)
    {
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::WorldLine;
        c.debugDraw = (mDrawPhase == DrawPhase::Debug);
        c.color = color;
        c.x1 = x1; c.y1 = y1;
        c.x2 = x2; c.y2 = y2;

        return;
    }

    // --- IMMEDIATE MODE ---
    DrawLineImmediate_(x1, y1, x2, y2, color);
}

void RenderManager::DrawRectScreen(const Rect& screenRect, Color color, bool filled) noexcept
{
    // --- RECORDING MODE ---
    if (snapRecord)
    {
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::ScreenRect;
        c.color = color;
        c.dstScreen = screenRect;
        c.filled = filled; // also reused here

        return;
    }

    // --- IMMEDIATE MODE ---
    // Screen-space submissions are UI even when issued during Scene::Render.
    // Preserve the caller phase; the snapshot replay path already uses UI.
    const DrawPhase previousPhase = mDrawPhase;
    mDrawPhase = DrawPhase::UI;
    DrawRectScreenImmediate_(screenRect, color, filled);
    mDrawPhase = previousPhase;

}

void RenderManager::DrawImageScreen(const Texture& tex, const Rect& dstScreen, const Rect* srcPixels, Color tint, bool flipX, bool flipY) noexcept
{
    // --- RECORDING MODE ---
    if (snapRecord)
    {
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::ScreenImage;

        c.tex = &tex;
        c.dstScreen = dstScreen;

        c.hasSrc = (srcPixels != nullptr);
        if (srcPixels) c.srcPx = *srcPixels;

        c.color = tint;
        c.flipX = flipX;
        c.flipY = flipY;

        return;
    }

    // --- IMMEDIATE MODE ---
    const DrawPhase previousPhase = mDrawPhase;
    mDrawPhase = DrawPhase::UI;
    DrawImageScreenImmediate_(tex, dstScreen, srcPixels, tint, flipX, flipY);
    mDrawPhase = previousPhase;
}

void RenderManager::DrawTextScreen(const Font& font, const std::string& text, float x_px, float y_px, Color color) noexcept
{
    if (text.empty()) return;

    // --- RECORDING MODE ---
    if (snapRecord)
    {
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::ScreenText;

        c.font = &font;
        c.text = text;
        c.textX = x_px;
        c.textY = y_px;
        c.color = color;

        return;
    }

    // --- IMMEDIATE MODE ---
    const DrawPhase previousPhase = mDrawPhase;
    mDrawPhase = DrawPhase::UI;
    DrawTextScreenImmediate_(font, text, x_px, y_px, color);
    mDrawPhase = previousPhase;
}

void RenderManager::ClearTextCache_() noexcept
{
    for (auto& entry : mTextCache)
        if (entry.second.texture) SDL_DestroyTexture(entry.second.texture);
    mTextCache.clear();
}

void RenderManager::DrawTextScreenImmediate_(const Font& font, const std::string& text, float x, float y, Color color) noexcept
{
    if (!mRenderer || !mWindow || !font.GetSDL() || text.empty()) return;
    const Vec2I size = mWindow->GetDrawableSize();
    if (size.x <= 0 || size.y <= 0) return;
    // Cache only bounded-size text to avoid retaining arbitrarily large content.
    const bool cacheable = text.size() <= 160;
    std::string key;
    if (cacheable)
    {
        key = std::to_string(reinterpret_cast<std::uintptr_t>(font.GetSDL())) + ":" +
              std::to_string(color.r) + "," + std::to_string(color.g) + "," +
              std::to_string(color.b) + "," + std::to_string(color.a) + ":" + text;
        auto found = mTextCache.find(key);
        if (found != mTextCache.end())
        {
            found->second.lastUsed = mTextCacheFrame;
            const auto& e = found->second;
            if ((int)x >= size.x || (int)y >= size.y ||
                (int)x + e.width <= 0 || (int)y + e.height <= 0) return;
            SDL_Rect dst{(int)x, (int)y, e.width, e.height};
            SDL_RenderCopy(mRenderer, e.texture, nullptr, &dst);
            ++nUIDrawCallsThisFrame;
            CountSubmission_();
            return;
        }
    }
    int width = 0, height = 0;
    if (TTF_SizeUTF8(font.GetSDL(), text.c_str(), &width, &height) == 0)
    {
        if ((int)x >= size.x || (int)y >= size.y ||
            (int)x + width <= 0 || (int)y + height <= 0) return;
    }
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font.GetSDL(), text.c_str(),
        SDL_Color{color.r,color.g,color.b,color.a});
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(mRenderer, surface);
    width = surface->w;
    height = surface->h;
    SDL_FreeSurface(surface);
    if (!texture) return;
    SDL_Rect dst{(int)x, (int)y, width, height};
    SDL_RenderCopy(mRenderer, texture, nullptr, &dst);
    ++nUIDrawCallsThisFrame;
    CountSubmission_();
    if (!cacheable)
    {
        mTempTextTextures.push_back(texture);
        return;
    }
    mTextCache.emplace(std::move(key), TextCacheEntry{texture, width, height, mTextCacheFrame});
}

void RenderManager::DrawRectImmediate_(const Rect& worldRect,
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

    // 1) World to screen (unrotated rectangle AABB)
    SDL_FRect dst = WorldRectToScreen(*mActiveCam, mWinW, mWinH, worldRect);

    // 2) Rotation center (pivot) in pixels
    SDL_FPoint center{ dst.w * pivot01.x, dst.h * pivot01.y };

    // 3) Cull using the rotated quad AABB (precise)
    Rect aabbRot = RotatedAABB_(dst, center, rotationDeg);
    Rect viewport{ 0.f, 0.f, (float)mWinW, (float)mWinH };
    if (!Intersects(aabbRot, viewport)) { ++mImmediateCulledCommands; return; }

    // 4) Dibujo
    if (filled) {
        // Fill: use the white 1x1 texture with color modulation
        if (!mWhite) return;
        ApplyTextureTint_(mWhite, color);

        SDL_RenderCopyExF(mRenderer, mWhite, nullptr, &dst, rotationDeg, &center, SDL_FLIP_NONE);

    }
    else {
        // Border: rotate four corners and draw lines
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
    CountSubmission_();
}

void RenderManager::DrawLineImmediate_(float x1, float y1, float x2, float y2, Color color) noexcept
{
    if ((color.r < 0 || color.r > 255) || (color.g < 0 || color.g > 255) || (color.b < 0 || color.b > 255) || (color.a < 0 || color.a > 255))
    {
        LogError("RenderManager warning", "DrawLine(): Color not valid.");
        return;
    }

    if (!mRenderer || !mActiveCam) return;

    Vec2 s1 = mActiveCam->WorldToScreen(x1, y1, mWinW, mWinH);
    Vec2 s2 = mActiveCam->WorldToScreen(x2, y2, mWinW, mWinH);
    // Conservative AABB test: SDL would otherwise be called for fully invisible lines.
    if (std::max(s1.x, s2.x) < 0.f || std::min(s1.x, s2.x) >= float(mWinW) ||
        std::max(s1.y, s2.y) < 0.f || std::min(s1.y, s2.y) >= float(mWinH)) { ++mImmediateCulledCommands; return; }

    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(mRenderer, color.r, color.g, color.b, color.a);

    SDL_RenderDrawLineF(mRenderer, s1.x, s1.y, s2.x, s2.y);
	nDrawCallsThisFrame++;
    CountSubmission_();
}

void RenderManager::DrawRectScreenImmediate_(const Rect& r, Color color, bool filled) noexcept
{
    if ((color.r < 0 || color.r > 255) || (color.g < 0 || color.g > 255) || (color.b < 0 || color.b > 255) || (color.a < 0 || color.a > 255))
    {
        LogError("RenderManager warning", "DrawRectScreen(): Color not valid.");
        return;
    }

    if (!mRenderer || !mWindow) return;

    const Vec2I wS{mWinW, mWinH};
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
    CountSubmission_();
}

void RenderManager::DrawImageScreenImmediate_(const Texture& tex, const Rect& dst, const Rect* src, Color tint, bool flipX, bool flipY) noexcept
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

        // Clamp to texture size
        if (sx >= texW || sy >= texH || sw <= 0 || sh <= 0) return;
        if (sx + sw > texW) sw = texW - sx;
        if (sy + sh > texH) sh = texH - sy;

        s = { sx, sy, sw, sh };
    }
    else {
        s = { 0, 0, texW, texH };
    }

    const Vec2I wS{mWinW, mWinH};
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

    ApplyTextureTint_(tex.GetSDL(), tint);
    const SDL_RendererFlip f = static_cast<SDL_RendererFlip>(
        (flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE) |
        (flipY ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE)
        );

    SDL_RenderCopyEx(mRenderer, tex.GetSDL(), &sAdj, &dFinal, 0.0, nullptr, f);
	nUIDrawCallsThisFrame++;
    CountSubmission_();
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
    // Debug commands are recorded as ordinary world commands when recording.
    if (!mRenderer) return;
    if (!mActiveCam && !snapRecord) return;
    const auto start = std::chrono::steady_clock::now();
    const DrawPhase saved = mDrawPhase;
    mDrawPhase = DrawPhase::Debug;
    nDebugDrawCallsThisFrame = 0;

    // Draw debug commands last (always on top)
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
    mDrawPhase = saved;
    mDebugMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - start).count();
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

// ADAPT TO THE ACTIVE CAMERA
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
    // 1) Physical tilemap in WORLD space (stationary as the player moves)
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

            // Tile size in WORLD units
            const float tileW = (tex->Width() / ppu) * scale.x;
            const float tileH = (tex->Height() / ppu) * scale.y;
            if (tileW <= 0.f || tileH <= 0.f) return;

            // Approximate world-space viewport (if ScreenToWorld exists, this is an approximation)
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

            // SeamSafe: add one tile around the visible region
            if (seamSafe) {
                area.x -= tileW; area.y -= tileH;
                area.w += tileW * 2.f; area.h += tileH * 2.f;
            }

            // indices to draw
            const int x0 = (int)std::floor((area.x - worldOrigin.x) / tileW);
            const int y0 = (int)std::floor((area.y - worldOrigin.y) / tileH);
            const int x1 = (int)std::ceil(((area.x + area.w) - worldOrigin.x) / tileW);
            const int y1 = (int)std::ceil(((area.y + area.h) - worldOrigin.y) / tileH);

            for (int ty = y0; ty < y1; ++ty)
            {
                // Mirrored repeat on Y when flipY is enabled
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

                    DrawTextureImmediate_(*tex, dstWorld, nullptr, 0.f, { 0.5f,0.5f }, tint, fx, fy);
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
    // 3) Tiling in SCREEN pixels with mirrored-repeat flipping
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

            // stable integer tile index (for flip parity)
            const int baseIx = (int)std::floor(offsetPx.x / tileW);
            const int baseIy = (int)std::floor(offsetPx.y / tileH);

            const float fracX = offsetPx.x - (float)baseIx * tileW; // [0..tileW) for a normalized offset
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
                    DrawImageScreenImmediate_(*tex, r, nullptr, tint, fx, fy);
                }
            }
        };

    // ------------------------------------------------------------
    // 4) Draw in simple mode (no physical tiling)
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
                DrawImageScreenImmediate_(*tex, dst, nullptr, tint, flipX, flipY);
                return;

            case BackgroundMode::Fit:
                dst = ComputeFitRect_(winW, winH, tw, th);
                dst.x += offsetPx.x; dst.y += offsetPx.y;
                DrawImageScreenImmediate_(*tex, dst, nullptr, tint, flipX, flipY);
                return;

            case BackgroundMode::Fill:
                dst = ComputeFillRect_(winW, winH, tw, th);
                dst.x += offsetPx.x; dst.y += offsetPx.y;
                DrawImageScreenImmediate_(*tex, dst, nullptr, tint, flipX, flipY);
                return;

            case BackgroundMode::Center:
                dst = Rect{ (winW - tw) * 0.5f + offsetPx.x, (winH - th) * 0.5f + offsetPx.y, tw, th };
                DrawImageScreenImmediate_(*tex, dst, nullptr, tint, flipX, flipY);
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

                // Use tiled screen with mirrored repeat
                drawTiledScreen(tex, tileW, tileH, offsetPx, tint, flipX, flipY, seamSafe);
                return;
            }

            default:
                return;
            }
        };

    // ------------------------------------------------------------
    // Simple case (without layers)
    // ------------------------------------------------------------
    if (!useLayers)
    {
        if (mBackgroundTexture && mBgMode != BackgroundMode::None)
            drawOne(mBackgroundTexture, mBgMode, mBackgroundTint,
                Vec2{ 0.f, 0.f }, Vec2{ 1.f, 1.f }, false, false, true);
        return;
    }

    // ------------------------------------------------------------
    // Layer order
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
        // ACTUAL PHYSICAL TILING (WORLD SPACE)
        // --------------------------------------------------------
        if (L.mode == BackgroundMode::TileWorldPhysical || L.mode == BackgroundMode::TileWorldPhysicalScaled)
        {
            Vec2 scl = (L.mode == BackgroundMode::TileWorldPhysicalScaled) ? L.scale : Vec2{ 1.f, 1.f };

            // Offset en mundo usado como origen
            Vec2 worldOrigin = L.offsetPx;

            // Bounds: disabled by default
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


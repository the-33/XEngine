#include "SpriteRenderer.h"

#include "ErrorHandler.h"
#include "Transform.h"
#include "RenderManager.h"
#include "GameObject.h"

void SpriteRenderer::SetTexture(const Texture* tex) noexcept {
    if (tex != nullptr) texture = tex;
}

void SpriteRenderer::SetSource(RectI src) noexcept {

    if (!texture) {
        LogError("SpriteRenderer warning", "SetSource() called without texture.");
        return;
    }

    const int texW = texture ? texture->Width() : 0;
    const int texH = texture ? texture->Height() : 0;

    srcX = std::clamp(src.x, 0, std::max(0, texW - 1));
    srcY = std::clamp(src.y, 0, std::max(0, texH - 1));

    // Clamp to prevent out-of-bounds texture reads
    if (texW > 0 && texH > 0) {
        srcW = (src.w > 0) ? std::min(src.w, texW - srcX) : texW;
        srcH = (src.h > 0) ? std::min(src.h, texH - srcY) : texH;
    }
    else {
        srcW = (src.w > 0) ? src.w : 0;
        srcH = (src.h > 0) ? src.h : 0;
    }
}

void SpriteRenderer::SetTint(Color tint) noexcept
{
    if ((tint.r < 0 || tint.r > 255) || (tint.g < 0 || tint.g > 255) || (tint.b < 0 || tint.b > 255) || (tint.a < 0 || tint.a > 255))
    {
        LogError("SpriteRenderer warning", "SetTint(): color not valid.");
        return;
    }

    tint_ = tint;
}

//std::unique_ptr<Component> SpriteRenderer::Clone(GameObject* newOwner) const {
//    auto up = std::make_unique<SpriteRenderer>(); // default constructor: properties already bound to *up
//    up->mGameObject = newOwner;
//
//    // --- Copy internal state only ---
//    up->texture = texture;
//    up->srcX = srcX;
//    up->srcY = srcY;
//    up->srcW = srcW;
//    up->srcH = srcH;
//
//    up->tint_ = tint_;
//    up->flipX_ = flipX_;
//    up->flipY_ = flipY_;
//
//    up->offset_ = offset_;
//    up->pivot01_ = pivot01_;
//
//    return up;
//}

void SpriteRenderer::Render()
{
    if (!enabled || !mGameObject || !texture) return;

    auto* t = mGameObject->GetComponent<Transform>();
    if (!t) return;

    // 1) Source region in pixels
    const int pxW = (srcW > 0 ? srcW : texture->Width());
    const int pxH = (srcH > 0 ? srcH : texture->Height());
    if (pxW <= 0 || pxH <= 0) return;

    // 2) PPU -> base world-space size
    const float ppu = texture->PixelsPerUnit();
    const float baseW = pxW / (ppu > 0.f ? ppu : 100.f);
    const float baseH = pxH / (ppu > 0.f ? ppu : 100.f);

    // 3) Snapshot the transform's numeric values while still on the main
    // thread. Parallel render preparation projects them without reading the
    // live Transform, Texture, or scene object.
    const Vec3 wp = t->position;
    const Vec2 sc = t->scale;
    const Vec3 rot = t->rotation;
    const SpriteProjection::Pose pose{
        wp.x, wp.y, sc.x, sc.y, rot.x, rot.y, rot.z,
        baseW, baseH, offset_.x, offset_.y, pivot01_.x, pivot01_.y};
    const Rect srcPx{(float)srcX, (float)srcY, (float)pxW, (float)pxH};
    RenderManager::GetInstance().DrawSprite_(
        *texture, pose, srcPx, tint_, flipX_, flipY_);

}


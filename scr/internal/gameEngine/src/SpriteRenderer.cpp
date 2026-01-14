#include "SpriteRenderer.h"

#include <cmath>

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

    // Clamp para evitar lecturas fuera de la textura
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
//    auto up = std::make_unique<SpriteRenderer>(); // ctor por defecto -> propiedades ya ligadas a *up
//    up->mGameObject = newOwner;
//
//    // --- Copia solo los datos internos ---
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

    // 1) Región fuente en píxeles
    const int pxW = (srcW > 0 ? srcW : texture->Width());
    const int pxH = (srcH > 0 ? srcH : texture->Height());
    if (pxW <= 0 || pxH <= 0) return;

    // 2) PPU -> tamaño base en mundo
    const float ppu = texture->PixelsPerUnit();
    const float baseW = pxW / (ppu > 0.f ? ppu : 100.f);
    const float baseH = pxH / (ppu > 0.f ? ppu : 100.f);

    // 3) Pose mundo (con 3 ejes de rot)
    const Vec3 wp = t->position;  // requiere métodos públicos en Transform
    const Vec2 sc = t->scale;
    const Vec3 rot = t->rotation;  // x,y para pseudo3D; z rota el sprite
    const float rotZ = rot.z;

    // 4) Pseudo-3D: rotY -> comprime X, rotX -> comprime Y (coseno)
    auto DegToRad = [](float d) { return d * 3.14159265358979323846f / 180.f; };
    const float minFac = 0.f; // evita colapsar a 0
    const float fx = std::max(minFac, std::cos(DegToRad(rot.y))); // tilt Y -> escala X
    const float fy = std::max(minFac, std::cos(DegToRad(rot.x))); // tilt X -> escala Y
    const float k = 1.0f; // intensidad del efecto (0..1).

    const float sx = sc.x * ((1.0f - k) + k * fx);
    const float sy = sc.y * ((1.0f - k) + k * fy);

    // 5) Tamaño final en mundo
    const float wWorld = baseW * sx;
    const float hWorld = baseH * sy;

    // 6) Offset local que gira con Z
    const float radZ = DegToRad(rotZ);
    const float offX = offset_.x * sc.x;
    const float offY = offset_.y * sc.y;
    const float offRotX = offX * std::cos(radZ) - offY * std::sin(radZ);
    const float offRotY = offX * std::sin(radZ) + offY * std::cos(radZ);

    // 7) Destino en mundo ajustado por pivot
    Rect dstWorld;
    dstWorld.x = wp.x + offRotX - wWorld * pivot01_.x;
    dstWorld.y = wp.y + offRotY - hWorld * pivot01_.y;
    dstWorld.w = wWorld;
    dstWorld.h = hWorld;

    // 8) Fuente en píxeles
    Rect srcPx{ (float)srcX, (float)srcY, (float)pxW, (float)pxH };

    // 9) Dibujo (RenderManager maneja cámara/culling/rotación)
    RenderManager::GetInstance().DrawTexture(
        *texture,
        dstWorld,
        &srcPx,
        rotZ,          // solo Z rota visualmente el sprite
        pivot01_,
        tint_,
        flipX_,
        flipY_
    );
}


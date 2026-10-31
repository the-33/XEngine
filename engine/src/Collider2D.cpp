#include "Collider2D.h"

#include <cmath>
#include <algorithm>

#include "Transform.h"
#include "CollisionManager.h"
#include "RenderManager.h"
#include "GameObject.h"
#include "RigidBody2D.h"
#include "SpriteRenderer.h"


static inline float DegToRad(float d) noexcept { return d * 3.14159265358979323846f / 180.f; }
static inline Vec2 Rot2D(Vec2 v, float deg) noexcept {
    const float r = DegToRad(deg);
    const float c = std::cos(r);
    const float s = std::sin(r);
    return { c * v.x - s * v.y, s * v.x + c * v.y };
}

bool Collider2D::FitToSprite(FitMode mode, float factor) noexcept
{
    return FitToSprite(mode, factor, factor);
}

bool Collider2D::FitToSprite(FitMode mode, float widthFactor, float heightFactor) noexcept
{
    if (!mGameObject || !std::isfinite(widthFactor) || !std::isfinite(heightFactor) ||
        widthFactor <= 0.f || heightFactor <= 0.f)
        return false;

    const auto* renderer = mGameObject->GetComponent<SpriteRenderer>();
    if (!renderer) return false;
    const Texture* texture = renderer->sprite.get();
    if (!texture) return false;

    const float ppu = texture->PixelsPerUnit();
    if (!(ppu > 0.f) || !std::isfinite(ppu)) return false;
    const RectI source = renderer->source.get();
    // Use the same source-region fallback as SpriteRenderer::Render().
    const int pixelWidth = source.w > 0 ? source.w : texture->Width();
    const int pixelHeight = source.h > 0 ? source.h : texture->Height();
    if (pixelWidth <= 0 || pixelHeight <= 0) return false;

    const float spriteWidth = static_cast<float>(pixelWidth) / ppu;
    const float spriteHeight = static_cast<float>(pixelHeight) / ppu;
    const float fittedWidth = spriteWidth * widthFactor;
    const float fittedHeight = spriteHeight * heightFactor;
    const Vec2 pivot = renderer->pivot01.get();
    const Vec2 offset = renderer->offset.get();
    const Vec2 center{
        offset.x + (0.5f - pivot.x) * spriteWidth,
        offset.y + (0.5f - pivot.y) * spriteHeight
    };
    if (!std::isfinite(spriteWidth) || !std::isfinite(spriteHeight) ||
        !std::isfinite(fittedWidth) || !std::isfinite(fittedHeight) ||
        !std::isfinite(center.x) || !std::isfinite(center.y) ||
        fittedWidth <= 0.f || fittedHeight <= 0.f)
        return false;

    Vec2 newSize = sizeLocal_;
    Vec2 newOffset = offsetLocal_;
    float newRadius = radiusLocal_;
    switch (mode) {
    case FitMode::Both:
        newSize = {fittedWidth, fittedHeight};
        newOffset = center;
        newRadius = 0.5f * std::min(fittedWidth, fittedHeight);
        break;
    case FitMode::Width:
        newSize.x = fittedWidth;
        newOffset.x = center.x;
        newRadius = 0.5f * fittedWidth;
        break;
    case FitMode::Height:
        newSize.y = fittedHeight;
        newOffset.y = center.y;
        newRadius = 0.5f * fittedHeight;
        break;
    default:
        return false;
    }

    if (shape_ == Shape::Circle)
        radiusLocal_ = newRadius;
    else
        sizeLocal_ = newSize;
    offsetLocal_ = newOffset;
    return true;
}

Collider2D::OrientedBox2D Collider2D::WorldOBB() const noexcept
{
    const Transform* tr = mGameObject ? mGameObject->GetComponent<Transform>() : nullptr;
    if (!tr) return {};

    const Vec3 wpos = tr->position;   // world
    const Vec2 wsc = tr->scale;      // world
    const Vec3 wrot = tr->rotation;   // world (xyz), usamos Z

    // local offset -> scale -> rotate with world rotZ
    Vec2 offs = { offsetLocal_.x * wsc.x, offsetLocal_.y * wsc.y };
    offs = inheritRotation_ ? Rot2D(offs, wrot.z) : offs;

    OrientedBox2D obb;
    obb.center = { wpos.x + offs.x, wpos.y + offs.y };
    obb.half = { 0.5f * sizeLocal_.x * wsc.x, 0.5f * sizeLocal_.y * wsc.y };
    obb.angleDeg = (inheritRotation_ ? wrot.z : 0.f) + localAngleOffsetDeg_;
    return obb;
}

Collider2D::Circle2D Collider2D::WorldCircle() const noexcept
{
    const Transform* tr = mGameObject ? mGameObject->GetComponent<Transform>() : nullptr;
    if (!tr) return {};

    const Vec3 wpos = tr->position;
    const Vec2 wsc = tr->scale;
    const Vec3 wrot = tr->rotation;

    Vec2 offs = { offsetLocal_.x * wsc.x, offsetLocal_.y * wsc.y };
    // although the circle is isotropic, its offset rotates when rotation is inherited
    offs = inheritRotation_ ? Rot2D(offs, wrot.z) : offs;

    Circle2D c;
    c.center = { wpos.x + offs.x, wpos.y + offs.y };
    c.radius = radiusLocal_ * std::max(wsc.x, wsc.y);
    return c;
}

Collider2D::Capsule2D Collider2D::WorldCapsule() const noexcept
{
    const Transform* tr = mGameObject ? mGameObject->GetComponent<Transform>() : nullptr;
    if (!tr) return {};

    const Vec3 pos = tr->position;
    const Vec2 scale = tr->scale;
    const Vec3 rotation = tr->rotation;
    Vec2 offset{offsetLocal_.x * scale.x, offsetLocal_.y * scale.y};
    if (inheritRotation_) offset = Rot2D(offset, rotation.z);

    const Vec2 center{pos.x + offset.x, pos.y + offset.y};
    const float width = std::max(0.f, sizeLocal_.x * scale.x);
    const float height = std::max(0.f, sizeLocal_.y * scale.y);
    const float radius = 0.5f * std::min(width, height);
    const bool vertical = height >= width;
    const float halfSegment = 0.5f * (std::max(width, height) - 2.f * radius);
    const float angle = (inheritRotation_ ? rotation.z : 0.f) + localAngleOffsetDeg_;
    const Vec2 direction = Rot2D(vertical ? Vec2{0.f, 1.f} : Vec2{1.f, 0.f}, angle);

    return {center - direction * halfSegment, center + direction * halfSegment, radius};
}

Rect Collider2D::WorldAABB() const noexcept
{
    const Transform* tr = mGameObject ? mGameObject->GetComponent<Transform>() : nullptr;
    if (!tr) return {};

    const Vec3 wpos = tr->position;
    // use absolute values to handle negative scales
	const Vec2 wsc = tr->scale;
    const float rotZ = tr->rotation->z;

    // --- world-space center (offset scaled and optionally rotated) ---
    Vec2 offs = { offsetLocal_.x * wsc.x, offsetLocal_.y * wsc.y };
    if (inheritRotation) {
        const float rr = rotZ * 3.14159265358979323846f / 180.f;
        const float c = std::cos(rr), s = std::sin(rr);
        const float rx = c * offs.x - s * offs.y;
        const float ry = s * offs.x + c * offs.y;
        offs.x = rx; offs.y = ry;
    }
    const Vec2 center = { wpos.x + offs.x, wpos.y + offs.y };

    if (shape == Shape::Circle)
    {
        // Circle: AABB = center +/- radius (no trigonometry needed)
        const float r = radiusLocal_ * std::max(wsc.x, wsc.y);
        return { center.x - r, center.y - r, r * 2.f, r * 2.f };
    }

    if (shape == Shape::Capsule)
    {
        const auto capsule = WorldCapsule();
        const float x0 = std::min(capsule.a.x, capsule.b.x) - capsule.radius;
        const float y0 = std::min(capsule.a.y, capsule.b.y) - capsule.radius;
        const float x1 = std::max(capsule.a.x, capsule.b.x) + capsule.radius;
        const float y1 = std::max(capsule.a.y, capsule.b.y) + capsule.radius;
        return {x0, y0, x1 - x0, y1 - y0};
    }

    // Box: compute AABB extents without constructing the full OBB
    const float hx = 0.5f * sizeLocal_.x * wsc.x;
    const float hy = 0.5f * sizeLocal_.y * wsc.y;

    // final collider angle
    const float angDeg = (inheritRotation ? rotZ : 0.f) + localAngleOffsetDeg_;
    const float angRad = angDeg * 3.14159265358979323846f / 180.f;

    // Optimization: avoid trigonometry when the angle is near multiples of 90 degrees
    const auto Near = [](float x, float targetDeg) noexcept -> bool {
        return std::fabs(x - targetDeg) <= 0.001f;
    };

    float ex, ey;
    if (Near(std::fmod(std::fabs(angDeg), 360.f), 0.f) || Near(std::fmod(std::fabs(angDeg), 180.f), 0.f)) {
        // ~0 or ~180 degrees: axes do not mix
        ex = hx; ey = hy;
    }
    else if (Near(std::fmod(std::fabs(angDeg), 180.f), 90.f)) {
        // ~90 or ~270 degrees: axes are swapped
        ex = hy; ey = hx;
    }
    else {
        // General: extents rotados = |R| * half
        const float c = std::cos(angRad), s = std::sin(angRad);
        ex = std::fabs(c * hx) + std::fabs(s * hy);
        ey = std::fabs(s * hx) + std::fabs(c * hy);
    }

    return { center.x - ex, center.y - ey, ex * 2.f, ey * 2.f };
}

//std::unique_ptr<Component> Collider2D::Clone(GameObject* newOwner) const
//{
//    auto up = std::make_unique<Collider2D>();   // default constructor: Properties already rebound to *up
//    up->mGameObject = newOwner;
//
//    // --- STATE copy (data fields only, no Property<> members) ---
//    up->shape_ = shape_;
//    up->isTrigger_ = isTrigger_;
//    up->layer_ = layer_;
//    up->mask_ = mask_;
//
//    up->drawCollider_ = drawCollider_;
//    up->drawColor_ = drawColor_;
//
//    up->sizeLocal_ = sizeLocal_;
//    up->radiusLocal_ = radiusLocal_;
//    up->offsetLocal_ = offsetLocal_;
//
//    up->inheritRotation_ = inheritRotation_;
//    up->localAngleOffsetDeg_ = localAngleOffsetDeg_;
//
//    // CollisionManager registration occurs during normal Awake/OnEnable
//    return up;
//}


RigidBody2D* Collider2D::GetAttachedBody() const noexcept
{
    GameObject* go = gameObject;
    while (go)
    {
        if (auto* rb = go->GetComponent<RigidBody2D>())
            return rb;
        go = go->parent;
    }
    return nullptr;
}

void Collider2D::Awake()
{
	CollisionManager::GetInstancePtr()->RegisterCollider(this);
}

void Collider2D::OnEnable()
{
    CollisionManager::GetInstancePtr()->SetColliderActive(this, true);
}

void Collider2D::OnDisable()
{
    CollisionManager::GetInstancePtr()->SetColliderActive(this, false);
}

void Collider2D::OnDestroy()
{
	CollisionManager::GetInstancePtr()->RemoveCollider(this);
}

void Collider2D::Render()
{
    if (!drawCollider_ && !Engine::GetInstancePtr()->mShowStatsOverlay) return;

    Color gizmoColor = (CustomGizmoColor) ? drawColor_ : (isTrigger_) ? Color::Yellow() : Color::Green();

    auto* rmPtr = RenderManager::GetInstancePtr();
    if (!rmPtr) return;
    auto& r = *rmPtr;

    if (shape_ == Shape::Circle) {
        const auto c = WorldCircle();
        const int segments = 24;
        for (int i = 0; i < segments; ++i) {
            const float a0 = (float)i * (2.f * 3.14159265358979323846f / segments);
            const float a1 = (float)(i + 1) * (2.f * 3.14159265358979323846f / segments);
            const Vec2 p0{ c.center.x + std::cos(a0) * c.radius, c.center.y + std::sin(a0) * c.radius };
            const Vec2 p1{ c.center.x + std::cos(a1) * c.radius, c.center.y + std::sin(a1) * c.radius };
            r.DrawDebugLine(p0.x, p0.y, p1.x, p1.y, gizmoColor);
        }
        return;
    }

    if (shape_ == Shape::Capsule) {
        const auto capsule = WorldCapsule();
        const Vec2 segment = capsule.b - capsule.a;
        const float lengthSq = math::Dot(segment, segment);
        const Vec2 axis = (lengthSq > 1e-12f)
            ? segment * (1.f / std::sqrt(lengthSq)) : Vec2{0.f, 1.f};
        const Vec2 normal{-axis.y, axis.x};
        const float radius = capsule.radius;

        if (lengthSq > 1e-12f) {
            const Vec2 leftA = capsule.a + normal * radius;
            const Vec2 leftB = capsule.b + normal * radius;
            const Vec2 rightA = capsule.a - normal * radius;
            const Vec2 rightB = capsule.b - normal * radius;
            r.DrawDebugLine(leftA.x, leftA.y, leftB.x, leftB.y, gizmoColor);
            r.DrawDebugLine(rightA.x, rightA.y, rightB.x, rightB.y, gizmoColor);
        }

        constexpr int segments = 16;
        constexpr float pi = 3.14159265358979323846f;
        auto drawArc = [&](Vec2 center, bool forward) {
            const Vec2 side = forward ? normal : -normal;
            const Vec2 outward = forward ? axis : -axis;
            for (int i = 0; i < segments; ++i) {
                const float a0 = pi * static_cast<float>(i) / segments;
                const float a1 = pi * static_cast<float>(i + 1) / segments;
                const Vec2 p0 = center + (side * std::cos(a0) + outward * std::sin(a0)) * radius;
                const Vec2 p1 = center + (side * std::cos(a1) + outward * std::sin(a1)) * radius;
                r.DrawDebugLine(p0.x, p0.y, p1.x, p1.y, gizmoColor);
            }
        };
        drawArc(capsule.b, true);
        drawArc(capsule.a, false);
        return;
    }

    // Box
    const auto obb = WorldOBB();
    const float a = DegToRad(obb.angleDeg);
    const float c = std::cos(a), s = std::sin(a);
    const Vec2 h = obb.half;

    Vec2 corners[4] = {
        {-h.x, -h.y}, { h.x, -h.y},
        { h.x,  h.y}, {-h.x,  h.y}
    };
    for (auto& v : corners) {
        // rotate each vertex and translate it to the center
        const float rx = c * v.x - s * v.y;
        const float ry = s * v.x + c * v.y;
        v.x = rx + obb.center.x;
        v.y = ry + obb.center.y;
    }

    r.DrawDebugLine(corners[0].x, corners[0].y, corners[1].x, corners[1].y, gizmoColor);
    r.DrawDebugLine(corners[1].x, corners[1].y, corners[2].x, corners[2].y, gizmoColor);
    r.DrawDebugLine(corners[2].x, corners[2].y, corners[3].x, corners[3].y, gizmoColor);
    r.DrawDebugLine(corners[3].x, corners[3].y, corners[0].x, corners[0].y, gizmoColor);
}

void Collider2D::Register()
{
    if (mAwoken && mStarted)
    {
        CollisionManager::GetInstancePtr()->RegisterCollider(this);
        CollisionManager::GetInstancePtr()->SetColliderActive(this, mEnabled && gameObject->activeInHierarchy);
    }
}

#include "Collider2D.h"

#include <cmath>
#include <algorithm>

#include "Transform.h"
#include "CollisionManager.h"
#include "RenderManager.h"
#include "GameObject.h"
#include "RigidBody2D.h"


static inline float DegToRad(float d) noexcept { return d * 3.14159265358979323846f / 180.f; }
static inline Vec2 Rot2D(Vec2 v, float deg) noexcept {
    const float r = DegToRad(deg);
    const float c = std::cos(r);
    const float s = std::sin(r);
    return { c * v.x - s * v.y, s * v.x + c * v.y };
}

Collider2D::OrientedBox2D Collider2D::WorldOBB() const noexcept
{
    const Transform* tr = mGameObject ? mGameObject->GetComponent<Transform>() : nullptr;
    if (!tr) return {};

    const Vec3 wpos = tr->position;   // world
    const Vec2 wsc = tr->scale;      // world
    const Vec3 wrot = tr->rotation;   // world (xyz), usamos Z

    // offset local -> escalar -> rotar con rotZ mundo
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
    // aunque el círculo es isotrópico, el offset sí rota con el objeto si hereda
    offs = inheritRotation_ ? Rot2D(offs, wrot.z) : offs;

    Circle2D c;
    c.center = { wpos.x + offs.x, wpos.y + offs.y };
    c.radius = radiusLocal_ * std::max(wsc.x, wsc.y);
    return c;
}

Rect Collider2D::WorldAABB() const noexcept
{
    const Transform* tr = mGameObject ? mGameObject->GetComponent<Transform>() : nullptr;
    if (!tr) return {};

    const Vec3 wpos = tr->position;
    // ojo: usa módulo por si hay escalas negativas
	const Vec2 wsc = tr->scale;
    const float rotZ = tr->rotation->z;

    // --- centro en mundo (offset escalado y, si procede, rotado) ---
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
        // Círculo: AABB = centro ± radio (no necesitas trig)
        const float r = radiusLocal_ * std::max(wsc.x, wsc.y);
        return { center.x - r, center.y - r, r * 2.f, r * 2.f };
    }

    // Box: calcula “extents” AABB sin construir el OBB completo
    const float hx = 0.5f * sizeLocal_.x * wsc.x;
    const float hy = 0.5f * sizeLocal_.y * wsc.y;

    // ángulo final del collider
    const float angDeg = (inheritRotation ? rotZ : 0.f) + localAngleOffsetDeg_;
    const float angRad = angDeg * 3.14159265358979323846f / 180.f;

    // Micro-optimización: si el ángulo está muy cerca de múltiplos de 90°, evita trig
    const auto Near = [](float x, float targetDeg) noexcept -> bool {
        return std::fabs(x - targetDeg) <= 0.001f;
    };

    float ex, ey;
    if (Near(std::fmod(std::fabs(angDeg), 360.f), 0.f) || Near(std::fmod(std::fabs(angDeg), 180.f), 0.f)) {
        // ~0° o ~180° -> no hay mezcla de ejes
        ex = hx; ey = hy;
    }
    else if (Near(std::fmod(std::fabs(angDeg), 180.f), 90.f)) {
        // ~90° o ~270° -> ejes intercambiados
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
//    auto up = std::make_unique<Collider2D>();   // ctor por defecto Properties ya “rebinded” a *up
//    up->mGameObject = newOwner;
//
//    // --- Copia de ESTADO (solo campos de datos, nada de Property<>) ---
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
//    // El registro en CollisionManager ocurrirá en Awake/OnEnable del ciclo normal
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
        // rota cada vértice y traslada a centro
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

#include "Transform.h"

#include <cmath>
#include <algorithm>

#include "GameObject.h"

static inline float DegToRad(float d) noexcept { return d * 3.14159265358979323846f / 180.f; }

// Rotate in the XY plane by the given angle (degrees) around Z.
static inline void Rot2D(float& x, float& y, float degZ) noexcept {
    const float r = DegToRad(degZ);
    const float c = std::cos(r);
    const float s = std::sin(r);
    const float nx = c * x - s * y;
    const float ny = s * x + c * y;
    x = nx; y = ny;
}

void Transform::SetLocalPosition(Vec3 newPos) noexcept {
    localX = newPos.x; localY = newPos.y; localZ = newPos.z;
    MarkDirty();
}

void Transform::SetLocalScale(Vec2 newSc) noexcept {
    if (newSc.x < 0 || newSc.y < 0) return;
    localSX = newSc.x; localSY = newSc.y;
    MarkDirty();
}

void Transform::SetLocalRotation(Vec3 newRot) noexcept {
    localRotX = newRot.x;
    localRotY = newRot.y;
    localRotZ = newRot.z;
    MarkDirty();
}

void Transform::SetWorldPosition(Vec3 newPos) noexcept {
    float x = newPos.x;
    float y = newPos.y;
    float z = newPos.z;

    GameObject* parentGO = (mGameObject ? mGameObject->Parent() : nullptr);
    Transform* pt = (parentGO ? parentGO->GetComponent<Transform>() : nullptr);

    if (!pt) {
        // Sin padre: local == world
        localX = x; localY = y; localZ = z;
        MarkDirty();
        return;
    }

    // Ensure the parent world transform is up to date
    pt->RecalcWorld_();

    // offset from the parent's world space
    float dx = x - pt->mWorldX;
    float dy = y - pt->mWorldY;

    // Remove parent rotation (use Z in 2D)
    Rot2D(dx, dy, -pt->mWorldRotZ);

    // Remove parent scale
    const float psx = (std::abs(pt->mWorldSX) > 1e-8f ? pt->mWorldSX : 1.f);
    const float psy = (std::abs(pt->mWorldSY) > 1e-8f ? pt->mWorldSY : 1.f);

    localX = dx / psx;
    localY = dy / psy;
    // In 2D, Z only translates (no rotation/scaling)
    localZ = z - pt->mWorldZ;

    MarkDirty();
}

void Transform::SetWorldRotation(Vec3 newRot) noexcept {
    GameObject* parentGO = (mGameObject ? mGameObject->Parent() : nullptr);
    Transform* pt = (parentGO ? parentGO->GetComponent<Transform>() : nullptr);

    if (!pt) {
        localRotX = newRot.x;
        localRotY = newRot.y;
        localRotZ = newRot.z;
    }
    else {
        pt->RecalcWorld_();
        localRotX = newRot.x - pt->mWorldRotX;
        localRotY = newRot.y - pt->mWorldRotY;
        localRotZ = newRot.z - pt->mWorldRotZ;
    }
    MarkDirty();
}

void Transform::SetWorldScale(Vec2 newSc) noexcept {
    if (newSc.x < 0 || newSc.y < 0) return;

    GameObject* parentGO = (mGameObject ? mGameObject->Parent() : nullptr);
    Transform* pt = (parentGO ? parentGO->GetComponent<Transform>() : nullptr);

    if (!pt) {
        localSX = newSc.x; localSY = newSc.y;
    }
    else {
        pt->RecalcWorld_();
        const float psx = (std::abs(pt->mWorldSX) > 1e-8f ? pt->mWorldSX : 1.f);
        const float psy = (std::abs(pt->mWorldSY) > 1e-8f ? pt->mWorldSY : 1.f);
        localSX = newSc.x / psx;
        localSY = newSc.y / psy;
    }
    MarkDirty();
}

Vec3 Transform::GetWorldPosition() const noexcept {
    RecalcWorld_();
    return { mWorldX, mWorldY, mWorldZ };
}

Vec3 Transform::GetWorldRotation() const noexcept {
    RecalcWorld_();
    return { mWorldRotX, mWorldRotY, mWorldRotZ };
}

Vec2 Transform::GetWorldScale() const noexcept {
    RecalcWorld_();
    return { mWorldSX, mWorldSY };
}

Vec3 Transform::GetLocalPosition() const noexcept {
    return { localX, localY, localZ };
}

Vec3 Transform::GetLocalRotation() const noexcept {
    return { localRotX, localRotY, localRotZ };
}

Vec2 Transform::GetLocalScale() const noexcept {
    return { localSX, localSY };
}

void Transform::MarkDirty() noexcept {
    mDirty = true;
    // Propagate to children so their world transforms are recalculated when accessed
    if (!mGameObject) return;
    for (GameObject* ch : mGameObject->Children()) {
        if (!ch) continue;
        if (auto* t = ch->GetComponent<Transform>()) {
            // Llamar recursivamente
            t->MarkDirty();
        }
    }
}

Vec3 Transform::GetRight() const noexcept
{
    RecalcWorld_();
    Vec2 vec2 = Rotate({ 1.f, 0.f }, mWorldRotZ);
    return Vec3(vec2.x, vec2.y, 0.f);
}

Vec3 Transform::GetUp() const noexcept
{
    RecalcWorld_();
    Vec2 vec2 = Rotate({ 0.f, 1.f }, mWorldRotZ);
    return Vec3(vec2.x, vec2.y, 0.f);
}

void Transform::Translate(const Vec3& delta) noexcept
{
    const Vec3 current = GetWorldPosition();
    SetWorldPosition({current.x + delta.x, current.y + delta.y,
                      current.z + delta.z});
}

static inline float RadToDeg(float r) noexcept { return r * 180.f / 3.14159265358979323846f; }

void Transform::LookAt(const Vec2& worldTarget, float offsetDeg) noexcept
{
    // Current world-space position
    const Vec3 p = GetWorldPosition();

    const float dx = worldTarget.x - p.x;
    const float dy = worldTarget.y - p.y;

    // Avoid NaN when the target is at the same position
    const float len2 = dx * dx + dy * dy;
    if (len2 < 1e-12f) return;

    float zDeg = RadToDeg(std::atan2(dy, dx)) + offsetDeg;

    // Keep X/Y (typically zero in 2D)
    Vec3 r = GetWorldRotation();
    r.z = zDeg;
    SetWorldRotation(r);
}

void Transform::RecalcWorld_() const noexcept {
    if (!mDirty) return;

    // Datos locales
    float wX = localX, wY = localY, wZ = localZ;
    float wSX = localSX, wSY = localSY;
    float wRotX = localRotX, wRotY = localRotY, wRotZ = localRotZ;

    // Compose with the parent if present
    GameObject* parentGO = (mGameObject ? mGameObject->Parent() : nullptr);
    Transform* pt = (parentGO ? parentGO->GetComponent<Transform>() : nullptr);

    if (pt) {
        // Ensure the parent transform has been recalculated
        pt->RecalcWorld_();

        // Escala mundo = escala padre * escala local (componente a componente)
        wSX = pt->mWorldSX * wSX;
        wSY = pt->mWorldSY * wSY;

        // World rotation = parent rotation + local rotation (per axis)
        wRotX = pt->mWorldRotX + wRotX;
        wRotY = pt->mWorldRotY + wRotY;
        wRotZ = pt->mWorldRotZ + wRotZ;

        // Position: T_parent + Rz_parent( S_parent( local_position ) )
        float px = localX * pt->mWorldSX;
        float py = localY * pt->mWorldSY;
        Rot2D(px, py, pt->mWorldRotZ);   // in 2D, only Z affects XY

        wX = pt->mWorldX + px;
        wY = pt->mWorldY + py;
        wZ = pt->mWorldZ + localZ;       // Z only translates in this 2D model
    }

    // Cachear
    mWorldX = wX; mWorldY = wY; mWorldZ = wZ;
    mWorldSX = wSX; mWorldSY = wSY;
    mWorldRotX = wRotX; mWorldRotY = wRotY; mWorldRotZ = wRotZ;

    mDirty = false;
}

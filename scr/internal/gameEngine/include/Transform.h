#pragma once

#include <memory>

#include "Behaviour.h"
#include "BaseTypes.h"
#include "GameObject.h"

class Transform final : public Component
{
	friend class GameObject;

private:
    // Local (relativo al padre)
    float localX = 0.f, localY = 0.f, localZ = 0.f;
    float localSX = 1.f, localSY = 1.f;
    float localRotX = 0.f, localRotY = 0.f, localRotZ = 0.f;

    // Accesores world (calculados)
    // Implementación: compón desde la cadena de padres (acumula S/R/T)
    // y cachea con "dirty" para no recomputar cada vez.
    void  SetLocalPosition(Vec3 newPos) noexcept;
    void  SetLocalScale(Vec2 newSc) noexcept;
    void  SetLocalRotation(Vec3 newRot) noexcept;
    // Establece pose mundo; internamente calcula local en base al padre
    void  SetWorldPosition(Vec3 newPos) noexcept;
    void  SetWorldScale(Vec2 newSc) noexcept;
    void  SetWorldRotation(Vec3 newRot) noexcept;
    
    // Getters privados
    Vec3  GetLocalPosition() const noexcept;
    Vec2  GetLocalScale() const noexcept;
    Vec3  GetLocalRotation() const noexcept;
    // Lecturas mundo
    Vec3  GetWorldPosition() const noexcept;
    Vec2  GetWorldScale() const noexcept;
    Vec3 GetWorldRotation() const noexcept;

    mutable bool mDirty = true;
    // Cache mundo
    mutable float mWorldX = 0.f, mWorldY = 0.f, mWorldZ = 0.f;
    mutable float mWorldSX = 1.f, mWorldSY = 1.f;
    mutable float mWorldRotX = 0.f, mWorldRotY = 0.f, mWorldRotZ = 0.f;
    // Recalcular si dirty (acumula padre->hijo)
    void RecalcWorld_() const noexcept;

    // Dirty flag
    void  MarkDirty() noexcept;

    // Ciclo
    void FixedUpdate(float) override {}
    void Update(float) override {}
    void Render() override {}

    static constexpr bool kUnique = true;

    Transform(const Transform&) = delete;
    Transform& operator=(const Transform&) = delete;
    Transform(Transform&&) = delete;
    Transform& operator=(Transform&&) = delete;

    static inline Vec2 Rotate(const Vec2& v, float deg) noexcept
    {
        const float rad = deg * 3.14159265f / 180.f;
        const float c = std::cos(rad);
        const float s = std::sin(rad);
        return Vec2{
            v.x * c - v.y * s,
            v.x * s + v.y * c
        };
    }

    Vec3 GetRight() const noexcept;
    Vec3 GetUp() const noexcept;

protected:

    // Componente unico
    bool IsUnique() const noexcept override { return kUnique; }

public:
    Transform() = default;
    ~Transform() override = default;

    void Translate(const Vec3& delta) noexcept;
    void LookAt(const Vec2& target, float offsetDeg = 0.f) noexcept;

    // Propiedades, acceso publico

    // position {get; set;} como en unity
    using PositionProp = Property<Transform, Vec3,
        &Transform::GetWorldPosition,
        &Transform::SetWorldPosition>;

    PositionProp position{ this };

    using LocalPositionProp = Property<Transform, Vec3,
        &Transform::GetLocalPosition,
        &Transform::SetLocalPosition>;

    LocalPositionProp localPosition{ this };

    // scale {get; set;} como en unity
    using ScaleProp = Property<Transform, Vec2,
        &Transform::GetWorldScale,
        &Transform::SetWorldScale>;

    ScaleProp scale{ this };

    using LocalScaleProp = Property<Transform, Vec2,
        &Transform::GetLocalScale,
        &Transform::SetLocalScale>;

    LocalScaleProp localScale{ this };

    // scale {get; set;} como en unity
    using RotationProp = Property<Transform, Vec3,
        &Transform::GetWorldRotation,
        &Transform::SetWorldRotation>;

    RotationProp rotation{ this };

    using LocalRotationProp = Property<Transform, Vec3,
        &Transform::GetLocalRotation,
        &Transform::SetLocalRotation>;

    LocalRotationProp localRotation{ this };

    using UpProp = PropertyRO<Transform, Vec3, &Transform::GetUp>;
    UpProp up{ this };

    using RightProp = PropertyRO<Transform, Vec3, &Transform::GetRight>;
    RightProp right{ this };
};
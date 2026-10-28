#pragma once
/** @file Transform.h
 * @brief Local and world position, scale, orientation and hierarchy transforms.
 */

#include <memory>

#include "Behaviour.h"
#include "BaseTypes.h"
#include "GameObject.h"

/** @brief Exposes world/local position, rotation and scale using owner-bound properties.
 * @ingroup xengine_math
 */
class Transform final : public Component
{
	friend class GameObject;

private:
    // Local (relative to parent)
    float localX = 0.f, localY = 0.f, localZ = 0.f;
    float localSX = 1.f, localSY = 1.f;
    float localRotX = 0.f, localRotY = 0.f, localRotZ = 0.f;

    // Accesores world (calculados)
    // Implementation: compose along the parent chain (accumulate scale/rotation/translation)
    // Cache using a dirty flag to avoid unnecessary recalculations.
    void  SetLocalPosition(Vec3 newPos) noexcept;
    void  SetLocalScale(Vec2 newSc) noexcept;
    void  SetLocalRotation(Vec3 newRot) noexcept;
    // Sets world pose; calculates local pose relative to the parent
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
    // Recalculate when dirty (accumulate parent-to-child transforms)
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
    /** @brief Reports whether unique is currently true.
     */
    bool IsUnique() const noexcept override { return kUnique; }

public:
    /** @brief Constructs a Transform instance.
     */
    Transform() = default;
    /** @brief Releases the Transform instance and any owned resources.
     */
    ~Transform() override = default;

    /** @brief Applies an additional translation to this transform.
     * @param delta Change applied to the current value.
     */
    void Translate(const Vec3& delta) noexcept;
    /** @brief Orients the transform to face the supplied target.
     * @param target Target point, vector or transform for this operation.
     * @param offsetDeg Additional rotation in degrees.
     */
    void LookAt(const Vec2& target, float offsetDeg = 0.f) noexcept;

    // Propiedades, acceso publico

    // position {get; set;} como en unity
    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `position`; use the corresponding instance member in gameplay code.
     */
    using PositionProp = Property<Transform, Vec3,
        &Transform::GetWorldPosition,
        &Transform::SetWorldPosition>;
    /** @endcond */

    /** @brief World-space position, accounting for the parent transform.
     */
    PositionProp position{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `local position`; use the corresponding instance member in gameplay code.
     */
    using LocalPositionProp = Property<Transform, Vec3,
        &Transform::GetLocalPosition,
        &Transform::SetLocalPosition>;
    /** @endcond */

    /** @brief Position relative to the parent transform.
     */
    LocalPositionProp localPosition{ this };

    // scale {get; set;} como en unity
    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `scale`; use the corresponding instance member in gameplay code.
     */
    using ScaleProp = Property<Transform, Vec2,
        &Transform::GetWorldScale,
        &Transform::SetWorldScale>;
    /** @endcond */

    /** @brief World-space scale, accounting for the parent transform.
     */
    ScaleProp scale{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `local scale`; use the corresponding instance member in gameplay code.
     */
    using LocalScaleProp = Property<Transform, Vec2,
        &Transform::GetLocalScale,
        &Transform::SetLocalScale>;
    /** @endcond */

    /** @brief Scale relative to the parent transform.
     */
    LocalScaleProp localScale{ this };

    // scale {get; set;} como en unity
    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `rotation`; use the corresponding instance member in gameplay code.
     */
    using RotationProp = Property<Transform, Vec3,
        &Transform::GetWorldRotation,
        &Transform::SetWorldRotation>;
    /** @endcond */

    /** @brief World-space rotation of the object, in degrees.
     */
    RotationProp rotation{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `local rotation`; use the corresponding instance member in gameplay code.
     */
    using LocalRotationProp = Property<Transform, Vec3,
        &Transform::GetLocalRotation,
        &Transform::SetLocalRotation>;
    /** @endcond */

    /** @brief Rotation relative to the parent, in degrees.
     */
    LocalRotationProp localRotation{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `up`; use the corresponding instance member in gameplay code.
     */
    using UpProp = PropertyRO<Transform, Vec3, &Transform::GetUp>;
    /** @endcond */
    /** @brief Read-only world-space up direction of this transform.
     */
    UpProp up{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `right`; use the corresponding instance member in gameplay code.
     */
    using RightProp = PropertyRO<Transform, Vec3, &Transform::GetRight>;
    /** @endcond */
    /** @brief Read-only world-space right direction of this transform.
     */
    RightProp right{ this };
};
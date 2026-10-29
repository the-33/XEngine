#pragma once
/** @file Transform.h
 * @brief Local and world position, scale, orientation and hierarchy transforms.
 */

#include <memory>

#include "Behaviour.h"
#include "BaseTypes.h"
#include "GameObject.h"

/** @brief Stores a 2D-oriented transform with parent-relative and composed world properties.
 *
 * XY positions are scaled and rotated by the parent's Z rotation; Z position
 * is translated only. World X/Y rotation components are added through the
 * parent chain but do not rotate XY geometry. Scales must be nonnegative.
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

    /** @brief Moves the transform by a world-space displacement.
     * @param delta World-space XYZ increment.
     * @note Recomputes local coordinates relative to the current parent and
     *       invalidates cached world transforms of descendants.
     */
    void Translate(const Vec3& delta) noexcept;
    /** @brief Points local +X toward a world-space target in the XY plane.
     * @param target Target world XY position. Equal current/target positions do nothing.
     * @param offsetDeg Additional Z-axis angle in degrees.
     * @note Preserves X/Y rotation and sets world Z rotation via `atan2`.
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

    /** @brief World-space XY scale, multiplied component-wise through parents.
     * @note Negative assignments are ignored. Near-zero parent scale makes
     *       world-to-local conversion approximate.
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

    /** @brief XYZ Euler angles relative to the parent, in degrees.
     * @note Only Z rotates 2D positions and sprites in this implementation.
     */
    LocalRotationProp localRotation{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `up`; use the corresponding instance member in gameplay code.
     */
    using UpProp = PropertyRO<Transform, Vec3, &Transform::GetUp>;
    /** @endcond */
    /** @brief Positive Y axis rotated by up-to-date world Z rotation.
     */
    UpProp up{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `right`; use the corresponding instance member in gameplay code.
     */
    using RightProp = PropertyRO<Transform, Vec3, &Transform::GetRight>;
    /** @endcond */
    /** @brief Positive X axis rotated by up-to-date world Z rotation.
     */
    RightProp right{ this };
};

#pragma once
/** @file Collider2D.h
 * @brief Two-dimensional collision shapes, triggers and layer masks.
 */

#include "Behaviour.h"
#include "BaseTypes.h"
#include "GameObject.h"

class CollisionManager;
class RigidBody2D;
class PhysicsManager;

/** @brief Public type alias for the layer bits accessor.
 */
using LayerBits = uint32_t;

/** @brief Describes a two-dimensional collision shape and trigger/layer settings.
 * @ingroup xengine_physics
 */
class Collider2D : public Behaviour
{
	friend class CollisionManager;
	friend class PhysicsManager;

public:
    /** @brief Available shape modes for Collider2D.
     */
    enum class Shape : uint8_t {
        Box,   ///< Rectangular collider using local width/height dimensions.
        Circle ///< Circular collider using its local radius.
    };

private:
    friend class CollisionManager;
	friend class GameObject;

    // --- General configuration ---
    Shape      shape_ = Shape::Box;
    bool       isTrigger_ = false;
    LayerBits  layer_ = 1u << 0;
    LayerBits  mask_ = 0xFFFFFFFFu;

    bool drawCollider_ = false;
    Color drawColor_ = { 0, 255, 0, 100 };
    bool CustomGizmoColor = false;

    // --- LOCAL-SPACE geometry (world units BEFORE scaling/rotation) ---
    // For Box: sizeLocal = {w, h}. For Circle: use radiusLocal.
    Vec2      sizeLocal_{ 1.f, 1.f };
    float      radiusLocal_ = 0.5f;

    // LOCAL-SPACE offset (scaled and rotated with the Transform).
    Vec2      offsetLocal_{ 0.f, 0.f };

    // Rotation: by default the collider inherits the Transform rotation.
    // An additional angular offset can be applied.
    bool       inheritRotation_ = true;
    float      localAngleOffsetDeg_ = 0.f;

    void SetLayer(LayerBits l) noexcept { layer_ = l; }
    void SetMask(LayerBits m) noexcept { mask_ = m; }
    void SetOffsetLocal(Vec2 off) noexcept { offsetLocal_ = off; }
	void SetInheritRotation(bool inherit) noexcept { inheritRotation_ = inherit; }
	void SetLocalAngleOffsetDeg(float angleDeg) noexcept { localAngleOffsetDeg_ = angleDeg; }
	void SetIsTrigger(bool trigger) noexcept { isTrigger_ = trigger; }
	void SetShape(Shape s) noexcept { shape_ = s; }
	void SetRadiusLocal(float r) noexcept { radiusLocal_ = r; }
	void SetSizeLocal(Vec2 s) noexcept { sizeLocal_ = s; }
    void SetDrawCollider(bool active) noexcept { drawCollider_ = active; }
    void SetDrawColor(const Color color) noexcept { drawColor_ = color; CustomGizmoColor = true; }

	LayerBits GetLayer() const noexcept { return layer_; }
    LayerBits GetMask() const noexcept { return mask_; }
    bool      GetIsTrigger() const noexcept { return isTrigger_; }
    Vec2     GetOffsetLocal() const noexcept { return offsetLocal_; }
    bool      GetInheritRotation() const noexcept { return inheritRotation_; }
    float     GetLocalAngleOffsetDeg() const noexcept { return localAngleOffsetDeg_; }
    Shape     GetShape() const noexcept { return shape_; }
    float     GetRadiusLocal() const noexcept { return radiusLocal_; }
    Vec2     GetSizeLocal() const noexcept { return sizeLocal_; }
    bool      GetDrawCollider() const noexcept { return drawCollider_; }
	Color     GetDrawColor() const noexcept { return drawColor_; }

    RigidBody2D* GetAttachedBody() const noexcept;

protected:
    // Lifecycle (no additional logic)
    /** @brief Lifecycle hook for awake; the engine controls invocation order.
     */
    void Awake() override;        // registrar collider
    /** @brief Lifecycle callback invoked for enable.
     */
    void OnEnable() override;     // enable collision
    /** @brief Lifecycle callback invoked for disable.
     */
    void OnDisable() override;    // desactivar
    /** @brief Lifecycle callback invoked for destroy.
     */
    void OnDestroy() override;    // limpiar registro
    /** @brief Lifecycle hook for render; the engine controls invocation order.
     */
    void Render() override;       // opcional debug

    /** @brief Registers this component type with the engine component system.
     */
    void Register();

    // ====== WORLD-SPACE queries (apply Transform position, scale and rotation) ======
    /** @brief Developer-visible value or service type OrientedBox2D.
     */
    struct OrientedBox2D {
        /** @brief Box center in world space.
         */
        Vec2 center;     // centro en mundo
        /** @brief Half-width and half-height along its local axes.
         */
        Vec2 half;       // semi-anchos en mundo
        /** @brief Orientation of the box, in degrees.
         */
        float angleDeg;   // angle (degrees, counterclockwise)
    };
    /** @brief Developer-visible value or service type Circle2D.
     */
    struct Circle2D {
        /** @brief Circle center in world space.
         */
        Vec2 center;
        /** @brief Circle radius in world units.
         */
        float radius;
    };

    /** @brief Calculates the collider’s oriented box in world coordinates.
     */
    OrientedBox2D WorldOBB() const noexcept;     // valid when shape == Box
    /** @brief Calculates the collider’s circle in world coordinates.
     */
    Circle2D      WorldCircle() const noexcept;  // valid when shape == Circle

    // World-space shape AABB (useful for broad-phase collision checks)
    /** @brief Calculates the axis-aligned world bounds of the collider.
     */
    Rect          WorldAABB() const noexcept;

    //std::unique_ptr<Component> Clone(GameObject* newOwner) const override;

public:
    /** @brief Constructs a Collider2D instance.
     */
    Collider2D() = default;
    /** @brief Releases the Collider2D instance and any owned resources.
     */
    ~Collider2D() override = default;

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `shape`; use the corresponding instance member in gameplay code.
     */
    using ShapeProp = Property<Collider2D, Shape,
        &Collider2D::GetShape,
		&Collider2D::SetShape>;
    /** @endcond */
    /** @brief Collider geometry used for overlap and contact tests.
     */
    ShapeProp shape{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `is trigger`; use the corresponding instance member in gameplay code.
     */
    using IsTriggerProp = Property<Collider2D, bool,
		&Collider2D::GetIsTrigger,
        &Collider2D::SetIsTrigger>;
    /** @endcond */
    /** @brief Whether this collider reports overlaps without physical collision response.
     */
    IsTriggerProp isTrigger{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `layer`; use the corresponding instance member in gameplay code.
     */
    using LayerProp = Property<Collider2D, LayerBits,
        &Collider2D::GetLayer,
		&Collider2D::SetLayer>;
    /** @endcond */
    /** @brief Collision layer assigned to this collider.
     */
    LayerProp layer{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `mask`; use the corresponding instance member in gameplay code.
     */
    using MaskProp = Property<Collider2D, LayerBits,
		&Collider2D::GetMask,
        &Collider2D::SetMask>;
    /** @endcond */
	/** @brief Bit mask specifying which collision layers may interact.
	 */
	MaskProp mask{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `draw collider`; use the corresponding instance member in gameplay code.
     */
    using DrawColliderProp = Property<Collider2D, bool,
        &Collider2D::GetDrawCollider,
		&Collider2D::SetDrawCollider>;
    /** @endcond */
    /** @brief Enable debug visualization of the collider geometry.
     */
    DrawColliderProp showGizmo{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `draw color`; use the corresponding instance member in gameplay code.
     */
    using DrawColorProp = Property<Collider2D, Color,
        &Collider2D::GetDrawColor,
		&Collider2D::SetDrawColor>;
    /** @endcond */
    /** @brief Color of the collider when debug visualization is enabled.
     */
    DrawColorProp gizmoColor{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `offset local`; use the corresponding instance member in gameplay code.
     */
    using OffsetLocalProp = Property<Collider2D, Vec2,
		&Collider2D::GetOffsetLocal,
		&Collider2D::SetOffsetLocal>;
    /** @endcond */
	/** @brief Collider center offset relative to its owner’s transform.
	 */
	OffsetLocalProp localOffset{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `inherit rotation`; use the corresponding instance member in gameplay code.
     */
    using InheritRotationProp = Property<Collider2D, bool,
        &Collider2D::GetInheritRotation,
		&Collider2D::SetInheritRotation>;
    /** @endcond */
	/** @brief Whether the collider inherits rotation from its transform.
	 */
	InheritRotationProp inheritRotation{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `local angle offset deg`; use the corresponding instance member in gameplay code.
     */
    using LocalAngleOffsetDegProp = Property<Collider2D, float,
        &Collider2D::GetLocalAngleOffsetDeg,
		&Collider2D::SetLocalAngleOffsetDeg>;
    /** @endcond */
    /** @brief Additional collider rotation, in degrees.
     */
    LocalAngleOffsetDegProp rotationOffset{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `size local`; use the corresponding instance member in gameplay code.
     */
    using SizeLocalProp = Property<Collider2D, Vec2,
		&Collider2D::GetSizeLocal,
		&Collider2D::SetSizeLocal>;
    /** @endcond */
    /** @brief Size of rectangular collider geometry in local world units.
     */
    SizeLocalProp size{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `radius local`; use the corresponding instance member in gameplay code.
     */
    using RadiusLocalProp = Property<Collider2D, float,
        &Collider2D::GetRadiusLocal,
        &Collider2D::SetRadiusLocal>;
    /** @endcond */
	/** @brief Radius of circular collider geometry in world units.
	 */
	RadiusLocalProp radius{ this };
};

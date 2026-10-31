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

/** @brief Bit field for collision layers and interaction masks.
 */
using LayerBits = uint32_t;

/** @brief Box/circle collision geometry attached to a GameObject and its Transform.
 *
 * Shape sizes and offsets are local; the current world scale applies. A circle
 * uses the larger world XY scale component for its radius. Capsules use the
 * size property as the total width and height, including their rounded ends.
 * A collider can share
 * the first Rigidbody2D found on its owner or an ancestor.
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
        Box,     ///< Rectangular collider using local width/height dimensions.
        Circle,  ///< Circular collider using its local radius.
        Capsule  ///< Rounded collider using total local width and height.
    };

    /** @brief Which sprite dimensions are fitted to the collider.
     * @note Width and Height preserve the other collider dimension and offset.
     */
    enum class FitMode : uint8_t {
        Both,   ///< Fit width and height independently.
        Width,  ///< Fit only width (or the circle diameter).
        Height  ///< Fit only height (or the circle diameter).
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
    /** @brief World-space capsule: the line between two circular cap centers and its radius.
     * @note A zero-length center segment is equivalent to a circle.
     */
    struct Capsule2D {
        Vec2 a{}; ///< Center of the first rounded end.
        Vec2 b{}; ///< Center of the second rounded end.
        float radius = 0.f; ///< World-space radius of both rounded ends.
    };

    /** @brief Calculates the collider’s oriented box in world coordinates.
     */
    OrientedBox2D WorldOBB() const noexcept;     // valid when shape == Box
    /** @brief Calculates the collider’s circle in world coordinates.
     */
    Circle2D      WorldCircle() const noexcept;  // valid when shape == Circle

    /** @brief Builds the capsule from total local size, world scale and Z rotation.
     * @note The longer scaled dimension determines its longitudinal axis;
     *       nonuniform scale still produces circular, not elliptical, end caps.
     */
    Capsule2D WorldCapsule() const noexcept; // valid when shape == Capsule

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

    /** @brief Fits this collider to the SpriteRenderer on the same GameObject.
     * @param mode Fit the sprite's width, height, or both dimensions.
     * @param factor Size multiplier (1.0 = 100%, 0.95 = 95%).
     * @return False and leaves geometry unchanged if no valid sprite exists or the factor is invalid.
     * @note Uses the selected sprite source region and pixels-per-unit, accounting
     *       for its offset and pivot. For Circle, Both fits the smaller diameter.
     *       This is a one-time fit; call again if sprite settings change.
     */
    bool FitToSprite(FitMode mode = FitMode::Both, float factor = 1.f) noexcept;

    /** @brief Fits selected axes with independent size multipliers.
     * @param mode Which axes to fit; untouched axes retain their original geometry.
     * @param widthFactor Multiplier for sprite width (1.0 = 100%).
     * @param heightFactor Multiplier for sprite height (1.0 = 100%).
     * @return False if the sprite/factors are invalid, without changing the collider.
     * @note For Circle, Both chooses the smaller fitted diameter.
     */
    bool FitToSprite(FitMode mode, float widthFactor, float heightFactor) noexcept;

    /** @brief Fits both axes using one size multiplier (e.g. 0.9 = 90%). */
    bool FitToSprite(float factor) noexcept { return FitToSprite(FitMode::Both, factor); }

    /** @brief Fits width and height independently (e.g. 0.9, 0.8). */
    bool FitToSprite(float widthFactor, float heightFactor) noexcept {
        return FitToSprite(FitMode::Both, widthFactor, heightFactor);
    }

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
    /** @brief Layer bit field assigned to this collider (default bit 0).
     */
    LayerProp layer{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `mask`; use the corresponding instance member in gameplay code.
     */
    using MaskProp = Property<Collider2D, LayerBits,
		&Collider2D::GetMask,
        &Collider2D::SetMask>;
    /** @endcond */
	/** @brief Bit mask of layers this collider accepts (default all bits).
	 * @note A pair is tested only if both `a.layer & b.mask` and
	 *       `b.layer & a.mask` are nonzero; colliders on the same body are ignored.
	 */
	MaskProp mask{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `draw collider`; use the corresponding instance member in gameplay code.
     */
    using DrawColliderProp = Property<Collider2D, bool,
        &Collider2D::GetDrawCollider,
		&Collider2D::SetDrawCollider>;
    /** @endcond */
    /** @brief Enable debug geometry rendering for this collider.
     * @note The engine statistics overlay can also force collider gizmos visible.
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
	/** @brief Local XY collider offset, scaled and optionally rotated by the Transform.
	 */
	OffsetLocalProp localOffset{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `inherit rotation`; use the corresponding instance member in gameplay code.
     */
    using InheritRotationProp = Property<Collider2D, bool,
        &Collider2D::GetInheritRotation,
		&Collider2D::SetInheritRotation>;
    /** @endcond */
	/** @brief Whether the collider follows Transform Z rotation (including its offset).
	 */
	InheritRotationProp inheritRotation{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `local angle offset deg`; use the corresponding instance member in gameplay code.
     */
    using LocalAngleOffsetDegProp = Property<Collider2D, float,
        &Collider2D::GetLocalAngleOffsetDeg,
		&Collider2D::SetLocalAngleOffsetDeg>;
    /** @endcond */
    /** @brief Additional Z rotation, in degrees, applied to box/capsule geometry.
     */
    LocalAngleOffsetDegProp rotationOffset{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `size local`; use the corresponding instance member in gameplay code.
     */
    using SizeLocalProp = Property<Collider2D, Vec2,
		&Collider2D::GetSizeLocal,
		&Collider2D::SetSizeLocal>;
    /** @endcond */
    /** @brief Unscaled local width/height for box and total capsule dimensions.
     */
    SizeLocalProp size{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `radius local`; use the corresponding instance member in gameplay code.
     */
    using RadiusLocalProp = Property<Collider2D, float,
        &Collider2D::GetRadiusLocal,
        &Collider2D::SetRadiusLocal>;
    /** @endcond */
	/** @brief Unscaled local circle radius; world radius multiplies by max(X/Y scale).
	 */
	RadiusLocalProp radius{ this };
};

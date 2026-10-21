#pragma once

#include "Behaviour.h"
#include "BaseTypes.h"
#include "GameObject.h"

class CollisionManager;
class RigidBody2D;
class PhysicsManager;

using LayerBits = uint32_t;

class Collider2D : public Behaviour
{
	friend class CollisionManager;
	friend class PhysicsManager;

public:
    enum class Shape : uint8_t { Box, Circle };

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
    void Awake() override;        // registrar collider
    void OnEnable() override;     // enable collision
    void OnDisable() override;    // desactivar
    void OnDestroy() override;    // limpiar registro
    void Render() override;       // opcional debug

    void Register();

    // ====== WORLD-SPACE queries (apply Transform position, scale and rotation) ======
    struct OrientedBox2D {
        Vec2 center;     // centro en mundo
        Vec2 half;       // semi-anchos en mundo
        float angleDeg;   // angle (degrees, counterclockwise)
    };
    struct Circle2D {
        Vec2 center;
        float radius;
    };

    OrientedBox2D WorldOBB() const noexcept;     // valid when shape == Box
    Circle2D      WorldCircle() const noexcept;  // valid when shape == Circle

    // World-space shape AABB (useful for broad-phase collision checks)
    Rect          WorldAABB() const noexcept;

    //std::unique_ptr<Component> Clone(GameObject* newOwner) const override;

public:
    Collider2D() = default;
    ~Collider2D() override = default;

    using ShapeProp = Property<Collider2D, Shape,
        &Collider2D::GetShape,
		&Collider2D::SetShape>;
    ShapeProp shape{ this };

    using IsTriggerProp = Property<Collider2D, bool,
		&Collider2D::GetIsTrigger,
        &Collider2D::SetIsTrigger>;
    IsTriggerProp isTrigger{ this };

    using LayerProp = Property<Collider2D, LayerBits,
        &Collider2D::GetLayer,
		&Collider2D::SetLayer>;
    LayerProp layer{ this };

    using MaskProp = Property<Collider2D, LayerBits,
		&Collider2D::GetMask,
        &Collider2D::SetMask>;
	MaskProp mask{ this };

    using DrawColliderProp = Property<Collider2D, bool,
        &Collider2D::GetDrawCollider,
		&Collider2D::SetDrawCollider>;
    DrawColliderProp showGizmo{ this };

    using DrawColorProp = Property<Collider2D, Color,
        &Collider2D::GetDrawColor,
		&Collider2D::SetDrawColor>;
    DrawColorProp gizmoColor{ this };

    using OffsetLocalProp = Property<Collider2D, Vec2,
		&Collider2D::GetOffsetLocal,
		&Collider2D::SetOffsetLocal>;
	OffsetLocalProp localOffset{ this };

    using InheritRotationProp = Property<Collider2D, bool,
        &Collider2D::GetInheritRotation,
		&Collider2D::SetInheritRotation>;
	InheritRotationProp inheritRotation{ this };

    using LocalAngleOffsetDegProp = Property<Collider2D, float,
        &Collider2D::GetLocalAngleOffsetDeg,
		&Collider2D::SetLocalAngleOffsetDeg>;
    LocalAngleOffsetDegProp rotationOffset{ this };

    using SizeLocalProp = Property<Collider2D, Vec2,
		&Collider2D::GetSizeLocal,
		&Collider2D::SetSizeLocal>;
    SizeLocalProp size{ this };

    using RadiusLocalProp = Property<Collider2D, float,
        &Collider2D::GetRadiusLocal,
        &Collider2D::SetRadiusLocal>;
	RadiusLocalProp radius{ this };
};
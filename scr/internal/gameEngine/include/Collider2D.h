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

    // --- Configuración general ---
    Shape      shape_ = Shape::Box;
    bool       isTrigger_ = false;
    LayerBits  layer_ = 1u << 0;
    LayerBits  mask_ = 0xFFFFFFFFu;

    bool drawCollider_ = false;
    Color drawColor_ = { 0, 255, 0, 100 };
    bool CustomGizmoColor = false;

    // --- Geometría en ESPACIO LOCAL (unidades de mundo ANTES de escala/rotación) ---
    // Para Box: sizeLocal = {w, h}. Para Circle, usar radiusLocal.
    Vec2      sizeLocal_{ 1.f, 1.f };
    float      radiusLocal_ = 0.5f;

    // Offset en ESPACIO LOCAL (se escalará y rotará con el Transform).
    Vec2      offsetLocal_{ 0.f, 0.f };

    // Rotación: por defecto el collider hereda la rotación del Transform.
    // Puedes sumar un offset angular si quieres.
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
    // Ciclo (sin lógica)
    void Awake() override;        // registrar collider
    void OnEnable() override;     // activar colisión
    void OnDisable() override;    // desactivar
    void OnDestroy() override;    // limpiar registro
    void Render() override;       // opcional debug

    void Register();

    // ====== Consultas en MUNDO (aplican Transform: posición, escala, rotación) ======
    struct OrientedBox2D {
        Vec2 center;     // centro en mundo
        Vec2 half;       // semi-anchos en mundo
        float angleDeg;   // ángulo (grados, CCW)
    };
    struct Circle2D {
        Vec2 center;
        float radius;
    };

    OrientedBox2D WorldOBB() const noexcept;     // válido si shape == Box
    Circle2D      WorldCircle() const noexcept;  // válido si shape == Circle

    // AABB de la forma en mundo (útil para broadphase)
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
#pragma once

#include <cstdint>
#include <memory>
#include <algorithm>

#include "Behaviour.h"
#include "BaseTypes.h"   // Vec2f, Vec3f, etc.
#include "Property.h"

class GameObject;
class PhysicsManager;

class RigidBody2D : public Behaviour
{
    friend class PhysicsManager;
    friend class GameObject;

public:
    enum class BodyType : uint8_t { Static, Dynamic, Kinematic };
    enum class CollisionDetection : uint8_t { Discrete, Continuous };
    enum class ForceMode : uint8_t { Force, Acceleration, Impulse, VelocityChange };

    enum class Constraints : uint8_t {
        None = 0,
        FreezePosX = 1 << 0,
        FreezePosY = 1 << 1,
        FreezeRot = 1 << 2,
    };

private:
    static constexpr bool kUnique = true;

    BodyType bodyType_ = BodyType::Dynamic;
    CollisionDetection collisionDetection_ = CollisionDetection::Discrete;

    float mass_ = 1.0f;
    float inertia_ = 1.0f;
    float gravityScale_ = 1.0f;

    float linearDamping_ = 0.0f;
    float angularDamping_ = 0.0f;

    float restitution_ = 0.0f;

    Constraints constraints_ = Constraints::None;

    Vec2 velocity_{ 0.f, 0.f };
    float angularVelocity_ = 0.f; // rad/s

    // acumuladores (se limpian cada step)
    Vec2 accumForce_{ 0.f, 0.f };        // N (Force)
    Vec2 accumAccel_{ 0.f, 0.f };        // m/s^2 (Acceleration)
    float accumTorque_ = 0.f;             // torque (Force)
    float accumAngularAccel_ = 0.f;       // rad/s^2 (Acceleration)

    // util
    float InvMass_() const noexcept {
        if (bodyType_ != BodyType::Dynamic) return 0.f;
        return (mass_ > 1e-6f) ? (1.f / mass_) : 0.f;
    }
    float InvInertia_() const noexcept {
        if (bodyType_ != BodyType::Dynamic) return 0.f;
        return (inertia_ > 1e-6f) ? (1.f / inertia_) : 0.f;
    }

    static inline bool Has_(Constraints v, Constraints f) noexcept {
        return (uint8_t(v) & uint8_t(f)) != 0;
    }

    BodyType GetBodyType() const noexcept { return bodyType_; }
    void SetBodyType(BodyType t) noexcept { bodyType_ = t; }

    CollisionDetection GetCollisionDetection() const noexcept { return collisionDetection_; }
    void SetCollisionDetection(CollisionDetection d) noexcept { collisionDetection_ = d; }

    float GetMass() const noexcept { return mass_; }
    void SetMass(float m) noexcept { mass_ = std::max(0.0001f, m); }

    float GetInertia() const noexcept { return inertia_; }
    void SetInertia(float i) noexcept { inertia_ = std::max(0.0001f, i); }

    float GetGravityScale() const noexcept { return gravityScale_; }
    void SetGravityScale(float g) noexcept { gravityScale_ = g; }

    float GetLinearDamping() const noexcept { return linearDamping_; }
    void SetLinearDamping(float d) noexcept { linearDamping_ = std::max(0.f, d); }

    float GetAngularDamping() const noexcept { return angularDamping_; }
    void SetAngularDamping(float d) noexcept { angularDamping_ = std::max(0.f, d); }

    float GetRestitution() const noexcept { return restitution_; }
    void SetRestitution(float r) noexcept { restitution_ = std::clamp(r, 0.f, 1.f); }

    Constraints GetConstraints() const noexcept { return constraints_; }
    void SetConstraints(Constraints c) noexcept { constraints_ = c; }

    Vec2 GetVelocity() const noexcept { return velocity_; }
    void SetVelocity(Vec2 v) noexcept { velocity_ = v; }

    float GetAngularVelocity() const noexcept { return angularVelocity_; }
    void SetAngularVelocity(float w) noexcept { angularVelocity_ = w; }

public:
    RigidBody2D() = default;
    ~RigidBody2D() override = default;

    void AddForce(const Vec2& f, ForceMode mode = ForceMode::Force) noexcept
    {
        if (!mAwoken || !mEnabled) return;
        if (!gameObject->activeInHierarchy) return;
        if (bodyType_ == BodyType::Static) return;

        switch (mode)
        {
        case ForceMode::Force:          accumForce_ += f; break;
        case ForceMode::Acceleration:   accumAccel_ += f; break;
        case ForceMode::Impulse:        velocity_ += f * InvMass_(); break;
        case ForceMode::VelocityChange: velocity_ += f; break;
        }
    }

    void AddTorque(float t, ForceMode mode = ForceMode::Force) noexcept
    {
        if (!mAwoken || !mEnabled) return;
        if (!gameObject->activeInHierarchy) return;
        if (bodyType_ == BodyType::Static) return;

        switch (mode)
        {
        case ForceMode::Force:          accumTorque_ += t; break;
        case ForceMode::Acceleration:   accumAngularAccel_ += t; break;
        case ForceMode::Impulse:        angularVelocity_ += t * InvInertia_(); break;
        case ForceMode::VelocityChange: angularVelocity_ += t; break;
        }
    }

    void ClearForces() noexcept {
        if (!mAwoken || !mEnabled) return;
        if (!gameObject->activeInHierarchy) return;
        accumForce_ = { 0,0 };
        accumAccel_ = { 0,0 };
        accumTorque_ = 0.f;
        accumAngularAccel_ = 0.f;
    }

    // --- Body setup ---
    using BodyTypeProp = Property<RigidBody2D, BodyType,
        &RigidBody2D::GetBodyType,
        &RigidBody2D::SetBodyType>;

    BodyTypeProp bodyType{ this };

    using CollisionDetectionProp = Property<RigidBody2D, CollisionDetection,
        &RigidBody2D::GetCollisionDetection,
        &RigidBody2D::SetCollisionDetection>;

    CollisionDetectionProp collisionDetection{ this };

    using MassProp = Property<RigidBody2D, float,
        &RigidBody2D::GetMass,
        &RigidBody2D::SetMass>;

    MassProp mass{ this };

    using InertiaProp = Property<RigidBody2D, float,
        &RigidBody2D::GetInertia,
        &RigidBody2D::SetInertia>;

    InertiaProp inertia{ this };

    using GravityScaleProp = Property<RigidBody2D, float,
        &RigidBody2D::GetGravityScale,
        &RigidBody2D::SetGravityScale>;

    GravityScaleProp gravityScale{ this };

    // --- Damping ---
    using LinearDampingProp = Property<RigidBody2D, float,
        &RigidBody2D::GetLinearDamping,
        &RigidBody2D::SetLinearDamping>;

    LinearDampingProp linearDamping{ this };

    using AngularDampingProp = Property<RigidBody2D, float,
        &RigidBody2D::GetAngularDamping,
        &RigidBody2D::SetAngularDamping>;

    AngularDampingProp angularDamping{ this };

    // --- Material ---
    using RestitutionProp = Property<RigidBody2D, float,
        &RigidBody2D::GetRestitution,
        &RigidBody2D::SetRestitution>;

    RestitutionProp restitution{ this };

    // --- Constraints ---
    using ConstraintsProp = Property<RigidBody2D, Constraints,
        &RigidBody2D::GetConstraints,
        &RigidBody2D::SetConstraints>;

    ConstraintsProp constraints{ this };

    // --- State ---
    using VelocityProp = Property<RigidBody2D, Vec2,
        &RigidBody2D::GetVelocity,
        &RigidBody2D::SetVelocity>;

    VelocityProp velocity{ this };

    using AngularVelocityProp = Property<RigidBody2D, float,
        &RigidBody2D::GetAngularVelocity,
        &RigidBody2D::SetAngularVelocity>;

    AngularVelocityProp angularVelocity{ this };

protected:
    bool IsUnique() const noexcept override { return kUnique; }

    void Awake() override;
    void OnEnable() override;
    void OnDisable() override;
    void OnDestroy() override;

    void Register();
};

inline constexpr RigidBody2D::Constraints operator|(RigidBody2D::Constraints a, RigidBody2D::Constraints b) noexcept {
    return RigidBody2D::Constraints(uint8_t(a) | uint8_t(b));
}
inline constexpr RigidBody2D::Constraints operator&(RigidBody2D::Constraints a, RigidBody2D::Constraints b) noexcept {
    return RigidBody2D::Constraints(uint8_t(a) & uint8_t(b));
}
#pragma once
/** @file RigidBody2D.h
 * @brief Physical motion, mass, forces and collision response parameters.
 */

#include <cstdint>
#include <memory>
#include <algorithm>

#include "Behaviour.h"
#include "BaseTypes.h"   // Vec2f, Vec3f, etc.
#include "Property.h"

class GameObject;
class PhysicsManager;

/** @brief Integrates dynamic or kinematic XY movement and Z-axis angular motion.
 *
 * The physics solver runs at fixed timesteps. Dynamic bodies respond to force,
 * gravity and contacts; kinematic bodies use their configured velocities and
 * ignore forces; static bodies are not integrated. A Rigidbody must be active
 * and awakened for its force methods to have an effect.
 * @ingroup xengine_physics
 */
class RigidBody2D : public Behaviour
{
    friend class PhysicsManager;
    friend class GameObject;

public:
    /** @brief Available body type modes for RigidBody2D.
     */
    enum class BodyType : uint8_t {
        Static,    ///< Immovable body with no simulated integration.
        Dynamic,   ///< Body whose motion is integrated by physics and forces.
        Kinematic  ///< Body moved by gameplay code rather than integrated forces.
    };
    /** @brief Available collision detection modes for RigidBody2D.
     */
    enum class CollisionDetection : uint8_t {
        Discrete,  ///< Check contacts at discrete simulation steps.
        Continuous ///< Enable additional continuous collision detection for fast movement.
    };
    /** @brief How AddForce/AddTorque apply their input to accumulated or current motion.
     */
    enum class ForceMode : uint8_t {
        Force,          ///< Apply an ongoing force that depends on mass and timestep.
        Acceleration,   ///< Apply acceleration independently of mass.
        Impulse,        ///< Apply an instantaneous momentum change depending on mass.
        VelocityChange  ///< Apply an instantaneous velocity change independent of mass.
    };

    /** @brief Available constraints modes for RigidBody2D.
     */
    enum class Constraints : uint8_t {
        None = 0,           ///< Do not constrain translation or rotation.
        FreezePosX = 1 << 0, ///< Prevent X-axis translation.
        FreezePosY = 1 << 1, ///< Prevent Y-axis translation.
        FreezeRot = 1 << 2,  ///< Prevent rotation; flags may be combined.
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

    // accumulators (cleared at each step)
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
    /** @brief Constructs a RigidBody2D instance.
     */
    RigidBody2D() = default;
    /** @brief Releases the RigidBody2D instance and any owned resources.
     */
    ~RigidBody2D() override = default;

    /** @brief Applies linear force, acceleration or an immediate velocity change.
     * @param f Force vector in world-space coordinates.
     * @param mode Force/Acceleration accumulate for the next physics step;
     *             Impulse applies `f / mass` immediately; VelocityChange adds `f`.
     * @note Ignored before Awake, when disabled/inactive, or for static bodies.
     *       Kinematic bodies ignore accumulated forces and accelerations.
     */
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

    /** @brief Applies angular torque, acceleration, impulse or velocity change.
     * @param t Torque/acceleration value or instantaneous angular impulse/velocity delta.
     * @param mode Force/Acceleration accumulate; Impulse changes angular
     *             velocity by `t / inertia`; VelocityChange adds `t` directly.
     * @note Angular velocity is measured in radians per second. Static and
     *       inactive bodies ignore the operation.
     */
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

    /** @brief Clears accumulated linear/angular forces and accelerations.
     * @note Does not reset current velocity or angular velocity; ignored if
     *       this component has not awakened or is disabled/inactive.
     */
    void ClearForces() noexcept {
        if (!mAwoken || !mEnabled) return;
        if (!gameObject->activeInHierarchy) return;
        accumForce_ = { 0,0 };
        accumAccel_ = { 0,0 };
        accumTorque_ = 0.f;
        accumAngularAccel_ = 0.f;
    }

    // --- Body setup ---
    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `body type`; use the corresponding instance member in gameplay code.
     */
    using BodyTypeProp = Property<RigidBody2D, BodyType,
        &RigidBody2D::GetBodyType,
        &RigidBody2D::SetBodyType>;
    /** @endcond */

    /** @brief Body mobility mode (static, dynamic or kinematic).
     */
    BodyTypeProp bodyType{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `collision detection`; use the corresponding instance member in gameplay code.
     */
    using CollisionDetectionProp = Property<RigidBody2D, CollisionDetection,
        &RigidBody2D::GetCollisionDetection,
        &RigidBody2D::SetCollisionDetection>;
    /** @endcond */

    /** @brief Collision-detection strategy used by this body.
     */
    CollisionDetectionProp collisionDetection{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `mass`; use the corresponding instance member in gameplay code.
     */
    using MassProp = Property<RigidBody2D, float,
        &RigidBody2D::GetMass,
        &RigidBody2D::SetMass>;
    /** @endcond */

    /** @brief Dynamic-body mass used for inverse-mass force and impulse calculations.
     * @note Assignments are clamped to at least 0.0001.
     */
    MassProp mass{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `inertia`; use the corresponding instance member in gameplay code.
     */
    using InertiaProp = Property<RigidBody2D, float,
        &RigidBody2D::GetInertia,
        &RigidBody2D::SetInertia>;
    /** @endcond */

    /** @brief Inertia used to convert torque/impulse into angular motion.
     * @note Assignments are clamped to at least 0.0001.
     */
    InertiaProp inertia{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `gravity scale`; use the corresponding instance member in gameplay code.
     */
    using GravityScaleProp = Property<RigidBody2D, float,
        &RigidBody2D::GetGravityScale,
        &RigidBody2D::SetGravityScale>;
    /** @endcond */

    /** @brief Multiplier of the scene gravity for this body.
     */
    GravityScaleProp gravityScale{ this };

    // --- Damping ---
    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `linear damping`; use the corresponding instance member in gameplay code.
     */
    using LinearDampingProp = Property<RigidBody2D, float,
        &RigidBody2D::GetLinearDamping,
        &RigidBody2D::SetLinearDamping>;
    /** @endcond */

    /** @brief Nonnegative damping applied to linear velocity each fixed timestep.
     */
    LinearDampingProp linearDamping{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `angular damping`; use the corresponding instance member in gameplay code.
     */
    using AngularDampingProp = Property<RigidBody2D, float,
        &RigidBody2D::GetAngularDamping,
        &RigidBody2D::SetAngularDamping>;
    /** @endcond */

    /** @brief Nonnegative damping applied to angular velocity each fixed timestep.
     */
    AngularDampingProp angularDamping{ this };

    // --- Material ---
    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `restitution`; use the corresponding instance member in gameplay code.
     */
    using RestitutionProp = Property<RigidBody2D, float,
        &RigidBody2D::GetRestitution,
        &RigidBody2D::SetRestitution>;
    /** @endcond */

    /** @brief Collision bounce coefficient, clamped to [0, 1].
     */
    RestitutionProp restitution{ this };

    // --- Constraints ---
    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `constraints`; use the corresponding instance member in gameplay code.
     */
    using ConstraintsProp = Property<RigidBody2D, Constraints,
        &RigidBody2D::GetConstraints,
        &RigidBody2D::SetConstraints>;
    /** @endcond */

    /** @brief Axis and rotational movement restrictions for this body.
     */
    ConstraintsProp constraints{ this };

    // --- State ---
    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `velocity`; use the corresponding instance member in gameplay code.
     */
    using VelocityProp = Property<RigidBody2D, Vec2,
        &RigidBody2D::GetVelocity,
        &RigidBody2D::SetVelocity>;
    /** @endcond */

    /** @brief Linear velocity in world units per second.
     */
    VelocityProp velocity{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `angular velocity`; use the corresponding instance member in gameplay code.
     */
    using AngularVelocityProp = Property<RigidBody2D, float,
        &RigidBody2D::GetAngularVelocity,
        &RigidBody2D::SetAngularVelocity>;
    /** @endcond */

    /** @brief Angular velocity about the Z axis, in radians per second.
     */
    AngularVelocityProp angularVelocity{ this };

protected:
    /** @brief Reports whether unique is currently true.
     */
    bool IsUnique() const noexcept override { return kUnique; }

    /** @brief Lifecycle hook for awake; the engine controls invocation order.
     */
    void Awake() override;
    /** @brief Lifecycle callback invoked for enable.
     */
    void OnEnable() override;
    /** @brief Lifecycle callback invoked for disable.
     */
    void OnDisable() override;
    /** @brief Lifecycle callback invoked for destroy.
     */
    void OnDestroy() override;

    /** @brief Registers this component type with the engine component system.
     */
    void Register();
};

/** @brief Combines two Constraints bit flags.
 * @param a First set of flags.
 * @param b Additional flags.
 */
inline constexpr RigidBody2D::Constraints operator|(RigidBody2D::Constraints a, RigidBody2D::Constraints b) noexcept {
    return RigidBody2D::Constraints(uint8_t(a) | uint8_t(b));
}
/** @brief Intersects two Constraints bit flags.
 * @param a First set of flags.
 * @param b Second set of flags.
 */
inline constexpr RigidBody2D::Constraints operator&(RigidBody2D::Constraints a, RigidBody2D::Constraints b) noexcept {
    return RigidBody2D::Constraints(uint8_t(a) & uint8_t(b));
}

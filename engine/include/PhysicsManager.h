#pragma once
/** @file PhysicsManager.h
 * @brief Fixed-step physics simulation settings and runtime diagnostics.
 */

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Singleton.h"
#include "BaseTypes.h"
#include "RigidBody2D.h"
#include "Engine.h"

struct NarrowContact;

class CollisionManager;
class SceneManager;

/** @brief Integrates active rigid bodies and resolves detected contacts in fixed steps.
 *
 * Physics substeps can increase for dynamic bodies using Continuous detection.
 * Trigger overlaps are reported by CollisionManager but receive no physical
 * contact response.
 * @ingroup xengine_physics
 */
class PhysicsManager : public Singleton<PhysicsManager>
{
    friend class Singleton<PhysicsManager>;
    friend class Engine;
    friend class SceneManager;
    friend class RigidBody2D;

    struct Config
    {
        Vec2 baseGravity = { 0.f, 9.81f };
        float penetrationSlop = 0.01f;
        float penetrationPercent = 0.8f;
        int maxSubsteps = 8;
        float ccdMinSizeFactor = 0.5f;
    };

private:
    PhysicsManager() = default;
    ~PhysicsManager() = default;

    bool Init(const Config& cfg) noexcept 
    { 
        gravity = cfg.baseGravity;
        penetrationSlop = cfg.penetrationSlop;
        penetrationPercent = cfg.penetrationPercent;
        maxSubsteps = cfg.maxSubsteps;
        ccdMinSizeFactor = cfg.ccdMinSizeFactor;

        mBodies.clear(); return true; 
    }
    void Shutdown() noexcept { mBodies.clear(); }

    void RegisterBody(RigidBody2D* b) noexcept;
    void RemoveBody(RigidBody2D* b) noexcept;
    void SetBodyActive(RigidBody2D* b, bool active) noexcept;

    // Call from Engine::DoFixedUpdates_
    void Step(float fixedDt) noexcept;

private:
    Vec2 gravity{ 0.f, 9.81f };    // {0,0} for top-down games

    // penetration correction
    float penetrationSlop = 0.01f;
    float penetrationPercent = 0.8f;

    // Continuous: substeps
    int maxSubsteps = 8;
    float ccdMinSizeFactor = 0.5f; // lower values produce more substeps (safer)

    inline Vec2 GetGravity() const noexcept { return gravity; }
    inline float GetPenetrationSlop() const noexcept { return penetrationSlop; }
    inline float GetPenetrationPercent() const noexcept { return penetrationPercent; }
    inline int   GetMaxSubsteps() const noexcept { return maxSubsteps; }
    inline float GetCCDMinSizeFactor() const noexcept { return ccdMinSizeFactor; }

    inline void SetGravity(const Vec2 g) noexcept { gravity = g; }
    inline void SetPenetrationSlop(float slop) noexcept { penetrationSlop = slop; }
    inline void SetPenetrationPercent(float percent) noexcept { penetrationPercent = percent; }
    inline void SetMaxSubsteps(int substeps) noexcept { maxSubsteps = (substeps < 1) ? 1 : substeps; }
    inline void SetCCDMinSizeFactor(float factor) noexcept { ccdMinSizeFactor = (factor < 0.0f) ? 0.0f : factor; }

private:
    std::unordered_map<RigidBody2D*, bool> mBodies;

    inline Vec2 ColliderCenterWorld_(const Collider2D* c) noexcept;

    int ComputeSubsteps_(float dt) noexcept;
    void Integrate_(float dt) noexcept;
    void SolveContacts_(const std::vector<NarrowContact>& contacts, float dt) noexcept;
    void PositionalCorrection_(const std::vector<NarrowContact>& contacts) noexcept;

    // helpers 2D
    static inline float Cross(const Vec2& a, const Vec2& b) noexcept { return a.x * b.y - a.y * b.x; }
    static inline Vec2 Perp(float wRad, const Vec2& r) noexcept {
        return { -wRad * r.y, wRad * r.x };
    }

    // Stats
    double stepTimeSec = 0.0;
    double integrateTimeSec = 0.0;
    double buildContactsTimeSec = 0.0;
    double solveTimeSec = 0.0;

    int nSubstepsThisFrame = 0;
    int solverIterations = 0;
    std::uint64_t nContactsProcessedThisFrame = 0;

public:

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `gravity`; use the corresponding instance member in gameplay code.
     */
    using GravityProp = Property<PhysicsManager, Vec2,
        &PhysicsManager::GetGravity,
        &PhysicsManager::SetGravity>;
    /** @endcond */

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `penetration slop`; use the corresponding instance member in gameplay code.
     */
    using PenetrationSlopProp = Property<PhysicsManager, float,
        &PhysicsManager::GetPenetrationSlop,
        &PhysicsManager::SetPenetrationSlop>;
    /** @endcond */

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `penetration percent`; use the corresponding instance member in gameplay code.
     */
    using PenetrationPercentProp = Property<PhysicsManager, float,
        &PhysicsManager::GetPenetrationPercent,
        &PhysicsManager::SetPenetrationPercent>;
    /** @endcond */

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `max substeps`; use the corresponding instance member in gameplay code.
     */
    using MaxSubstepsProp = Property<PhysicsManager, int,
        &PhysicsManager::GetMaxSubsteps,
        &PhysicsManager::SetMaxSubsteps>;
    /** @endcond */

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `ccdmin size factor`; use the corresponding instance member in gameplay code.
     */
    using CCDMinSizeFactorProp = Property<PhysicsManager, float,
        &PhysicsManager::GetCCDMinSizeFactor,
        &PhysicsManager::SetCCDMinSizeFactor>;
    /** @endcond */

    /** @brief World-space gravity acceleration (default +9.81 on Y, downward).
     */
    GravityProp gravityProp{ this };

    /** @brief Allowed penetration depth before positional correction begins.
     */
    PenetrationSlopProp penetrationSlopProp{ this };
    /** @brief Fraction of excess penetration to correct during a solver step.
     */
    PenetrationPercentProp penetrationPercentProp{ this };

    /** @brief Upper bound on physics substeps for continuous detection; minimum 1.
     */
    MaxSubstepsProp maxSubstepsProp{ this };
    /** @brief Fraction of collider size used to choose CCD substep distance.
     * @note Lower positive values produce more substeps, limited by `maxSubstepsProp`.
     *       Negative assignments clamp to zero.
     */
    CCDMinSizeFactorProp ccdMinSizeFactorProp{ this };
};

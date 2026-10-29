#pragma once
/** @file CollisionManager.h
 * @brief Contact generation, query and collision/trigger dispatch.
 */

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "XEngine/detail/ParallelExecutor.h"
#include "XEngine/detail/AdaptiveBroadphase.h"
#include "XEngine/detail/SpatialContextClassifier.h"
#include <string>

#include "Singleton.h"
#include "GameObject.h"
#include "Collider2D.h"
#include "SceneManager.h"
#include "Engine.h"

/** @brief Contact candidate between two scene entities and their colliders.
 */
struct NarrowContact {
    /** @brief First entity involved in the contact (non-owning).
     */
    GameObject* a = nullptr;
    /** @brief Second entity involved in the contact (non-owning).
     */
    GameObject* b = nullptr;
    /** @brief First collider participating in this detected contact.
     */
    Collider2D* colA = nullptr;
    /** @brief Second collider participating in this detected contact.
     */
    Collider2D* colB = nullptr;
    /** @brief Whether this pair is treated as a trigger overlap.
     */
    bool isTriggerPair = false;
    /** @brief Contact points and normals computed for the pair.
     */
    ContactPoint contact;
};

// Forward declarations
class PhysicsManager;

/** @brief Builds collision pairs and dispatches Enter/Stay/Exit or trigger callbacks.
 *
 * Uses a configurable brute-force, spatial-grid or adaptive broad phase.
 * Candidate geometry can be parallelized, but game callbacks and physics
 * integration remain on the engine thread. PhysicsManager resolves non-trigger
 * contact impulses separately.
 * @ingroup xengine_physics
 */
class CollisionManager : public Singleton<CollisionManager>
{
    friend class Singleton<CollisionManager>;
	friend class Collider2D;
    friend class SceneManager;
    friend class Engine;
    friend class PhysicsManager;

private:
    CollisionManager() = default;
    ~CollisionManager() = default;

    bool Init() noexcept;
    void Shutdown() noexcept;

    void RegisterCollider(Collider2D* c) noexcept;
    void RemoveCollider(Collider2D* c) noexcept;
    void SetColliderActive(Collider2D* c, bool active) noexcept;
    void ClearAll() noexcept;

    // Detecta solapes AABB y despacha On*Enter/Stay/Exit (collision/trigger).
    // Does not resolve positions or physics.
    void DetectAndDispatch() noexcept;

    // Only independent, read-only collision candidate evaluation is parallel.
    // Scene callbacks, physics integration and SDL stay on the main thread.
    void EnableParallelCollision(bool enabled) noexcept;
    // 0 = original all-pairs baseline, 1 = spatial hash grid.
    void ConfigureBroadphase(const std::string& mode, float cellSize, bool autoCellSize = false) noexcept;
    bool mSpatialBroadphase = false; // explicitly forced grid
    bool mAutoBroadphase = false;
    AdaptiveBroadphase mAdaptiveBroadphase;
    AdaptiveCellBroadphase mAdaptiveCellBroadphase;
    SpatialContextClassifier mSpatialContextClassifier;
    bool mAutoGridCellSize = false;
    float mLastGridCellSize = 0.f;
    float mFrameLastGridCellSize = 0.f;
    unsigned mFrameAutoCalibrationPasses = 0;
    unsigned mFrameAutoContextLast = 0;
    unsigned mFrameAutoContextTransitions = 0;
    unsigned mPreviousAutoContext = 0xFFFFFFFFu;
    unsigned mFrameAutoGridCell1Passes = 0;
    unsigned mFrameAutoGridCell2Passes = 0;
    unsigned mFrameAutoGridCell4Passes = 0;
    unsigned mFrameGridPasses = 0;
    unsigned mFrameBruteforcePasses = 0;
    float mBroadphaseCellSize = 2.f;
    float mLastGridBuildMs = 0.f;
    float mFrameGridBuildMs = 0.f;
    float mFrameAutoSelectorMs = 0.f;
    std::uint64_t mFrameContactSignature = 14695981039346656037ULL;

    unsigned ParallelWorkerCount() const noexcept;
    std::unique_ptr<ParallelExecutor> mParallelExecutor;
    float mLastSnapshotMs = 0.f;
    float mLastCandidateMs = 0.f;
    float mLastMergeMs = 0.f;
    // True only when the candidate-evaluation jobs were actually dispatched.
    bool mLastUsedParallelCandidates = false;
    bool mLastUsedGrid = false;

    // Per-frame totals (PhysicsManager substeps + DetectAndDispatch passes).
    // The mLast* and n*ThisFrame variables above/below remain per-pass values.
    void ResetFrameProfile_() noexcept;
    void AccumulatePassProfile_(std::size_t contactsBuilt) noexcept;
    float mFrameSnapshotMs = 0.f;
    float mFrameCandidateMs = 0.f;
    float mFrameMergeMs = 0.f;
    std::uint64_t mFrameBroadphaseTests = 0;
    std::uint64_t mFrameNarrowphaseTests = 0;
    std::uint64_t mFrameContactsBuilt = 0;
    unsigned mFrameCollisionPasses = 0;
    unsigned mFrameParallelPasses = 0;


    std::unordered_map<Collider2D*, bool> mRegisteredColliders;

    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;
    CollisionManager(CollisionManager&&) = delete;
    CollisionManager& operator=(CollisionManager&&) = delete;

    static inline constexpr std::uint64_t MakeKey_(EntityID idA, EntityID idB) noexcept
    {
        const uint64_t lo = (uint64_t)(idA < idB ? idA : idB);
        const uint64_t hi = (uint64_t)(idA < idB ? idB : idA);
        return (hi << 32) | lo;
    }

    struct PairInfo
    {
        GameObject* a = nullptr;
        GameObject* b = nullptr;
        Collider2D* colA = nullptr;
        Collider2D* colB = nullptr;
        bool isTriggerPair = false;
    };

    std::unordered_map<uint64_t, PairInfo> mPrevInfo;
    std::unordered_map<uint64_t, PairInfo> mCurrInfo;

    // The pair metadata maps also serve as pair-membership indices. Keeping
    // parallel unordered_sets would require an extra allocation/hash per contact.
    // A removed collider historically suppresses Stay on the next pass while
    // retaining prior metadata long enough to dispatch Exit.
    bool mSuppressPreviousPairMembership = false;

    int nBroadphaseTestsThisFrame = 0;
    int nNarrowphaseTestsThisFrame = 0;
    size_t nContactsBuiltThisFrame = 0;

    void BuildContacts_(std::vector<NarrowContact>& out) noexcept;
    void Dispatch_(const std::vector<NarrowContact>& contacts) noexcept;

    static bool ShouldTest(const Collider2D& a, const Collider2D& b) noexcept;

    // Narrow-phase:
    static bool TestOBB_OBB(const Collider2D::OrientedBox2D& A,
        const Collider2D::OrientedBox2D& B) noexcept;   // SAT 2D
    static bool TestCircle_Circle(const Vec2& ca, float ra,
        const Vec2& cb, float rb) noexcept;
    static bool TestOBB_Circle(const Collider2D::OrientedBox2D& box,
        const Vec2& cc, float r) noexcept;

    // Broad phase (already implemented): AABB vs AABB
    static bool TestBoxBox(const Rect& a, const Rect& b) noexcept;

    bool Contact_OBB_Circle(const Vec2& cc, float r, const Vec2& bc, const Vec2& half, float rotDeg, ContactPoint& out) noexcept;

    struct MinAxisResult { Vec2 axis; float depth; bool valid; };
    MinAxisResult SAT_MinAxis(const Collider2D::OrientedBox2D& A, const Collider2D::OrientedBox2D& B);

    bool Contact_OBB_OBB(const Vec2& ac, const Vec2& aHalf, float aRotDeg,
        const Vec2& bc, const Vec2& bHalf, float bRotDeg,
        ContactPoint& out) noexcept;

    bool Contact_Circle_Circle(const Vec2& ca, float ra,
        const Vec2& cb, float rb,
        ContactPoint& out) noexcept;
};

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
    /** @brief Permit the singleton base to construct and release the CollisionManager manager.
     */
    friend class Singleton<CollisionManager>;
	/** @brief Grant Collider2D access to the internal state of CollisionManager.
	 */
	friend class Collider2D;
    /** @brief Grant SceneManager access to the internal state of CollisionManager.
     */
    friend class SceneManager;
    /** @brief Grant Engine access to the internal state of CollisionManager.
     */
    friend class Engine;
    /** @brief Grant PhysicsManager access to the internal state of CollisionManager.
     */
    friend class PhysicsManager;

private:
    /** @brief Construct a CollisionManager instance.
     */
    CollisionManager() = default;
    /** @brief Destroy a CollisionManager instance and release owned resources.
     */
    ~CollisionManager() = default;

    /** @brief Initialize the CollisionManager subsystem.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Init() noexcept;
    /** @brief Release resources managed by CollisionManager.
     */
    void Shutdown() noexcept;

    /** @brief Register collider in CollisionManager.
     * @param c C supplied to CollisionManager.
     */
    void RegisterCollider(Collider2D* c) noexcept;
    /** @brief Remove collider in CollisionManager.
     * @param c C supplied to CollisionManager.
     */
    void RemoveCollider(Collider2D* c) noexcept;
    /** @brief Set collider active in CollisionManager.
     * @param c C supplied to CollisionManager.
     * @param active Active supplied to CollisionManager.
     */
    void SetColliderActive(Collider2D* c, bool active) noexcept;
    /** @brief Clear all in CollisionManager.
     */
    void ClearAll() noexcept;

    // Detecta solapes AABB y despacha On*Enter/Stay/Exit (collision/trigger).
    // Does not resolve positions or physics.
    /** @brief Detect and dispatch in CollisionManager.
     */
    void DetectAndDispatch() noexcept;

    // Only independent, read-only collision candidate evaluation is parallel.
    // Scene callbacks, physics integration and SDL stay on the main thread.
    /** @brief Enable parallel collision in CollisionManager.
     * @param enabled Whether the corresponding feature should be active.
     */
    void EnableParallelCollision(bool enabled) noexcept;
    // 0 = original all-pairs baseline, 1 = spatial hash grid.
    /** @brief Configure broadphase in CollisionManager.
     * @param mode Operating mode selected for this operation.
     * @param cellSize Cell size used to bound or select the operation.
     * @param autoCellSize Auto cell size used to bound or select the operation.
     */
    void ConfigureBroadphase(const std::string& mode, float cellSize, bool autoCellSize = false) noexcept;
    /** @brief Stored spatial broadphase value used by CollisionManager.
     */
    bool mSpatialBroadphase = false; // explicitly forced grid
    /** @brief Stored auto broadphase value used by CollisionManager.
     */
    bool mAutoBroadphase = false;
    /** @brief Stored adaptive broadphase value used by CollisionManager.
     */
    AdaptiveBroadphase mAdaptiveBroadphase;
    /** @brief Stored adaptive cell broadphase value used by CollisionManager.
     */
    AdaptiveCellBroadphase mAdaptiveCellBroadphase;
    /** @brief Stored spatial context classifier value used by CollisionManager.
     */
    SpatialContextClassifier mSpatialContextClassifier;
    /** @brief Stored auto grid cell size value used by CollisionManager.
     */
    bool mAutoGridCellSize = false;
    /** @brief Stored last grid cell size value used by CollisionManager.
     */
    float mLastGridCellSize = 0.f;
    /** @brief Stored frame last grid cell size value used by CollisionManager.
     */
    float mFrameLastGridCellSize = 0.f;
    /** @brief Stored frame auto calibration passes value used by CollisionManager.
     */
    unsigned mFrameAutoCalibrationPasses = 0;
    /** @brief Stored frame auto context last value used by CollisionManager.
     */
    unsigned mFrameAutoContextLast = 0;
    /** @brief Stored frame auto context transitions value used by CollisionManager.
     */
    unsigned mFrameAutoContextTransitions = 0;
    /** @brief Stored previous auto context value used by CollisionManager.
     */
    unsigned mPreviousAutoContext = 0xFFFFFFFFu;
    /** @brief Stored frame auto grid cell1 passes value used by CollisionManager.
     */
    unsigned mFrameAutoGridCell1Passes = 0;
    /** @brief Stored frame auto grid cell2 passes value used by CollisionManager.
     */
    unsigned mFrameAutoGridCell2Passes = 0;
    /** @brief Stored frame auto grid cell4 passes value used by CollisionManager.
     */
    unsigned mFrameAutoGridCell4Passes = 0;
    /** @brief Stored frame grid passes value used by CollisionManager.
     */
    unsigned mFrameGridPasses = 0;
    /** @brief Stored frame bruteforce passes value used by CollisionManager.
     */
    unsigned mFrameBruteforcePasses = 0;
    /** @brief Stored broadphase cell size value used by CollisionManager.
     */
    float mBroadphaseCellSize = 2.f;
    /** @brief Stored last grid build ms value used by CollisionManager.
     */
    float mLastGridBuildMs = 0.f;
    /** @brief Stored frame grid build ms value used by CollisionManager.
     */
    float mFrameGridBuildMs = 0.f;
    /** @brief Stored frame auto selector ms value used by CollisionManager.
     */
    float mFrameAutoSelectorMs = 0.f;
    /** @brief Stored frame contact signature value used by CollisionManager.
     */
    std::uint64_t mFrameContactSignature = 14695981039346656037ULL;

    /** @brief Parallel worker count in CollisionManager.
     * @return Result of parallel worker count in CollisionManager.
     */
    unsigned ParallelWorkerCount() const noexcept;
    /** @brief Stored parallel executor value used by CollisionManager.
     */
    std::unique_ptr<ParallelExecutor> mParallelExecutor;
    /** @brief Stored last snapshot ms value used by CollisionManager.
     */
    float mLastSnapshotMs = 0.f;
    /** @brief Stored last candidate ms value used by CollisionManager.
     */
    float mLastCandidateMs = 0.f;
    /** @brief Stored last merge ms value used by CollisionManager.
     */
    float mLastMergeMs = 0.f;
    // True only when the candidate-evaluation jobs were actually dispatched.
    /** @brief Stored last used parallel candidates value used by CollisionManager.
     */
    bool mLastUsedParallelCandidates = false;
    /** @brief Stored last used grid value used by CollisionManager.
     */
    bool mLastUsedGrid = false;

    // Per-frame totals (PhysicsManager substeps + DetectAndDispatch passes).
    // The mLast* and n*ThisFrame variables above/below remain per-pass values.
    /** @brief Reset frame profile in CollisionManager.
     */
    void ResetFrameProfile_() noexcept;
    /** @brief Accumulate pass profile in CollisionManager.
     * @param contactsBuilt Contacts built supplied to CollisionManager.
     */
    void AccumulatePassProfile_(std::size_t contactsBuilt) noexcept;
    /** @brief Stored frame snapshot ms value used by CollisionManager.
     */
    float mFrameSnapshotMs = 0.f;
    /** @brief Stored frame candidate ms value used by CollisionManager.
     */
    float mFrameCandidateMs = 0.f;
    /** @brief Stored frame merge ms value used by CollisionManager.
     */
    float mFrameMergeMs = 0.f;
    /** @brief Stored frame broadphase tests value used by CollisionManager.
     */
    std::uint64_t mFrameBroadphaseTests = 0;
    /** @brief Stored frame narrowphase tests value used by CollisionManager.
     */
    std::uint64_t mFrameNarrowphaseTests = 0;
    /** @brief Stored frame contacts built value used by CollisionManager.
     */
    std::uint64_t mFrameContactsBuilt = 0;
    /** @brief Stored frame collision passes value used by CollisionManager.
     */
    unsigned mFrameCollisionPasses = 0;
    /** @brief Stored frame parallel passes value used by CollisionManager.
     */
    unsigned mFrameParallelPasses = 0;


    /** @brief Stored registered colliders value used by CollisionManager.
     */
    std::unordered_map<Collider2D*, bool> mRegisteredColliders;

    /** @brief Construct a CollisionManager instance.
     * @param other Source object whose copy, move or equality is being requested.
     */
    CollisionManager(const CollisionManager& other) = delete;
    /** @brief Implement the operator= operation for CollisionManager.
     * @return Result of applying the overloaded operator.
     * @param other Source object whose copy, move or equality is being requested.
     */
    CollisionManager& operator=(const CollisionManager& other) = delete;
    /** @brief Construct a CollisionManager instance.
     * @param other Source object whose copy, move or equality is being requested.
     */
    CollisionManager(CollisionManager&& other) = delete;
    /** @brief Implement the operator= operation for CollisionManager.
     * @return Result of applying the overloaded operator.
     * @param other Source object whose copy, move or equality is being requested.
     */
    CollisionManager& operator=(CollisionManager&& other) = delete;

    /** @brief Make key in CollisionManager.
     * @param idA Id a supplied to CollisionManager.
     * @param idB Id b supplied to CollisionManager.
     * @return Result of make key in CollisionManager.
     */
    static inline constexpr std::uint64_t MakeKey_(EntityID idA, EntityID idB) noexcept
    {
        const uint64_t lo = (uint64_t)(idA < idB ? idA : idB);
        const uint64_t hi = (uint64_t)(idA < idB ? idB : idA);
        return (hi << 32) | lo;
    }

    /** @brief Pair info data and utilities used by CollisionManager.
     */
    struct PairInfo
    {
        /** @brief Configured a value used by PairInfo.
         */
        GameObject* a = nullptr;
        /** @brief Configured b value used by PairInfo.
         */
        GameObject* b = nullptr;
        /** @brief Configured col a value used by PairInfo.
         */
        Collider2D* colA = nullptr;
        /** @brief Configured col b value used by PairInfo.
         */
        Collider2D* colB = nullptr;
        /** @brief Configured is trigger pair value used by PairInfo.
         */
        bool isTriggerPair = false;
    };

    /** @brief Stored prev info value used by CollisionManager.
     */
    std::unordered_map<uint64_t, PairInfo> mPrevInfo;
    /** @brief Stored curr info value used by CollisionManager.
     */
    std::unordered_map<uint64_t, PairInfo> mCurrInfo;

    // The pair metadata maps also serve as pair-membership indices. Keeping
    // parallel unordered_sets would require an extra allocation/hash per contact.
    // A removed collider historically suppresses Stay on the next pass while
    // retaining prior metadata long enough to dispatch Exit.
    /** @brief Stored suppress previous pair membership value used by CollisionManager.
     */
    bool mSuppressPreviousPairMembership = false;

    /** @brief Configured n broadphase tests this frame value used by CollisionManager.
     */
    int nBroadphaseTestsThisFrame = 0;
    /** @brief Configured n narrowphase tests this frame value used by CollisionManager.
     */
    int nNarrowphaseTestsThisFrame = 0;
    /** @brief Configured n contacts built this frame value used by CollisionManager.
     */
    size_t nContactsBuiltThisFrame = 0;

    /** @brief Build contacts in CollisionManager.
     * @param out Caller-provided output destination.
     */
    void BuildContacts_(std::vector<NarrowContact>& out) noexcept;
    /** @brief Dispatch in CollisionManager.
     * @param contacts Contacts supplied to CollisionManager.
     */
    void Dispatch_(const std::vector<NarrowContact>& contacts) noexcept;

    /** @brief Should test in CollisionManager.
     * @param a First input operand.
     * @param b Second input operand.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    static bool ShouldTest(const Collider2D& a, const Collider2D& b) noexcept;

    // Narrow-phase:
    /** @brief Test obb obb in CollisionManager.
     * @param A A supplied to CollisionManager.
     * @param B B supplied to CollisionManager.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    static bool TestOBB_OBB(const Collider2D::OrientedBox2D& A,
        const Collider2D::OrientedBox2D& B) noexcept;   // SAT 2D
    /** @brief Test circle circle in CollisionManager.
     * @param ca Ca supplied to CollisionManager.
     * @param ra Ra supplied to CollisionManager.
     * @param cb Cb supplied to CollisionManager.
     * @param rb Rb supplied to CollisionManager.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    static bool TestCircle_Circle(const Vec2& ca, float ra,
        const Vec2& cb, float rb) noexcept;
    /** @brief Test obb circle in CollisionManager.
     * @param box Box supplied to CollisionManager.
     * @param cc Cc supplied to CollisionManager.
     * @param r Target rectangle in the relevant coordinate space.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    static bool TestOBB_Circle(const Collider2D::OrientedBox2D& box,
        const Vec2& cc, float r) noexcept;

    // Broad phase (already implemented): AABB vs AABB
    /** @brief Test box box in CollisionManager.
     * @param a First input operand.
     * @param b Second input operand.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    static bool TestBoxBox(const Rect& a, const Rect& b) noexcept;

    /** @brief Contact obb circle in CollisionManager.
     * @param cc Cc supplied to CollisionManager.
     * @param r Target rectangle in the relevant coordinate space.
     * @param bc Bc supplied to CollisionManager.
     * @param half Half supplied to CollisionManager.
     * @param rotDeg Rot deg supplied to CollisionManager.
     * @param out Caller-provided output destination.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Contact_OBB_Circle(const Vec2& cc, float r, const Vec2& bc, const Vec2& half, float rotDeg, ContactPoint& out) noexcept;

    /** @brief Configured axis value used by MinAxisResult.
     */
    struct MinAxisResult { Vec2 axis; float depth; bool valid; };
    /** @brief Sat min axis in CollisionManager.
     * @param A A supplied to CollisionManager.
     * @param B B supplied to CollisionManager.
     * @return Result of sat min axis in CollisionManager.
     */
    MinAxisResult SAT_MinAxis(const Collider2D::OrientedBox2D& A, const Collider2D::OrientedBox2D& B);

    /** @brief Contact obb obb in CollisionManager.
     * @param ac Ac supplied to CollisionManager.
     * @param aHalf A half supplied to CollisionManager.
     * @param aRotDeg A rot deg supplied to CollisionManager.
     * @param bc Bc supplied to CollisionManager.
     * @param bHalf B half supplied to CollisionManager.
     * @param bRotDeg B rot deg supplied to CollisionManager.
     * @param out Caller-provided output destination.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Contact_OBB_OBB(const Vec2& ac, const Vec2& aHalf, float aRotDeg,
        const Vec2& bc, const Vec2& bHalf, float bRotDeg,
        ContactPoint& out) noexcept;

    /** @brief Contact circle circle in CollisionManager.
     * @param ca Ca supplied to CollisionManager.
     * @param ra Ra supplied to CollisionManager.
     * @param cb Cb supplied to CollisionManager.
     * @param rb Rb supplied to CollisionManager.
     * @param out Caller-provided output destination.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Contact_Circle_Circle(const Vec2& ca, float ra,
        const Vec2& cb, float rb,
        ContactPoint& out) noexcept;
};

/** @name Co-declared engine member descriptions
 * @brief Members sharing declarations with other fields.
 * @{ */
/** @var Vec2 CollisionManager::MinAxisResult::axis
 * @brief Axis used by MinAxisResult.
 */

/** @var float CollisionManager::MinAxisResult::depth
 * @brief Depth used by MinAxisResult.
 */

/** @var bool CollisionManager::MinAxisResult::valid
 * @brief Valid used by MinAxisResult.
 */
/** @} */

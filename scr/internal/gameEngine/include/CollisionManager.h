#pragma once

#include <unordered_map>
#include <unordered_set>

#include "Singleton.h"
#include "GameObject.h"
#include "Collider2D.h"
#include "SceneManager.h"
#include "Engine.h"

struct NarrowContact {
    GameObject* a = nullptr;
    GameObject* b = nullptr;
    Collider2D* colA = nullptr;
    Collider2D* colB = nullptr;
    bool isTriggerPair = false;
    ContactPoint contact;
};

// Forward declarations
class PhysicsManager;

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
    // No resuelve posiciones ni físicas.
    void DetectAndDispatch() noexcept;

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

    std::unordered_set<uint64_t> mPrevPairs;
    std::unordered_set<uint64_t> mCurrPairs;

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

    // Broad-phase (ya lo tienes): AABB contra AABB
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
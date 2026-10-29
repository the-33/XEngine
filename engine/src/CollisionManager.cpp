#include "CollisionManager.h"

#include <vector>
#include <cmath>
#include <cfloat>  // FLT_MAX
#include <algorithm>
#include <thread>
#include <functional>
#include <new>
#include <chrono>
#include <bit>
#include "XEngine/detail/SpatialHashBroadphase.h"
#include <limits>

#include "Collider2D.h"
#include "GameObject.h"
#include "RigidBody2D.h"

using std::vector;

// =================== Internal geometry helpers ===================
static inline float AbsDot(Vec2 vecA, Vec2 vecB) noexcept {
    return std::fabs(math::Dot(vecA, vecB));
}

static inline bool Overlap1D(float aCenter, float aExt, float bCenter, float bExt) noexcept {
    return std::fabs(aCenter - bCenter) <= (aExt + bExt);
}

static inline float DegToRad(float d) { return d * 3.14159265358979323846f / 180.f; }

static inline void BuildAxes(float deg, Vec2& ux, Vec2& uy) {
    const float r = DegToRad(deg);
    const float c = std::cos(r), s = std::sin(r);
    ux = { c, s };     // eje X local
    uy = { -s, c };    // eje Y local
}

static inline float ClampF(float x, float a, float b) noexcept {
    return (x < a) ? a : (x > b) ? b : x;
}

static inline Vec2 ClampV(const Vec2& v, const Vec2& a, const Vec2& b) noexcept {
    return { ClampF(v.x, a.x, b.x), ClampF(v.y, a.y, b.y) };
}

struct Basis2 {
    Vec2 x; // axisX
    Vec2 y; // axisY
};

static inline Basis2 OBBBases(float rotDeg) noexcept
{
    const float r = DegToRad(rotDeg);
    const float c = std::cos(r);
    const float s = std::sin(r);
    // eje X rotado, eje Y rotado (ortonormales)
    return { Vec2{c, s}, Vec2{-s, c} };
}

static inline Vec2 WorldToLocal(const Vec2& pWorld, const Vec2& centerWorld, const Basis2& b) noexcept
{
    const Vec2 d = pWorld - centerWorld;
    // coordinates in the box basis: (dot(d,axisX), dot(d,axisY))
    return { math::Dot(d, b.x), math::Dot(d, b.y) };
}

static inline Vec2 LocalToWorld(const Vec2& pLocal, const Vec2& centerWorld, const Basis2& b) noexcept
{
    return centerWorld + b.x * pLocal.x + b.y * pLocal.y;
}

static inline Vec2 NormalizeSafe(const Vec2& v, const Vec2& fallback = Vec2{ 1,0 }) noexcept
{
    float ls = math::Dot(v, v);
    if (ls <= 1e-12f) return fallback;
    float inv = 1.0f / std::sqrt(ls);
    return v * inv;
}

// ---------- CIRCLE-CIRCLE ----------
bool CollisionManager::Contact_Circle_Circle(const Vec2& ca, float ra,
    const Vec2& cb, float rb,
    ContactPoint& out) noexcept
{
    Vec2 d = cb - ca;
    float distSq = math::Dot(d, d);
    float r = ra + rb;

    if (distSq >= r * r)
        return false;

    float dist = std::sqrt(std::max(distSq, 1e-8f));
    Vec2 n = (dist > 1e-6f) ? (d / dist) : Vec2{ 1, 0 };

    out.normalA = n;
    out.penetration = r - dist;

    // Actual contact point (better for torque)
    out.point = ca + n * (ra - out.penetration * 0.5f);

    return true;
}

// ---------- CIRCLE-OBB ----------
// Return normalA pointing from CIRCLE (A) toward OBB (B): circle->box
bool CollisionManager::Contact_OBB_Circle(const Vec2& cc, float r,
    const Vec2& bc, const Vec2& half,
    float rotDeg,
    ContactPoint& out) noexcept
{
    const Basis2 b = OBBBases(rotDeg);

    // 1) transform circle into box-local space
    const Vec2 cLocal = WorldToLocal(cc, bc, b);

    // 2) closest box-local point to the circle center
    const Vec2 minL = { -half.x, -half.y };
    const Vec2 maxL = { half.x,  half.y };
    const Vec2 closestLocal = ClampV(cLocal, minL, maxL);

    // 3) vuelve a mundo
    const Vec2 closestWorld = LocalToWorld(closestLocal, bc, b);

    // 4) vector from box to circle (for A->B normal when A=circle)
    Vec2 d = closestWorld - cc; // from circle to the box point (check the sign)
    float distSq = math::Dot(d, d);

    // General case: circle outside the box
    if (distSq > 1e-12f)
    {
        float dist = std::sqrt(distSq);
        // A->B normal (A=circle, B=box): points from circle to box
        Vec2 n = d / dist;

        out.normalA = n;
        out.penetration = r - dist;
        if (out.penetration <= 0.f) return false;

        // Actual contact point: closest point on the box
        out.point = closestWorld;
        return true;
    }

    // Special case: circle center is INSIDE (or nearly inside) the box.
    // Here closestLocal == cLocal and d ~= 0; choose the normal toward the nearest face.
    {
        // distance to each face in local space
        float dx = half.x - std::abs(cLocal.x);
        float dy = half.y - std::abs(cLocal.y);

        Vec2 nLocal;
        Vec2 pLocal = cLocal;

        constexpr float kTieEps = 1e-4f;

        if (std::abs(dx - dy) <= kTieEps) {
            // stable tie-break based on the component farther from the center
            if (std::abs(cLocal.x) > std::abs(cLocal.y)) {
                // trata como vertical
                dx = dy - 1.f; // or enter the vertical branch directly
            }
            else {
                // trata como horizontal
                dy = dx - 1.f;
            }
        }

        if (dx < dy) {
            // closer to a vertical face
            nLocal = { (cLocal.x >= 0.f) ? 1.f : -1.f, 0.f };
            pLocal.x = (cLocal.x >= 0.f) ? half.x : -half.x;
        }
        else {
            // closer to a horizontal face
            nLocal = { 0.f, (cLocal.y >= 0.f) ? 1.f : -1.f };
            pLocal.y = (cLocal.y >= 0.f) ? half.y : -half.y;
        }

        // world-space normal (A=circle -> B=box). If the circle is inside, push it outward:
        const Vec2 nWorld = NormalizeSafe(b.x * nLocal.x + b.y * nLocal.y);

        // penetration: distance needed to move the circle outward until it touches
        // = radius + distance from center to face (along that normal)
        const float faceDist = (dx < dy) ? dx : dy;
        out.penetration = std::max(r + faceDist, 0.0f);

        out.normalA = nWorld;

        // contact point: point on the face (world space)
        out.point = LocalToWorld(pLocal, bc, b);
        return true;
    }
}

// ---------- OBB-OBB (SAT, minimum penetration axis) ---------
CollisionManager::MinAxisResult CollisionManager::SAT_MinAxis(const Collider2D::OrientedBox2D& A,
    const Collider2D::OrientedBox2D& B) {
    Vec2 Ax, Ay, Bx, By; BuildAxes(A.angleDeg, Ax, Ay); BuildAxes(B.angleDeg, Bx, By);
    Vec2 T = B.center - A.center;

    auto projExt = [](const Collider2D::OrientedBox2D& box, const Vec2& axis,
        const Vec2& bx, const Vec2& by) {
            return box.half.x * std::fabs(math::Dot(bx, axis)) +
                box.half.y * std::fabs(math::Dot(by, axis));
        };

    const Vec2 axes[4] = { Ax, Ay, Bx, By };
    float bestPen = FLT_MAX;
    Vec2 bestAxis{};
    for (int i = 0; i < 4; ++i) {
        Vec2 a = axes[i];
        float dist = std::fabs(math::Dot(T, a));
        float ra = projExt(A, a, Ax, Ay);
        float rb = projExt(B, a, Bx, By);
        float overlap = (ra + rb) - dist;
        if (overlap < 0.f) return { {}, 0.f, false }; // separados
        if (overlap < bestPen) { bestPen = overlap; bestAxis = a; }
    }
    if (math::Dot(T, bestAxis) < 0.f) bestAxis = -bestAxis; // orienta A->B
    return { bestAxis, bestPen, true };
}

static inline float ProjectRadius(const Vec2& axis, const Basis2& b, const Vec2& half) noexcept
{
    // extents proyectados en axis: |dot(axis,ax)|*hx + |dot(axis,ay)|*hy
    return std::abs(math::Dot(axis, b.x)) * half.x + std::abs(math::Dot(axis, b.y)) * half.y;
}

static inline Vec2 SupportPointOBB(const Vec2& center, const Basis2& b, const Vec2& half, const Vec2& dir) noexcept
{
    // most extreme point in direction dir
    const float sx = (math::Dot(dir, b.x) >= 0.f) ? half.x : -half.x;
    const float sy = (math::Dot(dir, b.y) >= 0.f) ? half.y : -half.y;
    return center + b.x * sx + b.y * sy;
}

bool CollisionManager::Contact_OBB_OBB(const Vec2& ac, const Vec2& aHalf, float aRotDeg, const Vec2& bc, const Vec2& bHalf, float bRotDeg, ContactPoint& out) noexcept
{
    const Basis2 aB = OBBBases(aRotDeg);
    const Basis2 bB = OBBBases(bRotDeg);

    // ejes candidatos SAT (4): ax, ay, bx, by
    Vec2 axes[4] = { aB.x, aB.y, bB.x, bB.y };

    const Vec2 d = bc - ac;

    float bestPen = 1e30f;
    Vec2 bestAxis = { 1,0 };

    constexpr float kTieEps = 1e-4f; // adjust (1e-5..1e-3 depending on world scale)

    int bestIndex = 0;

    for (int i = 0; i < 4; ++i)
    {
        Vec2 axis = NormalizeSafe(axes[i]);

        const float ra = ProjectRadius(axis, aB, aHalf);
        const float rb = ProjectRadius(axis, bB, bHalf);
        const float dist = std::abs(math::Dot(d, axis));

        const float pen = (ra + rb) - dist;
        if (pen <= 0.f) return false;

        const bool better = (pen < bestPen - kTieEps);

        const bool tie = (std::abs(pen - bestPen) <= kTieEps);
        const bool preferThis =
            tie &&
            // Deterministic rule: prefer the axes of A
            ((i < 2) && !(bestIndex < 2)); // bestIndex must be stored

        if (better || preferThis || (tie && i < bestIndex))
        {
            bestPen = pen;
            bestIndex = i;

            const float s = (math::Dot(d, axis) >= 0.f) ? 1.f : -1.f;
            bestAxis = axis * s;
        }
    }

    out.normalA = bestAxis;
    out.penetration = bestPen;

    // Approximate contact point, but very useful for torque:
    // use support along the normal on A and opposite normal on B, then average
    const float ra = ProjectRadius(out.normalA, aB, aHalf);
    const float rb = ProjectRadius(out.normalA, bB, bHalf);

    const Vec2 pA = ac + out.normalA * ra;
    const Vec2 pB = bc - out.normalA * rb;

    out.point = (pA + pB) * 0.5f;

    return true;
}

// =================== Public API ===================
bool CollisionManager::Init() noexcept {
    mRegisteredColliders.clear();
    mPrevInfo.clear();
    mCurrInfo.clear();
    mSuppressPreviousPairMembership = false;
    mSpatialContextClassifier.Reset();
    mPreviousAutoContext = 0xFFFFFFFFu;
    return true;
}

void CollisionManager::Shutdown() noexcept {
    EnableParallelCollision(false); // Join workers before releasing collision state.
    ClearAll();
}

void CollisionManager::ClearAll() noexcept {
    mRegisteredColliders.clear();
    mPrevInfo.clear();
    mCurrInfo.clear();
    mSuppressPreviousPairMembership = false;
    mSpatialContextClassifier.Reset();
    mPreviousAutoContext = 0xFFFFFFFFu;
}

void CollisionManager::RegisterCollider(Collider2D* c) noexcept {
    if (!c) return;
    // Active by default when the Behaviour is enabled (lifecycle-controlled); store the logical flag here
    mRegisteredColliders[c] = true;
}

void CollisionManager::RemoveCollider(Collider2D* c) noexcept {
    if (!c) return;
    mRegisteredColliders.erase(c);
    // Preserve the original removal semantics: the next dispatch must not
    // produce Stay for an old pair, but old metadata is needed for Exit.
    if (!mPrevInfo.empty()) mSuppressPreviousPairMembership = true;
}

void CollisionManager::SetColliderActive(Collider2D* c, bool active) noexcept {
    if (!c) return;
    auto it = mRegisteredColliders.find(c);
    if (it != mRegisteredColliders.end()) it->second = active;
}

void CollisionManager::DetectAndDispatch() noexcept {
    vector<NarrowContact> contacts;
    contacts.reserve(128);

    mCurrInfo.clear();
    BuildContacts_(contacts);
    nContactsBuiltThisFrame = contacts.size();
    Dispatch_(contacts);

    // Pair metadata is the only membership index; Exit is handled in Dispatch_.
    mPrevInfo.swap(mCurrInfo);
    mSuppressPreviousPairMembership = false;
}

// =================== Internos ===================

bool CollisionManager::ShouldTest(const Collider2D& a, const Collider2D& b) noexcept {
    if (&a == &b) return false;
    if (!a.mGameObject || !b.mGameObject) return false;
    if (a.mGameObject == b.mGameObject) return false; // no colisionar contra uno mismo
    RigidBody2D* ra = a.GetAttachedBody();
    RigidBody2D* rb = b.GetAttachedBody();
    if (ra && rb && ra == rb) return false; // mismo rigidbody => ignorar

    // layers/masks
    if ((a.GetLayer() & b.GetMask()) == 0) return false;
    if ((b.GetLayer() & a.GetMask()) == 0) return false;
    return true;
}

void CollisionManager::ConfigureBroadphase(const std::string& mode, float cellSize, bool autoCellSize) noexcept
{
    mSpatialBroadphase = mode == "grid";
    mAutoBroadphase = mode == "auto";
    mAdaptiveBroadphase.Reset();
    mAdaptiveCellBroadphase.Reset();
    mSpatialContextClassifier.Reset();
    mPreviousAutoContext = 0xFFFFFFFFu;
    mAutoGridCellSize = autoCellSize && mode == "auto";
    mBroadphaseCellSize = (std::isfinite(cellSize) && cellSize >= 0.05f && cellSize <= 4096.f)
                              ? cellSize : 2.f;
}

void CollisionManager::EnableParallelCollision(bool enabled) noexcept
{
    if (!enabled)
    {
        mParallelExecutor.reset();
        return;
    }
    if (mParallelExecutor) return;
    const auto cores = std::thread::hardware_concurrency();
    if (cores < 2) return;
    // Cap the number of dedicated threads to keep small 2D games responsive.
    const unsigned workers = std::min(cores - 1, 3u);
    try
    {
        mParallelExecutor = std::make_unique<ParallelExecutor>(workers);
    }
    catch (...)
    {
        // Thread creation failure: retain the safe single-thread fallback.
        mParallelExecutor.reset();
    }
}

unsigned CollisionManager::ParallelWorkerCount() const noexcept
{
    return mParallelExecutor ? mParallelExecutor->WorkerCount() : 0u;
}

void CollisionManager::ResetFrameProfile_() noexcept
{
    mFrameSnapshotMs = 0.f;
    mFrameGridBuildMs = 0.f;
    mFrameAutoSelectorMs = 0.f;
    mFrameGridPasses = 0;
    mFrameLastGridCellSize = 0.f;
    mFrameAutoCalibrationPasses = 0;
    mFrameAutoContextLast = 0;
    mFrameAutoContextTransitions = 0;
    mFrameAutoGridCell1Passes = 0;
    mFrameAutoGridCell2Passes = 0;
    mFrameAutoGridCell4Passes = 0;
    mFrameBruteforcePasses = 0;
    mFrameContactSignature = 14695981039346656037ULL;
    mFrameCandidateMs = 0.f;
    mFrameMergeMs = 0.f;
    mFrameBroadphaseTests = 0;
    mFrameNarrowphaseTests = 0;
    mFrameContactsBuilt = 0;
    mFrameCollisionPasses = 0;
    mFrameParallelPasses = 0;
    mLastSnapshotMs = 0.f;
    mLastGridBuildMs = 0.f;
    mLastCandidateMs = 0.f;
    mLastMergeMs = 0.f;
    mLastUsedParallelCandidates = false;
    nBroadphaseTestsThisFrame = 0;
    nNarrowphaseTestsThisFrame = 0;
    nContactsBuiltThisFrame = 0;
}

void CollisionManager::AccumulatePassProfile_(std::size_t contactsBuilt) noexcept
{
    mFrameSnapshotMs += mLastSnapshotMs;
    mFrameGridBuildMs += mLastGridBuildMs;
    mFrameCandidateMs += mLastCandidateMs;
    mFrameMergeMs += mLastMergeMs;
    mFrameBroadphaseTests += static_cast<std::uint64_t>(nBroadphaseTestsThisFrame);
    mFrameNarrowphaseTests += static_cast<std::uint64_t>(nNarrowphaseTestsThisFrame);
    mFrameContactsBuilt += static_cast<std::uint64_t>(contactsBuilt);
    ++mFrameCollisionPasses;
    if (mLastUsedGrid) {
        ++mFrameGridPasses;
        mFrameLastGridCellSize = mLastGridCellSize;
    }
    else ++mFrameBruteforcePasses;
    if (mLastUsedParallelCandidates) ++mFrameParallelPasses;
}

void CollisionManager::BuildContacts_(std::vector<NarrowContact>& out) noexcept
{
    nBroadphaseTestsThisFrame = 0;
    nNarrowphaseTestsThisFrame = 0;

    std::vector<Collider2D*> active;
    active.reserve(mRegisteredColliders.size());
    for (auto& [c, isActive] : mRegisteredColliders)
        if (c && isActive) active.push_back(c);

    std::sort(active.begin(), active.end(),
        [](const Collider2D* a, const Collider2D* b)
        {
            const EntityID ida = (a && a->mGameObject) ? a->mGameObject->GetID() : 0;
            const EntityID idb = (b && b->mGameObject) ? b->mGameObject->GetID() : 0;
            if (ida != idb) return ida < idb;
            return a < b; // desempate estable
        });

    const auto snapshotStart = std::chrono::steady_clock::now();
    mLastSnapshotMs = 0.f;
    mLastGridBuildMs = 0.f;
    mLastCandidateMs = 0.f;
    mLastMergeMs = 0.f;
    mLastUsedParallelCandidates = false;
    mLastGridCellSize = 0.f;
    // Empty passes still report the selected fixed algorithm consistently.
    mLastUsedGrid = mSpatialBroadphase;

    // Snapshot all collider state before launching workers. No worker accesses
    // Scene, Transform, GameObject or Collider2D methods. Callback dispatch
    // and historical collision-pair updates happen on the main thread.
    struct Geometry
    {
        Collider2D* collider = nullptr;
        GameObject* gameObject = nullptr;
        GameObject* owner = nullptr;
        RigidBody2D* body = nullptr;
        Collider2D::Shape shape = Collider2D::Shape::Box;
        LayerBits layer = 0;
        LayerBits mask = 0;
        bool trigger = false;
        Rect aabb{};
        Collider2D::OrientedBox2D obb{};
        Collider2D::Circle2D circle{};
    };

    std::vector<Geometry> geometry;
    geometry.reserve(active.size());
    for (auto* collider : active)
    {
        Geometry g;
        g.collider = collider;
        g.gameObject = collider->mGameObject;
        if (!g.gameObject) continue;
        g.body = collider->GetAttachedBody();
        g.owner = g.body ? g.body->gameObject : g.gameObject;
        g.layer = collider->GetLayer();
        g.mask = collider->GetMask();
        g.trigger = collider->GetIsTrigger();
        g.shape = collider->GetShape();
        g.aabb = collider->WorldAABB();
        if (g.shape == Collider2D::Shape::Circle)
            g.circle = collider->WorldCircle();
        else
            g.obb = collider->WorldOBB();
        geometry.push_back(g);
    }

    const std::size_t n = geometry.size();
    mLastSnapshotMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - snapshotStart).count();
    if (n < 2)
    {
        AccumulatePassProfile_(out.size());
        return;
    }

    // Build immutable cell buckets ONCE on the main thread. Workers only read
    // the grid. Oversized/nonfinite objects use a conservative all-pairs path.
    const bool hasWorkers = mParallelExecutor && n >= 192;
    // Coarse spatial occupancy class segregates calibrations for dense and
    // sparse layouts at identical collider counts. No thresholds here choose
    // an algorithm: they only segregate independently timed observations.
    const auto selectorStart = std::chrono::steady_clock::now();
    unsigned densityClass = 0;
    if (mAutoBroadphase && mAutoGridCellSize) {
        double minX=std::numeric_limits<double>::infinity();
        double minY=minX, maxX=-minX, maxY=-minX, totalArea=0.;
        for (const auto& g:geometry) {
            const double x=g.aabb.x, y=g.aabb.y;
            const double right=static_cast<double>(g.aabb.Right());
            const double bottom=static_cast<double>(g.aabb.Bottom());
            if (!std::isfinite(x) || !std::isfinite(y) ||
                !std::isfinite(right) || !std::isfinite(bottom)) continue;
            minX=std::min(minX,x); minY=std::min(minY,y);
            maxX=std::max(maxX,right); maxY=std::max(maxY,bottom);
            totalArea+=std::max(0.,right-x)*std::max(0.,bottom-y);
        }
        const double sceneArea=(maxX-minX)*(maxY-minY);
        const double occupancy = std::isfinite(sceneArea) && sceneArea > 0.
            ? totalArea / sceneArea : 0.;
        double effectiveBins = 0.;
        // A 4x4 histogram distinguishes concentration changes even when
        // collider counts and the global occupied area are unchanged.
        if (std::isfinite(maxX-minX) && std::isfinite(maxY-minY) &&
            maxX>minX && maxY>minY) {
            const double scaleX=4.0/(maxX-minX), scaleY=4.0/(maxY-minY);
            std::array<unsigned,16> bins{};
            for (const auto& g : geometry) {
                const double x = double(g.aabb.x) + 0.5 * double(g.aabb.w);
                const double y = double(g.aabb.y) + 0.5 * double(g.aabb.h);
                if (!std::isfinite(x) || !std::isfinite(y)) continue;
                const unsigned bx=static_cast<unsigned>(std::clamp(
                    static_cast<int>((x-minX)*scaleX),0,3));
                const unsigned by=static_cast<unsigned>(std::clamp(
                    static_cast<int>((y-minY)*scaleY),0,3));
                ++bins[by*4+bx];
            }
            effectiveBins = SpatialContextClassifier::EffectiveBins(bins.data(), bins.size());
        }
        densityClass = mSpatialContextClassifier.Observe(occupancy, effectiveBins,
                                                         n, hasWorkers);
    }
    AdaptiveCellBroadphase::Choice autoChoice{};
    const bool adaptiveCells = mAutoBroadphase && mAutoGridCellSize;
    bool useGrid = false;
    if (adaptiveCells) {
        if (mPreviousAutoContext != 0xFFFFFFFFu && mPreviousAutoContext != densityClass)
            ++mFrameAutoContextTransitions;
        mPreviousAutoContext = densityClass;
        mFrameAutoContextLast = densityClass;
        autoChoice = mAdaptiveCellBroadphase.Choose(n, hasWorkers, densityClass);
        useGrid = autoChoice.strategy != 0;
        if (autoChoice.probe) ++mFrameAutoCalibrationPasses;
    } else {
        useGrid = mAutoBroadphase ? mAdaptiveBroadphase.Choose(n, hasWorkers)
                                  : mSpatialBroadphase;
    }
    if (mAutoBroadphase)
        mFrameAutoSelectorMs += std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - selectorStart).count();
    mLastUsedGrid = useGrid;
    const float chosenCellSize = adaptiveCells && useGrid
        ? AdaptiveCellBroadphase::CellSize(autoChoice.strategy, mBroadphaseCellSize)
        : mBroadphaseCellSize;
    mLastGridCellSize = useGrid ? chosenCellSize : 0.f;
    if (adaptiveCells && useGrid) {
        if (autoChoice.strategy==1) ++mFrameAutoGridCell1Passes;
        if (autoChoice.strategy==2) ++mFrameAutoGridCell2Passes;
        if (autoChoice.strategy==3) ++mFrameAutoGridCell4Passes;
    }
    SpatialHashBroadphase::Grid grid(chosenCellSize);
    if (useGrid)
    {
        const auto gridStart = std::chrono::steady_clock::now();
        std::vector<SpatialHashBroadphase::Bounds> bounds;
        bounds.reserve(n);
        for (const auto& g: geometry)
            bounds.push_back({g.aabb.x, g.aabb.y,
                              static_cast<double>(g.aabb.Right()),
                              static_cast<double>(g.aabb.Bottom())});
        grid.Build(bounds);
        mLastGridBuildMs = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - gridStart).count();
    }

    // One result slot per outer index guarantees deterministic contact order
    // independent of which worker finishes first.
    std::vector<std::vector<NarrowContact>> perColliderContacts(n);
    std::vector<int> broadphaseCounts(n, 0);
    std::vector<int> narrowphaseCounts(n, 0);

    const auto processCollider = [&](std::size_t i)
    {
        const Geometry& a = geometry[i];
        auto& localContacts = perColliderContacts[i];
        int broadphase = 0;
        int narrowphase = 0;
        std::vector<std::size_t> candidates;
        if (useGrid) grid.Candidates(i, n, candidates);
        const std::size_t candidateCount = useGrid ? candidates.size() : n - i - 1;
        for (std::size_t pos = 0; pos < candidateCount; ++pos)
        {
            const std::size_t j = useGrid ? candidates[pos] : i + pos + 1;
            const Geometry& b = geometry[j];
            if (a.gameObject == b.gameObject || !a.owner || !b.owner || a.owner == b.owner) continue;
            if (a.body && a.body == b.body) continue;
            if (!(a.layer & b.mask) || !(b.layer & a.mask)) continue;

            ++broadphase;
            if (!TestBoxBox(a.aabb, b.aabb)) continue;
            ++narrowphase;

            ContactPoint contact{};
            bool hit = false;
            if (a.shape == Collider2D::Shape::Circle && b.shape == Collider2D::Shape::Circle)
            {
                hit = Contact_Circle_Circle(a.circle.center, a.circle.radius,
                                            b.circle.center, b.circle.radius, contact);
            }
            else if (a.shape == Collider2D::Shape::Box && b.shape == Collider2D::Shape::Box)
            {
                hit = Contact_OBB_OBB(a.obb.center, a.obb.half, a.obb.angleDeg,
                                       b.obb.center, b.obb.half, b.obb.angleDeg, contact);
            }
            else if (a.shape == Collider2D::Shape::Circle)
            {
                hit = Contact_OBB_Circle(a.circle.center, a.circle.radius,
                                          b.obb.center, b.obb.half, b.obb.angleDeg, contact);
            }
            else
            {
                hit = Contact_OBB_Circle(b.circle.center, b.circle.radius,
                                          a.obb.center, a.obb.half, a.obb.angleDeg, contact);
                if (hit) contact.normalA = -contact.normalA;
            }
            if (!hit) continue;

            NarrowContact nc;
            nc.a = a.owner;
            nc.b = b.owner;
            nc.colA = a.collider;
            nc.colB = b.collider;
            nc.isTriggerPair = a.trigger || b.trigger;
            nc.contact = contact;
            localContacts.emplace_back(nc);
        }
        broadphaseCounts[i] = broadphase;
        narrowphaseCounts[i] = narrowphase;
    };

    // Avoid the synchronization cost in small scenes. The main thread
    // participates in larger workloads instead of waiting idly.
    const auto candidateStart = std::chrono::steady_clock::now();
    // The O(n^2) workload becomes worth dispatching only for larger scenes.
    if (mParallelExecutor && n >= 192)
    {
        mLastUsedParallelCandidates = true;
        const std::function<void(std::size_t)> task = processCollider;
        mParallelExecutor->Run(n, task);
    }
    else
    {
        for (std::size_t i = 0; i < n; ++i) processCollider(i);
    }

    mLastCandidateMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - candidateStart).count();
    const auto observationStart = std::chrono::steady_clock::now();
    if (adaptiveCells)
        mAdaptiveCellBroadphase.Observe(n, hasWorkers, densityClass,
                                       autoChoice.strategy,
                                       mLastGridBuildMs + mLastCandidateMs);
    else if (mAutoBroadphase)
        mAdaptiveBroadphase.Observe(n, hasWorkers, useGrid,
                                   mLastGridBuildMs + mLastCandidateMs);
    if (mAutoBroadphase)
        mFrameAutoSelectorMs += std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - observationStart).count();
    const auto mergeStart = std::chrono::steady_clock::now();
    // Avoid geometric reallocations of a large ordered contact vector and
    // rehash storms in the pair index under very dense overlap workloads.
    std::size_t contactCount = 0;
    for (const auto& group : perColliderContacts) contactCount += group.size();
    out.reserve(out.size() + contactCount);
    mCurrInfo.reserve(contactCount);
    // Only the main thread writes the pair map and dispatches callbacks.
    for (std::size_t i = 0; i < n; ++i)
    {
        nBroadphaseTestsThisFrame += broadphaseCounts[i];
        nNarrowphaseTestsThisFrame += narrowphaseCounts[i];
        for (const auto& nc : perColliderContacts[i])
        {
            out.emplace_back(nc);
            // FNV-1a of ordered entity pairs and trigger state, accumulated
            // across ALL collision passes in the frame. No pointer addresses.
            const auto hashPart = [&](std::uint64_t value) {
                for (unsigned k=0;k<8;++k) {
                    mFrameContactSignature ^= (value & 255ULL);
                    mFrameContactSignature *= 1099511628211ULL;
                    value >>= 8;
                }
            };
            hashPart(static_cast<std::uint64_t>(nc.a->GetID()));
            hashPart(static_cast<std::uint64_t>(nc.b->GetID()));
            hashPart(nc.isTriggerPair ? 1ULL : 0ULL);
            // Include complete collision response geometry to detect differences
            // in contact depth, normals or points, not only pair counts.
            hashPart(std::bit_cast<std::uint32_t>(nc.contact.penetration));
            hashPart(std::bit_cast<std::uint32_t>(nc.contact.normalA.x));
            hashPart(std::bit_cast<std::uint32_t>(nc.contact.normalA.y));
            hashPart(std::bit_cast<std::uint32_t>(nc.contact.point.x));
            hashPart(std::bit_cast<std::uint32_t>(nc.contact.point.y));
            const uint64_t key = MakeKey_(nc.a->GetID(), nc.b->GetID());
            mCurrInfo.insert_or_assign(key, PairInfo{
                nc.a, nc.b, nc.colA, nc.colB, nc.isTriggerPair
            });
        }
    }
    mLastMergeMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - mergeStart).count();
    AccumulatePassProfile_(out.size());
}

static inline ContactPoint FlipContactForB_(const ContactPoint& cp) noexcept
{
    ContactPoint out = cp;
    out.normalA = -cp.normalA;   // normal for B->A
    // if normalB is used, reverse it as well:
    out.normalB = -cp.normalB;
    return out;
}

void CollisionManager::Dispatch_(const std::vector<NarrowContact>& contacts) noexcept
{
    // --------------- ENTER / STAY ---------------
    for (const auto& c : contacts)
    {
        if (!c.a || !c.b) continue;

        const uint64_t key = MakeKey_(c.a->GetID(), c.b->GetID());
        const bool was = !mSuppressPreviousPairMembership &&
                         (mPrevInfo.find(key) != mPrevInfo.end());

        CollisionInfo2D infoAB;
        infoAB.self = c.a;
        infoAB.other = c.b;
        infoAB.selfCollider = c.colA;
        infoAB.otherCollider = c.colB;
        infoAB.contacts.clear();

        CollisionInfo2D infoBA;
        infoBA.self = c.b;
        infoBA.other = c.a;
        infoBA.selfCollider = c.colB;
        infoBA.otherCollider = c.colA;
        infoBA.contacts.clear();

        if (!c.isTriggerPair) {
            infoAB.contacts.push_back(c.contact);
            infoBA.contacts.push_back(FlipContactForB_(c.contact));
        }

        if (c.isTriggerPair)
        {
            if (!was) { c.a->OnTriggerEnter(infoAB); c.b->OnTriggerEnter(infoBA); }
            else { c.a->OnTriggerStay(infoAB);  c.b->OnTriggerStay(infoBA); }
        }
        else
        {
            if (!was) { c.a->OnCollisionEnter(infoAB); c.b->OnCollisionEnter(infoBA); }
            else { c.a->OnCollisionStay(infoAB);  c.b->OnCollisionStay(infoBA); }
        }
    }

    // --------------- EXIT ---------------
    // For each pair present previously but no longer present:
    for (const auto& [key, prev] : mPrevInfo)
    {
        if (mCurrInfo.find(key) != mCurrInfo.end())
            continue;

        if (!prev.a || !prev.b)
            continue;

        CollisionInfo2D ab;
        ab.self = prev.a;
        ab.other = prev.b;
        ab.selfCollider = prev.colA;
        ab.otherCollider = prev.colB;
        ab.contacts.clear();

        CollisionInfo2D ba;
        ba.self = prev.b;
        ba.other = prev.a;
        ba.selfCollider = prev.colB;
        ba.otherCollider = prev.colA;
        ba.contacts.clear();

        if (prev.isTriggerPair)
        {
            prev.a->OnTriggerExit(ab);
            prev.b->OnTriggerExit(ba);
        }
        else
        {
            prev.a->OnCollisionExit(ab);
            prev.b->OnCollisionExit(ba);
        }
    }
}

// =================== Narrow-phase ===================

bool CollisionManager::TestOBB_OBB(const Collider2D::OrientedBox2D& A,
    const Collider2D::OrientedBox2D& B) noexcept {
    // SAT with four axes: local axes of A and B
    Vec2 Ax, Ay, Bx, By;
    BuildAxes(A.angleDeg, Ax, Ay);
    BuildAxes(B.angleDeg, Bx, By);

    const Vec2 T = { B.center.x - A.center.x, B.center.y - A.center.y };

    // Proyecta centros
    const float T_Ax = math::Dot(T, Ax);
    const float T_Ay = math::Dot(T, Ay);
    const float T_Bx = math::Dot(T, Bx);
    const float T_By = math::Dot(T, By);

    // Projected radius components
    const float Ra_Ax = A.half.x;    // projection of A onto its own Ax axis
    const float Ra_Ay = A.half.y;    // en Ay

    const float Rb_Ax = B.half.x * AbsDot(Bx, Ax) + B.half.y * AbsDot(By, Ax);
    const float Rb_Ay = B.half.x * AbsDot(Bx, Ay) + B.half.y * AbsDot(By, Ay);

    const float Ra_Bx = A.half.x * AbsDot(Ax, Bx) + A.half.y * AbsDot(Ay, Bx);
    const float Ra_By = A.half.x * AbsDot(Ax, By) + A.half.y * AbsDot(Ay, By);
    const float Rb_Bx = B.half.x;
    const float Rb_By = B.half.y;

    // Test en 4 ejes
    if (!Overlap1D(0.f, Ra_Ax, T_Ax, Rb_Ax)) return false;
    if (!Overlap1D(0.f, Ra_Ay, T_Ay, Rb_Ay)) return false;
    if (!Overlap1D(0.f, Ra_Bx, T_Bx, Rb_Bx)) return false;
    if (!Overlap1D(0.f, Ra_By, T_By, Rb_By)) return false;

    return true;
}

bool CollisionManager::TestCircle_Circle(const Vec2& ca, float ra,
    const Vec2& cb, float rb) noexcept {
    const float r = ra + rb;
    return math::LengthSq(cb - ca) <= (r * r);
}

bool CollisionManager::TestOBB_Circle(const Collider2D::OrientedBox2D& box,
    const Vec2& cc, float r) noexcept {
    // Transform the circle into box-local space
    const float rad = box.angleDeg * 3.14159265358979323846f / 180.f;
    const float c = std::cos(-rad), s = std::sin(-rad);
    const Vec2 rel = { cc.x - box.center.x, cc.y - box.center.y };
    const Vec2 pLocal = { c * rel.x - s * rel.y, s * rel.x + c * rel.y };

    // Closest point in the local rectangle [-half..half]
    const float qx = std::clamp(pLocal.x, -box.half.x, box.half.x);
    const float qy = std::clamp(pLocal.y, -box.half.y, box.half.y);

    const Vec2 diff = { pLocal.x - qx, pLocal.y - qy };
    return math::LengthSq(diff) <= r * r;
}

// =================== Broad-phase ===================

bool CollisionManager::TestBoxBox(const Rect& a, const Rect& b) noexcept {
    return a.Overlaps(b);
}

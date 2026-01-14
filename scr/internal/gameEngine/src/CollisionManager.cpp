#include "CollisionManager.h"

#include <vector>
#include <cmath>
#include <algorithm>

#include "Collider2D.h"
#include "GameObject.h"
#include "RigidBody2D.h"

using std::vector;

// =================== Utilidades geométricas internas ===================
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
    // coordenadas en base del box: (dot(d,axisX), dot(d,axisY))
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

// ---------- CÍRCULO·CÍRCULO ----------
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

    // Punto REAL de contacto (mejor para torque)
    out.point = ca + n * (ra - out.penetration * 0.5f);

    return true;
}

// ---------- CÍRCULO·OBB ----------
// Devuelve normalA apuntando de CÍRCULO (A) hacia OBB (B): circle->box
bool CollisionManager::Contact_OBB_Circle(const Vec2& cc, float r,
    const Vec2& bc, const Vec2& half,
    float rotDeg,
    ContactPoint& out) noexcept
{
    const Basis2 b = OBBBases(rotDeg);

    // 1) círculo a espacio local del box
    const Vec2 cLocal = WorldToLocal(cc, bc, b);

    // 2) punto más cercano del box (local) al centro del círculo
    const Vec2 minL = { -half.x, -half.y };
    const Vec2 maxL = { half.x,  half.y };
    const Vec2 closestLocal = ClampV(cLocal, minL, maxL);

    // 3) vuelve a mundo
    const Vec2 closestWorld = LocalToWorld(closestLocal, bc, b);

    // 4) vector desde box hacia círculo (para normal A->B si A=circle)
    Vec2 d = closestWorld - cc; // desde círculo hacia el punto del box (ojo signo)
    float distSq = math::Dot(d, d);

    // Caso general: círculo fuera del box
    if (distSq > 1e-12f)
    {
        float dist = std::sqrt(distSq);
        // normal A->B (A=circle, B=box): debe ir de círculo a box
        Vec2 n = d / dist;

        out.normalA = n;
        out.penetration = r - dist;
        if (out.penetration <= 0.f) return false;

        // Punto real de contacto: el punto del box más cercano
        out.point = closestWorld;
        return true;
    }

    // Caso especial: el centro del círculo está DENTRO del box (o casi).
    // Aquí closestLocal == cLocal y d es ~0, así que se elige la normal hacia la cara más cercana.
    {
        // distancia a cada cara en local
        float dx = half.x - std::abs(cLocal.x);
        float dy = half.y - std::abs(cLocal.y);

        Vec2 nLocal;
        Vec2 pLocal = cLocal;

        constexpr float kTieEps = 1e-4f;

        if (std::abs(dx - dy) <= kTieEps) {
            // desempate estable por qué componente está “más lejos” del centro
            if (std::abs(cLocal.x) > std::abs(cLocal.y)) {
                // trata como vertical
                dx = dy - 1.f; // o directamente entra al bloque vertical
            }
            else {
                // trata como horizontal
                dy = dx - 1.f;
            }
        }

        if (dx < dy) {
            // más cerca de cara vertical
            nLocal = { (cLocal.x >= 0.f) ? 1.f : -1.f, 0.f };
            pLocal.x = (cLocal.x >= 0.f) ? half.x : -half.x;
        }
        else {
            // más cerca de cara horizontal
            nLocal = { 0.f, (cLocal.y >= 0.f) ? 1.f : -1.f };
            pLocal.y = (cLocal.y >= 0.f) ? half.y : -half.y;
        }

        // normal en mundo (A=circle -> B=box). Si el círculo está dentro, se empuja hacia fuera:
        const Vec2 nWorld = NormalizeSafe(b.x * nLocal.x + b.y * nLocal.y);

        // penetración: cuánto hay que sacar al círculo para que toque justo
        // = radio + distancia del centro a la cara (en esa normal)
        const float faceDist = (dx < dy) ? dx : dy;
        out.penetration = std::max(r + faceDist, 0.0f);

        out.normalA = nWorld;

        // punto de contacto: punto en la cara (mundo)
        out.point = LocalToWorld(pLocal, bc, b);
        return true;
    }
}

// ---------- OBB·OBB (SAT, eje mínima penetración) ---------
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
    // punto más extremo en dirección dir
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

    constexpr float kTieEps = 1e-4f; // ajusta (1e-5..1e-3 según tu escala)

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
            // Regla determinista: preferir ejes de A
            ((i < 2) && !(bestIndex < 2)); // necesitas guardar bestIndex

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

    // Punto de contacto aproximado pero MUY útil para torque:
    // usar soporte en dirección normal en A y soporte en dirección opuesta en B, y promediar
    const float ra = ProjectRadius(out.normalA, aB, aHalf);
    const float rb = ProjectRadius(out.normalA, bB, bHalf);

    const Vec2 pA = ac + out.normalA * ra;
    const Vec2 pB = bc - out.normalA * rb;

    out.point = (pA + pB) * 0.5f;

    return true;
}

// =================== API pública ===================
bool CollisionManager::Init() noexcept {
    mRegisteredColliders.clear();
    mPrevPairs.clear();
    mCurrPairs.clear();
    mPrevInfo.clear();
    mCurrInfo.clear();
    return true;
}

void CollisionManager::Shutdown() noexcept {
    ClearAll();
}

void CollisionManager::ClearAll() noexcept {
    mRegisteredColliders.clear();
    mPrevPairs.clear();
    mCurrPairs.clear();
    mPrevInfo.clear();
    mCurrInfo.clear();
}

void CollisionManager::RegisterCollider(Collider2D* c) noexcept {
    if (!c) return;
    // Por defecto activo si el Behaviour está enabled (el ciclo de vida lo controla), aquí se guarda el flag lógico
    mRegisteredColliders[c] = true;
}

void CollisionManager::RemoveCollider(Collider2D* c) noexcept {
    if (!c) return;
    mRegisteredColliders.erase(c);
    // Limpia pares pendientes con c
    if (!mPrevPairs.empty()) {
        vector<uint64_t> toErase;
        toErase.reserve(mPrevPairs.size());
        for (auto key : mPrevPairs) {
            // Dado que no hay mapa inverso desde key a ids, se vacía prevPairs.
            // El siguiente frame no habrá Stay y se disparará Exit implícito.
            (void)key;
        }
        mPrevPairs.clear();
    }
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
    mCurrPairs.clear();
    BuildContacts_(contacts);
    nContactsBuiltThisFrame = contacts.size();
    Dispatch_(contacts);

    // Exit: pares que estaban antes y ya no están
    for (auto key : mPrevPairs) {
        if (mCurrPairs.find(key) == mCurrPairs.end()) {
            // No se conocen los punteros A/B aquí; el Exit por pareja se hace en Dispatch_
            // Se guardan keys huérfanos para actuar: solución simple -> no es posible reconstruir info
            // sin mapa adicional. Por lo tanto la notificación de Exit se realiza en Dispatch_ comparando sets.
        }
    }

    // Actualiza histórico
    mPrevPairs.swap(mCurrPairs);
    mPrevInfo.swap(mCurrInfo);
}

// =================== Internos ===================

bool CollisionManager::ShouldTest(const Collider2D& a, const Collider2D& b) noexcept {
    if (&a == &b) return false;
    if (!a.mGameObject || !b.mGameObject) return false;
    if (a.mGameObject == b.mGameObject) return false; // no colisionar contra uno mismo
    RigidBody2D* ra = a.GetAttachedBody();
    RigidBody2D* rb = b.GetAttachedBody();
    if (ra && rb && ra == rb) return false; // mismo rigidbody => ignorar

    // capas/máscaras
    if ((a.GetLayer() & b.GetMask()) == 0) return false;
    if ((b.GetLayer() & a.GetMask()) == 0) return false;
    return true;
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

    const size_t n = active.size();

    for (size_t i = 0; i < n; ++i)
    {
        Collider2D* A = active[i];

        for (size_t j = i + 1; j < n; ++j)
        {
            Collider2D* B = active[j];
            if (!ShouldTest(*A, *B)) continue;

            // Broadphase
            nBroadphaseTestsThisFrame++;
            if (!TestBoxBox(A->WorldAABB(), B->WorldAABB())) continue;

            // Narrow + ContactPoint
            nNarrowphaseTestsThisFrame++;

            ContactPoint cp{};
            bool hit = false;

            const auto sa = A->GetShape();
            const auto sb = B->GetShape();

            if (sa == Collider2D::Shape::Circle && sb == Collider2D::Shape::Circle)
            {
                auto ca = A->WorldCircle();
                auto cb = B->WorldCircle();
                hit = Contact_Circle_Circle(ca.center, ca.radius, cb.center, cb.radius, cp);
            }
            else if (sa == Collider2D::Shape::Box && sb == Collider2D::Shape::Box)
            {
                auto oa = A->WorldOBB();
                auto ob = B->WorldOBB();

                // Ajustar nombres si la OBB no usa center/half/rotDeg
                // Si la OBB tiene size en vez de half:
                // Vec2 aHalf = oa.size * 0.5f;
                // Vec2 bHalf = ob.size * 0.5f;

                hit = Contact_OBB_OBB(
                    oa.center, oa.half, oa.angleDeg,
                    ob.center, ob.half, ob.angleDeg,
                    cp
                );
            }
            else if (sa == Collider2D::Shape::Circle && sb == Collider2D::Shape::Box)
            {
                auto ca = A->WorldCircle();
                auto ob = B->WorldOBB();

                // normal A->B = circle->box
                hit = Contact_OBB_Circle(
                    ca.center, ca.radius,
                    ob.center, ob.half, ob.angleDeg,
                    cp
                );
            }
            else if (sa == Collider2D::Shape::Box && sb == Collider2D::Shape::Circle)
            {
                auto oa = A->WorldOBB();
                auto cb = B->WorldCircle();

                // Llamar a Circle->Box y luego invertir la normal
                // para que quede normal A->B = box->circle
                hit = Contact_OBB_Circle(
                    cb.center, cb.radius,
                    oa.center, oa.half, oa.angleDeg,
                    cp
                );
                if (hit) cp.normalA = -cp.normalA;
            }

            if (!hit) continue;

            NarrowContact nc;

            auto* bodyA = A->GetAttachedBody();
            auto* bodyB = B->GetAttachedBody();

            GameObject* ownerA = bodyA ? bodyA->gameObject : A->mGameObject;
            GameObject* ownerB = bodyB ? bodyB->gameObject : B->mGameObject;

            if (!ownerA || !ownerB) continue;
            if (ownerA == ownerB) continue; // misma entidad física

            nc.a = ownerA;
            nc.b = ownerB;
            nc.colA = A;
            nc.colB = B;
            nc.isTriggerPair = A->GetIsTrigger() || B->GetIsTrigger();
            nc.contact = cp;

            out.emplace_back(nc);
            const uint64_t key = MakeKey_(nc.a->GetID(), nc.b->GetID());
            mCurrPairs.insert(key);

            mCurrInfo[key] = PairInfo{
                nc.a, nc.b,
                nc.colA, nc.colB,
                nc.isTriggerPair
            };
        }
    }
}

static inline ContactPoint FlipContactForB_(const ContactPoint& cp) noexcept
{
    ContactPoint out = cp;
    out.normalA = -cp.normalA;   // normal para B->A
    // si tienes normalB y la usas, también inviértela:
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
        const bool was = (mPrevPairs.find(key) != mPrevPairs.end());

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
    // Para cada par que estaba antes pero ya no está ahora:
    for (const auto& [key, prev] : mPrevInfo)
    {
        if (mCurrPairs.find(key) != mCurrPairs.end())
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
    // SAT con 4 ejes: ejes locales de A y de B
    Vec2 Ax, Ay, Bx, By;
    BuildAxes(A.angleDeg, Ax, Ay);
    BuildAxes(B.angleDeg, Bx, By);

    const Vec2 T = { B.center.x - A.center.x, B.center.y - A.center.y };

    // Proyecta centros
    const float T_Ax = math::Dot(T, Ax);
    const float T_Ay = math::Dot(T, Ay);
    const float T_Bx = math::Dot(T, Bx);
    const float T_By = math::Dot(T, By);

    // Componentes del radio proyectadas
    const float Ra_Ax = A.half.x;    // proyección de A en su propio eje Ax
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
    // Lleva el círculo al espacio local del box
    const float rad = box.angleDeg * 3.14159265358979323846f / 180.f;
    const float c = std::cos(-rad), s = std::sin(-rad);
    const Vec2 rel = { cc.x - box.center.x, cc.y - box.center.y };
    const Vec2 pLocal = { c * rel.x - s * rel.y, s * rel.x + c * rel.y };

    // Punto más cercano en el rect local [-half..half]
    const float qx = std::clamp(pLocal.x, -box.half.x, box.half.x);
    const float qy = std::clamp(pLocal.y, -box.half.y, box.half.y);

    const Vec2 diff = { pLocal.x - qx, pLocal.y - qy };
    return math::LengthSq(diff) <= r * r;
}

// =================== Broad-phase ===================

bool CollisionManager::TestBoxBox(const Rect& a, const Rect& b) noexcept {
    return a.Overlaps(b);
}
#include "PhysicsManager.h"

#include "Rigidbody2D.h"
#include "CollisionManager.h"
#include "Collider2D.h"
#include "Transform.h"
#include "GameObject.h"
#include "TimeManager.h"

static inline double NowSec_() noexcept
{
    if (auto* t = TimeManager::GetInstancePtr())
        return (double)t->timeSinceStart;
    return 0.0;
}

void PhysicsManager::RegisterBody(RigidBody2D* b) noexcept
{
    if (!b) return;
    mBodies[b] = true;

    if (auto* col = b->gameObject->GetComponent<Collider2D>()) {
        if (col->shape == Collider2D::Shape::Circle) {
            float r = col->radius;
            b->SetInertia(0.5f * b->GetMass() * r * r);
        }
        else {
            Vec2 s = (Vec2)col->size;
            float w = s.x, h = s.y;
            b->SetInertia((1.0f / 12.0f) * b->GetMass() * (w * w + h * h));
        }
    }
}

void PhysicsManager::RemoveBody(RigidBody2D* b) noexcept
{
    if (!b) return;
    mBodies.erase(b);
}

void PhysicsManager::SetBodyActive(RigidBody2D* b, bool active) noexcept
{
    if (!b) return;
    auto it = mBodies.find(b);
    if (it != mBodies.end()) it->second = active;
}

int PhysicsManager::ComputeSubsteps_(float dt) noexcept
{
    int steps = 1;

    for (auto& [rb, active] : mBodies)
    {
        if (!rb || !active) continue;
        if (rb->GetBodyType() != RigidBody2D::BodyType::Dynamic) continue;
        if (rb->GetCollisionDetection() != RigidBody2D::CollisionDetection::Continuous) continue;

        GameObject* go = rb->gameObject;
        if (!go) continue;

        auto* col = go->GetComponent<Collider2D>();
        if (!col) continue;

        // tamaño característico
        float size = 1.0f;
        if ((Collider2D::Shape)col->shape == Collider2D::Shape::Circle)
        {
            size = std::max(0.001f, (col->radius * 2.f));
        }
        else
        {
            const Vec2 s = (Vec2)col->size;
            size = std::max(0.001f, std::min(s.x, s.y));
        }

        const Vec2 v = rb->GetVelocity();
        const float speed = std::sqrt(v.x * v.x + v.y * v.y);
        const float dist = speed * dt;

        const float denom = std::max(0.001f, size * ccdMinSizeFactor);
        const int need = (int)std::ceil(dist / denom);

        steps = std::max(steps, need);
    }

    steps = std::clamp(steps, 1, maxSubsteps);
    return steps;
}

void PhysicsManager::Integrate_(float dt) noexcept
{
    for (auto& [rb, active] : mBodies)
    {
        if (!rb || !active) continue;

        GameObject* go = rb->gameObject;
        if (!go) { rb->ClearForces(); continue; }

        auto* tr = go->GetComponent<Transform>();
        if (!tr) { rb->ClearForces(); continue; }

        const auto type = rb->GetBodyType();
        const auto c = rb->GetConstraints();

        // Static: no se mueve ni integra
        if (type == RigidBody2D::BodyType::Static)
        {
            rb->SetVelocity({ 0,0 });
            rb->SetAngularVelocity(0.f);
            rb->ClearForces();
            continue;
        }

        Vec2 v = rb->GetVelocity();
        float w = rb->GetAngularVelocity();

        // Kinematic: ignora fuerzas, solo usa v/w
        if (type == RigidBody2D::BodyType::Kinematic)
        {
            // constraints
            if ((c & RigidBody2D::Constraints::FreezePosX) != RigidBody2D::Constraints::None) v.x = 0.f;
            if ((c & RigidBody2D::Constraints::FreezePosY) != RigidBody2D::Constraints::None) v.y = 0.f;
            if ((c & RigidBody2D::Constraints::FreezeRot) != RigidBody2D::Constraints::None) w = 0.f;

            Vec3 pos = tr->position;
            pos.x += v.x * dt;
            pos.y += v.y * dt;
            tr->position = pos;

            Vec3 rot = tr->rotation;
            rot.z += (w * dt) * (180.0f / 3.1415926535f);
            tr->rotation = rot;

            rb->SetVelocity(v);
            rb->SetAngularVelocity(w);
            rb->ClearForces();
            continue;
        }

        // Dynamic:
        const float invM = rb->InvMass_();
        const float invI = rb->InvInertia_();

        // acel = (F/m) + accel + gravedad
        Vec2 a = rb->accumForce_ * invM;
        a += rb->accumAccel_;
        a += gravity * rb->GetGravityScale();

        // constraints afectan aceleración/velocidad
        if ((c & RigidBody2D::Constraints::FreezePosX) != RigidBody2D::Constraints::None) { a.x = 0.f; v.x = 0.f; }
        if ((c & RigidBody2D::Constraints::FreezePosY) != RigidBody2D::Constraints::None) { a.y = 0.f; v.y = 0.f; }

        // angular accel
        float angA = rb->accumTorque_ * invI + rb->accumAngularAccel_;
        if ((c & RigidBody2D::Constraints::FreezeRot) != RigidBody2D::Constraints::None) { angA = 0.f; w = 0.f; }

        v += a * dt;
        w += angA * dt;

        // damping
        const float ld = rb->GetLinearDamping();
        if (ld > 0.f) {
            const float k = std::max(0.f, 1.f - ld * dt);
            v *= k;
        }
        const float ad = rb->GetAngularDamping();
        if (ad > 0.f) {
            const float k = std::max(0.f, 1.f - ad * dt);
            w *= k;
        }

        // integrar transform
        Vec3 pos = tr->position;
        pos.x += v.x * dt;
        pos.y += v.y * dt;
        tr->position = pos;

        Vec3 rot = tr->rotation;
        rot.z += (w * dt) * (180.0f / 3.1415926535f);
        tr->rotation = rot;

        rb->SetVelocity(v);
        rb->SetAngularVelocity(w);

        rb->ClearForces();
    }
}

Vec2 PhysicsManager::ColliderCenterWorld_(const Collider2D* c) noexcept
{
    if (!c) return { 0,0 };
    if (c->GetShape() == Collider2D::Shape::Circle) return c->WorldCircle().center;
    return c->WorldOBB().center;
}

void PhysicsManager::SolveContacts_(const std::vector<NarrowContact>& contacts, float dt) noexcept
{
    constexpr float kRestitutionThreshold = 0.5f;
    constexpr float beta = 0.10f; // bias suave (0.05..0.15)

    for (const auto& c : contacts)
    {
        if (!c.a || !c.b) continue;
        if (c.isTriggerPair) continue;

        auto* rbA = c.a->GetComponent<RigidBody2D>();
        auto* rbB = c.b->GetComponent<RigidBody2D>();
        if (!rbA && !rbB) continue;

        auto* trA = c.a->GetComponent<Transform>();
        auto* trB = c.b->GetComponent<Transform>();
        if (!trA || !trB) continue;

        Vec2 n = c.contact.normalA;      // A->B
        const float pen = c.contact.penetration;
        const Vec2 p = c.contact.point;

        const Vec2 centerA = (c.colA ? ColliderCenterWorld_(c.colA) : Vec2{ trA->position->x, trA->position->y });
        const Vec2 centerB = (c.colB ? ColliderCenterWorld_(c.colB) : Vec2{ trB->position->x, trB->position->y });

        if (math::Dot(centerB - centerA, n) < 0.f) n = -n;

        const float invMA = (rbA && rbA->GetBodyType() == RigidBody2D::BodyType::Dynamic) ? rbA->InvMass_() : 0.f;
        const float invMB = (rbB && rbB->GetBodyType() == RigidBody2D::BodyType::Dynamic) ? rbB->InvMass_() : 0.f;
        float invIA = (rbA && rbA->GetBodyType() == RigidBody2D::BodyType::Dynamic) ? rbA->InvInertia_() : 0.f;
        float invIB = (rbB && rbB->GetBodyType() == RigidBody2D::BodyType::Dynamic) ? rbB->InvInertia_() : 0.f;

        auto consA = rbA ? rbA->GetConstraints() : RigidBody2D::Constraints::None;
        auto consB = rbB ? rbB->GetConstraints() : RigidBody2D::Constraints::None;

        if ((consA & RigidBody2D::Constraints::FreezeRot) != RigidBody2D::Constraints::None) invIA = 0.f;
        if ((consB & RigidBody2D::Constraints::FreezeRot) != RigidBody2D::Constraints::None) invIB = 0.f;

        const float invMassSum = invMA + invMB;
        if (invMassSum <= 0.f) continue;

        // rA/rB desde COM a contacto
        const Vec2 rA = p - centerA;
        const Vec2 rB = p - centerB;

        Vec2 vA = rbA ? rbA->GetVelocity() : Vec2{ 0,0 };
        Vec2 vB = rbB ? rbB->GetVelocity() : Vec2{ 0,0 };
        float wA = rbA ? rbA->GetAngularVelocity() : 0.f;
        float wB = rbB ? rbB->GetAngularVelocity() : 0.f;

        const Vec2 velAtA = vA + Perp(wA, rA);
        const Vec2 velAtB = vB + Perp(wB, rB);
        const Vec2 rv = velAtB - velAtA;

        const float velAlongN = math::Dot(rv, n);

        // restitución
        float e = 0.f;
        if (rbA) e = std::max(e, rbA->GetRestitution());
        if (rbB) e = std::max(e, rbB->GetRestitution());
        if (-velAlongN < kRestitutionThreshold) e = 0.f;

        const float raCn = Cross(rA, n);
        const float rbCn = Cross(rB, n);

        const float denom = invMassSum + (raCn * raCn) * invIA + (rbCn * rbCn) * invIB;
        if (denom <= 1e-8f) continue;

        // bias (Baumgarte) solo en velocidades
        const float slop = penetrationSlop;
        const float bias = -(beta / dt) * std::max(pen - slop, 0.f);

        float j = -((1.f + e) * velAlongN + bias) / denom;
        if (j < 0.f) j = 0.f;

        const Vec2 impulse = n * j;

        if (rbA && rbA->GetBodyType() == RigidBody2D::BodyType::Dynamic)
        {
            auto consA = rbA->GetConstraints();

            Vec2 dv = impulse * invMA;
            if ((consA & RigidBody2D::Constraints::FreezePosX) != RigidBody2D::Constraints::None) dv.x = 0.f;
            if ((consA & RigidBody2D::Constraints::FreezePosY) != RigidBody2D::Constraints::None) dv.y = 0.f;

            vA -= dv;

            if (invIA > 0.f) wA -= raCn * j * invIA;
            else wA = 0.f;

            rbA->SetVelocity(vA);
            rbA->SetAngularVelocity(wA);
        }

        if (rbB && rbB->GetBodyType() == RigidBody2D::BodyType::Dynamic)
        {
            auto consB = rbB->GetConstraints();

            Vec2 dv = impulse * invMB;
            if ((consB & RigidBody2D::Constraints::FreezePosX) != RigidBody2D::Constraints::None) dv.x = 0.f;
            if ((consB & RigidBody2D::Constraints::FreezePosY) != RigidBody2D::Constraints::None) dv.y = 0.f;

            vB += dv;

            if (invIB > 0.f) wB += rbCn * j * invIB;
            else wB = 0.f;

            rbB->SetVelocity(vB);
            rbB->SetAngularVelocity(wB);
        }
    }
}

void PhysicsManager::PositionalCorrection_(const std::vector<NarrowContact>& contacts) noexcept
{
    // valores estables (no rebotan)
    const float slop = penetrationSlop;      // normalmente 0
    const float percent = penetrationPercent; // 0.15..0.30

    for (const auto& c : contacts)
    {
        if (!c.a || !c.b) continue;
        if (c.isTriggerPair) continue;

        auto* rbA = c.a->GetComponent<RigidBody2D>();
        auto* rbB = c.b->GetComponent<RigidBody2D>();
        if (!rbA && !rbB) continue;

        auto* trA = c.a->GetComponent<Transform>();
        auto* trB = c.b->GetComponent<Transform>();
        if (!trA || !trB) continue;

        Vec2 n = c.contact.normalA;
        const float pen = c.contact.penetration;

        Vec3 pa3 = trA->position;
        Vec3 pb3 = trB->position;
        
        const Vec2 centerA = (c.colA ? ColliderCenterWorld_(c.colA) : Vec2{ trA->position->x, trA->position->y });
        const Vec2 centerB = (c.colB ? ColliderCenterWorld_(c.colB) : Vec2{ trB->position->x, trB->position->y });

        if (math::Dot(centerB - centerA, n) < 0.f) n = -n;

        const float invMA = (rbA && rbA->GetBodyType() == RigidBody2D::BodyType::Dynamic) ? rbA->InvMass_() : 0.f;
        const float invMB = (rbB && rbB->GetBodyType() == RigidBody2D::BodyType::Dynamic) ? rbB->InvMass_() : 0.f;
        const float invMassSum = invMA + invMB;
        if (invMassSum <= 0.f) continue;

        const float mag = std::max(pen - slop, 0.f) / invMassSum * percent;
        if (mag <= 0.f) continue;

        const Vec2 corr = n * mag;

        if (rbA && rbA->GetBodyType() == RigidBody2D::BodyType::Dynamic)
        {
            auto consA = rbA->GetConstraints();
            Vec2 delta = corr * invMA;
            if ((consA & RigidBody2D::Constraints::FreezePosX) != RigidBody2D::Constraints::None) delta.x = 0.f;
            if ((consA & RigidBody2D::Constraints::FreezePosY) != RigidBody2D::Constraints::None) delta.y = 0.f;

            pa3.x -= delta.x;
            pa3.y -= delta.y;
            trA->position = pa3;
        }

        if (rbB && rbB->GetBodyType() == RigidBody2D::BodyType::Dynamic)
        {
            auto consB = rbB->GetConstraints();
            Vec2 delta = corr * invMB;
            if ((consB & RigidBody2D::Constraints::FreezePosX) != RigidBody2D::Constraints::None) delta.x = 0.f;
            if ((consB & RigidBody2D::Constraints::FreezePosY) != RigidBody2D::Constraints::None) delta.y = 0.f;

            pb3.x += delta.x;
            pb3.y += delta.y;
            trB->position = pb3;
        }
    }
}


void PhysicsManager::Step(float fixedDt) noexcept
{
    auto* collision = CollisionManager::GetInstancePtr();
    if (!collision) return;

    // reset stats
    stepTimeSec = 0.0;
    integrateTimeSec = 0.0;
    buildContactsTimeSec = 0.0;
    solveTimeSec = 0.0;

    nContactsProcessedThisFrame = 0;
    nSubstepsThisFrame = 0;
    solverIterations = 0;

    const double tStep0 = NowSec_();

    const int substeps = ComputeSubsteps_(fixedDt);
    const float dt = fixedDt / (float)substeps;

    nSubstepsThisFrame = substeps;

    std::vector<NarrowContact> contacts;
    contacts.reserve(128);

    constexpr int kSolverIters = 8;
    solverIterations = kSolverIters;

    for (int i = 0; i < substeps; ++i)
    {
        {
            const double t0 = NowSec_();
            Integrate_(dt);
            const double t1 = NowSec_();
            integrateTimeSec += (t1 - t0);
        }

        contacts.clear();
        {
            const double t0 = NowSec_();
            collision->BuildContacts_(contacts);
            const double t1 = NowSec_();
            buildContactsTimeSec += (t1 - t0);
        }

        // contactos "procesados" este substep (sin multiplicar por iteraciones)
        nContactsProcessedThisFrame += (std::uint64_t)contacts.size();

        {
            const double t0 = NowSec_();
            for (int it = 0; it < kSolverIters; ++it)
                SolveContacts_(contacts, dt);

            PositionalCorrection_(contacts);
            const double t1 = NowSec_();
            solveTimeSec += (t1 - t0);
        }
    }

    const double tStep1 = NowSec_();
    stepTimeSec = (tStep1 - tStep0);
}

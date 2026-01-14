#include "Rigidbody2D.h"
#include "PhysicsManager.h"
#include "GameObject.h"

void RigidBody2D::Awake()
{
    PhysicsManager::GetInstancePtr()->RegisterBody(this);
}

void RigidBody2D::OnEnable()
{
    PhysicsManager::GetInstancePtr()->SetBodyActive(this, true);
}

void RigidBody2D::OnDisable()
{
    PhysicsManager::GetInstancePtr()->SetBodyActive(this, false);
}

void RigidBody2D::OnDestroy()
{
    PhysicsManager::GetInstancePtr()->RemoveBody(this);
}

void RigidBody2D::Register()
{
    PhysicsManager::GetInstancePtr()->RegisterBody(this);
    PhysicsManager::GetInstancePtr()->SetBodyActive(this, mEnabled && gameObject->activeInHierarchy);
}

//std::unique_ptr<Component> RigidBody2D::Clone(GameObject* newOwner) const
//{
//    auto up = std::make_unique<RigidBody2D>();
//    up->mGameObject = newOwner;
//
//    up->bodyType_ = bodyType_;
//    up->collisionDetection_ = collisionDetection_;
//    up->mass_ = mass_;
//    up->inertia_ = inertia_;
//    up->gravityScale_ = gravityScale_;
//    up->linearDamping_ = linearDamping_;
//    up->angularDamping_ = angularDamping_;
//    up->restitution_ = restitution_;
//    up->constraints_ = constraints_;
//    up->velocity_ = velocity_;
//    up->angularVelocity_ = angularVelocity_;
//    up->accumForce_ = accumForce_;
//    up->accumAccel_ = accumAccel_;
//    up->accumTorque_ = accumTorque_;
//    up->accumAngularAccel_ = accumAngularAccel_;
//
//    return up;
//}
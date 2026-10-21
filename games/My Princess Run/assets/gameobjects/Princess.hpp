#pragma once
#include <XEngine.h>
#include "PrincessController.hpp"
#include "PrincessAnimator.hpp"

namespace MyPrincessRun
{
inline InstanceBuilder PrincessPrefab()
{
    return [](GameObject& princess, Scene& scene){
    auto* princessTex = AssetManager::GetInstance().GetTextureByKey("princess-run");

    princess.AddComponent<SpriteRenderer>()->sprite = princessTex;
	princess.GetComponent<SpriteRenderer>()->pivot01 = Vec2(0.5f, 1.f);

    float ppu = princessTex->PixelsPerUnit();
    float wUnits = (princessTex->Width()/8) / ppu;
    float hUnits = princessTex->Height() / ppu;

    princess.transform->scale = Vec2(1.0f / hUnits, 1.0f / hUnits) * 2.5f;

    princess.AddComponent<Collider2D>();
    princess.GetComponent<Collider2D>()->shape = Collider2D::Shape::Box;
    princess.GetComponent<Collider2D>()->size = Vec2(.3f * wUnits, 0.5f * hUnits);
    princess.GetComponent<Collider2D>()->localOffset = Vec2(0.f, -.14f);

	princess.AddComponent<RigidBody2D>()->constraints = RigidBody2D::Constraints::FreezePosX | RigidBody2D::Constraints::FreezeRot;
	princess.GetComponent<RigidBody2D>()->mass = 1.0f;
	princess.GetComponent<RigidBody2D>()->gravityScale = 4.0f;
	princess.GetComponent<RigidBody2D>()->restitution = 0.0f;
	princess.GetComponent<RigidBody2D>()->collisionDetection = RigidBody2D::CollisionDetection::Continuous;
	princess.AddComponent<PrincessController>();
	princess.AddComponent<PrincessAnimator>();
};
}
}

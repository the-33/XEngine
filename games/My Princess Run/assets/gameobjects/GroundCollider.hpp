#pragma once
#include <XEngine.h>

namespace MyPrincessRun
{
    inline GameObject* CreateGroundCollider(Scene& scene, float width)
    {
        auto* ground = scene.CreateObject("ground");
        ground->transform->position = Vec3(0.f, 3.f, 0.f);
        ground->transform->scale = Vec2(width, 1.f);
        ground->AddComponent<Collider2D>()->localOffset = Vec2(0.f, .5f);
        auto* rb = ground->AddComponent<RigidBody2D>();
        rb->bodyType = RigidBody2D::BodyType::Static;
        return ground;
    }
}

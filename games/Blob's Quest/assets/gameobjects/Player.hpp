#pragma once
#include <BaseTypes.h>
class Scene;
class GameObject;
GameObject* CreatePlayer(Scene& scene, Vec3 position);

#include "SpriteFactory.hpp"
#include "PlayerController.hpp"
#include "CameraFollow.hpp"
#include <XEngine.h>
inline GameObject* CreatePlayer(Scene& scene, Vec3 position)
{
    auto* texture = Assets->GetTextureByKey("player");
    auto* player = CreateSpriteObject(scene, "Player", texture, position,
        Vec2{1.f, 1.f}, false, 1u << 0, 0xFFFFFFFFu);
    if (!player) return nullptr;
    player->AddComponent<PlayerController>();
    player->AddComponent<CameraFollow>();
    player->AddComponent<RigidBody2D>();
    auto* collider = player->GetComponent<Collider2D>();
    collider->shape = Collider2D::Shape::Circle;
    collider->radius = collider->size->x / 2.5f;
    return player;
}

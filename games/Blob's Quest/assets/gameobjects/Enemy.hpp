#pragma once
#include <BaseTypes.h>
class Scene;
class GameObject;
GameObject* SpawnEnemy(Scene& scene, Vec3 position);

#include "SpriteFactory.hpp"
#include "EnemyFollow.hpp"
#include "EnemyDamage.hpp"
#include <XEngine.h>
inline GameObject* SpawnEnemy(Scene& scene, Vec3 position)
{
    auto* enemy = CreateSpriteObject(scene, "Enemy", Assets->GetTextureByKey("enemy"),
                                     position, Vec2{1.f, 1.f}, true, 1u << 3, 0xFFFFFFFFu);
    if (!enemy) return nullptr;
    enemy->AddComponent<EnemyFollow>();
    enemy->AddComponent<EnemyDamage>();
    auto* collider = enemy->GetComponent<Collider2D>();
    collider->shape = Collider2D::Shape::Circle;
    collider->radius = collider->size->x / 2.5f;
    enemy->tag = "enemy";
    return enemy;
}

#pragma once
#include <BaseTypes.h>
class Scene;
class GameObject;
void BuildCoin(GameObject& coin, Scene& scene);
GameObject* SpawnCoin(Scene& scene, Vec3 position);

#include "CoinPickup.hpp"
#include <XEngine.h>
inline void BuildCoin(GameObject& coin, Scene&)
{
    auto* tex = Assets->GetTextureByKey("coin");
    if (!tex) return;
    coin.AddComponent<SpriteRenderer>()->sprite = tex;
    const float w = tex->Width() / tex->PixelsPerUnit();
    const float h = tex->Height() / tex->PixelsPerUnit();
    coin.transform->scale = Vec2{0.5f / w, 0.5f / h};
    auto* collider = coin.AddComponent<Collider2D>();
    collider->isTrigger = true;
    collider->shape = Collider2D::Shape::Circle;
    collider->radius = (0.7f * w) / 2.5f;
    collider->mask = 1u << 0;
    collider->layer = 0xFFFFFFFFu;
    coin.AddComponent<CoinPickup>()->respawnBuilder = BuildCoin;
}
inline GameObject* SpawnCoin(Scene& scene, Vec3 position)
{
    return scene.Instantiate("coin", BuildCoin, position);
}

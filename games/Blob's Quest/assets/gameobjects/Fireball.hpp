#pragma once
class GameObject;
class Scene;
void BuildFireball(GameObject& fireball, Scene& scene);

#include "FireballController.hpp"
#include <XEngine.h>
inline void BuildFireball(GameObject& fireball, Scene&)
{
    auto* tex = Assets->GetTextureByKey("fireball");
    if (!tex) return;
    fireball.AddComponent<SpriteRenderer>()->sprite = tex;
    const float w = tex->Width() / tex->PixelsPerUnit();
    const float h = tex->Height() / tex->PixelsPerUnit();
    fireball.transform->scale = Vec2{0.5f / w, 0.5f / h};
    auto* collider = fireball.AddComponent<Collider2D>();
    collider->isTrigger = true;
    collider->radius = (0.7f * w) / 2.5f;
    collider->shape = Collider2D::Shape::Circle;
    fireball.AddComponent<FireballController>();
}

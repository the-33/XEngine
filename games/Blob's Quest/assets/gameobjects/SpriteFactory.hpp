#pragma once
#include <XEngine.h>
#include <string>
// Creates a GameObject with a sprite and an optional collider.
// Los objetos concretos se definen en Player/Enemy/Coin/Wall/Fireball.
GameObject* CreateSpriteObject(Scene& scene, const std::string& name,
    const Texture* texture, Vec3 position, Vec2 sizeWorld, bool isTrigger,
    LayerBits layer, LayerBits mask, bool addCollider = true);

inline GameObject* CreateSpriteObject(Scene& scene, const std::string& name,
    const Texture* texture, Vec3 position, Vec2 sizeWorld, bool isTrigger,
    LayerBits layer, LayerBits mask, bool addCollider)
{
    if (!texture) return nullptr;
    auto* obj = scene.CreateObject(name);
    Transform* tr = obj->transform;
    tr->position = position;
    auto* sr = obj->AddComponent<SpriteRenderer>();
    sr->sprite = texture;
    sr->source = RectI{0, 0, texture->Width(), texture->Height()};
    const float ppu = texture->PixelsPerUnit();
    const float w = texture->Width() / ppu;
    const float h = texture->Height() / ppu;
    tr->localScale = Vec2{sizeWorld.x / w, sizeWorld.y / h};
    if (addCollider) {
        auto* collider = obj->AddComponent<Collider2D>();
        collider->shape = Collider2D::Shape::Box;
        collider->size = Vec2{sizeWorld.x * w, sizeWorld.y * h};
        collider->isTrigger = isTrigger;
        collider->layer = layer;
        collider->mask = mask;
        collider->showGizmo = false;
        collider->gizmoColor = isTrigger ? Color::Yellow(255) : Color::Green(255);
    }
    return obj;
}

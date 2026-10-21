#pragma once
#include <BaseTypes.h>
#include <string>
class Scene;
class GameObject;
GameObject* CreateWallTile(Scene& scene, const std::string& name, Vec3 position);
GameObject* CreateWallBoundary(Scene& scene, const std::string& name,
                               Vec3 position, Vec2 worldSize);

#include "SpriteFactory.hpp"
#include <XEngine.h>
inline GameObject* CreateWallTile(Scene& scene, const std::string& name, Vec3 position)
{
    return CreateSpriteObject(scene, name, Assets->GetTextureByKey("wall"),
                              position, Vec2{1.f, 1.f}, false, 1u << 1, 0xFFFFFFFFu, false);
}
inline GameObject* CreateWallBoundary(Scene& scene, const std::string& name,
                               Vec3 position, Vec2 worldSize)
{
    auto* tex = Assets->GetTextureByKey("wall");
    if (!tex) return nullptr;
    auto* object = scene.CreateObject(name);
    object->transform->position = position;
    const float w = tex->Width() / tex->PixelsPerUnit();
    const float h = tex->Height() / tex->PixelsPerUnit();
    object->transform->scale = Vec2{worldSize.x / w, worldSize.y / h};
    object->AddComponent<Collider2D>()->size = Vec2{w, h};
    object->tag = "wall";
    return object;
}

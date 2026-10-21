#pragma once
#include <XEngine.h>
#include "CactusController.hpp"

namespace MyPrincessRun
{
inline InstanceBuilder Cactus3Prefab()
{
    return [](GameObject& cactus, Scene& scene) {
    auto* tex = AssetManager::GetInstance().GetTextureByKey("cactus-3");
    cactus.AddComponent<SpriteRenderer>()->sprite = tex;
	cactus.GetComponent<SpriteRenderer>()->pivot01 = Vec2(0.5f, 1.f);
    float ppu = tex->PixelsPerUnit();
    float wUnits = tex->Width() / ppu;
    float hUnits = tex->Height() / ppu;
    float randomScale = Random->Range(1.f, 1.5f);

    cactus.transform->scale = { randomScale / hUnits, randomScale / hUnits };
    cactus.AddComponent<Collider2D>()->shape = Collider2D::Shape::Box;
    cactus.GetComponent<Collider2D>()->size = Vec2(0.9f * wUnits, 0.9f * hUnits);
    cactus.GetComponent<Collider2D>()->localOffset = Vec2(0.f, -.09f);
    cactus.GetComponent<Collider2D>()->isTrigger = true;
    cactus.AddComponent<CactusController>();
	};
}
}

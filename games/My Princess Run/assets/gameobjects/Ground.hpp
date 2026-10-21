#pragma once
#include <XEngine.h>

namespace MyPrincessRun
{
inline InstanceBuilder GroundPrefab()
{
    return [](GameObject& ground, Scene& scene) {
    auto* groundTex = AssetManager::GetInstance().GetTextureByKey("ground");
    ground.AddComponent<SpriteRenderer>()->sprite = groundTex;
    float ppu = groundTex->PixelsPerUnit();
    float wUnits = groundTex->Width() / ppu;
    float hUnits = groundTex->Height() / ppu;
    ground.transform->scale = { 3.f / hUnits, 3.f / hUnits };
    };
}
}

#pragma once
#include <XEngine.h>

namespace MyPrincessRun
{
inline InstanceBuilder SkyPrefab()
{
    return [](GameObject& sky, Scene& scene) {
    auto* skyTex = AssetManager::GetInstance().GetTextureByKey("sky");
    sky.AddComponent<SpriteRenderer>()->sprite = skyTex;
    float ppu = skyTex->PixelsPerUnit();
    float wUnits = skyTex->Width() / ppu;
    float hUnits = skyTex->Height() / ppu;
    sky.transform->scale = { 8.f / hUnits, 8.f / hUnits };
    };
}
}

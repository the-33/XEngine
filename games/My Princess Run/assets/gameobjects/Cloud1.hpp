#pragma once
#include <XEngine.h>

namespace MyPrincessRun
{
inline InstanceBuilder Cloud1Prefab()
{
    return [](GameObject& cloud, Scene& scene) {
    auto* tex = AssetManager::GetInstance().GetTextureByKey("cloud-big");
    cloud.AddComponent<SpriteRenderer>()->sprite = tex;

    float ppu = tex->PixelsPerUnit();
    float hUnits = tex->Height() / ppu;

    constexpr float targetH = 1.2f;
    cloud.transform->scale = { targetH / hUnits, targetH / hUnits };
    };
}
}

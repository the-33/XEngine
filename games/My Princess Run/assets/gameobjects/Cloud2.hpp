#pragma once
#include <XEngine.h>

namespace MyPrincessRun
{
inline InstanceBuilder Cloud2Prefab()
{
    return [](GameObject& cloud, Scene& scene) {
    auto* tex = AssetManager::GetInstance().GetTextureByKey("cloud-small");
    cloud.AddComponent<SpriteRenderer>()->sprite = tex;

    float ppu = tex->PixelsPerUnit();
    float hUnits = tex->Height() / ppu;

    constexpr float targetH = 0.9f;
    cloud.transform->scale = { targetH / hUnits, targetH / hUnits };
    };
}
}

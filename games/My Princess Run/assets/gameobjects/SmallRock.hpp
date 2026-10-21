#pragma once
#include <XEngine.h>

namespace MyPrincessRun
{
inline InstanceBuilder SmallRockPrefab()
{
    return [](GameObject& rock, Scene& scene) {
    auto* rockTex = AssetManager::GetInstance().GetTextureByKey("rock-small");
    rock.AddComponent<SpriteRenderer>()->sprite = rockTex;
    float ppu = rockTex->PixelsPerUnit();
    float wUnits = rockTex->Width() / ppu;
    float hUnits = rockTex->Height() / ppu;
    rock.transform->scale = { 1.5f / hUnits, 1.5f / hUnits };
    };
}
}

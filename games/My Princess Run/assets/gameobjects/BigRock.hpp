#pragma once
#include <XEngine.h>

namespace MyPrincessRun
{
inline InstanceBuilder BigRockPrefab()
{
    return [](GameObject& rock, Scene& scene) {
    auto* rockTex = AssetManager::GetInstance().GetTextureByKey("rock-big");
    rock.AddComponent<SpriteRenderer>()->sprite = rockTex;
    float ppu = rockTex->PixelsPerUnit();
    float wUnits = rockTex->Width() / ppu;
    float hUnits = rockTex->Height() / ppu;
    rock.transform->scale = { 2.f / hUnits, 2.f / hUnits };
    };
}
}

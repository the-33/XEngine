#pragma once
#include <XEngine.h>
#include "GameState.hpp"

class CoinPickup final : public Behaviour
{
public:
    // Configured by the coin prefab; no dependency on Coin.hpp here.
    InstanceBuilder respawnBuilder;

    void OnTriggerEnter(const CollisionInfo2D& info) override
    {
        if (!info.other || info.other->name != "Player") return;
        BlobQuest::AddScore();
        if (auto* sfx = Assets->GetSFXByKey("coin")) Sound->PlaySFX(sfx);
        if (respawnBuilder) {
            gameObject->scene->Instantiate("coin", respawnBuilder,
                Vec3{Random->Range(-19.f, 19.f), Random->Range(-19.f, 19.f), 0.f});
        }
        gameObject->Destroy();
    }
};

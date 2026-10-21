#pragma once
#include <XEngine.h>
#include "GameState.hpp"
#include "Princess.hpp"
#include "Ground.hpp"
#include "Sky.hpp"
#include "BigRock.hpp"
#include "SmallRock.hpp"
#include "Cloud1.hpp"
#include "Cloud2.hpp"
#include "Cactus1.hpp"
#include "Cactus2.hpp"
#include "Cactus3.hpp"
#include "GroundCollider.hpp"
#include "ObjectThreadmill.hpp"
#include "CactusSpawner.hpp"
#include "HUDController.hpp"

namespace MyPrincessRun
{
    // The scene only determines WHICH objects appear and WHERE.
    // InitCallback.hpp handles asset loading, scene registration and activation.
    inline void BuildLevel1(Scene* scene)
    {
        if (!scene || !scene->camera) return;
        scene->camera->center = Vec2(0.f, 0.f);
        const auto screenWidth = Window->GetDrawableSize().x;
        gScreenBorderLeftWorld = scene->camera->ScreenToWorld(Vec2(0.f, 0.f)).x;
        gScreenBorderRightWorld = scene->camera->ScreenToWorld(Vec2(static_cast<float>(screenWidth), 0.f)).x;

        scene->Instantiate("princess", PrincessPrefab(), Vec3(-6.f, 0.f, 2.f));

        auto addTreadmill = [scene](const char* name, Vec3 position,
                                   InstanceBuilder builder, float speed)
        {
            auto* go = scene->CreateObject(name);
            go->transform->position = position;
            auto* tm = go->AddComponent<ObjectThreadmill>();
            tm->prefab = std::move(builder);
            tm->speed = speed;
        };

        addTreadmill("ground_threadmill", Vec3(0.f, 3.f, 0.f), GroundPrefab(), 10.f);
        CreateGroundCollider(*scene, math::Abs(gScreenBorderRightWorld - gScreenBorderLeftWorld));
        addTreadmill("sky_threadmill", Vec3(0.f, -5.7f, -1.f), SkyPrefab(), 1.f);
        addTreadmill("rock_threadmill_big", Vec3(0.f, 1.f, -0.5f), BigRockPrefab(), 6.f);
        addTreadmill("rock_threadmill_small", Vec3(0.f, 1.5f, -0.5f), SmallRockPrefab(), 9.f);
        addTreadmill("cloud_threadmill_big", Vec3(0.f, -1.0f, -1.0f), Cloud1Prefab(), 2.f);
        addTreadmill("cloud_threadmill_small", Vec3(0.f, -0.3f, -0.8f), Cloud2Prefab(), 3.f);

        auto* spawnerObject = scene->CreateObject("cactus_spawner");
        auto* spawner = spawnerObject->AddComponent<CactusSpawner>();
        spawner->cactusPrefab1 = Cactus1Prefab();
        spawner->cactusPrefab2 = Cactus2Prefab();
        spawner->cactusPrefab3 = Cactus3Prefab();

        scene->CreateObject("HUD")->AddComponent<HUDController>();
    }
}

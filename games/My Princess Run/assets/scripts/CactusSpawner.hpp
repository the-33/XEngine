#pragma once
#include <XEngine.h>
#include "GameState.hpp"

namespace MyPrincessRun
{
class CactusSpawner : public Behaviour
{
public:
    InstanceBuilder cactusPrefab1;
	InstanceBuilder cactusPrefab2;
	InstanceBuilder cactusPrefab3;

private:
    float spawnTimer = 0.f;
    float minSpawnInterval = .5f;
    float maxSpawnInterval = 2.f;

	float maxSpeedMultiplier = 3.f;
	float speedIncreasePerSecond = 0.01f;

    void Update(float dt) override
    {
        spawnTimer -= dt;
        if (spawnTimer <= 0.f && gSpeedMultiplier > 0.f)
        {
            spawnTimer = Random->Range(minSpawnInterval, maxSpawnInterval);
            int choice = Random->Range(0, 3);
            InstanceBuilder* prefab = nullptr;
            if (choice == 0) prefab = &cactusPrefab1;
            else if (choice == 1) prefab = &cactusPrefab2;
            else prefab = &cactusPrefab3;
            gameObject->scene->Instantiate("cactus", *prefab, Vec3(gScreenBorderRightWorld + 2.f, 3.f, 0.f))->tag = "cactus";
        }

        if (gSpeedMultiplier > 0.f)
		{
			gSpeedMultiplier += speedIncreasePerSecond * dt;
			gSpeedMultiplier = std::min(gSpeedMultiplier, maxSpeedMultiplier);

			gScore += 10.f * gSpeedMultiplier * dt;
		}
	}
};
}

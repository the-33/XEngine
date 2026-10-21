#pragma once
#include <XEngine.h>
#include "GameState.hpp"

namespace MyPrincessRun
{
class CactusController : public Behaviour
{
    float speed = 10.f;

    void Update(float dt) override
    {
        gameObject->transform->position -= Vec3::Right() * speed * gSpeedMultiplier * dt;
        if (gameObject->transform->position->x < gScreenBorderLeftWorld - 5.f)
        {
			gameObject->Destroy();
        }
	}
};
}

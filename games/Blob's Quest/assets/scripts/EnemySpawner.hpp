#pragma once
#include <Behaviour.h>
class EnemySpawner final : public Behaviour
{
public:
    float frequency = 5.f;
    float timer = 0.f;
    void Update(float dt) override;
};

#include "Enemy.hpp"
#include <XEngine.h>
inline void EnemySpawner::Update(float dt)
{
    timer += dt;
    if (timer < frequency) return;
    timer = 0.f;
    SpawnEnemy(*gameObject->scene.get(),
        Vec3{Random->Range(-19.f, 19.f), Random->Range(-19.f, 19.f), 0.f});
}

#pragma once
#include <Behaviour.h>
class FireballController final : public Behaviour
{
public:
    float speed = 10.f;
    void Update(float dt) override;
    void OnTriggerEnter(const CollisionInfo2D& info) override;
};

#include "EnemyDamage.hpp"
#include <XEngine.h>
inline void FireballController::Update(float dt)
{
    transform->position += transform->right * speed * dt;
}
inline void FireballController::OnTriggerEnter(const CollisionInfo2D& info)
{
    if (!info.other) return;
    if (info.other->tag == "wall") {
        gameObject->Destroy();
    } else if (info.other->tag == "enemy") {
        if (auto* hp = info.other->GetComponent<EnemyDamage>()) hp->Damage();
        gameObject->Destroy();
    }
}

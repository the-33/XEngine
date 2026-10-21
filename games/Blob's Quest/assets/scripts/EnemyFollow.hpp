#pragma once
#include <Behaviour.h>
class GameObject;
class EnemyFollow final : public Behaviour
{
public:
    float speed = 3.f;
    GameObject* target = nullptr;
    void Start() override;
    void Update(float dt) override;
    void OnTriggerEnter(const CollisionInfo2D& info) override;
};

#include "PlayerController.hpp"
#include "GameState.hpp"
#include <XEngine.h>
inline void EnemyFollow::Start() { target = gameObject->scene->Find("Player"); }
inline void EnemyFollow::Update(float dt)
{
    if (!target) return;
    Vec3 destination = target->transform->position;
    gameObject->transform->position = math::MoveTowards(
        (Vec3) gameObject->transform->position, destination, speed * dt);
}
inline void EnemyFollow::OnTriggerEnter(const CollisionInfo2D& info)
{
    if (!info.other || info.other->name != "Player") return;
    BlobQuest::ResetScore();
    if (auto* sfx = Assets->GetSFXByKey("hit")) Sound->PlaySFX(sfx);
    const Vec2 direction = (Vec2) math::Normalized(
        info.other->transform->position - gameObject->transform->position);
    if (auto* rb = info.other->GetComponent<RigidBody2D>())
        rb->AddForce(direction * 20.f, RigidBody2D::ForceMode::Impulse);
    if (auto* player = info.other->GetComponent<PlayerController>()) player->Damage();
}

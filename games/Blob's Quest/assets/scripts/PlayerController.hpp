#pragma once
#include <Behaviour.h>
#include <BaseTypes.h>
class PlayerController final : public Behaviour
{
public:
    float speed = 50.f;
    Vec2 velocity = Vec2::Zero();
    int lives = 3;
    float shootingTimer = 0.f;
    float shootingCooldown = 1.f;
    void Damage();
    void Start() override;
    void Update(float dt) override;
    void FixedUpdate(float dt) override;
};

#include "GameState.hpp"
#include "Fireball.hpp"
#include <XEngine.h>
inline void PlayerController::Damage()
{
    if (--lives <= 0) BlobQuest::RestartLevel();
}
inline void PlayerController::Start()
{
    auto* rb = gameObject->GetComponent<RigidBody2D>();
    if (!rb) return;
    rb->gravityScale = 0.f;
    rb->linearDamping = 10.f;
    rb->constraints = RigidBody2D::Constraints::FreezeRot;
    rb->collisionDetection = RigidBody2D::CollisionDetection::Continuous;
}
inline void PlayerController::Update(float dt)
{
    auto& input = InputManager::GetInstance();
    Vec2 direction{0.f, 0.f};
    if (input.KeyDown(Key::W)) direction.y -= 1.f;
    if (input.KeyDown(Key::S)) direction.y += 1.f;
    if (input.KeyDown(Key::A)) direction.x -= 1.f;
    if (input.KeyDown(Key::D)) direction.x += 1.f;
    shootingTimer -= dt;
    if (input.KeyPressed(Key::SPACE) && shootingTimer <= 0.f) {
        auto* bullet = gameObject->scene->Instantiate(
            "proyectile", BuildFireball, Vec3(transform->position->x, transform->position->y, -1.f));
        if (bullet) bullet->transform->LookAt(Input->GetMousePosWorld());
        shootingTimer = shootingCooldown;
    }
    if (math::LengthSq(direction) > 0.f) direction = math::Normalized(direction);
    velocity = direction * speed;
}
inline void PlayerController::FixedUpdate(float)
{
    if (auto* rb = gameObject->GetComponent<RigidBody2D>())
        rb->AddForce(velocity, RigidBody2D::ForceMode::Force);
}

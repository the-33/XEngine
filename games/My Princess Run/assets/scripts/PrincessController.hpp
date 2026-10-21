#pragma once
#include <XEngine.h>
#include "GameState.hpp"

namespace MyPrincessRun
{
class PrincessController : public Behaviour
{
public:
    bool isOnGround = false;
    bool isDead = false;

private:
    // --- Settings ---
    float jumpImpulse = 15.f;        // initial impulse (VelocityChange)
    float holdBoostPerSec = 20.f;    // extra impulse while the button is held (VelocityChange per second)
    float maxHoldTime = 0.18f;       // maximum hold time
    float jumpCutMultiplier = 0.45f; // on early release: scale down vy (0..1)

    // --- State ---
    bool  isJumping = false;
    float holdTimer = 0.f;

    bool JumpPressed_() const
    {
		if (isDead) return false;
        auto& input = InputManager::GetInstance();
        return input.KeyPressed(Key::W)
            || input.KeyPressed(Key::UP)
            || input.KeyPressed(Key::SPACE);
    }

    bool JumpHeld_() const
    {
		if (isDead) return false;
        auto& input = InputManager::GetInstance();
        return input.KeyDown(Key::W)
            || input.KeyDown(Key::UP)
            || input.KeyDown(Key::SPACE);
    }

    bool JumpReleased_() const
    {
		if (isDead) return true;
        auto& input = InputManager::GetInstance();
        return input.KeyReleased(Key::W)
            || input.KeyReleased(Key::UP)
            || input.KeyReleased(Key::SPACE);
    }

    void Update(float dt) override
    {
        auto* rb = gameObject->GetComponent<RigidBody2D>();
        if (!rb) return;

        // 1) Jump start (rising edge only)
        if (JumpPressed_() && isOnGround)
        {
            rb->AddForce(Vec2(0.f, -jumpImpulse), RigidBody2D::ForceMode::VelocityChange);

            isOnGround = false;
            isJumping = true;
            holdTimer = 0.f;
        }

        // 2) While held: additional impulse (limited)
        if (isJumping && JumpHeld_())
        {
            holdTimer += dt;
            if (holdTimer < maxHoldTime)
            {
                // VelocityChange per second: multiply by dt
                rb->AddForce(Vec2(0.f, -holdBoostPerSec * dt), RigidBody2D::ForceMode::VelocityChange);
            }
        }

        // 3) Released early: jump cut
        if (isJumping && JumpReleased_())
        {
            Vec2 v = rb->velocity;

            // If still ascending (with +Y gravity, upward motion means vy < 0)
            if (v.y < 0.f)
            {
                v.y *= jumpCutMultiplier; // less negative means a lower jump
                rb->velocity = v;
            }

            isJumping = false;
        }

        // 4) If descending, end the held-jump boost
        if (isJumping && rb->velocity->y > 0.f)
            isJumping = false;
    }

    void OnCollisionEnter(const CollisionInfo2D& info) override
    {
        if (info.other && info.other->name == "ground")
        {
            isOnGround = true;
            isJumping = false;
            holdTimer = 0.f;
        }
    }

    void OnCollisionExit(const CollisionInfo2D& info) override
    {
        if (info.other && info.other->name == "ground")
        {
            isOnGround = false;
        }
    }

    void OnTriggerEnter(const CollisionInfo2D& info) override
    {
        if (info.other && info.other->tag == "cactus" && !isDead)
        {
            isDead = true;
			gSpeedMultiplier = 0.f;
        }
	}
};
}

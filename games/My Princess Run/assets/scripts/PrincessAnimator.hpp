#pragma once
#include <XEngine.h>
#include "GameState.hpp"
#include "PrincessController.hpp"

namespace MyPrincessRun
{
class PrincessAnimator : public Behaviour
{
	Texture* runSpritesheet = nullptr;
	Texture* jumpSpritesheet = nullptr;
	Texture* fallSpritesheet = nullptr;
	Texture* dieSpritesheet = nullptr;

    Texture* currentSheet = nullptr;

    int runFrameCount = 8;
    int jumpFrameCount = 3;
    int fallFrameCount = 3;
    int dieFrameCount = 11;

	float runFrameDuration = 0.05f;
	float jumpFrameDuration = 0.2f;
	float fallFrameDuration = 0.2f;
	float dieFrameDuration = 0.05f;

	int currentFrame = 0;
	int frameCount = 0; 
	float frameDuration = 0.6f;
	float frameTimer = 0.f;

    SpriteRenderer* sr;
	RigidBody2D* rb;

    void UpdateSprite()
    {
        sr->sprite = currentSheet;
        int frameWidth = currentSheet->Width() / frameCount;
        sr->source = RectI{ currentFrame * frameWidth, 0, frameWidth, currentSheet->Height() };
    }

    void Start() override
    {
        runSpritesheet = AssetManager::GetInstance().GetTextureByKey("princess-run");
        jumpSpritesheet = AssetManager::GetInstance().GetTextureByKey("princess-jump");
        fallSpritesheet = AssetManager::GetInstance().GetTextureByKey("princess-fall");
		dieSpritesheet = AssetManager::GetInstance().GetTextureByKey("princess-die");

		sr = gameObject->GetComponent<SpriteRenderer>();
		rb = gameObject->GetComponent<RigidBody2D>();

        currentSheet = runSpritesheet;
        frameCount = runFrameCount;
        frameDuration = runFrameDuration;
		frameTimer = frameDuration;

		UpdateSprite();
    }

    void Update(float dt) override
    {
        if (!rb || !sr) return;
        float vy = rb->velocity->y;

		bool isdead = gameObject->GetComponent<PrincessController>()->isDead;

        if (isdead)
        {
            if (currentSheet != dieSpritesheet) currentFrame = 0;
            currentSheet = dieSpritesheet;
            frameCount = dieFrameCount;
            frameDuration = dieFrameDuration;
		}
        else if (gameObject->GetComponent<PrincessController>()->isOnGround)
        {
            if (currentSheet != runSpritesheet) currentFrame = 0;
            currentSheet = runSpritesheet;
            frameCount = runFrameCount;
            frameDuration = runFrameDuration;
        }
        else
        {
            if (vy > 0.1f)
            {
                if (currentSheet != jumpSpritesheet) currentFrame = 0;
                currentSheet = jumpSpritesheet;
                frameCount = jumpFrameCount;
                frameDuration = jumpFrameDuration;
            }
            else if (vy < -0.1f)
            {
                if (currentSheet != fallSpritesheet) currentFrame = 0;
                currentSheet = fallSpritesheet;
                frameCount = fallFrameCount;
                frameDuration = fallFrameDuration;
            }
        }

        // Actualiza el frame
        if (frameTimer >= 0.f) frameTimer -= (dt * ((currentSheet == runSpritesheet && gSpeedMultiplier != 0.f) ? gSpeedMultiplier : 1.f));
        else
        {
            currentFrame++;
            if (currentFrame >= frameCount)
            {
                if (!isdead) currentFrame = 0;
                else currentFrame = frameCount - 1;
            }

			frameTimer = frameDuration;
        }

        UpdateSprite();
	}
};
}

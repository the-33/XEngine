#pragma once
#include <Behaviour.h>
#include <BaseTypes.h>
class CameraFollow final : public Behaviour
{
public:
    Rect worldBounds{-21.5f, -20.5f, 43.f, 41.f};
    void Update(float dt) override;
};

#include <XEngine.h>
inline void CameraFollow::Update(float dt)
{
    Camera2D* camera = gameObject->scene->camera;
    if (!camera) return;
    Vec3 pos = gameObject->transform->position;
    camera->Follow(Vec2{pos.x, pos.y}, 8.f, dt);
    camera->ClampToWorld(worldBounds);
}

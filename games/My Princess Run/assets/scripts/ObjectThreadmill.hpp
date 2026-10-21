#pragma once
#include <XEngine.h>
#include "GameState.hpp"

namespace MyPrincessRun
{
class ObjectThreadmill : public Behaviour
{
public:
	InstanceBuilder prefab;
	float speed = 2.f;
    float objectsWidth = 1.f;
    std::string name = "threadmill_obj";
private:
    float screenBorderLeftWorld;
	float screenBorderRightWorld;

	float firstObjectPosX = 0.f;

	std::vector<GameObject*> objects;

    void Start() override
    {
        auto screenWidth = Window->GetDrawableSize().x;

        this->screenBorderLeftWorld = gScreenBorderLeftWorld;
        this->screenBorderRightWorld = gScreenBorderRightWorld;

        firstObjectPosX = screenBorderRightWorld;
		float x = firstObjectPosX;

        while(x > screenBorderLeftWorld - objectsWidth)
        {
			GameObject* obj = gameObject->scene->Instantiate(name, prefab, Vec3(x, gameObject->transform->position->y, gameObject->transform->position->z));
            auto* sr = obj->GetComponent<SpriteRenderer>();
            if (sr && sr->sprite)
            {
                float ppu = sr->sprite->PixelsPerUnit();
                float wUnits = sr->sprite->Width() / ppu;         // width in unscaled units
                float realWidthWorld = wUnits * obj->transform->scale->x; // ancho real en mundo
                objectsWidth = realWidthWorld;
                sr->pivot01 = Vec2(0.f, 0.f);
            }
            objects.push_back(obj);
            x -= objectsWidth;
        }
    }

    void Update(float dt) override
    {
        const float gap = 0.0f;
        const float step = objectsWidth - gap;

        // 1) Mover todos
        for (auto* obj : objects)
            obj->transform->position -= Vec3::Right() * speed * gSpeedMultiplier * dt;

        // 2) Find the rightmost object (maxX)
        float maxX = -1e30f;
        for (auto* obj : objects)
            maxX = std::max(maxX, obj->transform->position->x);

        // 3) Wrap: move objects that leave the left edge to the right of maxX
        for (auto* obj : objects)
        {
            if (obj->transform->position->x < screenBorderLeftWorld - objectsWidth)
            {
                obj->transform->position = Vec3(maxX + step, obj->transform->position->y, obj->transform->position->z);
                maxX = obj->transform->position->x; // when multiple objects leave during the same frame
            }
        }
    }
};
}

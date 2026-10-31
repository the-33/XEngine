#pragma once
#include <XEngine.h>

class ExampleBehaviour final : public Behaviour
{
public:
    void Update(float dt) override
    {
        (void)dt;
        // Both keyboard and any gamepad assigned to player 0 drive this action.
        if (const auto* move = Input->FindAction("Gameplay/Move")) {
            const Vec2 direction = move->ReadVec2(0);
            (void)direction; // Apply movement in your own gameplay code.
        }
        if (const auto* jump = Input->FindAction("Gameplay/Jump")) {
            if (jump->WasPressedThisFrame(0)) {
                // Jump();
            }
        }
    }
};

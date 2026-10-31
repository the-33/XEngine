// SDL-free core action state test. Compile this file together with
// engine/src/InputActions.cpp using C++20 and the engine/include search path.
#include "InputActions.h"

#include <cassert>

int main()
{
    InputAction jump("Jump", InputActionType::Button);
    int performed = 0, started = 0, canceled = 0;
    const auto token = jump.OnPerformed([&](PlayerId player) {
        assert(player == 0);
        ++performed;
    });
    jump.OnStarted([&](const InputActionEvent& event) {
        assert(event.phase == InputActionPhase::Started);
        ++started;
    });
    jump.OnCanceled([&](const InputActionEvent&) { ++canceled; });
    jump.Feed(0, {1.f, 0.f}, .016f);
    assert(jump.IsPressed() && jump.WasPressedThisFrame());
    assert(jump.WasPerformedThisFrame() && performed == 1 && started == 1);
    jump.Feed(0, {1.f, 0.f}, .016f);
    assert(!jump.WasPressedThisFrame() && performed == 1);
    jump.Feed(0, {}, .016f);
    assert(jump.WasReleasedThisFrame() && canceled == 1);
    assert(jump.RemoveListener(token));
    jump.Feed(0, {1.f, 0.f}, .016f);
    assert(performed == 1);

    InputAction move("Move", InputActionType::Axis2D);
    move.Feed(2, {.5f, -.25f}, .016f);
    assert(move.ReadValue<Vec2>(2).x == .5f);
    assert(move.ReadValue<Vec2>(0).x == 0.f);

    InputAction charge("Charge", InputActionType::Button);
    charge.SetInteraction({InputInteractionType::Hold, 0.15f, 2});
    charge.Feed(0, {1.f, 0.f}, .08f);
    assert(!charge.WasPerformedThisFrame());
    charge.Feed(0, {1.f, 0.f}, .08f);
    assert(charge.WasPerformedThisFrame());
    charge.Feed(0, {1.f, 0.f}, .08f);
    assert(!charge.WasPerformedThisFrame());

    InputAction dash("Dash", InputActionType::Button);
    dash.SetInteraction({InputInteractionType::MultiTap, .25f, 2});
    dash.Feed(0, {1.f, 0.f}, .04f);
    dash.Feed(0, {}, .04f);
    assert(!dash.WasPerformedThisFrame());
    dash.Feed(0, {}, .10f);
    dash.Feed(0, {1.f, 0.f}, .04f);
    dash.Feed(0, {}, .04f);
    assert(dash.WasPerformedThisFrame());
}

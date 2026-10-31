#include "InputActions.h"

#include <algorithm>
#include <cmath>
#include <utility>

const InputAction::State* InputAction::GetState(PlayerId player) const noexcept
{
    const auto it = states_.find(player);
    return it == states_.end() ? nullptr : &it->second;
}

InputValue InputAction::Read(PlayerId player) const noexcept
{
    const State* state = GetState(player);
    return state ? state->value : InputValue{};
}

bool InputAction::IsPressed(PlayerId player) const noexcept
{
    const State* state = GetState(player);
    return state && state->down;
}
bool InputAction::WasPressedThisFrame(PlayerId player) const noexcept
{
    const State* state = GetState(player);
    return state && state->pressed;
}
bool InputAction::WasReleasedThisFrame(PlayerId player) const noexcept
{
    const State* state = GetState(player);
    return state && state->released;
}
bool InputAction::WasPerformedThisFrame(PlayerId player) const noexcept
{
    const State* state = GetState(player);
    return state && state->performed;
}

InputListenerId InputAction::OnPerformed(Callback callback)
{
    const auto id = nextListener_++;
    listeners_.push_back({id, InputActionPhase::Performed, std::move(callback)});
    return id;
}
InputListenerId InputAction::OnStarted(Callback callback)
{
    const auto id = nextListener_++;
    listeners_.push_back({id, InputActionPhase::Started, std::move(callback)});
    return id;
}
InputListenerId InputAction::OnCanceled(Callback callback)
{
    const auto id = nextListener_++;
    listeners_.push_back({id, InputActionPhase::Canceled, std::move(callback)});
    return id;
}
bool InputAction::RemoveListener(InputListenerId id) noexcept
{
    const size_t before = listeners_.size();
    listeners_.erase(std::remove_if(listeners_.begin(), listeners_.end(),
        [id](const Listener& listener) { return listener.id == id; }), listeners_.end());
    return listeners_.size() != before;
}

void InputAction::Emit(PlayerId player, InputActionPhase phase, InputValue value) noexcept
{
    // A callback may unsubscribe itself or another callback.
    try {
        const auto snapshot = listeners_;
        for (const Listener& listener : snapshot)
            if (listener.phase == phase && listener.callback) {
                try { listener.callback({player, phase, value}); } catch (...) {
                    // User exceptions must not escape the noexcept SDL frame loop.
                }
            }
    } catch (...) { /* Allocation failure cannot bring down input processing. */ }
}

void InputAction::Advance(PlayerId player, InputValue value, float dt) noexcept
{
    try {
        State& s = states_[player];
        const bool previous = s.down;
        s.value = value;
        s.down = value.Pressed();
        s.pressed = s.down && !previous;
        s.released = !s.down && previous;
        s.performed = false;
        dt = std::clamp(dt, 0.f, 0.25f);
        if (s.pressed) {
            s.heldSeconds = 0.f;
            s.holdFired = false;
            Emit(player, InputActionPhase::Started, value);
        }
        if (s.down) s.heldSeconds += dt;
        else if (!s.released) s.heldSeconds = 0.f;

        const float duration = std::max(interaction_.duration, 0.f);
        if (s.tapCount && !s.down) {
            s.sinceTap += dt;
            if (s.sinceTap > duration) s.tapCount = 0;
        }
        switch (interaction_.type) {
        case InputInteractionType::Press:
            s.performed = s.pressed;
            break;
        case InputInteractionType::Release:
            s.performed = s.released;
            break;
        case InputInteractionType::Tap:
            s.performed = s.released && s.heldSeconds <= duration;
            break;
        case InputInteractionType::SlowTap:
            s.performed = s.released && s.heldSeconds >= duration;
            break;
        case InputInteractionType::Hold:
            if (s.down && !s.holdFired && s.heldSeconds >= duration) {
                s.performed = true;
                s.holdFired = true;
            }
            break;
        case InputInteractionType::MultiTap:
            if (s.released) {
                if (s.heldSeconds <= duration) {
                    ++s.tapCount;
                    s.sinceTap = 0.f;
                    if (s.tapCount >= std::max(1u, interaction_.taps)) {
                        s.performed = true;
                        s.tapCount = 0;
                    }
                } else s.tapCount = 0;
            }
            break;
        }
        if (s.performed) Emit(player, InputActionPhase::Performed, value);
        if (s.released) {
            Emit(player, InputActionPhase::Canceled, {});
            s.heldSeconds = 0.f;
        }
    } catch (...) { /* Keep the engine's input frame noexcept. */ }
}

void InputAction::Clear(PlayerId player) noexcept
{
    Advance(player, {}, 0.f);
}

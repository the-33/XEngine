#pragma once
/** @file PlayerInput.h
 * @brief Optional component that registers a player's device and action policy.
 */
#include "Behaviour.h"
#include "InputManager.h"
#include <optional>

/** @brief Attach to a controllable character to configure its player slot.
 * A player slot can outlive a character, but only one PlayerInput component
 * should own a given slot at once. Set settings before Start, or call Apply()
 * after changing them at runtime. For menu-only games, ConfigurePlayer() can
 * be used directly without any component.
 * @ingroup xengine_services
 */
class PlayerInput : public Behaviour {
public:
    /** @brief Only one PlayerInput component is allowed per GameObject. */
    static constexpr bool kUnique = true;
    /** @brief The slot's device policy, capability scheme and map selection. */
    PlayerInputSettings settings{};
    /** @brief Construct with default manual configuration. */
    PlayerInput() = default;
    /** @brief Construct with an explicit player profile. */
    explicit PlayerInput(PlayerInputSettings profile) : settings(std::move(profile)) {}

    /** @brief Applies this component's settings to the global input router. */
    void Apply() {
        if (auto* input = InputManager::GetInstancePtr_NO_ERROR_MSG()) {
            if (!registered_) {
                if (const auto* previous = input->GetPlayerSettings(settings.player))
                    previous_ = *previous;
            }
            if (input->ConfigurePlayer(settings))
                registered_ = true;
        }
    }
    /** @brief Non-owning action handle. Null if the action was not loaded. */
    InputAction* Action(const std::string& path) const noexcept {
        auto* input = InputManager::GetInstancePtr();
        return input ? input->FindAction(path) : nullptr;
    }
    /** @brief Reads a movement or aim action for this character's player. */
    Vec2 ReadVec2(const std::string& path) const noexcept {
        const auto* action = Action(path);
        return action ? action->ReadVec2(settings.player) : Vec2{};
    }
    /** @brief Reads the current frame's button press for this character. */
    bool WasPressed(const std::string& path) const noexcept {
        const auto* action = Action(path);
        return action && action->WasPressedThisFrame(settings.player);
    }
    /** @brief Restore the previous player profile or remove an owned slot. */
    ~PlayerInput() override {
        if (registered_) {
            if (auto* input = InputManager::GetInstancePtr_NO_ERROR_MSG()) {
                if (previous_) input->ConfigurePlayer(*previous_);
                else input->RemovePlayer(settings.player);
            }
        }
    }
    /** @brief Register this player's configuration when the scene starts. */
    void Start() override { Apply(); }

private:
    bool registered_ = false;
    std::optional<PlayerInputSettings> previous_;
};

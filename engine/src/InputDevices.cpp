#include "InputManager.h"
#include "SDL.h"

#include <algorithm>
#include <cmath>

void InputManager::NotifyGamepad(GamepadEvent event) noexcept
{
    try {
        mPadEvents.push_back(event);
        const auto callbacks = mPadListeners;
        for (const auto& listener : callbacks) if (listener.callback) {
            try { listener.callback(event); } catch (...) {}
        }
    } catch (...) {}
}

void InputManager::AddGamepad(int index) noexcept
{
    if (!SDL_IsGameController(index)) return;
    const GamepadId id = SDL_JoystickGetDeviceInstanceID(index);
    if (id < 0 || mPads.contains(id)) return;
    SDL_GameController* handle = SDL_GameControllerOpen(index);
    if (!handle) return;
    try {
        PadState state;
        state.handle = handle;
        mPads.emplace(id, std::move(state));
        NotifyGamepad({GamepadEventType::Connected, id, -1});
    } catch (...) { SDL_GameControllerClose(handle); }
}

void InputManager::RemoveGamepad(GamepadId id) noexcept
{
    try {
    auto it = mPads.find(id);
    if (it == mPads.end()) return;
    std::set<PlayerId> formerOwners;
    if (auto assigned = mPadOwners.find(id); assigned != mPadOwners.end()) {
        formerOwners = std::move(assigned->second);
        mPadOwners.erase(assigned);
    }
    for (PlayerId player : formerOwners) mLastPadOwner[id] = player;
    SDL_GameControllerClose(it->second.handle);
    mPads.erase(it);
    for (PlayerId player : formerOwners)
        NotifyGamepad({GamepadEventType::Unassigned, id, player});
    NotifyGamepad({GamepadEventType::Disconnected, id, GetLastAssignedPlayer(id)});
    } catch (...) { /* A failed notification must not unwind through SDL. */ }
}

std::vector<GamepadId> InputManager::GetConnectedGamepads() const
{
    std::vector<GamepadId> ids;
    for (const auto& [id, state] : mPads) ids.push_back(id);
    return ids;
}
bool InputManager::IsGamepadConnected(GamepadId id) const noexcept
{
    return mPads.contains(id);
}
std::string InputManager::GetGamepadName(GamepadId id) const
{
    const auto it = mPads.find(id);
    if (it == mPads.end() || !it->second.handle) return {};
    const char* name = SDL_GameControllerName(it->second.handle);
    return name ? name : "";
}
bool InputManager::GamepadDown(GamepadId id, GamepadButton button) const noexcept
{
    const auto it = mPads.find(id);
    const unsigned index = static_cast<unsigned>(button);
    return it != mPads.end() && index < 15 && it->second.buttons[index];
}
bool InputManager::GamepadPressed(GamepadId id, GamepadButton button) const noexcept
{
    const auto it = mPads.find(id);
    const unsigned index = static_cast<unsigned>(button);
    return it != mPads.end() && index < 15 &&
        it->second.buttons[index] && !it->second.previousButtons[index];
}
bool InputManager::GamepadReleased(GamepadId id, GamepadButton button) const noexcept
{
    const auto it = mPads.find(id);
    const unsigned index = static_cast<unsigned>(button);
    return it != mPads.end() && index < 15 &&
        !it->second.buttons[index] && it->second.previousButtons[index];
}
float InputManager::GamepadAxisValue(GamepadId id, ::GamepadAxis axis, float deadzone) const noexcept
{
    const auto it = mPads.find(id);
    const unsigned index = static_cast<unsigned>(axis);
    if (it == mPads.end() || index >= 6) return 0.f;
    const float value = it->second.axes[index];
    deadzone = std::clamp(deadzone, 0.f, 0.99f);
    if (std::abs(value) <= deadzone) return 0.f;
    return std::copysign((std::abs(value) - deadzone) / (1.f - deadzone), value);
}
InputListenerId InputManager::OnGamepadEvent(std::function<void(const GamepadEvent&)> callback)
{
    const auto id = mNextPadListener++;
    mPadListeners.push_back({id, std::move(callback)});
    return id;
}
bool InputManager::RemoveGamepadListener(InputListenerId id) noexcept
{
    const size_t before = mPadListeners.size();
    mPadListeners.erase(std::remove_if(mPadListeners.begin(), mPadListeners.end(),
        [id](const PadListener& listener) { return listener.id == id; }), mPadListeners.end());
    return mPadListeners.size() != before;
}

bool InputManager::ConfigurePlayer(PlayerInputSettings settings)
{
    const PlayerId player = settings.player;
    if (!settings.scheme.empty()) {
        const auto it = mSchemes.find(settings.scheme);
        if (it != mSchemes.end()) {
            settings.keyboard = it->second.keyboard;
            settings.mouse = it->second.mouse;
            settings.gamepad = it->second.gamepad;
            settings.touch = it->second.touch;
        }
    }
    if (settings.mode == InputAssignmentMode::Strict) {
        if (settings.idealDevice == InputDeviceKind::Gamepad) {
            settings.keyboard = false;
            settings.mouse = false;
            settings.touch = false;
        } else if (settings.idealDevice == InputDeviceKind::Keyboard) {
            settings.gamepad = false;
            settings.touch = false;
        } else if (settings.idealDevice == InputDeviceKind::Touch) {
            settings.keyboard = false;
            settings.mouse = false;
            settings.gamepad = false;
        }
    }
    for (const auto& [other, existing] : mPlayers) {
        if (other == player) continue;
        if (((settings.keyboard && existing.keyboard) || (settings.mouse && existing.mouse)) &&
            (!settings.shareDevices || !existing.shareDevices)) {
            mLastError = "Exclusive keyboard/mouse already belongs to player " +
                std::to_string(other);
            return false;
        }
    }
    mPlayers[player] = std::move(settings);
    mLastError.clear();
    return true;
}
const PlayerInputSettings* InputManager::GetPlayerSettings(PlayerId id) const noexcept
{
    const auto it = mPlayers.find(id);
    return it == mPlayers.end() ? nullptr : &it->second;
}
bool InputManager::RemovePlayer(PlayerId player)
{
    if (!mPlayers.erase(player)) return false;
    for (auto it = mPadOwners.begin(); it != mPadOwners.end();) {
        if (it->second.erase(player))
            NotifyGamepad({GamepadEventType::Unassigned, it->first, player});
        if (it->second.empty()) it = mPadOwners.erase(it);
        else ++it;
    }
    mLastActive.erase(player);
    for (auto& [map, actions] : mActions)
        for (auto& [name, action] : actions) action->Clear(player);
    return true;
}
bool InputManager::AssignGamepad(PlayerId player, GamepadId id, bool allowShared)
{
    if (!mPads.contains(id) || !mPlayers.contains(player) || !mPlayers[player].gamepad)
        return false;
    auto& owners = mPadOwners[id];
    if (owners.contains(player)) return true;
    if (!owners.empty() && !allowShared) return false;
    if (!owners.empty() && allowShared) {
        if (!mPlayers[player].shareDevices) return false;
        for (PlayerId existing : owners)
            if (!mPlayers[existing].shareDevices) return false;
    }
    if (!mPlayers[player].allowMultipleGamepads && !GetPlayerGamepads(player).empty())
        return false;
    owners.insert(player);
    mLastPadOwner[id] = player;
    NotifyGamepad({GamepadEventType::Assigned, id, player});
    return true;
}
bool InputManager::UnassignGamepad(PlayerId player, GamepadId id)
{
    const auto it = mPadOwners.find(id);
    if (it == mPadOwners.end() || !it->second.erase(player)) return false;
    mLastPadOwner[id] = player;
    NotifyGamepad({GamepadEventType::Unassigned, id, player});
    if (it->second.empty()) mPadOwners.erase(it);
    return true;
}
bool InputManager::AssignKeyboard(PlayerId player, bool allowShared)
{
    if (!mPlayers.contains(player)) return false;
    for (const auto& [id, settings] : mPlayers)
        if (id != player && settings.keyboard &&
            (!allowShared || !settings.shareDevices || !mPlayers[player].shareDevices))
            return false;
    mPlayers[player].keyboard = true;
    return true;
}
bool InputManager::AssignMouse(PlayerId player, bool allowShared)
{
    if (!mPlayers.contains(player)) return false;
    for (const auto& [id, settings] : mPlayers)
        if (id != player && settings.mouse &&
            (!allowShared || !settings.shareDevices || !mPlayers[player].shareDevices))
            return false;
    mPlayers[player].mouse = true;
    return true;
}
std::vector<GamepadId> InputManager::GetPlayerGamepads(PlayerId player) const
{
    std::vector<GamepadId> result;
    for (const auto& [id, owners] : mPadOwners)
        if (owners.contains(player) && mPads.contains(id)) result.push_back(id);
    return result;
}
PlayerId InputManager::GetLastAssignedPlayer(GamepadId id) const noexcept
{
    const auto found = mLastPadOwner.find(id);
    return found == mLastPadOwner.end() ? -1 : found->second;
}
InputDevice InputManager::GetLastActiveDevice(PlayerId player) const noexcept
{
    const auto found = mLastActive.find(player);
    return found == mLastActive.end() ? InputDevice{} : found->second;
}
void InputManager::MarkActive(InputDevice device) noexcept
{
    try {
        for (const auto& [player, settings] : mPlayers) {
            bool accepted = (device.kind == InputDeviceKind::Keyboard && settings.keyboard) ||
                (device.kind == InputDeviceKind::Mouse && settings.mouse);
            if (device.kind == InputDeviceKind::Gamepad) {
                auto found = mPadOwners.find(device.gamepad);
                accepted = found != mPadOwners.end() && found->second.contains(player);
            }
            if (accepted) mLastActive[player] = device;
        }
    } catch (...) {}
}
void InputManager::HandleJoin(GamepadId id) noexcept
{
    if (!mPads.contains(id) || mPadOwners.contains(id)) return;
    try {
        std::vector<PlayerInputSettings> candidates;
        for (const auto& [player, settings] : mPlayers)
            if (settings.gamepad && settings.autoJoin &&
                settings.mode != InputAssignmentMode::Manual &&
                (settings.allowMultipleGamepads || GetPlayerGamepads(player).empty()) &&
                (settings.replaceOnIdeal || settings.mode != InputAssignmentMode::Prefer ||
                 settings.idealDevice != InputDeviceKind::Gamepad ||
                 GetLastActiveDevice(player).kind == InputDeviceKind::None ||
                 GetLastActiveDevice(player).kind == InputDeviceKind::Gamepad) &&
                (settings.mode != InputAssignmentMode::Strict ||
                 settings.idealDevice == InputDeviceKind::Gamepad))
                candidates.push_back(settings);
        std::stable_sort(candidates.begin(), candidates.end(),
            [](const auto& a, const auto& b) {
                const bool aIdeal = a.mode == InputAssignmentMode::Prefer &&
                    a.idealDevice == InputDeviceKind::Gamepad;
                const bool bIdeal = b.mode == InputAssignmentMode::Prefer &&
                    b.idealDevice == InputDeviceKind::Gamepad;
                if (aIdeal != bIdeal) return aIdeal;
                if (a.priority != b.priority) return a.priority > b.priority;
                return a.player < b.player;
            });
        for (const auto& settings : candidates) {
            if (AssignGamepad(settings.player, id)) {
                mJoinedThisFrame.insert(id); // Joining must not also trigger Pause.
                MarkActive({InputDeviceKind::Gamepad, id});
                break;
            }
        }
    } catch (...) {}
}

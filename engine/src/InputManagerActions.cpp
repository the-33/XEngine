#include "InputManager.h"
#include "JsonSerializer.h"
#include "SDL.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cctype>

namespace {
using Json = nlohmann::json;

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
float Magnitude(InputValue value)
{
    return std::sqrt(value.x * value.x + value.y * value.y);
}
InputValue Limit(InputValue value)
{
    const float length = Magnitude(value);
    if (length > 1.f) { value.x /= length; value.y /= length; }
    return value;
}
InputValue Process(InputValue value, const InputProcessor& processor)
{
    switch (processor.type) {
    case InputProcessorType::Deadzone: {
        const float length = Magnitude(value);
        const float min = std::clamp(processor.value, 0.f, 0.99f);
        if (length <= min) return {};
        const float scale = (std::min(length, 1.f) - min) / ((1.f - min) * length);
        return {value.x * scale, value.y * scale};
    }
    case InputProcessorType::Scale:
    case InputProcessorType::Sensitivity:
        return {value.x * processor.value, value.y * processor.value};
    case InputProcessorType::Invert:
        return {-value.x, -value.y};
    case InputProcessorType::InvertX:
        return {-value.x, value.y};
    case InputProcessorType::InvertY:
        return {value.x, -value.y};
    case InputProcessorType::Normalize: {
        const float length = Magnitude(value);
        return length ? InputValue{value.x / length, value.y / length} : InputValue{};
    }
    case InputProcessorType::Clamp: {
        const float maximum = std::max(0.f, processor.max);
        return {std::clamp(value.x, -maximum, maximum),
                std::clamp(value.y, -maximum, maximum)};
    }
    case InputProcessorType::Curve: {
        const float exponent = std::max(0.01f, processor.value);
        return {std::copysign(std::pow(std::abs(value.x), exponent), value.x),
                std::copysign(std::pow(std::abs(value.y), exponent), value.y)};
    }
    case InputProcessorType::Smooth:
        return value; // Evaluated after combining sources, in the player state.
    }
    return value;
}
Json ReadJson(const std::string& path)
{
    const auto size = std::filesystem::file_size(path);
    if (size > 4u * 1024u * 1024u) throw std::runtime_error("Input JSON exceeds 4 MiB");
    std::ifstream stream(path);
    if (!stream) throw std::runtime_error("Cannot open: " + path);
    return Json::parse(stream);
}
int PadButton(std::string name);
int PadAxis(std::string name);
std::string BindingName(std::string path)
{
    const size_t slash = path.find('/');
    if (slash == std::string::npos || slash == 0 || slash == path.size() - 1)
        throw std::invalid_argument("Binding must be Device/Control: " + path);
    const auto family = Lower(path.substr(0, slash));
    if (family != "keyboard" && family != "mouse" && family != "gamepad")
        throw std::invalid_argument("Unknown binding device: " + path);
    const std::string control = path.substr(slash + 1);
    const std::string name = Lower(control);
    if (family == "keyboard") {
        if (name != "wasd" && name != "arrows") {
            auto validKey = [](const std::string& key) {
                return SDL_GetScancodeFromName(key.c_str()) != SDL_SCANCODE_UNKNOWN;
            };
            if (name.rfind("axis:", 0) == 0) {
                const size_t comma = control.find(',');
                if (comma == std::string::npos ||
                    !validKey(control.substr(5, comma - 5)) ||
                    !validKey(control.substr(comma + 1)))
                    throw std::invalid_argument("Invalid keyboard axis: " + path);
            } else {
                size_t from = 0;
                do {
                    const size_t end = control.find('+', from);
                    if (!validKey(control.substr(from, end - from)))
                        throw std::invalid_argument("Unknown SDL keyboard scancode: " + path);
                    if (end == std::string::npos) break;
                    from = end + 1;
                } while (from < control.size());
            }
        }
    } else if (family == "mouse") {
        const std::set<std::string> mouse = {
            "left", "middle", "right", "x1", "x2", "delta",
            "positiondelta", "deltax", "deltay", "wheel", "wheely",
            "1", "2", "3", "4", "5"
        };
        if (!mouse.contains(name)) throw std::invalid_argument("Unknown mouse control: " + path);
    } else if (name != "leftstick" && name != "rightstick" && name != "dpad" &&
               PadAxis(name) < 0 && PadButton(name) < 0)
        throw std::invalid_argument("Unknown gamepad control: " + path);
    return path;
}
InputActionType ParseActionType(const std::string& type)
{
    const auto value = Lower(type);
    if (value == "button") return InputActionType::Button;
    if (value == "axis1d") return InputActionType::Axis1D;
    if (value == "axis2d") return InputActionType::Axis2D;
    throw std::invalid_argument("Unknown action type: " + type);
}
InputCombineMode ParseCombine(const std::string& value)
{
    const auto lowered = Lower(value);
    if (lowered == "strongest") return InputCombineMode::Strongest;
    if (lowered == "latest") return InputCombineMode::Latest;
    if (lowered == "sumclamped") return InputCombineMode::SumClamped;
    throw std::invalid_argument("Unknown input combine mode: " + value);
}
InputProcessor ParseProcessor(const Json& node)
{
    if (!node.is_object()) throw std::invalid_argument("Processor must be an object");
    const std::string name = Lower(node.at("type").get<std::string>());
    const std::map<std::string, InputProcessorType> choices = {
        {"deadzone", InputProcessorType::Deadzone}, {"scale", InputProcessorType::Scale},
        {"invert", InputProcessorType::Invert}, {"invertx", InputProcessorType::InvertX},
        {"inverty", InputProcessorType::InvertY}, {"clamp", InputProcessorType::Clamp},
        {"normalize", InputProcessorType::Normalize}, {"curve", InputProcessorType::Curve},
        {"sensitivity", InputProcessorType::Sensitivity},
        {"smooth", InputProcessorType::Smooth}
    };
    const auto it = choices.find(name);
    if (it == choices.end()) throw std::invalid_argument("Unknown processor: " + name);
    InputProcessor result;
    result.type = it->second;
    result.value = node.value("value", name == "scale" || name == "sensitivity" ? 1.f : 0.15f);
    result.max = node.value("max", 1.f);
    if (!std::isfinite(result.value) || !std::isfinite(result.max) || result.max < 0)
        throw std::invalid_argument("Nonfinite or invalid processor parameters");
    return result;
}
InputInteraction ParseInteraction(const Json& node)
{
    const auto name = Lower(node.is_string() ? node.get<std::string>() :
        node.at("type").get<std::string>());
    InputInteraction result;
    if (name == "press") result.type = InputInteractionType::Press;
    else if (name == "release") result.type = InputInteractionType::Release;
    else if (name == "tap") result.type = InputInteractionType::Tap;
    else if (name == "slowtap") result.type = InputInteractionType::SlowTap;
    else if (name == "hold") result.type = InputInteractionType::Hold;
    else if (name == "multitap" || name == "doubletap")
        result.type = InputInteractionType::MultiTap;
    else throw std::invalid_argument("Unknown interaction: " + name);
    if (node.is_object()) {
        result.duration = node.value("duration", 0.3f);
        result.taps = node.value("taps", 2u);
    }
    if (!std::isfinite(result.duration) || result.duration < 0 || result.taps == 0 ||
        result.taps > 32) throw std::invalid_argument("Invalid interaction parameters");
    return result;
}
bool KeyHeld(const uint8_t* keys, bool focused, const std::string& name)
{
    if (!keys || !focused) return false;
    const SDL_Scancode scan = SDL_GetScancodeFromName(name.c_str());
    return scan != SDL_SCANCODE_UNKNOWN && keys[scan];
}
int PadButton(std::string name)
{
    name = Lower(name);
    const std::map<std::string, int> names = {
        {"south", 0}, {"a", 0}, {"east", 1}, {"b", 1}, {"west", 2}, {"x", 2},
        {"north", 3}, {"y", 3}, {"back", 4}, {"guide", 5}, {"start", 6},
        {"leftstickbutton", 7}, {"rightstickbutton", 8},
        {"leftshoulder", 9}, {"rightshoulder", 10},
        {"dpadup", 11}, {"dpaddown", 12}, {"dpadleft", 13}, {"dpadright", 14}
    };
    const auto it = names.find(name);
    if (it != names.end()) return it->second;
    if (!name.empty() && std::all_of(name.begin(), name.end(), ::isdigit)) {
        const int value = std::stoi(name);
        return value >= 0 && value < 15 ? value : -1;
    }
    return -1;
}
int PadAxis(std::string name)
{
    name = Lower(name);
    if (name == "leftx") return 0;
    if (name == "lefty") return 1;
    if (name == "rightx") return 2;
    if (name == "righty") return 3;
    if (name == "lefttrigger" || name == "lt") return 4;
    if (name == "righttrigger" || name == "rt") return 5;
    return -1;
}
} // namespace

InputValue InputManager::SampleBinding(const InputBinding& binding, PlayerId player,
                                        InputActionType type) const noexcept
{
    try {
        const auto found = mPlayers.find(player);
        if (found == mPlayers.end()) return {};
        const size_t slash = binding.path.find('/');
        if (slash == std::string::npos) return {};
        const std::string family = Lower(binding.path.substr(0, slash));
        const std::string control = binding.path.substr(slash + 1);
        const std::string lower = Lower(control);
        InputValue result{};
        if (family == "keyboard" && found->second.keyboard) {
            const auto held = [this](const std::string& name) {
                return KeyHeld(mCurrKeys, mFocused, name);
            };
            if (lower == "wasd" || lower == "arrows") {
                const bool wasd = lower == "wasd";
                result.x = float(held(wasd ? "D" : "Right")) - float(held(wasd ? "A" : "Left"));
                result.y = float(held(wasd ? "S" : "Down")) - float(held(wasd ? "W" : "Up"));
                result = Limit(result);
            } else if (lower.rfind("axis:", 0) == 0) {
                const size_t comma = control.find(',');
                if (comma != std::string::npos)
                    result.x = float(held(control.substr(comma + 1))) -
                        float(held(control.substr(5, comma - 5)));
            } else if (control.find('+') != std::string::npos) {
                bool all = true;
                size_t from = 0;
                while (from < control.size()) {
                    const size_t end = control.find('+', from);
                    all = all && held(control.substr(from, end - from));
                    if (end == std::string::npos) break;
                    from = end + 1;
                }
                result.x = all ? 1.f : 0.f;
            } else result.x = held(control) ? 1.f : 0.f;
        } else if (family == "mouse" && found->second.mouse && mFocused) {
            if (lower == "delta" || lower == "positiondelta")
                result = {float(mMouseDeltaX), float(mMouseDeltaY)};
            else if (lower == "deltax") result.x = float(mMouseDeltaX);
            else if (lower == "deltay") result.x = float(mMouseDeltaY);
            else if (lower == "wheely" || lower == "wheel") result.x = float(mWheelY);
            else {
                int button = -1;
                if (lower == "left") button = 1;
                if (lower == "middle") button = 2;
                if (lower == "right") button = 3;
                if (lower == "x1") button = 4;
                if (lower == "x2") button = 5;
                if (button < 0 && !lower.empty() &&
                    std::all_of(lower.begin(), lower.end(), ::isdigit))
                    button = std::stoi(lower);
                if (button >= 1 && button <= 5)
                    result.x = (mCurrMouseButtons & SDL_BUTTON(button)) ? 1.f : 0.f;
            }
        } else if (family == "gamepad") {
            for (GamepadId id : GetPlayerGamepads(player)) {
                if (lower == "start" && mJoinedThisFrame.contains(id)) continue;
                InputValue sample{};
                if (lower == "leftstick" || lower == "rightstick") {
                    const unsigned a = lower == "leftstick" ? 0u : 2u;
                    sample = {GamepadAxisValue(id, static_cast<::GamepadAxis>(a), 0.f),
                              GamepadAxisValue(id, static_cast<::GamepadAxis>(a + 1), 0.f)};
                } else if (lower == "dpad") {
                    sample = {float(GamepadDown(id, GamepadButton::DpadRight)) -
                                  float(GamepadDown(id, GamepadButton::DpadLeft)),
                              float(GamepadDown(id, GamepadButton::DpadDown)) -
                                  float(GamepadDown(id, GamepadButton::DpadUp))};
                    sample = Limit(sample);
                } else if (const int axis = PadAxis(lower); axis >= 0)
                    sample.x = GamepadAxisValue(id, static_cast<::GamepadAxis>(axis), 0.f);
                else if (const int button = PadButton(lower); button >= 0)
                    sample.x = GamepadDown(id, static_cast<GamepadButton>(button)) ? 1.f : 0.f;
                if (Magnitude(sample) > Magnitude(result)) result = sample;
            }
        }
        for (const auto& processor : binding.processors) result = Process(result, processor);
        if (type == InputActionType::Button)
            return {Magnitude(result) >= 0.5f ? 1.f : 0.f, 0.f};
        if (type == InputActionType::Axis1D) result.y = 0.f;
        return result;
    } catch (...) { return {}; }
}

void InputManager::EvaluateActions(float dt) noexcept
{
    try {
        for (auto& [map, actions] : mActions)
            for (auto& [name, action] : actions)
                for (const auto& [player, settings] : mPlayers) {
                    if (!IsActionMapEnabled(map, player)) {
                        action->Clear(player);
                        continue;
                    }
                    InputValue combined{};
                    for (const InputBinding& binding : action->bindings_) {
                        const InputValue candidate = SampleBinding(binding, player, action->type_);
                        if (action->combine_ == InputCombineMode::SumClamped) {
                            combined.x += candidate.x;
                            combined.y += candidate.y;
                        } else if (action->combine_ == InputCombineMode::Latest) {
                            if (candidate.Pressed()) combined = candidate;
                        } else if (Magnitude(candidate) > Magnitude(combined))
                            combined = candidate;
                    }
                    if (action->combine_ == InputCombineMode::SumClamped)
                        combined = Limit(combined);
                    for (const auto& processor : action->processors_) {
                        if (processor.type == InputProcessorType::Smooth) {
                            auto& state = action->states_[player];
                            const float strength = std::max(0.f, processor.value);
                            const float factor = 1.f - std::exp(-strength * dt);
                            state.smoothed.x += (combined.x - state.smoothed.x) * factor;
                            state.smoothed.y += (combined.y - state.smoothed.y) * factor;
                            combined = state.smoothed;
                            if (Magnitude(combined) < 0.005f) {
                                state.smoothed = {};
                                combined = {};
                            }
                        } else combined = Process(combined, processor);
                    }
                    if (action->type_ == InputActionType::Button)
                        combined = {Magnitude(combined) >= 0.5f ? 1.f : 0.f, 0.f};
                    action->Advance(player, combined, dt);
                }
    } catch (...) { /* Retain noexcept SDL loop. */ }
}

InputAction* InputManager::AddAction(const std::string& map, const std::string& action,
                                    InputActionType type)
{
    if (map.empty() || action.empty() || map.find('/') != std::string::npos ||
        action.find('/') != std::string::npos) return nullptr;
    auto& actions = mActions[map];
    auto it = actions.find(action);
    if (it != actions.end()) return it->second->Type() == type ? it->second.get() : nullptr;
    auto inserted = actions.emplace(action,
        std::unique_ptr<InputAction>(new InputAction(action, type)));
    return inserted.first->second.get();
}
InputAction* InputManager::FindAction(const std::string& path) const noexcept
{
    const size_t slash = path.find('/');
    if (slash == std::string::npos) return nullptr;
    const auto map = mActions.find(path.substr(0, slash));
    if (map == mActions.end()) return nullptr;
    const auto action = map->second.find(path.substr(slash + 1));
    return action == map->second.end() ? nullptr : action->second.get();
}
bool InputManager::BindAction(const std::string& path, InputBinding binding)
{
    if (!FindAction(path)) return false;
    try {
        BindingName(binding.path);
        for (const auto& processor : binding.processors)
            if (processor.type == InputProcessorType::Smooth)
                return false;
        FindAction(path)->AddBinding(std::move(binding));
        return true;
    } catch (...) { return false; }
}
bool InputManager::EnableActionMap(const std::string& map, PlayerId player) noexcept
{
    if (!mActions.contains(map)) return false;
    mDisabledMaps.erase({player, map});
    return true;
}
bool InputManager::DisableActionMap(const std::string& map, PlayerId player) noexcept
{
    if (!mActions.contains(map)) return false;
    try { mDisabledMaps.insert({player, map}); }
    catch (...) { return false; }
    for (auto& [name, action] : mActions.at(map)) action->Clear(player);
    return true;
}
bool InputManager::IsActionMapEnabled(const std::string& map, PlayerId player) const noexcept
{
    const auto it = mPlayers.find(player);
    if (it == mPlayers.end() || mDisabledMaps.contains({player, map})) return false;
    if (const auto stack = mContextStacks.find(player);
        stack != mContextStacks.end()) {
        for (auto context = stack->second.rbegin(); context != stack->second.rend(); ++context) {
            if (std::find(context->maps.begin(), context->maps.end(), map) != context->maps.end())
                return true;
            if (context->exclusive) return false;
        }
    }
    const auto& selected = it->second.maps;
    return selected.empty() || std::find(selected.begin(), selected.end(), map) != selected.end();
}
bool InputManager::PushActionContext(PlayerId player, const std::string& name)
{
    const auto it = mNamedContexts.find(name);
    return it != mNamedContexts.end() && PushActionContext(player, it->second);
}
bool InputManager::PushActionContext(PlayerId player, InputContext context)
{
    if (!mPlayers.contains(player) || context.name.empty()) return false;
    for (const auto& map : context.maps)
        if (!mActions.contains(map)) return false;
    mContextStacks[player].push_back(std::move(context));
    return true;
}
bool InputManager::PopActionContext(PlayerId player) noexcept
{
    const auto it = mContextStacks.find(player);
    if (it == mContextStacks.end() || it->second.empty()) return false;
    it->second.pop_back();
    return true;
}
const InputContext* InputManager::CurrentActionContext(PlayerId player) const noexcept
{
    const auto it = mContextStacks.find(player);
    return it == mContextStacks.end() || it->second.empty() ? nullptr : &it->second.back();
}

bool InputManager::LoadActionAsset(const std::string& path) noexcept
{
    try {
        const Json root = ReadJson(path);
        const auto& maps = root.at("maps");
        if (!maps.is_object()) throw std::invalid_argument("maps must be an object");
        // Validate the entire file before changing live action handles.
        struct Definition {
            std::string map, name;
            InputActionType type;
            InputCombineMode combine;
            InputInteraction interaction;
            std::vector<InputProcessor> processors;
            std::vector<InputBinding> bindings;
        };
        std::vector<Definition> definitions;
        std::map<std::string, InputContext> contexts;
        for (auto mapIt = maps.begin(); mapIt != maps.end(); ++mapIt) {
            if (mapIt.key().empty() || mapIt.key().find('/') != std::string::npos)
                throw std::invalid_argument("Invalid action map name");
            const Json& source = mapIt.value().contains("actions") ?
                mapIt.value().at("actions") : mapIt.value();
            if (!source.is_object()) throw std::invalid_argument("Action map must be an object");
            for (auto actionIt = source.begin(); actionIt != source.end(); ++actionIt) {
                if (actionIt.key().empty() || actionIt.key().find('/') != std::string::npos)
                    throw std::invalid_argument("Invalid action name");
                const Json& spec = actionIt.value();
                Definition def{mapIt.key(), actionIt.key(),
                    ParseActionType(spec.at("type").get<std::string>()),
                    ParseCombine(spec.value("combine", "Strongest")),
                    spec.contains("interaction") ? ParseInteraction(spec.at("interaction")) :
                        InputInteraction{}, {}, {}};
                if (spec.contains("processors"))
                    for (const auto& p : spec.at("processors")) def.processors.push_back(ParseProcessor(p));
                for (const auto& b : spec.at("bindings")) {
                    InputBinding binding;
                    binding.path = BindingName(b.is_string() ?
                        b.get<std::string>() : b.at("path").get<std::string>());
                    if (b.is_object() && b.contains("processors"))
                        for (const auto& p : b.at("processors")) {
                            auto processor = ParseProcessor(p);
                            if (processor.type == InputProcessorType::Smooth)
                                throw std::invalid_argument("Smooth is only valid on an action");
                            binding.processors.push_back(processor);
                        }
                    def.bindings.push_back(std::move(binding));
                }
                if (def.bindings.empty()) throw std::invalid_argument("Action without bindings");
                definitions.push_back(std::move(def));
            }
        }
        if (root.contains("contexts")) {
            for (auto it = root.at("contexts").begin(); it != root.at("contexts").end(); ++it) {
                InputContext context;
                context.name = it.key();
                context.exclusive = it.value().value("exclusive", true);
                context.maps = it.value().at("maps").get<std::vector<std::string>>();
                if (context.name.empty() || context.maps.empty())
                    throw std::invalid_argument("Invalid input context");
                for (const auto& map : context.maps)
                    if (!maps.contains(map) && !mActions.contains(map))
                        throw std::invalid_argument("Context names unknown map: " + map);
                contexts.emplace(context.name, std::move(context));
            }
        }
        for (const auto& def : definitions)
            if (!def.map.empty() && FindAction(def.map + "/" + def.name))
                throw std::invalid_argument("Existing action would be replaced: " + def.map + "/" + def.name);
        for (auto& def : definitions) {
            InputAction* action = AddAction(def.map, def.name, def.type);
            if (!action) throw std::invalid_argument("Invalid map/action name");
            action->SetCombineMode(def.combine);
            action->SetInteraction(def.interaction);
            for (auto& p : def.processors) action->AddProcessor(p);
            for (auto& b : def.bindings) action->AddBinding(std::move(b));
        }
        mNamedContexts.insert(contexts.begin(), contexts.end());
        mLastError.clear();
        return true;
    } catch (const std::exception& e) { mLastError = e.what(); return false; }
    catch (...) { mLastError = "Failed to parse input actions"; return false; }
}

bool InputManager::RebindAction(const std::string& path, const std::string& newBinding,
                                size_t index, bool rejectConflicts)
{
    try {
        InputAction* action = FindAction(path);
        if (!action || index >= action->bindings_.size()) return false;
        BindingName(newBinding);
        if (rejectConflicts)
            for (const auto& [map, actions] : mActions)
                for (const auto& [name, other] : actions)
                    for (size_t i = 0; i < other->bindings_.size(); ++i)
                        if (other->bindings_[i].path == newBinding &&
                            (other.get() != action || i != index)) {
                            mLastError = "Binding conflict: " + map + "/" + name;
                            return false;
                        }
        action->bindings_[index].path = newBinding;
        mLastError.clear();
        return true;
    } catch (const std::exception& e) { mLastError = e.what(); return false; }
}
bool InputManager::BeginRebind(const std::string& path, size_t index,
                               bool rejectConflicts) noexcept
{
    try {
        InputAction* action = FindAction(path);
        if (!action || index >= action->bindings_.size()) return false;
        mRebind = {path, index, rejectConflicts, true};
        return true;
    } catch (...) { return false; }
}
void InputManager::HandleRebindPath(const std::string& path) noexcept
{
    if (!mRebind.active) return;
    try {
        if (RebindAction(mRebind.action, path, mRebind.index, mRebind.rejectConflicts))
            mRebind.active = false;
    } catch (...) {}
}
bool InputManager::SaveBindingOverrides(const std::string& path) const noexcept
{
    try {
        Json root = Json::object();
        root["bindings"] = Json::object();
        for (const auto& [map, actions] : mActions)
            for (const auto& [name, action] : actions) {
                auto& dest = root["bindings"][map + "/" + name];
                dest = Json::array();
                for (const auto& binding : action->Bindings()) dest.push_back(binding.path);
            }
        return JsonSerializer::SaveToFile(path, root) == FileError::None;
    } catch (...) { return false; }
}
bool InputManager::LoadBindingOverrides(const std::string& path) noexcept
{
    try {
        const Json root = ReadJson(path);
        struct Change { InputAction* action; size_t index; std::string path; };
        std::vector<Change> changes;
        for (auto it = root.at("bindings").begin(); it != root.at("bindings").end(); ++it) {
            InputAction* action = FindAction(it.key());
            if (!action || !it.value().is_array() ||
                it.value().size() != action->bindings_.size())
                throw std::invalid_argument("Unknown action or incompatible binding count: " + it.key());
            for (size_t index = 0; index < it.value().size(); ++index)
                changes.push_back({action, index, BindingName(it.value()[index].get<std::string>())});
        }
        for (const auto& c : changes) c.action->bindings_[c.index].path = c.path;
        mLastError.clear();
        return true;
    } catch (const std::exception& e) { mLastError = e.what(); return false; }
    catch (...) { mLastError = "Could not read binding overrides"; return false; }
}

bool InputManager::LoadPlayerProfiles(const std::string& path) noexcept
{
    try {
        const Json root = ReadJson(path);
        std::vector<PlayerInputSettings> profiles;
        for (const auto& node : root.at("players")) {
            PlayerInputSettings profile;
            profile.player = node.at("player").get<PlayerId>();
            profile.scheme = node.value("scheme", std::string{});
            const auto mode = Lower(node.value("mode", "Manual"));
            if (mode == "manual") profile.mode = InputAssignmentMode::Manual;
            else if (mode == "strict") profile.mode = InputAssignmentMode::Strict;
            else if (mode == "prefer") profile.mode = InputAssignmentMode::Prefer;
            else if (mode == "any") profile.mode = InputAssignmentMode::Any;
            else throw std::invalid_argument("Unknown join mode: " + mode);
            const auto ideal = Lower(node.value("idealDevice", "Gamepad"));
            if (ideal == "keyboard") profile.idealDevice = InputDeviceKind::Keyboard;
            else if (ideal == "mouse") profile.idealDevice = InputDeviceKind::Mouse;
            else if (ideal == "gamepad") profile.idealDevice = InputDeviceKind::Gamepad;
            else throw std::invalid_argument("Unknown ideal device: " + ideal);
            profile.keyboard = node.value("keyboard", false);
            profile.mouse = node.value("mouse", false);
            profile.gamepad = node.value("gamepad", true);
            profile.touch = node.value("touch", false);
            profile.autoJoin = node.value("autoJoin", false);
            profile.shareDevices = node.value("shareDevices", false);
            profile.replaceOnIdeal = node.value("replaceOnIdeal", true);
            profile.allowMultipleGamepads = node.value("allowMultipleGamepads", false);
            profile.priority = node.value("priority", 0);
            if (node.contains("maps")) profile.maps = node.at("maps").get<std::vector<std::string>>();
            if (profile.player < 0 || profile.player > 255)
                throw std::invalid_argument("Player ID outside [0,255]");
            profiles.push_back(std::move(profile));
        }
        std::set<PlayerId> unique;
        for (const auto& profile : profiles)
            if (!unique.insert(profile.player).second)
                throw std::invalid_argument("Duplicate player ID");
        auto previous = mPlayers;
        for (const auto& profile : profiles)
            if (!ConfigurePlayer(profile)) {
                const std::string error = mLastError;
                mPlayers = std::move(previous);
                throw std::invalid_argument(error);
            }
        mLastError.clear();
        return true;
    } catch (const std::exception& e) { mLastError = e.what(); return false; }
    catch (...) { mLastError = "Could not read player profiles"; return false; }
}

bool InputManager::LoadInputSchemes(const std::string& path) noexcept
{
    try {
        const Json root = ReadJson(path);
        std::map<std::string, InputControlScheme> schemes;
        for (auto it = root.at("schemes").begin(); it != root.at("schemes").end(); ++it) {
            if (it.key().empty()) throw std::invalid_argument("Empty scheme name");
            const auto& node = it.value();
            schemes[it.key()] = {
                node.value("keyboard", false), node.value("mouse", false),
                node.value("gamepad", false), node.value("touch", false)
            };
        }
        mSchemes = std::move(schemes);
        mLastError.clear();
        return true;
    } catch (const std::exception& e) { mLastError = e.what(); return false; }
    catch (...) { mLastError = "Could not read input schemes"; return false; }
}

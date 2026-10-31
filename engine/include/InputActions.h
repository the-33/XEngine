#pragma once
/** @file InputActions.h
 * @brief Device-independent actions, bindings, player policies and event contracts.
 * @ingroup xengine_services
 */

#include "BaseTypes.h"
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

/** @brief Logical player slot ID, independent of device and character. */
using PlayerId = int;
using GamepadId = int32_t; ///< SDL joystick instance ID, never a device-list index.
/** @brief Registration token for explicit callback unsubscription. */
using InputListenerId = uint64_t;

/** Physical device family; keyboard and mouse are independently assignable. */
enum class InputDeviceKind {
    None,     ///< No recent source or unassigned.
    Keyboard, ///< Physical keyboard.
    Mouse,    ///< Mouse buttons, movement and wheel.
    Gamepad,  ///< Mapped SDL2 GameController.
    Touch     ///< Reserved for future touch adapters.
};
struct InputDevice {
    InputDeviceKind kind = InputDeviceKind::None; ///< Device family.
    GamepadId gamepad = -1; ///< Joystick instance ID if kind is Gamepad.
    /** @brief Compare device identities. */
    bool operator==(const InputDevice&) const = default;
};

/** How PlayerInput may claim a device when joining or changing controls.
 * Manual: only explicit assignment; Strict: ideal family only;
 * Prefer: ideal family first, with fallback and optional promotion;
 * Any: any permitted device. */
enum class InputAssignmentMode {
    Manual, ///< Explicit assignment only.
    Strict, ///< Only the ideal device family auto-joins.
    Prefer, ///< Prefer ideal devices while allowing configured alternatives.
    Any     ///< Any permitted device may join.
};

/** Configurable player slot, independent of a game object or physical device. */
struct PlayerInputSettings {
    PlayerId player = 0; ///< The logical slot to configure.
    std::string scheme; ///< Optional named capability scheme from an inputscheme JSON.
    InputAssignmentMode mode = InputAssignmentMode::Manual; ///< Assignment policy.
    InputDeviceKind idealDevice = InputDeviceKind::Gamepad; ///< Preferred family.
    bool keyboard = false; ///< Keyboard capability or direct ownership.
    bool mouse = false; ///< Mouse capability or direct ownership.
    bool gamepad = true; ///< Permit gamepad assignment.
    bool touch = false; ///< Reserved for platform touch adapters.
    bool autoJoin = false; ///< Allow an unassigned gamepad to join via Start.
    bool shareDevices = false; ///< Opt in to shared ownership.
    bool replaceOnIdeal = true; ///< Allow ideal pad to join after fallback keyboard use.
    bool allowMultipleGamepads = false; ///< Multiple pads may feed this one player.
    int priority = 0; ///< Higher-priority slots claim new devices first.
    std::vector<std::string> maps; ///< Empty means all action maps are enabled.
};

/** Reusable device capability declaration, independent of a player slot. */
struct InputControlScheme {
    bool keyboard = true; ///< Keyboard permitted.
    bool mouse = true; ///< Mouse permitted.
    bool gamepad = true; ///< SDL2 GameController permitted.
    bool touch = false; ///< Future touch capability declaration.
};
/** @brief One named layer on a player's map context stack. */
struct InputContext {
    std::string name; ///< Context identifier.
    std::vector<std::string> maps; ///< Active maps in this context.
    bool exclusive = true; ///< Block lower-level maps not included here.
};

/** @brief Standard SDL2 GameController button layout. */
enum class GamepadButton : uint8_t {
    South = 0,     ///< Lower face button (A on Xbox).
    East,          ///< Right face button (B on Xbox).
    West,          ///< Left face button (X on Xbox).
    North,         ///< Upper face button (Y on Xbox).
    Back,          ///< Back/select button.
    Guide,         ///< Platform guide/home.
    Start,         ///< Menu/start (also used for optional auto-join).
    LeftStick,     ///< Left stick press.
    RightStick,    ///< Right stick press.
    LeftShoulder,  ///< Left shoulder button.
    RightShoulder, ///< Right shoulder button.
    DpadUp,        ///< D-pad up.
    DpadDown,      ///< D-pad down.
    DpadLeft,      ///< D-pad left.
    DpadRight      ///< D-pad right.
};
/** @brief Standard controller axis indices and value ranges. */
enum class GamepadAxis : uint8_t {
    LeftX = 0,   ///< Left-stick horizontal -1..1.
    LeftY,       ///< Left-stick vertical -1..1.
    RightX,      ///< Right-stick horizontal -1..1.
    RightY,      ///< Right-stick vertical -1..1.
    LeftTrigger, ///< Left trigger 0..1.
    RightTrigger ///< Right trigger 0..1.
};
/** @brief Physical and logical controller lifecycle events. */
enum class GamepadEventType {
    Connected,    ///< Device physically connected.
    Disconnected, ///< Device physically removed.
    Assigned,     ///< Device attached to a player.
    Unassigned    ///< Device detached from a player.
};
/** @brief Gamepad event with the affected player (if known). */
struct GamepadEvent {
    GamepadEventType type = GamepadEventType::Connected; ///< Event kind.
    GamepadId id = -1; ///< Device instance ID.
    PlayerId player = -1; ///< Affected/last owning player, or -1.
};

/** @brief Supported logical action value types. */
enum class InputActionType {
    Button, ///< Boolean state.
    Axis1D, ///< Floating-point state.
    Axis2D  ///< 2D vector state.
};
/** @brief Rule for merging simultaneous physical bindings. */
enum class InputCombineMode {
    Strongest,  ///< Use the greatest-magnitude source.
    Latest,     ///< Use the last actuated binding in definition order.
    SumClamped  ///< Add sources and cap resulting magnitude.
};
/** @brief Time/edge condition that causes performed events. */
enum class InputInteractionType {
    Press,   ///< Perform on press edge.
    Release, ///< Perform on release edge.
    Tap,     ///< Perform on quick release.
    SlowTap, ///< Perform on release after a minimum hold.
    Hold,    ///< Perform once after a sustained hold.
    MultiTap ///< Perform after repeated quick taps.
};
/** @brief Numeric transforms applied to binding or action values. */
enum class InputProcessorType {
    Deadzone,   ///< Suppress/re-scale small values.
    Scale,      ///< Multiply axes.
    Invert,     ///< Reverse both axes.
    InvertX,    ///< Reverse horizontal component.
    InvertY,    ///< Reverse vertical component.
    Clamp,      ///< Restrict component magnitudes.
    Normalize,  ///< Normalize nonzero vectors to unit length.
    Curve,      ///< Power curve preserving component signs.
    Sensitivity,///< Multiply by sensitivity.
    Smooth      ///< Exponential low-pass filter at action level.
};
/** @brief Processor parameters and operation. */
struct InputProcessor {
    InputProcessorType type = InputProcessorType::Deadzone; ///< Processor operation.
    float value = 0.15f; ///< Main parameter (deadzone, scale, exponent, etc.).
    float max = 1.f; ///< Upper bound for Clamp/Deadzone.
};
/** @brief Interaction timing and count parameters. */
struct InputInteraction {
    InputInteractionType type = InputInteractionType::Press; ///< Interaction kind.
    float duration = 0.3f; ///< Hold time, tap maximum or multi-tap interval in seconds.
    unsigned taps = 2; ///< MultiTap count; DoubleTap is MultiTap with two taps.
};
/** One input source, optionally with value processors. Paths are documented in
 * docs/reference/game/INPUT_ACTIONS.md. */
struct InputBinding {
    std::string path; ///< Device/control path, e.g. Gamepad/LeftStick.
    std::vector<InputProcessor> processors; ///< Per-source value transforms.
};
/** @brief Physical or resolved numeric value with at most two axes. */
struct InputValue {
    float x = 0.f; ///< Horizontal or single-axis value.
    float y = 0.f; ///< Vertical value.
    /** @brief True for any nonzero component. */
    bool Pressed() const noexcept { return x != 0.f || y != 0.f; }
    /** @brief Return this numeric value as a Vec2. */
    Vec2 Vector() const noexcept { return {x, y}; }
};

/** @brief Callback phases emitted by an action. */
enum class InputActionPhase {
    Started,   ///< First frame with a nonzero value.
    Performed, ///< Interaction has been satisfied.
    Canceled   ///< Action returned to its idle state.
};
/** @brief Callback payload, including the logical player and sampled value. */
struct InputActionEvent {
    PlayerId player = 0; ///< Player receiving the action.
    InputActionPhase phase = InputActionPhase::Performed; ///< Triggered phase.
    InputValue value{}; ///< Resolved action value at emission time.
};

/** Stable until its action is removed or the InputManager is shut down.
 * Register callbacks with a token and remove it when the subscriber dies.
 * All callbacks run on the SDL/main thread during the input update. */
class InputAction final {
public:
    /** @brief Main-thread transition callback. */
    using Callback = std::function<void(const InputActionEvent&)>;
    /** @brief Standalone action for testing or an advanced custom input adapter.
     * Prefer InputManager::AddAction for normal gameplay.
     */
    InputAction(std::string name, InputActionType type)
        : name_(std::move(name)), type_(type) {}
    /** @brief Feeds a synthetic sample; the manager owns normal SDL-driven updates. */
    void Feed(PlayerId player, InputValue value, float deltaSeconds) noexcept {
        Advance(player, value, deltaSeconds);
    }

    /** @brief Action name within its parent map. */
    const std::string& Name() const noexcept { return name_; }
    /** @brief Declared value type. */
    InputActionType Type() const noexcept { return type_; }
    /** @brief Latest raw resolved value for a logical player. */
    InputValue Read(PlayerId player = 0) const noexcept;
    /** @brief Two-dimensional player value. */
    Vec2 ReadVec2(PlayerId player = 0) const noexcept { return Read(player).Vector(); }
    /** @brief Single-axis player value. */
    float ReadFloat(PlayerId player = 0) const noexcept { return Read(player).x; }
    /** @brief Whether the action has a nonzero value this frame. */
    bool IsPressed(PlayerId player = 0) const noexcept;
    /** @brief New pressed edge this frame. */
    bool WasPressedThisFrame(PlayerId player = 0) const noexcept;
    /** @brief New released edge this frame. */
    bool WasReleasedThisFrame(PlayerId player = 0) const noexcept;
    /** @brief Whether the configured interaction performed this frame. */
    bool WasPerformedThisFrame(PlayerId player = 0) const noexcept;
    /** @brief Typed convenience read: bool, float or Vec2. */
    template<class T> T ReadValue(PlayerId player = 0) const noexcept {
        if constexpr (std::is_same_v<T, Vec2>) return ReadVec2(player);
        else if constexpr (std::is_same_v<T, bool>) return IsPressed(player);
        else return static_cast<T>(ReadFloat(player));
    }
    /** @brief Listen for performed events, returning a removal token. */
    InputListenerId OnPerformed(Callback callback);
    /** @brief Listen for performed events with just the player ID. */
    InputListenerId OnPerformed(std::function<void(PlayerId)> callback) {
        return OnPerformed([callback = std::move(callback)](const InputActionEvent& event) {
            if (callback) callback(event.player);
        });
    }
    /** @brief Listen for new action activation. */
    InputListenerId OnStarted(Callback callback);
    /** @brief Listen for an active action returning to idle. */
    InputListenerId OnCanceled(Callback callback);
    /** @brief Unsubscribe a previously registered callback. */
    bool RemoveListener(InputListenerId id) noexcept;

    /** @brief Append a physical source. */
    void AddBinding(InputBinding binding) { bindings_.push_back(std::move(binding)); }
    /** @brief Inspect currently registered physical bindings. */
    const std::vector<InputBinding>& Bindings() const noexcept { return bindings_; }
    /** @brief Choose how simultaneous source values are combined. */
    void SetCombineMode(InputCombineMode mode) noexcept { combine_ = mode; }
    /** @brief Inspect the combination policy. */
    InputCombineMode CombineMode() const noexcept { return combine_; }
    /** @brief Set hold/tap/press interaction parameters. */
    void SetInteraction(InputInteraction interaction) noexcept { interaction_ = interaction; }
    /** @brief Read current interaction parameters. */
    const InputInteraction& Interaction() const noexcept { return interaction_; }
    /** @brief Append an action-level value processor. */
    void AddProcessor(InputProcessor processor) { processors_.push_back(processor); }

private:
    friend class InputManager;
    struct State {
        InputValue value{};       ///< Most recently evaluated action value.
        bool down = false;       ///< Whether the action is currently actuated.
        bool pressed = false;    ///< Whether the action became actuated this frame.
        bool released = false;   ///< Whether the action stopped being actuated this frame.
        bool performed = false;  ///< Whether the configured interaction completed this frame.
        float heldSeconds = 0.f; ///< Time the action has remained actuated, in seconds.
        float sinceTap = 0.f;    ///< Elapsed time since the previous tap, in seconds.
        unsigned tapCount = 0;   ///< Number of taps in the current interaction sequence.
        bool holdFired = false;  ///< Whether the hold interaction has already triggered.
        InputValue smoothed{};   ///< Value retained between smoothing updates.
    };
    void Advance(PlayerId player, InputValue value, float dt) noexcept;
    void Emit(PlayerId player, InputActionPhase phase, InputValue value) noexcept;
    void Clear(PlayerId player) noexcept;
    const State* GetState(PlayerId player) const noexcept;
    std::string name_;
    InputActionType type_;
    InputCombineMode combine_ = InputCombineMode::Strongest;
    InputInteraction interaction_{};
    std::vector<InputBinding> bindings_;
    std::vector<InputProcessor> processors_;
    std::unordered_map<PlayerId, State> states_;
    struct Listener {
        InputListenerId id;    ///< Identifier assigned to this action listener.
        InputActionPhase phase; ///< Action phase observed by the listener.
        Callback callback;     ///< Function invoked when the phase is emitted.
    };
    std::vector<Listener> listeners_;
    InputListenerId nextListener_ = 1;
};

/** Typed, non-owning facade for an action. Re-resolve after unloading actions. */
/** @brief Typed, non-owning action accessor for bool, float or Vec2. */
template<class T> class InputActionRef {
public:
    /** @brief Construct a non-owning typed action handle. */
    explicit InputActionRef(InputAction* action = nullptr) noexcept : action_(action) {}
    /** @brief Whether the action resolved with the expected type. */
    explicit operator bool() const noexcept { return action_ != nullptr; }
    /** @brief Read the player's typed action value. */
    T ReadValue(PlayerId player = 0) const noexcept {
        return action_ ? action_->ReadValue<T>(player) : T{};
    }
    /** @brief Whether the action is held. */
    bool IsPressed(PlayerId player = 0) const noexcept {
        return action_ && action_->IsPressed(player);
    }
    /** @brief Press edge in the current input frame. */
    bool WasPressedThisFrame(PlayerId player = 0) const noexcept {
        return action_ && action_->WasPressedThisFrame(player);
    }
    /** @brief Release edge in the current input frame. */
    bool WasReleasedThisFrame(PlayerId player = 0) const noexcept {
        return action_ && action_->WasReleasedThisFrame(player);
    }
    /** @brief Interaction was performed in this frame. */
    bool WasPerformedThisFrame(PlayerId player = 0) const noexcept {
        return action_ && action_->WasPerformedThisFrame(player);
    }
    /** @brief Subscribe to performed events including phase and value. */
    InputListenerId OnPerformed(InputAction::Callback callback) const {
        return action_ ? action_->OnPerformed(std::move(callback)) : 0;
    }
    /** @brief Subscribe to performed events carrying a PlayerId. */
    InputListenerId OnPerformed(std::function<void(PlayerId)> callback) const {
        return action_ ? action_->OnPerformed(std::move(callback)) : 0;
    }
    /** @brief Underlying non-owning action pointer. */
    InputAction* Get() const noexcept { return action_; }
private:
    InputAction* action_ = nullptr;
};

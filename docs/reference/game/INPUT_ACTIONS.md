# PlayerInput, input actions and local multiplayer {#xengine_input_actions}

XEngine supports **two input APIs simultaneously**. The classic `KeyDown`,
`KeyPressed`, `MouseDown`, etc. remain unchanged. The action system adds
device-independent controls, hot-plugged SDL2 controllers, player routing,
processors, interactions and JSON configuration. A `PlayerInput` component is
optional: you can use the same API directly from C++.

## Default BlankGame files and setup

New games include three editable JSON files under the **assets root**:

| File | Purpose |
|---|---|
| `input/Default.inputactions.json` | Gameplay, UI and Global maps; action types, bindings, processors and contexts |
| `input/Default.inputschemes.json` | Device capabilities, including reserved touch capability |
| `input/Default.players.json` | Local-player slots, ideal devices, priority, join and sharing policies |

The template's `src/InitCallback.hpp` loads schemes, then profiles, then
actions, and activates the `Playing` context. Paths are relative to the
assets working directory, including in Release.

Create other JSON assets with the existing generator:

~~~bash
./new_asset.sh inputmap Vehicle
./new_asset.sh inputscheme RacingWheel
./new_asset.sh inputprofile LocalCoop
~~~

On Windows use `new_asset.bat`. The generator writes into `assets/input/`,
never overwrites existing files, and does not alter your initializer: load
your new files explicitly.

## Read an action from any allowed device

~~~cpp
auto move = Input->FindAction<Vec2>("Gameplay/Move");
auto jump = Input->FindAction<bool>("Gameplay/Jump");

if (move) {
    Vec2 direction = move.ReadValue(0); // player 0; WASD diagonals normalized
    // Use direction to move your own character.
}
if (jump && jump.WasPressedThisFrame(0)) {
    // Jump once even if Space and gamepad South are pressed together.
}
~~~

There are three value types: `Button` (bool), `Axis1D` (float) and
`Axis2D` (`Vec2`). The template lookup returns an empty handle if the
configured action has a different type. An untyped handle can call
`ReadVec2(player)`, `ReadFloat(player)` and `IsPressed(player)`.
`WasPressedThisFrame`, `WasReleasedThisFrame` and
`WasPerformedThisFrame` are player-specific. Performed follows the
interaction, which need not fire on the first press.

Polling and callbacks can be combined:

~~~cpp
InputAction* action = Input->FindAction("Gameplay/Jump");
InputListenerId token = action->OnPerformed([](PlayerId player) {
    // React for exactly this player.
});
// Remove in your Behaviour's destructor, BEFORE destroying the action:
action->RemoveListener(token);
~~~

Callbacks run on the main SDL thread during the input phase, before the
gameplay `Update`. An action pointer is non-owning and invalid after
its action map is destroyed or the input manager shuts down. Callback
exceptions are caught by the input system.

## Action maps and contexts

~~~cpp
Input->EnableActionMap("Gameplay", 0);
Input->DisableActionMap("UI", 0);

Input->PushActionContext(0, "Menu"); // Menu = UI + Global (exclusive)
// When leaving the menu:
Input->PopActionContext(0);          // Restores the previous Playing context
~~~

Maps may be enabled together (e.g. Gameplay + Global). Named contexts are
declared in the actions JSON. The stack is per player: an exclusive context
blocks maps not listed in it, while a nonexclusive context overlays maps
without blocking those below. You can also push a C++ `InputContext`.
Explicitly disabled maps always remain disabled. Disabling a map releases
its held actions.

The default `UI/Navigate`, `UI/Submit` and `UI/Cancel` actions are consumed by
`UIManager` automatically for player 0 when its action map is loaded and
enabled. Navigation chooses the nearest selectable widget in the indicated
screen-space direction; holding a direction repeats at a controlled rate.
`UI/Submit` activates a focused button/checkbox or begins editing a text field.
`UI/Cancel` exits editing without clearing the field. In the absence of an
action asset, arrow keys, Enter and Escape also work; Tab/Shift+Tab traverse
the registration order. Mouse clicks select widgets as before.

The BlankGame input asset intentionally enables only `Gameplay` and `Global`
while the `Playing` context is active. When opening a menu, push the `Menu`
context (`Input->PushActionContext(0, "Menu")`) to enable the UI action map;
pop that context when closing the menu. Use `Overlay` for nonexclusive UI.
Controller navigation needs the UI map enabled, while the basic keyboard
fallback works even without an action asset.

Widgets with a stable, unique ID register themselves each UI frame; only
interactive controls (buttons, image/invisible buttons, checkboxes and
text fields) receive focus. `UIManager::SetNavigationEnabled(false)` disables
keyboard/controller traversal without disabling the mouse, and
`UIManager::SetNavigationPlayer(id)` chooses which player operates the UI.
For multiplayer menus, keep one navigation owner at a time; each menu can
choose a different owner as needed.

## Separate players, controllers and characters

A `PlayerId` names a logical slot, **not** a gamepad's list index or a
`GameObject`. One player can have keyboard, mouse and a controller at the
same time. Multiple players can deliberately share a physical device.
Multiple characters may query one logical slot.

~~~cpp
PlayerInputSettings profile;
profile.player = 0;
profile.mode = InputAssignmentMode::Prefer;
profile.idealDevice = InputDeviceKind::Gamepad;
profile.keyboard = true;
profile.mouse = true;
profile.autoJoin = true;
profile.priority = 100;
Input->ConfigurePlayer(profile);

// Optional character-level registration:
auto* hero = scene.CreateObject("Hero");
hero->AddComponent<PlayerInput>(profile);
~~~

The component temporarily applies its settings and restores a pre-existing
global profile on destruction. Only one `PlayerInput` should own a given
slot at a time. You may change its settings and call `Apply()`, but keep
the player ID fixed during the component's lifetime.

Four policies govern assignment:

| Mode | What it does |
|---|---|
| `Manual` | Only explicit calls assign devices; no auto-join |
| `Strict` | Automatic joining uses only the ideal device family |
| `Prefer` | Prefers the ideal device and permits configured fallbacks |
| `Any` | Accepts any device permitted by the scheme; chooses eligible slots by priority |

An **unassigned** SDL gamepad presses **Start** to join a configured
`autoJoin` slot. Slots with no free gamepad capacity are skipped unless
`allowMultipleGamepads` is true. Higher-priority preferred slots are tried
first. A successful Start join is consumed so that it does not trigger
Pause in the same frame. Auto-join never silently creates new players.
Keyboard and mouse ownership is assigned by profiles or the explicit APIs;
the automatic Start trigger applies to gamepads.

For `Prefer` with ideal Gamepad, `replaceOnIdeal` allows a controller to
join even if keyboard/mouse was last active; it does not forcibly remove
those other inputs. All players can configure `maps` independently.
To share a physical device, both players must have `shareDevices=true`
and explicit assignment must pass `allowShared=true`. Exclusive
keyboard/mouse overlaps are rejected instead of silently routing input
to multiple players.

## Connection, disconnection and last active device

~~~cpp
for (const GamepadEvent& event : Input->GetGamepadEvents()) {
    if (event.type == GamepadEventType::Disconnected) {
        // event.id: disconnected controller instance
        // event.player: last assigned player or -1
    }
}

InputListenerId token = Input->OnGamepadDisconnected(
    [](GamepadId id, PlayerId player) {
        // Decide whether the game should pause.
    });
// Later: Input->RemoveGamepadListener(token);
~~~

Use `GetConnectedGamepads`, `IsGamepadConnected`, `GetGamepadName`,
`GamepadDown`, `GamepadPressed`, `GamepadReleased` and `GamepadAxis`
to access a specific controller. IDs are SDL **joystick instance IDs**,
not transient device indices. Stick axes are normalized to -1..1;
triggers to 0..1. The direct axis getter has a configurable deadzone.
Controller unplugging clears its input contribution and causes dependent
actions to release/cancel. `GetLastActiveDevice(player)` preserves the last
meaningful device even after disconnect, so a game may choose to pause or
change its button prompts. **The engine never pauses the game automatically.**

Only joysticks SDL2 recognizes as `GameController` are supported without a
custom SDL controller mapping.

## JSON actions, control paths and processing

~~~json
{
  "maps": {
    "Gameplay": {
      "actions": {
        "Move": {
          "type": "Axis2D",
          "combine": "Strongest",
          "bindings": [
            "Keyboard/WASD",
            "Keyboard/Arrows",
            {"path": "Gamepad/LeftStick",
             "processors": [{"type": "Deadzone", "value": 0.2}]}
          ]
        },
        "Charge": {
          "type": "Button",
          "interaction": {"type": "Hold", "duration": 0.6},
          "bindings": ["Keyboard/Space", "Gamepad/South"]
        }
      }
    }
  },
  "contexts": {
    "Playing": {"maps": ["Gameplay"], "exclusive": true}
  }
}
~~~

Supported paths include:

- Keyboard: `Keyboard/W`, `Keyboard/Space`, `Keyboard/Left Ctrl+S`,
  `Keyboard/WASD`, `Keyboard/Arrows`, `Keyboard/Axis:A,D`
  (negative,positive), and SDL scancode names.
- Mouse: `Mouse/Left`, `Right`, `Middle`, `X1`, `X2`,
  `Delta`, `DeltaX`, `DeltaY` and `WheelY`.
- Gamepad: `South`, `East`, `West`, `North`, `Back`, `Start`,
  `Guide`, `LeftShoulder`, `RightShoulder`, `LeftStickButton`,
  `RightStickButton`, `DpadUp`, `DpadDown`, `DpadLeft`, `DpadRight`,
  `Dpad`, `LeftStick`, `RightStick`, `LeftX/Y`, `RightX/Y`
  and `LeftTrigger` / `RightTrigger` (prefix with `Gamepad/`).

Keyboard directional composites use Y=-1 for up. Combine modes:
`Strongest` (default, largest magnitude), `Latest` (last active binding
in declaration order), or `SumClamped` (adds then normalizes if length
exceeds one). Strongest avoids double-speed when keyboard and stick
are active together.

Processors: `Deadzone`, `Scale`, `Invert`, `InvertX`, `InvertY`,
`Clamp`, `Normalize`, `Curve`, `Sensitivity` and `Smooth`.
The last is action-level only, keeping per-player history with response
speed in inverse seconds. Others may be placed on bindings or actions.

Interactions: `Press` (default), `Release`, `Tap`, `SlowTap`,
`Hold`, `MultiTap` and `DoubleTap`. Hold fires once after the specified
duration; taps fire on release and multi-taps require consecutive quick
taps. Timing uses real (unscaled) SDL time, even during gameplay pause.

## Rebinding and persistent overrides

~~~cpp
Input->RebindAction("Gameplay/Jump", "Keyboard/F", 0);
Input->BeginRebind("Gameplay/Jump", 0); // Capture next SDL key/button
// Input->CancelRebind();
Input->SaveBindingOverrides("saves/controls.json");
Input->LoadBindingOverrides("saves/controls.json");
~~~

The default conflict policy rejects an already-used path in another
action; pass `false` as the last argument to permit intentional overlaps.
On a rejected capture, rebinding stays active until a valid input or
`CancelRebind`. Check `GetInputError()` for a diagnostic.
Overrides use atomic JSON persistence with potential `.bak` files; in a
shipped game choose a writable user-data path, **not** an immutable assets
folder. Malformed actions/profiles are rejected with an error message;
load them once during initialization rather than while live code holds
raw action pointers.

## Lifecycle and supported platforms

SDL events are consumed **once** by `InputManager::Update`. Focus loss
clears keyboard/mouse held state; unplugging a gamepad releases actions.
All new APIs preserve the previous keyboard/mouse polling behavior.

XEngine currently builds for **Windows and Linux**. Schemes reserve a
`touch` flag and standalone `InputAction::Feed` allows custom/synthetic
adapters, but this patch does **not** implement Android touch dispatch
or claim Android runtime compatibility.

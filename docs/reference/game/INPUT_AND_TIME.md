# Keyboard, mouse, time and randomness

@ref InputManager and @ref TimeManager are available through the convenient
`Input` and `Time` accessors when you include `<XEngine.h>`. @ref RandomManager
provides seeded sampling for procedural logic.

For local multiplayer, gamepads, action maps, processors, rebinding, JSON
assets and the optional PlayerInput character component, see the
[PlayerInput and Input Actions guide](INPUT_ACTIONS.md). All classic calls
below remain supported alongside the new system.

## Input edges versus held state

```cpp
#include <XEngine.h>

void Update(float dt) override {
    (void)dt;
    if (Input->KeyDown(Key::W)) {
        // Continuous movement while W is held.
    }
    if (Input->KeyPressed(Key::SPACE)) {
        // A one-frame jump/action event.
    }
}
```

These shortcuts are C++20 typed accessors, not macros. You can keep writing
`Input->...` or `Time->...`; to store a non-owning pointer temporarily, use
`auto* input = Input.get();`. The manager must be initialized before use.

Use `KeyReleased` for a key-up edge. The corresponding pointer APIs are
`MouseDown`, `MousePressed` and `MouseReleased`; `GetMousePos()` returns
logical screen coordinates. `GetMousePosWorld()` uses the current camera.
Mouse wheel and relative movement have separate getters.

Text entry is a different input mode: `SetTextInput(true)` starts collecting
typed UTF-8 input; call `GetTextInput()` to inspect the accumulated string
and `ClearTextInput()` after consuming it. Do not treat raw key codes as
text characters.

For game menus, prefer `UIManager::TextInput`: it automatically controls the
SDL text input session while editing a caller-owned `std::string`, supports
UTF-8 backspace/delete and keyboard/controller focus traversal, and avoids
manually consuming the frame text buffer in game code. Explicit
`InputManager::SetTextInput` remains useful for custom text editors.

## Frame time and simulation time

`Behaviour::Update(dt)` receives frame time, and `FixedUpdate(dt)` receives
the fixed simulation step. You may inspect:

```cpp
float frameSeconds = Time->deltaTime.get();
float fixedSeconds = Time->fixedDeltaTime.get();
double elapsed = Time->timeSinceStart.get();
Time->timeScale = 0.f; // Pause scaled gameplay time.
```

`highResTimeStamp` is for instrumentation; it is not a saved-game clock.
Use fixed-step updates for movement coupled to physics, and frame updates
for presentation and ordinary per-frame input.

## Repeatable random sequences

```cpp
Random->SetSeed(1234);
int count = Random->Range(0, 10);    // 0..9
float chance = Random->Value();      // [0, 1)
Vec2 offset = Random->InsideUnitCircle();
```

Set a seed explicitly when a level or test should reproduce its generation.
Random streams are not cryptographically secure and should not be used for
anti-cheat signatures or secret keys.

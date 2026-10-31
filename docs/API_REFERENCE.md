# XEngine — Public C++ API

The reference is generated from the **public C++20 headers** of XEngine. It is
designed for people building gameplay code, not for modifying renderer internals.
The original declarations are the source of truth; examples in this guide use
the regular Windows/Linux builds, with SDL2 and `nlohmann/json` installed.

**Search the live API:** [Game Developer Reference](https://the-33.github.io/XEngine/game/html/index.html)
for supported gameplay code, or [Engine Developer Reference](https://the-33.github.io/XEngine/engine/html/index.html)
for implementation and internal APIs. Both are available from the
[documentation portal](https://the-33.github.io/XEngine/).

If you are starting a game, first follow [Your first game](GETTING_STARTED.md).
For an overview organized by task, see the
documentation index (repository reference: `docs/DOCUMENTATION_INDEX.md`).

## Start here

1. Include `<XEngine.h>` for core components, managers and typed gameplay
   shortcuts, with no namespace qualifier required.
2. Include `<XEngine/Storage.h>` for filesystem and atomic writes,
   `<XEngine/BinaryIO.h>` for bounded low-level binary I/O, or
   `<XEngine/Serialization.h>` for JSON/binary object archives. They remain
   opt-in to avoid dragging their dependencies into every game file.
3. Register scenes and create `GameObject` instances in your game's initialization
   callback. Attach `Behaviour` subclasses for gameplay. The
   [getting-started guide](GETTING_STARTED.md) shows the actual BlankGame layout,
   and [headers and includes](INCLUDES_AND_DEPENDENCIES.md) explains which
   classes to include and where.
4. Call engine/SDL window and rendering APIs on the main thread. Save data may be
   written through `FileSystem` or `AtomicFile`; each subsystem's constraints
   are documented on the corresponding declarations.

## API areas

| Area | Main entry points | Purpose |
|---|---|---|
| Engine startup | @ref Engine, @ref SceneManager, @ref Scene | Start/stop; scene builders and lifetime. |
| Game objects | @ref GameObject, @ref Component, @ref Behaviour, @ref Transform | Entities, components, script callbacks, transforms. |
| Drawing & camera | @ref RenderManager, @ref SpriteRenderer, @ref Camera2D, @ref WindowManager | Draw, sprites, view mapping and window state. |
| Collision & movement | @ref Collider2D, @ref RigidBody2D, @ref CollisionManager, @ref PhysicsManager | Contacts, triggers and fixed-step physics. |
| Input & time | @ref InputManager, @ref InputAction, @ref PlayerInput, @ref TimeManager, @ref RandomManager | Keyboard, mouse, SDL gamepads, player actions, time and random numbers. |
| Resources & sound | @ref AssetManager, @ref Texture, @ref Font, @ref SoundManager | Load cached assets, render text and play audio. |
| Immediate UI | @ref UIManager, @ref UIButtonColors | Buttons, labels, panels and progress bars. |
| Persistent storage | @ref FileSystem, @ref AtomicFile, @ref PlayerPrefs | Raw files, candidate recovery and small preferences. |
| Data conversion | @ref BinaryWriter, @ref BinaryReader, @ref BinarySerializer, @ref JsonSerializer | Binary/JSON values, containers, registered classes. |
| Types & properties | @ref TVec2, @ref TVec3, @ref TRect, @ref Color, @ref Property, @ref PropertyRO | Math and Unity-like property syntax. |

You can navigate by **Classes**, **Files**, **Functions** and **Modules** in
the generated navigation panel, or use the built-in search box.

## `Property` and `PropertyRO` conventions

Engine object state is typically exposed as a `Property<Owner, T, Getter, Setter>`
or a `PropertyRO<Owner, T, Getter>`, rather than as a writable public backing field.

```cpp
auto* player = /* your GameObject* */;
player->name = std::string("Ada");   // writable Property
std::string name = player->name.get();
Vec3 position = player->transform->position.get();
player->transform->position = Vec3{4.f, 2.f, 0.f};
// player->activeInHierarchy is read-only; use the supported state-change API.
```

`Property` stores an owner pointer; it is not a free-standing value. Do not copy
or serialize the property wrapper itself. Serialize the underlying `.get()` value
when needed. Public aggregate types such as colors, vectors, collision event
snapshots and drawing options intentionally expose **plain data fields**: they
are value objects, not mutable manager state.

## Serialization: explicit fields, arbitrary formats

```cpp
#include <XEngine/Serialization.h>

struct Wallet {
    int coins = 0;
    int gems = 0;
    XENGINE_SERIALIZABLE(coins, gems)
};

auto bytes = BinarySerializer::Serialize(Wallet{5, 2});
auto state = BinarySerializer::Deserialize<Wallet>(bytes);
auto text = JsonSerializer::Serialize(state);
```

The engine never imposes a save version, magic signature, checksum or filename
extension. See [Persistence and serialization](PERSISTENCE_AND_SERIALIZATION.md)
for the binary layout, backup/temporary recovery policy and PlayerPrefs paths.

## Related developer guides

- [Your first game](GETTING_STARTED.md) explains project creation, scenes and
  gameplay scripts.
- [Headers and includes](INCLUDES_AND_DEPENDENCIES.md) explains the umbrella
  header, focused includes and C++ organization.
- [Window, camera and UI](WINDOW_AND_CAMERA.md) explains size/fullscreen
  properties, world/screen coordinates and responsive layout.
- [Persistence and serialization](PERSISTENCE_AND_SERIALIZATION.md) explains
  raw files, recovery choices, binary/JSON formats and player preferences.
- [Input Actions](reference/game/INPUT_ACTIONS.md) covers gamepads, local
  players, action maps, JSON, processing, callbacks and control rebinding.

## Build this reference locally

The published [XEngine documentation](https://the-33.github.io/XEngine/) is
available without Doxygen. To create and validate an offline copy, run from
the XEngine project root:

```bash
python tools/build_doxygen.py
python tools/check_doxygen_docs.py
```

Open `build/docs/index.html` in a browser: the **Game Developer** profile
documents supported gameplay API only, and the **Engine Developer** profile
includes internals and source implementations. Both group Markdown guides
under a single expandable **Guides and manuals** tree and provide light/dark
themes. Generation also produces machine-readable XML under the corresponding
`build/docs/game/xml/` and `build/docs/engine/xml/` directories; both outputs are generated
and are intentionally excluded from source packages. Doxygen is an **optional
developer documentation tool**, not a compiler/runtime dependency of XEngine.

The normal XEngine build automatically runs Doxygen **only after the engine
library is rebuilt**. If Doxygen is not installed, the build issues a helpful
warning and continues. When you change only game code or reuse the cached
engine library, no documentation rebuild is needed.
If you do not need local documentation, pass `--no-doc` to either the engine
or game build (e.g. `python tools/build.py engine --no-doc`). That skips
Doxygen generation even on a rebuild and makes the optional VS Code hover
extension prefer the online API until a successful build without the flag.
See [one-time contributor setup](DEVELOPER_GUIDE.md#first-checkout-optional-contributor-tools)
for Git hooks and editor integration; neither is required for builds.

`check_doxygen_docs.py` checks each public/protected declaration and every enum
value in the generated XML for a real brief or detailed description. It rejects
missing descriptions, stale/incomplete index references and an empty API. Keep
the generated XML in `build/`; neither HTML nor XML belongs in a source patch.

The same command works in Windows (MSYS2 or CMD) and Linux when Doxygen is in
the `PATH`. For stricter API coverage checks, run the documentation tests in
`tests/` and inspect Doxygen warnings before publishing.

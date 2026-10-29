# XEngine — Game Developer Reference {#xengine_game_reference}

Welcome to the **gameplay API** for XEngine. Everything in this reference is
intended to help you build games with the public C++20 headers. Engine backing
fields, private implementation classes and the property binding typedefs are
intentionally outside this view. Use the separate Engine Developer Reference
when you are modifying XEngine itself.

## Start building

1. Create a project with `new_project.bat "My Game"` on Windows or
   `./new_project.sh "My Game"` on Linux, from the engine root.
2. Open `games/My Game/assets/scenes/StartScene.hpp` to create your scene and
   `games/My Game/assets/scripts/` to add gameplay behaviours.
3. Build from the game directory with `build.bat --run` or `./build.sh --run`.
4. Include `<XEngine.h>` for the broad API, or include only the headers used by
   each file. For JSON and registered classes, also include
   `<JsonSerializer.h>` or `<Serialization.h>` explicitly.

**Optional first-time contributor setup:** the
[Developer Guide](../../DEVELOPER_GUIDE.md#first-checkout-optional-contributor-tools)
explains how to activate the per-clone Git pre-commit hook for tracked games
and Doxygen checks, configure VS Code IntelliSense for your games and install
the optional Doxygen Hover extension. You do not need any of these tools to
build or run a game. You can browse this **published reference** without
installing Doxygen; to skip regeneration on engine builds, use `--no-doc` (for
example `./build.sh --run --no-doc` inside your game on Linux).

When contributing engine code, follow the
[testing guide](../../TESTING.md) to compile and execute real SDL2 smoke
tests on Windows and Linux in Debug/Release. These checks are separate from
the optional Git hooks and from Doxygen generation.

## Features by task

| Feature | Read first | Important API |
|---|---|---|
| Create levels and player behaviour | [Scenes and scripts](SCENES_AND_BEHAVIOURS.md) | @ref SceneManager, @ref Scene, @ref GameObject, @ref Behaviour |
| Keyboard, pointer, time and randomness | [Input and time](INPUT_AND_TIME.md) | @ref InputManager, @ref TimeManager, @ref RandomManager |
| Sprites, window resolution and UI | [Rendering and UI](RENDERING_AND_UI.md) | @ref SpriteRenderer, @ref WindowManager, @ref Camera2D, @ref UIManager |
| Movement, bodies and triggers | [Physics and collisions](PHYSICS_AND_COLLISIONS.md) | @ref RigidBody2D, @ref Collider2D, @ref PhysicsManager |
| Load images, fonts and audio | [Assets and sound](ASSETS_AND_AUDIO.md) | @ref AssetManager, @ref SoundManager |
| Save progress, preferences, JSON and binary | [Data and persistence](DATA_AND_PERSISTENCE.md) | @ref FileSystem, @ref AtomicFile, @ref JsonSerializer, @ref BinaryWriter |
| Build, includes, profiles and shipping | [Projects and includes](PROJECTS_AND_INCLUDES.md) | @ref Engine and public headers |
| Platform-specific code and Debug/Release switches | [Platforms and conditional compilation](PLATFORMS_AND_BUILD_MODES.md) | `XENGINE_PLATFORM_*`, `XENGINE_DEBUG`, `XENGINE_RELEASE`, `XEngineBuild::IsDebug` |
| Coordinates and useful data types | [Math and utilities](MATH_AND_UTILITIES.md) | @ref TVec2, @ref TVec3, @ref TRect, @ref Property, @ref PropertyRO |

Each feature page explains common decisions and includes runnable-looking
usage examples. The **Classes**, **Files** and **Modules** tabs provide exact
signatures and enum values; use Search to find a method or property.

The reference also contains a curated set of complete manuals for game
developers: the [Developer Guide](../../DEVELOPER_GUIDE.md),
[cross-compilation](../../CROSS_COMPILATION.md), game configuration, release
packaging, [persistence](../../PERSISTENCE_AND_SERIALIZATION.md) and window
management. Find them under **Guides and manuals** in the navigation tree.

The engine's official release is read from the root `VERSION` file. Your
game's `project.version` is independent: read
[XEngine versions and compatibility](../../VERSIONING.md) when deciding
whether an engine update requires game code changes.

### Understanding the Files tab

**File Members** lists only free functions, global variables, type aliases,
macros and other declarations at file scope. Fields and properties belonging
to `GameObject`, `Transform`, `RigidBody2D` or another class appear under the
**Classes** reference, not under the File Members → Variables list. The game
API can legitimately have no exported file-scope variables. This is not a
documentation coverage check; for APIs use Classes, Files and Search.

## Public properties are the interface

Game code interacts with state using the public property object, not the
manager's private backing field or its property-adapter type alias:

```cpp
Vec3 position = player->transform->position.get();
player->transform->position = Vec3{4.f, 2.f, 0.f};

Window->size = Vec2I{1280, 720};
Vec2I drawablePixels = Window->drawableSize.get(); // read-only
```

Read-only `PropertyRO` properties do not allow assignment. Many mathematical
value types (`Vec2`, `Vec3`, `Rect`, `Color`) intentionally have plain fields;
their fields are **data**, not engine manager state.

## Ownership and platform limits

- Objects and components created in a `Scene` are owned by the scene. Do not
  delete them or cache pointers past a scene transition.
- SDL window/render APIs should be called from the main game thread.
- User save files belong in the per-user persistence directory, not under
  `assets/`; their format and extension are the game's choice.
- The supported desktop targets are Windows and Linux. Compile and run on
  both targets before shipping.

For engine internals, build the **Engine Developer Reference** and open
`build/docs/engine/html/index.html`. The documentation chooser is
`build/docs/index.html` when both profiles have been generated.

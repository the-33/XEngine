# Headers, includes and game code organization

XEngine is a C++20 source-based engine. Game scripts are compiled C++ code,
not dynamically interpreted files. Your game uses public headers from
`engine/include/`, and the build system links the compiled engine library.

If you have not created a project yet, start with
[your first XEngine game](GETTING_STARTED.md).

## Which engine headers should I include?

You have two valid styles:

```cpp
#include <XEngine.h> // Broad public engine entry point
```

or only the functionality you need:

```cpp
#include <Behaviour.h>
#include <GameObject.h>
#include <Transform.h>
#include <Scene.h>
```

`XEngine.h` includes the public core types, components, managers and window
APIs. It is convenient in a game's `InitCallback.hpp` or a small prototype.
Individual includes can reduce dependencies and compile time in larger
projects. Both forms use the *same* engine library and objects.

The following headers are intentionally **opt-in**:

```cpp
#include <JsonSerializer.h>  // JSON text and optional file-saving facade
#include <Serialization.h>   // Registered-class JSON/binary archives
```

They bring in nlohmann/json and template-heavy serialization code. Include
them in translation units that serialize game state; do not assume they are
transitively available through `XEngine.h`.

`<FileSystem.h>`, `<AtomicFile.h>`, `<BinaryIO.h>` and `<PlayerPrefs.h>` are
included by the umbrella header but can also be included individually.

## Header choices by task

| You want to... | Start with |
|---|---|
| Register and build scenes | `<SceneManager.h>`, `<Scene.h>`, `<GameObject.h>` |
| Write a gameplay script | `<Behaviour.h>`, `<GameObject.h>`, `<Transform.h>` |
| Use platform-specific code or Debug/Release flags | `<Platform.h>`, `<BuildConfig.h>` |
| Add sprite visuals | `<SpriteRenderer.h>`, `<AssetManager.h>`, `<Assets.h>` |
| Work with colliders and forces | `<Collider2D.h>`, `<RigidBody2D.h>`, `<PhysicsManager.h>` |
| Handle input and time | `<InputManager.h>`, `<KeyCodes.h>`, `<TimeManager.h>` |
| Draw custom graphics and UI | `<RenderManager.h>`, `<UIManager.h>`, `<Camera2D.h>` |
| Resize the window | `<WindowManager.h>` |
| Load texture, font or sound | `<AssetManager.h>`, `<SoundManager.h>` |
| Read or write any file | `<FileSystem.h>` |
| Publish a single file robustly | `<AtomicFile.h>` |
| Encode raw binary primitives and bits | `<BinaryIO.h>` |
| Serialize registered fields to JSON/binary | `<Serialization.h>`, `<JsonSerializer.h>` |
| Store small settings | `<PlayerPrefs.h>` |

These names are the actual public header filenames. Internal renderer headers,
private manager fields and `engine/src/*.cpp` are not gameplay extension APIs.
Consult [Doxygen](API_REFERENCE.md) for available overloads and constraints.
See [platform macros and build modes](reference/game/PLATFORMS_AND_BUILD_MODES.md)
for a complete reference to the public conditional-compilation flags.

## How include paths are resolved

The supplied build driver adds the following locations to the C++ compiler:

1. `engine/include/` for `<Behaviour.h>`, `<XEngine.h>`, etc.
2. `games/<Game>/assets/` and its header-containing subdirectories for
   `"scenes/StartScene.hpp"`, `"gameobjects/ExampleObject.hpp"`, or a short
   `"ExampleObject.hpp"` from its configured include directory.
3. `games/<Game>/build`-associated generated headers, such as
   `"GameBuild.generated.h"`, through the per-game generated include directory.
4. Your source/header directory via ordinary C++ quoted-include resolution.

Prefer **stable asset-relative paths** in game headers if ambiguity is possible:

```cpp
#include "scripts/DriftRight.hpp"
#include "gameobjects/PlayerFactory.hpp"
```

The shipped BlankGame template uses short header names too; its build flags
explicitly support them. Never hardcode paths to an installed SDK or your own
machine's `engine/include` directory inside source files.

## Separate classes from scene assembly

A maintainable layout has three responsibilities:

- `assets/scripts/`: classes derived from `Behaviour`. Keep reusable behavior
  here, not in `main.cpp`.
- `assets/gameobjects/`: factory functions creating an entity and attaching its
  components. They return a **non-owning** pointer managed by the scene.
- `assets/scenes/`: scene-builder functions that invoke those factories and
  configure scene-specific entities.

An ordinary object factory might look like this:

```cpp
#pragma once
#include <Scene.h>
#include <GameObject.h>
#include "scripts/DriftRight.hpp"

inline GameObject* CreatePlayer(Scene& scene)
{
    GameObject* player = scene.CreateObject("Player");
    player->AddComponent<DriftRight>();
    return player;
}
```

Scene functions can call the factory instead of duplicating the component
setup in each level. Do not manually `delete` a returned `GameObject*`.

## Header-only functions and multiple `.cpp` files

The starter project places small implementations in `.hpp` files for
convenience. This is valid C++ if you follow the One Definition Rule:

- Put `#pragma once` in each header.
- Methods defined **inside** a class definition are implicitly inline.
- Free functions or out-of-class method implementations defined **in a header**
  should be marked `inline` (as in the BlankGame template).
- For larger codebases, declare methods in `.hpp` and implement them once in
  your own `.cpp`. The build driver compiles `.cpp` sources from the supported
  game source and asset folders.

Avoid circular includes. If a header only stores `GameObject*`, forward-declare
`class GameObject;` there; put `<GameObject.h>` in the `.cpp` or header location
where you actually call its methods. `#pragma once` prevents repeated inclusion
but does **not** solve circular type dependencies by itself.

## Properties are accessors, not plain fields

```cpp
#include <GameObject.h>
#include <Transform.h>

GameObject* object = /* scene-owned entity */;
object->name = std::string("Hero");              // writable Property
std::string currentName = object->name.get();      // read current value
Transform* transform = object->transform.get();    // read-only PropertyRO
transform->position = Vec3{2.f, 3.f, 0.f};
```

Do not copy the property wrapper or persist it directly. Read its `.get()`
value for serialization; assign through the property or the explicitly
documented setter. Public *value types* such as vectors and colors can still
have ordinary fields because they represent data rather than manager state.

## Generated and runtime files are different

- **Generated build headers** (`GameBuild.generated.h`) are managed by the
  build driver. Do not edit them directly.
- **Runtime assets** (`.png`, `.ttf`, audio, engine JSON) are loaded while your
  game runs and are placed in the Release package.
- **C++ gameplay headers** (`.hpp`) are compiled into the executable and are
  not interpreted at runtime.
- **Persistent user files** belong in per-user storage; use `FileSystem` or
  `AtomicFile`, not the assets directory. See
  [persistence and serialization](PERSISTENCE_AND_SERIALIZATION.md).

For installation, build flags and packaging, consult the
[Developer Guide](DEVELOPER_GUIDE.md). For window/UI coordinates, see
[Window, camera and UI](WINDOW_AND_CAMERA.md).

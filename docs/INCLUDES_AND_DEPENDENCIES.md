# Headers, includes and game code organization

XEngine is a C++20 source-based engine. Game scripts are compiled C++ code,
not dynamically interpreted files. Your game uses public headers from
`engine/include/`, and the build system links the compiled engine library.

If you have not created a project yet, start with
[your first XEngine game](GETTING_STARTED.md).
You can work with these headers without VS Code or locally generated Doxygen.
If you prefer editor completion and clickable API links, the one-time setup
is covered in [VS Code integration](VSCODE_INTEGRATION.md); for Git hooks and
online-only builds (`--no-doc`), follow the
[first-checkout guide](DEVELOPER_GUIDE.md#first-checkout-optional-contributor-tools).

## Which engine headers should I include?

For ordinary gameplay there is just **one recommended starting point**:

```cpp
#include <XEngine.h> // Scene, GameObject, Behaviour, Transform, rendering, etc.
```

All common gameplay classes remain in the global namespace: there is no
`using namespace xengine;` requirement. To minimize includes in large games,
you may also include individual public headers:

```cpp
#include <Behaviour.h>
#include <GameObject.h>
#include <Transform.h>
#include <Scene.h>
```

`XEngine.h` provides core values, components, scenes, managers, camera,
window, physics, sound, resources, UI and the small `PlayerPrefs` settings
API. The more specialized file/serialization/diagnostic APIs are opt-in.
Responsive UI anchors (`UIRectTransform`, `UICanvas`, `UICanvasScaleMode`) are
also available directly from `<XEngine.h>`; a minimal include is
`<UIRectTransform.h>`. They do not require an additional namespace or library.
Individual headers can still reduce compile time in larger projects.
All forms use the *same* engine library and objects.

## Optional modules

```cpp
#include <XEngine/Storage.h>       // FileSystem, AtomicFile
#include <XEngine/BinaryIO.h>      // BinaryWriter, BinaryReader, bounded bit I/O
#include <XEngine/Serialization.h> // JSON and binary object serialization
#include <XEngine/Diagnostics.h>   // RenderDiagnostics CPU/selector helpers
#include <XEngine/Platform.h>      // BuildConfig and platform defines
```

These modules are independent headers, not separate libraries. The serialization
module intentionally brings in nlohmann/json and template-heavy code; include
it only in translation units that save or load registered game classes.

The old direct includes (`<FileSystem.h>`, `<AtomicFile.h>`,
`<BinaryIO.h>`, `<JsonSerializer.h>`, `<Serialization.h>`, etc.) remain valid,
so existing game projects do not need an include migration. Module aliases also
offer explicit names for specialized systems:

```cpp
#include <XEngine/Storage.h>
#include <XEngine/BinaryIO.h>

auto path = xengine::io::FileSystem::GetPersistentDataPath("studio", "game");
xengine::binary::BinaryWriter writer;
```

For these modules, `xengine::io::FileSystem` and the historical global
`FileSystem` denote the **same type**. There is no duplicated implementation.

There is no separate public formatter subsystem yet; we will only add a
`XEngine/Formatting.h` module when actual formatting utilities need one.

## Manager shortcuts are typed, not macros

```cpp
#include <XEngine.h>

void UpdateGameplay() {
    if (Input->KeyDown(Key::SPACE)) {
        // Existing Input->... / Time->... syntax is unchanged.
    }
    InputManager* input = Input.get(); // Explicit pointer when storing it
    float fixedSeconds = FixedDT;       // Read-only live value
}
```

`Input`, `Time`, `Assets`, `Sound`, `Scenes`, `Renderer`, `UI`, `Window`,
`Physics` and `Random` are lightweight, non-owning objects that resolve
their manager at access time. Unlike the old `#define` shortcuts they do not
rewrite tokens in unrelated user declarations. `CurrentCamera` and
`FixedDT` are similarly read-only live accessors. Use shortcuts only while
the engine is initialized, and never retain manager/camera pointers across a
shutdown or scene change.

To preserve correct C++ type deduction, use `auto* input = Input.get();` (not
`auto* input = Input;`). The accessor supports implicit conversion to the
actual manager pointer when a concrete pointer type is specified.

### Engine implementation headers

`XEngine/detail/*` is reserved for internal renderer, broadphase and worker
infrastructure. The old direct internal header filenames forward to their
new locations during the compatibility period. They are **not** a supported
game-extension interface and may change without a public API guarantee.

## Header choices by task

| You want to... | Start with |
|---|---|
| Register and build scenes | `<SceneManager.h>`, `<Scene.h>`, `<GameObject.h>` |
| Write a gameplay script | `<Behaviour.h>`, `<GameObject.h>`, `<Transform.h>` |
| Use platform-specific code or Debug/Release flags | `<XEngine/Platform.h>` |
| Add sprite visuals | `<SpriteRenderer.h>`, `<AssetManager.h>`, `<Assets.h>` |
| Work with colliders and forces | `<Collider2D.h>`, `<RigidBody2D.h>`, `<PhysicsManager.h>` |
| Handle input and time | `<InputManager.h>`, `<KeyCodes.h>`, `<TimeManager.h>` |
| Draw custom graphics and UI | `<RenderManager.h>`, `<UIManager.h>`, `<Camera2D.h>` |
| Anchor and scale responsive UI widgets | `<UIRectTransform.h>` or `<XEngine.h>` |
| Resize the window | `<WindowManager.h>` |
| Load texture, font or sound | `<AssetManager.h>`, `<SoundManager.h>` |
| Read or write any file | `<XEngine/Storage.h>` |
| Publish a single file robustly | `<XEngine/Storage.h>` |
| Encode raw binary primitives and bits | `<XEngine/BinaryIO.h>` |
| Serialize registered fields to JSON/binary | `<XEngine/Serialization.h>` |
| Diagnose render/selector behavior | `<XEngine/Diagnostics.h>` |
| Store small settings | `<XEngine.h>` (`PlayerPrefs`) |

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

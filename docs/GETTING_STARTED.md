# Your first game with XEngine

This guide takes you from a fresh engine checkout to a small, moving object.
You will work in **your game's source files**; you do not need to edit XEngine's
`engine/` directory. XEngine uses C++20, SDL2 and a scene/component model.
The supported desktop platforms are Windows and Linux.

For a map of the rest of the manuals, see the
documentation index (repository reference: `docs/DOCUMENTATION_INDEX.md`). For exact C++ signatures, use
the [Doxygen API reference](API_REFERENCE.md).

## 1. Install the development dependencies

You need a C++20 compiler, Python 3.10+, SDL2, SDL2_image, SDL2_mixer,
SDL2_ttf, `pkg-config`/`pkgconf` and `nlohmann/json` headers.

- **Windows:** use the MSYS2 **UCRT64** toolchain and packages from the
  [Windows installation instructions](DEVELOPER_GUIDE.md#windows-1011-x86_64--msys2-ucrt64).
- **Linux (Ubuntu/Debian):** install the packages in the
  [Linux installation instructions](DEVELOPER_GUIDE.md#linux-x86_64--ubuntu--debian).

Check your installation from the engine root:

```bash
# Linux
./check_dependencies.sh
```

```bat
REM Windows CMD
check_dependencies.bat
```

You do not need Doxygen to compile games or read the published API. It is useful
only when you want to generate documentation for local engine changes; see
[API documentation](API_REFERENCE.md).

### Optional setup for contributors and VS Code users

From the XEngine repository root, you can enable these **independent,
optional** integrations once:

```bash
# Only if you contribute with Git: local pre-commit hooks for game tracking
# and documentation checks on staged engine/docs changes.
git config --local core.hooksPath .githooks

# Only if you use VS Code: C++ IntelliSense for the engine and your games.
python tools/vscode.py setup

# Optional clickable documentation links in C++ hovers.
python tools/vscode.py hover-install
```

No Git hook or VS Code extension is automatically installed by XEngine.
The pre-commit hook can stage changes to games you have opted into Git;
always review the staged files before committing. See the
[Git hook and game tracking guide](GAME_GIT_TRACKING.md) and the
[VS Code integration guide](VSCODE_INTEGRATION.md).

**Prefer using the online documentation?** You can always read
[the published XEngine API](https://the-33.github.io/XEngine/) without
installing Doxygen. Add `--no-doc` to a build, for example
`python tools/build.py engine --no-doc` from the root or `./build.sh --run --no-doc`
from a Linux game directory (`build.bat --run --no-doc` on Windows). That build
will skip local Doxygen generation, and the optional hover extension will use
online documentation even when old generated HTML exists. A successful build
without `--no-doc` restores local-first lookup. Doxygen remains useful if
you actively edit the engine and want to inspect new or unpublished APIs.

## 2. Create and run a game

Run the project generator from **XEngine's root**, not from inside `tools/`.
Quote names containing spaces.

```bash
# Linux
./new_project.sh "My Game"
cd "games/My Game"
./build.sh --run
```

```bat
REM Windows CMD
new_project.bat "My Game"
cd "games\My Game"
build.bat --run
```

The generator copies the maintained BlankGame template into your game. The
first build compiles XEngine, compiles your game and starts its Debug executable.
Later builds reuse unchanged engine and game object files. If a build fails,
read the compiler error rather than editing generated files.

## 3. Know what each folder is for

Your generated `games/My Game/` project contains:

| Path | What you edit |
|---|---|
| `src/main.cpp` | Engine startup and working-directory setup; normally leave the template logic in place. |
| `src/InitCallback.hpp` | Load shared resources and register the first scene. |
| `assets/scenes/` | Functions that build a scene and populate its entities. |
| `assets/gameobjects/` | Reusable factory functions that configure objects and components. |
| `assets/scripts/` | Your gameplay classes, usually derived from `Behaviour`. |
| `assets/textures/`, `assets/fonts/`, `assets/audio/` | Runtime resources loaded by the game. |
| `assets/config/engine.json` | Runtime options such as window size, physics, audio and renderer. |
| `config.json` | Build, platform, project name, version and packaging choices. |

`engine/include/` contains the public engine API; `engine/src/` contains its
implementation. Your code can include public engine headers without copying
them into your game. See [includes and dependencies](INCLUDES_AND_DEPENDENCIES.md).

## 4. Understand the startup sequence

The generated `src/main.cpp` takes care of the working directory, starts
XEngine using the runtime configuration, registers your initialization callback
and enters `Engine::Run()`.

The default `src/InitCallback.hpp` registers a scene:

```cpp
#pragma once
#include <XEngine.h>
#include "StartScene.hpp"

inline void InitGame(Engine& engine)
{
    (void)engine;
    Scenes->Register("StartScene", BuildStartScene);
    Scenes->SetActive("StartScene");
}
```

The scene-builder function receives an owning `Scene*` and creates entities.
XEngine handles the scene's component lifetime, updates and rendering. Do not
allocate a `Scene` manually or retain raw pointers to destroyed objects.

## 5. Make your first gameplay component

Create `assets/scripts/DriftRight.hpp`:

```cpp
#pragma once
#include <Behaviour.h>
#include <GameObject.h>
#include <Transform.h>

class DriftRight final : public Behaviour
{
public:
    void Update(float dt) override
    {
        if (auto* tr = gameObject->GetComponent<Transform>())
        {
            Vec3 p = tr->position.get();
            p.x += 2.f * dt; // world units per second
            tr->position = p;
        }
    }
};
```

Then edit `assets/scenes/StartScene.hpp` to create an object with that script:

```cpp
#pragma once
#include <Scene.h>
#include <GameObject.h>
#include "DriftRight.hpp"

inline void BuildStartScene(Scene* scene)
{
    if (!scene) return;
    GameObject* player = scene->CreateObject("Player");
    player->AddComponent<DriftRight>();
}
```

This minimal object updates its position but has **no visible sprite** yet.
To make it visible, load a `Texture` with `AssetManager` and attach/configure
a `SpriteRenderer`; see its class entry in the Doxygen API and the examples in
the BlankGame template. Avoid interpreting a moving `Transform` alone as a
rendered sprite.

Rebuild the game from `games/My Game/`:

```bash
./build.sh --run
```

On Windows, use `build.bat --run` instead. The engine's library stays cached
when you change only this gameplay header; the game recompiles as needed.

## 6. Know the component lifecycle

Override these optional hooks in your `Behaviour` subclass:

| Hook | Typical use |
|---|---|
| `Awake()` | Initialize local component state after attachment. |
| `Start()` | Set up state before the first gameplay update. |
| `Update(float dt)` | Frame-based input, movement and non-physics gameplay. `dt` is seconds. |
| `FixedUpdate(float dt)` | Physics-related work at the fixed simulation step. |
| `Render()` | Per-frame custom rendering or immediate-mode UI. |
| `OnEnable()` / `OnDisable()` | React to activation and deactivation. |
| `OnCollisionEnter/Stay/Exit` | Handle physical contacts. |
| `OnTriggerEnter/Stay/Exit` | Handle overlap events from trigger colliders. |
| `OnDestroy()` | Release or unregister non-owned resources. |

The engine invokes these through the scene/component system. You do not call
`Update()` manually. Use `GameObject::Destroy()` for deferred removal and avoid
holding pointers to a scene's entities after scene transitions.

## 7. Assets, window and save data

- **Assets:** Place a texture under `assets/textures/` and load it after startup,
  for example `Assets->LoadTexture("./textures/player.png")`. Runtime paths are
  relative to the assets directory in Debug and to the packaged executable's
  runtime directory in Release.
- **Window and UI:** Use `Window->SetSize(...)`, fullscreen properties and
  `Window->resizeRevision` as described in [window, camera and UI](WINDOW_AND_CAMERA.md).
- **Save games:** Use [persistence and serialization](PERSISTENCE_AND_SERIALIZATION.md).
  Store saves in the per-user data directory, **not** under `assets/`.
- **Preferences:** `PlayerPrefs` is for small user settings, not full save slots.

## 8. Build a Release

```bash
# Linux, from games/My Game/
./build.sh --config Release
```

```bat
REM Windows CMD, from games\My Game\
build.bat --config Release
```

Release includes the executable and its runtime assets; your C++ scripts and
scene headers are compiled into the binary and are not shipped as editable
files. Test the resulting package on the target operating system. See the
[distribution guide](DEVELOPER_GUIDE.md#release-distribution).

## Next steps

1. [Includes and project structure](INCLUDES_AND_DEPENDENCIES.md): header
   choices, reusable classes and avoiding circular dependencies.
2. [Window, camera and UI](WINDOW_AND_CAMERA.md): resolutions, coordinate
   spaces, screen layout and aspect ratio.
3. [Persistence and serialization](PERSISTENCE_AND_SERIALIZATION.md): direct
   files, robust saves, JSON and binary data.
4. [Developer guide](DEVELOPER_GUIDE.md): dependencies, configuration and
   distribution.
5. [Doxygen API reference](API_REFERENCE.md): exact public types and signatures.

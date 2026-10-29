# Project layout, public includes and builds

XEngine game code is C++20 compiled against the engine's static library.
There is no scripting VM: your classes and headers are compiled with the
rest of the game, and the runtime loads assets and configuration separately.

## BlankGame layout

```text
games/My Game/
  new_asset.sh / new_asset.bat Generate C++ gameplay files (also interactive)
  src/main.cpp                Engine startup and InitGame callback
  src/InitCallback.hpp        Scene registration and shared initialization
  assets/scenes/              Scene-builder functions
  assets/gameobjects/         Reusable entity factories
  assets/scripts/             Behaviours and gameplay classes
  assets/textures/            Textures, fonts, music and other assets
  assets/config/engine.json   Runtime settings
  config.json                 Build and package settings
```

The game-local asset generator creates files in `assets/` by default, with an
optional folder selected by `--dir`:

```bash
./new_asset.sh behaviour Mover --dir scripts
./new_asset.sh object Ball --sprite circle --collider circle --behaviour Mover --dir gameobjects
./new_asset.sh scene Arena --dir scenes
```

On Windows use `new_asset.bat` instead. Alternatively, from the repository
root run `python tools/new_game_asset.py --game "My Game" object Ball`.
`--list-sprites` reports the PNGs provided by the canonical BlankGame
template. If the selected PNG has been removed from the game's assets, it is
copied back from the template without replacing a custom texture. The source
templates for newly generated code are located in
`tools/templates/AssetScaffolds/`, not the game's `assets/`; scene registration
and attaching generated objects remain explicit steps in your game code.

The build driver adds `engine/include/` and the game's asset header folders
to the compiler include path. Start with `<XEngine.h>` for gameplay: it
provides scenes, objects, components, managers and small `PlayerPrefs`
settings without requiring a namespace. Import specialized modules separately:

```cpp
#include <XEngine.h>               // Gameplay core
#include <XEngine/Storage.h>       // Files and atomic saves, if needed
#include <XEngine/BinaryIO.h>      // Low-level binary, if needed
#include <XEngine/Serialization.h> // JSON/registered classes, if needed
```

In large projects, individual headers such as `<Behaviour.h>` and
`<Transform.h>` are still available. The historical direct headers for
storage/serialization also remain compatible. The specialized module aliases
live in `xengine::io`, `xengine::binary` and `xengine::serialization`.
Internal engine implementation headers live under `XEngine/detail/` and are
not supported gameplay extension APIs.

Convenience accessors like `Input`, `Time` and `Renderer` are typed objects,
not preprocessor macros. The familiar `Input->KeyDown(...)` syntax still
works; use `auto* input = Input.get();` when deducing a manager pointer.

Use `<...>` for exported engine headers, and quoted paths for your own
project headers. If a free function is implemented in an `.hpp` included by
more than one translation unit, mark it `inline` to avoid linker duplicates.
Forward-declare pointer types where possible to avoid cyclic dependencies.

## Debug versus Release

From the game directory run `build.bat --run` on Windows or
`./build.sh --run` on Linux. `--config Release` builds a packaged release.
Debug uses the editable runtime configuration; Release embeds engine JSON.
Keep the build configuration `config.json` distinct from
`assets/config/engine.json`.

When game headers change, the game recompiles, but an unchanged engine library
remains cached. Doxygen is optional: its two references are regenerated only
after the engine library is actually rebuilt, or by the explicit
`python tools/build_doxygen.py` command from the repository root.
To build without regenerating the local API, pass `--no-doc` to your normal
game build (e.g. `./build.sh --run --no-doc` or `build.bat --run --no-doc`).
The optional VS Code Doxygen Hover extension will then use the published
reference rather than any old local HTML; a later build without the flag
restores local-first lookup. The
[one-time setup guide](../../DEVELOPER_GUIDE.md#first-checkout-optional-contributor-tools)
explains optional Git hooks and VS Code integration for newcomers.

Windows/Linux support the same public game API, but each target requires
its own compatible compiler and SDL2 development/runtime libraries.
See [Platforms and conditional compilation](PLATFORMS_AND_BUILD_MODES.md)
for every `XENGINE_PLATFORM_*`, Debug/Release and optional instrumentation
flag, with examples for gameplay `Behaviour` classes.

## Engine release versus game release

XEngine has one official release number, stored in the repository root
[`VERSION`](../../../VERSION) file and displayed in both Doxygen references.
Games use their **own** `project.version` from `config.json` for packaged
distributions. Upgrading XEngine does not alter a game's release number; see
[the engine versioning policy](../../VERSIONING.md) for compatibility rules.

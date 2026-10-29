# Project layout, public includes and builds

XEngine game code is C++20 compiled against the engine's static library.
There is no scripting VM: your classes and headers are compiled with the
rest of the game, and the runtime loads assets and configuration separately.

## BlankGame layout

```text
games/My Game/
  src/main.cpp                Engine startup and InitGame callback
  src/InitCallback.hpp        Scene registration and shared initialization
  assets/scenes/              Scene-builder functions
  assets/gameobjects/         Reusable entity factories
  assets/scripts/             Behaviours and gameplay classes
  assets/textures/            Textures, fonts, music and other assets
  assets/config/engine.json   Runtime settings
  config.json                 Build and package settings
```

The build driver adds `engine/include/` and the game's asset header folders
to the compiler include path. Start with `<XEngine.h>` for convenience or
include only the subsystem headers you use. Heavy JSON serialization is
explicitly opt-in:

```cpp
#include <Behaviour.h>
#include <GameObject.h>
#include <Transform.h>
#include <JsonSerializer.h> // Only when using the JSON facade.
```

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

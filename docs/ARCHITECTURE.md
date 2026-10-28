# Architecture and Platform Boundaries

This guide explains subsystem relationships. For exact public signatures,
properties, parameter semantics and enumeration values, start with the
[Doxygen public API reference](API_REFERENCE.md).
For the practical gameplay workflow, read [Your first game](GETTING_STARTED.md)
and [headers and includes](INCLUDES_AND_DEPENDENCIES.md). Window resolution,
camera projection and UI layout have their own
[window and camera guide](WINDOW_AND_CAMERA.md).

**Engine:** `engine/src` and `engine/include`. Scene, Component, Rigidbody2D and other core logic should remain platform-independent. SDL2 currently provides most desktop platform services. When other backends are introduced, platform-specific calls should be isolated in backend modules.

**Games:** Each game keeps initialization in `src/InitCallback.hpp`, scene assembly in `assets/scenes/*.hpp`, reusable GameObject factories in `assets/gameobjects/*.hpp` and gameplay Behaviours in `assets/scripts/*.hpp`. `src/main.cpp` only starts the engine and registers the initialization callback. Additional `.cpp` translation units can also be compiled. Release packages contain compiled machine code, not these source files.

**Build system:** `config.json` → `build.bat` / `build.sh` → `tools/build.py` (Python standard library only) → direct `g++`, `ar` and `pkg-config` commands. Python 3.10+ is required on both hosts; CMake, Make and Ninja are not required. A single Python build driver keeps flags consistent across Windows and Linux. Cross-compilation selects the target compiler and SDK rather than reusing host libraries.

**Build profiles:** `BuildConfig.h` validates exactly one of the Debug/Release configuration definitions. `Platform.h` detects the actual compiler target through predefined macros and validates it against the target chosen by the build tool.
See [platform macros and build modes](reference/game/PLATFORMS_AND_BUILD_MODES.md)
for supported target definitions, feature switches and game-code examples.

**Working directory:** `./` always means the assets root: `games/<Game>/assets/` during native Debug and the directory containing the executable in a packaged Release. SDL_GetBasePath sets the Release working directory at startup. Linux `run.sh` also configures `LD_LIBRARY_PATH` for bundled shared libraries.

**Persistent game data:** `FileSystem::GetPersistentDataPath` uses SDL2's per-user preference directory and deliberately does not depend on the working directory. `FileSystem` gives games direct file operations; `AtomicFile` optionally stages, syncs and publishes one opaque byte stream while exposing `.bak`/`.tmp` candidates. JSON and binary serializers remain independent of file policy; PlayerPrefs has its own Windows Registry/Linux configuration storage. See [Persistence and serialization](PERSISTENCE_AND_SERIALIZATION.md).

**Distribution:** A Release copies the executable and runtime assets directly into `dist/<platform>-<architecture>/<version>/`, excluding editable engine configuration and C++ source files. Packaging inspects executable dependencies and bundles the libraries it can resolve. Each Release also creates a game-and-version named distributable archive alongside the unpacked version folder (archive entries start at the Release root, without a version directory): `<game> - <version>.zip` for Windows (complete PE/DLL/assets bundle), or `<game> - <version>.tar.gz` for Linux (complete ELF/.so/assets bundle with POSIX executable modes). Test each package on a clean compatible machine and review third-party licenses before distribution. In cross-builds, use target inspection tools (`objdump` for PE, `readelf` for ELF) and target SDK libraries, never host libraries.

**Android:** `XENGINE_PLATFORM_ANDROID` is available for conditional compilation, but Android is not yet a supported build destination. Proper Android support will require an NDK/Clang toolchain, SDLActivity/JNI integration, APK/AAB packaging, Android asset access, input handling and runtime/backend adjustments. The dependency guide has been structured to accommodate future targets.

## Game Icons and Linux Desktop Launchers

- `config.json` → `project.icon`: `null`, a legacy `.ico` path for Windows only, or an object such as `{"windows": "assets/icons/game.ico", "linux": "assets/icons/game.png"}`.
- Windows uses `windres` to embed the `.ico` resource into the executable. Changing the icon invalidates the link cache. No separate `.ico` is shipped.
- Linux packages the PNG under `dist/icons/` and creates a `.desktop` launcher referring to `run.sh` and the icon using absolute paths. The ELF executable itself is unchanged.
- `setup_desktop.sh` regenerates the launcher for its current location. `--install` copies it to `${XDG_DATA_HOME:-$HOME/.local/share}/applications/` for the current user. Re-run it after moving the distribution.
- `run.sh` remains a relocatable entry point and does not require desktop launcher installation or Python on the end user's machine.
- A desktop application icon is not the same as the SDL window icon. Setting the window icon requires a separate runtime call such as `SDL_SetWindowIcon`.

## Runtime threading and profiling

The SDL event pump, renderer, scene lifecycle, `Update`, `FixedUpdate`, physics
integration and callback dispatch all run on the main thread. With
`Engine.multiThreading=true`, only immutable collision candidate geometry is
processed using persistent CPU workers for larger scenes; see
[MULTITHREADING_AND_PROFILING.md](MULTITHREADING_AND_PROFILING.md). The two modes
use identical high-resolution timers and can export comparable frame CSVs via
`Engine.profileCsv=true`.

## CPU-side render preparation

The main thread owns SDL rendering, SDL_ttf and all game state. For larger scenes,
`RenderManager::BeginRecord` records ordered draw commands, an immutable camera
state is captured, and `RenderManager::PrepareSnapshot` uses the existing
`ParallelExecutor` to compute world-to-screen rectangles and conservative
visibility in CPU workers. `RenderSnapshot` executes the ordered commands on
the main thread. A configurable 1536-command reference selects the
recording path in `fixed` mode; the experimental `adaptive` selector uses
weighted sprite complexity and hysteresis; smaller scenes retain immediate rendering. Cached text
textures avoid repeated SDL_ttf rasterization, with eviction after present.

The profiler reports recording, preparation, SDL submission and presentation
separately in both single and parallel modes. Workers never call SDL or access
live scene objects. See `MULTITHREADING_AND_PROFILING.md` for limitations.

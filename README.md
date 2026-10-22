<div align="center">
  <img src="docs/branding/xengine_logo.svg?v=2" alt="XEngine logo — purple X and silver gear" width="176" />
  <h1>XEngine</h1>
  <p><strong>A lightweight, cross-platform 2D game engine built with C++20 and SDL2.</strong></p>
  <p><em>Build 2D games in C++. Understand the engine. Own your code.</em></p>
  <p><small>Modern successor to <a href="https://github.com/the-33/XEngine-Legacy">XEngine-Legacy</a> (Visual Studio / MSVC)</small></p>
  <p>
    <a href="https://en.cppreference.com/w/cpp/20"><img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&amp;logoColor=white" /></a>
    <a href="https://www.libsdl.org/"><img alt="SDL2" src="https://img.shields.io/badge/SDL-2-1E6D83" /></a>
    <img alt="Windows x64" src="https://img.shields.io/badge/Windows-x64-555555?logo=windows" />
    <img alt="Linux x64" src="https://img.shields.io/badge/Linux-x64-555555?logo=linux" />
    <img alt="GCC / Clang" src="https://img.shields.io/badge/Build-GCC%20%2F%20Clang-7851B1" />
  </p>
  <p>
    <a href="#quick-start">Get started</a> ·
    <a href="#features">Features</a> ·
    <a href="#documentation">Documentation</a> ·
    <a href="#benchmarking-and-profiling">Benchmarks</a> ·
    <a href="#games-migrated-from-xengine-legacy">Migrated games</a> ·
    <a href="#migrating-from-xengine-legacy">Legacy migration</a>
  </p>
</div>

---

## Why XEngine?

XEngine is a **source-driven** 2D engine with a familiar **Scene → GameObject → Component / Behaviour** model. It takes inspiration from engines such as Unity without requiring a proprietary editor or scripting runtime: games and engine systems are regular, readable C++.

The project includes a portable build tool, a ready-to-use game template and an actual SDL2 benchmark game. The engine targets native **Windows** and **Linux** development, with optional cross-compilation when the corresponding target toolchain and SDL2 SDK are available.

> **Project origins:** XEngine is the modern, cross-platform evolution of [XEngine-Legacy](https://github.com/the-33/XEngine-Legacy), the original SDL2 engine developed using **Visual Studio and MSVC**. This version has a reorganized game structure and a portable **GCC/Clang** build workflow. Legacy game projects are not directly compatible; see the [migration plan](docs/LEGACY_MIGRATION_PLAN.md).

> **Scope:** XEngine uses SDL2, SDL2_image, SDL2_mixer and SDL2_ttf. Android, macOS, iOS and web are **not supported build targets** at present. Multithreading is optional; it does not promise faster rendering for every scene.

## Features

| System | What you get |
|:--|:--|
| **Scenes & components** | GameObjects, hierarchical transforms, reusable Behaviours and safe queued scene/object operations |
| **2D rendering** | Sprites, textures, animations, rotation (including X/Y pseudo-3D projection), camera, parallax, culling and UI |
| **Physics & collision** | RigidBody2D, box/circle colliders, triggers, collision events and fixed-timestep simulation |
| **Input, audio & assets** | Keyboard/mouse, cached textures and fonts, sound effects, music and immediate-mode UI |
| **Performance tools** | Optional collision workers, render-command preparation workers, adaptive collision broadphase, CSV profiling and F3/F4 overlays |
| **Build & distribution** | Windows/Linux native builds, optional cross-build SDK tooling and packaged Release archives |

For architecture, rendering, threading and physics details, read [Architecture](docs/ARCHITECTURE.md), [Multithreading & profiling](docs/MULTITHREADING_AND_PROFILING.md) and the [Developer Guide](docs/DEVELOPER_GUIDE.md).

## Quick start

Clone the repository and install the **host compiler, Python 3.10+ and SDL2 development libraries**. The commands below create a new game rather than depending on private sample games.

<details open>
<summary><strong>Windows 10/11 · MSYS2 UCRT64</strong></summary>

Install [MSYS2 UCRT64](https://www.msys2.org/) and follow the [Windows dependency guide](docs/DEVELOPER_GUIDE.md#windows-1011-x86_64--msys2-ucrt64). Ensure `g++` and `pkg-config` resolve to your UCRT64 installation, then run in a Windows terminal from the repository root:

```bat
check_dependencies.bat
new_project.bat "My Game"
cd "games\My Game"
build.bat --run
```

</details>

<details>
<summary><strong>Linux · Ubuntu / Debian</strong></summary>

Install GCC, Python and the SDL2 packages using the [Linux dependency guide](docs/DEVELOPER_GUIDE.md#linux-x86_64--ubuntu--debian). Then, from the repository root:

```bash
./check_dependencies.sh
./new_project.sh "My Game"
cd "games/My Game"
./build.sh --run
```

</details>

To build a distributable version, run `build.bat --config Release` (Windows) or `./build.sh --config Release` (Linux) inside the game directory. See the [Release distribution guide](docs/DEVELOPER_GUIDE.md#release-distribution) for packaging, icons, assets and runtime dependencies.

## Project structure

```text
XEngine/
├── engine/
│   ├── include/              # Public C++20 engine API
│   └── src/                  # Engine implementation
├── games/
│   └── XEngine Benchmark/    # Public, reproducible benchmark game
├── tools/
│   ├── templates/BlankGame/ # Starting point for new games
│   ├── new_project.py       # Create a game
│   ├── build.py             # Native and cross-platform build driver
│   ├── sync_engine_configs.py
│   └── sync_game_icons.py
├── docs/                     # Architecture and advanced guides
├── tests/                    # Regression tests
└── README.md
```

A generated game uses `src/main.cpp` for startup, `src/InitCallback.hpp` for asset loading and scene registration, and `assets/scenes/`, `assets/gameobjects/` and `assets/scripts/` for game code. Build settings live in `config.json`; runtime settings live in `assets/config/engine.json`.

### Runtime configuration

The canonical runtime schema lives in [`BlankGame/assets/config/engine.json`](tools/templates/BlankGame/assets/config/engine.json). When the engine's configuration changes, the template and synchronization manifest must be updated together.

```bash
python tools/sync_engine_configs.py --check  # Preview changes
python tools/sync_engine_configs.py          # Apply without replacing custom values
```

**Every game build automatically runs both the engine configuration and default-icon synchronizers before compilation**. No manual sync command is required when building; these commands are useful for checking your workspace beforehand. Neither operation replaces custom game settings or custom icon filenames.

For icons, put your original `xengine_icon.ico` (Windows) and 512×512 `xengine_icon.png` (Linux) in the BlankGame template and use `"default"` in `config.json` for either platform. A missing configured custom icon is reported and falls back to the synchronized *game-local* default. Run `python tools/sync_game_icons.py --check` to inspect changes and `python tools/sync_game_icons.py --apply` to sync all games outside a build. See [Default game icons](docs/DEFAULT_GAME_ICONS.md) and [Engine configuration migration](docs/ENGINE_CONFIG_MIGRATION.md). The render selector has a **single global threshold** (`parallelMinCommands`, experimental default **1536**) and two strategies: `fixed` (default) and opt-in `adaptive` (weighted projection cost with hysteresis). Existing custom values are preserved by configuration synchronization. See the [adaptive render selector guide](docs/ADAPTIVE_RENDER_SELECTION.md).

### Which games are published?

The **XEngine Benchmark** is included in Git by default. Other games are excluded unless their root contains `games/<Game Name>/.gitkeep`. Markers inside `assets/` do **not** opt a game in. The new-project generator deliberately skips `.gitkeep` markers.

To reconcile rules and stage opted-in games:

```bash
python tools/sync_game_gitignore.py --stage
```

An optional `pre-commit` hook automates this step after it has been enabled with `git config --local core.hooksPath .githooks`. **It stages all changes in included games**, so review `git diff --cached` before committing. See [Game Git tracking](docs/GAME_GIT_TRACKING.md).

## Platforms & builds

| Host | Target | Status |
|:--|:--|:--|
| Windows · MSYS2 UCRT64 | Windows x86_64 | Native build supported |
| Linux · GCC | Linux x86_64 | Native build supported |
| Linux · MinGW-w64 | Windows x86_64 | Cross-build and packaging demonstrated; verify on Windows |
| Windows · Clang/LLD | Linux x86_64 | Cross-build tooling provided; end-to-end validation required |

**Cross-compilation requires the target SDK** (including the target SDL2 libraries). XEngine offers optional setup helpers; there is no requirement to use Visual Studio, CMake, Make or Ninja.

[Cross-compilation guide](docs/CROSS_COMPILATION.md) · [Dependency and packaging reference](docs/DEVELOPER_GUIDE.md) · [Configure a custom build](docs/DEVELOPER_GUIDE.md#build-configuration-configjson)

**Optional cross-SDK setup** (run from the repository root after installing the host compiler):

```powershell
install_cross_sdk.bat --target linux  # Windows → Linux
```

```bash
./install_cross_sdk.sh --target windows  # Linux → Windows
```

Use `--sdk-dir` to choose a custom SDK directory. For a manual alternative, see the [cross-compilation reference](docs/CROSS_COMPILATION.md).

## Benchmarking and profiling

<p align="center">
  <a href="docs/BENCHMARK_GAME.md">
    <img src="docs/branding/xengine_benchmark_logo.svg?v=2" alt="XEngine Benchmark logo — a speedometer within a gear" width="136" />
  </a>
</p>

`games/XEngine Benchmark/` contains an SDL2 game for repeatable comparisons of collisions, threading and render workloads (including rotation, tint, culling and composite scenes). It is checked into Git; the results generated locally are ignored.

For an interactive overview, toggle the **F3** debug profiler and use **F4** to change pages. When `Engine.profileCsv` is enabled, frame and subsystem timings can be exported for analysis.

```bash
python tools/compare_profiles.py --game "XEngine Benchmark"
```

The parallel collision path has shown measurable advantages in high-contact workloads. **Render preparation parallelism is workload-dependent**: existing measurements do not establish a universal speedup or threshold. See the [benchmark game guide](docs/BENCHMARK_GAME.md), [render calibration](docs/RENDER_THRESHOLD_CALIBRATION.md), [adaptive render selector](docs/ADAPTIVE_RENDER_SELECTION.md) and [adaptive broadphase](docs/ADAPTIVE_BROADPHASE.md).

**v16.8:** Sprite X/Y/Z projection now skips unnecessary trigonometry and can run on the CPU render workers without touching SDL. The [sprite projection optimization and A/B procedure](docs/SPRITE_PROJECTION_OPTIMIZATION.md) distinguishes main-thread vs worker-side geometry within the same recorded render path. No performance gain is asserted until platform benchmarks confirm it.

## Games migrated from XEngine-Legacy

**Two playable projects were migrated from the original [XEngine-Legacy](https://github.com/the-33/XEngine-Legacy) (Visual Studio/MSVC) to the current cross-platform XEngine.** They demonstrate how the same SDL2 gameplay can be reorganized into independently buildable C++20 projects, with scenes, reusable GameObject factories, Behaviours and per-game configuration.

| Game | Original game | What the migration demonstrates | Migration notes |
|:--|:--|:--|:--|
| **Blob's Quest** | [Legacy Game1](https://github.com/the-33/XEngine-Legacy/blob/main/scr/internal/game1/main.cpp) | Modular player/enemy/coin/projectile factories, physics, collision callbacks, HUD, camera and shared initialization | [Read `MIGRATION.md`](docs/migrations/blobs-quest/MIGRATION.md) |
| **My Princess Run** | [Legacy Game2](https://github.com/the-33/XEngine-Legacy/blob/main/scr/internal/game2/main.cpp) | Runner gameplay, background/object factories, animation, shared game state and an intentional cactus-spawn fix | [Read `MIGRATION.md`](docs/migrations/my-princess-run/MIGRATION.md) |

The [Legacy migration documentation](docs/LEGACY_MIGRATION_PLAN.md#real-world-migration-case-studies) explains both case studies and how they inform a possible general-purpose migration tool. Each original report also lives at **`games/<Game Name>/MIGRATION.md`** in the complete local workspace. Since personal games are **not published by default**, the links above point to public documentation copies under `docs/`, so they work even when the game's source and assets remain private. The games require their original resource files to build and run as intended.

<details>
<summary><strong>Watch the original gameplay GIFs (hosted by XEngine-Legacy)</strong></summary>

<p align="center">
  <strong>Blob's Quest</strong><br />
  <img src="https://raw.githubusercontent.com/the-33/XEngine-Legacy/main/docs/gameplay/blobs_quest.gif" width="420" alt="Blob's Quest gameplay in the original XEngine-Legacy" />
</p>
<p align="center">
  <strong>My Princess Run</strong><br />
  <img src="https://raw.githubusercontent.com/the-33/XEngine-Legacy/main/docs/gameplay/my_princess_run.gif" width="420" alt="My Princess Run gameplay in the original XEngine-Legacy" />
</p>

*These previews show the games in XEngine-Legacy; they are not recordings of a recent modern XEngine build. The original GIFs are large, so they are referenced rather than copied into this repository.*

</details>

## Migrating from XEngine-Legacy

[XEngine-Legacy](https://github.com/the-33/XEngine-Legacy) is the **earlier Visual Studio/MSVC-based engine**, not an earlier build of this repository. Its projects use a different source layout, startup procedure and configuration paths.

A **general-purpose Legacy → modern XEngine migrator is being evaluated, not yet implemented**. The proposed tool would generate a modern game project, copy assets, translate known configuration keys and produce a report of C++ code requiring manual attention. It should never silently rewrite unknown C++ syntax or modify the original project.

Read the [Legacy migration feasibility and proposed design](docs/LEGACY_MIGRATION_PLAN.md), including the [two completed game migrations](docs/LEGACY_MIGRATION_PLAN.md#real-world-migration-case-studies).

## Documentation

| Guide | Subject |
|:--|:--|
| [Architecture](docs/ARCHITECTURE.md) | Engine systems, scenes and lifecycle |
| [Developer guide](docs/DEVELOPER_GUIDE.md) | Complete installation, building, packaging and troubleshooting reference |
| [Cross-compilation](docs/CROSS_COMPILATION.md) | Windows/Linux SDKs, cross builds and verification |
| [Multithreading and profiling](docs/MULTITHREADING_AND_PROFILING.md) | Worker scheduling, profiler and tradeoffs |
| [Benchmark game](docs/BENCHMARK_GAME.md) | Reproducible game-integrated performance tests |
| [Render threshold calibration](docs/RENDER_THRESHOLD_CALIBRATION.md) | Deterministic render scenarios and equivalence checking |
| [Adaptive render selector](docs/ADAPTIVE_RENDER_SELECTION.md) | Fixed vs weighted cost selection, hysteresis and variable-load calibration |
| [Engine config synchronization](docs/ENGINE_CONFIG_MIGRATION.md) | Canonical settings, backups and safe updates |
| [Game Git tracking](docs/GAME_GIT_TRACKING.md) | How to include or exclude your games |
| [Legacy migration plan](docs/LEGACY_MIGRATION_PLAN.md) | Scope and limits of a future Legacy migrator, plus completed game examples |
| [Blob's Quest migration](docs/migrations/blobs-quest/MIGRATION.md) | Original game migration notes (public copy) |
| [My Princess Run migration](docs/migrations/my-princess-run/MIGRATION.md) | Original game migration notes (public copy) |

## Contributing & testing

XEngine is an evolving C++20 engine. Changes should preserve cross-platform behavior and be accompanied by reproducible tests, particularly for scene lifetime, collisions, rendering and packaging.

```bash
python -m unittest discover -s tests -v
```

Tests do not replace compiling and running a real SDL2 game on the intended host and target platforms. The C++ engine is portable, but toolchains, binaries and library dependencies are platform-specific.

---

<div align="center">
  <sub>Made with C++20 and SDL2 · <a href="https://github.com/the-33/XEngine-Legacy">From XEngine-Legacy to XEngine</a></sub>
</div>

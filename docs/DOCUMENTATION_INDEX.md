# XEngine documentation

Choose the guide that matches what you want to accomplish. These documents
describe how to **use the engine to build games**; implementation history,
benchmarks and experimental performance investigations are listed separately.

## Build a game

| Guide | When to use it |
|---|---|
| [Your first game](GETTING_STARTED.md) | Start with a generated project, write a `Behaviour`, build a scene and run the game. |
| [Headers and includes](INCLUDES_AND_DEPENDENCIES.md) | Organize C++ code, choose `<XEngine.h>` versus focused headers, avoid circular includes and work with properties. |
| [Platform macros and build modes](reference/game/PLATFORMS_AND_BUILD_MODES.md) | Compile Windows/Linux-specific code; use Debug/Release and optional instrumentation flags safely. |
| [Public API reference](API_REFERENCE.md) | Look up exact classes, signatures, property semantics and enum values. |
| [Window, camera and UI](WINDOW_AND_CAMERA.md) | Change resolution/fullscreen, understand world vs screen coordinates and lay out UI. |
| [Persistence and serialization](PERSISTENCE_AND_SERIALIZATION.md) | Read/write files, save data atomically, validate backup candidates, use binary/JSON and PlayerPrefs. |
| [Architecture](ARCHITECTURE.md) | Understand scenes, lifecycle, platform boundaries and renderer ownership. |
| [Versioning](VERSIONING.md) | Check the current engine version, compatibility policy and distinction from game releases. |

## Install, configure and ship

| Guide | When to use it |
|---|---|
| [Developer Guide](DEVELOPER_GUIDE.md) | Install Windows/Linux dependencies, build Debug/Release and troubleshoot toolchains. |
| [Cross-compilation](CROSS_COMPILATION.md) | Configure a target toolchain and matching SDK for builds between Windows and Linux. |
| [Game build config](GAME_CONFIG_SYNC.md) | Understand `games/<Game>/config.json` and its schema synchronization. |
| [Runtime engine config](ENGINE_CONFIG_MIGRATION.md) | Understand `assets/config/engine.json` defaults and synchronization. |
| [Game icons](DEFAULT_GAME_ICONS.md) | Supply Windows ICO/Linux PNG and let XEngine package application icons. |
| [Tracking games in Git](GAME_GIT_TRACKING.md) | Choose which games to check in and how the opt-in marker works. |
| [Release backend selection](RENDER_BACKENDS.md) | Configure SDL render backends and compare performance on real machines. |

## Performance, diagnostics and engine internals

These guides are for developers who want to profile, optimize or modify
XEngine. They are **not prerequisites** for making a game and should not be
interpreted as mandatory configuration or guaranteed performance gains.

| Guide | Subject |
|---|---|
| [Benchmark game](BENCHMARK_GAME.md) | Reproduce engine-integrated benchmark scenarios and read their results. |
| [Multithreading and profiling](MULTITHREADING_AND_PROFILING.md) | CPU worker scheduling, profiling and practical limitations. |
| [Render diagnostics](RENDER_DIAGNOSTICS.md) | Interpret draw calls, submissions, path selection and frame timings. |
| [Documentation contribution workflow](reference/engine/DOCUMENTATION_WORKFLOW.md) | Maintain public/internal guides, Doxygen comments, navigation, profiling branding and light/dark variants. |

## Historical development archive (not current API guidance)

The documents below record earlier releases, benchmarks, unimplemented
proposals and intermediate decisions. They remain in the source repository
for context but are deliberately **not included in generated Doxygen**. To
understand current configuration, use the developer and reference guides
above and check the shipped `engine.json` template.

| Archived report | Historical scope |
|---|---|
| [Adaptive broadphase](ADAPTIVE_BROADPHASE.md) | v11–v14 broadphase evolution, not current tuning instructions. |
| [Adaptive render selection](ADAPTIVE_RENDER_SELECTION.md) | v16.9+ experimental development log. |
| [Broadphase/render investigation](SPATIAL_BROADPHASE_AND_RENDER.md) | v8 design and performance observations. |
| [Contact merge and context](SPATIAL_CONTEXT_AND_CONTACT_MERGE.md) | v15 collision implementation and old test status. |
| [Sprite projection](SPRITE_PROJECTION_OPTIMIZATION.md) | v16.8 optimization experiment. |
| [Render threshold calibration](RENDER_THRESHOLD_CALIBRATION.md) | Benchmarks and historical thresholds. |
| [Dynamic broadphase benchmark](DYNAMIC_BROADPHASE_BENCHMARK.md) | v14 workload experiments. |
| [Render culling forensics](RENDER_CULLING_RELEASE_FORENSICS_20261022.md) | Dated diagnostic investigation. |
| [Legacy migration](LEGACY_MIGRATION_PLAN.md) | Proposal and case studies, not an implemented migrator. |

## Generate the searchable API documentation

The source `.h` files contain Doxygen comments for the public API. Install
Doxygen if you want the searchable reference locally, particularly when
modifying the engine:

```bash
python tools/build_doxygen.py
python tools/check_doxygen_docs.py
```

Open `build/docs/index.html` and choose the **Game Developer Reference**
or **Engine Developer Reference**. Each has Markdown manuals grouped in a
collapsible topic tree and the XEngine light/dark visual theme. Machine-readable
XML is generated separately in `build/docs/game/xml/` and
`build/docs/engine/xml/`. All generated docs are **build outputs**, not tracked
source documentation.

The Game Developer Reference also includes a curated selection of the
full-length general manuals, such as `DEVELOPER_GUIDE.md`, setup,
cross-compilation, configuration, persistence and packaging. It does not
include engine implementation internals or historical benchmark reports.

The regular engine build also regenerates this reference automatically after
the engine library is actually rebuilt. If Doxygen is missing, the build
prints a recommendation and continues normally. Rebuilding **only game code**
does not regenerate unchanged engine documentation.

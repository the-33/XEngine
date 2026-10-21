# Migrating Games from XEngine-Legacy: Feasibility and Proposed Design

**Status: proposal only. No Legacy-to-modern migration command exists yet.**

The two projects are different generations:

- **[XEngine-Legacy](https://github.com/the-33/XEngine-Legacy)** used Visual Studio/MSVC, solution and `.vcxproj` files, and a shared engine/source/assets tree.
- **[XEngine](../README.md)** is a C++20/SDL2 source-driven engine with independent game projects, a Python build system, and native Windows/Linux or cross builds using GCC/Clang.

This is distinct from historical one-off scripts that reorganized an *intermediate, already-modern* XEngine workspace. Those scripts are intentionally **not** part of the proposed solution.

## Evidence from the Legacy repository

The published Legacy tree uses, among other paths:

| Legacy location | Meaning | Modern location / target |
|:--|:--|:--|
| `project/Games/Game1/Game1.vcxproj` | Visual Studio project | `games/<Name>/config.json` and `build.bat`/`build.sh` from BlankGame |
| `scr/internal/game1/main.cpp` | Game startup, factories, scenes and Behaviours, often in one translation unit | `src/main.cpp`, `src/InitCallback.hpp`, plus `assets/scenes/`, `assets/scripts/`, `assets/gameobjects/` |
| `project/Games/Game1/XEngine_CONFIG.json` | Legacy engine settings | `assets/config/engine.json` with modern schema |
| `assets/game1/{textures,audio,fonts}` | Game resources | `games/<Name>/assets/{textures,audio,fonts}` |
| `scr/internal/gameEngine/` | Legacy engine sources | **Not copied:** link the modern shared `engine/` instead |
| `scr/external/SDL2*` | Visual Studio third-party SDKs | **Not copied:** use modern host/target SDL2 development packages |

The Legacy game entry point invokes `Engine::Start(...)` with an old relative config path and may keep game setup, prefab lambdas, global variables and Behaviours in `main.cpp`. Its JSON includes keys such as `Engine.logStats` / `Engine.logErrors` and `Assets.assetsFolderPath`, whose handling must be compared with the modern configuration schema rather than copied blindly.

## Proposed tool contract

Potential command-line interface (illustrative; **not implemented**):

```text
python tools/migrate_legacy_game.py --legacy-root PATH --legacy-project Game1 --name "My Game" --dry-run
python tools/migrate_legacy_game.py --legacy-root PATH --legacy-project Game1 --name "My Game" --output-dir games
```

### Phase 1 — Discovery and safety

1. Require an explicit Legacy repository root and project choice. Read `.vcxproj` to locate the actual `.cpp` sources instead of assuming every game uses `game1` and `assets/game1`.
2. Refuse ambiguous, missing, out-of-root or inaccessible source paths and warn about absolute Windows/MSVC-only paths, preprocessor definitions and custom libraries.
3. Produce a complete **dry-run manifest** listing source files, resources, requested transforms, skipped files and uncertainties.
4. Never overwrite an existing modern game without a separate explicit option and a verified backup. Never delete or rewrite the original Legacy project.

### Phase 2 — Safe automatic project conversion

1. Generate a new project using the **current BlankGame template** and `tools/new_project.py` conventions.
2. Copy user-owned textures, audio, fonts, and other resources preserving case and relative paths. Record missing/duplicate files; do not copy bundled Legacy SDL DLLs or engine sources.
3. Migrate **known, schema-supported** JSON fields into `assets/config/engine.json`. Keep modern defaults for fields that do not exist in Legacy. Report unsupported or renamed keys; preserve user choices for window size, title, gravity, audio and input whenever their semantics match.
4. Extract a basic initialization entry point only when the source matches recognized constructs. Preserve unrecognized C++ as original source material under a review area; do **not** claim successful conversion if it still requires adaptation.
5. Generate the normal independent-game build config, scripts and asset tree. Do not add root-level `.gitkeep` automatically: game publication must remain opt-in.

### Phase 3 — Assisted C++ API migration

A generic search-and-replace over C++ is unsafe. Potential differences include:

- Start and lifecycle wiring (`Engine::Start`, `Engine::Run`, `Engine::SetInitCallback`, relative runtime paths).
- Header-only gameplay and definitions that must be `inline` or moved to a `.cpp` to avoid ODR/duplicate-symbol failures.
- Scene registration, prefab/`Instantiate`, callback and component signatures that may have changed.
- C++ `Property` access expressions, names of manager helpers, assets and SDL version-dependent APIs.
- MSVC-specific pragmas, Windows-only includes, precompiled headers and case-insensitive asset paths that may fail on Linux.
- Physics/collision semantic differences, not just syntactic changes: verify layers, triggers, collision event order and gameplay behavior.

Any automated rewrite must be limited to **documented, tested transformations with predictable preconditions**. For everything else, report file, line, suggested action and sample replacement rather than guessing. A future LibTooling-based mode could increase accuracy, but should remain optional: the basic tool should use the Python standard library.

### Phase 4 — Validation

- Check the output tree and references; verify every referenced asset file exists (including filename case).
- Validate JSON structure against the current runtime/template keys; leave unsupported keys in a diagnostic report, not live config.
- Compile a generated skeleton using the existing `tools/build.py` on the selected host; treat compilation failures as actionable migration findings.
- Require manual scene/gameplay tests. Successful compilation does **not** imply identical movement, physics, rendering, timing or audio.
- Use the Legacy repository's `Game1` and `Game2` as regression fixtures, without copying potentially copyrighted game assets into this repository or public tests.

## What an initial implementation could realistically promise

**Reliable, automation-friendly:** discover MSVC project sources, copy assets, construct modern game folders, migrate mapped config fields, generate a migration report, and produce buildable *skeletons*.

**Only sometimes automatic:** split large gameplay `.cpp` files, convert scene setup and normalize known API signatures.

**Requires human review:** substantial gameplay logic, platform-specific C++, semantics of physics/render changes, and behavior equivalence.

The recommended delivery is **a report-first, non-destructive converter**, then optional AST-aware conversion for well-supported patterns. It should not replace the modern XEngine project generator and it should not resurrect old one-game scripts.

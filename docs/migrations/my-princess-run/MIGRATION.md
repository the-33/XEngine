# My Princess Run: Migration to XEngine

> **Public migration case study.** This document is based on the game-specific `games/My Princess Run/MIGRATION.md` supplied by the project author. The live game and its assets are not included in Git unless the game is explicitly opted in. This records a completed **XEngine-Legacy → modern XEngine** migration; it is **not** a one-command migration procedure. [See the migration plan](../../LEGACY_MIGRATION_PLAN.md#real-world-migration-case-studies).


## Project Structure

- `src/main.cpp`: engine startup, Debug/Release selection and asset working directory.
- `src/InitCallback.hpp`: shared resource loading, `level1` registration and activation.
- `assets/scenes/Level1.hpp`: scene composition only; no asset registration or loading.
- `assets/scripts/*.hpp`: one complete Behaviour per header.
- `assets/gameobjects/*.hpp`: reusable sprite, obstacle and background-object factories.
- `assets/scripts/GameState.hpp`: shared score, speed and screen-boundary state.

## Required Original Assets

Copy these files from the original game into the new project; the source assets were not supplied with the migration:

| File | Asset key |
|---|---|
| `assets/textures/ground.png` | `ground` |
| `assets/textures/princess-run.png` | `princess-run` |
| `assets/textures/princess-jump.png` | `princess-jump` |
| `assets/textures/princess-fall.png` | `princess-fall` |
| `assets/textures/princess-die.png` | `princess-die` |
| `assets/textures/sky.png` | `sky` |
| `assets/textures/back-1.png` | `rock-big` |
| `assets/textures/back-2.png` | `rock-small` |
| `assets/textures/cloud-1.png` | `cloud-big` |
| `assets/textures/cloud-2.png` | `cloud-small` |
| `assets/textures/cactus-1.png` | `cactus-1` |
| `assets/textures/cactus-2.png` | `cactus-2` |
| `assets/textures/cactus-3.png` | `cactus-3` |
| `assets/textures/ui-btn-down.png` | `ui-button` |
| `assets/fonts/ui-font-default.ttf` | `ui_default`, `ui_default_big` |

## Configuration

`assets/config/engine.json` preserves values from the original `XEngine_CONFIG(1).json`, including `multiThreading: true`, `logStats: true`, `fixedDt: 0.005`, graphics, audio and physics settings. Only `Assets.assetsFolderPath` changes to `./` to follow the XEngine asset-root convention.

**Multithreading note:** The original engine may encounter issues if SDL2 rendering is performed on a thread other than the main thread. If the application fails during runtime, try setting `multiThreading: false` in Debug as a diagnostic step; this does not change the original project defaults.

## Intentional Gameplay Fix

The three cactus factories no longer reset `transform->position` to `(0,3,0)`. The spawner supplies a spawn position at the right edge of the screen, and the old assignment inadvertently overwrote that position.

No original executable or graphical asset files are included.

## Integrating and Building

1. Extract the project files under `games/My Princess Run/` within the XEngine workspace.
2. Use the current `tools/build.py`, which discovers C++ headers throughout `assets/`.
3. Copy the original textures and font into the new `assets/` folder.
4. From PowerShell, build the project:

```powershell
cd "games/My Princess Run"
.\build.bat --run
.\build.bat --config Release
```

For Linux, use `./build.sh --run` or `./build.sh --config Release` from the same game directory. A Release includes runtime assets but not the `.hpp` files. Debug keeps `engine.json` editable; Release embeds it in the executable.

## Windows and Linux Icons

The original game icon was not supplied. The recommended cross-platform configuration is:

```json
"project": {
  "name": "My Princess Run",
  "version": "0.1.0",
  "icon": {
    "windows": "assets/icons/princess.ico",
    "linux": "assets/icons/princess.png"
  }
}
```

**Only use these paths if the files actually exist.** Windows embeds the ICO using `windres`. Linux includes the PNG in the Release and references it through a `.desktop` launcher. To generate an ICO from a PNG and update the project config, use:

```powershell
.\tools\png_to_ico.bat "My Princess Run" princess --set-config
```

Run the above command from the **XEngine root** and use `--force` if replacing an existing ICO. Windows Explorer may cache old executable icons; refresh Explorer or inspect the `.exe` properties when validating the new icon.

# Blob's Quest: Modular Migration to XEngine

## Architecture

- `src/main.cpp`: starts the engine and assigns `Engine::SetInitCallback(BlobQuest::InitGame)`.
- `src/InitCallback.hpp`: loads shared assets and the configured font, sets up the background and music, registers `level1` and makes it active.
- `assets/scenes/Level1.hpp`: contains **only `BuildLevel1(Scene*)`**, which places the player, walls, coins, HUD and spawner, and configures the camera.
- `assets/scripts/`: **one Behaviour per `.hpp`**, plus `GameState` for score and restart state.
- `assets/gameobjects/`: reusable C++ factories (`Player`, `Enemy`, `Coin`, `Fireball`, `Wall`, `SpriteFactory`). These are **compiled C++**, not serialized asset files.
- `assets/textures/`, `assets/audio/`, `assets/fonts/`: runtime assets.

New projects generated with `new_project.bat` contain an example factory in `assets/gameobjects/ExampleObject.hpp`.

`.hpp` files are compiled through their include chain (`src/main.cpp` → initialization callback → scene → game objects and Behaviours). No companion `.cpp` is required. A Release ships runtime assets but **never these C++ sources**.

## Required Original Assets (Not Included)

```text
assets/
  textures/player.png, enemy.png, wall.png, coin.png, floor.png, fireball.png
  audio/music.ogg, coin.wav, hit.wav, enemy_hit.wav, enemy_die.wav
  fonts/ui-font-default.ttf
```

Copy these assets from the original game, preserving their filenames. If critical textures are missing, the game deliberately aborts scene registration instead of dereferencing null texture pointers.

## Applying Older Migration Patches

Extract the relevant patch into the XEngine root directory, overwriting matching files, then run:

```powershell
py -3 tools\migrate_to_hpp.py
```

This moves obsolete split `.h/.cpp` components, factories and scenes to `_legacy_split_files/` as a backup without deleting previous work. For an even older monolithic `BlobQuestGameplay.cpp` migration, also run `py -3 tools\migrate_blob_quest_modular.py`.

**Do not leave duplicate legacy `.cpp` files in `assets/scripts/`.**

## Building and Running

Windows (PowerShell):

```powershell
cd "games/Blob's Quest"
.\build.bat --run
.\build.bat --config Release
```

Linux:

```bash
cd "games/Blob's Quest"
./build.sh --run
./build.sh --config Release
```

## Gameplay and Behavior

The original game logic has been split into focused components while retaining the initial settings for firing, enemies, coins, camera, score, health and UI. `RestartLevel()` now resets the score when restarting (previously, it was reset only in some event paths). Null-pointer checks were added to sensitive gameplay operations.

These migration notes describe the changes to the sample; gameplay and audio should still be regression-tested with the original asset files on each target platform.

## Restored Engine Configuration

The game's original `engine.json` values are preserved, except for `Assets.assetsFolderPath`, which is now `./` to match the XEngine directory layout. The engine loads the font selected by `Assets.defaultFontRelativePath` once; the scene must not load it again. Confirm that `assets/fonts/ui-font-default.ttf` exists. If your source game uses `ui_font.ttf`, update the path in `engine.json` (and any code that refers to it).

Debug reads the editable JSON at every start. Release embeds the configuration in the executable, so changes require rebuilding.

## Responsibility Boundaries

`main.cpp` → `src/InitCallback.hpp` (`BlobQuest::InitGame`) → `assets/scenes/Level1.hpp` (`BuildLevel1`) → `assets/gameobjects/` → `assets/scripts/`.

Register additional scenes in `InitCallback.hpp`; do not load global resources or call `Scenes->SetActive()` from `Level1.hpp`.

# Default game icons and automatic synchronization

XEngine reserves two **game-local** default icon filenames:

| Platform | File in each game | Original source in BlankGame |
| --- | --- | --- |
| Windows | `assets/icons/xengine_logo.ico` | `tools/templates/BlankGame/assets/icons/xengine_logo.ico` |
| Linux | `assets/icons/xengine_logo.png` (512×512) | `tools/templates/BlankGame/assets/icons/xengine_logo.png` |

The maintainer supplies the original images. The synchronization tool **does not generate, convert, rebrand or modify the pixels**. GitHub's README uses the separate vector logo `docs/branding/xengine_logo.svg`. These files are not included in code-only patches if the maintainer has not supplied them to the patch author.

## Recommended `config.json`

```json
{
  "project": {
    "name": "My Game",
    "icon": {
      "windows": "default",
      "linux": "default"
    }
  }
}
```

The `default` keyword resolves to the icons **inside the game**, not directly to the template. A string `"icon": "default"` also uses the game-local platform default on both Windows and Linux. Explicit platform paths continue to work. `null` disables the icon for the corresponding platform; it is not treated as a missing file.

If a configured **custom path** is missing, `build.py` prints `[ICON] WARNING` and falls back to the game-local default for the relevant target. It does not modify `config.json`. Invalid custom files, unsafe paths (`..`, absolute paths, symlinks outside the game), and missing/corrupt default icons cause an error instead of a silent fallback.

## Build integration

Every `python tools/build.py game --project "My Game"` (or game-level `build.bat` / `build.sh`) does the following **before compiling game or engine sources**:

1. `sync_engine_configs.py --game "My Game"` updates its runtime engine JSON and additional startup profiles from the canonical BlankGame schema while preserving compatible custom values and backing up changes under `.xengine-config-backups/`.
2. `sync_game_icons.py --game "My Game"` compares SHA-256 of `xengine_logo.ico` and `xengine_logo.png` with BlankGame; it copies absent files and replaces outdated reserved filenames, backing up replaced bytes under `.xengine-icon-backups/`.
3. On success, the normal compiler and linker run; a changed Windows icon causes a relink through the existing build signature mechanism.

Only the **requested game** is modified by a build. A standalone sync without `--game` updates all game directories. Both synchronizers are idempotent.

All tool output remains visible on the console (and benchmark `build.log` through its live tee). If either synchronizer or the compiler fails, the build exits nonzero: no benchmark may run a stale binary.

## Command-line synchronization

```powershell
python tools/sync_game_icons.py --check                 # Compare all games; exit 1 if changes needed
python tools/sync_game_icons.py --apply                 # Synchronize all games
python tools/sync_game_icons.py --game "My Game" --apply # Synchronize one game
python tools/sync_engine_configs.py --check             # Inspect runtime JSON
```

**Custom icons are never overwritten** when stored under other filenames such as `assets/icons/player.ico` or `assets/icons/custom.png`, even if `config.json` selects those filenames. The reserved names `xengine_logo.ico` and `xengine_logo.png` are always managed; do not use them for game-specific artwork.

## Template artwork prerequisite

Before applying this update, confirm that you have added the actual `xengine_logo.ico`, `xengine_logo.png` and `xengine_logo.svg` files to the locations listed above. The original artwork is not present in all historical XEngine ZIPs. If the two BlankGame defaults are missing, the sync fails with a descriptive error rather than copying obsolete logos.

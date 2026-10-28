# Default game icons and automatic synchronization

XEngine reserves two **game-local** default icon filenames:

| Platform | File in each game | Original source in BlankGame |
| --- | --- | --- |
| Windows | `assets/icons/xengine_icon.ico` | `tools/templates/BlankGame/assets/icons/xengine_icon.ico` |
| Linux | `assets/icons/xengine_icon.png` (512×512) | `tools/templates/BlankGame/assets/icons/xengine_icon.png` |

The maintainer supplies the original images. The synchronization tool **does not generate, convert, rebrand or modify the pixels**. GitHub's README uses the separate vector logo `docs/branding/xengine_logo.svg`. These files are not included in code-only patches if the maintainer has not supplied them to the patch author.

To regenerate the template ICO from the existing template PNG, run `python tools/png_to_ico.py` without arguments (or `tools\png_to_ico.bat` on Windows). This optional converter uses Pillow and replaces `xengine_icon.ico` in the template; it never changes the PNG. For an arbitrary image, use `python tools/png_to_ico.py --png "path/to/image.png"` (add `--force` to overwrite an existing ICO).

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

1. `sync_game_configs.py --game "My Game"` updates its root `config.json` against BlankGame, preserving chosen icon values (including legacy strings and `null`), project identity, build settings and custom keys. Previous manifests are backed up under `.xengine-config-backups/`.
2. `sync_engine_configs.py --game "My Game"` updates its runtime engine JSON and additional startup profiles from the canonical BlankGame schema while preserving compatible custom values and backing up changes under `.xengine-config-backups/`.
3. `sync_game_icons.py --game "My Game"` compares SHA-256 of `xengine_icon.ico` and `xengine_icon.png` with BlankGame; it copies absent files and replaces outdated reserved filenames, backing up replaced bytes under `.xengine-icon-backups/`.
4. On success, the normal compiler and linker run; a changed Windows icon causes a relink through the existing build signature mechanism.

Only the **requested game** is modified by a build. A standalone sync without `--game` updates all game directories. All three synchronizers are idempotent.

All tool output remains visible on the console (and benchmark `build.log` through its live tee). If any synchronizer or the compiler fails, the build exits nonzero: no benchmark may run a stale binary.

## Command-line synchronization

```powershell
python tools/sync_game_icons.py --check                 # Compare all games; exit 1 if changes needed
python tools/sync_game_icons.py --apply                 # Synchronize all games
python tools/sync_game_icons.py --game "My Game" --apply # Synchronize one game
python tools/sync_game_configs.py --check              # Inspect game-level config.json
python tools/sync_engine_configs.py --check             # Inspect runtime JSON
```

**Custom icons are never overwritten** when stored under other filenames such as `assets/icons/player.ico` or `assets/icons/custom.png`, even if `config.json` selects those filenames. The reserved names `xengine_icon.ico` and `xengine_icon.png` are always managed; do not use them for game-specific artwork. Older game-local `xengine_logo.ico` and `xengine_logo.png` files are no longer managed: synchronization neither overwrites nor deletes them. Explicit `config.json` paths to those older filenames still work as custom paths.

## Template artwork prerequisite

Before applying this update, confirm that your actual game icon images are named `xengine_icon.ico` and `xengine_icon.png` in `tools/templates/BlankGame/assets/icons/`. These are distinct from the README artwork `docs/branding/xengine_logo.svg`, which keeps its name. Some older project snapshots still contain template files named `xengine_logo.ico` and `xengine_logo.png`; rename **your current icon artwork** in the template if necessary, or place your updated icon artwork there, before building. The synchronization tool does not silently substitute old logo bytes for missing icons.

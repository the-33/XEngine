# Synchronizing game `config.json` manifests

XEngine has **two distinct JSON configurations**:

| File | Purpose | Synchronizer |
|---|---|---|
| `games/<Game>/config.json` | Project metadata, version, icons, build targets and shipping | `tools/sync_game_configs.py` |
| `games/<Game>/assets/config/engine.json` and other engine profiles | Runtime settings, renderer, physics and gameplay | `tools/sync_engine_configs.py` |

The canonical build-manifest template is
`tools/templates/BlankGame/config.json`. Starting in **v16.22**, every game
build runs `sync_game_configs.py --game <Game>` **before** syncing runtime
settings/icons and before compiling. Both Debug and Release, including
cross-builds, use the same safe preflight. A failed sync stops the build; it
must not silently continue with an old executable.

## What survives a template update?

The synchronizer adds **missing template keys** to existing game manifests.
Values already present with compatible types are retained, including:

- `project.name` (a new manifest generated from the template gets the actual
  game directory name; an inconsistent *existing* name remains untouched and
  the build validator will report it);
- `project.version` and version strings set independently by each game;
- `project.icon.windows` and `.linux`, including customized paths and explicit
  `null`, plus the legacy `project.icon` string or `null` supported by build.py;
- `build.platform`, `build.architecture` and `build.configuration` (Debug or Release);
- `defines`, `shipping.copy_runtime_assets` and game-specific extra keys.

If an existing value has an **incompatible type**, the canonical template
value replaces it after a byte-exact backup; the `[TYPE]` warning names
the affected field. It does not auto-correct valid project-specific choices.
Unknown keys are **preserved** unless you explicitly choose
`--prune-unknown`.

When a key formerly owned by the template is removed from the template, its
historical path is retained in the tracked
`tools/game_config_schema_state.json` manifest and that obsolete key can be
pruned from game files. This history must not be edited as if it were a list
of current defaults.

## Commands

From the XEngine root, using Python (`python3` on Linux):

```powershell
# Preview pending schema updates without changing any files; exit 1 if needed.
python tools/sync_game_configs.py --check

# Apply to every existing game (backups created before any edit).
python tools/sync_game_configs.py

# Apply to just one game.
python tools/sync_game_configs.py --game "My Princess Run"

# Place the backups in a different directory, if desired.
python tools/sync_game_configs.py --game "My Princess Run" --backup-dir "C:\XEngineBackups"
```

The Linux equivalents use `python3` and Unix-style paths, e.g.:

```bash
python3 tools/sync_game_configs.py --check
python3 tools/sync_game_configs.py --game 'XEngine Benchmark' --backup-dir /tmp/xengine-config-backups
```

The default backups are placed under `.xengine-config-backups/` (ignored by
Git). This is a separate backup, not an extra manifest inside a game.
Synchronizing one game is idempotent: unchanged files are not rewritten.
All input manifests are parsed and validated **before** modifying any files;
a malformed manifest halts the entire operation. If an atomic write later
fails, already-written files are restored on a best-effort basis from the
recorded original bytes. Backups are not removed by the synchronizer.

## Integration with `tools/build.py`

The pre-build order is:

1. `[SYNC] sync_game_configs.py` — update the game `config.json` schema.
2. `[SYNC] sync_engine_configs.py` — update engine runtime JSONs.
3. `[SYNC] sync_game_icons.py` — synchronize reserved default icon artwork.
4. Compile and link with the project-specific settings that remain in place.

The new synchronizer does not change any compiler flags itself; it maintains
the JSON keys that `build.py` reads. The three synchronization steps run for
every game build, not for a standalone engine-library build.

Tests: `python -m unittest discover -s tests -p test_sync_game_configs.py -v`.
They cover custom values, legacy/disabled icons, schema addition/removal,
read-only checks, backups, rollback, invalid input and build integration.

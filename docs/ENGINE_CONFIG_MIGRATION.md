# Synchronizing XEngine runtime configurations

`tools/sync_engine_configs.py` keeps the runtime JSON files of existing games in sync with the **canonical** configuration at:

```
tools/templates/BlankGame/assets/config/engine.json
```

No hand-maintained list of current settings is required. **Every game build now invokes the synchronization automatically before any engine or game compilation**, limited to that game with `--game <Name>`. To inspect or reconcile all games manually, run the script from the repository root:

```bash
python tools/sync_engine_configs.py
```

The script applies updates to all `games/<Name>/assets/config/engine.json` files and to other **engine startup** JSON profiles in the same directories (such as the benchmark's `single.json`, `collision.json`, and `parallel.json`). Other JSON files are ignored unless they look like engine configuration profiles. A missing `engine.json` is created.

## What is preserved?

- **Existing values take precedence** whenever a setting still exists in the template and its JSON type is compatible. This applies to engine multithreading, renderer settings, resolutions, gravity, asset paths, UI colors, game titles, and benchmark-specific mode flags.
- **New settings** are added with the default from the template. `{{PROJECT_NAME}}` placeholders in newly added values are replaced with the game directory name.
- **Settings removed from the template** are removed automatically *only when the script recognizes them as former template-managed keys*.
- **Game-specific extensions** that were never part of the template are retained, even if the current template has no matching key.
- A setting whose JSON type no longer matches the template is reset to the new default; the exact original file is backed up.
- JSON arrays are treated as whole values, not as independently synchronized elements; numeric JSON values can remain integers or floating-point numbers.

The small tracked file `tools/engine_config_schema_state.json` is updated automatically. It stores the **historical paths** previously supplied by the template, not copies of users' game settings. This allows future runs to recognize keys removed from the template without mistaking game-specific extensions for obsolete engine settings. Commit this file together with template changes after running the script. Do not edit it by hand.

> If a setting is **renamed**, there is no safe way to infer that the new name is the same concept from the template alone. The old setting is retired and the new setting receives its default. In that case, move game-specific values to the new key deliberately before or after migration; original JSON bytes remain in the backup. A change in type or meaning likewise merits manual review.

## Commands

```bash
# Preview changes without touching files (exit code 1 when changes are needed)
python tools/sync_engine_configs.py --check

# Synchronize all games and runtime profiles, with backups
python tools/sync_engine_configs.py

# Synchronize only one game, including its alternative startup profiles
python tools/sync_engine_configs.py --game "XEngine Benchmark"

# Synchronize only files literally named engine.json
python tools/sync_engine_configs.py --engine-only

# Explicit destructive cleanup: remove ALL keys absent from the current
# template, including game-specific extensions. Review --check first.
python tools/sync_engine_configs.py --check --prune-unknown
python tools/sync_engine_configs.py --prune-unknown
```

Use repeated `--game` arguments to target multiple games. The path to the template is resolved from the script location, so the working directory does not matter.

### Backups and failure handling

Whenever modifications are necessary, the tool saves untouched original bytes under `.xengine-config-backups/<UTC timestamp>/games/...`, preserving original relative paths. It backs up the historical manifest as well when it changes. Backups are excluded from Git and from game assets. An unreadable JSON, malformed template, or duplicate key aborts planning **before** any changes are written. File writes use atomic replacement, and errors trigger a best-effort rollback; the backup is kept. Running the script again without changes is idempotent and does not produce an additional backup.

### Scope

This applies to **runtime** `assets/config/*.json` engine profiles, not the unrelated game build configuration `games/<Name>/config.json` or machine-local `tools/toolchains.local.json`.

### Example

If you add:

```json
"Collision": {
  "broadphase": "bruteforce",
  "cellSize": 2.0
}
```

to the template, a game with a pre-existing customized `Collision.cellSize` of `4.0` retains `4.0`. A game without the section receives `bruteforce` and `2.0`.

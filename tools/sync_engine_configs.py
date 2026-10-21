#!/usr/bin/env python3
"""Synchronize all game runtime JSON files with BlankGame's engine.json.

The template is the only source of current keys/defaults. A small tracked
historical manifest identifies former template-managed keys, so a removed
setting can be deleted automatically without deleting game-only extensions.
No third-party packages are required (Python >= 3.9).
"""
from __future__ import annotations

import argparse
import copy
import datetime as dt
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE_REL = Path('tools/templates/BlankGame/assets/config/engine.json')
STATE_REL = Path('tools/engine_config_schema_state.json')
BACKUP_DIR = '.xengine-config-backups'


class ConfigError(ValueError):
    """The template, manifest, or an individual configuration is invalid."""


def _unique_pairs(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ConfigError(f'Duplicate JSON key: {key!r}')
        value[key] = item
    return value


def read_json(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding='utf-8-sig'),
                          object_pairs_hook=_unique_pairs)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ConfigError(f'{path}: {exc}') from exc
    if not isinstance(data, dict):
        raise ConfigError(f'{path}: expected a JSON object at the root')
    return data


def leaf_paths(tree: dict, prefix=()) -> set[tuple[str, ...]]:
    result = set()
    for key, value in tree.items():
        path = (*prefix, key)
        if isinstance(value, dict) and value:
            result.update(leaf_paths(value, path))
        else:
            result.add(path)
    return result


def read_state(path: Path, current_paths: set[tuple[str, ...]]):
    if not path.is_file():
        # Older checkouts: no historical information means unknown keys are
        # preserved, never guessed to be obsolete.
        return set(current_paths), False
    state = read_json(path)
    if state.get('version') != 1 or not isinstance(state.get('managedLeaves'), list):
        raise ConfigError(f'{path}: unsupported managed-key manifest format')
    result = set()
    for item in state['managedLeaves']:
        if not isinstance(item, list) or not item or any(not isinstance(k, str) for k in item):
            raise ConfigError(f'{path}: malformed managedLeaves entry: {item!r}')
        result.add(tuple(item))
    return result, True


def encode_state(paths: set[tuple[str, ...]]) -> dict:
    return {'version': 1, 'managedLeaves': [list(path) for path in sorted(paths)]}


def _remove_obsolete(document: dict, path: tuple[str, ...]) -> bool:
    """Remove a formerly managed leaf; prune empty parents, keep custom siblings."""
    chain = [document]
    for key in path[:-1]:
        parent = chain[-1]
        if not isinstance(parent, dict) or not isinstance(parent.get(key), dict):
            return False
        chain.append(parent[key])
    parent = chain[-1]
    if path[-1] not in parent:
        return False
    del parent[path[-1]]
    for depth in range(len(chain) - 1, 0, -1):
        if chain[depth]:
            break
        del chain[depth - 1][path[depth - 1]]
    return True


def _compatible(actual: Any, default: Any) -> bool:
    # Collision.cellSize supports both a numeric fixed width and the literal
    # "auto". Preserve existing valid numeric values when the template changes
    # to "auto" instead of overwriting a game's carefully tuned cell size.
    if isinstance(default, bool):
        return isinstance(actual, bool)
    if isinstance(default, (float, int)) and not isinstance(default, bool):
        return isinstance(actual, (float, int)) and not isinstance(actual, bool)
    return type(actual) is type(default)


def _substitute(value: Any, name: str) -> Any:
    if isinstance(value, str):
        return value.replace('{{PROJECT_NAME}}', name)
    if isinstance(value, list):
        return [_substitute(v, name) for v in value]
    if isinstance(value, dict):
        return {k: _substitute(v, name) for k, v in value.items()}
    return copy.deepcopy(value)


def merge_config(existing: dict, template: dict, name: str,
                 known_paths: set[tuple[str, ...]],
                 *, prune_unknown: bool = False):
    """Return (document, added, removed, reset, preserved_extra) leaf paths.

    Values of keys shared with the template survive. For incompatible types,
    prefer the valid template default and preserve the old bytes in a backup.
    """
    current = leaf_paths(template)
    old = copy.deepcopy(existing)
    removed = []
    for path in sorted(known_paths - current, key=lambda x: (-len(x), x)):
        if _remove_obsolete(old, path):
            removed.append(path)

    added, reset, extras = [], [], []

    def merge(source: dict, defaults: dict, prefix=()):
        output = {}
        for key, value in defaults.items():
            path = (*prefix, key)
            if key not in source:
                output[key] = _substitute(value, name)
                added.extend(sorted(leaf_paths({key: value}, prefix)))
            elif isinstance(value, dict) and isinstance(source[key], dict):
                output[key] = merge(source[key], value, path)
            elif (path == ('Collision', 'cellSize') and value == 'auto' and
                  (source[key] == 'auto' or
                   (isinstance(source[key], (int, float)) and
                    not isinstance(source[key], bool) and
                    0.05 <= source[key] <= 4096.0))):
                output[key] = copy.deepcopy(source[key])
            elif _compatible(source[key], value):
                output[key] = copy.deepcopy(source[key])
            else:
                output[key] = _substitute(value, name)
                reset.append(path)
        for key, value in source.items():
            if key not in defaults:
                path = (*prefix, key)
                if prune_unknown:
                    removed.extend(sorted(leaf_paths({key: value}, prefix)))
                else:
                    output[key] = copy.deepcopy(value)
                    extras.extend(sorted(leaf_paths({key: value}, prefix)))
        return output

    updated = merge(old, template)
    return updated, added, removed, reset, extras


def discover_games(root: Path, names: list[str] | None):
    games_dir = root / 'games'
    if not games_dir.is_dir():
        raise ConfigError(f'Games directory not found: {games_dir}')
    if names:
        result = []
        for name in dict.fromkeys(names):
            game = games_dir / name
            if name in ('.', '..') or game.parent != games_dir or not game.is_dir():
                raise ConfigError(f'Game not found: {name}')
            result.append(game)
        return result
    return sorted((p for p in games_dir.iterdir()
                   if p.is_dir() and (p / 'config.json').is_file()),
                  key=lambda p: p.name.casefold())


def discover_configs(game: Path, *, engine_only: bool):
    config_dir = game / 'assets' / 'config'
    main = config_dir / 'engine.json'
    if engine_only:
        return [main]
    result = [main]
    if config_dir.is_dir():
        for p in sorted(config_dir.glob('*.json')):
            if p.name == 'engine.json':
                continue
            candidate = read_json(p)
            # Only other engine startup profiles; do not rewrite arbitrary game
            # config JSON living in the same directory.
            if sum(key in candidate for key in ('Engine', 'Window', 'Renderer', 'Physics')) >= 2:
                result.append(p)
    return result


def _write_atomic(path: Path, data: bytes):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f'.{path.name}.', suffix='.tmp', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temp_name, path)
    finally:
        if os.path.exists(temp_name):
            os.unlink(temp_name)


def _json_bytes(doc: dict) -> bytes:
    return (json.dumps(doc, ensure_ascii=False, indent=2, allow_nan=False) + '\n').encode('utf-8')


def sync(root: Path, names=None, *, check=False, prune_unknown=False,
         engine_only=False, backup_dir: Path | None = None, quiet=False) -> int:
    template = read_json(root / TEMPLATE_REL)
    if not template:
        raise ConfigError('The template engine.json cannot be empty')
    paths = leaf_paths(template)
    state_path = root / STATE_REL
    known, state_exists = read_state(state_path, paths)
    managed = known | paths
    outdated = known - paths
    games = discover_games(root, names)
    if not games:
        raise ConfigError('No games to process')

    # Plan everything before changing any file. A malformed input must never
    # result in half of the repository being silently migrated.
    changes = []
    for game in games:
        for config_path in discover_configs(game, engine_only=engine_only):
            existing = read_json(config_path) if config_path.exists() else {}
            result, added, removed, reset, extras = merge_config(
                existing, template, game.name, known, prune_unknown=prune_unknown)
            changed = existing != result or not config_path.exists()
            status = 'UPDATE' if changed else 'OK'
            if not quiet:
                print(f'[{status}] {config_path.relative_to(root)} | '
                      f'+{len(added)} -{len(removed)} reset={len(reset)} '
                      f'custom={len(extras)}')
                for path in reset:
                    print(f'  [TYPE] {".".join(path)} -> template default (old value backed up)')
            if changed:
                changes.append((config_path, _json_bytes(result)))

    state_content = encode_state(managed)
    needs_state = not state_exists or read_json(state_path) != state_content
    if not quiet:
        if outdated:
            print(f'[SCHEMA] {len(outdated)} former template keys tracked for removal')
        print(f'[SUMMARY] {len(changes)} configuration(s) to update; '
              f'schema manifest {"to update" if needs_state else "unchanged"}')
    if check:
        return 1 if changes or needs_state else 0
    if not changes and not needs_state:
        return 0

    backup_root = backup_dir or root / BACKUP_DIR
    stamp = dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    destination = backup_root / stamp
    # Save byte-for-byte copies of each modified existing file and manifest.
    original = {}
    for path, _ in changes:
        original[path] = path.read_bytes() if path.exists() else None
    if needs_state:
        original[state_path] = state_path.read_bytes() if state_path.exists() else None
    for path, contents in original.items():
        if contents is not None:
            relative = path.relative_to(root)
            archive = destination / relative
            archive.parent.mkdir(parents=True, exist_ok=True)
            archive.write_bytes(contents)
    if not quiet:
        print(f'[BACKUP] {destination}')

    written = []
    try:
        for path, data in changes:
            _write_atomic(path, data)
            written.append(path)
        if needs_state:
            _write_atomic(state_path, _json_bytes(state_content))
            written.append(state_path)
    except OSError:
        # Best-effort restore original bytes. Originals remain in the backup.
        for path in reversed(written):
            before = original[path]
            if before is None:
                path.unlink(missing_ok=True)
            else:
                _write_atomic(path, before)
        raise
    if not quiet:
        print(f'[OK] Updated {len(changes)} configuration(s). '
              'Custom values preserved; backup available.')
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Synchronize game configs from BlankGame engine.json while preserving per-game values.')
    parser.add_argument('--check', '--dry-run', action='store_true',
                        help='Preview without writing. Exit 1 if migration is needed.')
    parser.add_argument('--game', action='append', metavar='NAME',
                        help='Only migrate this game (repeatable); default: all games.')
    parser.add_argument('--engine-only', action='store_true',
                        help='Skip additional startup profiles such as benchmark single.json.')
    parser.add_argument('--prune-unknown', action='store_true',
                        help='Also remove custom keys never known to the template (destructive).')
    parser.add_argument('--backup-dir', type=Path,
                        help='Custom destination for timestamped backups.')
    parser.add_argument('--root', type=Path, default=ROOT, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    try:
        return sync(args.root.resolve(), args.game, check=args.check,
                    prune_unknown=args.prune_unknown,
                    engine_only=args.engine_only, backup_dir=args.backup_dir)
    except (ConfigError, OSError, ValueError) as exc:
        print(f'[ERROR] {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())

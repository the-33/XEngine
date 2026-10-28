#!/usr/bin/env python3
"""Synchronize game-level config.json manifests with BlankGame/config.json.

Only the template owns the schema. Existing compatible values and custom
fields survive, including legacy project.icon strings/null and build choices.
Removed former template leaves are pruned using a tracked historical manifest.
Writes are atomic, with byte-exact backups and best-effort rollback on failure.
"""
from __future__ import annotations

import argparse
import copy
import datetime as dt
import os
from pathlib import Path
import sys

import sync_engine_configs as common


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE_REL = Path('tools/templates/BlankGame/config.json')
STATE_REL = Path('tools/game_config_schema_state.json')
BACKUP_DIR = '.xengine-config-backups'


def merge_game_config(existing: dict, template: dict, name: str,
                      known_paths: set[tuple[str, ...]], *,
                      prune_unknown: bool = False):
    """Merge template keys while preserving all intentional game-local values.

    Legacy manifests may express project.icon as a string or JSON null; these
    are valid in tools/build.py even though the modern template uses an object.
    """
    result, added, removed, reset, extras = common.merge_config(
        existing, template, name, known_paths, prune_unknown=prune_unknown)
    project = existing.get('project')
    icon_template = template.get('project', {}).get('icon')
    if isinstance(project, dict) and 'icon' in project and isinstance(icon_template, dict):
        icon = project['icon']
        if icon is None or isinstance(icon, str):
            result['project']['icon'] = copy.deepcopy(icon)
            reset = [path for path in reset if path != ('project', 'icon')]
        elif isinstance(icon, dict):
            # Per-OS null explicitly disables that icon; build.py accepts it.
            for platform in icon_template:
                if platform in icon and icon[platform] is None:
                    result['project']['icon'][platform] = None
                    reset = [path for path in reset
                             if path != ('project', 'icon', platform)]
    return result, added, removed, reset, extras


def discover_games(root: Path, names: list[str] | None) -> list[Path]:
    """Allow a named new game without config.json so the template can create it."""
    games_dir = root / 'games'
    if not games_dir.is_dir():
        raise common.ConfigError(f'Games directory not found: {games_dir}')
    if names:
        games = []
        for name in dict.fromkeys(names):
            game = games_dir / name
            if (name in ('.', '..') or game.parent != games_dir or
                not game.is_dir() or game.is_symlink()):
                raise common.ConfigError(f'Game not found: {name}')
            games.append(game)
        return games
    return sorted((p for p in games_dir.iterdir()
                   if p.is_dir() and not p.is_symlink() and
                   (p / 'config.json').is_file()), key=lambda p: p.name.casefold())


def sync(root: Path, names: list[str] | None = None, *,
         check: bool = False, prune_unknown: bool = False,
         backup_dir: Path | None = None, quiet: bool = False) -> int:
    template = common.read_json(root / TEMPLATE_REL)
    if not template or not isinstance(template.get('project'), dict):
        raise common.ConfigError('BlankGame/config.json needs a project object')
    paths = common.leaf_paths(template)
    state_path = root / STATE_REL
    known, exists = common.read_state(state_path, paths)
    managed = known | paths
    games = discover_games(root, names)
    if not games:
        raise common.ConfigError('No games to synchronize')

    # Validate/plan every input before writing even one of the game files.
    changes: list[tuple[Path, bytes]] = []
    for game in games:
        config = game / 'config.json'
        if config.is_symlink():
            raise common.ConfigError(f'Refusing symbolic-link config: {config}')
        existing = common.read_json(config) if config.is_file() else {}
        merged, added, removed, reset, extras = merge_game_config(
            existing, template, game.name, known, prune_unknown=prune_unknown)
        if not isinstance(merged.get('project'), dict):
            raise common.ConfigError(f'Invalid merged project config: {config}')
        changed = existing != merged or not config.exists()
        if not quiet:
            label = 'UPDATE' if changed else 'OK'
            print(f'[{label}] {config.relative_to(root)} | +{len(added)} '
                  f'-{len(removed)} reset={len(reset)} custom={len(extras)}')
            for path in reset:
                print(f'  [TYPE] {".".join(path)} -> template default (backed up)')
        if changed:
            changes.append((config, common._json_bytes(merged)))

    state_contents = common.encode_state(managed)
    needs_state = not exists or common.read_json(state_path) != state_contents
    if not quiet:
        if known - paths:
            print(f'[SCHEMA] {len(known - paths)} historical key(s) for removal')
        print(f'[SUMMARY] {len(changes)} config.json file(s) to update; '
              f'schema manifest {"to update" if needs_state else "unchanged"}')
    if check:
        return 1 if changes or needs_state else 0
    if not changes and not needs_state:
        return 0

    backup_root = backup_dir or root / BACKUP_DIR
    destination = backup_root / dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    originals: dict[Path, bytes | None] = {}
    for config, _ in changes:
        originals[config] = config.read_bytes() if config.is_file() else None
    if needs_state:
        originals[state_path] = state_path.read_bytes() if exists else None
    for path, previous in originals.items():
        if previous is not None:
            backup = destination / path.relative_to(root)
            backup.parent.mkdir(parents=True, exist_ok=True)
            backup.write_bytes(previous)
    if not quiet:
        print(f'[BACKUP] {destination}')

    written: list[Path] = []
    try:
        for config, data in changes:
            common._write_atomic(config, data)
            written.append(config)
        if needs_state:
            common._write_atomic(state_path, common._json_bytes(state_contents))
            written.append(state_path)
    except OSError:
        for path in reversed(written):
            previous = originals[path]
            if previous is None:
                path.unlink(missing_ok=True)
            else:
                common._write_atomic(path, previous)
        raise
    if not quiet:
        print(f'[OK] Updated {len(changes)} game config.json file(s); '
              'game-specific settings preserved.')
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', '--dry-run', action='store_true',
                        help='Report required updates without writing; returns 1 when out of sync')
    parser.add_argument('--game', action='append', metavar='NAME',
                        help='Synchronize just this game; repeatable; default: every game')
    parser.add_argument('--prune-unknown', action='store_true',
                        help='Also remove custom never-template keys (normally preserve them)')
    parser.add_argument('--backup-dir', type=Path,
                        help='Directory for backups; defaults to .xengine-config-backups')
    parser.add_argument('--root', type=Path, default=ROOT, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    try:
        return sync(args.root.resolve(), args.game, check=args.check,
                    prune_unknown=args.prune_unknown, backup_dir=args.backup_dir)
    except (common.ConfigError, OSError, ValueError) as exc:
        print(f'[ERROR] {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())

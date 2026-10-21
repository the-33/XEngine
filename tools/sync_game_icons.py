#!/usr/bin/env python3
"""Synchronize the two reserved XEngine default icons with BlankGame.

Only assets/icons/xengine_icon.ico and assets/icons/xengine_icon.png are
managed. All other filenames, including explicitly configured custom icons,
are left untouched. No imaging libraries are required.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import os
from pathlib import Path
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = Path('tools/templates/BlankGame/assets/icons')
ICON_NAMES = ('xengine_icon.ico', 'xengine_icon.png')
BACKUPS = '.xengine-icon-backups'


class IconSyncError(ValueError):
    pass


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _verify(name: str, data: bytes) -> None:
    if name.endswith('.ico'):
        if len(data) < 22 or data[:4] != b'\x00\x00\x01\x00' or int.from_bytes(data[4:6], 'little') < 1:
            raise IconSyncError(f'Invalid ICO: {name}')
    else:
        if (len(data) < 33 or data[:8] != b'\x89PNG\r\n\x1a\n' or data[12:16] != b'IHDR'
                or int.from_bytes(data[16:20], 'big') != 512
                or int.from_bytes(data[20:24], 'big') != 512):
            raise IconSyncError(f'{name}: the default Linux icon must be a valid 512x512 PNG')


def _games(root: Path, names: list[str] | None):
    directory = root / 'games'
    if not directory.is_dir():
        raise IconSyncError(f'Games directory not found: {directory}')
    if names:
        found = []
        for name in dict.fromkeys(names):
            path = directory / name
            if name in ('.', '..') or path.parent != directory or not (path / 'config.json').is_file() or path.is_symlink():
                raise IconSyncError(f'Game not found: {name}')
            found.append(path)
        return found
    return sorted((p for p in directory.iterdir() if p.is_dir() and not p.is_symlink()
                   and (p / 'config.json').is_file()), key=lambda p: p.name.casefold())


def _atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp = tempfile.mkstemp(prefix=f'.{path.name}.', suffix='.tmp', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, path)
    finally:
        if os.path.exists(temp):
            os.unlink(temp)


def sync(root: Path, names: list[str] | None = None, *, check: bool = False,
         quiet: bool = False, backup_dir: Path | None = None) -> int:
    root = root.resolve()
    sources: dict[str, bytes] = {}
    for name in ICON_NAMES:
        source = root / TEMPLATE / name
        if source.is_symlink() or not source.is_file():
            raise IconSyncError(f'Default icon missing: {source}. Add your own xengine_icon files to BlankGame first.')
        content = source.read_bytes()
        _verify(name, content)
        sources[name] = content
    changes: list[tuple[Path, bytes, bytes | None]] = []
    for game in _games(root, names):
        icon_dir = game / 'assets' / 'icons'
        if icon_dir.is_symlink() or not icon_dir.resolve().is_relative_to(game.resolve()):
            raise IconSyncError(f'Refusing icon directory outside the game: {icon_dir}')
        for name, data in sources.items():
            target = icon_dir / name
            if target.is_symlink():
                raise IconSyncError(f'Refusing to overwrite symlink: {target}')
            if target.exists() and not target.is_file():
                raise IconSyncError(f'Icon path is not a file: {target}')
            previous = target.read_bytes() if target.exists() else None
            if previous is None or _sha256(previous) != _sha256(data):
                changes.append((target, data, previous))
                if not quiet:
                    print(f'[ICON] {"MISSING" if previous is None else "OUTDATED"} | {target.relative_to(root)}', flush=True)
            elif not quiet:
                print(f'[ICON] OK | {target.relative_to(root)}', flush=True)
    if not quiet:
        print(f'[ICON] {len(changes)} default icon(s) to {"update" if not check else "synchronize"}', flush=True)
    if check:
        return 1 if changes else 0
    if not changes:
        return 0
    backup_root = backup_dir or (root / BACKUPS)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup_path = backup_root / stamp
    # Save *all* originals before writing anything. Never modify custom icons.
    for path, _, previous in changes:
        if previous is not None:
            backup = backup_path / path.relative_to(root)
            backup.parent.mkdir(parents=True, exist_ok=True)
            backup.write_bytes(previous)
    written: list[tuple[Path, bytes | None]] = []
    try:
        for path, data, previous in changes:
            _atomic_write(path, data)
            written.append((path, previous))
    except OSError:
        for path, previous in reversed(written):
            if previous is None:
                path.unlink(missing_ok=True)
            else:
                _atomic_write(path, previous)
        raise
    if not quiet:
        print(f'[ICON] Synchronized {len(changes)} file(s); prior versions backed up in {backup_path}', flush=True)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', action='append', help='Sync one game (repeatable). Default: all games.')
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--check', '--dry-run', action='store_true', help='Only report differences; exit 1 if changes are needed.')
    modes.add_argument('--apply', action='store_true', help='Apply changes (also the default).')
    parser.add_argument('--backup-dir', type=Path)
    parser.add_argument('--root', type=Path, default=ROOT, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    try:
        return sync(args.root, args.game, check=args.check, backup_dir=args.backup_dir)
    except (IconSyncError, OSError) as exc:
        print(f'[ICON ERROR] {exc}', file=sys.stderr, flush=True)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())

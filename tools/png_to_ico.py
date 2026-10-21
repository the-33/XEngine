#!/usr/bin/env python3
"""Convert a game's PNG icon to a multiresolution Windows ICO.

Usage:
  python tools/png_to_ico.py "Blob's Quest" xengine
  python tools/png_to_ico.py "My Princess Run" princess --set-config
  python tools/png_to_ico.py "My Princess Run" game --png textures/logo.png

Pillow is required ONLY for this optional artwork conversion tool, not XEngine builds.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
ICO_SIZES = (16, 24, 32, 48, 64, 128, 256)
WINDOWS_RESERVED = {
    'CON', 'PRN', 'AUX', 'NUL',
    *(f'COM{n}' for n in range(1, 10)),
    *(f'LPT{n}' for n in range(1, 10)),
}


class IconError(Exception):
    pass


def safe_filename(name: str) -> str:
    """Accept names with spaces/apostrophes, but never paths or Windows metacharacters."""
    if name.lower().endswith(('.png', '.ico')):
        name = name[:-4]
    if (not name or name != name.strip() or name.endswith('.')
            or any(ord(c) < 32 or c in r'<>:"/\\|?*' for c in name)
            or name.upper().split('.')[0] in WINDOWS_RESERVED):
        raise IconError('Invalid icon name; provide only a filename without directories, e.g. princess')
    return name


def game_path(name: str) -> Path:
    if (not name or name != name.strip()
            or any(ord(c) < 32 or c in r'<>:"/\\|?*' for c in name)
            or name.upper().split('.')[0] in WINDOWS_RESERVED):
        raise IconError('Invalid game name; use the project folder name, e.g. "My Princess Run"')
    root = (ROOT / 'games').resolve()
    game = (root / name).resolve()
    if not game.is_relative_to(root) or not game.is_dir():
        raise IconError(f'Game not found: {game}\nAvailable games are located under: {root}')
    return game


def source_png(assets: Path, relative: str) -> Path:
    # An explicit PNG is interpreted relative to the game's assets directory.
    portable = relative.replace('\\', '/')
    if (not portable or portable.startswith('/') or ':' in portable
            or not portable.lower().endswith('.png')):
        raise IconError('--png must be a relative PNG path under the game\'s assets/, e.g. icons/game.png')
    src = (assets / portable).resolve()
    if not src.is_relative_to(assets.resolve()):
        raise IconError('PNG path must stay inside the game assets/ directory')
    if not src.is_file():
        raise IconError(f'PNG source not found: {src}')
    return src


def convert_png(src: Path, dest: Path, force: bool = False) -> None:
    if dest.exists() and not force:
        raise IconError(f'ICO already exists: {dest}\nUse --force to replace it.')
    try:
        from PIL import Image, ImageOps, UnidentifiedImageError
    except ImportError as exc:
        raise IconError('Pillow is required to convert PNG to ICO.\n'
                        'Install once with: py -3 -m pip install Pillow  (Windows)\n'
                        'or: python3 -m pip install Pillow  (Linux, preferably in a venv)') from exc

    try:
        with Image.open(src) as im:
            if im.format != 'PNG':
                raise IconError(f'The source is not an actual PNG file: {src}')
            im.load()
            rgba = im.convert('RGBA')
    except (OSError, ValueError, UnidentifiedImageError) as exc:
        raise IconError(f'Cannot read the PNG image: {src}\n{exc}') from exc

    # Fit inside the square without stretching if the original image is rectangular.
    artwork = ImageOps.contain(rgba, (256, 256), method=Image.Resampling.LANCZOS)
    icon = Image.new('RGBA', (256, 256), (0, 0, 0, 0))
    icon.alpha_composite(artwork, ((256 - artwork.width) // 2,
                                   (256 - artwork.height) // 2))

    dest.parent.mkdir(parents=True, exist_ok=True)
    fd, tempname = tempfile.mkstemp(prefix='.xengine_ico_', suffix='.ico', dir=dest.parent)
    os.close(fd)
    temp = Path(tempname)
    try:
        icon.save(temp, format='ICO', sizes=[(s, s) for s in ICO_SIZES])
        os.replace(temp, dest)
    finally:
        if temp.exists():
            temp.unlink()


def update_config(game: Path, windows_ico: Path, linux_png: Path) -> None:
    config = game / 'config.json'
    if not config.is_file():
        raise IconError(f'config.json not found: {config}')
    try:
        data = json.loads(config.read_text(encoding='utf-8-sig'))
    except (OSError, json.JSONDecodeError) as exc:
        raise IconError(f'Cannot read game config.json: {exc}') from exc
    if not isinstance(data, dict) or not isinstance(data.get('project'), dict):
        raise IconError('config.json must have a "project" object')
    # Paths in XEngine config.json are relative to the game project directory.
    data['project']['icon'] = {
        'windows': windows_ico.relative_to(game).as_posix(),
        'linux': linux_png.relative_to(game).as_posix(),
    }
    fd, tempname = tempfile.mkstemp(prefix='.xengine_config_', suffix='.json', dir=game)
    os.close(fd)
    temp = Path(tempname)
    try:
        temp.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        os.replace(temp, config)
    finally:
        if temp.exists():
            temp.unlink()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description='Convert games/<game>/assets/icons/<icon>.png to a multiresolution Windows .ico'
    )
    parser.add_argument('game', help='Game folder name, e.g. "Blob\'s Quest"')
    parser.add_argument('icon', help='Icon name without extension, e.g. princess')
    parser.add_argument('--png', help='Alternative source PNG path relative to assets/, e.g. textures/logo.png')
    parser.add_argument('--force', action='store_true', help='Overwrite an existing .ico')
    parser.add_argument('--set-config', action='store_true',
                        help='Set project.icon.windows (ICO) and project.icon.linux (PNG) in config.json')
    args = parser.parse_args(argv)
    try:
        name = safe_filename(args.icon)
        game = game_path(args.game)
        assets = game / 'assets'
        src = source_png(assets, args.png if args.png is not None else f'icons/{name}.png')
        dest = (assets / 'icons' / f'{name}.ico').resolve()
        if not dest.is_relative_to(assets.resolve()):
            raise IconError('Icon output path must remain inside assets/')
        if args.set_config and not (game / 'config.json').is_file():
            raise IconError(f'config.json not found: {game / "config.json"}')
        convert_png(src, dest, args.force)
        print(f'[OK] Windows ICO: {dest}')
        print(f'[OK] Sizes (px): {", ".join(map(str, ICO_SIZES))}')
        print(f'[OK] Linux PNG unchanged: {src}')
        if args.set_config:
            update_config(game, dest, src)
            print(f'[OK] Updated: {game / "config.json"}')
        else:
            print('[INFO] config.json unchanged; use --set-config to set both icon paths.')
        return 0
    except (IconError, OSError) as exc:
        print(f'[ERROR] {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())

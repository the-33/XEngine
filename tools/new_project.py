#!/usr/bin/env python3
"""Create a blank project from templates without third-party Python modules."""
import sys
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
TEMPLATE=ROOT/'tools'/'templates'/'BlankGame'

# Windows and Linux allow spaces and apostrophes in directory/executable names.
# Keep a conservative cross-platform set of characters, because the project
# directory also becomes the executable name and config.json project.name.
NAME_PUNCTUATION = " _-'’"
WINDOWS_RESERVED = {
    'CON', 'PRN', 'AUX', 'NUL',
    *(f'COM{i}' for i in range(1, 10)),
    *(f'LPT{i}' for i in range(1, 10)),
}

def valid_project_name(name: str) -> bool:
    return (
        1 <= len(name) <= 64
        and name == name.strip()
        and name[0].isalpha()
        and all(c.isalnum() or c in NAME_PUNCTUATION for c in name)
        and name.upper() not in WINDOWS_RESERVED
    )

def main():
    if len(sys.argv) != 2 or not valid_project_name(sys.argv[1]):
        print('Usage: new_project "Game Name" '
              '(letters, digits, spaces, underscore, hyphen, apostrophe; '
              'start with a letter; max 64 characters)', file=sys.stderr)
        return 2
    name = sys.argv[1]
    dest=ROOT/'games'/name
    if dest.exists():
        print(f'[ERROR] Project already exists: {dest}',file=sys.stderr)
        return 1
    # Keep resource directories, but never copy Git opt-in markers from BlankGame.
    # A newly generated game is private until the developer deliberately adds
    # games/<Game Name>/.gitkeep at its root.
    # Includes the maintainer-supplied xengine_icon.ico / .png by name; their
    # template copies are the source of truth for subsequent per-game sync.
    shutil.copytree(TEMPLATE, dest, ignore=shutil.ignore_patterns('.gitkeep'))
    for f in dest.rglob('*'):
        if f.is_file() and f.suffix in {'.json','.cpp','.h','.hpp','.md','.bat','.sh'}:
            data=f.read_text(encoding='utf-8')
            if '{{PROJECT_NAME}}' in data:
                f.write_text(data.replace('{{PROJECT_NAME}}',name),encoding='utf-8')
    # Empty asset directories exist locally after copytree; no .gitkeep is
    # created automatically. Only a root-level marker opts a game into Git.
    print(f'[OK] Created games/{name}')
    print(f'    Windows: cd \"games\\{name}\"  y despues  .\\build.bat --run')
    print(f'    Linux:   cd \"games/{name}\"  y despues  ./build.sh --run')
    return 0

if __name__=='__main__':
    raise SystemExit(main())

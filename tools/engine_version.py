"""Single source of truth for the XEngine distribution version.

The root VERSION file contains a SemVer string without a leading "v". Game
versions belong to individual game config.json files and are unrelated.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
SEMVER = re.compile(
    r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)'
    r'(?:-(?:0|[1-9][0-9]*|[0-9A-Za-z-]*[A-Za-z-][0-9A-Za-z-]*)'
    r'(?:\.(?:0|[1-9][0-9]*|[0-9A-Za-z-]*[A-Za-z-][0-9A-Za-z-]*))*)?'
    r'(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?'
)


def read_engine_version(root: Path = ROOT) -> str:
    """Read and validate the official engine version, not a game's version."""
    path = root / 'VERSION'
    version = path.read_text(encoding='utf-8').strip()
    if not SEMVER.fullmatch(version):
        raise ValueError(
            f'{path}: expected MAJOR.MINOR.PATCH (optional -prerelease/+build), '
            f'without a leading v; got {version!r}'
        )
    return version


def source_archive_name(version: str, *, full: bool = True) -> str:
    """Name an engine source distribution; not a game Release package."""
    if not SEMVER.fullmatch(version):
        raise ValueError(f'Invalid XEngine SemVer: {version!r}')
    return f"XEngine-{version}-{'source' if full else 'patch'}.zip"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive-name', choices=('source', 'patch'),
                        help='print a versioned engine ZIP filename instead')
    args = parser.parse_args()
    version = read_engine_version()
    print(source_archive_name(version, full=args.archive_name == 'source')
          if args.archive_name else version)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

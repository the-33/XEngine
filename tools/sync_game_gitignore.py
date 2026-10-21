#!/usr/bin/env python3
"""Opt games into Git with a root-level .gitkeep, keeping private games local.

Only games/<Name>/.gitkeep is an opt-in marker; preexisting .gitkeep files in
assets/ are intentionally ignored. Gitignore cannot test file existence, so
this script regenerates games/.gitignore from those explicit opt-ins.

With --stage it also reconciles Git's *index*: tracked private games are
untracked without deleting any local file; opted-in games and the benchmark
are staged. It is suitable for use from the optional pre-commit hook.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GAMES = ROOT / 'games'
IGNORE = GAMES / '.gitignore'
BENCHMARK = 'XEngine Benchmark'


def game_directories() -> list[Path]:
    if not GAMES.exists():
        return []
    return sorted((p for p in GAMES.iterdir() if p.is_dir() and not p.is_symlink()),
                  key=lambda p: (p.name.casefold(), p.name))


def included_games() -> list[str]:
    return [p.name for p in game_directories()
            if p.name == BENCHMARK or (p / '.gitkeep').is_file()]


def escape_gitignore(name: str) -> str:
    """Escape a literal directory name in a slash-delimited gitignore pattern."""
    if not name or name in ('.', '..') or '/' in name or '\n' in name or '\r' in name:
        raise ValueError(f'Invalid game directory name: {name!r}')
    return ''.join('\\' + c if c in r'\\*?[]!#' else c for c in name).rstrip(' ') + ('\\ ' * (len(name) - len(name.rstrip(' '))))


def desired_ignore() -> str:
    names = sorted(set(included_games() + [BENCHMARK]), key=lambda n: (n.casefold(), n))
    return (
        '# XEngine game version-control policy. Managed by tools/sync_game_gitignore.py.\n'
        '# A game is public only with games/<Game Name>/.gitkeep at its root.\n'
        '# .gitkeep files inside assets/ do NOT enable a game.\n'
        '# Do not add private game directories manually below.\n'
        '/*/\n'
        + ''.join(f'!/{escape_gitignore(name)}/\n' for name in names)
    )


def git(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(['git', *args], cwd=ROOT, text=True,
                          capture_output=True, check=True)


def stage_games() -> None:
    try:
        actual_root = Path(git('rev-parse', '--show-toplevel').stdout.strip()).resolve()
    except (FileNotFoundError, subprocess.CalledProcessError) as exc:
        raise RuntimeError('--stage requires a Git checkout and the git command') from exc
    if actual_root != ROOT.resolve():
        raise RuntimeError('--stage requires the current XEngine directory to be the repository root')

    included = set(included_games())
    # Stage rules before adding opted-in paths so previously ignored files are visible.
    git('add', '--', 'games/.gitignore')
    for name in sorted(included):
        git('add', '-A', '--', f'games/{name}')

    # Ignoring a path does not untrack files already in Git's index.
    # --cached removes index entries only; the actual projects stay untouched.
    for folder in game_directories():
        if folder.name not in included:
            git('rm', '-r', '-q', '--cached', '--ignore-unmatch', '--', f'games/{folder.name}')

    # Also untrack formerly public game directories that are no longer on disk.
    existing = {folder.name for folder in game_directories()}
    tracked = git('ls-files', '-z', '--', 'games/').stdout
    previously_indexed = set()
    for item in tracked.split('\0'):
        parts = item.split('/')
        if len(parts) > 2:
            previously_indexed.add(parts[1])
    for name in sorted(previously_indexed - existing - included):
        git('rm', '-r', '-q', '--cached', '--ignore-unmatch', '--', f'games/{name}')


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Return 1 when games/.gitignore needs updating')
    parser.add_argument('--stage', action='store_true', help='Update the Git index, staging opted-in games and untracking private games')
    parser.add_argument('--quiet', '-q', action='store_true', help='Suppress informational output')
    args = parser.parse_args(argv)
    if args.check and args.stage:
        parser.error('--check and --stage are mutually exclusive')
    wanted = desired_ignore()
    current = IGNORE.read_text(encoding='utf-8') if IGNORE.exists() else None
    if args.check:
        if current != wanted:
            if not args.quiet:
                print('[CHECK] games/.gitignore is out of sync with root-level game markers')
            return 1
        if not args.quiet:
            print('[CHECK] games/.gitignore matches opt-in markers')
        return 0
    if current != wanted:
        GAMES.mkdir(parents=True, exist_ok=True)
        IGNORE.write_text(wanted, encoding='utf-8', newline='\n')
    if args.stage:
        try:
            stage_games()
        except (OSError, subprocess.CalledProcessError, RuntimeError) as exc:
            detail = exc.stderr if isinstance(exc, subprocess.CalledProcessError) else str(exc)
            print(f'[ERROR] Could not update Git index: {detail}', file=sys.stderr)
            return 2
    if not args.quiet:
        action = 'Staged' if args.stage else 'Configured'
        print(f'[OK] {action} games: {", ".join(included_games()) or "(none)"}')
        print('[OK] All other game folders remain local (unless already committed remotely).')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

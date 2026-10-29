#!/usr/bin/env python3
"""Opt-in VS Code IntelliSense and separately opt-in Doxygen hover extension.

Neither CMake, an editor extension nor third-party Python modules are required
to build XEngine. Generated .vscode/ and build/ files remain Git-ignored.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

import build as xbuild

ROOT = Path(__file__).resolve().parent.parent
HOVER_EXTENSION_ID = 'the-33.xengine-doxygen-hover'
HOVER_VSIX = (Path(__file__).resolve().parent / 'vscode_doxygen_hover' /
              'xengine-doxygen-hover-0.1.0.vsix')


def _code_cli(command: str = 'code') -> str:
    """Find a VS Code CLI, also accepting a path to code/code.cmd."""
    candidate = shutil.which(command)
    if candidate:
        return candidate
    if Path(command).is_file():
        return str(Path(command).resolve())
    raise xbuild.BuildError(
        f'VS Code command {command!r} not found. Install VS Code and enable its '
        'command-line launcher, or pass --code-cli with its full path '
        '(e.g. code-insiders or code.cmd).')


def _run_code_cli(*arguments: str, code_cli: str = 'code') -> str:
    executable = _code_cli(code_cli)
    result = subprocess.run([executable, *arguments], text=True,
                            capture_output=True, check=False)
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise xbuild.BuildError(
            f'VS Code CLI returned {result.returncode}: {detail or "unknown error"}')
    return result.stdout.strip()


def hover_status(*, code_cli: str = 'code') -> bool:
    """Query the selected VS Code installation without modifying anything."""
    output = _run_code_cli('--list-extensions', '--show-versions', code_cli=code_cli)
    return any(line.split('@', 1)[0].strip().casefold() == HOVER_EXTENSION_ID.casefold()
               for line in output.splitlines())


def hover_install(*, code_cli: str = 'code', vsix: Path = HOVER_VSIX) -> None:
    """Install/update only when explicitly requested by the user."""
    if not vsix.is_file():
        raise xbuild.BuildError(
            f'Bundled Doxygen Hover VSIX not found: {vsix}. '
            'Re-download the complete XEngine repository or package the extension.')
    output = _run_code_cli('--install-extension', str(vsix.resolve()), '--force',
                           code_cli=code_cli)
    if output:
        print(output)


def hover_uninstall(*, code_cli: str = 'code') -> None:
    output = _run_code_cli('--uninstall-extension', HOVER_EXTENSION_ID,
                           code_cli=code_cli)
    if output:
        print(output)


def _data_path(root: Path) -> Path:
    return root / 'build' / 'ide' / 'vscode.json'


def _database_path(root: Path) -> Path:
    return root / 'build' / 'ide' / 'compile_commands.json'


def _json(data: object) -> str:
    return json.dumps(data, indent=2, ensure_ascii=False) + '\n'


def _sha(contents: str) -> str:
    return hashlib.sha256(contents.encode('utf-8')).hexdigest()


def _games(root: Path) -> list[Path]:
    directory = root / 'games'
    if not directory.exists():
        return []
    return sorted((p for p in directory.iterdir()
                   if p.is_dir() and (p / 'config.json').is_file()),
                  key=lambda p: p.name.casefold())


def _compile_entry(*, root: Path, src: Path, base: Path, objroot: Path,
                   compiler: str, flags: list[str], cflags: list[str]) -> dict:
    try:
        rel = src.relative_to(base)
    except ValueError:
        rel = Path(src.name)  # generated GameBuild source, as in build.py
    obj = objroot / rel.with_suffix('.o')
    return {
        'directory': str(root.resolve()),
        'file': str(src.resolve()),
        'arguments': xbuild.source_compile_command(compiler, flags, cflags, src, obj),
    }


def _commands(root: Path, *, target: str, profile: str, arch: str,
              tooling: xbuild.Toolchain) -> tuple[list[dict], list[Path]]:
    common = [*xbuild.common_flags(target, profile), *tooling.cxxflags]
    engine_src = root / 'engine' / 'src'
    engine_headers = root / 'engine' / 'include'
    engine_cflags = ['-I' + str(engine_headers), *tooling.pkg_cflags]
    engine_out = root / 'build' / f'{target}-{arch}' / profile / 'engine' / 'obj'
    entries = [
        _compile_entry(root=root, src=src, base=engine_src, objroot=engine_out,
                       compiler=tooling.compiler, flags=common, cflags=engine_cflags)
        for src in xbuild.collect_sources(engine_src)
    ]
    if not entries:
        raise xbuild.BuildError('No engine .cpp files found for IntelliSense')

    games = _games(root)
    for game in games:
        config = xbuild.json_file(game / 'config.json')
        game_out = root / 'build' / f'{target}-{arch}' / profile / 'games' / game.name
        generated = xbuild.generate_game_config(game, game_out, profile,
                                                 cross=tooling.cross)
        flags = [*common, *xbuild.validate_defines(config.get('defines', []))]
        cflags = [
            '-I' + str(engine_headers),
            *('-I' + str(folder) for folder in xbuild.asset_include_dirs(game / 'assets')),
            '-I' + str(game_out / 'generated'),
            *tooling.pkg_cflags,
        ]
        entries.extend(
            _compile_entry(root=root, src=src, base=game,
                           objroot=game_out / 'obj', compiler=tooling.compiler,
                           flags=flags, cflags=cflags)
            for src in xbuild.game_sources(game, target, generated)
        )
    return entries, games


def _configuration(*, root: Path, folder: Path, target: str, profile: str,
                   arch: str, compiler: str) -> str:
    is_engine = folder == root
    here = '${workspaceFolder}'
    prefix = here if is_engine else here + '/../..'
    engine_headers = prefix + '/engine/include'
    includes = [
        engine_headers,
        prefix + '/engine/src',
        (here + '/games/**') if is_engine else (here + '/**'),
    ]
    if not is_engine:
        includes.append(prefix + '/build/' + f'{target}-{arch}/{profile}/games/' +
                        folder.name + '/generated')
    config = {
        'configurations': [{
            'name': 'XEngine (auto)',
            'compilerPath': compiler,
            'compileCommands': [prefix + '/build/ide/compile_commands.json'],
            'cppStandard': 'c++20',
            'cStandard': 'c17',
            'intelliSenseMode': ('windows-gcc-x64' if target == 'windows'
                                 else 'linux-gcc-x64'),
            'includePath': includes,
            'browse': {'path': [here, prefix + '/engine/include',
                                prefix + '/engine/src']},
        }],
        'version': 4,
    }
    return _json(config)


def _managed_config(root: Path, folder: Path, contents: str,
                    managed: dict, warnings: list[str]) -> None:
    path = folder / '.vscode' / 'c_cpp_properties.json'
    key = path.relative_to(root).as_posix()
    previous = managed.get(key)
    current = path.read_text(encoding='utf-8') if path.exists() else None
    if previous and current is not None and _sha(current) != previous['sha256']:
        # Someone edited the generated file. Never clobber personal changes.
        warnings.append(f'Skipped user-edited {key}; keep or merge it manually')
        return
    if current == contents:
        if previous:
            previous['sha256'] = _sha(contents)
        return
    backup = previous.get('backup') if previous else None
    if not previous and current is not None:
        # First setup: preserve any pre-existing user configuration.
        backup = 'build/ide/backups/' + hashlib.sha256(
            key.encode('utf-8')).hexdigest()[:24] + '.json'
        xbuild.write_if_changed(root / backup, current)
    xbuild.write_if_changed(path, contents)
    managed[key] = {'sha256': _sha(contents), 'backup': backup}


def refresh(root: Path = ROOT, *, platform: str | None = None,
            configuration: str | None = None, quiet: bool = False,
            setup: bool = False, tooling: xbuild.Toolchain | None = None) -> list[str]:
    marker = _data_path(root)
    if not setup and not marker.is_file():
        raise xbuild.BuildError('VS Code is not enabled; run python tools/vscode.py setup first')
    state = xbuild.json_file(marker) if marker.is_file() else {
        'schema': 1, 'managed': {},
    }
    if state.get('schema') != 1 or not isinstance(state.get('managed'), dict):
        raise xbuild.BuildError(f'Invalid VS Code integration marker: {marker}')
    target = xbuild.resolve_platform(platform, state.get('platform', 'auto'))
    profile = xbuild.normalize_profile(configuration or
                                       state.get('configuration', 'debug'))
    arch = xbuild.normalized_arch(state.get('arch', 'x86_64'))
    tooling = tooling or xbuild.prepare_tools(target)
    commands, games = _commands(root, target=target, profile=profile, arch=arch,
                                tooling=tooling)
    warnings: list[str] = []
    # Write through a stable, shared database. JSON arguments arrays preserve
    # spaces/apostrophes in project paths and compiler flags on Windows/Linux.
    xbuild.write_if_changed(_database_path(root), _json(commands))
    for folder in [root, *games]:
        content = _configuration(root=root, folder=folder, target=target,
                                 profile=profile, arch=arch, compiler=tooling.compiler)
        _managed_config(root, folder, content, state['managed'], warnings)
    state.update({'platform': target, 'configuration': profile, 'arch': arch})
    xbuild.write_if_changed(marker, _json(state))
    if not quiet:
        print(f'[IDE] {len(commands)} compile commands, {len(games)} games '
              f'({target}/{profile}); .vscode/ and build/ are ignored by Git')
        for warning in warnings:
            print(f'[IDE] WARNING: {warning}', file=sys.stderr)
    return warnings


def disable(root: Path = ROOT) -> list[str]:
    marker = _data_path(root)
    if not marker.is_file():
        return []
    state = xbuild.json_file(marker)
    warnings: list[str] = []
    for key, record in state.get('managed', {}).items():
        path = root / key
        # Never touch paths outside this checkout or unexpected filenames.
        if (not path.resolve().is_relative_to(root.resolve()) or
                path.name != 'c_cpp_properties.json' or
                path.parent.name != '.vscode'):
            warnings.append(f'Skipped invalid managed path: {key}')
            continue
        if not path.is_file():
            continue
        if _sha(path.read_text(encoding='utf-8')) != record['sha256']:
            warnings.append(f'Preserved user-edited {key}')
            continue
        backup = record.get('backup')
        if backup:
            backup_path = root / backup
            if (not backup_path.resolve().is_relative_to(
                    (root / 'build' / 'ide' / 'backups').resolve()) or
                    not backup_path.is_file()):
                warnings.append(f'Preserved {key}: original backup unavailable')
                continue
            xbuild.write_if_changed(path, backup_path.read_text(encoding='utf-8'))
            backup_path.unlink()
        else:
            path.unlink()
    marker.unlink()
    database = _database_path(root)
    if database.exists():
        database.unlink()
    return warnings


def main() -> int:
    parser = argparse.ArgumentParser(
        description='Optional VS Code IntelliSense integration for XEngine')
    parser.add_argument('action', choices=('setup', 'refresh', 'disable',
                                           'hover-install', 'hover-status',
                                           'hover-uninstall'))
    parser.add_argument('--platform', choices=('auto', 'windows', 'linux'))
    parser.add_argument('--configuration', choices=('Debug', 'Release'))
    parser.add_argument('--code-cli', default='code',
                        help='VS Code CLI name/path for hover commands (default: code)')
    parser.add_argument('--quiet', action='store_true')
    args = parser.parse_args()
    try:
        if args.action == 'hover-install':
            hover_install(code_cli=args.code_cli)
            if not args.quiet:
                print('[IDE] XEngine Doxygen Hover installed; reload VS Code if needed')
        elif args.action == 'hover-uninstall':
            hover_uninstall(code_cli=args.code_cli)
            if not args.quiet:
                print('[IDE] XEngine Doxygen Hover uninstalled')
        elif args.action == 'hover-status':
            installed = hover_status(code_cli=args.code_cli)
            print('[IDE] XEngine Doxygen Hover: ' +
                  ('installed' if installed else 'not installed'))
        elif args.action == 'disable':
            warnings = disable()
            if not args.quiet:
                print('[IDE] VS Code integration disabled (personal edits preserved)')
                for warning in warnings:
                    print(f'[IDE] WARNING: {warning}', file=sys.stderr)
        else:
            refresh(platform=args.platform, configuration=args.configuration,
                    quiet=args.quiet, setup=args.action == 'setup')
    except (xbuild.BuildError, OSError, ValueError, KeyError) as exc:
        print(f'[IDE] ERROR: {exc}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

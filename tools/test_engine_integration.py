#!/usr/bin/env python3
"""Compile and execute the *real* SDL2 engine integration test on the host OS.

Uses tools/build.py's native toolchain and compilation flags, never mocks SDL.
The test runs with the BlankGame template's actual PNG and TTF fixtures.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import build as native_build  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug', 'Release'), default='Debug')
    parser.add_argument('--frames', type=int, default=16,
                        help='Number of real frames; 600-2000 for a short soak run')
    parser.add_argument('--timeout', type=int, default=120,
                        help='Maximum runtime in seconds, excluding compilation')
    opts = parser.parse_args()
    if not 2 <= opts.frames <= 20000 or opts.timeout < 1:
        parser.error('--frames must be 2..20000 and --timeout must be positive')

    profile = native_build.normalize_profile(opts.config)
    target = native_build.host_platform()
    if target not in ('windows', 'linux'):
        parser.error('Native SDL integration is currently supported on Windows and Linux')

    try:
        tooling = native_build.prepare_tools(target)
        library = native_build.create_engine(target=target, profile=profile,
                                             arch='x86_64', tooling=tooling,
                                             no_doc=True)
        folder = ROOT / 'build' / f'{target}-x86_64' / profile / 'tests' / 'integration'
        source = ROOT / 'tests' / 'integration' / 'engine_smoke.cpp'
        flags = [*native_build.common_flags(target, profile), *tooling.cxxflags]
        includes = ['-I' + str(ROOT / 'engine' / 'include'), *tooling.pkg_cflags]
        objects, changed = native_build.compile_sources(
            [source], objroot=folder / 'obj', compiler=tooling.compiler,
            flags=flags, cflags=includes,
            headers=native_build.all_headers([ROOT / 'engine' / 'include']),
            compiler_version=tooling.version, base=source.parent)
        binary = folder / ('xengine_integration.exe' if target == 'windows'
                           else 'xengine_integration')
        options = [*tooling.ldflags, *tooling.pkg_libs]
        if target == 'windows':
            options.append('-ladvapi32')
        signature = native_build.link_signature(objects, library, None,
                                                tooling.compiler, options)
        signature_path = folder / 'IntegrationLink.sha256'
        if (changed or not binary.exists() or not signature_path.is_file()
                or signature_path.read_text(encoding='ascii') != signature):
            native_build.command([
                tooling.compiler, *map(str, objects), str(library), '-o', str(binary),
                *options,
            ])
            signature_path.write_text(signature, encoding='ascii')
        else:
            native_build.log('[CACHE] Native integration executable unchanged')
        if target == 'windows':
            native_build.package_windows_libs(folder, binary, tooling=tooling)
    except (native_build.BuildError, OSError) as exc:
        print(f'[INTEGRATION] Build failed: {exc}', file=sys.stderr)
        return 2

    env = os.environ.copy()
    env.setdefault('SDL_AUDIODRIVER', 'dummy')
    env.setdefault('SDL_VIDEODRIVER', 'dummy')
    env.setdefault('SDL_RENDER_DRIVER', 'software')
    asset_root = ROOT / 'tools' / 'templates' / 'BlankGame' / 'assets'
    print(f'[INTEGRATION] {target}/{opts.config}, {opts.frames} frames, '
          f'video={env["SDL_VIDEODRIVER"]}, timeout={opts.timeout}s', flush=True)
    try:
        completed = subprocess.run(
            [str(binary), str(opts.frames)], cwd=asset_root, env=env,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, errors='replace', timeout=opts.timeout, check=False)
    except subprocess.TimeoutExpired as exc:
        print(f'[INTEGRATION] FAIL: executable exceeded {opts.timeout}s timeout',
              file=sys.stderr)
        if exc.stdout:
            print(exc.stdout.decode(errors='replace') if isinstance(exc.stdout, bytes)
                  else exc.stdout, file=sys.stderr)
        return 1
    except OSError as exc:
        print(f'[INTEGRATION] FAIL: executable could not run: {exc}', file=sys.stderr)
        return 1

    print(completed.stdout, end='\n' if not completed.stdout.endswith('\n') else '')
    if completed.returncode != 0 or 'XENGINE_INTEGRATION_OK:' not in completed.stdout:
        print(f'[INTEGRATION] FAIL: exit={completed.returncode}, '
              'expected successful native test marker', file=sys.stderr)
        return 1
    print('[INTEGRATION] PASS: real SDL2 engine startup, scene lifecycle, '
          'hierarchy, asset cache, frame loop and safe shutdown')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

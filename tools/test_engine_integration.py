#!/usr/bin/env python3
"""Compile and execute the *real* SDL2 engine integration test on the host OS.

Uses tools/build.py's native toolchain and compilation flags, never mocks SDL.
The test runs with the BlankGame template's actual PNG and TTF fixtures.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import wave

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import build as native_build  # noqa: E402


def prepare_fixture_assets(source: Path, destination: Path) -> None:
    """Create isolated assets, including two valid little-endian PCM WAVs."""
    shutil.copytree(source, destination)
    audio = destination / 'audio'
    audio.mkdir()
    for name, sample_count in (('tone.wav', 2205), ('other.wav', 4410)):
        with wave.open(str(audio / name), 'wb') as wav:
            wav.setnchannels(1)
            wav.setsampwidth(2)
            wav.setframerate(22050)
            wav.writeframes(b'\0\0' * sample_count)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', choices=('Debug', 'Release'), default='Debug')
    parser.add_argument('--frames', type=int, default=16,
                        help='Number of real frames; 600-2000 for a short soak run')
    parser.add_argument('--timeout', type=int, default=120,
                        help='Maximum runtime in seconds, excluding compilation')
    parser.add_argument('--sanitizers', action='store_true',
                        help='Linux native GCC: instrument engine and test with ASan and UBSan')
    opts = parser.parse_args()
    if not 2 <= opts.frames <= 20000 or opts.timeout < 1:
        parser.error('--frames must be 2..20000 and --timeout must be positive')

    profile = native_build.normalize_profile(opts.config)
    target = native_build.host_platform()
    if target not in ('windows', 'linux'):
        parser.error('Native SDL integration is currently supported on Windows and Linux')
    if opts.sanitizers and target != 'linux':
        parser.error('--sanitizers currently requires native Linux GCC')

    try:
        tooling = native_build.prepare_tools(target)
        folder = ROOT / 'build' / f'{target}-x86_64' / profile / 'tests' / 'integration'
        sanitizer_flags = (['-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                            '-fno-sanitize-recover=all'] if opts.sanitizers else [])
        if opts.sanitizers:
            # Compiling the normal libXEngine.a with sanitizers would taint the
            # engine cache for games and the four ordinary CI jobs. Use a fully
            # isolated archive containing every instrumented engine .cpp.
            folder = folder / 'sanitized'
            engine_folder = folder / 'engine'
            engine_sources = native_build.collect_sources(native_build.ENGINE_SRC)
            engine_flags = [*native_build.common_flags(target, profile),
                            *tooling.cxxflags, *sanitizer_flags]
            engine_includes = ['-I' + str(native_build.ENGINE_HEADERS),
                               *tooling.pkg_cflags]
            engine_objects, engine_changed = native_build.compile_sources(
                engine_sources, objroot=engine_folder / 'obj',
                compiler=tooling.compiler, flags=engine_flags,
                cflags=engine_includes,
                headers=native_build.all_headers([native_build.ENGINE_HEADERS]),
                compiler_version=tooling.version, base=native_build.ENGINE_SRC)
            library = engine_folder / 'libXEngine.a'
            if engine_changed or not library.is_file():
                if library.exists():
                    library.unlink()
                native_build.command([tooling.archiver, 'rcs', str(library),
                                      *map(str, engine_objects)])
        else:
            library = native_build.create_engine(target=target, profile=profile,
                                                 arch='x86_64', tooling=tooling,
                                                 no_doc=True)
        source = ROOT / 'tests' / 'integration' / 'engine_smoke.cpp'
        flags = [*native_build.common_flags(target, profile),
                 *tooling.cxxflags, *sanitizer_flags]
        includes = ['-I' + str(ROOT / 'engine' / 'include'), *tooling.pkg_cflags]
        objects, changed = native_build.compile_sources(
            [source], objroot=folder / 'obj', compiler=tooling.compiler,
            flags=flags, cflags=includes,
            headers=native_build.all_headers([ROOT / 'engine' / 'include']),
            compiler_version=tooling.version, base=source.parent)
        binary = folder / ('xengine_integration.exe' if target == 'windows'
                           else 'xengine_integration')
        options = [*tooling.ldflags, *sanitizer_flags[:1], *tooling.pkg_libs]
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
    if opts.sanitizers:
        env.setdefault('ASAN_OPTIONS', 'detect_leaks=1:halt_on_error=1')
        env.setdefault('UBSAN_OPTIONS', 'print_stacktrace=1:halt_on_error=1')
    asset_root = ROOT / 'tools' / 'templates' / 'BlankGame' / 'assets'
    print(f'[INTEGRATION] {target}/{opts.config}, {opts.frames} frames, '
          f'sanitizers={opts.sanitizers}, video={env["SDL_VIDEODRIVER"]}, '
          f'timeout={opts.timeout}s', flush=True)
    try:
        # Keep the committed BlankGame template pristine. SDL_mixer gets a
        # tiny real PCM WAV, generated with the Python standard library in a
        # disposable copy of the test assets (no binary fixture in Git).
        with tempfile.TemporaryDirectory(prefix='xengine-integration-') as tmp:
            fixture_assets = Path(tmp) / 'assets'
            prepare_fixture_assets(asset_root, fixture_assets)
            completed = subprocess.run(
                [str(binary), str(opts.frames)], cwd=fixture_assets, env=env,
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
          'hierarchy, physics, input, UI, asset/audio cache and safe shutdown')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

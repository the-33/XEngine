#!/usr/bin/env python3
"""Validate a completed Release backend benchmark and install its local auto order.

The preference stays outside the repository and is never included in game
packages. At startup XEngine additionally checks the SDL/video/display/window
identity before using any of the measured priorities.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import os
from pathlib import Path
import re
import sys
import tempfile
import textwrap

from benchmark_render_backends import (MODES, WORKLOADS, VSYNC_EXCLUSION_CODE,
                                       VSYNC_EXCLUSION_MESSAGE,
                                       has_vsync_exclusion_evidence, rank_backends,
                                       stability_sort_key)
from sweep_render_selector import read_presenter

DISPLAY_MATCH = ('video_driver', 'sdl_version', 'window_width',
                 'window_height', 'display_index', 'display_refresh_hz')
REQUIRED_TAILS = ('measured_frames', 'frame_over_60hz_count', 'sdl_present_over_60hz_count',
                  'frame_over_display_count', 'sdl_present_over_display_count',
                  'frame_p999_ms', 'frame_p95_score_ms', 'frame_p99_score_ms',
                  'frame_p50_score_ms')


def destination(platform: str | None = None, environ: dict[str, str] | None = None) -> Path:
    platform = platform or sys.platform
    env = environ if environ is not None else os.environ
    if platform == 'win32':
        base = env.get('LOCALAPPDATA', '').strip()
        if not base:
            raise ValueError('LOCALAPPDATA is missing; cannot locate machine-local Windows settings')
        return Path(base) / 'XEngine' / 'render_backend_ranking.json'
    if platform.startswith('linux'):
        base = env.get('XDG_CONFIG_HOME', '').strip()
        if not base:
            home = env.get('HOME', '').strip()
            if not home:
                raise ValueError('XDG_CONFIG_HOME and HOME are missing')
            base = str(Path(home) / '.config')
        return Path(base) / 'xengine' / 'render_backend_ranking.json'
    raise ValueError(f'Unsupported platform for renderer auto ranking: {platform}')


def _read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding='utf-8-sig', newline='') as stream:
        return list(csv.DictReader(stream))


def validate_results(results: Path, *, platform: str | None = None) -> dict:
    """Fail closed on incomplete/legacy p95-only results or incompatible hosts."""
    metadata = json.loads((results / 'metadata.json').read_text(encoding='utf-8'))
    host = platform or sys.platform
    if metadata.get('platform') != host:
        raise ValueError(f'Benchmark OS {metadata.get("platform")!r} differs from host {host!r}')
    if metadata.get('ranking_valid') is not True:
        raise ValueError('Benchmark did not pass equivalence and reliability checks')
    if (metadata.get('build_config') != 'Release' or
        set(metadata.get('workloads', [])) != set(WORKLOADS) or
        metadata.get('repetitions', 0) < 3 or metadata.get('frames', 0) < 240):
        raise ValueError('Auto ranking requires full Release workloads, >=3 runs and >=240 frames')
    if 'SDL_RenderPresent' not in str(metadata.get('selection_policy', '')):
        raise ValueError('Legacy p95-only ranking: rerun the updated backend benchmark')

    rows = _read_csv(results / 'backend_ranking.csv')
    available = [str(name).lower() for name in metadata.get('candidate_backends', [])]
    names = [str(row.get('backend', '')).lower() for row in rows]
    declared_exclusions = metadata.get('excluded_backends', [])
    if (not isinstance(declared_exclusions, list) or
        any(not isinstance(name, str) for name in declared_exclusions)):
        raise ValueError('Invalid declared backend exclusions')
    declared_exclusions = [name.lower() for name in declared_exclusions]
    exclusions_path = results / 'backend_exclusions.csv'
    if declared_exclusions and not exclusions_path.is_file():
        raise ValueError('Declared backend exclusions have no evidence CSV')
    exclusions = _read_csv(exclusions_path) if exclusions_path.is_file() else []
    excluded_names = [str(item.get('backend', '')).lower() for item in exclusions]
    if (len(names) < 2 or len(names) > 32 or len(names) != len(set(names)) or
        len(available) != len(set(available)) or
        any(not re.fullmatch(r'[a-zA-Z0-9_-]{1,64}', name)
            for name in names + available + excluded_names)):
        raise ValueError('Backend ranking is incomplete, duplicated or contains unsafe names')
    if (len(excluded_names) != len(set(excluded_names)) or
        excluded_names != declared_exclusions or
        set(names) & set(excluded_names) or
        set(names) | set(excluded_names) != set(available)):
        raise ValueError('Ranked and excluded backends do not cover the exact candidates')
    for exclusion in exclusions:
        if (exclusion.get('reason_code') != VSYNC_EXCLUSION_CODE or
            exclusion.get('reason') != VSYNC_EXCLUSION_MESSAGE or
            not has_vsync_exclusion_evidence(results / 'backends' /
                                             exclusion['backend'], metadata['workloads'][0])):
            raise ValueError('Unsupported or unverified backend exclusion')
    for row in rows:
        for field in REQUIRED_TAILS:
            if row.get(field) in (None, ''):
                raise ValueError(f'Missing {field}: old benchmark results lack stability telemetry')
            value = float(row[field])
            if not math.isfinite(value) or value < 0:
                raise ValueError(f'Invalid or negative stability metric {field}')
            if (field.endswith('_count') or field == 'measured_frames') and not value.is_integer():
                raise ValueError(f'Expected integral stability metric {field}')
        expected_frames = (len(WORKLOADS) * 2 * metadata['repetitions'] *
                           metadata['frames'])
        if int(float(row['measured_frames'])) != expected_frames:
            raise ValueError('Measured frame count differs from benchmark configuration')
        stability_sort_key(row)  # also verifies that every metric is numeric
    policy_version = metadata.get('selection_policy_version', 1)
    if policy_version == 2:
        run_path = results / 'backend_tail_runs.csv'
        if not run_path.is_file():
            raise ValueError('Updated ranking lacks per-repetition evidence')
        runs = _read_csv(run_path)
        expected_keys = {(name, workload, mode, rep)
                         for name in names for workload in WORKLOADS
                         for mode in MODES
                         for rep in range(1, metadata['repetitions'] + 1)}
        observed_keys: list[tuple[str, str, str, int]] = []
        for run in runs:
            try:
                rep = int(run['repetition'])
                frames = int(run['measured_frames'])
                if frames != metadata['frames']:
                    raise ValueError('Incomplete per-repetition frame count')
                observed_keys.append((run['backend'].lower(), run['workload'],
                                      run['mode'], rep))
                for metric in ('frame_p95_ms', 'frame_p99_ms'):
                    value = float(run[metric])
                    if not math.isfinite(value) or value <= 0:
                        raise ValueError('Invalid per-repetition timing')
            except (TypeError, KeyError, OverflowError) as exc:
                raise ValueError('Malformed per-repetition evidence') from exc
        if len(observed_keys) != len(expected_keys) or set(observed_keys) != expected_keys:
            raise ValueError('Missing or duplicate backend repetition evidence')
        expected_order = rank_backends(rows, runs, platform=host)
    elif policy_version == 1:
        # Old verified reports are kept installable; new measurements use v2.
        expected_order = sorted(rows, key=stability_sort_key)
    else:
        raise ValueError('Unsupported renderer ranking policy version')
    if rows != expected_order:
        raise ValueError('Backend order does not agree with conservative stability scores')
    if _read_csv(results / 'backend_failures.csv'):
        raise ValueError('At least one backend failed its validation')
    checks = _read_csv(results / 'backend_equivalence.csv')
    if not checks or any(row.get('equivalent') != '1' for row in checks):
        raise ValueError('Missing or failed cross-backend functional equivalence checks')

    reference = read_presenter(
        results / 'backends' / names[0] / metadata['workloads'][0] / 'fixed_run01')
    display = {}
    for key in DISPLAY_MATCH:
        value = reference.get(key)
        if key in ('video_driver', 'sdl_version'):
            if not isinstance(value, str) or not value:
                raise ValueError(f'Invalid presenter {key}')
        elif type(value) is not int or (value <= 0 and key != 'display_index'):
            raise ValueError(f'Invalid presenter {key}')
        display[key] = value
    if display['display_index'] < 0:
        raise ValueError('Presenter display index cannot be negative')
    # Added after schema v1: retain compatibility with verified historical
    # results while new captures explicitly restrict fullscreen/windowed mode.
    if 'window_fullscreen' in reference:
        if type(reference['window_fullscreen']) is not bool:
            raise ValueError('Invalid SDL fullscreen presenter state')
        display['window_fullscreen'] = reference['window_fullscreen']

    return {
        'schema': 1,
        'platform': 'windows' if host == 'win32' else 'linux',
        'ranking_valid': True,
        'display': display,
        'ranked_backends': names,
        'benchmark': {
            'build_config': metadata['build_config'],
            'workloads': metadata['workloads'],
            'repetitions': metadata['repetitions'],
            'frames': metadata['frames'],
            'selection_policy': metadata['selection_policy'],
            'excluded_backends': excluded_names,
        },
    }


def install(results: Path, *, target: Path | None = None,
            platform: str | None = None) -> tuple[Path, dict]:
    record = validate_results(results, platform=platform)
    output = target or destination(platform=platform)
    output.parent.mkdir(parents=True, exist_ok=True)
    # Atomic replacement: never leave a truncated preference after interruption.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile('w', encoding='utf-8',
                                         dir=output.parent, prefix='.render-ranking-',
                                         suffix='.json', delete=False) as stream:
            temporary = Path(stream.name)
            json.dump(record, stream, indent=2)
            stream.write('\n')
        os.replace(temporary, output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return output, record


def format_status(path: Path, record: dict) -> str:
    """Readable, dependency-free summary of the installed auto priority."""
    if not isinstance(record, dict):
        raise ValueError('Installed ranking must be a JSON object')

    display = record.get('display') or {}
    benchmark = record.get('benchmark') or {}
    ranked = record.get('ranked_backends') or []
    if not isinstance(display, dict) or not isinstance(benchmark, dict) or not isinstance(ranked, list):
        raise ValueError('Malformed installed ranking')

    names = {
        'direct3d11': 'Direct3D 11', 'direct3d12': 'Direct3D 12',
        'direct3d': 'Direct3D 9', 'opengl': 'OpenGL',
        'opengles2': 'OpenGL ES 2', 'software': 'Software',
    }
    lines = [
        '',
        '  XENGINE  /  RENDER BACKEND AUTO',
        '  ' + '=' * 55,
        f'  Status      : {"VALID BENCHMARK INSTALLED" if record.get("ranking_valid") else "NOT VALIDATED"}',
        f'  Platform    : {str(record.get("platform", "unknown")).capitalize()}',
        f'  SDL         : {display.get("sdl_version", "unknown")}',
        f'  Display     : {display.get("window_width", "?")} x {display.get("window_height", "?")} '
        f'@ {display.get("display_refresh_hz", "?")} Hz  '
        f'(index {display.get("display_index", "?")}, {display.get("video_driver", "?")})',
        f'  Window mode : {"desktop fullscreen" if display.get("window_fullscreen") is True else "windowed" if display.get("window_fullscreen") is False else "not recorded (legacy ranking)"}',
        '',
        '  BACKEND PRIORITY  (first available and compatible)',
        '  ' + '-' * 55,
    ]
    if ranked:
        for index, backend in enumerate(ranked, start=1):
            name = names.get(str(backend).lower(), str(backend))
            suffix = '  <-- preferred' if index == 1 else ''
            lines.append(f'   {index:>2}. {name:<16} [{backend}]{suffix}')
    else:
        lines.append('      No ranked backends')

    workloads = benchmark.get('workloads') or []
    excluded = benchmark.get('excluded_backends') or []
    lines.extend([
        '',
        '  BENCHMARK',
        '  ' + '-' * 55,
        f'  Build       : {benchmark.get("build_config", "?")}',
        f'  Runs        : {benchmark.get("repetitions", "?")} repetitions, '
        f'{benchmark.get("frames", "?")} frames per run',
        f'  Workloads   : {", ".join(map(str, workloads)) or "unknown"}',
        f'  Excluded    : {", ".join(map(str, excluded)) or "none"}',
    ])
    policy = str(benchmark.get('selection_policy', '')).strip()
    if policy:
        lines.append('  Selection   :')
        lines.extend('    ' + segment for segment in textwrap.wrap(policy, width=75))
    lines.extend([
        '',
        f'  File: {path}',
        '  Note: auto uses this ranking only when SDL, display and window settings match.',
        '',
    ])
    return '\n'.join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--results', type=Path, help='Existing completed render_backends_* directory')
    parser.add_argument('--status', action='store_true', help='Show the current installed ranking')
    parser.add_argument('--json', action='store_true', help='Show --status as machine-readable JSON')
    parser.add_argument('--check', action='store_true', help='Validate results without installing')
    args = parser.parse_args(argv)
    if args.json and not args.status:
        parser.error('--json requires --status')
    try:
        if args.status:
            path = destination()
            if path.is_file():
                record = json.loads(path.read_text(encoding='utf-8'))
                if args.json:
                    print(json.dumps(record, indent=2, ensure_ascii=False))
                else:
                    print(format_status(path, record))
                return 0
            print(f'[RENDER-BACKEND] no installed ranking ({path})')
            return 1
        if args.results is None:
            parser.error('--results is required unless --status is supplied')
        if args.check:
            record = validate_results(args.results)
            print(f'[RENDER-BACKEND] validated={" -> ".join(record["ranked_backends"])}')
        else:
            path, record = install(args.results)
            print(f'[RENDER-BACKEND] installed={path}')
            print(f'[RENDER-BACKEND] auto={" -> ".join(record["ranked_backends"])}')
        return 0
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(f'[ERROR] Cannot install backend priority: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())

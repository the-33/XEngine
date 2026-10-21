#!/usr/bin/env python3
"""Repeatable XEngine game benchmark runner; only Python stdlib required.

Build game once, then start a NEW process for every mode/scenario/run.
Mode selection is performed by the game at startup, no rebuild required.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
from datetime import datetime
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / 'games' / 'XEngine Benchmark'
SCENARIOS = ('light', 'render', 'collision', 'balanced', 'stress')
MODES = ('single', 'collision', 'parallel')
LAYOUTS = ('static', 'clustered', 'spread', 'alternating', 'cycling')
METRICS = ('frame_ms', 'input_ms', 'fixed_ms', 'update_ms', 'physics_ms',
           'collision_ms', 'collision_snapshot_ms', 'collision_candidates_ms',
           'collision_merge_ms', 'render_present_ms', 'render_record_ms',
           'render_prepare_ms', 'render_submit_ms', 'render_only_present_ms',
           'render_worker_wait_ms', 'render_sprite_submit_ms',
           'fixed_steps', 'broadphase_tests', 'narrowphase_tests', 'workers',
           'collision_parallel_steps', 'collision_passes',
           'collision_parallel_passes', 'contacts_built',
           'physics_step_ms', 'physics_integrate_ms',
           'physics_build_contacts_ms', 'physics_solve_ms',
           'physics_substeps', 'physics_contacts_processed',
           'deterministic_steps_per_frame', 'collision_grid_build_ms',
           'collision_broadphase_mode', 'collision_grid_passes',
           'collision_bruteforce_passes', 'collision_grid_cell_size_last',
           'collision_auto_calibration_passes', 'collision_auto_cell_half_passes',
           'collision_auto_cell_base_passes', 'collision_auto_cell_double_passes', 'collision_auto_selector_ms',
           'collision_auto_context_last', 'collision_auto_context_transitions',
           'render_workers', 'render_parallel_executed', 'render_parallel_fallback',
           'render_executed_switched', 'render_effective_units_current',
           'render_estimated_offscreen_sprites',
           'render_eligible_current', 'render_prepared_commands',
           'render_culled_commands', 'render_immediate_culled',
           'render_world_draws', 'render_recorded_total', 'render_path_reason',
           'render_tint_cache_hits', 'render_tint_updates', 'render_background_draws',
           'render_ui_draws', 'render_debug_draws')


def validate_mode_configs(config_dir: Path, modes: tuple[str, ...]) -> dict[str, dict[str, object]]:
    """Fail closed when the benchmark would compare unintended engine settings.

    These are runtime configs, not the build-only config/engine.json.
    All mode files must be identical apart from the two threading switches
    and the display title, so that the comparison is controlled.
    """
    metadata: dict[str, dict[str, object]] = {}
    baseline: dict[str, object] | None = None
    for mode in modes:
        path = config_dir / f'{mode}.json'
        try:
            raw = path.read_bytes()
            config = json.loads(raw.decode('utf-8-sig'))
            if not isinstance(config, dict):
                raise ValueError('JSON root must be an object')
            engine, render = config['Engine'], config['Renderer']
            if not isinstance(engine, dict) or not isinstance(render, dict):
                raise ValueError('Engine and Renderer must be JSON objects')
            expected = {
                'Engine.multiThreading': mode != 'single',
                'Engine.profileCsv': True,
                'Renderer.parallelPreparation': mode == 'parallel',
                'Renderer.vsync': False,
            }
            for key, value in expected.items():
                section, field = key.split('.')
                actual = config[section].get(field)
                if type(actual) is not bool or actual != value:
                    raise ValueError(f'{key} must be {value!r}, found {actual!r}')
            # Ignore ONLY the deliberately different properties.
            normalized = json.loads(json.dumps(config))
            normalized['Engine'].pop('multiThreading')
            normalized['Renderer'].pop('parallelPreparation')
            if 'Window' in normalized:
                normalized['Window'].pop('title', None)
            if baseline is None:
                baseline = normalized
            elif normalized != baseline:
                raise ValueError('Non-threading settings differ from another selected mode')
            metadata[mode] = {
                'path': str(path.resolve()),
                'sha256': hashlib.sha256(raw).hexdigest(),
                'multiThreading': engine['multiThreading'],
                'parallelPreparation': render['parallelPreparation'],
            }
        except (OSError, UnicodeError, ValueError, KeyError, TypeError) as exc:
            raise ValueError(f'{path}: {exc}') from exc
    return metadata


def expected_config_marker(mode: str) -> str:
    return (f'[BENCH-CONFIG] mode={mode} multithreading={int(mode != "single")} '
            f'render_parallel={int(mode == "parallel")} vsync=0 profile_csv=1')


def expected_timing_marker(timing: str, fixed_steps: int) -> str:
    return (f'[BENCH-TIMING] mode={timing} fixed_steps_per_frame='
            f'{fixed_steps if timing == "deterministic" else 0}')


def assert_render_configuration_marker(stdout: str, *, threshold: int, sprites: int,
                                       workload: str, selection: str, cycle: bool) -> None:
    """Fail closed on mismatched render settings without depending on banner layout.

    The game prints a BENCH-RENDER handshake after Engine::StartFromJson.
    Compare all requested properties, including adaptive selection and load
    cycling, while allowing additional diagnostic fields in future versions.
    """
    prefix = '[BENCH-RENDER] '
    banners = [line for line in stdout.splitlines() if line.startswith(prefix)]
    expected = {
        'threshold': str(threshold), 'sprites': str(sprites),
        'workload': workload, 'selection': selection, 'cycle': str(int(cycle)),
    }
    if len(banners) != 1:
        raise RuntimeError(f'Expected exactly one {prefix.strip()} configuration banner; '
                           f'found {len(banners)}. Check executable version and build logs.')
    observed: dict[str, str] = {}
    for field in banners[0][len(prefix):].split():
        key, sep, value = field.partition('=')
        if sep and key in expected:
            if key in observed:
                raise RuntimeError(f'Duplicate {key!r} in render configuration banner: {banners[0]}')
            observed[key] = value
    mismatches = {key: (value, observed.get(key, '<missing>'))
                  for key, value in expected.items() if observed.get(key) != value}
    if mismatches:
        details = ', '.join(f'{key}: expected={want}, actual={got}'
                            for key, (want, got) in mismatches.items())
        raise RuntimeError(f'Game render configuration handshake mismatch ({details}). '
                           f'Actual banner: {banners[0]}. Rebuild with --build and '
                           'verify benchmark flags if needed.')


def assert_fixed_workload(path: Path, warmup: int, frames: int, fixed_steps: int) -> None:
    """Verify EVERY profile row including warmup, not just the mean.

    This is a scheduled workload, not a wall-clock or FPS simulation.  An
    unexpected step count makes cross-mode benchmark results incomparable.
    """
    with path.open(newline='', encoding='utf-8-sig') as handle:
        reader = csv.DictReader(handle)
        required = {'fixed_steps', 'deterministic_steps_per_frame'}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError(f'{path}: missing fixed-step verification columns')
        rows = list(reader)
    if len(rows) < warmup + frames:
        raise ValueError(f'{path}: too few profile rows to verify deterministic workload')
    for index, row in enumerate(rows[:warmup + frames]):
        for column in required:
            value = float(row[column])
            if value != fixed_steps:
                raise ValueError(f'{path}: frame {index} has {column}={value}; '
                                 f'expected exactly {fixed_steps}')


def quantile(items: list[float], q: float) -> float:
    ordered = sorted(items)
    i = q * (len(ordered) - 1)
    low = int(i)
    high = min(low + 1, len(ordered) - 1)
    return ordered[low] + (ordered[high] - ordered[low]) * (i - low)


def measure(path: Path, warmup: int, frames: int | None = None) -> dict[str, float]:
    with path.open(newline='', encoding='utf-8-sig') as handle:
        reader = csv.DictReader(handle)
        required = {'frame_ms', 'collision_ms', 'render_prepare_ms', 'render_path_reason',
                    'broadphase_tests', 'fixed_steps'}
        if not reader.fieldnames or not required.issubset(reader.fieldnames):
            raise ValueError(f'{path}: unsupported XEngine profile CSV: {required - set(reader.fieldnames or [])}')
        rows = list(reader)
    if len(rows) <= warmup:
        raise ValueError(f'{path}: {len(rows)} frames, warmup {warmup}')
    if frames is not None and len(rows) < warmup + frames:
        raise ValueError(f'{path}: expected at least {warmup + frames} recorded frames, got {len(rows)}')
    samples = rows[warmup:warmup + frames if frames is not None else None]
    summary: dict[str, float] = {'recorded_frames': len(rows), 'sampled_frames': len(samples)}
    for metric in METRICS:
        if metric not in samples[0]:
            continue
        values = [float(row[metric]) for row in samples if row.get(metric) not in (None, '')]
        if not values:
            continue
        summary[metric + '_mean'] = statistics.fmean(values)
        summary[metric + '_p50'] = quantile(values, .50)
        summary[metric + '_p95'] = quantile(values, .95)
    summary['render_parallel_requested_frames'] = sum(
        int(float(row['render_path_reason'])) == 3 for row in samples)
    # Engine v16.10+ separates route selected from actual worker dispatch.
    # Keep older profile CSVs readable, but never infer active workers solely
    # from the selected route if the execution metric is available.
    summary['render_parallel_frames'] = sum(
        int(float(row['render_parallel_executed'])) if row.get('render_parallel_executed') not in (None,'')
        else int(float(row.get('render_workers', '0'))) > 0
        if row.get('render_workers') not in (None, '')
        else int(float(row['render_path_reason'])) == 3
        for row in samples)
    # Weighted per-step values (ratio of sums, not average of per-frame ratios).
    # Real-time mode may have zero steps in many frames; deterministic mode
    # must have the same total fixed-step workload for each run.
    for column in ('fixed_steps', 'collision_passes', 'physics_substeps',
                   'collision_parallel_passes'):
        if column in samples[0]:
            summary[column + '_total'] = sum(float(row[column]) for row in samples)
    fixed_total = summary.get('fixed_steps_total', 0.0)
    if fixed_total:
        for column in ('physics_ms', 'collision_ms', 'fixed_ms'):
            if column in samples[0]:
                summary[column + '_per_fixed_step'] = sum(
                    float(row[column]) for row in samples) / fixed_total
    passes_total = summary.get('collision_passes_total', 0.0)
    if passes_total:
        for column in ('collision_snapshot_ms', 'collision_candidates_ms', 'collision_merge_ms', 'collision_grid_build_ms'):
            if column in samples[0]:
                summary[column + '_per_collision_pass'] = sum(
                    float(row[column]) for row in samples) / passes_total
    return summary


def assert_rendered_sprites(metrics: dict[str, float], scenario: str) -> None:
    """The benchmark must actually draw sprites, not just prepare or cull them.

    Every defined scenario has visible sprites. A zero SDL world-draw count
    means timings do not measure the intended render workload (e.g. a bad camera
    property binding or full-screen culling).
    """
    draws = metrics.get('render_world_draws_mean', 0.0)
    if draws <= 0:
        raise ValueError(f'{scenario}: no world sprites submitted to SDL '
                         '(render_world_draws=0); check camera/viewBase, culling '
                         'and scene visibility before comparing timings')


def mode_order(repetition: int, modes: tuple[str, ...]) -> list[str]:
    ordered = list(modes)
    if not ordered:
        return []
    shift = repetition % len(ordered)
    ordered = ordered[shift:] + ordered[:shift]
    if repetition % 2:
        ordered.reverse()
    return ordered


# Windows may report NTSTATUS as an unsigned 32-bit value; normalize both
# positive GetExitCodeProcess codes and negative values exposed by wrappers.
WINDOWS_LOADER_FAILURES = {
    0xC0000135: 'required DLL not found',
    0xC0000139: 'DLL entry point not found (incompatible DLL version)',
    0xC000007B: 'invalid executable/DLL format or architecture',
    0xC0000142: 'DLL initialization failed',
}


def windows_loader_error(returncode: int) -> str | None:
    """Describe Windows loader failures, including signed exit codes."""
    if sys.platform != 'win32':
        return None
    status = returncode & 0xFFFFFFFF
    detail = WINDOWS_LOADER_FAILURES.get(status)
    if detail is None:
        return None
    return (f'Windows loader failure 0x{status:08X}: {detail}. '
            'Check that the SDL2 and MinGW DLLs next to the executable '
            'come from the same GCC toolchain. Rebuild with --build to '
            'synchronize them; if the error persists, inspect DLL exports '
            'and dependencies with a Windows PE dependency viewer.')


def executable_default(build_config: str = 'Debug') -> Path:
    target = 'windows' if sys.platform == 'win32' else 'linux'
    suffix = '.exe' if target == 'windows' else ''
    if build_config == 'Release':
        game_config = json.loads((GAME / 'config.json').read_text(encoding='utf-8'))
        version = str(game_config['project']['version'])
        return GAME / 'dist' / f'{target}-x86_64' / version / f'XEngine Benchmark{suffix}'
    return (ROOT / 'build' / f'{target}-x86_64' / 'debug' / 'games' /
            'XEngine Benchmark' / f'XEngine Benchmark{suffix}')


def auto_cell_arg(value: str) -> str:
    if value == 'auto': return value
    try:
        size = float(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError('expected auto or a floating-point cell width') from exc
    if not 0.05 <= size <= 4096.0:
        raise argparse.ArgumentTypeError('cell width must be within [0.05, 4096]')
    return str(size)


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--exe', type=Path, help='Path to the selected benchmark executable (optional)')
    parser.add_argument('--build-config', choices=('Debug','Release'), default='Debug',
                        help='Debug or packaged Release benchmark (default Debug)')
    parser.add_argument('--build', action='store_true', help='Build selected configuration with tools/build.py first')
    parser.add_argument('--scenarios', nargs='+', choices=SCENARIOS, default=list(SCENARIOS))
    parser.add_argument('--modes', nargs='+', choices=MODES, default=list(MODES))
    parser.add_argument('--repetitions', type=int, default=3)
    parser.add_argument('--frames', type=int, default=360, help='Measured frames (after warmup)')
    parser.add_argument('--warmup', type=int, default=90)
    parser.add_argument('--seed', type=int, default=12345)
    parser.add_argument('--layout', choices=LAYOUTS, default='static',
                        help='Spatial distribution: static or controlled moving phases')
    parser.add_argument('--phase-frames', type=int, default=120,
                        help='Frames per spatial phase for alternating/cycling layouts')
    parser.add_argument('--broadphase', choices=('bruteforce', 'grid', 'auto'), default='bruteforce')
    parser.add_argument('--cell-size', type=auto_cell_arg, default='2.0',
                        help='Spatial grid cell width in world units')
    parser.add_argument('--render-workload', choices=('baseline','transforms','tint','culling','mixed','rotations_xyz'), default='baseline',
                        help='Render stress profile for sprite rotation/tint/culling/primitives')
    parser.add_argument('--sprites', type=int, default=None,
                        help='Override sprite count of selected scenario (0..10000)')
    parser.add_argument('--render-selection', choices=('fixed','adaptive'), default='fixed')
    parser.add_argument('--render-cycle', action='store_true', help='Cycle render sprite load through 55/75/100/65%% phases')
    parser.add_argument('--sprite-projection', choices=('main','workers'), default='workers',
                        help='A/B sprite XYZ geometry: project on main thread vs CPU render workers')
    parser.add_argument('--render-threshold', type=int, default=1536,
                        help='Minimum eligible/recorded render commands (1..100000; default 1536)')
    parser.add_argument('--colliders', type=int, default=None,
                        help='Override collider count of selected scenario (0..3000)')
    parser.add_argument('--compare-broadphase', action='store_true',
                        help='Run both algorithms separately, then compare contacts per frame')
    parser.add_argument('--compare-auto-cell', action='store_true',
                        help='Include auto broadphase with adaptive cell sizes in equivalence comparison')
    parser.add_argument('--compare-auto', action='store_true',
                        help='Include adaptive selection in the broadphase equivalence comparison')
    parser.add_argument('--timing', choices=('realtime', 'deterministic'), default='realtime',
                        help='Physics schedule: wall-clock accumulator or exactly N fixed steps per frame')
    parser.add_argument('--fixed-steps', type=int, default=1,
                        help='Fixed updates per frame in deterministic mode (1..16)')
    parser.add_argument('--timeout', type=int, default=300, help='Per-run timeout, seconds')
    parser.add_argument('--output', type=Path, help='Output folder; created per session by default')
    return parser.parse_args()


def main() -> int:
    args = arguments()
    if getattr(args, 'compare_auto_cell', False) and not getattr(args, 'compare_broadphase', False):
        print('[ERROR] --compare-auto-cell requires --compare-broadphase', file=sys.stderr)
        return 2
    if getattr(args, 'compare_auto', False) and not getattr(args, 'compare_broadphase', False):
        print('[ERROR] --compare-auto requires --compare-broadphase', file=sys.stderr)
        return 2
    if getattr(args, 'compare_broadphase', False):
        if args.cell_size == 'auto':
            print('[ERROR] Compare fixed-size baselines with --cell-size 2, '
                  'then run --broadphase auto --cell-size auto separately', file=sys.stderr)
            return 2
        return run_broadphase_comparison(args)
    if args.sprites is not None and not 0 <= args.sprites <= 10000:
        print('[ERROR] --sprites must be within 0..10000', file=sys.stderr)
        return 2
    if not 1 <= args.render_threshold <= 100000:
        print('[ERROR] --render-threshold must be within 1..100000', file=sys.stderr)
        return 2
    if getattr(args, 'colliders', None) is not None and not 0 <= args.colliders <= 3000:
        print('[ERROR] --colliders must be within 0..3000', file=sys.stderr)
        return 2
    if (getattr(args, 'cell_size', '2.0') != 'auto' and
        not 0.05 <= float(getattr(args, 'cell_size', '2.0')) <= 4096.0):
        print('[ERROR] --cell-size must be between 0.05 and 4096', file=sys.stderr)
        return 2
    if args.cell_size == 'auto' and args.broadphase != 'auto':
        print('[ERROR] --cell-size auto requires --broadphase auto', file=sys.stderr)
        return 2
    if min(args.repetitions, args.frames, args.timeout) < 1 or args.warmup < 0 or not 0 <= args.seed <= 1000000:
        print('[ERROR] repetitions/frames/timeout must be positive, warmup nonnegative and seed between 0 and 1000000', file=sys.stderr)
        return 2
    if not 1 <= args.fixed_steps <= 16 or (args.timing == 'realtime' and args.fixed_steps != 1):
        print('[ERROR] --fixed-steps must be 1..16 and is only used with --timing deterministic',
              file=sys.stderr)
        return 2
    if not 1 <= args.phase_frames <= 100000:
        print('[ERROR] --phase-frames must be within 1..100000', file=sys.stderr)
        return 2
    if args.frames + args.warmup > 100000:
        print('[ERROR] XEngine profile recording is capped at 100000 frames', file=sys.stderr)
        return 2
    # Fail before building/running when the requested measurement is invalid.
    assets = GAME / 'assets'
    try:
        configs = validate_mode_configs(assets / 'config', tuple(args.modes))
    except ValueError as exc:
        print(f'[ERROR] Benchmark configuration: {exc}', file=sys.stderr)
        return 2
    if args.build:
        command = [sys.executable, str(ROOT / 'tools/build.py'), 'game', '--project', GAME.name, '--config', args.build_config]
        print('[BUILD]', ' '.join(command), flush=True)
        try:
            subprocess.run(command, check=True)
        except (subprocess.CalledProcessError, OSError) as exc:
            print(f'[ERROR] Benchmark build failed: {exc}', file=sys.stderr)
            return 1
    executable = (args.exe or executable_default(args.build_config)).expanduser().resolve()
    if not executable.is_file():
        print(f'[ERROR] Missing benchmark executable: {executable}\n'
              f'Build first: python tools/build.py game --project "XEngine Benchmark" --config {args.build_config}',
              file=sys.stderr)
        return 2
    # The Release binary changes cwd to SDL_GetBasePath(), i.e. the packaged
    # distribution. The Debug binary uses the editable project's assets/.
    # Never read stale Debug profiles when benchmarking a packaged Release.
    assets = executable.parent if args.build_config == 'Release' else GAME / 'assets'
    if args.build_config == 'Release':
        for mode in args.modes:
            actual = (assets / 'config' / f'{mode}.json')
            if not actual.is_file() or hashlib.sha256(actual.read_bytes()).hexdigest() != configs[mode]['sha256']:
                print(f'[ERROR] Release assets differ from source config: {actual}; rebuild with --build', file=sys.stderr)
                return 2
    output = (args.output or (GAME / 'results' / datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    if output.exists() and any(output.iterdir()):
        print(f'[ERROR] Output directory is not empty (refusing to overwrite results): {output}', file=sys.stderr)
        return 2
    output.mkdir(parents=True, exist_ok=True)
    raw_results: list[dict[str, object]] = []
    failures: list[str] = []
    loader_failure: str | None = None
    metadata = {'executable': str(executable), 'build_config': args.build_config,
                'runtime_assets': str(assets), 'os': platform.platform(),
                'python': platform.python_version(), 'seed': args.seed,
                'frames': args.frames, 'warmup': args.warmup,
                'broadphase': getattr(args, 'broadphase', 'bruteforce'), 'cell_size': getattr(args, 'cell_size', 2.0),
                'timing': args.timing,
                'fixed_steps_per_frame': args.fixed_steps if args.timing == 'deterministic' else 0,
                'expected_measured_fixed_steps': (args.frames * args.fixed_steps
                                                  if args.timing == 'deterministic' else None),
                'colliders_override': getattr(args, 'colliders', None),
                'sprites_override': args.sprites, 'render_threshold': args.render_threshold,
                'render_workload': args.render_workload, 'sprite_projection': args.sprite_projection,
                'render_selection': args.render_selection, 'render_cycle': args.render_cycle,
                'layout': args.layout, 'phase_frames': args.phase_frames,
                'repetitions': args.repetitions, 'scenarios': args.scenarios,
                'modes': args.modes, 'vsync': False, 'runtime_configurations': configs,
                'timestamp': datetime.now().astimezone().isoformat(),
                'note': 'Each test is a separate engine process. Path reason=3 means recorded route requested; render_parallel_executed confirms actual worker preparation.'}
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')

    for scenario in args.scenarios:
        for repeat in range(args.repetitions):
            for mode in mode_order(repeat, tuple(args.modes)):
                tag = f'{scenario}_{mode}_run{repeat + 1:02d}'
                command = [str(executable), '--scenario', scenario, '--mode', mode,
                           '--frames', str(args.frames), '--warmup', str(args.warmup),
                           '--seed', str(args.seed), '--layout', args.layout,
                           '--phase-frames', str(args.phase_frames), '--timing', args.timing,
                           '--fixed-steps', str(args.fixed_steps),
                           '--broadphase', getattr(args, 'broadphase', 'bruteforce'),
                           '--cell-size', str(getattr(args, 'cell_size', 2.0)),
                           '--render-threshold', str(args.render_threshold),
                           '--render-selection', args.render_selection, '--render-cycle', str(int(args.render_cycle)),
                           '--render-workload', args.render_workload,
                           '--sprite-projection', args.sprite_projection]
                if args.sprites is not None:
                    command += ['--sprites', str(args.sprites)]
                if getattr(args, 'colliders', None) is not None:
                    command += ['--colliders', str(args.colliders)]
                source = assets / ('xengine_profile_single.csv' if mode == 'single'
                                   else 'xengine_profile_parallel.csv')
                source.unlink(missing_ok=True)
                print(f'[RUN] {tag} ({len(raw_results) + len(failures) + 1})', flush=True)
                started = time.perf_counter()
                try:
                    config_bytes = (assets / 'config' / f'{mode}.json').read_bytes()
                    if hashlib.sha256(config_bytes).hexdigest() != configs[mode]['sha256']:
                        raise ValueError(f'Runtime config for {mode} changed during benchmark')
                    proc = subprocess.run(command, cwd=assets, timeout=args.timeout,
                                          capture_output=True, text=True, errors='replace')
                    elapsed = time.perf_counter() - started
                    (output / f'{tag}.log').write_text(
                        f'command: {command!r}\nexit: {proc.returncode}\n'
                        f'elapsed_seconds: {elapsed:.3f}\n\nSTDOUT\n{proc.stdout}\n\nSTDERR\n{proc.stderr}',
                        encoding='utf-8')
                    if proc.returncode != 0:
                        loader_failure = windows_loader_error(proc.returncode)
                        if loader_failure is not None:
                            raise RuntimeError(f'exit={proc.returncode}, profile_exists={source.is_file()}; '
                                               f'{loader_failure}')
                    if proc.returncode != 0 or not source.is_file():
                        raise RuntimeError(f'exit={proc.returncode}, profile_exists={source.is_file()}; '
                                           f'see {output / (tag + ".log")}')
                    if expected_config_marker(mode) not in proc.stdout.splitlines():
                        raise RuntimeError('Game did not confirm the expected runtime configuration; '
                                           'rebuild it with --build and verify the mode JSON')
                    cell_value = getattr(args, 'cell_size', '2.0')
                    cell_marker = 'auto' if cell_value == 'auto' else f'{float(cell_value):.3f}'
                    if f'[BENCH-BROADPHASE] mode={getattr(args, "broadphase", "bruteforce")} cell_size={cell_marker}' not in proc.stdout.splitlines():
                        raise RuntimeError('Game did not confirm selected broadphase configuration')
                    if f'[BENCH-LAYOUT] layout={args.layout} phase_frames={args.phase_frames}' not in proc.stdout.splitlines():
                        raise RuntimeError('Game did not confirm the requested spatial layout')
                    expected_sprites = (args.sprites if args.sprites is not None else
                                        {'light': 120, 'render': 2200, 'collision': 220,
                                         'balanced': 1300, 'stress': 2600}[scenario])
                    assert_render_configuration_marker(
                        proc.stdout, threshold=args.render_threshold, sprites=expected_sprites,
                        workload=args.render_workload, selection=args.render_selection,
                        cycle=args.render_cycle)
                    if f'[BENCH-SPRITE-PROJECTION] mode={args.sprite_projection}' not in proc.stdout.splitlines():
                        raise RuntimeError('Game did not confirm selected sprite projection mode; rebuild with --build')
                    if expected_timing_marker(args.timing, args.fixed_steps) not in proc.stdout.splitlines():
                        raise RuntimeError('Game did not confirm the expected physics schedule; '
                                           'rebuild it with --build')
                    destination = output / f'{tag}.csv'
                    source.replace(destination)
                    metrics = measure(destination, args.warmup, args.frames)
                    if args.timing == 'deterministic':
                        assert_fixed_workload(destination, args.warmup, args.frames, args.fixed_steps)
                        if metrics['fixed_steps_total'] != args.frames * args.fixed_steps:
                            raise RuntimeError(f'{tag}: unequal total fixed-step workload')
                    assert_rendered_sprites(metrics, scenario)
                    requested_bp = getattr(args, 'broadphase', 'bruteforce')
                    expected_mode = {'bruteforce': 0.0, 'grid': 1.0, 'auto': 2.0}[requested_bp]
                    if metrics.get('collision_broadphase_mode_mean') != expected_mode:
                        raise RuntimeError('Profiler reports a different broadphase from requested')
                    total_passes = metrics.get('collision_passes_total', 0)
                    grids = metrics.get('collision_grid_passes_mean', 0) * args.frames
                    brutes = metrics.get('collision_bruteforce_passes_mean', 0) * args.frames
                    if abs(grids + brutes - total_passes) > 0.001:
                        raise RuntimeError('Actual algorithm pass counters do not match collision_passes')
                    if requested_bp == 'grid' and brutes != 0:
                        raise RuntimeError('Brute-force pass detected in grid mode')
                    if requested_bp == 'bruteforce' and grids != 0:
                        raise RuntimeError('Grid pass detected in bruteforce mode')
                    # A mode name alone does not prove that a parallel worker path ran.
                    # Keep direct telemetry in summary.csv for honest comparisons.
                    if mode != 'parallel' and metrics['render_parallel_frames']:
                        raise RuntimeError('Parallel rendering activated with the mode disabled')
                    if mode == 'single' and metrics.get('workers_mean', 0) != 0:
                        raise RuntimeError('Collision workers activated in single-threaded mode')
                    if mode == 'single' and metrics.get('collision_parallel_steps_mean', 0) != 0:
                        raise RuntimeError('Parallel collision dispatch activated in single-threaded mode')
                    if mode == 'parallel' and not metrics['render_parallel_frames']:
                        print(f'[WARN] {tag}: no parallel render frames (threshold or worker availability)', flush=True)
                    if mode != 'single' and not metrics.get('workers_mean', 0):
                        print(f'[WARN] {tag}: no collision workers available (possible single-core CPU)', flush=True)
                    elif (mode != 'single' and scenario in ('collision', 'balanced', 'stress')
                          and not metrics.get('collision_parallel_steps_mean', 0)):
                        print(f'[WARN] {tag}: collision workers exist but no candidate jobs were dispatched', flush=True)
                    raw_results.append({'scenario': scenario, 'mode': mode,
                                        'layout': args.layout, 'phase_frames': args.phase_frames,
                                        'sprites_override': args.sprites, 'render_threshold': args.render_threshold,
                'render_workload': args.render_workload, 'sprite_projection': args.sprite_projection,
                'render_selection': args.render_selection, 'render_cycle': args.render_cycle,
                                        'broadphase': getattr(args, 'broadphase', 'bruteforce'), 'cell_size': getattr(args, 'cell_size', 2.0), 'colliders_override': getattr(args, 'colliders', None),
                'timing': args.timing,
                                        'scheduled_fixed_steps': (args.fixed_steps if args.timing == 'deterministic' else 0),
                                        'repetition': repeat + 1, 'seed': args.seed,
                                        'elapsed_seconds': round(elapsed, 3), **metrics})
                    print(f'  frame p50={metrics["frame_ms_p50"]:.3f} ms '
                          f'p95={metrics["frame_ms_p95"]:.3f} ms '
                          f'fixed_steps={int(metrics.get("fixed_steps_total", 0))} '
                          f'render_parallel_frames={int(metrics["render_parallel_frames"])}', flush=True)
                except (RuntimeError, ValueError, subprocess.TimeoutExpired, OSError) as exc:
                    failures.append(f'{tag}: {exc}')
                    print(f'[FAIL] {tag}: {exc}', file=sys.stderr, flush=True)
                    # Startup/loader failures are independent of scenario and
                    # multithreading. Repeating 45 identical failures obscures
                    # the root cause and wastes time. Preserve the first log.
                    if loader_failure is not None:
                        print(f'[ABORT] {loader_failure}', file=sys.stderr, flush=True)
                        break
            if loader_failure is not None:
                break
        if loader_failure is not None:
            break

    # One row per run. Paired relative results use the single-threaded baseline
    # from the exact same scenario/repetition to avoid cross-scenario speedups.
    baseline = {(r['scenario'], r['repetition']): r['frame_ms_p50']
                for r in raw_results if r['mode'] == 'single'}
    for row in raw_results:
        b = baseline.get((row['scenario'], row['repetition']))
        row['speedup_vs_single_pct'] = (100. * (float(b) / float(row['frame_ms_p50']) - 1.)
                                        if b and row['frame_ms_p50'] else '')
    if raw_results:
        names = list(dict.fromkeys(key for row in raw_results for key in row))
        with (output / 'summary.csv').open('w', newline='', encoding='utf-8') as handle:
            writer = csv.DictWriter(handle, fieldnames=names)
            writer.writeheader()
            writer.writerows(raw_results)
    if failures:
        (output / 'failures.txt').write_text('\n'.join(failures) + '\n', encoding='utf-8')
    print(f'[BENCH] Saved {len(raw_results)} profiles; failures={len(failures)}; output={output}', flush=True)
    return 1 if failures else 0


def compare_broadphase_profiles(brute: Path, grid: Path) -> tuple[int, str]:
    """Compare ALL sampled frame contacts, not just aggregated timings."""
    columns = ('collision_contact_signature', 'contacts_built',
               'narrowphase_tests', 'fixed_steps', 'render_world_draws')
    with brute.open(newline='', encoding='utf-8-sig') as fd:
        a = list(csv.DictReader(fd))
    with grid.open(newline='', encoding='utf-8-sig') as fd:
        b = list(csv.DictReader(fd))
    if len(a) != len(b):
        return 1, f'frame count {len(a)} != {len(b)}'
    if not a or any(c not in a[0] or c not in b[0] for c in columns):
        return 1, 'missing comparison columns'
    for i, (x, y) in enumerate(zip(a, b)):
        for column in columns:
            if x[column] != y[column]:
                return 1, f'frame={i} {column}: brute={x[column]} grid={y[column]}'
    return 0, f'{len(a)} frames equivalent'


def run_broadphase_comparison(args: argparse.Namespace) -> int:
    """Run two isolated subprocess batches, aborting if equivalence differs."""
    if args.timing != 'deterministic':
        print('[ERROR] --compare-broadphase requires --timing deterministic', file=sys.stderr)
        return 2
    output = (args.output or (GAME / 'results' / datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    if output.exists() and any(output.iterdir()):
        print('[ERROR] Output folder must be empty', file=sys.stderr)
        return 2
    script_args = sys.argv[1:]
    clean = []
    i = 0
    while i < len(script_args):
        if script_args[i] in ('--compare-broadphase', '--compare-auto', '--compare-auto-cell'):
            i += 1
        elif script_args[i] in ('--output', '--broadphase'):
            i += 2
        else:
            clean.append(script_args[i]); i += 1
    options = ['bruteforce', 'grid']
    if args.compare_auto: options.append('auto')
    if args.compare_auto_cell: options.append('auto_cells')
    for bp in options:
        cmd = [sys.executable, str(Path(__file__).resolve()), *clean,
               '--broadphase', 'auto' if bp == 'auto_cells' else bp,
               '--output', str(output / bp)]
        if bp == 'auto_cells': cmd.extend(['--cell-size', 'auto'])
        if bp != 'bruteforce' and '--build' in cmd:
            cmd.remove('--build')  # already compiled on first batch
        print('[COMPARE]', bp, flush=True)
        rc = subprocess.call(cmd)
        if rc: return rc
    failures = []
    checked = 0
    for scenario in args.scenarios:
        for mode in args.modes:
            for r in range(1, args.repetitions + 1):
                tag = f'{scenario}_{mode}_run{r:02d}.csv'
                for bp in options[1:]:
                    rc, detail = compare_broadphase_profiles(
                        output / 'bruteforce' / tag, output / bp / tag)
                    print(f'[EQUIV] {bp}/{tag}: {detail}', flush=True)
                    checked += 1
                    if rc: failures.append(f'{bp}/{tag}: {detail}')
    (output / 'equivalence.txt').write_text(
        ('PASSED' if not failures else 'FAILED') + '\n' + '\n'.join(failures) + '\n',
        encoding='utf-8')
    print(f'[COMPARE] {checked - len(failures)} '
          f'paired profiles equivalent; mismatches={len(failures)}; output={output}')
    return 1 if failures else 0


if __name__ == '__main__':
    raise SystemExit(main())

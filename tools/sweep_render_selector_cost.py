#!/usr/bin/env python3
"""Controlled fixed-route A/B/C benchmark for render-selector scoring cost.

Keep the *rendering route and workers identical* while disabling scoring,
enabling weights, then enabling conservative visibility. Compare complete frame
and world-render distributions (including p95/p99); timing differences between
independent processes remain estimates, not direct per-sprite measurements.

Use --build to rebuild the selected Debug/Release game, with compiler output
streamed live and copied into build.log. Never benchmark after a failed build.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import json
from pathlib import Path
import statistics
import subprocess
import sys

from sweep_render_threshold import (ROOT, GAME, compare_profiles, read_profile,
                                    run_build_with_live_log, write_csv)
from sweep_render_selector import median, percentile

ESTIMATES = ('off', 'weighted', 'visibility')
WORKLOADS = ('baseline', 'transforms', 'tint', 'culling', 'mixed', 'rotations_xyz')
METRICS = ('frame_ms', 'render_world_ms', 'render_record_ms', 'render_prepare_ms',
           'render_submit_ms', 'render_selector_decision_ms')
REQUIRED = (*METRICS, 'render_parallel_executed', 'render_workers',
            'render_work_estimate_mode', 'render_route_override',
            'render_path_reason', 'render_estimated_offscreen_sprites',
            'render_eligible_current')
ESTIMATE_CODES = {'off': 0, 'weighted': 1, 'visibility': 2}
ROUTE_CODES = {'immediate': (1, 4, 0), 'recorded': (2, 3, 1)}


def arguments(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workloads', nargs='+', choices=WORKLOADS,
                        default=['baseline', 'culling', 'mixed', 'rotations_xyz'])
    parser.add_argument('--routes', nargs='+', choices=tuple(ROUTE_CODES),
                        default=['recorded', 'immediate'])
    parser.add_argument('--sprites', type=int, default=2048)
    parser.add_argument('--threshold', type=int, default=1536)
    parser.add_argument('--phase-frames', type=int, default=60)
    parser.add_argument('--frames', type=int, default=480)
    parser.add_argument('--warmup', type=int, default=60)
    parser.add_argument('--repetitions', type=int, default=3)
    parser.add_argument('--seed', type=int, default=12345)
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--build-config', choices=('Debug', 'Release'), default='Debug')
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--output', type=Path)
    return parser.parse_args(argv)


def variant_order(repetition: int) -> list[str]:
    modes = list(ESTIMATES)
    shift = (repetition - 1) % len(modes)
    modes = modes[shift:] + modes[:shift]
    return modes if repetition % 2 else list(reversed(modes))


def load_checked(path: Path, warmup: int, frames: int, estimate: str,
                 route: str) -> tuple[list[dict[str, str]], list[dict[str, str]]]:
    all_rows, measured = read_profile(path, warmup, frames)
    missing = set(REQUIRED) - set(all_rows[0])
    if missing:
        raise ValueError(f'{path}: missing selector cost metrics {sorted(missing)}; rebuild the benchmark')
    route_code, expected_reason, workers_expected = ROUTE_CODES[route]
    estimate_code = ESTIMATE_CODES[estimate]
    for index, row in enumerate(all_rows):
        if (int(float(row['render_work_estimate_mode'])) != estimate_code or
            int(float(row['render_route_override'])) != route_code or
            int(float(row['render_path_reason'])) != expected_reason):
            raise ValueError(f'{path}: incorrect selector/forced-route telemetry at frame {index}')
        active = int(float(row['render_parallel_executed']))
        workers = int(float(row['render_workers']))
        if active != workers_expected or ((workers > 0) != bool(workers_expected)):
            raise ValueError(f'{path}: workers did not execute the forced {route} path at frame {index}')
        if estimate == 'off' and float(row['render_estimated_offscreen_sprites']) != 0:
            raise ValueError(f'{path}: disabled scoring still ran visibility estimation at frame {index}')
        if estimate == 'weighted' and float(row['render_estimated_offscreen_sprites']) != 0:
            raise ValueError(f'{path}: weighted-only scoring still ran visibility estimation at frame {index}')
    return all_rows, measured


def assert_same_paths(first: list[dict[str, str]], second: list[dict[str, str]]) -> None:
    err, message = compare_profiles(first, second)
    if err:
        raise ValueError(f'Non-equivalent physics or render submissions: {message}')
    if len(first) != len(second):
        raise ValueError('Unequal profile length')
    for index, (a, b) in enumerate(zip(first, second)):
        for key in ('render_path_reason', 'render_parallel_executed', 'render_workers',
                    'render_eligible_current', 'render_route_override'):
            if a[key] != b[key]:
                raise ValueError(f'Unmatched render execution at frame {index}: {key}={a[key]} vs {b[key]}')


def stats(rows: list[dict[str, str]]) -> dict[str, float]:
    return {f'{metric}_{suffix}': percentile(rows, metric, q)
            for metric in METRICS
            for suffix, q in (('p50', .50), ('p95', .95), ('p99', .99))}


def add_deltas(record: dict[str, object], summaries: dict[str, dict[str, float]]) -> None:
    for metric in METRICS:
        for suffix in ('p50', 'p95', 'p99'):
            key = f'{metric}_{suffix}'
            for mode in ESTIMATES:
                record[f'{mode}_{key}'] = summaries[mode][key]
            record[f'weights_minus_off_{key}_ms'] = summaries['weighted'][key] - summaries['off'][key]
            record[f'visibility_minus_weights_{key}_ms'] = summaries['visibility'][key] - summaries['weighted'][key]
            record[f'visibility_minus_off_{key}_ms'] = summaries['visibility'][key] - summaries['off'][key]


def summarize(pairs: list[dict[str, object]]) -> list[dict[str, object]]:
    groups: dict[tuple[str, str], list[dict[str, object]]] = {}
    for row in pairs:
        groups.setdefault((str(row['workload']), str(row['route'])), []).append(row)
    result = []
    for (workload, route), group in sorted(groups.items()):
        summary: dict[str, object] = {'workload': workload, 'route': route,
                                      'repetitions': len(group)}
        for metric in METRICS:
            for suffix in ('p50', 'p95', 'p99'):
                for diff in ('weights_minus_off', 'visibility_minus_weights', 'visibility_minus_off'):
                    key = f'{diff}_{metric}_{suffix}_ms'
                    summary[key + '_median'] = statistics.median(float(row[key]) for row in group)
                    summary[key + '_positive_runs'] = sum(float(row[key]) > 0 for row in group)
        result.append(summary)
    return result


def run(argv: list[str] | None = None) -> int:
    args = arguments(argv)
    if (not args.workloads or len(args.workloads) != len(set(args.workloads)) or
        not args.routes or len(args.routes) != len(set(args.routes)) or
        not 1 <= args.sprites <= 10000 or not 1 <= args.threshold <= 100000 or
        not 1 <= args.phase_frames <= 100000 or args.frames < args.phase_frames * 4 or
        args.frames + args.warmup > 100000 or args.warmup < 0 or
        args.repetitions < 1 or args.timeout < 1 or not 0 <= args.seed <= 1000000):
        print('[ERROR] Invalid selector cost sweep parameters; include a full four-phase cycle', file=sys.stderr)
        return 2
    output = (args.output or GAME / 'results' /
              ('render_selector_cost_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        print(f'[ERROR] Refusing to overwrite a nonempty output: {output}', file=sys.stderr)
        return 2
    output.mkdir(parents=True, exist_ok=True)
    (output / 'metadata.json').write_text(json.dumps({
        'purpose': 'fixed-path estimator ablation; positive deltas mean added CPU/frame time',
        'workloads': args.workloads, 'routes': args.routes, 'estimates': ESTIMATES,
        'sprites': args.sprites, 'threshold': args.threshold, 'phase_frames': args.phase_frames,
        'frames': args.frames, 'warmup': args.warmup, 'repetitions': args.repetitions,
        'seed': args.seed, 'build_config': args.build_config,
        'render_cycle_percentages': [55, 75, 100, 65],
        'warning': 'Separate executable runs; frame/world deltas are measured A/B estimates, not direct timing inside the sprite loop. '
                   'Forced routes isolate scoring from path switching. p95/p99 are independent distribution quantiles.',
    }, indent=2) + '\n', encoding='utf-8')

    if args.build:
        cmd = [sys.executable, str(ROOT / 'tools/build.py'), 'game',
               '--project', GAME.name, '--config', args.build_config]
        rc = run_build_with_live_log(cmd, cwd=ROOT, log_path=output / 'build.log')
        if rc:
            message = f'Build failed ({rc}); benchmark aborted without executing any game binary.'
            (output / 'failures.txt').write_text(message + '\n', encoding='utf-8')
            print('[FAIL]', message, file=sys.stderr, flush=True)
            return 1

    pairs: list[dict[str, object]] = []
    phases: list[dict[str, object]] = []
    outliers: list[dict[str, object]] = []
    for workload in args.workloads:
        for route in args.routes:
            for repetition in range(1, args.repetitions + 1):
                captures = {}
                for estimate in variant_order(repetition):
                    dest = output / workload / route / f'{estimate}_run{repetition:02d}'
                    cmd = [sys.executable, str(ROOT / 'tools/run_benchmark.py'),
                           '--scenarios', 'render', '--modes', 'parallel',
                           '--build-config', args.build_config,
                           '--render-workload', workload, '--sprites', str(args.sprites),
                           '--colliders', '0', '--render-cycle', '--phase-frames', str(args.phase_frames),
                           '--render-selection', 'fixed', '--render-estimate', estimate,
                           '--render-route', route, '--render-threshold', str(args.threshold),
                           '--broadphase', 'bruteforce', '--timing', 'deterministic', '--fixed-steps', '1',
                           '--repetitions', '1', '--frames', str(args.frames), '--warmup', str(args.warmup),
                           '--seed', str(args.seed), '--timeout', str(args.timeout), '--output', str(dest)]
                    print(f'[SELECTOR-COST] {workload}/{route} r={repetition} estimate={estimate}', flush=True)
                    rc = subprocess.call(cmd, cwd=ROOT)
                    try:
                        if rc:
                            raise ValueError(f'benchmark process exited {rc}; see {dest}')
                        captures[estimate] = load_checked(dest / 'render_parallel_run01.csv',
                                                          args.warmup, args.frames, estimate, route)
                    except (OSError, KeyError, ValueError) as exc:
                        failure = f'{workload}/{route}/r{repetition}/{estimate}: {exc}'
                        (output / 'failures.txt').write_text(failure + '\n', encoding='utf-8')
                        print('[FAIL]', failure, file=sys.stderr, flush=True)
                        return 1
                try:
                    first = captures['off'][0]
                    for estimate in ('weighted', 'visibility'):
                        assert_same_paths(first, captures[estimate][0])
                    base = {'workload': workload, 'route': route, 'repetition': repetition,
                            'equivalent_frames': len(first), 'measured_frames': args.frames}
                    measured = {estimate: captures[estimate][1] for estimate in ESTIMATES}
                    record = dict(base)
                    add_deltas(record, {estimate: stats(rows) for estimate, rows in measured.items()})
                    record['offscreen_visibility_median'] = median(measured['visibility'], 'render_estimated_offscreen_sprites')
                    record['worker_frames'] = sum(int(float(row['render_parallel_executed'])) for row in measured['off'])
                    pairs.append(record)
                    for phase in range(4):
                        for estimate in ESTIMATES:
                            rows = [row for i, row in enumerate(measured[estimate], args.warmup)
                                    if (i // args.phase_frames) % 4 == phase]
                            if rows:
                                phases.append({**base, 'phase': phase, 'estimate': estimate,
                                               'frames': len(rows),
                                               'sprites_median': median(rows, 'render_eligible_current'),
                                               'offscreen_median': median(rows, 'render_estimated_offscreen_sprites'),
                                               **stats(rows)})
                    for estimate, rows in measured.items():
                        p99 = percentile(rows, 'frame_ms', .99)
                        for frame, row in enumerate(rows, args.warmup):
                            if float(row['frame_ms']) >= p99:
                                outliers.append({**base, 'estimate': estimate, 'frame': frame,
                                                 'phase': (frame // args.phase_frames) % 4,
                                                 'frame_ms': float(row['frame_ms']),
                                                 'render_world_ms': float(row['render_world_ms']),
                                                 'render_prepare_ms': float(row['render_prepare_ms']),
                                                 'render_submit_ms': float(row['render_submit_ms']),
                                                 'render_selector_decision_ms': float(row['render_selector_decision_ms']),
                                                 'worker_count': int(float(row['render_workers']))})
                except (KeyError, ValueError, ZeroDivisionError) as exc:
                    failure = f'{workload}/{route}/r{repetition}: failed parity/cost analysis: {exc}'
                    (output / 'failures.txt').write_text(failure + '\n', encoding='utf-8')
                    print('[FAIL]', failure, file=sys.stderr, flush=True)
                    return 1
                print(f'[EQUIV] {workload}/{route} r{repetition}: {len(first)} equivalent frames; '
                      f'visibility weight cost (world p50) '
                      f'{record["visibility_minus_weights_render_world_ms_p50_ms"]:+.5f} ms', flush=True)

    write_csv(output / 'paired_cost.csv', pairs)
    write_csv(output / 'phase_cost.csv', phases)
    write_csv(output / 'p99_outliers.csv', outliers)
    write_csv(output / 'summary.csv', summarize(pairs))
    print(f'[SELECTOR-COST] {len(pairs)} equivalent triplets; outputs at {output}', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(run())

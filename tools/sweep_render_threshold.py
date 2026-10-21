#!/usr/bin/env python3
"""Calibrate the render recorded/worker crossover with paired SDL game runs.

The immediate baseline is `collision` (collision workers enabled, immediate SDL
rendering). `parallel` has exactly the same simulation and collision settings,
but records and prepares render commands with a controllable threshold. This
measures the COMPLETE render-path tradeoff, not just worker CPU time. The tool
never silently rewrites the game's default 384-command threshold.
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime
import json
from pathlib import Path
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / 'games' / 'XEngine Benchmark'
VERIFY = ('fixed_steps', 'collision_contact_signature', 'contacts_built',
          'narrowphase_tests', 'render_world_draws', 'render_eligible_current',
          'render_background_draws', 'render_ui_draws', 'render_tint_updates')
FIELDS = ('frame_ms', 'render_present_ms', 'render_record_ms', 'render_prepare_ms',
          'render_submit_ms', 'render_only_present_ms', 'render_worker_wait_ms',
          'render_workers', 'render_path_reason', 'render_prepared_commands',
          'render_world_draws', 'render_eligible_current',
          'render_background_draws', 'render_ui_draws', 'render_tint_updates',
          'render_culled_commands', 'render_immediate_culled')


def read_profile(path: Path, warmup: int, frames: int) -> tuple[list[dict[str, str]], list[dict[str, str]]]:
    with path.open(newline='', encoding='utf-8-sig') as handle:
        rd = csv.DictReader(handle)
        required = set(VERIFY) | set(FIELDS)
        missing = required - set(rd.fieldnames or [])
        if missing:
            raise ValueError(f'{path}: missing profiler columns: {sorted(missing)}')
        values = list(rd)
    if len(values) < warmup + frames:
        raise ValueError(f'{path}: {len(values)} rows, expected at least {warmup + frames}')
    return values[:warmup + frames], values[warmup:warmup + frames]


def compare_profiles(a: list[dict[str, str]], b: list[dict[str, str]]) -> tuple[int, str]:
    if len(a) != len(b):
        return 1, f'frame count {len(a)} vs {len(b)}'
    for i, (left, right) in enumerate(zip(a, b)):
        for key in VERIFY:
            if left[key] != right[key]:
                return 1, f'frame={i}, {key}: immediate={left[key]}, parallel={right[key]}'
    return 0, f'{len(a)} equivalent frames (contacts, physics, world submissions)'


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        return
    fields = list(dict.fromkeys(k for row in rows for k in row))
    with path.open('w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def aggregate(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    groups: dict[tuple[str, int], list[dict[str, object]]] = {}
    for row in rows:
        groups.setdefault((str(row.get('workload', 'baseline')), int(row['sprites'])), []).append(row)
    result = []
    for (workload, count), group in sorted(groups.items()):
        gains = [float(g['gain_pct']) for g in group]
        diff = [float(g['immediate_p50_ms']) - float(g['parallel_p50_ms']) for g in group]
        result.append({
            'workload': workload, 'sprites': count, 'repetitions': len(group),
            'immediate_p50_ms': statistics.median(float(g['immediate_p50_ms']) for g in group),
            'parallel_p50_ms': statistics.median(float(g['parallel_p50_ms']) for g in group),
            'gain_pct_median': statistics.median(gains),
            'gain_ms_median': statistics.median(diff),
            'wins': sum(d > 0 for d in diff),
            'observed_eligible_median': statistics.median(float(g['render_eligible_median']) for g in group),
            'parallel_worker_frames_min': min(int(g['render_worker_frames']) for g in group),
            'immediate_p95_ms': statistics.median(float(g['immediate_p95_ms']) for g in group),
            'parallel_p95_ms': statistics.median(float(g['parallel_p95_ms']) for g in group),
        })
    return result


def crossing(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    # Interpolations are descriptive only. Require adjacent points and report
    # replication support; never call an interpolation a universal threshold.
    output = []
    for left, right in zip(rows, rows[1:]):
        if left.get('workload', 'baseline') != right.get('workload', 'baseline'):
            continue
        x1, x2 = int(left['sprites']), int(right['sprites'])
        y1, y2 = float(left['gain_ms_median']), float(right['gain_ms_median'])
        if y1 == y2 or y1*y2 >= 0:
            continue
        x = x1 + (0 - y1) * (x2-x1) / (y2-y1)
        output.append({'workload': left.get('workload', 'baseline'), 'lower_sprites': x1, 'upper_sprites': x2,
                       'interpolated_sprites': round(x, 1),
                       'left_gain_ms': round(y1, 6), 'right_gain_ms': round(y2, 6),
                       'left_wins': left['wins'], 'right_wins': right['wins'],
                       'repetitions': left['repetitions'],
                       'note': 'Exploratory interpolation; requires independent confirmation'})
    return output


def _recommendation_one(summary: list[dict[str, object]], min_gain_pct: float = 2.0) -> list[dict[str, object]]:
    """Return a conservative *candidate*, never a universal hardware threshold.

    Require 3 repeats and two adjacent tested loads with >=2/3 wins, >=2% p50
    gain and no >5% p95 regression. If no such region is found, do not guess.
    """
    for left, right in zip(summary, summary[1:]):
        if left.get('workload', 'baseline') != right.get('workload', 'baseline'):
            continue
        acceptable = []
        for row in (left, right):
            n = int(row['repetitions'])
            p95 = float(row['immediate_p95_ms'])
            p95_delta_pct = (p95 - float(row['parallel_p95_ms'])) / p95 * 100 if p95 > 0 else -100
            acceptable.append(n >= 3 and int(row['wins']) * 3 >= 2 * n and
                              float(row['gain_pct_median']) >= min_gain_pct and
                              p95_delta_pct >= -5.0 and int(row['parallel_worker_frames_min']) > 0)
        if all(acceptable):
            return [{'workload': left.get('workload', 'baseline'), 'status': 'candidate_only',
                     'suggested_eligible_commands': int(float(left['observed_eligible_median'])),
                     'start_sprites': left['sprites'], 'confirm_sprites': right['sprites'],
                     'minimum_gain_pct': min_gain_pct,
                     'note': 'Two consecutive supported loads; confirm with a new denser sweep and more independent repetitions before changing production default.'}]
    return [{'workload': 'all', 'status': 'not_confirmed', 'suggested_eligible_commands': '',
             'start_sprites': '', 'confirm_sprites': '', 'minimum_gain_pct': min_gain_pct,
             'note': 'No adjacent pair satisfies replication, median gain and p95 safeguards.'}]



def recommendation(summary: list[dict[str, object]], min_gain_pct: float = 2.0) -> list[dict[str, object]]:
    workloads = sorted({str(row.get('workload', 'baseline')) for row in summary})
    if not workloads:
        return _recommendation_one([], min_gain_pct)
    results = []
    for workload in workloads:
        group = [row for row in summary if str(row.get('workload', 'baseline')) == workload]
        for result in _recommendation_one(group, min_gain_pct):
            result['workload'] = workload
            results.append(result)
    return results


def arguments(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--counts', nargs='+', type=int,
                   default=[128, 192, 256, 320, 383, 384, 448, 512, 640, 768, 1024, 1536, 2048])
    p.add_argument('--workloads', nargs='+', choices=('baseline','transforms','tint','culling','mixed','rotations_xyz'),
                   default=['baseline'], help='Compare distinct render workloads; baseline retained for historical comparability')
    p.add_argument('--render-threshold', type=int, default=1,
                   help='Force recorded+worker path for calibration (default 1; production remains 384)')
    p.add_argument('--repetitions', type=int, default=3)
    p.add_argument('--frames', type=int, default=360)
    p.add_argument('--warmup', type=int, default=90)
    p.add_argument('--seed', type=int, default=12345)
    p.add_argument('--timeout', type=int, default=300)
    p.add_argument('--build', action='store_true')
    p.add_argument('--output', type=Path)
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = arguments(argv)
    if (not args.counts or any(n < 1 or n > 10000 for n in args.counts) or
        len(set(args.counts)) != len(args.counts) or len(set(args.workloads)) != len(args.workloads) or not 1 <= args.render_threshold <= 100000 or
        args.repetitions < 1 or args.frames < 1 or args.warmup < 0 or
        args.frames + args.warmup > 100000 or args.timeout < 1 or not 0 <= args.seed <= 1000000):
        print('[ERROR] Invalid counts, repetitions, frames, threshold or seed', file=sys.stderr)
        return 2
    output = (args.output or (GAME / 'results' / ('render_sweep_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f')))).resolve()
    if output.exists() and any(output.iterdir()):
        print(f'[ERROR] Refusing to overwrite nonempty output: {output}', file=sys.stderr)
        return 2
    output.mkdir(parents=True, exist_ok=True)
    metadata = {'counts': sorted(args.counts), 'workloads': args.workloads, 'repetitions': args.repetitions,
                'warmup': args.warmup, 'frames': args.frames,
                'render_threshold': args.render_threshold,
                'production_default_threshold': 384,
                'modes': ['collision', 'parallel'], 'scenario': 'render',
                'colliders': 0, 'timing': 'deterministic', 'fixed_steps_per_frame': 1,
                'seed': args.seed,
                'note': 'Threshold 1 forces recorded preparation; measure total path overhead including SDL submission. '
                        'All runs use Debug profiling and vsync=false; no screenshot/pixel-perfect equivalence claim.'}
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n', encoding='utf-8')
    if args.build:
        command = [sys.executable, str(ROOT/'tools/build.py'), 'game', '--project', GAME.name, '--config', 'Debug']
        print('[BUILD]', ' '.join(command), flush=True)
        with (output/'build.log').open('w', encoding='utf-8') as log:
            proc = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, bufsize=1, errors='replace')
            assert proc.stdout is not None
            for line in proc.stdout:
                print(line, end='', flush=True)
                log.write(line)
            rc = proc.wait()
        if rc:
            print(f'[FAIL] Build failed ({rc})', file=sys.stderr)
            return 1
    paired, runs, equivalent = [], [], []
    failures = []
    for workload in args.workloads:
      for count in sorted(args.counts):
          path = output / workload / f'sprites_{count:05d}'
          command = [sys.executable, str(ROOT/'tools/run_benchmark.py'),
                     '--scenarios', 'render', '--modes', 'collision', 'parallel',
                     '--sprites', str(count), '--colliders', '0', '--render-workload', workload,
                     '--broadphase', 'bruteforce', '--timing', 'deterministic',
                     '--fixed-steps', '1', '--render-threshold', str(args.render_threshold),
                     '--repetitions', str(args.repetitions), '--frames', str(args.frames),
                     '--warmup', str(args.warmup), '--seed', str(args.seed),
                     '--timeout', str(args.timeout), '--output', str(path)]
          print(f'[SWEEP] workload={workload} sprites={count}, repetitions={args.repetitions}', flush=True)
          rc = subprocess.call(command, cwd=ROOT)
          if rc:
              failures.append(f'workload={workload}, sprites={count}: run_benchmark exit={rc}')
              continue
          with (path/'summary.csv').open(newline='', encoding='utf-8-sig') as handle:
              perf = {(str(row['mode']), int(row['repetition'])): row for row in csv.DictReader(handle)}
          for repetition in range(1, args.repetitions+1):
              try:
                  a, abase = read_profile(path/f'render_collision_run{repetition:02d}.csv', args.warmup, args.frames)
                  b, bbase = read_profile(path/f'render_parallel_run{repetition:02d}.csv', args.warmup, args.frames)
                  status, detail = compare_profiles(a, b)
                  worker_frames = sum(float(row['render_workers']) > 0 for row in bbase)
                  reason_frames = sum(int(float(row['render_path_reason'])) == 3 for row in bbase)
                  if status == 0 and worker_frames == 0:
                      status, detail = 1, f'no render workers activated (threshold={args.render_threshold})'
                  if status == 0 and reason_frames != worker_frames:
                      status, detail = 1, 'recorded route was selected but workers did not prepare every recorded frame'
                  if status == 0 and any(float(row['render_workers']) > 0 for row in abase):
                      status, detail = 1, 'render workers activated in immediate baseline'
                  if status == 0 and workload in ('tint', 'mixed'):
                      if not any(float(row['render_tint_updates']) > 0 for row in bbase):
                          status, detail = 1, 'no texture tint updates measured'
                  if status == 0 and workload in ('culling', 'mixed'):
                      if not any(float(row['render_culled_commands']) > 0 for row in bbase):
                          status, detail = 1, 'no parallel sprite culling measured'
                  if status == 0 and workload == 'mixed':
                      if not any(float(row['render_ui_draws']) > 0 and
                                 float(row['render_background_draws']) > 0 for row in bbase):
                          status, detail = 1, 'mixed workload missing UI or background submissions'
                  equivalent.append({'workload':workload,'sprites':count,'repetition':repetition,
                                     'equivalent':int(not status),'detail':detail,
                                     'frames_checked':len(a), 'parallel_worker_frames':worker_frames,
                                     'parallel_route_frames': reason_frames})
                  if status:
                      failures.append(f'workload={workload} sprites={count} repetition={repetition}: {detail}')
                      print(f'[FAIL] {failures[-1]}', flush=True)
                      continue
                  left, right = perf['collision',repetition], perf['parallel',repetition]
                  for mode, perf_row in (('collision',left),('parallel',right)):
                      runs.append({'workload':workload,'sprites':count,'repetition':repetition,'render_mode':mode,
                                   **perf_row})
                  im = float(left['frame_ms_p50']); pa = float(right['frame_ms_p50'])
                  paired.append({'workload':workload,'sprites':count,'repetition':repetition,
                      'immediate_p50_ms':im,'parallel_p50_ms':pa,
                      'immediate_p95_ms':float(left['frame_ms_p95']),
                      'parallel_p95_ms':float(right['frame_ms_p95']),
                      'gain_pct':(im-pa)/im*100 if im else 0.0,
                      'render_eligible_median':statistics.median(float(row['render_eligible_current']) for row in bbase),
                      'render_worker_frames':worker_frames,'parallel_route_frames':reason_frames,
                      'immediate_render_ms_mean':float(left['render_present_ms_mean']),
                      'parallel_render_ms_mean':float(right['render_present_ms_mean']),
                      'parallel_record_ms_mean':float(right['render_record_ms_mean']),
                      'parallel_prepare_ms_mean':float(right['render_prepare_ms_mean']),
                      'parallel_submit_ms_mean':float(right['render_submit_ms_mean']),
                      'parallel_worker_wait_ms_mean':float(right['render_worker_wait_ms_mean']),
                      'immediate_world_draws_mean':float(left['render_world_draws_mean']),
                      'parallel_world_draws_mean':float(right['render_world_draws_mean']),
                      'immediate_tint_updates_mean':float(left['render_tint_updates_mean']),
                      'parallel_tint_updates_mean':float(right['render_tint_updates_mean']),
                      'immediate_ui_draws_mean':float(left['render_ui_draws_mean']),
                      'parallel_ui_draws_mean':float(right['render_ui_draws_mean'])})
                  print(f'[EQUIV] {workload}/{count} sprites r{repetition}: {detail}; '
                        f'gain={(im-pa)/im*100:.2f}% (parallel workers {worker_frames}/{args.frames})',flush=True)
              except (OSError, ValueError, KeyError) as exc:
                  failures.append(f'workload={workload} sprites={count} repetition={repetition}: {exc}')
                  print('[FAIL]', failures[-1], flush=True)
    summary = aggregate(paired)
    write_csv(output/'runs.csv',runs)
    write_csv(output/'paired_comparisons.csv',paired)
    write_csv(output/'summary.csv',summary)
    write_csv(output/'equivalence.csv',equivalent)
    write_csv(output/'crossovers.csv',crossing(summary))
    write_csv(output/'threshold_candidate.csv',recommendation(summary))
    if failures:
        (output/'failures.txt').write_text('\n'.join(failures)+'\n',encoding='utf-8')
    print(f'[SWEEP] completed pairs={len(paired)} errors={len(failures)} output={output}',flush=True)
    return 1 if failures else 0


if __name__ == '__main__':
    raise SystemExit(main())

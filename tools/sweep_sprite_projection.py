#!/usr/bin/env python3
"""Paired A/B benchmark: X/Y/Z sprite projection on main thread vs workers.

Unlike sweep_render_threshold.py, BOTH arms use exactly the same recorded,
parallel render pipeline and the same settings. Only the CPU location of sprite
projection differs. No production engine.json settings are changed.
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

from sweep_render_threshold import (GAME, ROOT, compare_profiles, read_profile,
                                    run_build_with_live_log, write_csv)

WORKLOADS = ('baseline', 'transforms', 'tint', 'culling', 'mixed', 'rotations_xyz')
METRICS = ('frame_ms', 'render_record_ms', 'render_prepare_ms', 'render_submit_ms',
           'render_world_ms', 'render_sprite_submit_ms', 'render_worker_wait_ms')
CHECK = ('render_recorded_total', 'render_culled_commands', 'render_prepared_commands')


def args_parser(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workloads', nargs='+', choices=WORKLOADS,
                   default=['baseline', 'transforms', 'rotations_xyz'])
    p.add_argument('--counts', type=int, nargs='+', default=[384, 768, 1536, 2048])
    p.add_argument('--repetitions', type=int, default=3)
    p.add_argument('--frames', type=int, default=360)
    p.add_argument('--warmup', type=int, default=90)
    p.add_argument('--seed', type=int, default=12345)
    p.add_argument('--timeout', type=int, default=300)
    p.add_argument('--build', action='store_true')
    p.add_argument('--output', type=Path)
    return p.parse_args(argv)


def metric_median(rows, key):
    return statistics.median(float(r[key]) for r in rows)


def metric_p95(rows, key):
    samples = sorted(float(r[key]) for r in rows)
    return samples[min(len(samples)-1, int((len(samples)-1)*.95))]


def main(argv=None):
    args = args_parser(argv)
    if (not args.counts or any(n < 1 or n > 10000 for n in args.counts) or
        len(set(args.counts)) != len(args.counts) or
        len(set(args.workloads)) != len(args.workloads) or
        args.repetitions < 1 or args.frames < 1 or args.warmup < 0 or
        args.frames + args.warmup > 100000 or args.timeout < 1 or
        args.seed < 0 or args.seed > 1000000):
        print('[ERROR] Invalid sweep arguments', file=sys.stderr)
        return 2
    output = (args.output or (GAME / 'results' / (
        'sprite_projection_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f')))).resolve()
    if output.exists() and any(output.iterdir()):
        print('[ERROR] Refusing to overwrite nonempty folder:', output, file=sys.stderr)
        return 2
    output.mkdir(parents=True, exist_ok=True)
    metadata = dict(variants=['main', 'workers'], renderer='parallel', render_threshold=1,
                    workloads=args.workloads, counts=sorted(args.counts),
                    repetitions=args.repetitions, frames=args.frames, warmup=args.warmup,
                    seed=args.seed, timing='deterministic', fixed_steps=1, colliders=0,
                    note='Both variants record and replay with workers. A/B compares only CPU projection placement; not pixels.')
    (output/'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n', encoding='utf-8')
    if args.build:
        build = [sys.executable, str(ROOT/'tools/build.py'), 'game',
                 '--project', GAME.name, '--config', 'Debug']
        # Live terminal output is essential: the engine library and the game
        # are both built by tools/build.py. Preserve the same output in build.log.
        rc = run_build_with_live_log(build, cwd=ROOT, log_path=output/'build.log')
        if rc:
            print(f'[FAIL] Build failed ({rc}); see {output / "build.log"}', file=sys.stderr)
            return 1
    comparisons, equivalences, errors = [], [], []
    for workload in args.workloads:
        for sprites in sorted(args.counts):
            for rep in range(1, args.repetitions+1):
                case = output/workload/f'sprites_{sprites:05d}'/f'rep_{rep:02d}'
                # Alternate execution order to reduce systematic thermal / cache bias.
                variants = ('main', 'workers') if rep % 2 else ('workers', 'main')
                pairs = {}
                for variant in variants:
                    dest = case/variant
                    cmd = [sys.executable, str(ROOT/'tools/run_benchmark.py'),
                           '--scenarios', 'render', '--modes', 'parallel',
                           '--render-workload', workload, '--sprite-projection', variant,
                           '--sprites', str(sprites), '--colliders', '0',
                           '--render-threshold', '1', '--timing', 'deterministic',
                           '--fixed-steps', '1', '--broadphase', 'bruteforce',
                           '--frames', str(args.frames), '--warmup', str(args.warmup),
                           '--repetitions', '1', '--seed', str(args.seed),
                           '--timeout', str(args.timeout), '--output', str(dest)]
                    print(f'[RUN] {workload} sprites={sprites} r{rep} projection={variant}', flush=True)
                    rc = subprocess.call(cmd, cwd=ROOT)
                    if rc:
                        errors.append(f'{workload}/{sprites}/r{rep} {variant}: exit={rc}')
                        break
                    try:
                        pairs[variant] = read_profile(dest/'render_parallel_run01.csv',
                                                      args.warmup, args.frames)
                    except (OSError, ValueError, KeyError) as e:
                        errors.append(f'{workload}/{sprites}/r{rep} {variant}: {e}')
                        break
                if len(pairs) != 2:
                    continue
                (m_all, m), (w_all, w) = pairs['main'], pairs['workers']
                mismatch, msg = compare_profiles(m_all, w_all)
                if not mismatch:
                    for column in CHECK:
                        if any(a[column] != b[column] for a,b in zip(m_all,w_all)):
                            mismatch, msg = 1, f'{column} differs'
                            break
                if not mismatch:
                    for variant, rows in (('main',m),('workers',w)):
                        if sum(float(r['render_workers']) > 0 for r in rows) != args.frames:
                            mismatch, msg = 1, f'{variant}: workers not active every measured frame'
                            break
                equivalences.append(dict(workload=workload, sprites=sprites, repetition=rep,
                                         equivalent=int(not mismatch), detail=msg,
                                         frames_checked=len(m_all)))
                if mismatch:
                    errors.append(f'{workload}/{sprites}/r{rep}: {msg}')
                    continue
                base = metric_median(m, 'frame_ms')
                optimized = metric_median(w, 'frame_ms')
                row = dict(workload=workload, sprites=sprites, repetition=rep,
                           main_frame_p50_ms=base, workers_frame_p50_ms=optimized,
                           gain_pct=(base-optimized)/base*100 if base else 0.,
                           main_frame_p95_ms=metric_p95(m,'frame_ms'),
                           workers_frame_p95_ms=metric_p95(w,'frame_ms'))
                for metric in METRICS:
                    if metric in m[0] and metric in w[0]:
                        row[f'main_{metric}_median'] = metric_median(m, metric)
                        row[f'workers_{metric}_median'] = metric_median(w, metric)
                comparisons.append(row)
                print(f'[EQUIV] {workload}/{sprites} r{rep}: {msg} p50 gain={row["gain_pct"]:+.2f}%',flush=True)
    summary = []
    for workload in args.workloads:
        for count in sorted(args.counts):
            values=[r for r in comparisons if r['workload']==workload and r['sprites']==count]
            if values:
                gains=[float(v['gain_pct']) for v in values]
                summary.append(dict(workload=workload, sprites=count,
                                    repetitions=len(values), median_gain_pct=statistics.median(gains),
                                    workers_wins=sum(g>0 for g in gains),
                                    main_p50_ms=statistics.median(v['main_frame_p50_ms'] for v in values),
                                    workers_p50_ms=statistics.median(v['workers_frame_p50_ms'] for v in values),
                                    main_p95_ms=statistics.median(v['main_frame_p95_ms'] for v in values),
                                    workers_p95_ms=statistics.median(v['workers_frame_p95_ms'] for v in values)))
    write_csv(output/'comparisons.csv',comparisons)
    write_csv(output/'equivalence.csv',equivalences)
    write_csv(output/'summary.csv',summary)
    if errors:
        (output/'failures.txt').write_text('\n'.join(errors)+'\n',encoding='utf-8')
    print(f'[SWEEP] {len(comparisons)} equivalent pairs; errors={len(errors)} output={output}',flush=True)
    return 1 if errors else 0


if __name__ == '__main__':
    raise SystemExit(main())

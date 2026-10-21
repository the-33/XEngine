#!/usr/bin/env python3
"""Compare fixed and provisional adaptive render-route selection with cycling sprite loads.

Both runs use the same multithreaded engine and visual workload; only route
selection differs. Build output is streamed to the console AND build.log.
A build failure is fatal and never falls back to a previous executable.
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

from sweep_render_threshold import (ROOT, GAME, VERIFY, read_profile,
                                    compare_profiles, write_csv, run_build_with_live_log)


def arguments(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workloads', nargs='+', default=['baseline', 'mixed', 'rotations_xyz'],
                   choices=['baseline','transforms','tint','culling','mixed','rotations_xyz'])
    p.add_argument('--sprites', type=int, default=2048)
    p.add_argument('--threshold', type=int, default=1536)
    p.add_argument('--phase-frames', type=int, default=60)
    p.add_argument('--frames', type=int, default=480)
    p.add_argument('--warmup', type=int, default=60)
    p.add_argument('--repetitions', type=int, default=3)
    p.add_argument('--seed', type=int, default=12345)
    p.add_argument('--timeout', type=int, default=300)
    p.add_argument('--build', action='store_true')
    p.add_argument('--build-config', choices=('Debug','Release'), default='Debug')
    p.add_argument('--output', type=Path)
    return p.parse_args(argv)


def median(rows: list[dict[str,str]], metric: str) -> float:
    return statistics.median(float(r[metric]) for r in rows)


def percentile(rows: list[dict[str,str]], metric: str, quantile: float) -> float:
    values = sorted(float(r[metric]) for r in rows)
    pos = (len(values) - 1) * quantile
    lo = int(pos)
    return values[lo] + (values[min(lo + 1, len(values) - 1)] - values[lo]) * (pos - lo)


def main(argv: list[str] | None = None) -> int:
    args = arguments(argv)
    if (not 1 <= args.sprites <= 10000 or not 1 <= args.threshold <= 100000 or
        not 1 <= args.phase_frames <= 100000 or args.frames < args.phase_frames * 4 or
        args.warmup < 0 or args.repetitions < 1 or args.timeout < 1 or
        not 0 <= args.seed <= 1000000 or len(set(args.workloads)) != len(args.workloads)):
        print('[ERROR] Invalid selector sweep parameters; measured window must cover a complete four-phase cycle', file=sys.stderr)
        return 2
    output = (args.output or GAME / 'results' / ('render_selector_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    if output.exists() and any(output.iterdir()):
        print('[ERROR] Refusing to overwrite nonempty output', file=sys.stderr)
        return 2
    output.mkdir(parents=True, exist_ok=True)
    (output / 'metadata.json').write_text(json.dumps({
        'selector_modes': ['fixed', 'adaptive'], 'workloads': args.workloads,
        'sprites': args.sprites, 'threshold': args.threshold,
        'phase_frames': args.phase_frames, 'frames': args.frames, 'warmup': args.warmup,
        'repetitions': args.repetitions, 'seed': args.seed, 'build_config': args.build_config,
        'render_cycle_percentages': [55,75,100,65],
        'note': 'Exploratory: adaptive work weights are heuristic; both modes use the same render pool. '
                'Fixed-to-adaptive transitions are not expected to be identical.',
    }, indent=2)+'\n', encoding='utf-8')
    if args.build:
        cmd = [sys.executable,str(ROOT/'tools/build.py'),'game','--project',GAME.name,'--config', args.build_config]
        rc = run_build_with_live_log(cmd,cwd=ROOT,log_path=output/'build.log')
        if rc:
            print(f'[FAIL] Build failed ({rc}); benchmark aborted; see {output / "build.log"}',file=sys.stderr)
            return 1
    # One binary/configuration per sweep; the runner resolves the Release
    # packaged assets alongside the Release executable, not Debug assets/.
    pairs=[]; phases=[]; failures=[]
    for workload in args.workloads:
        for rep in range(1,args.repetitions+1):
            # Alternate order to reduce temperature/drift bias.
            modes = ('fixed','adaptive') if rep%2 else ('adaptive','fixed')
            captures={}
            for mode in modes:
                path = output / workload / f'{mode}_run{rep:02d}'
                cmd=[sys.executable,str(ROOT/'tools/run_benchmark.py'),
                     '--scenarios','render','--modes','parallel','--build-config',args.build_config,
                     '--render-workload',workload,
                     '--sprites',str(args.sprites),'--colliders','0','--render-cycle',
                     '--phase-frames',str(args.phase_frames),'--render-selection',mode,
                     '--render-threshold',str(args.threshold),'--broadphase','bruteforce',
                     '--timing','deterministic','--fixed-steps','1',
                     '--repetitions','1','--frames',str(args.frames),'--warmup',str(args.warmup),
                     '--seed',str(args.seed),'--timeout',str(args.timeout),'--output',str(path)]
                print(f'[SELECTOR] workload={workload} r={rep} mode={mode}',flush=True)
                rc=subprocess.call(cmd,cwd=ROOT)
                if rc:
                    failures.append(f'{workload}/{rep}/{mode}: benchmark exited {rc}')
                    continue
                try:
                    captures[mode]=read_profile(path/'render_parallel_run01.csv',args.warmup,args.frames)
                except (OSError,ValueError,KeyError) as exc:
                    failures.append(f'{workload}/{rep}/{mode}: {exc}')
            if len(captures)!=2:continue
            fixed,fixSamples=captures['fixed']
            adaptive,adSamples=captures['adaptive']
            err,detail=compare_profiles(fixed,adaptive)
            if err:
                failures.append(f'{workload}/{rep}: NOT EQUIVALENT: {detail}')
                continue
            try:
                for mode,rows in [('fixed',fixSamples),('adaptive',adSamples)]:
                    expected=1 if mode=='adaptive' else 0
                    if any(int(float(row['render_selection_mode']))!=expected for row in rows):
                        raise ValueError(f'{mode}: selector mode profiler mismatch')
                    for phase in range(4):
                        # Note: phase boundaries include warmup; use actual profile row number.
                        window=[row for i,row in enumerate(rows,args.warmup)
                                if (i // args.phase_frames) % 4 == phase]
                        if not window:continue
                        phases.append({'workload':workload,'repetition':rep,'mode':mode,'phase':phase,
                            'frame_count':len(window),'eligible_commands_median':median(window,'render_eligible_current'),
                            'weighted_units_median':median(window,'render_weighted_units_current'),
                            'parallel_frames':sum(int(float(r['render_parallel_executed'])) for r in window),
                            'parallel_requested_frames':sum(int(float(r['render_path_reason']))==3 for r in window),
                            'parallel_fallback_frames':sum(int(float(r['render_parallel_fallback'])) for r in window),
                            'execution_switches':sum(int(float(r['render_executed_switched'])) for r in window),
                            'definitely_offscreen_median':median(window,'render_estimated_offscreen_sprites'),
                            'effective_work_units_median':median(window,'render_effective_units_current'),
                            'frame_p95_ms':percentile(window,'frame_ms',.95),
                            'frame_p99_ms':percentile(window,'frame_ms',.99),
                            'switches':sum(int(float(r['render_path_switched'])) for r in window),
                            'frame_p50_ms':median(window,'frame_ms')})
                speed=100*(median(fixSamples,'frame_ms')-median(adSamples,'frame_ms'))/median(fixSamples,'frame_ms')
                pairs.append({'workload':workload,'repetition':rep,'equivalent_frames':len(fixed),
                    'fixed_frame_p50_ms':median(fixSamples,'frame_ms'),
                    'adaptive_frame_p50_ms':median(adSamples,'frame_ms'),
                    'adaptive_gain_pct':speed,
                    'fixed_parallel_frames':sum(int(float(r['render_parallel_executed'])) for r in fixSamples),
                    'fixed_parallel_requested_frames':sum(int(float(r['render_path_reason']))==3 for r in fixSamples),
                    'fixed_fallback_frames':sum(int(float(r['render_parallel_fallback'])) for r in fixSamples),
                    'adaptive_parallel_frames':sum(int(float(r['render_parallel_executed'])) for r in adSamples),
                    'adaptive_parallel_requested_frames':sum(int(float(r['render_path_reason']))==3 for r in adSamples),
                    'adaptive_fallback_frames':sum(int(float(r['render_parallel_fallback'])) for r in adSamples),
                    'fixed_frame_p95_ms':percentile(fixSamples,'frame_ms',.95),
                    'adaptive_frame_p95_ms':percentile(adSamples,'frame_ms',.95),
                    'fixed_frame_p99_ms':percentile(fixSamples,'frame_ms',.99),
                    'adaptive_frame_p99_ms':percentile(adSamples,'frame_ms',.99),
                    'fixed_switches':sum(int(float(r['render_path_switched'])) for r in fixSamples),
                    'adaptive_switches':sum(int(float(r['render_path_switched'])) for r in adSamples),
                    'fixed_executed_switches':sum(int(float(r['render_executed_switched'])) for r in fixSamples),
                    'adaptive_executed_switches':sum(int(float(r['render_executed_switched'])) for r in adSamples)})
                print(f'[EQUIV] {workload} r{rep}: {detail}; adaptive gain={speed:+.2f}%',flush=True)
            except (KeyError,ValueError,ZeroDivisionError) as exc:
                failures.append(f'{workload}/{rep}: {exc}')
    write_csv(output/'paired_comparisons.csv',pairs)
    write_csv(output/'phase_results.csv',phases)
    if failures:
        (output/'failures.txt').write_text('\n'.join(failures)+'\n',encoding='utf-8')
        for failure in failures:print('[FAIL]',failure,file=sys.stderr)
    print(f'[SWEEP] pairs={len(pairs)} errors={len(failures)} output={output}',flush=True)
    return 1 if failures else 0


if __name__=='__main__':
    raise SystemExit(main())

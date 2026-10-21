#!/usr/bin/env python3
"""Calibrate XEngine broadphase on real hardware, preserving full profiler CSVs.

Run paired brute/grid benchmarks with identical collision counts, deterministic
steps and seed. Rotate run order across repetitions to reduce scheduling bias.
Each candidate is verified for full-frame contact equivalence. Crossover
estimates are piecewise-linear *interpolations*, not engine constants.
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

import run_benchmark as bench


def crossovers(points: list[tuple[int, float]]) -> list[float]:
    """Find zero crossings in performance delta; do not extrapolate."""
    ordered = sorted(points)
    crossings: list[float] = []
    for (n0, d0), (n1, d1) in zip(ordered, ordered[1:]):
        if d0 == 0:
            crossings.append(float(n0))
        elif (d0 < 0 < d1 or d0 > 0 > d1) and n1 > n0:
            crossings.append(n0 + (n1 - n0) * (-d0) / (d1 - d0))
    if ordered and ordered[-1][1] == 0:
        crossings.append(float(ordered[-1][0]))
    return crossings


def write_rows(path: Path, rows: list[dict]) -> None:
    if not rows:
        return
    with path.open('w', newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=list(dict.fromkeys(k for row in rows for k in row)))
        writer.writeheader()
        writer.writerows(rows)


def aggregate(rows: list[dict], counts: list[int], modes: list[str], sizes: list[float]) -> tuple[list[dict], list[dict]]:
    result = []
    crossings_out = []
    for mode in modes:
        for cell in sizes:
            deltas = []
            for n in counts:
                def times(bp: str):
                    return [float(r['frame_ms_p50']) for r in rows
                            if r['colliders'] == n and r['mode'] == mode and r['broadphase'] == bp
                            and (bp == 'bruteforce' or r['cell_size'] == cell)]
                a, b = times('bruteforce'), times('grid')
                if not a or not b:
                    raise ValueError(f'Incomplete experiment: mode={mode}, colliders={n}, cellSize={cell}')
                brute = statistics.median(a)
                grid = statistics.median(b)
                delta = brute - grid
                deltas.append((n, delta))
                def phase_cost(bp: str) -> float | None:
                    samples=[]
                    for row in rows:
                        if (row['colliders'] != n or row['mode'] != mode or
                            row['broadphase'] != bp or
                            (bp == 'grid' and row['cell_size'] != cell)):
                            continue
                        candidates=row.get('collision_candidates_ms_mean')
                        build=row.get('collision_grid_build_ms_mean',0)
                        if candidates is not None:
                            samples.append(float(candidates)+float(build or 0))
                    return statistics.median(samples) if samples else None
                brute_cost = phase_cost('bruteforce')
                grid_cost = phase_cost('grid')
                cost_delta=(brute_cost-grid_cost) if brute_cost is not None and grid_cost is not None else None
                result.append({'mode': mode, 'colliders': n, 'cell_size': cell,
                               'bruteforce_p50_ms': round(brute, 6),
                               'grid_p50_ms': round(grid, 6),
                               'grid_gain_ms': round(delta, 6),
                               'grid_gain_pct': round(100 * delta / brute, 4) if brute else 0,
                               'bruteforce_broadphase_ms': brute_cost,
                               'grid_broadphase_ms': grid_cost,
                               'grid_broadphase_gain_ms': cost_delta,
                               'grid_broadphase_gain_pct': round(100*cost_delta/brute_cost,4)
                                   if cost_delta is not None and brute_cost else None,
                               'repetitions': len(a)})
            for crossing in crossovers(deltas):
                crossings_out.append({'mode': mode, 'cell_size': cell,
                                      'interpolated_colliders': round(crossing, 2),
                                      'kind': 'linear interpolation; confirm with finer measurements'})
    return result, crossings_out


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', action='store_true')
    p.add_argument('--scenario', choices=bench.SCENARIOS, default='balanced')
    p.add_argument('--collider-counts', nargs='+', type=int,
                   default=[64, 128, 256, 384, 520, 640, 740, 900, 1050])
    p.add_argument('--cell-sizes', nargs='+', type=float, default=[1.0, 2.0, 4.0, 8.0])
    p.add_argument('--modes', nargs='+', choices=bench.MODES, default=['single', 'collision'])
    p.add_argument('--repetitions', type=int, default=3)
    p.add_argument('--frames', type=int, default=360)
    p.add_argument('--warmup', type=int, default=90)
    p.add_argument('--seed', type=int, default=12345)
    p.add_argument('--fixed-steps', type=int, default=1)
    p.add_argument('--timeout', type=int, default=300)
    p.add_argument('--output', type=Path)
    return p.parse_args()


def main() -> int:
    args = parse_args()
    counts = sorted(set(args.collider_counts))
    sizes = sorted(set(args.cell_sizes))
    if (not counts or not sizes or any(n < 0 or n > 3000 for n in counts)
        or any(not 0.05 <= x <= 4096 for x in sizes)
        or args.repetitions < 1 or args.frames < 1 or args.warmup < 0
        or args.fixed_steps not in range(1, 17)):
        print('[ERROR] Invalid sweep workload or parameter range', file=sys.stderr)
        return 2
    output = (args.output or (bench.GAME / 'results' / ('sweep_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f')))).resolve()
    if output.exists() and any(output.iterdir()):
        print('[ERROR] Output directory is not empty', file=sys.stderr)
        return 2
    output.mkdir(parents=True, exist_ok=True)
    if args.build:
        command = [sys.executable, str(bench.ROOT/'tools/build.py'), 'game',
                   '--project', bench.GAME.name, '--config', 'Debug']
        if subprocess.call(command):
            print('[ERROR] Build failed', file=sys.stderr)
            return 1
    (output/'metadata.json').write_text(json.dumps({
        'experiment': 'paired deterministic grid cell size and collider count sweep',
        'scenario': args.scenario, 'colliders': counts, 'cell_sizes': sizes,
        'modes': args.modes, 'frames': args.frames, 'warmup': args.warmup,
        'repetitions': args.repetitions, 'fixed_steps': args.fixed_steps,
        'seed': args.seed, 'note': 'A crossover is hardware-specific and interpolated only between tested counts.'
    }, indent=2)+'\n', encoding='utf-8')
    result_rows = []
    verification = []
    failures = []
    for n in counts:
        for mode in args.modes:
            for repeat in range(1, args.repetitions + 1):
                # Reverse algorithm sequence across repetitions; rotate sizes.
                options = [('bruteforce', 2.0)] + [('grid', c) for c in sizes]
                if repeat % 2 == 0:
                    options.reverse()
                else:
                    options = options[:1] + options[1:][(repeat-1)%len(sizes):] + options[1:][:(repeat-1)%len(sizes)]
                baseline = None
                other_profiles = []
                for bp, cell in options:
                    tag = f'n{n}_{mode}_r{repeat:02d}_{bp}_cell{cell:g}'
                    folder = output / tag
                    cmd = [sys.executable, str(bench.ROOT/'tools/run_benchmark.py'),
                           '--scenarios', args.scenario, '--modes', mode,
                           '--colliders', str(n), '--broadphase', bp, '--cell-size', str(cell),
                           '--repetitions', '1', '--timing', 'deterministic',
                           '--frames', str(args.frames), '--warmup', str(args.warmup),
                           '--seed', str(args.seed), '--fixed-steps', str(args.fixed_steps),
                           '--timeout', str(args.timeout), '--output', str(folder)]
                    print(f'[SWEEP] {tag}', flush=True)
                    completed = subprocess.run(cmd, capture_output=True, text=True, errors='replace')
                    (output/(tag+'.log')).write_text(completed.stdout + '\nSTDERR\n'+completed.stderr, encoding='utf-8')
                    if completed.returncode:
                        failures.append(f'{tag}: return={completed.returncode}; see log')
                        break
                    profile = folder / f'{args.scenario}_{mode}_run01.csv'
                    with (folder/'summary.csv').open(newline='', encoding='utf-8') as f:
                        row = next(csv.DictReader(f))
                    row.update(colliders=n, mode=mode, repetition=repeat, broadphase=bp, cell_size=cell,
                               profile_path=str(profile.relative_to(output)))
                    result_rows.append(row)
                    if bp == 'bruteforce':
                        baseline = profile
                    else:
                        other_profiles.append((tag, profile))
                if failures:
                    break
                for tag, profile in other_profiles:
                    code, detail = bench.compare_broadphase_profiles(baseline, profile)
                    verification.append({'comparison': tag, 'equal': code == 0, 'detail': detail})
                    if code: failures.append(f'{tag}: {detail}')
                if failures:
                    break
            if failures:
                break
        if failures:
            break
    write_rows(output/'runs.csv', result_rows)
    write_rows(output/'equivalence.csv', verification)
    if failures:
        (output/'failures.txt').write_text('\n'.join(failures)+'\n', encoding='utf-8')
        print(f'[FAIL] {failures[0]}; output={output}', file=sys.stderr)
        return 1
    combined, crossover = aggregate(result_rows, counts, args.modes, sizes)
    write_rows(output/'comparisons.csv', combined)
    write_rows(output/'crossovers.csv', crossover)
    best = [min((r for r in combined if r['colliders'] == n and r['mode'] == mode),
                key=lambda row: row['grid_p50_ms'])
            for n in counts for mode in args.modes]
    write_rows(output/'best_cell_size.csv', best)
    # Algorithm crossovers based on the measured cost the online selector
    # actually minimizes (grid construction + candidate evaluation).
    phase_crossings = []
    best_strategies = []
    for mode in args.modes:
        for cell in sizes:
            points=[(int(r['colliders']),float(r['grid_broadphase_gain_ms']))
                    for r in combined if r['mode']==mode and r['cell_size']==cell
                    and r['grid_broadphase_gain_ms'] is not None]
            for x in crossovers(points):
                phase_crossings.append({'mode': mode,'cell_size': cell,
                    'interpolated_colliders':round(x,2),
                    'basis':'median grid_build_ms + collision_candidates_ms',
                    'status':'interpolation, not universal threshold'})
        for n in counts:
            options=[r for r in combined if r['mode']==mode and r['colliders']==n
                     and r['grid_broadphase_ms'] is not None]
            if not options: continue
            brute=options[0]['bruteforce_broadphase_ms']
            fastest=min(options,key=lambda row:row['grid_broadphase_ms'])
            winner='bruteforce' if brute <= fastest['grid_broadphase_ms'] else 'grid'
            best_strategies.append({'mode':mode,'colliders':n,'winner':winner,
                'cell_size':fastest['cell_size'] if winner=='grid' else '',
                'bruteforce_cost_ms':brute,
                'best_grid_cost_ms':fastest['grid_broadphase_ms'],
                'best_grid_size':fastest['cell_size'],
                'grid_gain_ms':brute-fastest['grid_broadphase_ms']})
    write_rows(output/'cost_crossovers.csv',phase_crossings)
    write_rows(output/'best_strategy.csv',best_strategies)
    print(f'[SWEEP] {len(result_rows)} profiles; {len(verification)} verified equivalents; '
          f'{len(crossover)} interpolated crossovers; output={output}')
    print('[SWEEP] Read comparisons.csv, best_cell_size.csv and crossovers.csv; '
          'cost_crossovers.csv and best_strategy.csv describe measured phase costs; no threshold is imposed.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

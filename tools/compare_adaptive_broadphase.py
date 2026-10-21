#!/usr/bin/env python3
"""Paired, deterministic comparison of XEngine broadphase strategies.

Run brute force, fixed 1/2-unit grids, and auto-cell in a rotating order for
*each* scenario/mode/repetition. Preserve all individual profiler CSVs, verify
contacts frame-by-frame, and export paired and aggregated CPU costs.
This tool does not invent a globally applicable collider-count threshold.
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

import run_benchmark as benchmark

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / 'games' / 'XEngine Benchmark'
STRATEGIES = ('bruteforce', 'grid1', 'grid2', 'auto_cells')
SETTINGS = {
    'bruteforce': ('bruteforce', '2'),
    'grid1': ('grid', '1'),
    'grid2': ('grid', '2'),
    'auto_cells': ('auto', 'auto'),
}


def rotated_order(repetition: int, scenario_index: int, mode_index: int) -> tuple[str, ...]:
    """Spread early/late positions across repetitions and scene/mode pairs."""
    offset = (repetition - 1 + scenario_index + mode_index) % len(STRATEGIES)
    values = STRATEGIES[offset:] + STRATEGIES[:offset]
    return tuple(reversed(values)) if (repetition + scenario_index) % 2 == 0 else values


def robust_median(values: list[float]) -> float:
    return statistics.median(values)


def median_summary(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    groups: dict[tuple[str, str, str, str], list[dict[str, object]]] = {}
    for row in rows:
        groups.setdefault((str(row.get('layout','static')), str(row['scenario']), str(row['mode']), str(row['strategy'])), []).append(row)
    result: list[dict[str, object]] = []
    for (layout, scenario, mode, strategy), group in sorted(groups.items()):
        record: dict[str, object] = {
            'layout': layout, 'scenario': scenario, 'mode': mode, 'strategy': strategy,
            'repetitions': len(group),
        }
        for key in ('frame_ms_p50', 'frame_ms_p95', 'broadphase_cost_ms_mean',
                    'collision_auto_calibration_passes_total',
                    'collision_auto_selector_ms_mean', 'effective_broadphase_ms_mean',
                    'collision_grid_passes_total', 'collision_bruteforce_passes_total',
                    'collision_auto_cell_half_passes_total',
                    'collision_auto_cell_base_passes_total',
                    'collision_auto_cell_double_passes_total'):
            vals = [float(g[key]) for g in group if g.get(key) not in ('', None)]
            record[key + '_median'] = robust_median(vals) if vals else ''
        result.append(record)
    by_combo = {(r['layout'], r['scenario'], r['mode']): [] for r in result}
    for record in result:
        by_combo[record['layout'], record['scenario'], record['mode']].append(record)
    for group in by_combo.values():
        fixed = [r for r in group if r['strategy'] != 'auto_cells']
        best = min(fixed, key=lambda r: float(r['frame_ms_p50_median']))
        for record in group:
            record['best_fixed_strategy'] = best['strategy']
            record['best_fixed_p50_ms'] = best['frame_ms_p50_median']
            record['auto_vs_best_fixed_pct'] = (
                100. * (float(best['frame_ms_p50_median']) - float(record['frame_ms_p50_median'])) /
                float(best['frame_ms_p50_median']) if record['strategy'] == 'auto_cells' else '')
    return result


def paired_comparisons(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    grouped: dict[tuple[str, str, str, int], dict[str, dict[str, object]]] = {}
    for row in rows:
        grouped.setdefault((str(row.get('layout','static')), str(row['scenario']), str(row['mode']), int(row['repetition'])), {})[str(row['strategy'])] = row
    comparisons = []
    for (layout, scenario, mode, rep), data in sorted(grouped.items()):
        if len(data) != len(STRATEGIES):
            raise ValueError(f'Incomplete comparison at {scenario}/{mode}/run{rep:02d}')
        auto = data['auto_cells']
        for strategy in STRATEGIES[:-1]:
            fixed = data[strategy]
            comparisons.append({
                'layout': layout, 'scenario': scenario, 'mode': mode, 'repetition': rep,
                'baseline': strategy, 'auto_p50_ms': auto['frame_ms_p50'],
                'baseline_p50_ms': fixed['frame_ms_p50'],
                'auto_vs_baseline_pct': 100. * (
                    float(fixed['frame_ms_p50']) - float(auto['frame_ms_p50'])) /
                    float(fixed['frame_ms_p50']),
                'auto_broadphase_mean_ms': auto['broadphase_cost_ms_mean'],
                'baseline_broadphase_mean_ms': fixed['broadphase_cost_ms_mean'],
                'auto_probe_passes': auto.get('collision_auto_calibration_passes_total', 0),
            })
    return comparisons


def spatial_phase(layout: str, phase_index: int) -> str:
    if layout == 'clustered': return 'clustered'
    if layout == 'spread': return 'spread'
    if layout == 'alternating': return ('regular', 'clustered')[phase_index % 2]
    if layout == 'cycling': return ('regular', 'clustered', 'spread')[phase_index % 3]
    return 'regular'


def phase_metrics(profile: Path, layout: str, scenario: str, mode: str,
                  repetition: int, strategy: str, warmup: int, frames: int,
                  period: int, settle: int) -> list[dict[str, object]]:
    """Measure identical layout phases independently, excluding transition frames.

    Frame indexes are relative to the start of the process (warmup included).
    The engine records one row per frame and the distribution script advances
    exactly once during Update. Avoid phase p50 contamination by transitions.
    """
    with profile.open(newline='', encoding='utf-8-sig') as handle:
        rows = list(csv.DictReader(handle))
    groups: dict[int, list[dict[str, str]]] = {}
    for index, row in enumerate(rows[warmup:warmup + frames], start=warmup):
        if index % period < settle: continue
        groups.setdefault(index // period, []).append(row)
    result: list[dict[str, object]] = []
    for phase, samples in sorted(groups.items()):
        if not samples: continue
        times = [float(row['frame_ms']) for row in samples]
        candidates = [float(row.get('collision_candidates_ms') or 0) +
                      float(row.get('collision_grid_build_ms') or 0) +
                      float(row.get('collision_auto_selector_ms') or 0) for row in samples]
        probes = sum(float(row.get('collision_auto_calibration_passes') or 0) for row in samples)
        context_changes = sum(float(row.get('collision_auto_context_transitions') or 0) for row in samples)
        context_classes = sorted(set(int(float(row.get('collision_auto_context_last') or 0)) for row in samples))
        brutes = sum(float(row.get('collision_bruteforce_passes') or 0) for row in samples)
        grids = sum(float(row.get('collision_grid_passes') or 0) for row in samples)
        result.append({'layout': layout, 'scenario': scenario, 'mode': mode,
                       'repetition': repetition, 'strategy': strategy,
                       'phase_index': phase, 'spatial_phase': spatial_phase(layout, phase),
                       'measured_frames': len(samples),
                       'frame_p50_ms': robust_median(times),
                       'broadphase_mean_ms': statistics.fmean(candidates),
                       'probe_passes': probes, 'context_transitions': context_changes,
                       'context_classes': '/'.join(map(str,context_classes)),
                       'bruteforce_passes': brutes,
                       'grid_passes': grids})
    return result


def paired_phases(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    grouped: dict[tuple, dict[str, dict[str, object]]] = {}
    for row in rows:
        key = (row['layout'],row['scenario'],row['mode'],row['repetition'],row['phase_index'])
        grouped.setdefault(key,{})[str(row['strategy'])]=row
    output: list[dict[str, object]]=[]
    for (layout,scenario,mode,rep,phase), strategies in sorted(grouped.items()):
        if set(strategies)!=set(STRATEGIES):
            raise ValueError(f'Missing phase baseline: {layout}/{scenario}/{mode}/{rep}/{phase}')
        auto=strategies['auto_cells']
        best=min((strategies[s] for s in STRATEGIES[:-1]),
                 key=lambda item:float(item['broadphase_mean_ms']))
        output.append({'layout':layout,'scenario':scenario,'mode':mode,
                       'repetition':rep,'phase_index':phase,
                       'spatial_phase':auto['spatial_phase'],
                       'best_fixed':best['strategy'],
                       'best_fixed_broadphase_ms':best['broadphase_mean_ms'],
                       'auto_broadphase_ms':auto['broadphase_mean_ms'],
                       'auto_vs_best_fixed_pct':100.*(float(best['broadphase_mean_ms'])-
                                      float(auto['broadphase_mean_ms'])) /
                                      max(float(best['broadphase_mean_ms']),1.e-9),
                       'auto_probe_passes':auto['probe_passes'],
                       'auto_context_transitions':auto['context_transitions'],
                       'auto_context_classes':auto['context_classes'],
                       'auto_brute_passes':auto['bruteforce_passes'],
                       'auto_grid_passes':auto['grid_passes']})
    return output


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        return
    columns = list(dict.fromkeys(key for row in rows for key in row))
    with path.open('w', encoding='utf-8', newline='') as handle:
        writer = csv.DictWriter(handle, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)


def read_run(output: Path, scenario: str, mode: str, repetition: int,
             strategy: str, frames: int, warmup: int, fixed_steps: int) -> tuple[dict[str, object], Path]:
    profile = output / f'{scenario}_{mode}_run01.csv'
    if not profile.is_file():
        raise ValueError(f'Missing profile: {profile}')
    metrics = benchmark.measure(profile, warmup, frames)
    benchmark.assert_rendered_sprites(metrics, scenario)
    # The deterministic runner itself verifies every frame (warmup included).
    if int(metrics['fixed_steps_total']) != frames * fixed_steps:
        raise ValueError(f'{profile}: fixed-step mismatch')
    with (output / 'summary.csv').open(newline='', encoding='utf-8') as fd:
        summary_rows = list(csv.DictReader(fd))
    if len(summary_rows) != 1:
        raise ValueError(f'{output}: expected exactly one summary row')
    row: dict[str, object] = {
        'scenario': scenario, 'mode': mode, 'repetition': repetition,
        'strategy': strategy, 'frames': frames, 'warmup': warmup,
        'fixed_steps_per_frame': fixed_steps,
        'frame_ms_p50': metrics['frame_ms_p50'],
        'frame_ms_p95': metrics['frame_ms_p95'],
        'collision_candidates_ms_mean': metrics.get('collision_candidates_ms_mean', 0.),
        'collision_grid_build_ms_mean': metrics.get('collision_grid_build_ms_mean', 0.),
        'broadphase_cost_ms_mean': (metrics.get('collision_candidates_ms_mean', 0.) +
                                     metrics.get('collision_grid_build_ms_mean', 0.)),
        'collision_auto_selector_ms_mean': metrics.get('collision_auto_selector_ms_mean', 0.),
        'effective_broadphase_ms_mean': (metrics.get('collision_candidates_ms_mean', 0.) +
                                         metrics.get('collision_grid_build_ms_mean', 0.) +
                                         metrics.get('collision_auto_selector_ms_mean', 0.)),
        'collision_auto_calibration_passes_total': (
            metrics.get('collision_auto_calibration_passes_mean', 0.) * frames),
        'collision_grid_passes_total': metrics.get('collision_grid_passes_mean', 0.) * frames,
        'collision_bruteforce_passes_total': metrics.get('collision_bruteforce_passes_mean', 0.) * frames,
        'collision_auto_cell_half_passes_total': metrics.get('collision_auto_cell_half_passes_mean', 0.) * frames,
        'collision_auto_cell_base_passes_total': metrics.get('collision_auto_cell_base_passes_mean', 0.) * frames,
        'collision_auto_cell_double_passes_total': metrics.get('collision_auto_cell_double_passes_mean', 0.) * frames,
        'render_world_draws_mean': metrics.get('render_world_draws_mean', 0.),
        'profile_path': str(profile.relative_to(output.parents[4])),
    }
    return row, profile


def run_command(command: list[str], logfile: Path, *, stream: bool = False) -> None:
    """Save combined stdout/stderr; optionally tee build output live to the console.

    Benchmark subprocesses stay quiet (their complete output remains in their
    individual log files).  Only --build uses streaming so compiler/cache
    diagnostics are visible before the first [PAIR] line.
    """
    if not stream:
        finished = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  text=True, encoding='utf-8', errors='replace')
        logfile.write_text(f'$ {" ".join(command)}\n{finished.stdout}', encoding='utf-8')
        if finished.returncode:
            raise RuntimeError(f'Command failed ({finished.returncode}): {" ".join(command)}\n'
                               f'Log: {logfile}\n{finished.stdout[-1800:]}')
        return

    with logfile.open('w', encoding='utf-8') as log:
        log.write(f'$ {" ".join(command)}\n')
        log.flush()
        with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, encoding='utf-8', errors='replace',
                              bufsize=1) as process:
            assert process.stdout is not None
            for line in process.stdout:
                print(line, end='', flush=True)
                log.write(line)
                log.flush()
            result = process.wait()
    if result:
        raise RuntimeError(f'Command failed ({result}): {" ".join(command)}\n'
                           f'Complete build output: {logfile}')


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', action='store_true')
    p.add_argument('--exe', type=Path)
    p.add_argument('--scenarios', nargs='+', choices=benchmark.SCENARIOS,
                   default=['light', 'collision', 'balanced', 'stress'])
    p.add_argument('--modes', nargs='+', choices=benchmark.MODES,
                   default=['single', 'collision'])
    p.add_argument('--repetitions', type=int, default=3)
    p.add_argument('--frames', type=int, default=360)
    p.add_argument('--warmup', type=int, default=90)
    p.add_argument('--fixed-steps', type=int, default=1)
    p.add_argument('--seed', type=int, default=12345)
    p.add_argument('--colliders', type=int, default=None)
    p.add_argument('--layouts', nargs='+', choices=benchmark.LAYOUTS, default=['static'],
                   help='Compare fixed layouts or deterministic spatial transitions')
    p.add_argument('--phase-frames', type=int, default=120)
    p.add_argument('--settle-frames', type=int, default=12,
                   help='Exclude N frames after a layout transition from steady-phase timings')
    p.add_argument('--output', type=Path)
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.repetitions < 1 or args.frames < 1 or args.warmup < 0 or args.frames + args.warmup > 100000:
        print('[ERROR] Invalid repetitions/frames/warmup', file=sys.stderr)
        return 2
    if not 1 <= args.fixed_steps <= 16 or not 0 <= args.seed <= 1000000:
        print('[ERROR] Invalid fixed-steps/seed', file=sys.stderr)
        return 2
    if not 1 <= args.phase_frames <= 100000 or not 0 <= args.settle_frames < args.phase_frames:
        print('[ERROR] Invalid phase-frames/settle-frames', file=sys.stderr)
        return 2
    if args.colliders is not None and not 0 <= args.colliders <= 3000:
        print('[ERROR] Invalid collider count', file=sys.stderr)
        return 2
    output = (args.output or GAME / 'results' /
              ('adaptive_pair_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    if output.exists() and any(output.iterdir()):
        print(f'[ERROR] Directory is not empty: {output}', file=sys.stderr)
        return 2
    output.mkdir(parents=True, exist_ok=True)
    metadata = {'strategies': SETTINGS, 'scenarios': args.scenarios, 'modes': args.modes,
                'layouts': args.layouts, 'phase_frames': args.phase_frames,
                'settle_frames': args.settle_frames,
                'repetitions': args.repetitions, 'frames': args.frames,
                'warmup': args.warmup, 'fixed_steps': args.fixed_steps,
                'seed': args.seed, 'colliders': args.colliders,
                'design': 'paired, rotating strategy order; deterministic; real subprocess per sample'}
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    if args.build:
        print('[BUILD] Checking XEngine Benchmark Debug build (incremental)...', flush=True)
        # Python output must be unbuffered for live [BUILD]/[CACHE] diagnostics.
        run_command([sys.executable, '-u', str(ROOT / 'tools' / 'build.py'), 'game',
                     '--project', GAME.name, '--config', 'Debug'],
                    output / 'build.log', stream=True)
        print(f'[BUILD] Build check successful; log={output / "build.log"}', flush=True)
    executable = (args.exe or benchmark.executable_default()).resolve()
    if not executable.is_file():
        print(f'[ERROR] Executable missing: {executable}', file=sys.stderr)
        return 2
    all_rows: list[dict[str, object]] = []
    phase_rows: list[dict[str, object]] = []
    equivalences: list[dict[str, object]] = []
    total = len(args.layouts) * len(args.scenarios) * len(args.modes) * args.repetitions * len(STRATEGIES)
    run_number = 0
    for li, layout in enumerate(args.layouts):
        for si, scenario in enumerate(args.scenarios):
            for mi, mode in enumerate(args.modes):
                for rep in range(1, args.repetitions + 1):
                    references: dict[str, Path] = {}
                    samples: dict[str, dict[str, object]] = {}
                    for strategy in rotated_order(rep, si + li, mi):
                        run_number += 1
                        dest = output / 'runs' / layout / scenario / mode / f'run{rep:02d}' / strategy
                        dest.mkdir(parents=True, exist_ok=True)
                        broadphase, cell = SETTINGS[strategy]
                        command = [sys.executable, str(ROOT / 'tools' / 'run_benchmark.py'),
                                   '--exe', str(executable), '--timing', 'deterministic',
                                   '--fixed-steps', str(args.fixed_steps),
                                   '--frames', str(args.frames), '--warmup', str(args.warmup),
                                   '--scenarios', scenario, '--modes', mode, '--repetitions', '1',
                                   '--seed', str(args.seed), '--layout', layout,
                                   '--phase-frames', str(args.phase_frames), '--broadphase', broadphase,
                                   '--cell-size', cell, '--output', str(dest)]
                        if args.colliders is not None:
                            command.extend(['--colliders', str(args.colliders)])
                        print(f'[PAIR] {run_number}/{total} {layout}/{scenario}/{mode}/r{rep:02d} {strategy}', flush=True)
                        try:
                            run_command(command, output / f'log_{li}_{si}_{mi}_{rep}_{strategy}.txt')
                            metrics, profile = read_run(dest, scenario, mode, rep, strategy,
                                                        args.frames, args.warmup, args.fixed_steps)
                        except (OSError, ValueError, RuntimeError, KeyError) as exc:
                            (output / 'failures.txt').write_text(str(exc) + '\n', encoding='utf-8')
                            print(f'[FAIL] {exc}', file=sys.stderr)
                            return 1
                        references[strategy] = profile
                        metrics['layout'] = layout
                        samples[strategy] = metrics
                        phase_rows.extend(phase_metrics(profile, layout, scenario, mode, rep, strategy,
                                                       args.warmup, args.frames,
                                                       args.phase_frames, args.settle_frames))
                    baseline = references['bruteforce']
                    for strategy in STRATEGIES[1:]:
                        fail, detail = benchmark.compare_broadphase_profiles(baseline, references[strategy])
                        equivalences.append({'layout': layout, 'scenario': scenario, 'mode': mode,
                                             'repetition': rep, 'strategy': strategy,
                                             'equivalent': fail == 0, 'detail': detail})
                        if fail:
                            write_csv(output / 'equivalence.csv', equivalences)
                            (output / 'failures.txt').write_text(
                                f'{scenario}/{mode}/r{rep:02d}/{strategy}: {detail}\n', encoding='utf-8')
                            print(f'[FAIL] Equivalence: {detail}', file=sys.stderr)
                            return 1
                    all_rows.extend(samples[s] for s in STRATEGIES)
                    write_csv(output / 'runs.csv', all_rows)
                    write_csv(output / 'equivalence.csv', equivalences)
    write_csv(output / 'summary.csv', median_summary(all_rows))
    write_csv(output / 'paired_comparisons.csv', paired_comparisons(all_rows))
    write_csv(output / 'phase_summary.csv', phase_rows)
    write_csv(output / 'phase_paired.csv', paired_phases(phase_rows))
    print(f'[PAIR] {len(all_rows)} profiles; {len(equivalences)} equivalent pairs; '
          f'output={output}', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

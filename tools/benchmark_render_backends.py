#!/usr/bin/env python3
"""Benchmark every SDL2 renderer available to XEngine on the current machine.

Uses the existing fixed/adaptive sweep for each driver, preserving every
original result. Does not edit engine.json. With --install-ranking, installs
the validated order to the current user's machine-local renderer preferences.
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys

from run_benchmark import executable_default
from sweep_render_selector import read_presenter
from sweep_render_threshold import (ROOT, GAME, compare_profiles, read_profile,
                                    run_build_with_live_log, write_csv)


PROBE_MARKER = '[BENCH-RENDER-DRIVERS] '
WORKLOADS = ('baseline', 'culling', 'mixed', 'rotations_xyz')
PERCENTILES = ('p50', 'p95', 'p99')
MODES = ('fixed', 'adaptive')
REFERENCE_REFRESH_HZ = 60.0
VSYNC_EXCLUSION_MESSAGE = ('SDL reports an active VSync presenter; '
                           'benchmark requires VSync off')
VSYNC_EXCLUSION_CODE = 'vsync_unavailable'
TAIL_TIE_RELATIVE = 0.02
TAIL_TIE_ABSOLUTE_MS = 0.02
SPEED_TIE_RELATIVE = 0.02
SPEED_TIE_ABSOLUTE_MS = 0.01
# A single unusual repetition must not overturn the same overall recommendation.
CONSISTENCY_TIE_FRACTION = 0.10
STABILITY_FIELDS = ('frame_over_60hz_count', 'sdl_present_over_60hz_count',
                    'frame_over_display_count', 'sdl_present_over_display_count')
DISPLAY_FIELDS = ('video_driver', 'sdl_version', 'renderer_output_width',
                  'renderer_output_height', 'window_width', 'window_height',
                  'display_index', 'display_refresh_hz', 'window_fullscreen')
RUN_PRESENTER_FIELDS = DISPLAY_FIELDS + ('renderer_flags', 'renderer_accelerated',
                                         'renderer_software', 'renderer_targettexture')


def arguments(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backends', nargs='+',
                        help='Limit the available SDL backends to these names; default: all')
    parser.add_argument('--exclude-software', action='store_true',
                        help='Skip the software renderer when measuring GPU backends')
    parser.add_argument('--workloads', nargs='+', choices=WORKLOADS,
                        default=list(WORKLOADS))
    parser.add_argument('--repetitions', type=int, default=3)
    parser.add_argument('--sprites', type=int, default=2048)
    parser.add_argument('--threshold', type=int, default=1536)
    parser.add_argument('--width', type=int, default=1280,
                        help='Benchmark window width (default 1280; match the target game)')
    parser.add_argument('--height', type=int, default=720,
                        help='Benchmark window height (default 720; match the target game)')
    parser.add_argument('--fullscreen', action='store_true',
                        help='Use SDL desktop fullscreen; defaults to a window for comparable runs')
    parser.add_argument('--phase-frames', type=int, default=60)
    parser.add_argument('--frames', type=int, default=480)
    parser.add_argument('--warmup', type=int, default=60)
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--build-config', choices=('Debug', 'Release'), default='Release')
    parser.add_argument('--skip-build', action='store_true',
                        help='Reuse a binary already built with the new --list-render-backends probe')
    parser.add_argument('--show', action='append', choices=('warnings', 'commands'), default=[])
    parser.add_argument('--output', type=Path)
    parser.add_argument('--install-ranking', action='store_true',
                        help='Install the validated order for Windows/Linux auto on this machine')
    return parser.parse_args(argv)


def parse_available_backends(stdout: str) -> list[dict[str, object]]:
    lines = [line[len(PROBE_MARKER):] for line in stdout.splitlines()
             if line.startswith(PROBE_MARKER)]
    if len(lines) != 1:
        raise ValueError('Expected one SDL backend probe; build the updated benchmark')
    try:
        values = json.loads(lines[0])
    except json.JSONDecodeError as exc:
        raise ValueError('Invalid SDL backend probe JSON') from exc
    if (not isinstance(values, list) or not values or
        any(not isinstance(item, dict) or
            not isinstance(item.get('name'), str) or
            not re.fullmatch(r'[a-zA-Z0-9_-]{1,64}', item['name']) or
            type(item.get('flags')) is not int
            for item in values)):
        raise ValueError('Invalid/empty SDL backend list')
    normalized = [item['name'].lower() for item in values]
    if len(normalized) != len(set(normalized)):
        raise ValueError('Duplicate SDL backend names')
    return values


def has_vsync_exclusion_evidence(target: Path, workload: str) -> bool:
    """Verify an early, *single-run* VSync incompatibility, not arbitrary failures.

    A rejected backend is never assigned a performance score. Checking original
    nested evidence prevents a failed build, renderer mismatch, incomplete
    profile, or equivalence error being relabeled as a harmless exclusion.
    """
    first = target / workload / 'fixed_run01' / 'failures.txt'
    sweep = target / 'failures.txt'
    order = target / 'run_order.csv'
    if not (first.is_file() and sweep.is_file() and order.is_file()):
        return False
    try:
        lines = first.read_text(encoding='utf-8').splitlines()
        sweep_lines = sweep.read_text(encoding='utf-8').splitlines()
        with order.open(newline='', encoding='utf-8-sig') as stream:
            runs = list(csv.DictReader(stream))
    except (OSError, UnicodeError, csv.Error):
        return False
    return (len(lines) == 1 and
            lines[0] == f'render_parallel_run01: {VSYNC_EXCLUSION_MESSAGE}' and
            sweep_lines == [f'{workload}/1/fixed: benchmark exited 1'] and
            len(runs) == 1 and runs[0].get('workload') == workload and
            runs[0].get('repetition') == '1' and
            runs[0].get('mode') == 'fixed' and
            runs[0].get('runner_exit_code') == '1')


def write_backend_outcomes(path: Path, rows: list[dict[str, str]],
                           fields: tuple[str, ...]) -> None:
    """Emit an explicit header even when no driver failed or was excluded."""
    with path.open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def read_pairs(path: Path, *, backend: str, workloads: list[str],
               repetitions: int, warmup: int, frames: int) -> list[dict[str, str]]:
    with (path / 'paired_comparisons.csv').open(newline='', encoding='utf-8-sig') as stream:
        rows = list(csv.DictReader(stream))
    expected = {(name, str(rep)) for name in workloads for rep in range(1, repetitions + 1)}
    observed = [(row.get('workload'), row.get('repetition')) for row in rows]
    if len(rows) != len(expected) or set(observed) != expected:
        raise ValueError(f'{backend}: missing or duplicate workload/repetition pairs')
    if any(row.get('sdl_renderer', '').lower() != backend.lower() for row in rows):
        raise ValueError(f'{backend}: actual SDL renderer differs from the requested backend')
    for row in rows:
        if int(row['equivalent_frames']) != warmup + frames:
            raise ValueError(f'{backend}: unexpected fixed/adaptive equivalence frame count')
        for mode in MODES:
            for quantile in PERCENTILES:
                value = float(row[f'{mode}_frame_{quantile}_ms'])
                if not math.isfinite(value) or value <= 0:
                    raise ValueError(f'{backend}: invalid {mode} {quantile} sample')
    return rows


def summarize_backend(backend: str, pairs: list[dict[str, str]],
                      workloads: list[str]) -> tuple[list[dict[str, object]], dict[str, object]]:
    """Equal workload/mode weighting; median repeated runs before aggregate."""
    details: list[dict[str, object]] = []
    for workload in workloads:
        subset = [row for row in pairs if row['workload'] == workload]
        for mode in MODES:
            entry: dict[str, object] = {
                'backend': backend, 'workload': workload, 'mode': mode,
                'repetitions': len(subset),
            }
            for quantile in PERCENTILES:
                entry[f'frame_{quantile}_ms'] = statistics.median(
                    float(row[f'{mode}_frame_{quantile}_ms']) for row in subset)
            details.append(entry)
    score = {'backend': backend, 'workloads': len(workloads),
             'samples_per_workload': len(pairs) // len(workloads)}
    for quantile in PERCENTILES:
        # Geometric mean (positive values) avoids favoring a single heavy
        # workload or an abundance of samples in one workload.
        score[f'frame_{quantile}_score_ms'] = math.exp(
            statistics.mean(math.log(float(row[f'frame_{quantile}_ms']))
                            for row in details))
    return details, score


def percentile_samples(values: list[float], quantile: float) -> float:
    """Linear interpolation, including p99.9 over all measured frames."""
    ordered = sorted(values)
    if not ordered:
        raise ValueError('Cannot compute a percentile of zero measured frames')
    position = (len(ordered) - 1) * quantile
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def frame_stability(frame_times: list[float], present_times: list[float],
                    display_hz: float) -> dict[str, int | float]:
    """Stutter counters from the measured window only, not warmup/end frames.

    SDL_RenderPresent blocking is operationally defined as a call that itself
    exceeds a full display/60-Hz frame budget, regardless of its underlying
    reason (driver synchronization, compositor, etc.).
    """
    if (not frame_times or len(frame_times) != len(present_times) or
        not math.isfinite(display_hz) or display_hz <= 0):
        raise ValueError('Invalid or incomplete frame/stability samples')
    if any(not math.isfinite(value) or value < 0
           for value in frame_times + present_times):
        raise ValueError('Invalid frame or SDL_RenderPresent duration')
    display_budget = 1000.0 / display_hz
    reference_budget = 1000.0 / REFERENCE_REFRESH_HZ
    return {
        'measured_frames': len(frame_times),
        'display_refresh_hz': display_hz,
        'display_budget_ms': display_budget,
        'reference_budget_ms': reference_budget,
        'frame_p95_ms': percentile_samples(frame_times, .95),
        'frame_p99_ms': percentile_samples(frame_times, .99),
        'frame_p999_ms': percentile_samples(frame_times, .999),
        'frame_max_ms': max(frame_times),
        'frame_over_display_count': sum(value > display_budget for value in frame_times),
        'frame_over_60hz_count': sum(value > reference_budget for value in frame_times),
        'sdl_present_p99_ms': percentile_samples(present_times, .99),
        'sdl_present_p999_ms': percentile_samples(present_times, .999),
        'sdl_present_max_ms': max(present_times),
        'sdl_present_over_display_count': sum(value > display_budget for value in present_times),
        'sdl_present_over_60hz_count': sum(value > reference_budget for value in present_times),
    }


def read_backend_tails(root: Path, backend: str, *, workloads: list[str],
                       repetitions: int, warmup: int, frames: int,
                       reference_presenter: dict[str, object]) -> tuple[
                           list[dict[str, object]], list[dict[str, object]],
                           dict[str, object]]:
    """Read every backend's recorded frames; preserve per-run and per-mode data.

    Pooling is appropriate here because each workload/mode has the same number
    of repetitions and measured frames. A backend with missing telemetry is
    rejected rather than silently falling back to a p95-only recommendation.
    """
    per_run: list[dict[str, object]] = []
    per_mode: list[dict[str, object]] = []
    all_frame_times: list[float] = []
    all_present_times: list[float] = []
    expected_hz = float(reference_presenter['display_refresh_hz'])
    for workload in workloads:
        for mode in MODES:
            mode_frames: list[float] = []
            mode_presents: list[float] = []
            for repetition in range(1, repetitions + 1):
                run = root / workload / f'{mode}_run{repetition:02d}'
                actual = read_presenter(run)
                differences = {field: (reference_presenter.get(field), actual.get(field))
                               for field in RUN_PRESENTER_FIELDS
                               if reference_presenter.get(field) != actual.get(field)}
                if differences or str(actual.get('renderer_name', '')).lower() != backend:
                    raise ValueError(f'{backend}/{workload}/{mode}/{repetition}: '
                                     f'presenter/video mismatch {differences}')
                if actual.get('renderer_vsync') is not False:
                    raise ValueError(f'{backend}/{workload}/{mode}/{repetition}: '
                                     'VSync must be disabled for this comparison')
                _, measured = read_profile(run / 'render_parallel_run01.csv',
                                           warmup, frames)
                durations: list[float] = []
                presents: list[float] = []
                for row in measured:
                    try:
                        durations.append(float(row['frame_ms']))
                        presents.append(float(row['render_sdl_present_ms']))
                    except (KeyError, TypeError, ValueError) as exc:
                        raise ValueError(f'{backend}/{workload}/{mode}/{repetition}: '
                                         'missing/invalid SDL present telemetry') from exc
                if len(durations) != frames:
                    raise ValueError(f'{backend}/{workload}/{mode}/{repetition}: '
                                     'measured frame count differs from requested')
                result = frame_stability(durations, presents, expected_hz)
                per_run.append({'backend': backend, 'workload': workload, 'mode': mode,
                                'repetition': repetition, **result})
                mode_frames.extend(durations)
                mode_presents.extend(presents)
            per_mode.append({'backend': backend, 'workload': workload, 'mode': mode,
                             'repetitions': repetitions,
                             **frame_stability(mode_frames, mode_presents, expected_hz)})
            all_frame_times.extend(mode_frames)
            all_present_times.extend(mode_presents)
    return (per_run, per_mode,
            {'backend': backend, **frame_stability(
                all_frame_times, all_present_times, expected_hz)})


def stability_sort_key(row: dict[str, object]) -> tuple[float, ...]:
    """Legacy exact key; also used for strict stutter-count ordering.

    The numerator of each count is comparable because the same measured frame
    count and display refresh are required across all valid SDL backends.
    """
    return (float(row['frame_over_60hz_count']),
            float(row['sdl_present_over_60hz_count']),
            float(row['frame_over_display_count']),
            float(row['sdl_present_over_display_count']),
            float(row['frame_p999_ms']),
            float(row['frame_p95_score_ms']),
            float(row['frame_p99_score_ms']),
            float(row['frame_p50_score_ms']))


def _meaningful_faster(a: float, b: float, *, relative: float,
                       absolute: float) -> int:
    """Return -1 if a wins, +1 if b wins, 0 if the difference is too small."""
    if not all(math.isfinite(value) and value >= 0 for value in (a, b)):
        raise ValueError('Invalid backend timing in ranking')
    if abs(a - b) <= max(absolute, min(a, b) * relative):
        return 0
    return -1 if a < b else 1


def _fallback_backend_key(backend: str, platform: str) -> tuple[int, str]:
    """Deterministic tie-breaker, never a substitute for measured differences."""
    if platform == 'win32':
        defaults = ('direct3d11', 'direct3d', 'direct3d12',
                    'opengles2', 'opengl', 'software')
    else:
        defaults = ('opengl', 'opengles2', 'software')
    try:
        return defaults.index(backend), backend
    except ValueError:
        return len(defaults), backend


def _tolerance_groups(rows: list[dict[str, object]], field: str, *,
                      relative: float, absolute: float) -> list[list[dict[str, object]]]:
    """Disjoint bands anchored to the smallest observed timing (transitive).

    Comparing neighbours with isclose() is not transitive and could produce
    different results depending on the enumeration order of SDL backends.
    """
    remaining = sorted(rows, key=lambda row: (float(row[field]), str(row['backend'])))
    groups: list[list[dict[str, object]]] = []
    while remaining:
        anchor = float(remaining[0][field])
        limit = anchor + max(absolute, anchor * relative)
        size = 0
        while size < len(remaining) and float(remaining[size][field]) <= limit:
            size += 1
        groups.append(remaining[:size])
        remaining = remaining[size:]
    return groups


def _consistency_points(rows: list[dict[str, object]],
                        run_rows: list[dict[str, object]]) -> tuple[dict[str, int], int]:
    """Pairwise p95/p99 evidence across scenario medians and individual runs.

    Scenario medians earn three points; each individual run earns one. Timing
    differences <=2% or <=0.01 ms do not produce points. Every backend must
    have exactly the same set of workload/mode/repetition samples.
    """
    backends = {str(row['backend']) for row in rows}
    by_backend: dict[str, dict[tuple[str, str, int], dict[str, object]]] = {
        backend: {} for backend in backends}
    for run in run_rows:
        backend = str(run.get('backend'))
        if backend not in by_backend:
            continue
        key = (str(run['workload']), str(run['mode']), int(run['repetition']))
        if key in by_backend[backend]:
            raise ValueError('Duplicate backend scenario/mode/repetition')
        for metric in ('frame_p95_ms', 'frame_p99_ms'):
            value = float(run[metric])
            if not math.isfinite(value) or value <= 0:
                raise ValueError('Invalid backend per-run percentile')
        by_backend[backend][key] = run
    samples = [set(items) for items in by_backend.values()]
    if not samples or not samples[0] or any(keys != samples[0] for keys in samples):
        raise ValueError('Backend repetition telemetry is missing or inconsistent')
    scenarios = {(workload, mode) for workload, mode, _ in samples[0]}
    scores = {backend: 0 for backend in backends}
    from itertools import combinations
    possible_per_pair = 0
    for left, right in combinations(sorted(backends), 2):
        for workload, mode in sorted(scenarios):
            repetitions = sorted(rep for w, m, rep in samples[0]
                                 if (w, m) == (workload, mode))
            for metric in ('frame_p95_ms', 'frame_p99_ms'):
                left_values = [float(by_backend[left][workload, mode, rep][metric])
                               for rep in repetitions]
                right_values = [float(by_backend[right][workload, mode, rep][metric])
                                for rep in repetitions]
                comparisons = [(statistics.median(left_values),
                                statistics.median(right_values), 3)]
                comparisons.extend(zip(left_values, right_values,
                                       [1] * len(repetitions)))
                for a, b, weight in comparisons:
                    winner = _meaningful_faster(a, b, relative=SPEED_TIE_RELATIVE,
                                                absolute=SPEED_TIE_ABSOLUTE_MS)
                    if winner < 0:
                        scores[left] += weight
                    elif winner > 0:
                        scores[right] += weight
                possible_per_pair += 3 + len(repetitions)
    return scores, possible_per_pair


def rank_backends(rows: list[dict[str, object]],
                  run_rows: list[dict[str, object]], *,
                  platform: str | None = None) -> list[dict[str, object]]:
    """Stable, reproducible ranking based on stalls, tail bands and consistency.

    Only backends with identical hard stutter counts may compete within a
    p99.9 technical-tie band. Large tail differences still take precedence.
    """
    if not rows:
        return []
    platform = platform or sys.platform
    # Validate all run evidence even when there is only one stability tier.
    _consistency_points(rows, run_rows)
    ordered: list[dict[str, object]] = []
    remaining = sorted(rows, key=stability_sort_key)
    while remaining:
        counters = stability_sort_key(remaining[0])[:4]
        same = [row for row in remaining if stability_sort_key(row)[:4] == counters]
        remaining = remaining[len(same):]
        for tail_group in _tolerance_groups(
                same, 'frame_p999_ms', relative=TAIL_TIE_RELATIVE,
                absolute=TAIL_TIE_ABSOLUTE_MS):
            points, opportunities = _consistency_points(tail_group, run_rows)
            contenders = list(tail_group)
            while contenders:
                best_points = max(points[str(row['backend'])] for row in contenders)
                threshold = max(3, math.ceil(opportunities *
                                             CONSISTENCY_TIE_FRACTION))
                consistent = [row for row in contenders
                              if best_points - points[str(row['backend'])] <= threshold]
                # Both speed percentiles must be meaningfully different to
                # override the platform's deterministic final fallback.
                for p95_group in _tolerance_groups(
                        consistent, 'frame_p95_score_ms',
                        relative=SPEED_TIE_RELATIVE, absolute=SPEED_TIE_ABSOLUTE_MS):
                    for p99_group in _tolerance_groups(
                            p95_group, 'frame_p99_score_ms',
                            relative=SPEED_TIE_RELATIVE,
                            absolute=SPEED_TIE_ABSOLUTE_MS):
                        ordered.extend(sorted(
                            p99_group, key=lambda row: _fallback_backend_key(
                                str(row['backend']), platform)))
                selected = {str(row['backend']) for row in consistent}
                contenders = [row for row in contenders
                              if str(row['backend']) not in selected]
    return ordered


def compare_backend_invariants(paths: dict[str, Path], *,
                               workloads: list[str], repetitions: int,
                               warmup: int, frames: int) -> list[dict[str, object]]:
    """Cross-driver functional equivalence for both modes and every position."""
    results: list[dict[str, object]] = []
    names = list(paths)
    for left_index, left in enumerate(names):
        for right in names[left_index + 1:]:
            for workload in workloads:
                for rep in range(1, repetitions + 1):
                    for mode in MODES:
                        rel = Path(workload) / f'{mode}_run{rep:02d}' / 'render_parallel_run01.csv'
                        try:
                            before, _ = read_profile(paths[left] / rel, warmup, frames)
                            after, _ = read_profile(paths[right] / rel, warmup, frames)
                            if len(before) != warmup + frames or len(after) != len(before):
                                raise ValueError('different frame counts')
                            mismatch, detail = compare_profiles(before, after)
                            results.append({
                                'backend_a': left, 'backend_b': right,
                                'workload': workload, 'repetition': rep, 'mode': mode,
                                'positions_compared': len(before), 'equivalent': int(not mismatch),
                                'details': detail,
                            })
                        except (OSError, KeyError, ValueError, TypeError) as exc:
                            results.append({
                                'backend_a': left, 'backend_b': right,
                                'workload': workload, 'repetition': rep, 'mode': mode,
                                'positions_compared': 0, 'equivalent': 0,
                                'details': str(exc),
                            })
    return results


def render_report(ranking: list[dict[str, object]], *,
                  discovered: list[str], failed: list[dict[str, str]],
                  comparisons: list[dict[str, object]], reliable: bool,
                  details: list[dict[str, object]] | None = None,
                  excluded: list[dict[str, str]] | None = None,
                  run_details: list[dict[str, object]] | None = None) -> str:
    lines = [
        '# XEngine — comparación automática de backends SDL2',
        '',
        'Este resultado es **exploratorio**, específico del equipo y la sesión.',
        'La clasificación prioriza evitar tirones y bloqueos largos en SDL_RenderPresent,',
        'luego el presupuesto de pantalla. Diferencias de p99,9 de hasta el 2 %',
        '(mínimo 0,02 ms) son empates técnicos: se valoran p95/p99 por',
        'escenario y repetición y, si persiste el empate, el orden fijo por plataforma.',
        'Los scores p50/p95/p99 son medias geométricas de medianas por repetición,',
        'con el mismo peso por escenario/modo. p99,9 cuenta todos los frames',
        'medidos (sin calentamiento). No son cronómetros de GPU.',
        '',
        'Backends enumerados por SDL: ' + ', '.join(discovered) + '.',
        '',
        '| Posición | Backend | p50 score | p95 score | p99 score | p99,9 real | '
        '>Hz monitor | >60 Hz | Present >60 Hz | Peor frame | Peor Present |',
        '|--:|:--|--:|--:|--:|--:|--:|--:|--:|--:|--:|',
    ]
    for place, row in enumerate(ranking, 1):
        lines.append(f'| {place} | {row["backend"]} | '
                     f'{row["frame_p50_score_ms"]:.4f} | '
                     f'{row["frame_p95_score_ms"]:.4f} | '
                     f'{row["frame_p99_score_ms"]:.4f} | '
                     f'{row["frame_p999_ms"]:.3f} | '
                     f'{row["frame_over_display_count"]}/{row["measured_frames"]} | '
                     f'{row["frame_over_60hz_count"]} | '
                     f'{row["sdl_present_over_60hz_count"]} | '
                     f'{row["frame_max_ms"]:.2f} | '
                     f'{row["sdl_present_max_ms"]:.2f} |')
    lines.append('')
    if not ranking:
        lines.append('**Sin backend válido:** no existe un ganador medido.')
    else:
        best = ranking[0]
        fastest = min(ranking, key=lambda row: float(row['frame_p95_score_ms']))
        if reliable:
            lines.append(f'**Backend recomendado por estabilidad: `{best["backend"]}`.**')
        else:
            lines.append(f'**Primer puesto observado: `{best["backend"]}`, '
                         'sin recomendación validada.**')
        lines.append(f'**Mejor p95 medido entre backends válidos: `{fastest["backend"]}`.**')
        if fastest['backend'] != best['backend']:
            penalty = 100 * (float(best['frame_p95_score_ms']) /
                             float(fastest['frame_p95_score_ms']) - 1)
            lines.append(f'La recomendación queda **{penalty:.2f}%** por detrás '
                         'del mejor p95 por los criterios de clasificación. '
                         f'Tirones >60 Hz: **{best["frame_over_60hz_count"]}** '
                         f'frente a **{fastest["frame_over_60hz_count"]}**. '
                         f'Fotogramas >presupuesto monitor: '
                         f'**{best["frame_over_display_count"]}** frente a '
                         f'**{fastest["frame_over_display_count"]}**.')
        if len(ranking) > 1:
            runner_up = sorted(ranking, key=lambda row: float(row['frame_p95_score_ms']))[1]
            gain = 100 * (float(runner_up['frame_p95_score_ms']) /
                          float(fastest['frame_p95_score_ms']) - 1)
            lines.append(f'Ventaja relativa respecto al segundo: **{gain:.2f}%** '
                         '(cuanto menor p95, mejor).')
            if gain < 5:
                lines.append('**Resultado cercano (<5%) en p95:** revisar '
                             'consistencia por escenario y repetición; una '
                             'diferencia pequeña no prueba estabilidad superior.')
        if not reliable:
            lines.append('**Sin recomendación válida de configuración:** hubo '
                         'menos de dos backends válidos, fallos, cambios de '
                         'entorno o invariantes distintas. El primer puesto '
                         'solo describe las muestras conservadas.')
    lines.extend([
        '',
        f'Contrastes funcionales: {sum(int(x["equivalent"]) for x in comparisons)}'
        f'/{len(comparisons)} verificaciones entre drivers.'
        if comparisons else 'Contrastes funcionales: no hay dos backends válidos.',
        '',
        f'Backends con problemas: {len(failed)}.',
    ])
    for item in failed:
        lines.append(f'- `{item["backend"]}`: {item["reason"]}')
    excluded = excluded or []
    lines.append(f'Backends no comparables excluidos: {len(excluded)}.')
    for item in excluded:
        lines.append(f'- `{item["backend"]}` ({item["reason_code"]}): '
                     f'{item["reason"]}. Excluido del ranking y sus métricas; '
                     'puede seguir disponible como alternativa de arranque.')
    if reliable and len(ranking) > 1 and run_details:
        first, second = ranking[:2]
        identical_stalls = (stability_sort_key(first)[:4] ==
                            stability_sort_key(second)[:4])
        difference = abs(float(first['frame_p999_ms']) -
                         float(second['frame_p999_ms']))
        tolerance = max(TAIL_TIE_ABSOLUTE_MS,
                        min(float(first['frame_p999_ms']),
                            float(second['frame_p999_ms'])) * TAIL_TIE_RELATIVE)
        lines.extend(['', '## Evidencia de la recomendación', ''])
        if identical_stalls and difference <= tolerance:
            points, opportunities = _consistency_points([first, second], run_details)
            lines.append(
                f'**Empate técnico en p99,9** entre `{first["backend"]}` y '
                f'`{second["backend"]}`: diferencia {difference:.6f} ms '
                f'(tolerancia {tolerance:.4f} ms). Ninguno tiene ventaja '
                'suficiente en esa métrica para decidir por sí sola.')
            lines.append(
                'Consistencia de p95/p99: comparación de medianas por escenario/modo '
                'y repeticiones individuales, ignorando diferencias <=2 % o <=0,01 ms. '
                f'Puntos: **{first["backend"]} {points[str(first["backend"])]}**, '
                f'**{second["backend"]} {points[str(second["backend"])]}** '
                f'(máximo {opportunities} por pareja).')
        else:
            lines.append(
                'Los dos primeros no empatan simultáneamente en los contadores '
                'de tirones y en el margen de p99,9; se aplica primero el '
                'criterio estricto de estabilidad.')
    lines.extend(render_stability_sections(ranking, details or []))
    lines.extend([
        '',
        '## Cómo interpretarlo',
        '',
        '- La selección de backends es por bloques, no fotogramas simultáneos; '
        'temperatura, cachés, tareas externas, frecuencia del monitor y drivers pueden afectar.',
        '- Las invariantes comparan física y contadores de draw, **no los píxeles**.',
        '- Present largo no prueba un problema de shader, GPU o controlador; '
        'puede ser sincronización u otras causas. Revisa los perfiles originales.',
        '- Los umbrales de empate reducen cambios causados por ruido minúsculo, '
        'pero no garantizan el mismo ganador entre sesiones si las mediciones '
        'varían de forma apreciable; no se conserva el ranking anterior.',
        '- El ranking conservador de fluidez puede sacrificar velocidad p95; '
        'no es automáticamente un default universal.',
        '- La salida de cada driver se conserva bajo `backends/<nombre>/`; '
        'examinar `paired_comparisons.csv`, `phase_tail_frames.csv`, '
        '`phase_pass_results.csv` y `metadata.json` antes de sacar conclusiones.',
        '- Repetir en orden inverso y A/B/A si hay una diferencia relevante. '
        'Comparar capturas del mismo estado deterministicamente antes de modificar defaults.',
        '- **Este benchmark nunca cambia `config.json` ni `engine.json`.**',
        '',
    ])
    return '\n'.join(lines)


def render_stability_sections(ranking: list[dict[str, object]],
                              details: list[dict[str, object]]) -> list[str]:
    """Per-workload evidence, including where stalls occur and fixed/adaptive."""
    if not ranking or not details:
        return []
    refresh = float(ranking[0]['display_refresh_hz'])
    budget = float(ranking[0]['display_budget_ms'])
    lines = [
        '',
        '## Condiciones y latencias extremas',
        '',
        f'Monitor **{refresh:g} Hz**: presupuesto de frame **{budget:.3f} ms**. '
        'Referencia **60 Hz**: **16,667 ms**. Los contadores son estrictamente '
        'superiores al umbral. Una llamada lenta a SDL_RenderPresent puede '
        'bloquear la CPU; el cronómetro no determina por qué.',
        '',
        '## Desglose completo por escenario y modo',
        '',
        'Percentiles sobre las repeticiones agrupadas; sin warmup. '
        'Los recuentos son números absolutos de fotogramas medidos.',
        '',
        '| Backend | Escenario | Modo | Frames | p95 | p99 | p99,9 | '
        '>pantalla | >60Hz | Present >60Hz | Máx. frame | Máx. Present |',
        '|:--|:--|:--|--:|--:|--:|--:|--:|--:|--:|--:|--:|',
    ]
    for item in details:
        lines.append(
            f'| {item["backend"]} | {item["workload"]} | {item["mode"]} | '
            f'{item["measured_frames"]} | {item["frame_p95_ms"]:.3f} | '
            f'{item["frame_p99_ms"]:.3f} | {item["frame_p999_ms"]:.3f} | '
            f'{item["frame_over_display_count"]} | '
            f'{item["frame_over_60hz_count"]} | '
            f'{item["sdl_present_over_60hz_count"]} | '
            f'{item["frame_max_ms"]:.2f} | {item["sdl_present_max_ms"]:.2f} |')
    lines.extend(['', '## Distribución de los fotogramas lentos', ''])
    any_stall = False
    for backend in [str(row['backend']) for row in ranking]:
        breakdown: dict[str, int] = {}
        modes: dict[str, int] = {}
        for item in details:
            if item['backend'] != backend:
                continue
            count = int(item['frame_over_60hz_count'])
            if count:
                name = str(item['workload'])
                breakdown[name] = breakdown.get(name, 0) + count
                modes[f'{name}/{item["mode"]}'] = count
        if breakdown:
            any_stall = True
            summary = ', '.join(f'{name}: {count}' for name, count in breakdown.items())
            detailed = ', '.join(f'{name}: {count}' for name, count in modes.items())
            lines.append(f'- **{backend}**: {summary} (desglose: {detailed}).')
            if len(breakdown) == 1:
                lines.append('  - Solo un escenario registra tirones >16,667 ms. '
                             'Podría reflejar inicialización o calentamiento '
                             'del controlador, pero **no está confirmado**.')
    if not any_stall:
        lines.append('No se observaron frames >16,667 ms en la ventana medida.')
    chosen = str(ranking[0]['backend'])
    lines.extend([
        '',
        f'## fixed frente a adaptive en el primer backend: {chosen}',
        '',
        'p95 = mediana del p95 por repetición, conservando la metodología '
        'original de comparación de modos.',
        '',
        '| Escenario | fixed p95 (ms) | adaptive p95 (ms) | Ganador por p95 |',
        '|:--|--:|--:|:--|',
    ])
    selected = [item for item in details if item['backend'] == chosen]
    fixed_wins = adaptive_wins = 0
    for workload in dict.fromkeys(str(row['workload']) for row in selected):
        modes = {str(row['mode']): row for row in selected
                 if row['workload'] == workload}
        if set(modes) != set(MODES):
            continue
        fixed = float(modes['fixed']['frame_p95_run_median_ms'])
        adaptive = float(modes['adaptive']['frame_p95_run_median_ms'])
        winner = ('fixed' if fixed < adaptive else
                  'adaptive' if adaptive < fixed else 'empate')
        fixed_wins += winner == 'fixed'
        adaptive_wins += winner == 'adaptive'
        lines.append(f'| {workload} | {fixed:.3f} | {adaptive:.3f} | {winner} |')
    lines.append(f'Escenarios ganados por p95: **fixed {fixed_wins}**, '
                 f'**adaptive {adaptive_wins}**. Esto no cambia '
                 'parallelSelection automáticamente.')
    return lines


def main(argv: list[str] | None = None) -> int:
    args = arguments(argv)
    if (args.repetitions < 2 or args.frames < 4 * args.phase_frames or
        args.sprites < 1 or args.sprites > 10000 or args.threshold < 1 or
        args.warmup < 0 or args.timeout < 1 or
        not 320 <= args.width <= 16384 or not 240 <= args.height <= 16384 or
        len(args.workloads) != len(set(args.workloads))):
        print('[ERROR] Invalid backend benchmark parameters', file=sys.stderr)
        return 2
    output = (args.output or GAME / 'results' /
              ('render_backends_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))).resolve()
    if output.exists() and any(output.iterdir()):
        print('[ERROR] Refusing to overwrite a nonempty results directory', file=sys.stderr)
        return 2
    output.mkdir(parents=True, exist_ok=True)
    if not args.skip_build:
        command = [sys.executable, str(ROOT / 'tools/build.py'), 'game',
                   '--project', GAME.name, '--config', args.build_config,
                   '--raw-log', str(output / 'build.log')]
        for show in args.show:
            command.extend(['--show', show])
        rc = run_build_with_live_log(command, cwd=ROOT,
                                     log_path=output / 'build_console.log')
        if rc:
            print('[ERROR] Build failed; no backends tested', file=sys.stderr)
            return 1
    executable = executable_default(args.build_config)
    if not executable.is_file():
        print(f'[ERROR] Benchmark binary missing: {executable}', file=sys.stderr)
        return 1
    probe_env = os.environ.copy()
    probe_env.pop('SDL_RENDER_DRIVER', None)
    try:
        probe = subprocess.run([str(executable), '--list-render-backends'],
                               cwd=executable.parent, env=probe_env,
                               text=True, capture_output=True, timeout=args.timeout,
                               check=False)
        if probe.returncode:
            raise ValueError(f'SDL probe exited {probe.returncode}: {probe.stderr[-300:]}')
        discovered = parse_available_backends(probe.stdout)
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print(f'[ERROR] Cannot discover SDL renderers: {exc}', file=sys.stderr)
        return 1
    (output / 'available_backends.json').write_text(
        json.dumps(discovered, indent=2) + '\n', encoding='utf-8')
    candidates = [item['name'].lower() for item in discovered]
    if args.exclude_software:
        candidates = [name for name in candidates if name != 'software']
    if args.backends:
        requested = [name.lower() for name in args.backends]
        if len(requested) != len(set(requested)) or any(name not in candidates for name in requested):
            print(f'[ERROR] Backend not available (or duplicate). Available: {candidates}', file=sys.stderr)
            return 2
        candidates = requested
    if not candidates:
        print('[ERROR] No eligible backend found', file=sys.stderr)
        return 1
    print(f'[BACKENDS] SDL candidates: {", ".join(candidates)}', flush=True)
    successful: dict[str, Path] = {}
    details: list[dict[str, object]] = []
    tail_runs: list[dict[str, object]] = []
    tail_details: list[dict[str, object]] = []
    ranking: list[dict[str, object]] = []
    failed: list[dict[str, str]] = []
    excluded: list[dict[str, str]] = []
    reference_presenter: dict[str, object] | None = None
    for driver in candidates:
        target = output / 'backends' / driver
        environment = os.environ.copy()
        environment['SDL_RENDER_DRIVER'] = driver
        command = [sys.executable, str(ROOT / 'tools/sweep_render_selector.py'),
                   '--build-config', args.build_config,
                   '--workloads', *args.workloads,
                   '--sprites', str(args.sprites),
                   '--width', str(args.width), '--height', str(args.height),
                   '--threshold', str(args.threshold),
                   '--phase-frames', str(args.phase_frames),
                   '--frames', str(args.frames),
                   '--warmup', str(args.warmup),
                   '--repetitions', str(args.repetitions),
                   '--timeout', str(args.timeout),
                   '--output', str(target)]
        if args.fullscreen:
            command.append('--fullscreen')
        print(f'[BACKEND] start={driver}', flush=True)
        try:
            rc = subprocess.call(command, cwd=ROOT, env=environment)
            if rc or (target / 'failures.txt').is_file():
                raise ValueError(f'sweep returned {rc}; see {target}/failures.txt')
            pairs = read_pairs(target, backend=driver, workloads=args.workloads,
                               repetitions=args.repetitions, warmup=args.warmup,
                               frames=args.frames)
            # Verify actual backend and stable screen conditions beyond the CSV
            # label. SDL backend identity must agree for fixed/adaptive.
            presenter = read_presenter(target / args.workloads[0] / 'fixed_run01')
            if str(presenter.get('renderer_name', '')).lower() != driver:
                raise ValueError('SDL presenter did not use the requested renderer')
            if reference_presenter is not None:
                differences = {field: (reference_presenter.get(field), presenter.get(field))
                               for field in DISPLAY_FIELDS
                               if reference_presenter.get(field) != presenter.get(field)}
                if differences:
                    raise ValueError(f'display/video conditions changed: {differences}')
            stats, score = summarize_backend(driver, pairs, args.workloads)
            run_tails, mode_tails, overall_tails = read_backend_tails(
                target, driver, workloads=args.workloads,
                repetitions=args.repetitions, warmup=args.warmup,
                frames=args.frames, reference_presenter=presenter)
            summary_by_mode = {(str(row['workload']), str(row['mode'])): row
                               for row in stats}
            for row in mode_tails:
                original = summary_by_mode[(str(row['workload']), str(row['mode']))]
                row['frame_p95_run_median_ms'] = original['frame_p95_ms']
                row['frame_p99_run_median_ms'] = original['frame_p99_ms']
            score.update({key: value for key, value in overall_tails.items()
                          if key != 'backend'})
            details.extend(stats)
            tail_runs.extend(run_tails)
            tail_details.extend(mode_tails)
            ranking.append(score)
            successful[driver] = target
            if reference_presenter is None:
                reference_presenter = presenter
            print(f'[BACKEND] valid={driver} p95-score={score["frame_p95_score_ms"]:.4f} ms '
                  f'p99.9={score["frame_p999_ms"]:.3f} ms '
                  f'frames>60Hz={score["frame_over_60hz_count"]} '
                  f'frames>display={score["frame_over_display_count"]} '
                  f'SDL-present>60Hz={score["sdl_present_over_60hz_count"]}',
                  flush=True)
        except (OSError, KeyError, TypeError, ValueError, ZeroDivisionError) as exc:
            if has_vsync_exclusion_evidence(target, args.workloads[0]):
                excluded.append({'backend': driver,
                                 'reason_code': VSYNC_EXCLUSION_CODE,
                                 'reason': VSYNC_EXCLUSION_MESSAGE})
                print(f'[BACKEND] non-comparable={driver}: {VSYNC_EXCLUSION_MESSAGE}',
                      flush=True)
            else:
                failed.append({'backend': driver, 'reason': str(exc)})
                print(f'[BACKEND] failed={driver}: {exc}', file=sys.stderr, flush=True)
    comparisons = compare_backend_invariants(
        successful, workloads=args.workloads, repetitions=args.repetitions,
        warmup=args.warmup, frames=args.frames)
    reliable = (len(ranking) >= 2 and not failed and
                bool(comparisons) and all(int(row['equivalent']) for row in comparisons))
    ranking = rank_backends(ranking, tail_runs)
    write_csv(output / 'backend_results.csv', details)
    write_csv(output / 'backend_ranking.csv', ranking)
    write_csv(output / 'backend_tail_runs.csv', tail_runs)
    write_csv(output / 'backend_tail_modes.csv', tail_details)
    write_csv(output / 'backend_equivalence.csv', comparisons)
    write_backend_outcomes(output / 'backend_failures.csv', failed,
                           ('backend', 'reason'))
    write_backend_outcomes(output / 'backend_exclusions.csv', excluded,
                           ('backend', 'reason_code', 'reason'))
    (output / 'report.md').write_text(render_report(
        ranking, discovered=[item['name'] for item in discovered],
        failed=failed, comparisons=comparisons, reliable=reliable,
        details=tail_details, excluded=excluded, run_details=tail_runs),
        encoding='utf-8')
    (output / 'metadata.json').write_text(json.dumps({
        'platform': sys.platform, 'build_config': args.build_config,
        'workloads': args.workloads, 'repetitions': args.repetitions,
        'sprites': args.sprites, 'threshold': args.threshold,
        'requested_window_width': args.width,
        'requested_window_height': args.height,
        'requested_fullscreen': args.fullscreen,
        'phase_frames': args.phase_frames, 'frames': args.frames,
        'warmup': args.warmup, 'candidate_backends': candidates,
        'excluded_backends': [item['backend'] for item in excluded],
        'ranking_valid': reliable, 'default_config_modified': False,
        'selection_policy_version': 2,
        'selection_policy': ('fewest frames >60Hz; fewest SDL_RenderPresent '
                             'calls >60Hz; fewest frames over display budget; '
                             'fewest SDL_RenderPresent over display budget; '
                             'pooled p99.9 with 2%/0.02ms technical tie; '
                             'p95/p99 workload/repetition consistency; '
                             'meaningful p95/p99 differences; platform fallback'),
        'reference_budget_ms': 1000.0 / REFERENCE_REFRESH_HZ,
        'warning': 'No automated pixel-for-pixel verification or OS/GPU workload control.',
    }, indent=2) + '\n', encoding='utf-8')
    if ranking:
        print(f'[BACKENDS] stability-first={ranking[0]["backend"]} '
              f'p95-only={min(ranking, key=lambda row: float(row["frame_p95_score_ms"]))["backend"]}',
              flush=True)
    print(f'[BACKENDS] tested={len(candidates)} valid={len(ranking)} '
          f'non_comparable={len(excluded)} failed={len(failed)} '
          f'functional_checks={len(comparisons)} reliable={int(reliable)} '
          f'report={output / "report.md"}', flush=True)
    if args.install_ranking:
        if not reliable:
            print('[ERROR] Ranking not installed because comparison is unreliable',
                  file=sys.stderr)
        else:
            from install_render_backend_ranking import install
            try:
                installed, record = install(output)
                print(f'[BACKENDS] installed={installed} '
                      f'order={" -> ".join(record["ranked_backends"])}', flush=True)
            except (OSError, ValueError, KeyError, TypeError) as exc:
                print(f'[ERROR] Benchmark passed but ranking installation failed: {exc}',
                      file=sys.stderr)
                return 1
    return 0 if reliable else 1


if __name__ == '__main__':
    raise SystemExit(main())

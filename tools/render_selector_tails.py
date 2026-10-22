#!/usr/bin/env python3
"""Inspect p95/p99 frame tails in a fixed/adaptive selector sweep.

Accepts either a sweep directory or a ZIP of one or more sweep directories.
Top-level timing stages do not overlap; render_* subtimers are nested and
MUST NOT be added to frame_ms or interpreted as proven causal attribution.
"""
from __future__ import annotations

import argparse
import csv
import io
import json
import math
from pathlib import Path
import statistics
import sys
import zipfile

from sweep_render_threshold import compare_profiles, write_csv


TOP_LEVEL = ('input_ms', 'fixed_ms', 'update_ms', 'render_present_ms')
RENDER_DETAIL = ('render_world_ms', 'render_record_ms', 'render_prepare_ms',
                 'render_submit_ms', 'render_only_present_ms',
                 'render_worker_wait_ms', 'render_selector_decision_ms')
TIMINGS = TOP_LEVEL + RENDER_DETAIL
EXACT_PRESENT = 'render_sdl_present_ms'
RENDER_SPLIT = ('render_cpu_pre_end_ms', 'render_sdl_present_ms',
                'render_end_cleanup_ms')
ROUTE_TELEMETRY = ('render_path_reason', 'render_parallel_executed',
                   'render_parallel_fallback', 'render_executed_switched',
                   'render_eligible_current', 'render_weighted_units_current',
                   'render_effective_units_current', 'render_estimated_offscreen_sprites',
                   'render_visibility_skipped_sprites')


def percentile(values: list[float], quantile: float) -> float:
    ordered = sorted(values)
    if not ordered:
        raise ValueError('empty measured profile')
    index = (len(ordered) - 1) * quantile
    lo = int(index)
    return ordered[lo] + (ordered[min(lo + 1, len(ordered) - 1)] - ordered[lo]) * (index - lo)


def analyze_run(rows: list[dict[str, str]], *, build: str, workload: str,
                repetition: int, mode: str, phase_frames: int
                ) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    """Return frame-level p95+ observations and descriptive tail summaries.

    The p95 and p99 cutoffs are calculated independently for EACH run;
    samples selected from two runs are not synchronized latency spikes.
    """
    if not rows or phase_frames < 1:
        raise ValueError('empty rows or invalid phase length')
    required = {'frame', 'frame_ms', *TIMINGS}
    if not required <= rows[0].keys():
        raise ValueError(f'missing tail profiler fields: {sorted(required - rows[0].keys())}')
    exact_present = EXACT_PRESENT in rows[0] and rows[0][EXACT_PRESENT] != ''
    if any(not math.isfinite(float(row[key])) or float(row[key]) < 0
           for row in rows for key in ('frame_ms', *TIMINGS, *((EXACT_PRESENT,) if exact_present else ()))):
        raise ValueError('nonfinite or negative frame/stage timing in measured profile')
    if any(float(row['render_only_present_ms']) > float(row['render_present_ms']) + .02
           or (exact_present and float(row[EXACT_PRESENT]) > float(row['render_only_present_ms']) + .02)
           for row in rows):
        raise ValueError('nested render timing exceeds its parent by more than 0.02 ms')
    p95 = percentile([float(row['frame_ms']) for row in rows], .95)
    p99 = percentile([float(row['frame_ms']) for row in rows], .99)
    shared = {'build_config': build, 'workload': workload,
              'repetition': repetition, 'mode': mode}

    frames = []
    for row in rows:
        duration = float(row['frame_ms'])
        if duration < p95:
            continue
        frame = int(row['frame'])
        times = {key: float(row[key]) for key in TIMINGS}
        accounted = sum(times[key] for key in TOP_LEVEL)
        largest = max(TOP_LEVEL, key=lambda key: times[key])
        before_end = max(0., times['render_present_ms'] - times['render_only_present_ms'])
        sdl_present = float(row[EXACT_PRESENT]) if exact_present else None
        cleanup = max(0., times['render_only_present_ms'] - sdl_present) if exact_present else None
        segments = {'render_cpu_pre_end_ms': before_end}
        if exact_present:
            segments['render_sdl_present_ms'] = sdl_present
            segments['render_end_cleanup_ms'] = cleanup
        else:
            segments['render_end_unsplit_ms'] = times['render_only_present_ms']
        frames.append({**shared, 'frame': frame,
                       'phase': (frame // phase_frames) % 4,
                       'is_p99_tail': int(duration >= p99),
                       'frame_ms': duration, 'unaccounted_ms': duration - accounted,
                       'largest_top_level_stage': largest, **times,
                       'render_cpu_pre_end_ms': before_end,
                       'render_sdl_present_ms': sdl_present if exact_present else '',
                       'render_end_cleanup_ms': cleanup if exact_present else '',
                       'dominant_render_segment': max(segments, key=segments.get),
                       **{field: row.get(field, '') for field in ROUTE_TELEMETRY}})

    summaries = []
    for name, cutoff in (('p95', p95), ('p99', p99)):
        selected = [frame for frame in frames if frame['frame_ms'] >= cutoff]
        totals = {stage: sum(float(row[stage]) for row in selected) for stage in TOP_LEVEL}
        total_frame = sum(float(row['frame_ms']) for row in selected)
        report: dict[str, object] = {
            **shared, 'tail': name, 'threshold_ms': cutoff,
            'tail_frames': len(selected),
            'frame_tail_median_ms': statistics.median(float(r['frame_ms']) for r in selected),
            'frame_tail_max_ms': max(float(r['frame_ms']) for r in selected),
            'unaccounted_median_ms': statistics.median(float(r['unaccounted_ms']) for r in selected),
            'actual_parallel_tail_frames': sum(r['render_parallel_executed'] in ('1', '1.0')
                                               for r in selected),
            'exact_sdl_present_available': int(exact_present),
        }
        for phase in range(4):
            report[f'phase_{phase}_frames'] = sum(row['phase'] == phase for row in selected)
        for stage in TOP_LEVEL:
            report[f'{stage}_tail_median'] = statistics.median(float(r[stage]) for r in selected)
            report[f'{stage}_tail_share_pct'] = 100 * totals[stage] / total_frame if total_frame else 0.
            report[f'{stage}_largest_count'] = sum(r['largest_top_level_stage'] == stage for r in selected)
        for stage in RENDER_DETAIL:
            report[f'{stage}_tail_median'] = statistics.median(float(r[stage]) for r in selected)
        report['render_cpu_pre_end_ms_tail_median'] = statistics.median(
            float(r['render_cpu_pre_end_ms']) for r in selected)
        if exact_present:
            for stage in (EXACT_PRESENT, 'render_end_cleanup_ms'):
                report[f'{stage}_tail_median'] = statistics.median(float(r[stage]) for r in selected)
        else:
            report[f'{EXACT_PRESENT}_tail_median'] = ''
            report['render_end_cleanup_ms_tail_median'] = ''
        for segment in (*RENDER_SPLIT, 'render_end_unsplit_ms'):
            report[f'{segment}_dominant_count'] = sum(
                r['dominant_render_segment'] == segment for r in selected)
        summaries.append(report)
    return frames, summaries


def analyze_phase_run(rows: list[dict[str, str]], *, build: str, workload: str,
                      repetition: int, mode: str, phase_frames: int,
                      phase_loads: tuple[int, ...] = (55, 75, 100, 65)
                      ) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    """Independent within-phase p95/p99 cutoffs; complements full-run tails."""
    if len(phase_loads) != 4:
        raise ValueError('exactly four phase loads are required')
    result_frames: list[dict[str, object]] = []
    result_summaries: list[dict[str, object]] = []
    for phase in range(4):
        window = [row for row in rows if (int(row['frame']) // phase_frames) % 4 == phase]
        if not window:
            raise ValueError(f'missing measured phase {phase}')
        frames, summaries = analyze_run(window, build=build, workload=workload,
            repetition=repetition, mode=mode, phase_frames=phase_frames)
        common = {'phase': phase, 'phase_load_pct': phase_loads[phase]}
        result_frames.extend({**frame, **common} for frame in frames)
        result_summaries.extend({**summary, **common,
            'phase_measured_frames': len(window),
            'phase_frame_p50_ms': percentile([float(r['frame_ms']) for r in window], .5)}
            for summary in summaries)
    return result_frames, result_summaries


def analyze_existing_all(source: Path) -> tuple[list[dict[str, object]],
                                                list[dict[str, object]],
                                                list[dict[str, object]],
                                                list[dict[str, object]], int]:
    """Inspect only completed, equivalent fixed/adaptive pairs in an archive/dir."""
    archive = zipfile.ZipFile(source) if source.is_file() else None
    try:
        members = archive.namelist() if archive else [str(p.relative_to(source)).replace('\\', '/')
                                                      for p in source.rglob('*') if p.is_file()]
        metadata_paths = [name for name in members if name.endswith('metadata.json') and
                          (name.count('/') == 0 or name.split('/')[-2].startswith('render_selector_'))]
        all_frames: list[dict[str, object]] = []
        all_summaries: list[dict[str, object]] = []
        phase_frames: list[dict[str, object]] = []
        phase_summaries: list[dict[str, object]] = []
        pairs = 0

        def read(name: str) -> str:
            if archive:
                return archive.read(name).decode('utf-8-sig')
            return (source / name).read_text(encoding='utf-8-sig')

        for name in sorted(metadata_paths):
            parent = name.removesuffix('metadata.json')
            meta = json.loads(read(name))
            if meta.get('selector_modes') != ['fixed', 'adaptive']:
                continue
            for workload in meta['workloads']:
                for repetition in range(1, int(meta['repetitions']) + 1):
                    captures: dict[str, tuple[list[dict[str, str]], list[dict[str, str]]]] = {}
                    for mode in ('fixed', 'adaptive'):
                        profile = f'{parent}{workload}/{mode}_run{repetition:02d}/render_parallel_run01.csv'
                        data = list(csv.DictReader(io.StringIO(read(profile))))
                        warmup, frames = int(meta['warmup']), int(meta['frames'])
                        if len(data) < warmup + frames:
                            raise ValueError(f'{profile}: missing measured frames')
                        captures[mode] = data[:warmup + frames], data[warmup:warmup + frames]
                    equivalent, detail = compare_profiles(captures['fixed'][0], captures['adaptive'][0])
                    if equivalent:
                        raise ValueError(f'{workload}/{repetition}: NOT EQUIVALENT: {detail}')
                    pairs += 1
                    for mode in ('fixed', 'adaptive'):
                        raw, stats = analyze_run(captures[mode][1],
                            build=str(meta['build_config']), workload=workload,
                            repetition=repetition, mode=mode,
                            phase_frames=int(meta['phase_frames']))
                        all_frames.extend(raw)
                        all_summaries.extend(stats)
                        phase_raw, phase_stats = analyze_phase_run(captures[mode][1],
                            build=str(meta['build_config']), workload=workload,
                            repetition=repetition, mode=mode,
                            phase_frames=int(meta['phase_frames']),
                            phase_loads=tuple(meta.get('render_cycle_percentages', [55, 75, 100, 65])))
                        phase_frames.extend(phase_raw)
                        phase_summaries.extend(phase_stats)
        if not pairs:
            raise ValueError('no fixed/adaptive selector sweeps found in input')
        return all_frames, all_summaries, phase_frames, phase_summaries, pairs
    finally:
        if archive:
            archive.close()


def analyze_existing(source: Path) -> tuple[list[dict[str, object]], list[dict[str, object]], int]:
    """Backward-compatible whole-run tail API."""
    frames, summaries, _, _, pairs = analyze_existing_all(source)
    return frames, summaries, pairs


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path,
                        help='render_selector_* directory or ZIP containing selector sweeps')
    parser.add_argument('--output', required=True, type=Path,
                        help='directory for tail_frames.csv and tail_summary.csv')
    args = parser.parse_args(argv)
    try:
        frames, summaries, phase_frames, phase_summaries, pairs = analyze_existing_all(args.input)
        args.output.mkdir(parents=True, exist_ok=True)
        write_csv(args.output / 'tail_frames.csv', frames)
        write_csv(args.output / 'tail_summary.csv', summaries)
        write_csv(args.output / 'phase_tail_frames.csv', phase_frames)
        write_csv(args.output / 'phase_tail_summary.csv', phase_summaries)
    except (OSError, KeyError, ValueError, json.JSONDecodeError, zipfile.BadZipFile) as exc:
        print(f'[FAIL] Tail analysis: {exc}', file=sys.stderr)
        return 1
    print(f'[SELECTOR-TAILS] equivalent_pairs={pairs} p95_frames={len(frames)} '
          f'summaries={len(summaries)} phase_p95_frames={len(phase_frames)} '
          f'phase_summaries={len(phase_summaries)} errors=0 output={args.output}', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

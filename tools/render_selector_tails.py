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


def analyze_phase_passes(rows: list[dict[str, str]], *, build: str,
                         workload: str, repetition: int, mode: str,
                         phase_frames: int, warmup: int,
                         phase_loads: tuple[int, ...] = (55, 75, 100, 65)
                         ) -> list[dict[str, object]]:
    """One record per contiguous phase visit, preserving first/second visits.

    Uses absolute profile frame numbers (not zero-based measured offsets),
    because the phase cycle begins during warmup. Partial phase windows are
    explicitly labelled; old captures without exact SDL timing remain readable.
    """
    if phase_frames < 1 or warmup < 0 or len(phase_loads) != 4 or not rows:
        raise ValueError('invalid phase-pass configuration or empty samples')
    windows: dict[int, list[dict[str, str]]] = {}
    for row in rows:
        frame = int(row['frame'])
        if frame < warmup:
            raise ValueError('phase pass rows must contain measured frames only')
        windows.setdefault(frame // phase_frames, []).append(row)
    seen: dict[int, int] = {}
    result: list[dict[str, object]] = []
    for segment, window in sorted(windows.items()):
        phase = segment % 4
        seen[phase] = seen.get(phase, 0) + 1
        frame_numbers = [int(row['frame']) for row in window]
        if (len(window) > phase_frames or len(set(frame_numbers)) != len(window) or
            any(frame // phase_frames != segment for frame in frame_numbers)):
            raise ValueError('malformed phase segment frames')
        # A valid full visit must cover the exact contiguous phase interval.
        complete = (len(window) == phase_frames and
                    min(frame_numbers) == segment * phase_frames and
                    max(frame_numbers) == (segment + 1) * phase_frames - 1)
        ordered = sorted(window, key=lambda row: int(row['frame']))
        exact = all(row.get(EXACT_PRESENT, '') != '' for row in ordered)
        vals = lambda field: [float(row[field]) for row in ordered]
        full = vals('frame_ms')
        end = vals('render_only_present_ms')
        render = vals('render_present_ms')
        first = full[:10]
        remaining = full[10:]
        first_cutoff = percentile(full, .95)
        slow = [value >= first_cutoff for value in full]
        streak = longest = 0
        for is_slow in slow:
            streak = streak + 1 if is_slow else 0
            longest = max(longest, streak)
        data: dict[str, object] = {
            'build_config': build, 'workload': workload, 'repetition': repetition,
            'mode': mode, 'phase': phase, 'phase_load_pct': phase_loads[phase],
            'pass_index': seen[phase], 'phase_start_frame': segment * phase_frames,
            'measured_first_frame': min(frame_numbers),
            'measured_last_frame': max(frame_numbers),
            'frame_count': len(window), 'complete_phase': int(complete),
            'frame_p50_ms': percentile(full, .5),
            'frame_p95_ms': percentile(full, .95),
            'frame_p99_ms': percentile(full, .99),
            'frame_max_ms': max(full),
            'render_p95_ms': percentile(render, .95),
            'end_p95_ms': percentile(end, .95),
            'cpu_pre_end_p95_ms': percentile([max(0., a - b)
                                           for a, b in zip(render, end)], .95),
            'record_p95_ms': percentile(vals('render_record_ms'), .95),
            'prepare_p95_ms': percentile(vals('render_prepare_ms'), .95),
            'submit_p95_ms': percentile(vals('render_submit_ms'), .95),
            'worker_wait_p95_ms': percentile(vals('render_worker_wait_ms'), .95),
            'first10_frame_p95_ms': percentile(first, .95),
            'remaining_frame_p95_ms': percentile(remaining, .95) if remaining else '',
            'p95_tail_in_first10': sum(slow[:10]),
            'max_consecutive_p95_frames': longest,
            'parallel_requested_frames': sum(int(float(row['render_path_reason'])) == 3
                                             for row in ordered),
            'parallel_executed_frames': sum(int(float(row['render_parallel_executed']))
                                            for row in ordered),
            'parallel_fallback_frames': sum(int(float(row['render_parallel_fallback']))
                                            for row in ordered),
            'sdl_exact_available': int(exact),
            'sdl_present_p50_ms': '', 'sdl_present_p95_ms': '',
            'sdl_present_p99_ms': '', 'sdl_present_max_ms': '',
            'end_cleanup_p95_ms': '',
        }
        if exact:
            sdl = vals(EXACT_PRESENT)
            data.update(sdl_present_p50_ms=percentile(sdl, .5),
                        sdl_present_p95_ms=percentile(sdl, .95),
                        sdl_present_p99_ms=percentile(sdl, .99),
                        sdl_present_max_ms=max(sdl),
                        end_cleanup_p95_ms=percentile([max(0., e - p)
                                                       for e, p in zip(end, sdl)], .95))
        result.append(data)
    return result


def compare_phase_passes(data: list[dict[str, object]]
                         ) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    """Independent pairs: fixed vs adaptive per visit; visit 1 vs 2 per mode.

    Pair only full, same-length phase visits. These are independent processes,
    not synchronized frames; percentile deltas are descriptive, not causal.
    """
    matched: dict[tuple[str, int, int, int, str], dict[str, object]] = {}
    for record in data:
        key = (str(record['workload']), int(record['repetition']),
               int(record['phase']), int(record['pass_index']), str(record['mode']))
        if key in matched:
            raise ValueError(f'duplicate phase visit {key}')
        matched[key] = record
    modes: list[dict[str, object]] = []
    visits: list[dict[str, object]] = []

    def percent_gain(first: object, second: object) -> object:
        if first == '' or second == '' or float(first) == 0:
            return ''
        return 100 * (float(first) - float(second)) / float(first)

    for (workload, rep, phase, visit, mode), a in sorted(matched.items()):
        if not a['complete_phase']:
            continue
        if mode == 'fixed':
            other = matched.get((workload, rep, phase, visit, 'adaptive'))
            if other and other['complete_phase'] and a['frame_count'] == other['frame_count']:
                common = {'workload': workload, 'repetition': rep, 'phase': phase,
                          'phase_load_pct': a['phase_load_pct'], 'pass_index': visit}
                modes.append({**common, 'fixed_frame_p95_ms': a['frame_p95_ms'],
                              'adaptive_frame_p95_ms': other['frame_p95_ms'],
                              'adaptive_frame_p95_gain_pct': percent_gain(a['frame_p95_ms'], other['frame_p95_ms']),
                              'fixed_frame_p99_ms': a['frame_p99_ms'],
                              'adaptive_frame_p99_ms': other['frame_p99_ms'],
                              'adaptive_frame_p99_gain_pct': percent_gain(a['frame_p99_ms'], other['frame_p99_ms']),
                              'fixed_sdl_p95_ms': a['sdl_present_p95_ms'],
                              'adaptive_sdl_p95_ms': other['sdl_present_p95_ms'],
                              'adaptive_sdl_p95_gain_pct': percent_gain(a['sdl_present_p95_ms'], other['sdl_present_p95_ms']),
                              'fixed_sdl_p99_ms': a['sdl_present_p99_ms'],
                              'adaptive_sdl_p99_ms': other['sdl_present_p99_ms'],
                              'adaptive_sdl_p99_gain_pct': percent_gain(a['sdl_present_p99_ms'], other['sdl_present_p99_ms']),
                              'fixed_parallel_executed': a['parallel_executed_frames'],
                              'adaptive_parallel_executed': other['parallel_executed_frames']})
        if visit == 1:
            later = matched.get((workload, rep, phase, 2, mode))
            if later and later['complete_phase'] and a['frame_count'] == later['frame_count']:
                visits.append({'workload': workload, 'repetition': rep,
                               'mode': mode, 'phase': phase, 'phase_load_pct': a['phase_load_pct'],
                               'first_frame_p95_ms': a['frame_p95_ms'],
                               'second_frame_p95_ms': later['frame_p95_ms'],
                               'second_frame_p95_gain_pct': percent_gain(a['frame_p95_ms'], later['frame_p95_ms']),
                               'first_frame_p99_ms': a['frame_p99_ms'],
                               'second_frame_p99_ms': later['frame_p99_ms'],
                               'second_frame_p99_gain_pct': percent_gain(a['frame_p99_ms'], later['frame_p99_ms']),
                               'first_sdl_p95_ms': a['sdl_present_p95_ms'],
                               'second_sdl_p95_ms': later['sdl_present_p95_ms'],
                               'second_sdl_p95_gain_pct': percent_gain(a['sdl_present_p95_ms'], later['sdl_present_p95_ms']),
                               'first_sdl_p99_ms': a['sdl_present_p99_ms'],
                               'second_sdl_p99_ms': later['sdl_present_p99_ms'],
                               'second_sdl_p99_gain_pct': percent_gain(a['sdl_present_p99_ms'], later['sdl_present_p99_ms']),
                               'first_p95_tail_in_first10': a['p95_tail_in_first10'],
                               'second_p95_tail_in_first10': later['p95_tail_in_first10']})
    return modes, visits


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


def analyze_existing_phase_passes(source: Path) -> tuple[list[dict[str, object]],
                                                        list[dict[str, object]],
                                                        list[dict[str, object]]]:
    """Offline revisit of old/new archives; keep analyze_existing_all's API stable."""
    archive = zipfile.ZipFile(source) if source.is_file() else None
    try:
        names = archive.namelist() if archive else [str(p.relative_to(source)).replace('\\', '/')
                                                    for p in source.rglob('*') if p.is_file()]
        metadata_paths = [name for name in names if name.endswith('metadata.json') and
                          (name.count('/') == 0 or name.split('/')[-2].startswith('render_selector_'))]

        def read(name: str) -> str:
            return (archive.read(name).decode('utf-8-sig') if archive else
                    (source / name).read_text(encoding='utf-8-sig'))

        passes: list[dict[str, object]] = []
        for name in sorted(metadata_paths):
            root = name.removesuffix('metadata.json')
            meta = json.loads(read(name))
            if meta.get('selector_modes') != ['fixed', 'adaptive']:
                continue
            warmup, count = int(meta['warmup']), int(meta['frames'])
            for workload in meta['workloads']:
                for rep in range(1, int(meta['repetitions']) + 1):
                    for mode in ('fixed', 'adaptive'):
                        profile = f'{root}{workload}/{mode}_run{rep:02d}/render_parallel_run01.csv'
                        data = list(csv.DictReader(io.StringIO(read(profile))))
                        if len(data) < warmup + count:
                            raise ValueError(f'{profile}: missing measured frames')
                        passes.extend(analyze_phase_passes(
                            data[warmup:warmup + count], build=str(meta['build_config']),
                            workload=workload, repetition=rep, mode=mode,
                            phase_frames=int(meta['phase_frames']), warmup=warmup,
                            phase_loads=tuple(meta.get('render_cycle_percentages', [55,75,100,65]))))
        if not passes:
            raise ValueError('no phase visits found in archive')
        modes, visits = compare_phase_passes(passes)
        return passes, modes, visits
    finally:
        if archive:
            archive.close()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path,
                        help='render_selector_* directory or ZIP containing selector sweeps')
    parser.add_argument('--output', required=True, type=Path,
                        help='directory for tail_frames.csv and tail_summary.csv')
    args = parser.parse_args(argv)
    try:
        frames, summaries, phase_frames, phase_summaries, pairs = analyze_existing_all(args.input)
        passes, paired_modes, paired_visits = analyze_existing_phase_passes(args.input)
        args.output.mkdir(parents=True, exist_ok=True)
        write_csv(args.output / 'tail_frames.csv', frames)
        write_csv(args.output / 'tail_summary.csv', summaries)
        write_csv(args.output / 'phase_tail_frames.csv', phase_frames)
        write_csv(args.output / 'phase_tail_summary.csv', phase_summaries)
        write_csv(args.output / 'phase_pass_results.csv', passes)
        write_csv(args.output / 'phase_pass_comparisons.csv', paired_modes)
        write_csv(args.output / 'phase_pass_transitions.csv', paired_visits)
    except (OSError, KeyError, ValueError, json.JSONDecodeError, zipfile.BadZipFile) as exc:
        print(f'[FAIL] Tail analysis: {exc}', file=sys.stderr)
        return 1
    print(f'[SELECTOR-TAILS] equivalent_pairs={pairs} p95_frames={len(frames)} '
          f'summaries={len(summaries)} phase_p95_frames={len(phase_frames)} '
          f'phase_summaries={len(phase_summaries)} '
          f'phase_passes={len(passes)} pass_pairs={len(paired_modes)} '
          f'pass_transitions={len(paired_visits)} errors=0 output={args.output}', flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

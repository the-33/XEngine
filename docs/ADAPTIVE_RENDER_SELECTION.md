# Adaptive render-route selector (experimental, XEngine v16.9)

## What changes

There is still **one global threshold**: `Renderer.parallelMinCommands`.
The default reference changes from 384 to **1536** commands following the
v16.8 calibrated fixed-path experiments. This is an **experimental, conservative
reference**, not a validated universal performance optimum. Existing game
configs with an explicitly stored 384 retain their value after synchronization.

`Renderer.parallelSelection` chooses the strategy, **not another threshold**:

- `"fixed"` (default): select the recorded path when the previous frame's
  eligible command count is >= `parallelMinCommands` (legacy behavior).
- `"adaptive"` (opt-in): compare the previous frame's **weighted CPU-work
  estimate** with the same reference, then apply a 15% exit hysteresis. The
  decision is taken on the main thread using cheap integer counters. No
  SDL API is called by worker threads.

```json
"Renderer": {
  "parallelPreparation": true,
  "parallelMinCommands": 1536,
  "parallelSelection": "fixed"
}
```

To try the provisional adaptive policy, change only `parallelSelection` to
`"adaptive"`. The engine also needs `Engine.multiThreading=true` and an
available worker pool; otherwise it uses the immediate path.

## Cost model (NOT empirically calibrated)

A plain eligible world sprite/rectangle contributes 256 units. An X/Y-tilted
sprite adds 192; nonzero Z rotation adds 64; nonzero local offset adds 64.
A rotated world rectangle adds 64. A Z-rotated sprite with a local offset
therefore receives 384 units; a fully tilted/rotated/offset sprite receives
576. This approximates **deferrable CPU geometry preparation**, not GPU
execution time, tint cost or pixel overdraw. UI/debug/background commands do
not contribute. The model only uses values already available during submission,
without new trigonometric operations or mutable-state reads.

At 1536 plain commands, the entry boundary is 393216 units. Once in the
parallel path, it remains there until weighted work drops below 85% of the
entry boundary. The selector uses the **previous** frame, so transitions are
expected one frame after a scene load change; the first frame is immediate.
Hysteresis is not a guarantee of zero mode changes for wildly oscillating loads.

The selected path and weighted counters appear in the F3 overlay and CSV:

- `render_path_reason`: 0 disabled, 1 no workers, 2 below threshold, 3 recorded.
- `render_selection_mode`: 0 fixed, 1 adaptive.
- `render_weighted_units_previous`, `render_weighted_units_current`: raw integer
  work units, not equivalent to eligible command counts.
- `render_path_switched`: 1 if the selected path changed since the previous frame.
- `render_workers`: confirms **actual** worker participation.

The adaptive dispatch bypasses the separate *raw command count* gate in
`PrepareSnapshot`. Without this, the selector could choose parallel for
complex 1024-sprite scenes only for the workers to be suppressed by a second
1536-command test. The fixed selector preserves the existing dispatch gate.

## Controlled A/B tests with changing scene loads

The benchmark includes `--render-cycle 1`, which cycles **55%, 75%, 100% and
65%** of non-physics sprites as active over phases of `--phase-frames N`.
This tests real route transitions and load drops within one engine process,
instead of only comparing independent static process runs.

Quick smoke test:

```powershell
python tools/sweep_render_selector.py --build --workloads baseline rotations_xyz --sprites 2048 --threshold 1536 --phase-frames 30 --frames 240 --warmup 30 --repetitions 1
```

Extended calibration:

```powershell
python tools/sweep_render_selector.py --build --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 5
```

Both run fixed-vs-adaptive comparisons with **the same collision workers** and
use deterministic 1-fixed-step frames with VSync off. The tool alternates run
order, preserves per-frame CSVs, fails on unequal physics/contact/draw counts,
and exports `paired_comparisons.csv`, `phase_results.csv` and `metadata.json`.
The `--build` step streams engine **and** game compiler output live while
writing `build.log`. Failure aborts immediately with a nonzero status: an
old executable must never be used as a fallback. Source-equivalence checks are
not pixel-perfect screenshots.

**Before making the adaptive mode the production default**, gather replicated
Windows Debug and Release results: per-phase p50/p95/p99, number of transitions,
actual worker activity, CPU overhead and confidence under noise. If hysteresis
causes regressions or unstable p95, retain fixed or revise the heuristic based
on evidence; do not choose profile-specific hardcoded thresholds.

## Configuration synchronization

The canonical runtime defaults live in
`tools/templates/BlankGame/assets/config/engine.json` and managed keys in
`tools/engine_config_schema_state.json`. For an existing workspace:

```powershell
python tools/sync_engine_configs.py
python tools/sync_engine_configs.py --check
```

The synchronizer adds the new selection field while retaining valid custom
thresholds and game-specific values and making backups. It does not silently
replace a stored 384 with 1536; adjust that value explicitly if testing the
new reference. The benchmark's CLI overrides the two settings only for its
process and does not rewrite game JSON files.

## v16.10 — Conservative viewport-aware scoring and execution telemetry

**Experimental; not yet performance validated on Windows or Linux SDL.** The
v16.9.1 repeated Windows cycling benchmark produced 20/20 logically equivalent
fixed/adaptive pairs, but adaptive p95 improved in only 7/20 and culling showed
unacceptable regressions. This update is an investigation, **not** a claim that
those regressions have been fixed.

In `adaptive`, each submitted sprite is checked against a conservative
world-space view bound built from its unprojected dimensions, absolute scale,
local offset and pivot. The bound includes **all** X/Y tilt and Z rotations;
its only purpose is to identify sprites *definitely* outside the camera without
computing trigonometric functions in the main thread. Potentially visible
sprites retain their original weight. Definitely offscreen sprites receive
**25% of that weight** (heuristic; their projection and culling still require
CPU but they cannot generate on-screen texture submissions). Non-sprite world
commands retain full weight. Camera invalidity and non-finite values fail open
as potentially visible. **Actual renderer culling and submitted draws are
unchanged.** The view check is evaluated only when adaptive selection is active
and affects the **next** frame's decision, consistent with prior-frame selection.

The older `render_weighted_units_previous/current` remain raw transformation-
weighted work totals; the new `render_effective_units_previous/current` are
the viewport-adjusted estimates that `adaptive` actually uses. `fixed` continues
to use eligible command counts only, with no extra viewport-scoring overhead.

Profiler fields distinguish route choice from worker dispatch:

| Field | Meaning |
| --- | --- |
| `render_path_reason == 3` | Requested/selected recorded render route, NOT worker execution |
| `render_parallel_executed` | `1` when the renderer dispatched worker preparation |
| `render_parallel_fallback` | `1` if recording was selected but preparation ran sequentially |
| `render_path_switched` | Transition of selected recorded versus immediate route |
| `render_executed_switched` | Transition of actual worker dispatch (may differ at sudden load drops) |
| `render_estimated_offscreen_sprites` | Conservatively rejected sprite count for adaptive scoring only |
| `render_effective_units_previous/current` | View-adjusted selector work units |

The benchmark runner now counts `render_parallel_frames` from **actual worker
execution**, while preserving `render_parallel_requested_frames` separately.
`phase_results.csv` includes per-phase p50/p95/p99, selected/actual worker
counts, fallbacks, execution transitions, effective units and offscreen counts.
`paired_comparisons.csv` includes full-run p50/p95/p99, both transition metrics
and requested/actual worker counts. This allows examination of `culling`
regressions without assigning every process-level latency spike to the selector.

### Debug and Release replication

Start with a short Windows Debug smoke test, inspecting `failures.txt`, frame
counter parity and `render_parallel_fallback` on fixed load drops:

```powershell
python tools/sweep_render_selector.py --build --build-config Debug --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --phase-frames 30 --frames 240 --warmup 30 --repetitions 1
```

Then compare repeated Debug and **packaged Release** runs independently:

```powershell
python tools/sweep_render_selector.py --build --build-config Debug --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 7
python tools/sweep_render_selector.py --build --build-config Release --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 7
```

`--build-config Release` launches `games/XEngine Benchmark/dist/<platform>-x86_64/<version>/XEngine Benchmark[.exe]`
with mode configurations and generated profiles **inside that same packaged
folder**. Source mode JSON hashes are checked against packaged versions before
any measurement. A failed build aborts the sweep before running any executable.
Build output from both engine and game streams to the console and is duplicated
in `build.log`. Debug and Release results MUST NOT be paired directly because
they are separate compiler profiles; compare fixed/adaptive *within each*.
Disable background tasks, use alternating run order (already implemented),
monitor p95/p99 and repeat on independent sessions. Profiling itself adds
runtime overhead in both modes; release profiling is enabled by the benchmark's
runtime `Engine.profileCsv` field rather than an assumed Debug-only build flag.

No new `engine.json` keys or threshold changes are introduced by v16.10.
Keep `Renderer.parallelMinCommands: 1536` as an experimental reference and
`parallelSelection: "fixed"` as the production default until actual Windows
Debug and Release measurements show a reliable improvement without tail cost.

The subsequent validated 2026-10-21 Windows experiment compared seven
repetitions across four workloads in both Debug and Release: all **56/56**
fixed/adaptive pairs were equivalent (30,240 compared frames). Adaptive had
negative median p50/p95/p99 gains in both configurations, particularly
`culling` p95 in Release. These results support retaining `fixed`, and motivate
the scoring-cost ablation below rather than changing production thresholds.

## Isolate selector scoring cost without changing the render route (post-v16.11)

`tools/sweep_render_selector_cost.py` performs an independent **three-way
ablation**. Each variant starts a fresh benchmark process with identical seed,
physics, sprites, workload, and **forced rendering route**:

| `--render-estimate` | CPU work performed for each world sprite | Units available to selector |
| --- | --- | --- |
| `off` | No estimated unit calculation or visibility estimation | 0 (benchmark-only) |
| `weighted` | Basic sprite transform/rectangle weights | Raw weighted units |
| `visibility` | Basic weights **plus** conservative viewport rejection | Visibility-adjusted units |

These options are **benchmark-only overrides**, not new `engine.json` keys.
The `--render-route recorded` option always requests recording and worker
preparation (dispatcher threshold 1); `immediate` always uses the immediate SDL
rendering path. The selected policy is otherwise `fixed` for all three variants,
with benchmark-only `render_path_reason=4` identifying a forced immediate path,
and the benchmark rejects a run if actual worker participation, render commands,
physics signatures or submitted draws differ across variants.

Additional frame-profile columns:

- `render_selector_decision_ms`: directly measures the once-per-frame path
  selection function; **does not include per-sprite scoring**.
- `render_work_estimate_mode`: 0 = off; 1 = weights; 2 = visibility.
- `render_route_override`: 0 = automatic; 1 = forced immediate; 2 = forced recorded.
- Existing `render_world_ms` includes sprite scoring and scene work. A/B
  differences in this metric estimate scoring cost; `frame_ms` determines
  whether it matters in the complete game. `render_prepare_ms`,
  `render_submit_ms`, `render_parallel_executed` and `render_workers` reveal
  whether unexpected path or submission differences caused tail regressions.

Windows smoke test, for a single recorded-route `culling` workload:

```powershell
python tools/sweep_render_selector_cost.py --build --build-config Debug --workloads culling --routes recorded --sprites 2048 --threshold 1536 --phase-frames 30 --frames 240 --warmup 30 --repetitions 1
```

Then replicate on independently rebuilt Debug and Release binaries:

```powershell
python tools/sweep_render_selector_cost.py --build --build-config Debug --workloads baseline culling mixed rotations_xyz --routes recorded immediate --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 7
python tools/sweep_render_selector_cost.py --build --build-config Release --workloads baseline culling mixed rotations_xyz --routes recorded immediate --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 7
```

The tool alternates the three run orders and exports `paired_cost.csv` (within
triplet p50/p95/p99), `phase_cost.csv` (55/75/100/65% load), `p99_outliers.csv`
(frame indices and stage breakdowns), `summary.csv` (medians across independent
repetitions), `metadata.json`, and `build.log` when `--build` is used. Positive
`weights_minus_off` or `visibility_minus_weights` values indicate a slower
scored variant. These are **between-process timing differences**, not exact
per-sprite durations; the experiment intentionally avoids invasive per-sprite
clock instrumentation. Quantiles of separate runs are not matched latency
samples, even though the deterministic workload and game counters are aligned.
Repeat a session if p95/p99 disagree with another session, and inspect phase
and outlier exports before attributing tails to visibility calculations.

Like the other benchmark sweeps, this command always prints a final result
summary after a completed session or a failed build, benchmark subprocess,
telemetry/parity check or CSV export. A successful one-triplet smoke test ends
with the following shape (the actual output path will vary):

```text
[SELECTOR-COST] triplets=1/1 equivalent_frames=270 runs_validated=3/3 errors=0 output=...
```

`triplets` counts **fully equivalent** `off`/`weighted`/`visibility` groups,
not individual benchmark runs. `runs_validated` counts profiles that passed
their telemetry checks; each triplet requires three runs. `equivalent_frames`
counts frame positions compared per triplet, including warmup frames.
`errors=0` indicates successful validation and export. On failure, the same
summary reports `errors=1`, the script exits nonzero, and `failures.txt`
explains why. A triplet whose profiles were captured but failed equivalence
still counts as **zero** equivalent triplets. Invalid CLI arguments or a
nonempty destination are rejected before the sweep starts.

Finally, rerun `sweep_render_selector.py` with **automatic** paths and both
strategies. A reduction in scoring cost is not by itself evidence that the
adaptive selector is faster: real activation thresholds, fallbacks, transitions
and p95/p99 must still be assessed in that normal A/B comparison. Keep the
production `fixed`/1536 setting unchanged until such results exist.

## Visibility fast path and saturated adaptive scoring (post-v16.12)

The Windows Debug/Release scoring-cost experiment found extra median
`render_world_ms` p50 of **+0.05480 ms Debug / +0.01137 ms Release** for
visibility versus weighted scoring across 56 matched triplets per build.
This is measured *before* the optimization below; do not reuse it as an
estimate of the new implementation's speed.

The automatic `adaptive` path now avoids two classes of redundant work:

1. **One camera calculation per frame.** Validate the viewport and cache its
   four world-space boundaries in `Begin`/`BeginRecord`. A sprite whose anchor
   lies inside both viewport axes cannot be rejected by the conservative
   enclosing-extent test, so skip its remaining geometry operations. Sprites
   outside the view still use the same finite-value validation and conservative
   scale, offset and pivot bounds. The actual render/cull pipeline is untouched.
2. **Stop scoring after a provably sufficient result.** Accumulate effective
   units up to the *entry* threshold (`parallelMinCommands * 256`). Once that
   total is reached, subsequent sprite visibility checks cannot change the
   next-frame decision, whether the previous path was parallel or immediate:
   the exit threshold is lower (85% of entry). Keep accumulating the **raw**
   weighted units but skip further visibility checks. Frames below entry still
   evaluate all their sprites. The logic is independent of the workload type
   and does not speculate about future sprite counts.

This second shortcut runs **only** when `parallelSelection="adaptive"` and
the scoring probe is in its normal `auto` mode. An explicit benchmark
`--render-estimate visibility` still performs the **full uncapped** visibility
evaluation even with a forced route, so the existing three-way ablation
remains a meaningful full-cost reference. `fixed` and default config values
are unchanged.

### Profiling and interpretation

- `render_visibility_skipped_sprites`: new counter for sprite visibility
  checks omitted **after the entry threshold was already reached**. It is
  always zero in forced `off/weighted/visibility` cost probes.
- `render_estimated_offscreen_sprites`: number actually classified as
  definitely offscreen, **among evaluated sprites**. With the adaptive
  shortcut active, it is a lower bound on all offscreen sprites, not a full
  census; compare the new skipped counter before interpreting it.
- `render_effective_units_previous/current`: in automatic adaptive mode,
  **saturates at the entry threshold**. It is sufficient for route selection
  but is *not* an exact total after saturation. The raw
  `render_weighted_units_*` fields remain unbounded weights as before.

`tools/sweep_render_selector.py` now includes
`visibility_checks_skipped_median` in the per-phase CSV and
`adaptive_visibility_checks_skipped` in paired results. The selector-cost
sweep requires the new CSV field and rejects skipped checks in explicit
scoring probes; this also catches accidental use of a stale binary.

### Validation procedure

Run the CPU-only unit tests first:

```powershell
python -m unittest tests.test_render_visibility_fastpath_v1613 tests.test_render_selector_visibility_v1610 tests.test_render_selector_cost_v1612
```

The new test compares the optimized conservative bound against the exact
previous formula over 100,000 deterministic inputs, plus invalid inputs,
and verifies 7 thresholds with both hysteresis states and synthetic streams
of mixed sprite weights. This validates the comparison mathematics; it does
**not** prove overall SDL frame-time improvement or pixel equivalence.

Build and run independently in **Debug and Release**, keeping the engine and
game's complete build output visible:

```powershell
python tools/sweep_render_selector_cost.py --build --build-config Debug --workloads baseline culling mixed rotations_xyz --routes recorded immediate --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 7
python tools/sweep_render_selector_cost.py --build --build-config Release --workloads baseline culling mixed rotations_xyz --routes recorded immediate --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 7
python tools/sweep_render_selector.py --build --build-config Debug --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 7
python tools/sweep_render_selector.py --build --build-config Release --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 7
```

Inspect the skipped-check counts by phase; compare `render_world_ms` p50,
full-frame p50/p95/p99, actual worker frames, route switches, fallbacks and
physics/render parity. The forced-route ablation isolates the cheaper
per-sprite viewport predicate, **not** the full benefit of threshold
saturation (which is exercised only by automatic adaptive selection).
Keep `fixed` as production default until repeated automatic A/B results
show consistent improvement without tail-latency or equivalence regressions.

## p95/p99 stage forensics and centered-sprite fast path (post-v16.13, 2026-10-22)

### What the next Windows calibration actually showed

The user-provided `results(2).zip` contains two independent Debug/Release
sessions (56 equivalent fixed/adaptive pairs, seven repetitions of four
workloads in each configuration). Reprocessing all **480 measured frames**
per run produced 2,689 frame observations at or above each run's p95 and
224 per-run p95/p99 summaries. The frame-tail reports include the *actual*
render route and distinguish top-level timing stages from nested render timers.

Specific tails requiring caution:

- Debug, `baseline`, repetition 2: fixed p95/p99 = **2.205/2.447 ms**;
  adaptive = **2.583/4.240 ms**. Adaptive frame 369 spent **3.990 ms**
  inside `SDL_RenderPresent`; frame 370 spent **3.307 ms** in input, while
  frames 527–529 were on the **immediate** route (no render workers) and
  `render_world_ms` reached **4.701 ms** on frame 529. No single selector
  decision timer explains these different peaks.
- Release, `baseline`, repetition 1: fixed p95/p99 = **1.431/1.610 ms**;
  adaptive = **1.677/1.962 ms**. At frame 127 adaptive spent **1.567 ms**
  inside `SDL_RenderPresent`; the entire frame was **2.406 ms**. At frames
  129 and 369, input itself was **0.267/0.206 ms**. These are scheduling/
  submission/presentation candidates to profile further, not proof that the
  visibility predicate made these frames slow.
- Across all seven `baseline` repetitions in each build, the render stage
  was the largest measured top-level contributor to virtually every p95+
  frame (Debug adaptive: 167 of 168, with one input-dominated spike).
  **This does not make the SDL-present call the cause of every regression**:
  `render_present_ms` includes the *whole* render stage, while
  `render_only_present_ms` is the nested SDL-present call.

The experiments were separate sequential processes, not synchronized
per-frame A/B. Differencing independent p95 or p99 quantiles cannot establish
causality. `render_world_ms`, `render_record_ms`, `render_prepare_ms`,
`render_submit_ms` and `render_only_present_ms` are nested within
`render_present_ms` and **must never be added to it**. `frame_ms` also includes
unmeasured overhead; `unaccounted_ms` is a descriptive difference, not a new
independent profiler timer.

### Why `culling` skipped so few visibility checks

The observed adaptive run skipped **4,560 / 724,920** eligible sprite checks
(**0.63%**) over 480 frames, while classifying approximately **31.3%** of
examined sprites as offscreen. This is consistent with correct saturation:

- At 2,048 active `culling` sprites, ~644 offscreen × 25% weight leaves
  approximately `2048 - 644 * 0.75 = 1565` equivalent commands, **just 29**
  above the 1,536 entry threshold. Saturation starts near the end of the
  sprite stream, yielding ~38 skipped checks per full-load frame.
- At 1,536 active sprites, ~484 offscreen leaves `1536 - 484 * 0.75 = 1173`
  equivalent commands, so the threshold is never reached. There is no
  permission to omit checks while a future sprite could change selection.
- `baseline` at 2,048 fully visible sprites instead reaches 1,536 units
  early and skips the final 512 visibility checks in its full-load phase.

The low `culling` skipped count is thus **not** by itself a defect. Increasing
it without a mathematical guarantee would change fixed/adaptive route
decisions and invalidate the comparison. The new `RenderDiagnostics.h`
shortcut instead recognizes the common **pivot `(0.5,0.5)` and zero local
offset** geometry and computes the **same conservative extent** using fewer
operations. Custom pivots, offsets, invalid values and tilted/rotated sprite
bounds still use the prior behavior. It does not change actual rendering,
worker dispatch, offscreen classification, score units, thresholds or config.

### Reproducible tail exports

Every successful `sweep_render_selector.py` run additionally writes:

- `tail_frames.csv`: every measured frame at or above that **run's own p95**,
  whether it is also above p99, frame index/phase, top-level durations,
  nested render timers, raw unaccounted time, selected/actual route telemetry
  and which nonoverlapping top-level stage took longest.
- `tail_summary.csv`: one p95 and one p99 row **per mode/repetition/workload**;
  threshold, number of frames, max, phase distribution, median substage time
  among selected tail frames, aggregate top-level shares and count of
  frames with real worker execution. These are *descriptive*, not a causal
  decomposition or independent percentile-by-stage subtraction.

An existing ZIP or run folder can be analyzed **without rerunning SDL**:

```powershell
python tools/render_selector_tails.py --input "C:\path\to\results.zip" --output "C:\path\to\tail_diagnostics"
```

For new native Debug and Release A/B measurements, use the four-workload
`sweep_render_selector.py` commands given above; collect the two new CSVs
along with per-phase CSVs and build logs. Re-run the forced route
`sweep_render_selector_cost.py` ablation to see whether the centered-sprite
branch saves visibility CPU time. This optimization has been verified only
by SDL-free C++ equivalence tests on Linux so far: do **not** claim a Windows
p50/p95/p99 gain before new matched sessions. Preserve production `fixed`
and the experimental `1536` threshold.

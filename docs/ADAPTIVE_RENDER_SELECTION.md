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

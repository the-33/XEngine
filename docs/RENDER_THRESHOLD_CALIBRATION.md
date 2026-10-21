# Render Worker Threshold Calibration (v16)

## Goal and scope

Measure the *full* SDL frame-time tradeoff of immediate rendering against the
recorded/parallel preparation route, using the actual `XEngine Benchmark` game.
The previous threshold of **384 eligible world commands** was heuristic. No
new production threshold is asserted by this patch. This experiment isolates
render-path differences with **zero colliders** and the same `collision` worker
pool on both sides; it deliberately does not compare `single` to `parallel`,
which would introduce a collision-threading confounder.

## Configuration

`Renderer.parallelMinCommands` is an **optional** positive integer (1–100000)
with default **384**. Both independent selection gates consume it:

1. Previous frame's eligible world command count chooses the recorded path.
2. Current frame's recorded command count determines whether to dispatch CPU
   preparation to workers.

Setting `parallelPreparation=false` retains the immediate path irrespective of
this numeric value. SDL drawing and presentation always remain on the main
thread. Existing JSON files that omit the key continue to use **384**.

### Keeping game configurations synchronized (v16.2)

The canonical default is now explicitly present in
`tools/templates/BlankGame/assets/config/engine.json` as
`"parallelMinCommands": 384`, and the field is tracked by
`tools/engine_config_schema_state.json`. On an existing checkout, run:

```powershell
python tools/sync_engine_configs.py --check
python tools/sync_engine_configs.py
python tools/sync_engine_configs.py --check
```

The first `--check` returns exit status 1 if any game or alternate startup
profile is missing the field; after migration the last check returns 0.
The migrator adds this default to missing fields while preserving valid existing
numeric overrides (e.g. 640), unrelated game settings, and individual profile
flags. It creates timestamped backups in `.xengine-config-backups/` before
writing. `--game "XEngine Benchmark"` optionally scopes migration to that game.
Do **not** overwrite individual games' complete JSON using the template. There
is **no change to the runtime default or worker selection algorithm**; this
merely exposes an already implemented setting to template-based synchronization.

The benchmark game's CLI can override its effective runtime config with
`--render-threshold N` and its workload with `--sprites N` (0–10000). Its
`[BENCH-RENDER] threshold=N sprites=M` startup confirmation is required by the
updated benchmark runner. The runner saves both values in its metadata and
`summary.csv`. The `--build` option remains incremental.

## Run the experiment

From the XEngine root, first run a small smoke/calibration pass:

```powershell
python tools/sweep_render_threshold.py --build --counts 128 256 384 512 --repetitions 1 --frames 120 --warmup 30
```

Then run the full paired experiment, preferably with low background CPU load:

```powershell
python tools/sweep_render_threshold.py --build --counts 128 192 256 320 383 384 448 512 640 768 1024 1536 2048 --repetitions 3 --frames 360 --warmup 90
```

**Interpretation of `--render-threshold 1`:** it deliberately forces the
recorded worker route at low loads *to measure its cost*, even though the
production selection would remain immediate below 384. Do **not** set a game to
1 on the assumption that it is faster. To test the current real policy
separately, run the sweep with `--render-threshold 384`; loads below 384 will
be rejected as parallel comparisons because they never exercise render
workers. The low-load results from that run should be interpreted as policy
fallback checks, not performance measurements of parallel preparation.

The sweep executes `collision` (immediate render with collision workers
available) and `parallel` (recorded render with CPU preparation workers) under
the **same** seeded, deterministic 1-fixed-step-per-frame workload. The mode
order rotates by repetition to limit simple run-order bias. Each sprite count
is tested using a separate runner session with raw profiles and logs retained.

## Outputs

The folder `games/XEngine Benchmark/results/render_sweep_<timestamp>/` holds:

- `metadata.json`, `build.log` when `--build` is used, and one subdirectory per
  requested sprite count, containing both mode profiles, subprocess logs and
  runner metadata.
- `runs.csv`: per-execution performance/diagnostic counters.
- `paired_comparisons.csv`: per-repetition paired p50/p95, render-stage costs,
  actual eligible commands, worker participation and percent gain.
- `summary.csv`: median of paired per-run results, including number of wins.
- `equivalence.csv`: frame-by-frame checks of contact signature, physics steps,
  narrowphase, SDL world-draw count and attempted world commands.
- `crossovers.csv`: **exploratory** linear interpolations across sign changes,
  not proven thresholds; absent when no sign changes occur.
- `threshold_candidate.csv`: a conservative candidate only if two **adjacent**
  tested loads both have at least 3 repeats, >=2/3 paired p50 wins, >=2% median
  p50 improvement, and no >5% median p95 regression. The candidate is based on
  **observed eligible commands**, not the nominal number of game objects.
  Otherwise it explicitly says `not_confirmed`.
- `failures.txt` when validations fail; failures return a nonzero exit status.

The checks are **not pixel-by-pixel image equality tests**. Independent
geometry/culling regression tests remain part of the test suite. In particular,
correctness of SDL command ordering, transparency and visual equivalence also
requires running the game on Windows/Ubuntu and visually comparing the paths.

## Scientific cautions

An observed crosspoint depends on CPU, SDL backend, driver, image visibility,
workload, render resolution, thermal state, profiling overhead and compiler
settings. The benchmark uses **Debug** builds so that the current runner's
runtime JSON and profilers can be controlled, and disables VSync. A stable
Debug threshold is not automatically a valid Release threshold; confirm any
candidate with matched Release or carefully controlled target-game tests
before changing production defaults. Prefer examining p50, p95, run-to-run
variance, `render_worker_wait_ms`, `render_record_ms`, `render_prepare_ms`,
`render_submit_ms`, `render_world_draws` and `render_eligible_current` together.

Run the focused tests:

```powershell
python -m unittest discover -s tests -p test_render_threshold_sweep.py -v
python -m unittest discover -s tests -p test_render_diagnostics.py -v
```

An SDL2-equipped Windows/Ubuntu build and benchmark run are **not yet
validated** for v16. The previous v15 system and default behavior are
preserved, but the new optional configuration input needs target-side smoke
testing. Send the entire `render_sweep_...` ZIP for analysis; do not substitute
a report based on frame time alone.

## Extended render workloads (v16.1)

The original v16 sweep uses only the `baseline` workload. This is useful for
isolating the cost of recorded/parallel preparation as a function of eligible
sprite count, but it does **not** stress all rendering features. v16.1 adds
separately repeatable render workloads without changing the engine's default
384-command threshold:

| `--workloads` value | Tested behavior |
|---|---|
| `baseline` | Original deterministic four-texture scene; reproduces v16 measurement design. |
| `transforms` | Static and animated sprite rotations, nonuniform scale, pivot/offset changes, source cropping and flips. |
| `tint` | Varying RGB and alpha per object and per frame; texture modulation and tint-cache churn. |
| `culling` | Fully offscreen and partially visible sprites with deterministic small position shifts. |
| `mixed` | Combines all the above with world lines/filled and outlined rotated rectangles, camera pan/zoom, repeated tinted background, UI rectangles/images and cached text. |

The documented ordinary `RenderManager` drawing methods are publicly
accessible to gameplay/benchmark code in v16.1; their immediate internal
counterparts remain private. **Do not invoke these draw calls from workers.**

A baseline run is unchanged **by default**. Workload controls affect only the
benchmark game, not `RenderManager` or ordinary game configuration.

The sweep now groups results and threshold candidates **by workload**. Sprite
count is a requested input, whereas `render_eligible_current` counts the
actual eligible world commands; `mixed` adds some world primitives. Its screen
UI and background submissions are timed but are not parallelized in the same
way as world sprite geometry. Some workloads can therefore remain unsuitable
for parallel render preparation even at large command counts.

### Recommended validation commands

Smoke test (30 runs: 5 workloads, 3 loads, 2 render paths, one repetition):

```powershell
python tools/sweep_render_threshold.py --build --workloads baseline transforms tint culling mixed --counts 256 384 768 --repetitions 1 --frames 120 --warmup 30
```

Replicated experiment (120 runs: 5 workloads, 4 loads, 2 paths, 3 repetitions):

```powershell
python tools/sweep_render_threshold.py --build --workloads baseline transforms tint culling mixed --counts 256 384 640 1024 --repetitions 3 --frames 360 --warmup 90
```

Inspect each workload's `summary.csv` rows and the per-workload
`threshold_candidate.csv` entries; do **not** turn a mixed-workload crossover
into one universal command threshold. The profiler now includes the tint update,
UI/background submission, and culling counters used for diagnostics.

### Coverage limitations

The additional tests preserve frame-by-frame equivalence of physics state,
world command counts, UI/background submissions and texture tint-update counts.
These are **counter/semantic checks, not pixel-perfect visual validation**.
A rendered-frame comparison with controlled SDL backend, GPU/driver and capture
format is a separate task. Font rasterization may vary by SDL_ttf version;
background modes beyond the one used here and window-resize/shader/back-end
specific behavior are not exhaustive. No benchmark has yet proven that
parallel rendering is faster on the user's machine under these workloads.


## v16.3: screen-space UI profiling regression in the mixed workload

The first USER-VALIDATED Windows smoke sweep of the five v16.2 workloads ran
30 render-mode profiles successfully. Twelve of 15 paired comparisons passed.
Every `mixed` pair failed `render_world_draws` equality by exactly four draws
(191 vs 187 at 256 sprites, 286 vs 282 at 384, 568 vs 564 at 768).
`BenchmarkRenderExercise` emits one screen rectangle, two screen images, and
one screen text draw per frame. The immediate path incorrectly classified their
SDL submissions as World because the scene's current draw phase was World.
The recorded/parallel path correctly assigned UI when replaying those commands.

v16.3 temporarily sets `DrawPhase::UI` around the three immediate screen-space
public API dispatches and then restores the caller's phase. Both paths should
now agree on `render_world_draws` **and** `render_ui_draws` without ignoring
mismatches. The fix changes profiling counters, not the SDL draw calls or their
order. Native Windows SDL2 confirmation must still be obtained; passing the
counter checks does not establish pixel-perfect visual equivalence.

Repeat the same reduced run before starting the expanded 120-run calibration:

```powershell
python tools/sweep_render_threshold.py --build --workloads baseline transforms tint culling mixed --counts 256 384 768 --repetitions 1 --frames 120 --warmup 30
```

No engine config changes are involved, so the canonical BlankGame
`engine.json` and `sync_engine_configs.py` remain untouched. The production
threshold remains 384 commands; do not infer an optimized threshold from
only one repetition per workload/count.


## v16.4: XYZ pseudo-3D rotation coverage and replicated render results

The developer's Windows v16.3 extended sweep (`render_sweep_20261021_111122_693971.zip`)
ran five workloads (`baseline transforms tint culling mixed`) at 256, 384,
640 and 1024 sprites, three repetitions each, both immediate (`collision`)
and prepared (`parallel`) rendering: **120 executions and 60/60 paired
frame-by-frame counter/contact equivalences**, covering 27,000 frames
including warmup. All 60 parallel runs engaged render workers for the 360
measured frames. Parallel p50 won only 6/60 paired repetitions, and all 20
per-workload/count median gains were negative. No workload satisfied the
threshold-candidate safeguards. Keep the production default at 384 **pending
further evidence**; it is not an empirically optimal threshold.

`SpriteRenderer` renders rotation X/Y as pseudo-3D cosine compression of
height/width, and Z as in-plane rotation, unlike true perspective projection.
Earlier `transforms` and `mixed` workloads only varied Z. Version v16.4 adds
an independent `rotations_xyz` workload, keeping the previous five workloads
**byte-for-byte equivalent in behavior** to enable comparison with older runs.

`rotations_xyz` covers five index-selected groups: X-only, Y-only, Z-only,
combined X+Y+Z and combined X+Y. The deterministic helper
`games/XEngine Benchmark/assets/scripts/BenchmarkAxisRotations.hpp` also
provides animated angles and configurations approaching 89 degrees in X/Y,
without exactly zero-sized projection at 90 degrees. Like `transforms`, it
exercises nonuniform scale, sprite pivot/offset, flips, and cropped sources.
Only the benchmark game, run tools, and tests change; the `RenderManager`
threading threshold, `engine.json`, `BlankGame` config and sync schema are
unchanged.

After installing the patch on v16.3, test only this *new* profile to avoid
repeating the entire 120-run dataset unnecessarily:

```powershell
# 6 executions; require 3/3 equivalence first.
python tools/sweep_render_threshold.py --build --workloads rotations_xyz --counts 256 384 768 --repetitions 1 --frames 120 --warmup 30

# 36 executions, 18 comparisons; 1536 and 2048 extend the crossover search.
python tools/sweep_render_threshold.py --build --workloads rotations_xyz --counts 256 384 640 1024 1536 2048 --repetitions 3 --frames 360 --warmup 90
```

Counter/signature comparisons do not constitute pixel-level visual validation.
A Windows SDL2 run is needed to confirm there is no route-specific culling
problem for the new near-edge projections. Timing results are compiler,
hardware, and workload specific; preserve per-profile results and compare
measured `render_eligible_current`, not just requested sprite count.

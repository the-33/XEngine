> **Actualización v14:** este documento describe principalmente la implementación
> inicial de v11. El selector de v13 y la clasificación espacial de v14 están
> documentados en [DYNAMIC_BROADPHASE_BENCHMARK.md](DYNAMIC_BROADPHASE_BENCHMARK.md).
> Los parámetros numéricos históricos de esta página no reflejan la configuración
> actual (`cellSize: "auto"` en el template ni la histéresis de v13).

# Adaptive broadphase and calibration — XEngine v11

## Configuration

The `Collision` settings in `tools/templates/BlankGame/assets/config/engine.json` are now:

```json
"Collision": { "broadphase": "auto", "cellSize": 2.0 }
```

The supported values are `bruteforce`, `grid` and `auto`. Existing game JSONs
are **not overwritten** and keep their chosen algorithm. The engine's fallback
when a game omits the entire `Collision` section remains `bruteforce` to retain
backward compatibility. New games created from the template receive `auto`.
Run `python tools/sync_engine_configs.py` after editing the template to add
new settings while preserving each game's existing values. It does not replace
an already existing `bruteforce` value with `auto`.

### How `auto` selects an algorithm

- It evaluates exactly the same collider geometry as the fixed algorithms.
  Contacts and callbacks retain the old deterministic index order.
- Each collision pass is assigned to a workload bucket defined by collider
  count (power-of-two interval) and whether worker threads can be used.
- For each bucket, it alternates six measurements per algorithm. It compares
  **grid build + candidate evaluation** against the original candidate loop,
  which is the relevant incremental cost, using an exponentially weighted mean.
- Once calibrated, a challenger must be at least 8% faster to displace the
  current selection. A losing strategy is remeasured every 48 subsequent
  passes so that changes in collision clustering can influence the decision.
- Sampling and preference are local to the current engine run. Reconfiguring
  the broadphase resets the estimates. The chosen mode can still differ among
  passes in a single frame during initial calibration; this is intentional.
- The adaptive policy **does not promise a universal speedup**. It probes the
  slower option periodically and timings can fluctuate. Use the fixed modes
  for a strict baseline or a fully repeatable instruction path.

The new profiler fields are:

| CSV column | Meaning |
|---|---|
| `collision_broadphase_mode` | Configured mode: `0` brute, `1` grid, `2` auto |
| `collision_grid_passes` | Actual passes using grid during this frame |
| `collision_bruteforce_passes` | Actual passes using brute force during this frame |
| `collision_grid_build_ms` | Time spent building spatial buckets |
| `collision_candidates_ms` | Candidate generation and testing time |

Both pass counters sum to `collision_passes` (including warmup samples). The
F3 profiler also shows the actual pass counts for auto mode.

## Benchmark with adaptive mode

From the repository root, to check that brute, grid and auto compute identical
contacts, use:

```powershell
python tools/run_benchmark.py --build --timing deterministic --fixed-steps 1 --repetitions 1 --frames 120 --warmup 30 --scenarios collision balanced stress --compare-broadphase --compare-auto
```

Each executable runs in a fresh process with the same workload. Profiles are
saved separately for all three algorithms. The benchmark's mode-specific JSON
files deliberately retain `bruteforce` for standalone reproducible baselines;
`--broadphase auto` overrides that setting *in memory*, not on disk.

## Finding a crossover and tuning `cellSize`

`tools/sweep_broadphase.py` keeps the same sprites, layout, seed and fixed-step
schedule while varying the number of active colliders in the benchmark scene.
It runs a brute-force baseline and several fixed grid cell sizes for every
combination of collider count and threading mode. Experimental order is
alternated across repetitions, and every profile is checked against the brute
baseline **frame by frame**, including warmup.

A quick experiment:

```powershell
python tools/sweep_broadphase.py --build --collider-counts 128 256 384 520 640 740 900 --cell-sizes 1 2 4 --modes single collision --repetitions 2 --frames 180 --warmup 60
```

A longer (default) experiment:

```powershell
python tools/sweep_broadphase.py --build --repetitions 3 --frames 360 --warmup 90
```

Results are written to `games/XEngine Benchmark/results/sweep_<timestamp>/`:

- `runs.csv`: every execution and its profiling statistics.
- `comparisons.csv`: median p50 frame time for each collider count, cell size
  and mode, including brute-to-grid improvement in ms and percent.
- `best_cell_size.csv`: fastest measured grid cell width at each measured count.
- `crossovers.csv`: *piecewise linear interpolation* of observed changes of
  sign in brute-minus-grid timings. It does not extrapolate outside the
  measured range, and the estimates are hardware/workload dependent.
- `equivalence.csv`: all paired contact comparisons (a mismatch aborts the
  experiment). Per-run CSVs, logs, and effective configurations are retained.

**Interpretation:** a mathematical crossover of *measured* median time is not a
universal collider-count threshold. Number of moving bodies, density, broadphase
bucket occupancy, CPU frequency and threading overhead also affect the result.
It is therefore not automatically written back into the engine. The online
`auto` policy continues to measure actual candidate-generation costs. If a
crossing lies between e.g. 384 and 520 colliders, rerun with finer values in
that interval to reduce interpolation uncertainty.

All benchmark measurements require a compiled SDL2 game on the target machine;
unit tests and syntax checks alone cannot establish runtime speedups.

## XEngine v12 — Adaptive cell size

The conclusion of the v11 sweeps was **not** encoded as a universal 314/490
collider cutoff. The previous `broadphase: "auto"` already measured the local CPU
cost of brute force against one fixed cell size. It did not choose a cell size.
The v12 engine adds the independent option to let the runtime measure both:

```json
"Collision": { "broadphase": "auto", "cellSize": "auto" }
```

This is the **default for newly created games** from BlankGame. Existing game
configs retain their explicit settings (including numeric `cellSize`) when
using `sync_engine_configs.py`, so no game's tuning is silently overwritten.

- `bruteforce` + numeric cell size: fixed all-pairs reference (cell size unused).
- `grid` + numeric cell size: fixed uniform grid reference.
- `auto` + numeric cell size: legacy v11 selector, one grid size.
- `auto` + `"auto"` cell size: **four timed alternatives**, brute force and
  grids of 1, 2 and 4 world units around the reference 2.0. This reference
  acts as a scale, not a measured universal optimal size.
- `grid` + `"auto"` or `bruteforce` + `"auto"` is rejected deliberately.

The online selector uses separate exponential timing estimates for the collider
count bracket, worker availability and a coarse **spatial occupancy class**.
It initially samples each of four strategies five times. The normal winner is
changed only if a measured alternative beats it by at least 8%. Every 24 passes
it remeasures a competitor, allowing adaptation to changing workloads. Timings
cover grid construction plus candidate enumeration and narrowphase work. A
classifying occupancy threshold is **not** a broadphase performance cutoff.

This adaptive policy cannot offer one universal collider-count threshold: the
crossovers measured around 314 (single) and 490–500 (multithreaded) were specific
to one benchmark distribution and CPU. It chooses from real measurements within
the running game. It may temporarily choose slower variants while calibrating;
use fixed `bruteforce`/`grid` for controlled performance comparisons.

New profiler columns: `collision_grid_cell_size_last`,
`collision_auto_calibration_passes`, `collision_auto_cell_half_passes`,
`collision_auto_cell_base_passes`, `collision_auto_cell_double_passes`.
The cell size field is zero if no grid ran in that frame. Pass counts are
accumulated over all physical and collision-detection passes.

Run deterministic auto-cell benchmarks with:

```powershell
python tools/run_benchmark.py --build --timing deterministic --fixed-steps 1 --broadphase auto --cell-size auto --scenarios light collision balanced stress --modes single collision --repetitions 3 --frames 360 --warmup 90
```

For **functional equivalence** across brute, grid, fixed-size auto and
adaptive-size auto, use:

```powershell
python tools/run_benchmark.py --build --timing deterministic --fixed-steps 1 --repetitions 1 --frames 120 --warmup 30 --scenarios light collision balanced stress --compare-broadphase --compare-auto --compare-auto-cell
```

The sweep tool also exports `cost_crossovers.csv` (interpolated zero crossings
of **measured broadphase cost**, instead of overall frame-time noise) and
`best_strategy.csv` (the lowest measured build-plus-candidates time at each
sampled count and mode). Neither is silently applied as a fixed threshold.

**Other existing fixed thresholds:** worker dispatch at 192 colliders and
parallel render preparation at 384 eligible commands are unchanged. They need
separate targeted experiments and should not be described as learned by this
broadphase calibrator. A Windows/SDL2 benchmark is needed to validate actual
v12 runtime performance and equivalence beyond unit and syntax tests.

## XEngine v13 — Stable auto strategy and paired comparisons

The v12 multi-run benchmark showed that `auto + auto cellSize` correctly
reproduced all contact signatures, but selection between grid widths could
oscillate in medium/high loads. Its probes affected 8.33% of measured frames
in the deterministic two-pass collision scenarios. Those results did **not**
prove that a single collider-count threshold or cell size works universally.

v13 changes **only the adaptive-cell selector** (`Collision.broadphase: "auto"`
**and** `Collision.cellSize: "auto"`). Existing fixed algorithms and the
legacy `auto` + numeric `cellSize` algorithm remain available unchanged.

- Initial exploration still collects five round-robin timing samples for each
  of four strategies (brute, half/base/double-width grid).
- Each strategy stores its nine most recent samples. A trimmed mean ignores
  one extreme at either end when at least five samples are available.
- An alternative must be at least **12%** cheaper than the current selection
  for **three separate probe samples** to be accepted. These values are
  conservative anti-flapping safeguards, **not** learned collider thresholds.
- The first probe occurs after 24 stable passes; after each three unsuccessful
  probes, the interval doubles up to 192 passes. It never stops probing.
- Once a challenger appears promising, confirmation probes occur every eight
  passes; a successful switch starts a 64-pass cooldown.
- Three sustained observations over 30% above the recent incumbent cost
  restore rapid probing, even if the collider-count and occupancy class stay
  constant. Moving between classes also uses independent calibration state.
- Selection still compares *measured* grid construction + candidate costs.
  The new `collision_auto_selector_ms` column separately measures time used
  to classify occupancy, decide and update the selector. The total observed
  overhead is visible in the frame time; it is not wrongly charged only to
  one candidate algorithm.

These timings are CPU observations affected by noise, compiler/build type,
worker count, collider geometry and spatial distribution. No experimental
crossover (~314 in single-threaded or ~490-500 in parallel benchmark loads)
was hardcoded into the engine. The 12% conservative switching margin trades
some possible small peak-performance gains for greater stability.

### Paired benchmark: four strategies, rotated order

For a quick validation on Windows (32 runs by default):

```powershell
python tools/compare_adaptive_broadphase.py --build --repetitions 1 --frames 120 --warmup 30
```

For a three-repetition comparison (96 runs by default):

```powershell
python tools/compare_adaptive_broadphase.py --build --repetitions 3 --frames 360 --warmup 90
```

The executable is built once, then each variant is launched in its **own
process** with fixed-step deterministic physics, a constant seed, and a
rotating sequence within each scenario/mode/repetition. The variants are:

1. `bruteforce`, cell size ignored;
2. `grid`, size 1;
3. `grid`, size 2;
4. `auto`, `cellSize: "auto"`.

The runner rejects unequal contacts/narrowphase/steps/world draws for **any
frame**, including warmup. It generates `runs.csv`, `summary.csv`,
`paired_comparisons.csv`, `equivalence.csv`, `metadata.json`, as well as every
individual profiler CSV and process log. The summaries include `p50`, `p95`,
measured build+candidate cost, selector overhead and probe counts. It reports
how far auto was from the best **measured fixed** option; this is a local
benchmark comparison, not proof of a universal optimal threshold.

To investigate one count more closely, add `--colliders 500`. Scenarios and
modes can be limited with `--scenarios collision balanced --modes single collision`.

The full SDL2 benchmark must run on the target machine; a GCC syntax/unit
check cannot establish measured performance or Windows runtime behavior.

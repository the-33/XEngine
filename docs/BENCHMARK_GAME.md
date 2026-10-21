# XEngine Benchmark

Independent C++20/SDL2 benchmark game under `games/XEngine Benchmark/`. It uses the engine's actual `Scene`, `GameObject`, `Transform`, `SpriteRenderer`, `Collider2D`, `RigidBody2D` and `Behaviour` components, not a synthetic substitute for the engine's lifecycle.

## Build

From the XEngine repository root:

```bash
# Windows (GCC/MinGW) or Linux (GCC):
python tools/build.py game --project "XEngine Benchmark" --config Debug
```

On Windows Debug, `tools/build.py` also synchronizes the complete dependency DLL chain (SDL2 + MinGW runtimes from the configured GCC toolchain) **beside the executable, even when building without `--run`**. This is required because the benchmark launches the compiled `.exe` from a separate Python process: otherwise Windows might pick incompatible DLLs from `PATH` and exit with `0xC0000139` before the game prints anything. Rebuilding with `--build` repairs missing/stale DLLs, including for an executable that was already up to date. Do not manually combine DLLs from different MSYS2 environments (`ucrt64`, `mingw64`, `clang64`).

The game includes `assets/config/engine.json` as the **default build configuration** required by `tools/build.py` in both Debug and Release. The benchmark does **not** use its flags to decide each measured run. Instead, the executable reads `assets/config/{mode}.json` for `single`, `collision` or `parallel`, validates all critical booleans and passes the verified JSON **text** to `Engine::StartFromJson()`. Unlike `Engine::Start(path)`, this path cannot silently revert to default single-thread settings when the file is missing or invalid. Both builds rely on the same mode-specific startup settings.
All four benchmark configs now declare `Collision.broadphase` (initially
`bruteforce`) and `Collision.cellSize` (2.0 world units). The launcher supplies
`--broadphase` and `--cell-size` as **in-memory overrides** of the mode-specific
runtime JSON, so the JSON values are explicit, but the CLI decides which
broadphase is measured. Use `--compare-broadphase` to run both algorithms with
identical threading and physics settings. The default build-only `engine.json`
does not decide the broadphase for a measured run. When launching the
executable manually with no broadphase arguments, its selected `{mode}.json`
settings are respected; explicit `--broadphase` or `--cell-size` arguments take
precedence individually.

The launcher validates each requested configuration **before building or running** (multiThreading, parallelPreparation, vsync off, profiling on); it also requires all other settings to match between modes, records the SHA-256 of each configuration in `metadata.json` and rejects changes midway through a session. After startup, each game prints a `[BENCH-CONFIG]` confirmation; the launcher requires this confirmation before accepting a profile. Thus a comparison cannot be mislabeled merely because the command-line `--mode` and JSON disagree.

The normal `games/XEngine Benchmark/build.bat` and `build.sh` wrappers also work. The benchmark is built through the same toolchain and static engine library as every other XEngine game. Running the profiler requires a **Debug** build; a Release configuration may not include profiler instrumentation.

## Interactive run

The executable is normally located at:

- Windows: `build/windows-x86_64/debug/games/XEngine Benchmark/XEngine Benchmark.exe`
- Linux: `build/linux-x86_64/debug/games/XEngine Benchmark/XEngine Benchmark`

Launch with arguments, e.g.:

```bash
"build/linux-x86_64/debug/games/XEngine Benchmark/XEngine Benchmark" \
    --scenario balanced --mode parallel --warmup 90 --frames 360 --seed 12345
```

Windows PowerShell equivalent:

```powershell
& '.\build\windows-x86_64\debug\games\XEngine Benchmark\XEngine Benchmark.exe' --scenario balanced --mode parallel --frames 360 --warmup 90 --seed 12345
```

In Debug, the game sets the working directory to its `assets` directory; its own `config/{mode}.json` files choose the CPU worker and render-preparation configuration **before** the engine starts. Nothing is rebuilt when changing `--mode`. Changing the mode **while the same game process is running is intentionally not supported** because persistent workers and timing histories should be reset. Every mode is a clean process restart. The game closes itself via an SDL quit event after the requested number of frames plus warmup, so the engine can flush the profile CSV normally. Use F3 for the compact stats panel and F4 to advance its pages.

## Scenarios

| Scenario | Sprites | Colliders | Kinematic bodies | Main purpose |
| --- | ---: | ---: | ---: | --- |
| `light` | 120 | 64 | 12 | Show multithreading overhead below dispatch thresholds |
| `render` | 2,200 | 0 | 0 | Exercise parallel command preparation independently of collisions |
| `collision` | 220 | 740 | 0 | Exercise parallel collision candidate evaluation |
| `balanced` | 1,300 | 520 | 96 | Mixed workload resembling a busy game scene |
| `stress` | 2,600 | 1,050 | 240 | Push both parallel paths at once |

Animated `BenchmarkMotion` components are part of the gameplay load. The object positions and texture choices are deterministic for a given seed on the same platform/compiler. Objects use a collider lattice with intentional overlap, trigger contacts, and some physics bodies. The engine's regular fixed-step physics and event dispatch are still in use. The camera sees the entire grid; hardware texture rendering is used where available.

## Visible-render validation

The benchmark must render **real, on-screen sprites** in every scenario. The
launcher now rejects a run with `render_world_draws_mean == 0`, and includes
`render_world_draws`, `render_immediate_culled`, `render_culled_commands` and
`render_recorded_total` in `summary.csv`. A successful parallel-preparation
count alone is **not** proof that sprites have been submitted to SDL.

This protects against a previous `Camera2D::viewBase` property binding bug:
`viewBase` erroneously used `CenterProperty`, so assigning a viewport size
actually displaced the camera and culled every sprite. The correct member is
`ViewProperty viewBase{ this };`. Historical results collected with that bug
must not be used to benchmark real sprite submission or GPU rendering.

In **realtime** mode, the engine's accumulator executes wall-clock-driven
fixed updates: an equal number of rendered frames does not imply an equal
number of physics steps. Compare physical CPU time per fixed step, or use the
**deterministic** benchmark mode below to schedule equal physics workloads.
Frame p50/p95 can otherwise compare very different populations of frames.

Since v7, the profiler resets and accumulates physics and collision counters
**per rendered frame**, including every `PhysicsManager` substep and the
separate event-detection pass. A frame with zero fixed updates now reports
zero `collision_snapshot_ms`, `collision_candidates_ms`, `collision_merge_ms`,
`broadphase_tests`, `narrowphase_tests`, `contacts_built`, and physics step
subtotals. Frames with multiple steps contain sums, not the last step alone.
Unlike v6, collision phase and pair-check figures include physics solver
BuildContacts calls, in addition to collision/trigger event detection. This
change of definition means those fields cannot be directly compared to older
v6 figures without normalization.

## Modes

| Mode | `multiThreading` | `parallelPreparation` | Explanation |
| --- | --- | --- | --- |
| `single` | false | false | Baseline: collision processing and rendering on main thread |
| `collision` | true | false | Workers only for collision candidate evaluation |
| `parallel` | true | true | Collision workers **and** CPU parallel render command preparation |

The renderer and collision worker paths remain **adaptive**: the JSON selects permission to run in parallel, **not evidence that a worker actually processed a given frame**. The profile now includes `collision_parallel_steps` (number of fixed-step collision passes per frame that actually dispatched parallel candidate evaluation). Worker count only tells us whether a pool exists; this new metric records whether the collision path was used. `render_parallel_frames` counts actual parallel render preparation frames. The runner checks for unexpected workers in `single` and unexpected parallel render frames in `single`/`collision`, and warns if `parallel` never actually prepares render commands in parallel or if a requested worker pool cannot be created.

The render path is **adaptive** and may remain immediate when there are fewer than 384 eligible world commands. The collision path requires at least 192 active colliders before dispatching worker tasks. A mode selection does **not** guarantee that the adaptive parallel route was used for every frame. Check `render_parallel_frames` and `render_path_reason` in the captured CSV.

## Automatic runner

Use the built binary with the launcher:

```bash
python tools/run_benchmark.py --build --repetitions 3 --warmup 90 --frames 360 --seed 12345
```

### Equal-workload deterministic benchmark (new)

```bash
python tools/run_benchmark.py --build --timing deterministic --fixed-steps 1 --repetitions 3 --warmup 90 --frames 360 --seed 12345
```

- `--timing realtime` (default): unchanged accumulator behavior; measure real
  game frame pacing and variable physics workload.
- `--timing deterministic --fixed-steps N` (`1 <= N <= 16`): each rendered
  frame executes **exactly N** `FixedUpdate` + `PhysicsManager::Step` +
  collision dispatch passes, independent of elapsed wall time. The accumulator
  is bypassed; the `Update` callback receives `fixedDt * N` to keep simulation
  dt consistent. Worker selection remains controlled only by `single.json`,
  `collision.json`, or `parallel.json`.
- For 360 measured frames and `--fixed-steps 1`, **each run** carries 360 fixed
  steps (plus 90 during warmup). The launcher validates the two dedicated
  profile columns **on every captured frame, including warmup**, and rejects
  a run with the wrong step count. The stdout handshake `[BENCH-TIMING]`
  prevents accepting an outdated executable that silently ignored the option.
- This controls **the number of physics steps**, not CPU/GPU scheduling,
  operating-system timing, or a bit-identical physics state on every host.
  Native `frame_ms`/phase CPU timings remain real measured wall time.

To run the previous benchmark explicitly:

```bash
python tools/run_benchmark.py --timing realtime --repetitions 3 --frames 360 --warmup 90
```

To benchmark quickly:

```bash
python tools/run_benchmark.py --scenarios render collision balanced --repetitions 1 --frames 180 --warmup 30
```

Run a nonstandard executable:

```bash
python tools/run_benchmark.py --exe "/path/to/XEngine Benchmark" --scenarios stress --modes single parallel --repetitions 3
```

Supported options include `--scenarios`, `--modes`, `--repetitions`, `--frames`, `--warmup`, `--seed`, `--timing`, `--fixed-steps`, `--exe`, `--build`, `--timeout` and `--output`. The launcher changes the order of the three modes between repetitions, isolates every run in a separate process, and moves the original profiler CSV to a uniquely named destination. It never overwrites the outputs of another scenario or mode. Each invocation creates its own timestamped directory by default: `games/XEngine Benchmark/results/YYYYMMDD_HHMMSS_microseconds/`.

Outputs:

- `summary.csv`: one row per scenario/mode/repetition, with mean, median (p50) and p95 per metric, totals for `fixed_steps`, `collision_passes`, `physics_substeps`, and CPU times normalized by physical step or collision pass. The `timing` and `scheduled_fixed_steps` columns identify the workload. Paired `speedup_vs_single_pct` is provided when a baseline exists. Do not interpret the p50 frame speedup as the speedup of a single collision pass.
- `{scenario}_{mode}_runXX.csv`: original full frame-by-frame engine profile, including warmup.
- `{scenario}_{mode}_runXX.log`: stdout, stderr, process exit code and execution duration.
- `metadata.json`: run configuration, environment, selected scheduling mode, expected fixed-step totals, and SHA-256 of each runtime JSON.
- `failures.txt`: created only if runs failed; failure status is returned to the calling shell. A Windows loader error (such as `0xC0000139`, `0xC0000135` or `0xC000007B`) **aborts the batch after the first process** instead of repeating the same startup failure for every scenario/mode. Inspect the corresponding `.log`. Other per-scenario failures continue to be collected.

The run-level profile CSV also contains `collision_passes`, `collision_parallel_passes`,
`contacts_built`, `physics_step_ms`, `physics_integrate_ms`,
`physics_build_contacts_ms`, `physics_solve_ms`, `physics_substeps`,
`physics_contacts_processed` and `deterministic_steps_per_frame`. The existing
`collision_parallel_steps` counts fixed-step event-detection passes that used
parallel candidates, while `collision_parallel_passes` counts **all** parallel
BuildContacts calls, including physics substeps. In particular, divide the
sum of candidate CPU timings by `collision_passes_total` (not just
`fixed_steps_total`) when comparing per-collision-pass costs.

The runner discards the first `--warmup` frame samples and uses exactly `--frames` samples for aggregates (a final SDL_QUIT frame may also exist in the original CSV). It reports frame **wall time**, not just CPU time; `vsync` is explicitly disabled in every mode. Time measurements depend on GPU drivers, frequency scaling, thermals and other processes. Run on the same machine, graphics backend and resolution; use several repetitions and interpret p50/p95 together with `fixed_steps`, `broadphase_tests`, `render_eligible_current` and `render_path_reason`. Do not interpret the number of worker threads as achieved speedup. In real-time mode, fixed-step counts can differ with frame time, so normalize collision/physics comparisons against actual step/passes counts. Deterministic mode instead schedules equal counts and validates them before accepting results.

**Limitations:** A complete performance comparison requires a compiled SDL2 engine and a usable real graphics driver. CPU-only tests validate the benchmark runner and render preparation, but do not measure the SDL application's runtime. This repository does not ship fabricated timing results.

## v13: paired adaptive-strategy benchmark

`tools/compare_adaptive_broadphase.py` rotates four baseline/adaptive strategies
within each deterministic scenario/mode/repetition, saving profiles separately
and verifying contact equivalence for every recorded frame:

```powershell
python tools/compare_adaptive_broadphase.py --build --repetitions 3 --frames 360 --warmup 90
```

See [ADAPTIVE_BROADPHASE.md](ADAPTIVE_BROADPHASE.md) for the robustness rules,
statistics, output CSV files and the reduced smoke-test command.

## Pruebas de distribución espacial dinámica (v14)

El benchmark dispone ahora de `--layout static|clustered|spread|alternating|cycling`
y `--phase-frames N`. Las distribuciones se cambian por índice de fotograma,
manteniendo constante el número de colliders. El comando
`tools/compare_adaptive_broadphase.py` acepta `--layouts` y genera los CSV
`phase_summary.csv` y `phase_paired.csv` con los resultados de cada tramo.

Consulta [DYNAMIC_BROADPHASE_BENCHMARK.md](DYNAMIC_BROADPHASE_BENCHMARK.md)
para la metodología y los comandos de comparación. Sin estos parámetros,
la prueba anterior continúa utilizando el layout `static`.

## Render threshold calibration (v16)

For the paired render-worker experiment, see [`RENDER_THRESHOLD_CALIBRATION.md`](RENDER_THRESHOLD_CALIBRATION.md). The benchmark supports `--sprites N` and `--render-threshold N` overrides, and `tools/sweep_render_threshold.py` compares immediate vs recorded/parallel rendering at controlled loads. The production threshold remains 384 pending real Windows measurements.

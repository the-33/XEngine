# Multithreading and performance profiling

## Model

XEngine uses one SDL/gameplay thread for **window events, input, scene lifecycle,
`Update`, `FixedUpdate`, physics integration, collision callbacks, SDL renderer,
texture usage and `SDL_RenderPresent`**. This is intentional: gameplay and SDL
resources cannot safely be mutated from independent threads without an explicit
ownership model.

When `Engine.multiThreading` is enabled, XEngine creates a **persistent, bounded
CPU worker pool** (up to three worker threads, according to the number of
available cores). The pool parallelizes *independent geometry checks for
collision candidates*, as well as optional *render command preparation*. A snapshot of collider geometry and layer data
is built on the main thread, then worker threads read only that immutable data.
Candidate results are merged in deterministic collider order and collision/
trigger callbacks are dispatched on the main thread. Scene transitions and
component destruction occur after the frame's collision work has finished.

For **fewer than 192 active colliders**, candidate evaluation stays on the main
thread to avoid unnecessary thread scheduling overhead. A game with only a few
objects should not be expected to run faster in the parallel mode. Even larger
scenes may be constrained by rendering or VSync. This threshold is a heuristic,
not a performance guarantee. The physics solver is currently single-threaded.

## Adaptive parallel render preparation

The `Renderer.parallelPreparation` setting defaults to `true` but only takes effect
when `Engine.multiThreading` is also `true`. The main thread keeps all SDL
renderer calls, SDL_ttf calls, live scene access, texture creation/destruction and
`SDL_RenderPresent`. In the default `fixed` selector, when at least **1536 world commands** were submitted
in the previous frame, the engine records draw commands in their original
submission order, captures a plain-data camera snapshot, and uses the **same
persistent CPU worker pool** used for collisions to calculate world-to-screen
rectangles and conservative rotated-AABB visibility for textures and world
rectangles. Commands are then replayed on the main thread, preserving sprite
Z ordering, alpha blending and UI ordering. Work is chunked (64 commands per
chunk) to avoid per-sprite task dispatch overhead. Small scenes continue to use
the original immediate path, without recording/copying command vectors.

A fixed-size cache of up to 128 text textures prevents repeated SDL_ttf
rasterization for frequently reused labels; it stores a font/color/text key and
is cleared when asset fonts are reinitialized or shut down, and when the SDL
renderer is recreated. Cache eviction happens after presenting the frame.

This is **CPU preparation**, not multiple GPU renderers or parallel SDL calls.
Performance is workload-dependent, and recording can cost more than it saves.
Sprites are already culled when using the immediate renderer; parallel mode
moves that work to worker threads and reuses precomputed texture coordinates.

You can disable render preparation independently while testing collision workers:

```json
{
  "Engine": { "multiThreading": true, "profileCsv": true },
  "Renderer": { "parallelPreparation": false, "vsync": false }
}
```

To test render preparation, change `parallelPreparation` to `true`. This
sample is a fragment; preserve the remaining game configuration keys.

This replaces the old approach of running SDL rendering on a worker and holding
a global scene mutex around physics and gameplay. That design could serialize
nearly all useful work, duplicate large draw-command snapshots, and violate
SDL's main-thread rendering requirements on some platforms.

## New-project template defaults

`tools/templates/BlankGame/assets/config/engine.json` includes the following
settings for games created with `new_project.bat` or `new_project.sh`:

```json
{
  "Engine": {
    "multiThreading": false,
    "profileCsv": false,
    "logStats": true,
    "logErrors": true
  },
  "Renderer": {
    "vsync": true,
    "hardwareAcceleration": true,
    "parallelPreparation": true,
    "bgColor": [24, 24, 36, 255]
  }
}
```

This is only an excerpt; keep the other configuration sections. For A/B
benchmarks, first disable VSync, then compare `multiThreading: false`,
`multiThreading: true` with `parallelPreparation: false`, and
`multiThreading: true` with `parallelPreparation: true`. Enable
`profileCsv` for each measured run. The template preserves empty asset
folders with `.gitkeep` files; Release packaging excludes those placeholders.

## Profiling configuration

Add or edit the `Engine` section of the game's engine JSON configuration,
e.g. `games/Blob's Quest/assets/config/engine.json`:

```json
{
  "Engine": {
    "multiThreading": false,
    "profileCsv": true,
    "logStats": true,
    "logErrors": true
  },
  "Renderer": {
    "vsync": false,
    "hardwareAcceleration": true
  }
}
```

The snippet demonstrates relevant fields; preserve the rest of your game's
actual configuration. `profileCsv` defaults to `false`. With profiling enabled,
XEngine buffers a maximum of 100,000 frame samples and writes one CSV **when the
game loop exits normally**:

- `xengine_profile_single.csv` for `multiThreading: false`
- `xengine_profile_parallel.csv` for `multiThreading: true`

The files are written to the game **working directory** (Debug: usually the
assets directory; Release: the launch directory). They are deliberately not
created by default. When a run is forcibly terminated, the CSV may not be
written. Capture equally long runs under similar load, preferably Release builds.

Columns include `frame_ms`, `input_ms`, `fixed_ms`, `update_ms`,
`render_present_ms`, `physics_ms`, `collision_ms`, `collision_snapshot_ms`,
`collision_candidates_ms`, `collision_merge_ms`, fixed steps, broad/narrowphase
test counts and worker count. Additional columns are `render_record_ms`,
`render_prepare_ms`, `render_submit_ms`, `render_only_present_ms`,
`render_sdl_present_ms`,
`render_worker_wait_ms`, `render_prepared_commands`, `render_culled_commands`,
and `render_workers`. `render_prepare_ms` **includes** worker waiting;
`render_worker_wait_ms` is a diagnostic subset, not a separate phase to add. Timing uses `std::chrono::steady_clock` in **both**
modes, rather than treating `TimeManager::timeSinceStart` as a stopwatch.
`render_only_present_ms` includes all of `RenderManager::End()` (presentation
**and cleanup**); the new optional `render_sdl_present_ms` measures only the
SDL call and is unavailable in historical captures. Neither timer measures
GPU execution independently.
`fixed_ms` includes physics and collision substeps; those numbers must not be
added twice. `render_present_ms` includes VSync waiting, when enabled; it is not
pure CPU work. The onscreen Debug overlay (F3) reports frame median (p50) and
p95 from a rolling 512-frame window using the same clock.

### On-screen Debug profiler (F3 / F4)

**F3** toggles the profiler overlay in Debug builds. **F4** advances to the
next profiler page while the overlay is visible. A page counter is displayed
at the top; all sections are accessible even at lower resolutions. Both
single-threaded and multithreaded measurements use the same layout.

The overlay measures its actual SDL_ttf font widths and heights before placing
labels and values, instead of assuming monospaced character widths. It aligns
values separately, automatically packs sections into columns, and paginates
when the viewport is too small. Overlong labels are truncated with an ellipsis
instead of overlapping neighboring columns. Geometry and fitted strings are
cached and refreshed four times per second (or on window resize), avoiding
font measurements on every frame. **These UI changes do not alter the CSV
profiler or the clocks used for performance measurement.**

## Comparing runs

Keep the same scene, resolution, number of colliders, speed, OS and hardware.
Disable VSync when measuring maximum throughput; repeat with VSync enabled to
assess pacing. Compare **medians and 95th percentiles** rather than one FPS
observation. The tool works with the standard Python library:

The simplest option is to specify a game by name from the XEngine repository root:

```bash
python tools/compare_profiles.py --game "Blob's Quest"
# Also accepted: python tools/compare_profiles.py "Blob's Quest"
```

The tool searches only the named game under `games/`, including its `assets/`
(Debug runs), the game directory, and `dist/<platform>/<version>/` (Release
runs). It automatically picks the newest complete single/parallel pair from
the same directory and prints the selected paths. If there is no pair in one
directory, it selects the most recent file for each mode and warns that the
captures come from different directories. If either capture is missing, the
command explains which one and where it searched. You can pass a game directory
instead of the name:

```bash
python tools/compare_profiles.py --game "games/Blob's Quest"
```

The original explicit CSV mode remains supported (including files outside the
game directory):

```bash
python tools/compare_profiles.py path/to/xengine_profile_single.csv path/to/xengine_profile_parallel.csv
```

The comparator discards the first 120 frames by default to avoid startup noise;
for short runs, use `--warmup 0`. Inspect the printed mean physics/collision
counts to confirm that both runs processed comparable workloads. A difference
in work invalidates a simple speedup comparison.

**Expected result:** small games may see zero improvement or a slight regression
from snapshot/dispatch overhead. Parallel collision is intended for scenes with
many independently testable colliders. Optimize the actual dominant phase before
adding further thread pools. Never parallelize SDL calls or gameplay callbacks
merely to increase the number of threads.

## Development checks

The standalone `ParallelExecutor` is a header-only C++20 worker pool. The
render projection/culling helper (`RenderPreparation.h`) also has an SDL-free
C++20 regression test which compares the serial and worker-produced results.
Both have regression tests that compiles and runs on an available host `g++` without SDL:

```bash
python -m unittest discover -s tests -p test_multithreading_runtime.py -v
python -m unittest discover -s tests -p test_parallel_render_preparation.py -v
```

The suite tests worker reuse and exactly-once processing, common frame timing,
main-thread SDL/lifecycle constraints and the CSV comparator. After applying a
patch, also build and run a real game on Windows and Linux and verify rendering,
collisions, physics, shutdown and scene changes. Cross-compilation by itself does
not substitute for runtime testing.

## Adaptive render diagnosis and repeatable CPU-only benchmark

For the latest render path selection, detailed SDL draw-call categories,
background/world/debug/UI timings, texture modulation optimizations, immediate
and prepared culling counters, and the standalone deterministic C++20 workload
benchmark, see [RENDER_DIAGNOSTICS.md](RENDER_DIAGNOSTICS.md).

The profiler distinguishes **selected snapshot rendering** (`render_path_reason=3`)
from **actual worker participation** (`render_workers > 0`). A large number
of tiled-background SDL calls is not sufficient to trigger the parallel
scene-preparation threshold. To analyze recorded runs, use:

```bash
python tools/compare_profiles.py --game "Blob's Quest"
```

To measure only the CPU preparation and synthetic AABB-pair workload without
SDL/GPU:

```bash
python tools/benchmark_render_cpu.py --sprites 2048 --colliders 256 --repeats 50
```

**Do not interpret synthetic benchmark results as game FPS.** The full engine
and SDL rendering still require native/cross builds and on-device validation.

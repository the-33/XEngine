# Spatial broadphase and rendering optimizations — XEngine v8

## Broadphase selection

XEngine now supports two candidate-generation algorithms without changing the
existing narrowphase, physics solver, event dispatch, scene callbacks or worker
architecture:

- `bruteforce` (**default**, backward-compatible): existing nested pair loops.
- `grid`: rebuilds a spatial hash of world AABBs for each collision pass. Only
  objects sharing one or more grid cells become narrowphase candidates.

The `Collision` section is included in the XEngine Benchmark
(`assets/config/engine.json`, `single.json`, `collision.json`, `parallel.json`)
and the `tools/templates/BlankGame/assets/config/engine.json` template. The
files ship with the backward-compatible default:

```json
"Collision": {
  "broadphase": "bruteforce",
  "cellSize": 2.0
}
```

To enable the spatial grid in a regular game, set `"broadphase": "grid"` in
its runtime `engine.json`. New projects created from `BlankGame` already expose
these settings. Existing games without this section still use the same defaults.

For **XEngine Benchmark**, `engine.json` is required by the build process but
is not the configuration loaded by benchmark measurements: `single.json`,
`collision.json` or `parallel.json` is loaded according to the selected mode.
The benchmark runner overrides only `Collision.broadphase` and
`Collision.cellSize` *in memory* with `--broadphase` and `--cell-size` (defaults:
`bruteforce` and `2.0`). This preserves comparable settings for each mode and
allows automated `--compare-broadphase` runs without rewriting JSON files.
When the benchmark executable is started directly **without** `--broadphase`
or `--cell-size`, it instead uses the corresponding values from its selected
`{mode}.json` file. That file, not the build-only `engine.json`, controls the
manual benchmark session.

`cellSize` is in **world units**; valid range is 0.05–4096. Smaller cells
reduce candidate pairs in sparse scenes but create more buckets and hashing
work. Oversized colliders spanning more than 128 cells take a conservative
all-pairs fallback. AABBs touching a cell edge are included on both sides.

Every collision pass snapshots colliders on the main thread. The read-only grid
is built once per pass, then candidate evaluation can use existing collision
workers. Candidate indices are sorted and deduplicated before narrowphase so
the resulting order matches `bruteforce` and existing Enter/Stay/Exit handling.

## New profiler columns

- `collision_broadphase_mode`: 0 = brute force, 1 = spatial grid.
- `collision_grid_build_ms`: sum of spatial-index construction time across
  collision passes **within the same frame** (zero for brute force).
- `collision_contact_signature`: unsigned 64-bit FNV-1a signature of ordered
  contact entities, trigger state, depth, normal and point, accumulated across
  all collision passes. Used for deterministic equivalence checks. Hashes are
  diagnostic evidence, not a mathematical proof of equality.

`broadphase_tests` counts **AABB checks actually performed after the existing
owner/layer/mask filters**; unlike the old brute force path, the spatial grid
avoids checking many eligible-but-separated pairs. `narrowphase_tests` and
`contacts_built` must remain the same for deterministic equivalent scenes.
The new grid-build column must be included when comparing candidate-generation
costs (`collision_grid_build_ms + collision_candidates_ms`).

## Deterministic comparison of broadphases

From the repository root:

```powershell
python tools/run_benchmark.py --build --timing deterministic --fixed-steps 1 --repetitions 3 --frames 360 --warmup 90 --compare-broadphase
```

This performs **90 runs** (5 scenarios × 3 threading modes × 3 repetitions
× 2 broadphases). It keeps profiles in `results/<timestamp>/bruteforce/`
and `results/<timestamp>/grid/`, saves an `equivalence.txt`, and exits with a
failure if even one frame differs in contact signature, number of contacts,
narrowphase checks, fixed steps, or world-draw count. The comparison includes
warmup frames. Run-to-run timings still depend on hardware and system load.

For a faster initial check:

```powershell
python tools/run_benchmark.py --build --timing deterministic --frames 120 --warmup 30 --repetitions 1 --scenarios collision balanced stress --compare-broadphase
```

Run a single algorithm separately with `--broadphase grid` or
`--broadphase bruteforce`; tune grid size via `--cell-size 2.0`.
The benchmark injects the selected `Collision` settings into the validated
mode JSON *in memory*, without modifying the mode files or changing the
single/collision/parallel thread switches. Benchmark metadata records which
algorithm and grid width were used.

## Render optimizations

- **Per-texture modulation cache:** color and alpha modulation are cached by
  `SDL_Texture*` per frame, avoiding repeated SDL state calls when textures
  alternate (e.g. 4 textures across 2,600 sprites). Entries are removed when
  `Texture` releases its SDL handle; cache is cleared on frame begin,
  renderer restart and shutdown. Direct external SDL mutations during a frame
  must be coordinated with `RenderManager` to avoid stale cached state.
- **Command recording:** `DrawCmd` objects are constructed directly in the
  reusable snapshot vector instead of constructing a separate temporary and
  moving it into the vector. Existing draw order and geometry preparation
  logic remain unchanged.

**Performance is not yet verified on Windows/SDL2.** Compare fresh v8
profiles with your v7 baseline in deterministic mode; small time differences
require several repetitions before interpreting them as improvements.

## v11: Adaptive selection and crossover calibration

`auto` can now choose between the two existing algorithms at runtime using
measured cost, with hysteresis and periodic re-probing. New game templates use
`auto`; benchmark mode profiles intentionally retain the fixed baseline.
See [ADAPTIVE_BROADPHASE.md](ADAPTIVE_BROADPHASE.md) for the selector algorithm,
new profiler columns, deterministic equivalence tests and the cell-size/count
sweep, including its interpolated crossover output.

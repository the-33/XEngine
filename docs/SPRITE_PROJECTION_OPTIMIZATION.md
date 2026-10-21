# Sprite XYZ projection and worker offload (v16.8)

## Motivation and actual scope

The v16.3 full SDL render sweep (five workload types, 256–1024 sprites)
reported negative **median** parallel frame-time gains for every group.
The v16.4 `rotations_xyz` sweep (256–2048 sprites) reported 18/18 equivalent
pairs, but 3/18 parallel p50 victories and no supported parallel threshold.
These results do **not** establish a universal crossover point or a faster
parallel renderer. At the time of this v16.8 experiment the threshold was **384**; v16.9 introduces an experimental 1536 reference and an opt-in adaptive selector.

Source analysis showed `SpriteRenderer::Render()` computing two X/Y cosines and
one Z sine/cosine pair on the **main thread**, even for the typical un-tilted
or zero-offset case. That work preceded `RenderManager::DrawTexture()` and was
therefore never performed by the render-preparation workers.

## Two independent changes

1. **Fast-path for absent rotations/offsets**: pure CPU
   `SpriteProjection::Project(Pose)` skips `cos(0)` for the pseudo-3D X/Y tilt,
   skips all Z trigonometry if `offsetX == offsetY == 0`, and skips Z
   trigonometry for nonzero local offsets when `rotationZ == 0`.
   The Z angle itself is still supplied to `RenderPreparation` / SDL, so
   sprite rotation semantics and drawing order are unchanged. This change
   also affects the *immediate* renderer, so it may speed up both arms.
2. **Offload actual XYZ projection**: in recorded/parallel mode,
   `SpriteRenderer` now reads each live Transform/Texture on the main thread,
   captures only its numeric pose and source dimensions, and appends an
   immutable `SpriteProjection::Pose` to a reusable sidecar array in the
   snapshot. The `DrawCmd` stores an index into that array. Workers project
   the raw pose into world-space geometry immediately before CPU-only camera
   projection and culling, in a disjoint command slot. The sidecar keeps the
   much larger per-command structure from expanding for other command types.
   SDL tint and `SDL_RenderCopyExF`, texture lifetime, and final command replay
   remain on the **main thread**.

The benchmark switch `Renderer->SetDeferredSpriteProjection(false)` selects
projection on the main thread even if recording; `true` is the default.
This is a diagnostic switch, **not a new `engine.json` setting**. Therefore
no config template/schema synchronization is necessary.

## A/B test (isolates the offload, not all rendering optimizations)

Run from the repository root on a machine with the SDL2 development SDK:

```powershell
python tools/sweep_sprite_projection.py --build --workloads baseline transforms rotations_xyz --counts 256 384 768 --repetitions 1 --frames 120 --warmup 30
```

The `--build` phase now prints engine-library and benchmark-game compiler output
**live in the terminal**, including warnings and errors. The exact combined
stdout/stderr stream is also written to `build.log` inside the sweep results
folder (for example, `games/XEngine Benchmark/results/sprite_projection_.../build.log`).
A build error stops the sweep before running any performance samples. This
same live+file logging is used by `sweep_render_threshold.py`. No `--verbose`
flag is needed; even cached builds report their cache status.

Only after 9/9 paired comparisons pass:

```powershell
python tools/sweep_sprite_projection.py --build --workloads baseline transforms tint culling mixed rotations_xyz --counts 384 768 1536 2048 --repetitions 3 --frames 360 --warmup 90
```

In the A/B sweep, **both variants** enable parallel recording, replay and
workers with `--render-threshold 1`. The variants differ only in whether
`SpriteProjection::Project` runs on `main` or `workers`. Three repetitions
alternate process order to reduce systematic order bias. The script checks
physics/contact signatures, rendered world/background/UI counts, eligible
commands, prepared command count, culling count and worker participation for
**every frame including warmup**. Outputs include `comparisons.csv`,
`equivalence.csv`, `summary.csv`, `metadata.json` and raw per-run profiles.
An equal p50 is not a proof of identical pixel output.

Interpret `render_world_ms` and `render_record_ms` as potentially decreasing
when CPU projection moves from the main thread to workers, while
`render_prepare_ms` may increase. The only meaningful overall speedup is a
reliable improvement to end-to-end `frame_ms` p50/p95 without meaningful
p95 regressions. Nested timings must not be added together.

### Full-path comparison versus immediate render

Separately rerun the existing benchmark using the new code:

```powershell
python tools/sweep_render_threshold.py --build --workloads baseline transforms tint culling mixed rotations_xyz --counts 256 384 768 1536 2048 --repetitions 3 --frames 360 --warmup 90
```

That answers a *different* question: does the complete recorded worker
pipeline beat immediate SDL rendering after these changes? Because both
arms now benefit from zero-rotation fast paths, do **not** compare this with
the old v16.3/v16.4 sweep as though the workloads' CPU implementation were
unchanged. A/B `main` vs `workers` avoids that confounder within v16.8.

### Limitations

- No Windows/native SDL runtime benchmark or screenshot/pixel-perfect
  verification is performed in the SDK-free test environment used to prepare
  this patch. Collect Windows/Ubuntu benchmarks before claiming an FPS gain.
- Main-thread scene traversal, `GetComponent<Transform>()`, reading Texture
  dimensions, copying command data, command ordering and **all SDL calls**
  remain main-thread tasks. This is a targeted geometry optimization, not a
  fully parallel renderer and not a generic GPU batching implementation.
- The new per-frame sidecar uses extra memory, although only sprite commands
  populate it. Benchmark CPU memory/cache costs across all workloads.
- The new numerical fast paths are tested against the previous equations,
  including XYZ rotations, nonuniform scale, pivot/offset and near-90-degree
  tilts. Numerical tolerance is applied to floating-point comparisons;
  renderer/culling frame-counter equivalence must still be checked on SDL.

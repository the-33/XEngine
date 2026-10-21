# Render diagnostics, SDL submissions and controlled benchmarks

## Why draw calls do not imply parallel preparation

`Renderer.parallelPreparation=true` enables CPU-side projection/culling only
when `Engine.multiThreading=true` and the **previous frame** contained at least
384 eligible scene commands (`DrawTexture` or `DrawRect`). Background tiling,
UI and debug primitives can generate many SDL draw submissions, but are not
eligible. `render_path_reason=3` means the snapshot path was selected using
previous-frame counts; `render_workers=0` is still possible if the current
frame has too few recorded commands to dispatch to workers. `render_path_reason` in the CSV reports why a path was chosen:

| Value | Reason |
| --- | --- |
| 0 | disabled by Engine/Renderer config |
| 1 | no worker pool available |
| 2 | below 384 eligible scene commands in previous frame |
| 3 | CPU parallel preparation selected |

`render_immediate_culled` counts offscreen world sprite/rect/line calls
rejected before SDL in the immediate path (in recorded playback, offscreen
prepared commands are counted by `render_culled_commands`).

`render_eligible_previous` is the count used for selection;
`render_eligible_current` counts this frame's eligible scene commands. The
recorded list also includes UI/debug commands and is not equivalent to eligible
commands. The F3 overlay reports the route and counts; CSV contains per-frame
details. The first frame normally selects the immediate path.

## SDL call diagnostics

New optional CSV columns (existing consumers can ignore them):

- `render_background_draws`, `render_world_draws`, `render_debug_draws`,
  `render_ui_draws`: actual SDL draw function invocations (not GPU draw-call counts).
- `render_background_ms`: clear and background work, including tiled layers.
- `render_world_ms`: `Scene::Render` elapsed time; **includes** the nested
  `render_debug_ms` and time to traverse/sort/dispatch game objects. It is not
  exclusively time inside SDL draw calls.
- `render_debug_ms`: time spent in `FlushDebug`, including command recording
  when parallel snapshot mode is active.
- `render_sprite_submit_ms`: time spent setting tint + calling SDL to draw
  **world sprites**; measured per sprite when CSV profiling is enabled.
  This timing adds overhead and is nested within world/submit timings.
- `render_ui_ms`: UI ending and F3 debug overlay; in recorded mode this is
  recording overhead, while SDL submission occurs later.
- `render_tint_cache_hits` and `render_tint_updates`: repeated texture
  color/alpha modulation operations saved/applied per frame.
- `render_recorded_total` reports recorded commands even if the selected snapshot
path used no workers that frame; `render_workers` gives actual worker-pool
participation.
- `render_record_ms`, `render_prepare_ms`, `render_worker_wait_ms`,
  `render_submit_ms`, `render_only_present_ms` retain their previous meanings.

The frame-cached drawable dimensions are reused for screen-space draw calls,
so tiled backgrounds do not query the SDL window size for each tile.

A world line whose conservative screen-space bounds lie wholly outside the
viewport is now discarded before calling SDL. Repeated color/alpha modulations
on the same texture are skipped until the tint or texture changes (cache resets
at the start of each rendered frame and upon renderer recreation). These changes
preserve draw order and do not attempt unsafe reordering of transparent sprites.

In immediate mode `render_submit_ms` encompasses background, world and UI;
in recorded mode `render_record_ms` covers scene recording and `render_submit_ms`
covers final playback. Nested times **must not** be summed with their parent.
Profiling overhead affects performance; use comparable runs on the same build.

## Real gameplay comparison

Enable `Engine.profileCsv=true`, disable VSync for raw throughput testing, and
capture the same resolution and workload in both modes. Close normally to flush
CSV, then run:

```sh
python tools/compare_profiles.py --game "Blob's Quest"
```

Check `render_path_reason`: if no frames are `3`, the capture did **not** test
parallel rendering. The comparison script reports route distribution and mean
SDL submissions. Use the p50 and p95 frame times, not FPS alone; compare runs
with matched game state, duration and number of objects. Disable the F3 overlay
while measuring.

## Reproducible synthetic CPU-only workload

```sh
python tools/benchmark_render_cpu.py --sprites 2048 --colliders 256 --workers 3 --repeats 50 --warmup 5 --csv render_cpu_benchmark.csv
```

Runs sequential and parallel code on the **same** deterministically generated
sprites and AABB collider pairs, alternating order to reduce drift, checking an
identical result checksum and exporting each timed sample. To test a threshold,
repeat with small and large sprite/collider counts. Use `--cxx` for a specific
C++20 compiler. No third-party Python packages or SDL libraries are required.

**Important limitation:** this is a CPU-only synthetic geometry benchmark,
not an interactive SDL scene, full physics step, renderer call benchmark or GPU
benchmark. It can demonstrate whether the pool overhead is worthwhile for CPU
jobs, but cannot establish a FPS improvement in Blob's Quest. A final end-to-end
comparison still requires actual game profiles with matching workloads.

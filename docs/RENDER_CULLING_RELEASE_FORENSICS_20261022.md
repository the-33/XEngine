# Release culling selector: phase and tail-latency investigation (2026-10-22)

## Evidence and scope

Input: maintainer-provided `render_selector_20261022_112229_339088(1).zip`.
This is an **analysis of the existing Windows Release executable**, not a
benchmark of the instrumentation patch shipped with this report. The workload
was `culling`, 2,048 maximum sprites, provisional threshold 1,536, fixed
deterministic physics, VSync disabled, seed 12345, alternating selector order,
seven fixed/adaptive pairs, 60 warmup and 480 measured frames per process.
The four load phases were 55%, 75%, 100%, 65%, each 60 frames, repeated twice
within the measured window (120 measured frames per phase/run).

The archive has 14 completed captures (541 profile rows each), no
`failures.txt`, seven recorded equivalent pairs, 3,780 matched frame positions
including warmup, and 6,720 measured observations across both modes. The
strict comparisons cover fixed steps, contact signatures/counts, narrowphase
work, eligible world commands, SDL world/background/UI submissions and texture
state updates. They are **not** pixel-by-pixel image comparisons. The archive
also has 112 phase-tail summary records, 336 within-phase p95+ frame records,
and 112 within-phase p99+ frame identifications.

Run-level p50/p95/p99 use the 480 rows `[60:540]` per capture; the trailing
row 540 is excluded. Percentiles linearly interpolate the sorted values at
`(n-1)*q`. The gain is `(fixed-adaptive)/fixed*100`, so **positive favors
adaptive**. We summarize the median of the **seven paired percentage gains**,
not the percentage difference of two independently summarized medians.

## Paired frame times (whole run)

| Metric | Fixed run-median (ms) | Adaptive run-median (ms) | Median paired gain | Adaptive wins |
| --- | ---: | ---: | ---: | ---: |
| p50 | 0.85800 | 0.86545 | -0.56% | 2/7 |
| p95 | 1.24643 | 1.26597 | +0.07% | 4/7 |
| p99 | 1.45003 | 1.41203 | -0.41% | 3/7 |

| Repetition | p50 adaptive gain | p95 adaptive gain | p99 adaptive gain |
| --- | ---: | ---: | ---: |
| 1 | -0.45% | +0.07% | +2.62% |
| 2 | -2.64% | +1.42% | +6.64% |
| 3 | -0.56% | +3.24% | -0.41% |
| 4 | +1.99% | -0.36% | -3.14% |
| 5 | -0.87% | +2.32% | +10.00% |
| 6 | +0.67% | -5.08% | -10.74% |
| 7 | -4.68% | -5.85% | -7.10% |

Seven process pairs with alternating order show no robust p95/p99 win.
Avoid claims of an adaptive speedup or a single-regression culprit from
these low-single-digit, direction-changing comparisons.

## Independent within-phase p95/p99

Each cell is the median of seven paired percentage gains, calculated from
that *phase's own 120-frame percentile* on each run. This is not a global
whole-run tail. Counts are adaptive wins out of seven.

| Load | p50 gain (wins) | p95 gain (wins) | p99 gain (wins) |
| --- | ---: | ---: | ---: |
| 55% | -3.49% (2/7) | -3.78% (1/7) | -0.76% (3/7) |
| 75% | +0.43% (5/7) | -6.71% (2/7) | -2.64% (2/7) |
| 100% | -0.48% (3/7) | +0.79% (4/7) | +1.44% (4/7) |
| 65% | +0.28% (4/7) | -0.49% (3/7) | -1.24% (2/7) |

The 75% phase is the most useful *hypothesis-generating* comparison: fixed
executes workers for 118/120 frames and adaptive executes none, while phase
p50 changes little and adaptive loses phase p95 in five of seven pairs.
The 100% phase uses nearly the same route and has nearly identical tails.
Do not infer a winner from whole-run medians that mix distinct load states.

## Selection versus actual work

Every fixed run **requests** recording for 240/480 measured frames, but
**executes** workers for only 238/480: two phase-downshift fallbacks have
`render_path_reason=3`, `render_parallel_fallback=1` and no render worker.
Every adaptive run requests/executes exactly 120/480, with zero fallbacks.
Each mode records four actual route transitions per run. The worker pool has
three dedicated threads available, but this does **not** mean all three
always do work; use `render_parallel_executed` and `render_workers` for actual
dispatch. The automatic visibility scorer identifies around 31% of examined
culling sprites as certainly offscreen and skips 4,560 checks per adaptive
run (38 near the end of each full-load frame). This matches the conservative
threshold-saturation logic; it is not evidence of a missing optimization.

Across all seven repetitions (840 measured observations per mode/phase),
the **pooled stage medians** are:

| Phase | Mode | Frame (ms) | World/record (ms) | Prepare (ms) | Submit (ms) | Full `End()` (ms) | Executed worker frames |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 55% | Fixed | 0.6895 | 0.2252 / 0 | 0 | 0.2260 | 0.3041 | 0 |
| 55% | Adaptive | 0.7072 | 0.2246 / 0 | 0 | 0.2257 | 0.3125 | 0 |
| 75% | Fixed | 0.9127 | 0.1669 / 0.1702 | 0.0595 | 0.0875 | 0.3809 | 826 |
| 75% | Adaptive | 0.9136 | 0.2919 / 0 | 0 | 0.2930 | 0.3936 | 0 |
| 100% | Fixed | 1.1547 | 0.2068 / 0.2117 | 0.0740 | 0.1160 | 0.4799 | 840 |
| 100% | Adaptive | 1.1669 | 0.2195 / 0.2240 | 0.0680 | 0.1130 | 0.4734 | 826 |
| 65% | Fixed | 0.7973 | 0.2588 / 0 | 0 | 0.2596 | 0.3509 | 0 |
| 65% | Adaptive | 0.8051 | 0.2546 / 0 | 0 | 0.2556 | 0.3547 | 14 |

The 75% phase explains a *real execution-path difference*: fixed's world
recording and CPU preparation reduce main-thread SDL submission, while
adaptive stays immediate. Yet total frame times do not improve enough or
consistently enough to justify changing the visibility weights or threshold.
These stage medians are **nested and non-additive**: `render_world_ms` lives
inside `render_record_ms` on recorded frames and inside `render_submit_ms`
on immediate frames; `render_worker_wait_ms` lives inside preparation. Also,
separate median values need not add up to a whole-frame median.

## Slowest frames, consecutive spikes and attribution limits

Within each phase/run, six observations are at/above its p95 and two are
at/above its p99. The full render stage is the largest measured top-level
stage in **all 336 phase-p95+ observations**. The historical metric
`render_only_present_ms` is actually the time for **all of
`RenderManager::End()`**, including SDL presentation, temporary texture/surface
destruction and text-cache eviction. It is the largest of the two coarse
render segments for 160/168 fixed and 154/168 adaptive phase-p95+
observations. It accounts for roughly 59–67% of the render stage at the
median of each mode/phase's selected tail frames. This **does not identify
the SDL call**: the original binary has no exact `render_sdl_present_ms`.

| Mode / repetition / frame | Frame (ms) | Full `End()` (ms) | Other salient nested evidence |
| --- | ---: | ---: | --- |
| Adaptive r3, frame 71, 75% | 44.4563 | 43.8495 | World 0.3089; workers inactive; frame 72 also 2.0160 ms |
| Fixed r5, frame 65, 75% | 7.4994 | 6.9533 | World 0.1612; prepare 0.0864; workers active |
| Adaptive r6, frame 128, 100% | 4.2655 | 3.5846 | World 0.2346; prepare 0.0252; workers active |
| Fixed r3, frame 80, 75% | 3.3917 | 2.6713 | Prepare 0.2625, including 0.2438 worker wait |
| Adaptive r3, frame 75, 75% | 2.2932 | 0.4942 | World 1.5833 on immediate route: **separate CPU-side spike** |

There are also concentrated bursts: 30 consecutive within-phase p95+ groups
for fixed (maximum group length four), and 26 for adaptive (maximum five).
The first ten frames of each 60-frame load segment account for 74/168
fixed and 68/168 adaptive within-phase p95+ observations (44.0% and 40.5%,
versus 16.7% of the temporal window); around phase transitions, loads and
cache/scheduling state change together. For example, the initial 100% frame
120 is consistently slower than the corresponding second-cycle frame 360
in the same mode. This is evidence of a phase-transition-associated tail
pattern, **not proof of a texture-cache or GPU cause**.

The two modes run as **separate processes**; frame index 71 in adaptive and
frame index 71 in fixed are not a synchronized real-time event. `SDL_RenderPresent`
may block without VSync because of backpressure or scheduling, but the old
`End()` timer cannot distinguish that from CPU cleanup. OS contention, GPU
queue waits and SDL driver effects remain unverified hypotheses.

## Why the exact SDL column is absent

- All 14 archived profile headers lack `render_sdl_present_ms`.
- Current `tools/render_selector_tails.py` already supports that **optional**
  newer column, and correctly left all `exact_sdl_present_available` flags at 0.
- The provided XEngine source's `RenderManager::End()` called SDL presentation
  without its own stopwatch; `Engine::FrameProfile` and `WriteProfiles_()` had
  no exact-present field. Consequently the source itself did not implement
  the instrumented capture path, despite the latest historical context
  describing the intended capability.
- `build.log` reports both the engine archive and game binary **unchanged**;
  this is consistent with running that older source, but the log alone cannot
  prove a binary/source timestamp mismatch. The build tool hashes engine
  source and headers and tracks game link inputs, so the new C++ changes
  should invalidate the cache on the next genuine `--build`.
- The sweep previously set `--raw-log` and its live-console tee to the **same**
  file, a concurrent-writer collision visible as a mangled duplicate final
  line in this archive. The patch now separates `build.log` (complete raw
  compiler transcript) from `build_console.log` (live concise progress).

## Implemented correction; no algorithmic optimization

`RenderManager::End()` now conditionally times only the SDL-present delegate
using `steady_clock`, excluding later cleanup and leaving the former inclusive
`render_only_present_ms` intact for comparisons. `Engine` exports a new
`render_sdl_present_ms` CSV field. The tail analyzer remains backward
compatible with old profiles; the *fresh* fixed/adaptive sweep validates the
new field and stops after its first capture if it is absent or malformed.
Tests verify source wiring, the backward-compatibility split, the stale-binary
guard and the two independent compiler logs. The following differences become
available on new profiles:

- `render_cpu_pre_end_ms` = full render stage minus complete `End()`;
  it is a coarse **pre-End duration**, not an exact CPU-only metric.
- `render_sdl_present_ms` = stopwatch around the actual SDL presentation call.
- `render_end_cleanup_ms` = complete `End()` minus measured SDL present;
  approximate cleanup/measurement overhead, never a GPU-specific timer.

**No weights, route-selection policy, worker dispatch, SDL draw order,
physics, JSON defaults or global 1536 threshold were changed.**

## Validation and next controlled experiment

Local Python regression tests for exact-present wiring and log separation
pass, as do the existing source-based and offline diagnostic checks. A native
Windows/MSYS2 Release build, SDL2 runtime visual comparison and precise
`SDL_RenderPresent()` measurements are **not available in this environment**.

After installing the patch, run in a fresh results directory from the
repository root (PowerShell; the `--build` is essential):

```powershell
python tools/sweep_render_selector.py --build --build-config Release --workloads culling --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 7
```

Check that `build_console.log` shows engine compilation and game linking,
`build.log` preserves unfiltered command/diagnostic text, and **every**
`culling/{fixed,adaptive}_run*/render_parallel_run01.csv` includes a numeric
`render_sdl_present_ms`. There must be `pairs=7`, `errors=0`, per-frame
equivalence, and consistent visual rendering. Interpret the new
`phase_tail_frames.csv`/`phase_tail_summary.csv` split by route, phase,
present and cleanup. If the exact SDL call remains responsible for spikes,
next capture actual SDL renderer backend, monitor GPU/OS scheduling and
repeat with a controlled VSync comparison; if CPU preparation or world
submission dominates, run the already-implemented **forced-route**
`sweep_render_selector_cost.py` ablation with the same culling workload.
Do not adjust adaptive heuristics on these old unsplit tails.

---

## 2026-10-22 — Follow-up with the corrected v16.15 Windows Release executable

This section supersedes the earlier **unresolved SDL-vs-cleanup attribution** for
new data, while retaining the original 11:22 investigation above as historical
context. An intermediate user run at 14:04 had been generated before the patch
was applied, explaining both the missing CSV field and the corrupt combined
build log. The corrected archive is
`render_selector_20261022_142128_292465.zip`. Its `build_console.log` confirms
recompilation of `Engine.cpp`, `RenderManager.cpp`, the engine library and the
benchmark executable; raw `build.log` is separate and complete. The ZIP CRC
check passes. All 14 profiles contain valid, nonzero, nested
`render_sdl_present_ms`; there are seven matched pairs, no failed runs,
6,720 measured frames, 112 per-phase summaries and 336 phase-p95+ frames.

### Full-run fixed/adaptive comparisons (Release culling, milliseconds)

The p50/p95/p99 columns are **medians of each mode's seven per-run
percentiles**; pair-win counts compare the seven runs individually. These
medians are not pooled quantiles or medians of pairwise percentage changes.

| Whole frame | Fixed | Adaptive | Adaptive better pairs |
|---|---:|---:|---:|
| p50 | 0.9006 | 0.8976 | 4/7 |
| p95 | 1.3057 | 1.3130 | 2/7 |
| p99 | 1.5358 | 1.4822 | 4/7 |

Median paired adaptive gains are +1.25% p50, -0.71% p95 and +0.92% p99.
Individual runs reverse direction; this is not persuasive evidence to change
the default `fixed` route or the experimental threshold of 1536.

| Load phase | Fixed p95 | Adaptive p95 | Fixed p99 | Adaptive p99 |
|---|---:|---:|---:|---:|
| 55% | 0.8963 | 0.8837 | 0.9620 | 0.9577 |
| 75% | 1.1789 | 1.1691 | 1.3417 | 1.3401 |
| 100% | 1.4414 | 1.4143 | 1.6588 | 1.5758 |
| 65% | 1.0420 | 0.9985 | 1.1242 | 1.1045 |

### SDL presentation time versus cleanup

The nested stopwatch directly wraps `WindowManager::Present`, whose body is
only `SDL_RenderPresent`. Comparing pooled per-frame component percentiles
across **3,360 frames per mode** gives:

| Component | Fixed p95 (ms) | Adaptive p95 (ms) |
|---|---:|---:|
| Entire frame | 1.3066 | 1.3080 |
| Entire `RenderManager::End()` | 0.6216 | 0.6247 |
| Exact `SDL_RenderPresent()` | 0.6215 | 0.6245 |
| Post-SDL cleanup | 0.0002 | 0.0002 |
| Render work before `End()` (coarse) | 0.4324 | 0.4451 |
| Worker wait | 0.0595 | 0.0538 |

The cleanup median is 0.0001 ms, p99 0.0003 ms; the largest recorded
cleanup is 0.0689 ms, with only five cleanup measurements above 0.005 ms
among 6,720 frames. **These component percentiles are not additive.** The
dominant measured render segment in the 168 within-phase p95+ frames of each
mode is SDL present in **150 fixed** and **149 adaptive** frames; coarse
pre-`End()` CPU time dominates the remaining 18 and 19. The overall render
stage is the largest top-level stage in 167/168 fixed and 168/168 adaptive
tails (one fixed frame has a larger `fixed_ms` stage).

Two important independent extremes:

| Mode / repetition / frame | Load | Full frame | SDL present | Worker wait |
|---|---:|---:|---:|---:|
| fixed / 7 / 127 | 100% | 9.446 ms | 8.693 ms | 0.051 ms |
| adaptive / 2 / 139 | 100% | 2.223 ms | 1.395 ms | 0.030 ms |

These extremes **localize time to the SDL call**, not to an identified
driver, GPU, compositor, VSync or command-buffer cause. They are separate
benchmark processes, not matched simultaneous events; do not conclude from
their maxima that adaptive is inherently more stable. The SDL call can
perform deferred work and synchronization even when VSync is disabled in
the recorded runtime metadata.

### Workers, phase boundaries and equivalence

- Fixed requests 240 worker-route frames per 480 measured and executes
  238, with two fallbacks; adaptive requests/executes 120 and has zero
  fallbacks. At 75% load, fixed executes 118/120 frames with workers and
  adaptive executes none. At 100%, fixed executes 120/120 versus 118/120;
  at 65%, fixed 0/120 versus adaptive 2/120. Both modes switch selected
  and executed routes four times. Adaptive skips 4,560 conservative
  offscreen visibility checks per run.
- The established nine physics/contact/world-submission invariants matched
  without exception in **7 × 540 = 3,780** paired frame positions (including
  warmup). These checks are **not** pixel-perfect screenshot verification.
- Of 168 per-mode phase-p95+ observations, the first ten frames after a
  phase transition account for **65 fixed** and **66 adaptive**, versus a
  16.7% baseline share of all phase positions. Consecutive tail clusters
  are short, with maximum length three frames in either mode.
- The first 75%-load cycle has a higher p95 than the second in **7/7 fixed**
  and **6/7 adaptive** runs; exact SDL present p95 improves in 6/7 and
  7/7 respectively. At 100%, this holds for adaptive in 7/7, but for fixed
  only 3/7; first-pass slowdown is not fully uniform across modes or runs.
  Cold resources, scene changes, runtime scheduling and presenter queues
  remain competing explanations.

### Decision and reproducibility

No engine code, route heuristics, VSync config, physics, render semantics,
default selector or threshold changed in this follow-up. The existing
measurement answers the immediate question: **the dominant stalls reside
inside `SDL_RenderPresent()`, not in the `End()` cleanup**. The next
controlled A/B should record renderer/backend/driver details and compare
the first and second appearances of 75% and 100% load, with matched run
order, stable display/window conditions and a larger sample before trying
backend or rendering optimizations. Maintain a distinct steady-state and
first-transition analysis rather than silently removing spikes by extending
warmup. Use this command from the v16.15+ project root:

```powershell
python tools/sweep_render_selector.py --build --build-config Release --workloads culling --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 7
```

The current measurements originate from Miguel's native Windows benchmark.
No additional native build was run in the analysis environment. Only the
historical root `AI_CONTEXT.md` and this report are changed in the
documentation-only follow-up; the old methodology and all original
project assets remain intact.

## 2026-10-22 — v16.17: presenter-identity and contiguous-phase experiment

This is a **new instrumentation/experiment patch**, not a claim that the
Windows driver, GPU or OS has been identified as the cause of the stalls.
It does not alter worker selection, SDL drawing, default `fixed`, threshold
1536, frame pacing, VSync settings, or the simulated scene.

### Actual SDL presenter identity

The benchmark now obtains `SDL_GetRendererInfo` **after** engine startup and
prints one machine-readable `[BENCH-PRESENTER]` JSON line, recording:

- Actual renderer name/flags (accelerated, software, VSync, target-texture),
  SDL version, selected video driver and SDL renderer-driver/VSync hints.
- Window and renderer output dimensions, window flags, display index and
  current display refresh rate where SDL can report it.

The Python runner refuses missing/invalid banners and unexpectedly enabled
renderer VSync, saves every presenter observation in per-run `metadata.json`
and extracts stable identifiers into `summary.csv`. The selector sweep refuses
to compare a fixed/adaptive pair if the reported renderer, flags, display,
dimensions or SDL version differ. `run_order.csv` records the **actual launch
order and runner exit status**. The preexisting alternating run order was
already implemented and is retained: odd repetitions start with fixed;
even repetitions start with adaptive. With an even number of repetitions,
each route starts exactly half the pairs.

These fields are an **identity/conditions check**, not GPU queue timings or
a GPU utilization measurement. An SDL renderer flag does not describe the
Windows compositor, driver queuing, CPU preemption or VRR state. The new
presenter handshake cannot retrospectively identify the backend used by
the older archived run; its banner did not exist at capture time.

### Separate first and repeated passes

New reports (from fresh sweeps *or* from the offline tail analyzer):

| CSV | Observations |
|---|---|
| `phase_pass_results.csv` | One contiguous phase visit; p50/p95/p99 of frame and exact SDL present, CPU preparation/record/submit/wait p95, first-ten vs remaining frames, consecutive p95 runs, route request/execution/fallback counts |
| `phase_pass_comparisons.csv` | Matched fixed/adaptive percentiles for the **same** load and pass number (separate processes, not synchronized frames) |
| `phase_pass_transitions.csv` | First vs second complete visit for each load and mode: frame and SDL-present p95/p99 and fraction of tails near entry |
| `run_order.csv` | Sweep launch order and process exit code; complements existing nested runner logs |

Passes are indexed from the *absolute profiler frame*, not from frame zero
of the measurement. A warmup of 60 frames and a phase length of 60 means
measured visit order is 75%, 100%, 65%, 55%, then repeat; the labels do
**not** assume 55% always comes first. Incomplete visits are marked and
excluded from paired comparisons. An older profile lacking
`render_sdl_present_ms` still produces pass-level frame statistics, with
SDL columns left empty; fresh sweeps still require exact-present telemetry.

The 2026-10-22 corrected Release archive was reprocessed with these new
offline functions: **112 full visits, 56 mode pairs, 56 within-mode pass
pairs, zero analysis failures**. The second 75% visit has a lower frame p95
in **7/7 fixed** and **6/7 adaptive** runs (median first-to-second
improvements **13.51%** and **7.41%**, respectively). At 100%, this holds
in **3/7 fixed** and **7/7 adaptive** runs (median **-0.76%** and
**+10.62%**). The SDL-present p95 improvement counts at 75% are 6/7 fixed
and 7/7 adaptive; at 100% they are 3/7 fixed and 7/7 adaptive. These
separate-visit calculations agree with the earlier aggregate findings but
make their load/pass origin explicit. They are descriptive samples, not
evidence of shader recompilation, a specific D3D/OpenGL backend, or a
general adaptive p95 advantage.

### Controlled native Windows Release follow-up

From the **updated v16.17** project, keep one graphics output, resolution,
refresh rate, power plan and SDL driver environment for the entire study.
Close unnecessary GPU-using applications. Start with the matched experiment
using an **even** repetition count (balanced first-run order):

```powershell
python tools/sweep_render_selector.py --build --build-config Release --workloads culling --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 8
```

Then, as a **separate** warm-start sensitivity experiment on the same machine
and settings, optionally run with a full four-phase warmup (240 frames):

```powershell
python tools/sweep_render_selector.py --build-config Release --workloads culling --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 240 --repetitions 8
```

The second command is **not** a synchronized A/B comparison against the
first: its startup/warmup history is different. Treat them as two
independent samples of initial-transition versus warmed conditions. In both,
inspect `run_order.csv`, each nested `metadata.json` presenter observation,
`phase_pass_results.csv`, `phase_pass_transitions.csv`, `phase_tail_frames.csv`
and `failures.txt` (which should be absent). The exact first-ten effect can
vary; changing the warmup without preserving this distinction would hide it.

Reprocess any already-captured compatible archive without a new SDL run:

```powershell
python tools/render_selector_tails.py --input "C:\\ruta\\render_selector_20261022_142128_292465.zip" --output "C:\\ruta\\analisis_fases"
```

**Current validation boundary:** Python source-level/CSV/offline tests pass;
the updated SDL C++ probe has **not** been compiled against the maintainer's
native Windows/MSYS2 SDL SDK in this environment. The first new native run
must verify the banner and compare the actual backend between both modes.

---

## v16.18 follow-up: native Windows Release with SDL backend identification

**New independent capture:** `render_selector_20261022_151057_556139.zip`,
recorded by the maintainer after applying v16.17. Do **not** pool its frame
samples with those of the earlier seven-repetition archive: they are different
Windows sessions. This archive passes ZIP CRC inspection, has no
`failures.txt`, contains eight equivalent fixed/adaptive pairs (16 complete
runs), all 16 exact `render_sdl_present_ms` profile columns, 128 full
`phase_pass_results.csv` records, 64 cross-mode phase-visit comparisons and
64 first/second-visit transitions. The independent offline analyzer reproduces
the exported pass reports exactly. All 4,320 paired frame positions, including
warmup, pass the nine recorded physics/contact/render-submission equivalence
checks. These counters do **not** prove pixel-for-pixel visual equivalence.

The live `[BENCH-PRESENTER]` observations are identical in all 16 runs:

| Reported condition | Actual value |
|---|---|
| SDL renderer | `direct3d` (SDL2's Direct3D renderer, **not** `direct3d11`) |
| SDL video driver/version | `windows` / `2.32.10` |
| Renderer flags | `10`: accelerated, target-texture; renderer VSync **off** |
| Output and window | 1280 × 720 pixels |
| Display refresh | 144 Hz |
| Run order | Odd pair fixed first, even pair adaptive first: 4/4 balanced |
| Workload / threshold | `culling` / 2048 sprites maximum / provisional 1536 |
| Frames / phase sequence | 60 warmup + 480 measured; 55/75/100/65%, 60 frames each, twice |

The Windows build transcript confirms recompilation of the relevant C++
translation units and a fresh Release executable link. Therefore the **former
stale executable / absent SDL-present column discrepancy is resolved** in this
capture; no additional instrumented producer is missing. The launcher exited
successfully on all 16 processes, and all measured profiles include the exact
SDL timing. The backend identity verifies SDL's own renderer selection only;
it does **not** expose GPU command completion, DWM/compositor, VRR, driver
queues or operating-system thread scheduling.

### Eight matched whole-run comparisons

Positive `adaptive gain` means adaptive is faster. This table uses the
**median of the eight per-pair percentage gains** (not a ratio of pooled
percentiles), and each percentile is calculated separately for the 480
measured frames of each process.

| Whole-frame metric | Median paired adaptive gain | Adaptive wins |
|---|---:|---:|
| p50 | **+0.99%** | 6/8 |
| p95 | **−1.39%** | 2/8 |
| p99 | **−6.07%** | 1/8 |

| Pair | First process | Adaptive p50 gain | Adaptive p95 gain | Adaptive p99 gain |
|---:|---|---:|---:|---:|
| 1 | fixed | +3.80% | +1.73% | −1.73% |
| 2 | adaptive | +0.29% | −0.30% | −5.37% |
| 3 | fixed | +2.31% | −0.55% | −0.79% |
| 4 | adaptive | +1.56% | +0.01% | +0.66% |
| 5 | fixed | −2.40% | −17.14% | −18.48% |
| 6 | adaptive | −1.53% | −15.43% | −6.78% |
| 7 | fixed | +0.42% | −10.25% | −14.20% |
| 8 | adaptive | +2.51% | −2.23% | −17.19% |

The last four pairs are less favorable to adaptive despite balanced launch
order. This is a **repeatable observation within the session**, not a proven
change in selector cost or a reliable cross-session trend. A few extreme
single-frame stalls strongly affect p99; do not draw driver or GPU conclusions
from the maximum alone.

### Separating exact SDL presentation from the rest of the frame

The following percentiles **pool 960 measured frames per mode and load**
(eight runs × 120 frames). They describe distributions at each load; they are
*not* paired effect sizes. `Outside SDL` is simply
`frame_ms - render_sdl_present_ms`: it contains multiple CPU stages,
scheduling and other possible waits. It is **not** a direct CPU utilization
measurement. Nested `render_world_ms`, record, prepare and submit counters
must not be added to inclusive rendering counters.

| Load | Route | Frame p95 / p99 (ms) | Exact SDL p95 / p99 (ms) | Outside SDL p95 / p99 (ms) |
|---|---|---:|---:|---:|
| 55% | fixed | 0.817 / 0.933 | 0.392 / 0.504 | 0.438 / 0.559 |
| 55% | adaptive | 0.817 / 0.930 | 0.408 / 0.525 | 0.433 / 0.538 |
| 75% | fixed | 1.149 / 1.382 | 0.600 / 0.772 | 0.592 / 0.699 |
| 75% | adaptive | 1.136 / 1.334 | 0.593 / 0.756 | 0.575 / 0.687 |
| 100% | fixed | 1.359 / 1.508 | 0.693 / 0.796 | 0.747 / 0.806 |
| 100% | adaptive | 1.529 / 1.745 | 0.739 / 0.901 | 0.844 / 0.986 |
| 65% | fixed | 0.931 / 1.042 | 0.448 / 0.559 | 0.514 / 0.600 |
| 65% | adaptive | 0.973 / 1.118 | 0.468 / 0.593 | 0.521 / 0.633 |

At 100% load, **adaptive has worse p95 outside SDL in seven of eight
matched pairs**, with a median paired relative difference of **−10.35%**.
It remains worse in 7/8 pairs after discarding only the first two frames of
*each* full-load visit (median **−8.83%**). This exclusion is a **diagnostic
sensitivity test**, never a change to the official benchmark metric. Thus
the first momentarily immediate adaptive frame at each 100%-phase entry is
not by itself sufficient to explain the wider slowdown. An additional
CPU-side/scheduling contribution is plausible; exact ownership among scene
work, worker execution, submission and system waits remains unproven. At 75%
the direction reverses: adaptive is faster at outside-SDL p95 in 7/8 pairs
(median paired gain **+3.83%**), while `fixed` actually uses the workers.

Across 960 measured frames per mode at each load, actual worker execution:

| Load | Fixed worker frames | Adaptive worker frames |
|---|---:|---:|
| 55% | 0 | 0 |
| 75% | 944 | 0 |
| 100% | 960 | 944 |
| 65% | 0 | 16 |

In particular, `fixed` requests the worker route at 75% but falls back on
two transition frames per repetition; adaptive does not request it there.
At 100%, both modes use worker execution in almost every frame; adaptive's
16 missed worker frames are the entries to the two high-load visits per run.
`render_selector_decision_ms` remains tiny and does **not** time the full
visibility scorer, so it cannot exclude scoring overhead by itself.

### Phase transitions and isolated tails

The first 10 frames of each phase visit represent just 16.7% of positions,
yet account for **90/192 fixed** and **90/193 adaptive** within-phase p95+
observations (ties count). For the 75%-load phase, the second visit improves
whole-frame p95 over the first in **8/8 fixed and 8/8 adaptive** runs,
median **+18.65%/+14.12%**. Exact SDL-present p95 improves in **8/8 for
both**, median **+29.38%/+25.82%**. For 100%, second-visit whole-frame p95
improves in **7/8 fixed and 7/8 adaptive**, median **+9.49%/+13.83%**.
The pattern is load-dependent; 55%-phase p95 often becomes slower on its
second visit. Transition-correlated SDL delays therefore affect **both**
strategies but do not establish shader compilation, texture uploads or a
specific driver mechanism.

Examples (not meaningful standalone performance rankings): fixed pair 8
frame 61 takes **18.589 ms total / 17.849 ms exact SDL**; fixed pair 4
frame 70 takes **12.051 / 11.570 ms**; adaptive pair 1 frame 120 takes
**8.007 / 7.215 ms**. These are distinct independent processes. Further
adaptive full-load tails contain measurable time outside SDL, so focusing
exclusively on the largest present stall would miss another contributor.

### Decision and next experiment

**No engine code, heuristic, default or threshold is modified.** There is
not yet a proven optimization that would consistently improve p95/p99
without sacrificing behavior. Retain `parallelSelection="fixed"` and the
provisional threshold 1536. The next experiment should isolate backend
effects while keeping the build, display and workload identical. SDL2 can
prefer a renderer through the `SDL_RENDER_DRIVER` hint, but may fall back;
**accept a capture for backend comparison only if its actual reported
`renderer_name` matches the requested value in every run**. A/B/A sequencing
offers a qualitative guard against global drift, not true simultaneous pairs.

PowerShell, from the same v16.17-or-newer project checkout on the Windows
machine, preferably with the same monitor, power plan, refresh, VRR setting
and no other active GPU-intensive programs:

```powershell
# One Release build before any capture; avoid rebuilding between backend arms.
$previousDriverHint = [Environment]::GetEnvironmentVariable('SDL_RENDER_DRIVER', 'Process')
try {
    $env:SDL_RENDER_DRIVER = 'direct3d'
    python tools/sweep_render_selector.py --build --build-config Release --workloads culling --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 8
    if ($LASTEXITCODE -ne 0) { throw 'direct3d A capture failed' }

    $env:SDL_RENDER_DRIVER = 'direct3d11'
    python tools/sweep_render_selector.py --build-config Release --workloads culling --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 8
    if ($LASTEXITCODE -ne 0) { throw 'direct3d11 B capture failed' }

    $env:SDL_RENDER_DRIVER = 'direct3d'
    python tools/sweep_render_selector.py --build-config Release --workloads culling --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 8
    if ($LASTEXITCODE -ne 0) { throw 'direct3d A2 capture failed' }
}
finally {
    if ($null -eq $previousDriverHint) {
        Remove-Item Env:SDL_RENDER_DRIVER -ErrorAction SilentlyContinue
    } else {
        $env:SDL_RENDER_DRIVER = $previousDriverHint
    }
}
```

The first arm checks the newly observed `direct3d` path while rebuilding
once; the second explicitly requests the alternate backend; the third checks
whether results drift over time. All three sweeps still alternate and
independently validate fixed/adaptive runs. Compare **within** each backend's
eight pairs and their `phase_pass_results.csv` and `phase_pass_transitions.csv`
before looking at between-backend differences. Record GPU model, Windows
build, graphics driver version and compositor/VRR/display settings manually;
SDL metadata alone does not supply them. If `direct3d11` falls back to
`direct3d`, the alternate-backend test was **not executed**. Never claim GPU
causality or change defaults based on that failed intervention.

**v16.18 validation scope:** archive CRC; eight-sweep match and presenter
identity; exact SDL profiling and build/link logs; independent old/new
phase-pass CSV reanalysis; per-frame invariants; 100%-load first-frame
sensitivity test; unchanged production code. No native SDL/MinGW compilation
is newly run in the analysis environment—native Windows build evidence is
supplied in the user's capture. No statistically controlled Windows A/B/A
backend benchmark has yet been run.

---

## v16.19 — Resultado del ensayo A/B/A real: `direct3d` → `direct3d11` → `direct3d` (22-10-2026)

### Origen, protocolo y validación

Fuente: `backend_aba_20261022_154758.zip`, capturado en Windows Release por
Miguel. El fichero contiene los tres brazos completos **A = direct3d**, **B =
direct3d11**, **A2 = direct3d**, con **8 parejas fixed/adaptive por brazo** (48
procesos), 480 fotogramas medidos por proceso, 60 de calentamiento y cargas
55/75/100/65%, con visitas repetidas a las fases. Todos los procesos terminaron
con código cero; no hay `failures.txt`; el CRC de los 230 miembros del ZIP es
correcto. Cada perfil tiene 78 columnas, incluida `render_sdl_present_ms`.
La primera ejecución del brazo A reconstruye el ejecutable; B y A2 lo
reutilizan sin otra compilación. Por tanto, se comparan **los mismos binarios
del motor con diferente backend SDL solicitado y confirmado**, no dos versiones
de XEngine.

En los 48 `metadata.json` se comprobó el backend realmente creado:
`renderer_name=direct3d` para A/A2 y `renderer_name=direct3d11` para B,
sin fallback. También coinciden SDL 2.32.10, controlador de vídeo `windows`,
flags SDL=10 (acelerado y texturas de destino, sin VSync de renderer), resolución
1280×720 y frecuencia declarada 144 Hz. Se alternó el orden de inicio
fixed/adaptive dentro de cada brazo; el protocolo A/B/A recupera el rendimiento
anterior al volver al primer backend, lo que reduce la plausibilidad de una
deriva temporal monótona. No se capturaron temperatura, GPU/driver,
compositor, VRR ni trazas del controlador.

### Percentiles de duración del fotograma

Los siguientes números son la **mediana de ocho percentiles independientes
por ejecución**, en milisegundos. Nunca confundirlos con el percentil de los
3.840 fotogramas agrupados.

| Ruta | Métrica | A `direct3d` | B `direct3d11` | A2 `direct3d` | Mejora B vs A | Mejora B vs A2 |
|:--|:--|--:|--:|--:|--:|--:|
| fixed | p50 | 1.4798 | 0.6895 | 1.4743 | 53.40% | 53.23% |
| fixed | p95 | 2.0769 | 1.0209 | 2.0900 | 50.84% | 51.15% |
| fixed | p99 | 2.2138 | 1.1281 | 2.2062 | 49.04% | 48.86% |
| adaptive | p50 | 1.4166 | 0.6874 | 1.4316 | 51.47% | 51.99% |
| adaptive | p95 | 2.1373 | 1.0399 | 2.1067 | 51.35% | 50.64% |
| adaptive | p99 | 2.2357 | 1.1165 | 2.2350 | 50.06% | 50.04% |

Para **ambas rutas**, B vence en p50, p95 y p99 a A y a A2 en **las ocho
repeticiones respectivas**. Esto no implica 48 mediciones simultáneas
emparejadas temporalmente: las ejecuciones y brazos son secuenciales.

Percentiles p95 de `frame_ms` agrupados por **fase y ruta** (960 observaciones
por celda de backend/ruta/fase; milisegundos):

| Carga | fixed A | fixed B | fixed A2 | adaptive A | adaptive B | adaptive A2 |
|--:|--:|--:|--:|--:|--:|--:|
| 55% | 1.3569 | 0.6636 | 1.3176 | 1.3251 | 0.6545 | 1.3175 |
| 65% | 1.5216 | 0.7379 | 1.5046 | 1.5222 | 0.7680 | 1.5033 |
| 75% | 1.7979 | 0.8365 | 1.7970 | 1.7418 | 0.8451 | 1.7412 |
| 100% | 2.2281 | 1.1013 | 2.2428 | 2.2957 | 1.1187 | 2.3109 |

La diferencia no depende exclusivamente del tramo de 100% de carga: existe
en **todas** las fases. Durante la fase del 75%, la ruta fija ejecuta
trabajadores en 944/960 fotogramas, mientras adaptive no los ejecuta;
al 100% ejecutan 960/960 y 944/960 respectivamente. A 55% ninguna
ejecuta trabajadores, y a 65% fixed 0/960 frente a adaptive 16/960.
Estos conteos son **idénticos en A, B y A2**. La sustitución del backend
no alteró las decisiones del selector.

### Descomposición de los tiempos (CPU y SDL)

Aquí se usan los **percentiles de las 3.840 muestras medidas agrupadas por
brazo y ruta**. Los cronómetros anidados **no deben sumarse** y sus
percentiles no son aditivos.

| Ruta | Brazo | `frame_ms` p50/p95/p99 | `render_submit_ms` p50/p95/p99 | `render_sdl_present_ms` p50/p95/p99 |
|:--|:--|:--|:--|:--|
| fixed | A | 1.4789 / 2.0914 / 2.2504 | 0.9698 / 1.3182 / 1.3947 | 0.1547 / 0.2596 / 0.3662 |
| fixed | B | 0.6898 / 1.0244 / 1.1231 | 0.3784 / 0.4815 / 0.5351 | 0.0207 / 0.0910 / 0.1390 |
| fixed | A2 | 1.4857 / 2.0914 / 2.2760 | 0.9766 / 1.3242 / 1.4372 | 0.1592 / 0.2562 / 0.3462 |
| adaptive | A | 1.4187 / 2.1323 / 2.3183 | 1.0856 / 1.3340 / 1.4804 | 0.1512 / 0.2522 / 0.3363 |
| adaptive | B | 0.6858 / 1.0404 / 1.1568 | 0.4198 / 0.5342 / 0.6286 | 0.0187 / 0.0818 / 0.1168 |
| adaptive | A2 | 1.4306 / 2.1394 / 2.3398 | 1.0886 / 1.3490 / 1.5029 | 0.1596 / 0.2578 / 0.3517 |

El punto más relevante es el coste del **envío de sprites**: para `fixed`,
el p50 agrupado de `render_sprite_submit_ms` cae desde **0.85745 ms**
(A) a **0.24130 ms** (B), y vuelve a **0.85365 ms** (A2).
El `render_submit_ms` p50 también cae de aproximadamente 0.970 a
0.378 ms, mientras el `render_sdl_present_ms` p50 pasa de aproximadamente
0.155 a 0.021 ms. Así, atribuir el ahorro entero a
`SDL_RenderPresent()` sería erróneo: **la reducción dominante medida
ocurre durante las llamadas de envío/dibujo de sprites en SDL**, que
pueden incluir costes internos de la implementación del backend.
No se dispone de trazas para separar CPU en el controlador, esperas
sincronizadas, GPU y cola del compositor.

Como comprobación adicional, para la ruta fija el p95 de
`frame_ms - render_sdl_present_ms`, calculado **fila a fila**, es
**1.8859/0.9447/1.8872 ms** en A/B/A2, respectivamente. B también
mejora mucho **fuera** de la llamada exacta a `SDL_RenderPresent()`.
`fixed_ms` p50 es ~0.093/0.092 ms y `update_ms` ~0.095/0.096 ms
en A/B, lo que concuerda con un cuello de botella localizado en el
renderizador. Aun así, “fuera de SDL present” no equivale a tiempo
de CPU activo, ni demuestra por sí solo que toda la diferencia sea del
driver gráfico.

### Cola extrema, repetición de fases y selector

- La mejora no elimina todos los outliers. En A fixed, repetición 1,
  fotograma 135: **7.085 ms** total, **6.005 ms** en `render_submit_ms`
  y solo 0.383 ms en `SDL_RenderPresent()`. En B fixed, repetición 1,
  fotograma 118: **4.582/3.815/0.330 ms**. En A adaptive, repetición 7,
  fotograma 72 hay un pico distinto de ~**1.872 ms** dentro del
  `SDL_RenderPresent()` medido (3.424 ms de fotograma).
- Fotogramas medidos con `frame_ms > 2 ms`, por brazo y ruta sobre
  3.840 observaciones: **A 451 fixed/540 adaptive; B 2 fixed/0 adaptive;
  A2 529 fixed/648 adaptive**. Es una comparación de umbral absoluto,
  **no** el porcentaje p99 ni una garantía de ausencia de stutter.
- En B a carga 75%, la segunda visita muestra un p95 de fotograma menor
  que la primera para fixed en **7/8** repeticiones, con una mediana de
  mejora de ~**8.9%**; para adaptive **5/8** y ~**1.4%**. También cambia
  `SDL_RenderPresent()` entre visitas, pero el efecto en milisegundos
  absolutos es pequeño. No atribuir estas variaciones a la temperatura,
  calentamiento de caché o planificación sin otros experimentos.
- Las ganancias adaptive respecto a fixed **dentro de cada backend**
  siguen siendo inestables en las colas. Mediana de ganancia por pareja
  (positivo = adaptive mejor): A **p50 +4.01%, p95 −2.83%, p99 −2.09%**;
  B **+0.10%, −1.71%, +0.62%**; A2 **+3.69%, −2.21%, −3.69%**.
  No hay evidencia para cambiar el valor predeterminado
  `parallelSelection: "fixed"` ni el umbral experimental **1536**.

### Equivalencia y decisión

Los pares fixed/adaptive pasan las invariantes de equivalencia del barrido.
Adicionalmente se contrastaron A↔B, B↔A2 y A↔A2 para cada ruta,
repetición y posición (8 × 2 × 540 = **8.640 posiciones por contraste**),
con **cero discordancias** en las nueve columnas comprobadas:
`fixed_steps`, `collision_contact_signature`, `contacts_built`,
`narrowphase_tests`, `render_world_draws`,
`render_eligible_current`, `render_background_draws`,
`render_ui_draws`, `render_tint_updates`.
Esto prueba equivalencia de trabajo/contadores en este escenario,
**no** igualdad de píxeles ni compatibilidad universal de los dos drivers.

**Decisión v16.19:** resultados favorables y repetibles del backend
`direct3d11` en este equipo Windows/carga `culling`; documentar el
opt-in mediante `SDL_RENDER_DRIVER=direct3d11`, **sin modificar**
`RenderManager::Init`, heurísticas, rutas, VSync, defaults ni código
de producción. Para considerar cambiar el backend preferente se requieren
comparaciones visuales de capturas, múltiples workloads
(`baseline`, `mixed`, `rotations_xyz`), condiciones de pantalla
controladas y, preferiblemente, otros equipos/GPU. Si el backend real
fuera distinto del solicitado, desechar ese brazo. La comparación A/B/A
identifica una diferencia ligada a la elección de backend en este entorno,
pero no una causa interna a GPU/driver concreta.

Reproducción aconsejada en PowerShell **desde la raíz del motor**, dejando
intacto el valor predeterminado de la configuración:

```powershell
$previousDriverHint = [Environment]::GetEnvironmentVariable('SDL_RENDER_DRIVER', 'Process')
try {
    $env:SDL_RENDER_DRIVER = 'direct3d11'
    python tools/sweep_render_selector.py --build --build-config Release --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 8
    if ($LASTEXITCODE -ne 0) { throw 'El sweep con direct3d11 ha fallado' }
}
finally {
    if ($null -eq $previousDriverHint) {
        Remove-Item Env:SDL_RENDER_DRIVER -ErrorAction SilentlyContinue
    } else {
        $env:SDL_RENDER_DRIVER = $previousDriverHint
    }
}
```

Para un nuevo A/B/A completo, reutilizar el bloque protegido de v16.18
cambiando `--workloads culling` por
`--workloads baseline culling mixed rotations_xyz`. Verificar siempre
`metadata.json` (renderer real), salidas por fase, equivalencia y
capturas visuales equivalentes. La instrumentación actual mide tiempos
SDL en la CPU, no ejecución GPU por comandos.

**Verificación v16.19:** inspección completa del ZIP A/B/A, metadatos,
encabezados, resultados, percentiles y posiciones comparadas, más pruebas
regresivas de instrumentación. Solo cambian este documento histórico y
`AI_CONTEXT.md`; no hay nueva compilación del motor ni promesa de
aceleración añadida por el parche (el beneficio se obtuvo al seleccionar
un backend que SDL2 ya proporciona).

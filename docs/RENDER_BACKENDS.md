# SDL2 render backends on Windows and Linux

XEngine uses the SDL2 `SDL_Renderer` interface. The same game logic works
with multiple graphics backends; the backend is **not** the
`Renderer.parallelSelection` choice (`fixed` or `adaptive`). Switching SDL
backends does not switch the engine's physics, draw ordering or thread selection.

## Choosing a backend in the runtime config

Edit the **runtime** `assets/config/engine.json` of your game (the template is
`tools/templates/BlankGame/assets/config/engine.json`):

```json
"Renderer": {
  "vsync": true,
  "hardwareAcceleration": true,
  "backend": {
    "windows": "auto",
    "linux": "auto"
  },
  "backendFallback": true,
  "parallelPreparation": true,
  "parallelMinCommands": 1536,
  "parallelSelection": "fixed"
}
```

This is a **fragment**, not a standalone JSON file. `Renderer.backend` holds
platform-specific preferences and `Renderer.backendFallback` (default `true`)
controls recovery from a failed explicit config choice. Missing OS entries
are `auto`. Invalid types or unknown OS keys fail JSON validation.

- Windows `auto`: **uses an installed, validated local benchmark order first**.
  When no comparable measurement is installed, it tries **`direct3d11` →
  `direct3d` (D3D9) → `direct3d12` → `opengles2` → `opengl` →
  `software` → SDL default**. Only installed SDL drivers are attempted.
- Linux `auto`: **uses an installed, validated local benchmark order first**,
  then SDL's default. Without such a measurement it retains SDL's default
  selection; XEngine does not assume that OpenGL always beats OpenGL ES.
- Explicit backend (`direct3d11`, `opengl`, etc.): try this driver first.
  With `backendFallback: true`, move through the other Windows candidates
  (or the measured Linux candidates and SDL default on Linux) if it is
  unavailable or fails at startup.
  Set `backendFallback: false` for strict explicit selection and a clear error.
- `SDL_RENDER_DRIVER` takes precedence over JSON and **is always strict**,
  regardless of `backendFallback`. This prevents benchmarks from silently
  measuring a substitute renderer. Unset it to use JSON selection.
- All failed attempts and fallbacks are logged to stderr. XEngine tries every
  allowed driver with the configured acceleration/VSync flags before retrying
  with reduced flags. The successful `[RENDER-BACKEND] requested=... source=...
  actual=... flags=...` log records the *real* renderer.
- During the existing `SetVSync()` renderer recreation, the last working
  backend is tried first. XEngine recreates its internal white draw texture.
  SDL renderer recreation can invalidate **game textures**, so runtime texture
  reload may still be necessary; startup fallback is the reliable recovery
  path. The benchmark independently records actual driver and display details
  in every run's `metadata.json`.

### Which values make sense?

| OS | Typical values | When to try them |
|:--|:--|:--|
| Windows | `direct3d11` | Strong result in the October 2026 XEngine culling A/B/A test; try on modern Windows machines with compatible drivers |
| Windows | `direct3d` | Compatibility, comparison/control, or when D3D11 shows artifacts or worse performance |
| Windows | `opengl`, `opengles2` | Compare alternate driver stacks; behavior depends on GPU vendor and its GL support |
| Linux | `opengl` | Often the main hardware-accelerated desktop path; test on your actual Mesa/proprietary driver and X11/Wayland session |
| Linux | `opengles2` | Alternative when the local SDL build and graphics stack support it |
| Supported systems | `software` | Diagnostic or compatibility fallback, usually significantly slower for sprite-heavy scenes |

The list is *not* a promise that every driver is available. It depends on
the SDL2 build, operating system, video subsystem and graphics drivers.
Use the enumerator below to discover the drivers compiled into **your** SDL2.
The benchmark additionally verifies which of those are usable at runtime.

## Why Windows `auto` now prefers Direct3D 11

The controlled Windows **A (`direct3d`) → B (`direct3d11`) →
A2 (`direct3d`)** benchmark, 8 fixed/adaptive pairs per arm, found:

| Fixed route | A D3D9 | B D3D11 | A2 D3D9 |
|:--|--:|--:|--:|
| Frame p50 | 1.4798 ms | 0.6895 ms | 1.4743 ms |
| Frame p95 | 2.0769 ms | 1.0209 ms | 2.0900 ms |
| Frame p99 | 2.2138 ms | 1.1281 ms | 2.2062 ms |

Direct3D11 was substantially faster for that machine and the
`culling` workload. The measured improvement is mostly in SDL draw/sprite
submission, not solely in `SDL_RenderPresent()`. No counter/physics
differences were found, but **pixel-level visual equivalence has not been
verified**; the other render workloads, Linux and other GPU/driver
combinations were not covered. See
[`RENDER_CULLING_RELEASE_FORENSICS_20261022.md`](RENDER_CULLING_RELEASE_FORENSICS_20261022.md)
for raw methodology, tails and limitations.

In the later four-workload Release benchmark, D3D11 completed 11,520 measured
frames with **zero** frames over the 144 Hz display budget or 60 Hz budget;
its pooled frame p99.9 was ~3.42 ms. GLES2 was slightly faster by p95 but had
**56** frames over the 144 Hz budget, **32** over 60 Hz, **29** long SDL present
calls and ~56.86 ms p99.9. D3D9 had six frames over 144 Hz and zero over
60 Hz; D3D12 had 112 and 18 respectively. This supports the above order
**for the developer's machine**. Results on other GPUs, graphics drivers,
compositors and workloads may vary.

The JSON still says `"windows": "auto"`. Without an installed benchmark,
it means XEngine's compatibility-first D3D11 preference rather than SDL's
default D3D9. With a valid local benchmark, its measured order wins. The
engine never measures frame times during startup. Cross-backend pixel-level
equivalence remains unverified.

- Windows default: `auto`, measured preference when installed, otherwise
  D3D11-first with startup fallbacks and logs.
- Linux default: `auto`, measured preference when installed, otherwise
  SDL's default renderer.
- `Renderer.parallelSelection` remains `fixed`; global experimental
  `parallelMinCommands` remains **1536**.

A benchmark never edits `assets/config/engine.json`, even if it
finds a measured winner. A user may **explicitly install** its verified order
into machine-local preferences, independently of any game. New schema fields
are added in canonical BlankGame
and propagated with `tools/sync_engine_configs.py` without replacing custom
backend or other per-game settings.

## Automatic benchmark: discover, run and rank backends

From the XEngine repository root, with your normal build dependencies installed:

**Windows PowerShell:**

```powershell
python tools/benchmark_render_backends.py --build-config Release --workloads baseline culling mixed rotations_xyz --repetitions 3 --sprites 2048 --threshold 1536 --install-ranking
```

**Linux Bash:**

```bash
python3 tools/benchmark_render_backends.py --build-config Release --workloads baseline culling mixed rotations_xyz --repetitions 3 --sprites 2048 --threshold 1536 --install-ranking
```

**Linux PowerShell 7 (`pwsh`, if installed):**

```powershell
python3 tools/benchmark_render_backends.py --build-config Release --workloads baseline culling mixed rotations_xyz --repetitions 3 --sprites 2048 --threshold 1536 --install-ranking
```

The script:

1. Builds the Release benchmark **once** (unless `--skip-build` is given).
   A failed build **stops** the experiment: no stale binary is run.
2. Launches the built game with `--list-render-backends` to enumerate SDL2
   renderer names and flags. A discovered driver is not automatically a valid
   candidate: it must complete the controlled no-VSync experiment.
3. Executes the existing, counterbalanced `fixed/adaptive` selector sweep for
   **each** eligible backend, without recompilation between arms. If the first
   presenter reports VSync still active, that backend cannot participate in
   the no-VSync comparison. The nested runner and sweep stop immediately,
   preserve that failed attempt and record a **verified non-comparable**
   exclusion; the other backends continue. Other failures (crash, incomplete
   telemetry, wrong renderer, mismatched display) remain **fatal to the
   reliability of the overall comparison**. The
   `SDL_RENDER_DRIVER` environment variable is set **only in subprocesses**;
   your shell/session and `engine.json` are untouched.
4. Requires the actual `renderer_name` to match each requested driver,
   verifies identical display/video conditions, every run's fixed/adaptive
   invariants, complete profiles and valid p50/p95/p99. Contrasts all successful
   drivers against one another, comparing the same frame positions in both
   modes for each seed/repetition/workload.
5. Reads **every frame of the measured window** from each run
   (warmup and terminal frame excluded). Requires valid
   `render_sdl_present_ms` telemetry, a consistent monitor refresh
   rate and **VSync disabled** for every run. In addition to the original
   equal-workload/equal-mode p50/p95/p99 scores, it computes pooled p99.9,
   maximum frame duration, maximum/p99/p99.9 of SDL_RenderPresent, and the
   following absolute counts (same number of measured frames per backend):

   - Frames slower than the actual display refresh budget:
     **1000 / display_refresh_hz** ms (e.g., 6.944 ms at 144 Hz).
   - Frames slower than the reference 60 Hz budget: **16.667 ms**.
   - Calls to SDL_RenderPresent exceeding each of those budgets; a
     **Present >16.667 ms** call is counted as a potentially blocking call,
     without attributing its cause to a specific driver or shader.
6. Sorts by a conservative **stability-first** rule: fewest frames >16.667 ms,
   fewest SDL Present calls >16.667 ms, fewest frames above the display
   budget and fewest SDL Present calls above the display budget. These
   **absolute counters remain strict**: a measured over-budget frame is not
   erased by an otherwise excellent average.
7. When those counters match, compares pooled p99.9 using a **technical-tie
   tolerance of 2% (minimum 0.02 ms)**. The band is anchored to the
   *lowest* p99.9 and groups are disjoint, so SDL enumeration order cannot
   influence which values count as equivalent.
8. Inside each tail band, compares **p95 and p99 in every workload/mode and
   every repetition**. Differences below 2% or 0.01 ms give no victory.
   A meaningful scenario/mode median earns 3 consistency points and an
   individual repetition earns 1; differences within 10% of the possible
   points (minimum 3) are treated as close. Remaining ties are broken by
   meaningful aggregate p95, then p99, then a deterministic compatibility
   order (on Windows this begins with Direct3D 11; on Linux with OpenGL).
   **The fastest backend by p95 is still printed separately.**

This is a heuristic for selecting a backend worth investigating, **not**
statistical proof of a universal winner. In particular, a single rare
external interruption can affect the conservative ordering: inspect
per-run data and repeat A/B/A before changing defaults.
The technical-tie rule prevents microscopic changes of p99.9 alone from
flipping a recommendation. It is **not persistent hysteresis**: sufficiently
different evidence in a future session may legitimately produce a new winner.

The script never changes `engine.json` or claims visual parity. The optional
`--install-ranking` only writes a local preference after a reliable comparison;
without this flag the benchmark is purely observational.
Only explicitly proven no-VSync incompatibilities are *non-comparable* and
excluded without invalidating the successful drivers. A verified exclusion
never receives a performance score or becomes a measured ranking candidate;
it may remain available as a last-resort runtime startup fallback. At least
two fully measured backends, complete cross-backend functional equivalence,
consistent display conditions and **zero genuine failures** are still required
for an installable result. An arbitrary exception cannot be relabeled as an
exclusion: the installer reopens the original single-run failure log and
execution-order evidence. Unknown errors or missing evidence make the ranking
unreliable and leave the machine-local preference untouched.

Output is in `games/XEngine Benchmark/results/render_backends_*/`:

- `report.md`: recommendation with p50/p95/p99/p99.9, slow frames,
  SDL_RenderPresent stalls and worst cases **for all backends**. Includes
  a workload/mode breakdown, identifies which scenario concentrates the
  stutters, explains top-two p99.9 technical ties and consistency points,
  and compares `fixed`/`adaptive` for the recommended backend.
- `backend_ranking.csv`: aggregate scores plus all tail/stability counters.
- `backend_results.csv`: **separate workload/mode** p50/p95/p99.
- `backend_tail_modes.csv`: pooled percentiles and stutter counts by
  workload and selector mode, including per-repetition median p95.
- `backend_tail_runs.csv`: each individual repetition with its
  measured sample count, p95/p99/p99.9, budgets, over-budget counters
  and SDL_RenderPresent timing. The v2 installer verifies the complete
  workload/mode/repetition matrix and **recomputes the same ranking** from it.
- `backend_equivalence.csv`: per-pair functional checks across drivers.
- `backend_exclusions.csv`: verified non-comparable drivers, reason codes and
  diagnostics; these are **not** ranked and do **not** invalidate good samples.
- `backend_failures.csv`: genuine failed drivers and reasons; any entry
  prevents automatic installation.
  Both outcome CSVs include headers even when they have no rows, so an empty
  failures list is unambiguous to the installer.
- `available_backends.json`: SDL2's enumerated candidates.
- `backends/<driver>/`: complete unmodified selector sweep output, including
  per-frame profiles, p95/p99 tails, per-phase reports, actual renderer metadata,
  execution order and logs.

Useful variants:

```powershell
# Compare two Windows renderers with more repetitions.
python tools/benchmark_render_backends.py --backends direct3d direct3d11 --repetitions 8

# Compare Linux OpenGL APIs if SDL enumerates both.
python3 tools/benchmark_render_backends.py --backends opengl opengles2 --repetitions 8

# Limit the test to GPU renderers. By default software is tested once and
# automatically excluded if the presenter cannot satisfy VSync off.
python tools/benchmark_render_backends.py --exclude-software
```

Note: these are commands for the relevant OS; a Linux system generally uses
`python3`, regardless of whether launched from Bash or PowerShell.
The batch may take a substantial time if many SDL renderers are compiled.
All generated results remain available for manual investigation.

### Benchmark resolution and fullscreen mode

The backend benchmark defaults to a **1280 × 720 window**, `--width 1280
--height 720`, and no VSync. This is deliberately repeatable and does not
force a resolution change on the user's desktop. For the most relevant
per-game automatic ranking, measure **the same window dimensions and mode as
the game you intend to launch**. For example, Windows desktop fullscreen on
a 1920 × 1080 monitor:

```powershell
python tools/benchmark_render_backends.py --build-config Release --width 1920 --height 1080 --fullscreen --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --repetitions 7 --install-ranking
```

Linux windowed at 1600 × 900:

```bash
python3 tools/benchmark_render_backends.py --build-config Release --width 1600 --height 900 --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --repetitions 7 --install-ranking
```

`--fullscreen` uses **SDL desktop fullscreen**, not an exclusive display-mode
switch. The monitor's actual desktop mode may override requested window
dimensions; XEngine records the **actual** SDL window size, output resolution,
refresh rate and fullscreen state in each new presenter capture. A newly
installed ranking is applied only to games with matching SDL/video/display,
window size **and fullscreen state**. Older verified ranking files remain
readable with their previous matching rules. There is one installed ranking
per user at a time; the next successful `--install-ranking` replaces it.

Fullscreen is not automatically faster or more reliable than windowed:
desktop composition, GPU scaling and driver behavior vary. Keep VSync disabled
for controlled backend comparisons, measure in the intended game mode and
repeat close results before replacing a known-good ranking. For a windowed
game at 1280 × 720, the default benchmark is appropriate; no extra flag is
needed. The same `--width`, `--height`, `--fullscreen` options can also be
used with `tools/sweep_render_selector.py` and `tools/run_benchmark.py`.

## End-user procedure: calibrate `auto` on your own computer

You can repeat the comparison on **Windows or Linux** without editing engine
source or changing `Renderer.parallelSelection`. Run it on the **computer that
will play the game**, using your normal SDL installation and graphics drivers.

1. In the XEngine repository, build and benchmark in **Release** with all four
   workloads, at least three repetitions and the default 480 measured frames.
   Use the OS-specific `--install-ranking` command above. Repeating the run
   with more repetitions is recommended if two candidates are close.
2. Open the generated `report.md`. Look at p95 **and** p99/p99.9, measured
   frames above both the screen refresh budget and 16.667 ms, and long
   `SDL_RenderPresent` calls. A driver winning only p95 need not win overall.
   The report explicitly labels technical p99.9 ties and shows consistency
   points from workload/mode medians and individual repetitions. Compare the
   per-workload and per-run CSVs to identify initialization-only spikes,
   periodic hitching or unstable repeats.
3. Check `backend_exclusions.csv` and `backend_failures.csv` separately. A
   verifiable no-VSync incompatibility (for example, `software` on some
   Windows SDL2 installations) is not a performance result and need not block
   installation of the remaining valid GPU rankings. If the report says the
   comparison is **unreliable**, do not install it: investigate genuine
   failures, missing telemetry, display changes or functional differences.
   The installer rejects old p95-only results and cannot establish visual
   pixel-for-pixel equivalence.
4. Keep `"Renderer": {"backend": {"windows": "auto", "linux": "auto"}}`
   in each game's runtime `engine.json` (and normally `backendFallback: true`).
   On the next launch, XEngine loads the *installed* ranking only when its OS,
   SDL version, video subsystem, active display index, refresh rate and game
   **window width/height** exactly match the original benchmark. It tries the
   best eligible driver first and moves down the measured list if it fails.
   The Windows compatibility order or SDL default remains available afterward.
5. Verify stderr or the game log: `[RENDER-BACKEND] benchmark-priority=loaded`
   confirms the preference passed validation; the later
   `[RENDER-BACKEND] requested=auto ... actual=...` banner reports the **real**
   renderer. A warning about changed display/window/SDL conditions means the
   benchmark was ignored. In that case rerun it for the new conditions.

If you already have a complete recent `render_backends_*` results directory,
install it **without rerunning** the benchmark (from the repository root):

```powershell
# Windows PowerShell (a Windows-produced result)
python tools/install_render_backend_ranking.py --results "games/XEngine Benchmark/results/render_backends_YYYYMMDD_HHMMSS_ffffff" --check
python tools/install_render_backend_ranking.py --results "games/XEngine Benchmark/results/render_backends_YYYYMMDD_HHMMSS_ffffff"
python tools/install_render_backend_ranking.py --status
```

```bash
# Linux Bash (a Linux-produced result)
python3 tools/install_render_backend_ranking.py --results "games/XEngine Benchmark/results/render_backends_YYYYMMDD_HHMMSS_ffffff" --check
python3 tools/install_render_backend_ranking.py --results "games/XEngine Benchmark/results/render_backends_YYYYMMDD_HHMMSS_ffffff"
python3 tools/install_render_backend_ranking.py --status
```

`--status` prints a readable, numbered backend priority with the preferred
renderer, display/SDL identity, benchmark size, exclusions and selection
policy. The stored preference is still JSON; for scripts or troubleshooting,
use `python tools/install_render_backend_ranking.py --status --json` to print
only the original structured record. A valid installed preference is not
necessarily active at runtime: the game's SDL/display/window settings must
still match the measurements (check the startup logs in step 5).

Replace the placeholder directory with the real generated folder. If you
received a results ZIP, **extract it first** and supply the extracted directory.
Result ZIPs from another OS, reports missing p99.9/SDL-present data, and
benchmarks with failed equivalence cannot be installed as automatic priorities.

The installed preference is *per user and per machine*, **not** shipped with
games or synchronized through BlankGame:

| OS | Local preference file |
|:--|:--|
| Windows | `%LOCALAPPDATA%\XEngine\render_backend_ranking.json` |
| Linux | `$XDG_CONFIG_HOME/xengine/render_backend_ranking.json`, or `$HOME/.config/xengine/render_backend_ranking.json` when XDG_CONFIG_HOME is unset |

The installer intentionally does **not** update this file unless you pass
`--install-ranking` to the benchmark or run the installer explicitly. You may
back up or remove this single file to restore the platform's built-in `auto`
behavior. Changing GPU drivers, SDL version, window resolution, monitor or
display server warrants a new benchmark even if some compatibility checks
still pass. These conditions do **not** fully identify the physical GPU,
therefore do not copy this local file to another PC.

### Choose a backend manually instead

Set the OS entry to a name confirmed by `--list-render-backends` or the
benchmark's `available_backends.json`, e.g. `"linux": "opengl"` or
`"windows": "direct3d11"`. This explicit choice is tried **before** any
locally measured fallback candidates. With `backendFallback: false`, failure
to initialize that backend stops the game with a useful error. The
`SDL_RENDER_DRIVER` process override always has strict priority over both the
manual JSON setting and the installed benchmark ranking; unset it after
controlled experiments. Benchmark comparisons normally turn VSync **off**;
real-game performance with VSync on must still be checked separately.

### Why extreme frame times matter (Windows example, October 2026)

In the uploaded Windows Release session (3 repetitions, 4 workloads,
fixed/adaptive, 2048 sprites, 144 Hz, 11,520 measured frames per backend),
Direct3D11 had a slightly worse p95 score than OpenGL ES 2,
but vastly better tail behavior:

| Backend | p95 score | Pooled p99.9 | Frames >6.944 ms | Frames >16.667 ms | SDL Present >16.667 ms | Worst frame |
|:--|--:|--:|--:|--:|--:|--:|
| Direct3D11 | 1.814 ms | 3.42 ms | 0 | 0 | 0 | 5.78 ms |
| OpenGL ES 2 | 1.771 ms | 56.86 ms | 56 | 32 | 29 | 140.29 ms |

For OpenGL ES 2, **all 32 frames over 16.667 ms occurred in baseline**,
distributed as **16 fixed and 16 adaptive**. It might indicate driver
initialization/caching, but that hypothesis needs independent verification.
The benchmark's stability-based recommendation is **Direct3D11** in this
specific Windows test. It does not imply the same result for every GPU
or for native Linux, and does not by itself prove visual pixel parity.

For the recommended Direct3D11 backend, fixed had lower per-run median
p95 than adaptive in **all four** measured workloads, supporting the current
fixed default without making any automatic configuration change.

## Manual A/B/A and exact driver override

For more conclusive results, repeat the first backend after the second. One
Release build in the first arm; **no** rebuild between the subsequent arms.
The same PowerShell block works on Windows or on Linux with `pwsh` (replace
the backend names with supported ones for that OS):

```powershell
$previousDriver = [Environment]::GetEnvironmentVariable('SDL_RENDER_DRIVER', 'Process')
try {
    foreach ($driver in @('direct3d', 'direct3d11', 'direct3d')) {
        $env:SDL_RENDER_DRIVER = $driver
        python tools/sweep_render_selector.py --build-config Release --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 8
        if ($LASTEXITCODE -ne 0) { throw "Falló el benchmark con ${driver}" }
    }
}
finally {
    if ($null -eq $previousDriver) {
        Remove-Item Env:SDL_RENDER_DRIVER -ErrorAction SilentlyContinue
    } else {
        $env:SDL_RENDER_DRIVER = $previousDriver
    }
}
```

**Before** this block, run the automatic benchmark once or build explicitly:
`python tools/build.py game --project "XEngine Benchmark" --config Release`.
The script above intentionally omits `--build` to avoid recompiling between
arms. Its output names contain timestamps, so the first and third `direct3d`
captures are separate. Use `@('opengl','opengles2','opengl')` on Linux when
both are confirmed available. If a requested backend is absent, initialization
fails **by design**, and that arm is not a valid performance result.

Equivalent Linux Bash:

```bash
python3 tools/build.py game --project 'XEngine Benchmark' --config Release
SDL_RENDER_DRIVER=opengl python3 tools/sweep_render_selector.py --build-config Release --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 8
SDL_RENDER_DRIVER=opengles2 python3 tools/sweep_render_selector.py --build-config Release --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 8
SDL_RENDER_DRIVER=opengl python3 tools/sweep_render_selector.py --build-config Release --workloads baseline culling mixed rotations_xyz --sprites 2048 --threshold 1536 --phase-frames 60 --frames 480 --warmup 60 --repetitions 8
```

The shell assignments in these Bash commands affect only that command. Do
not set `SDL_RENDER_DRIVER=direct3d11` in a Linux environment and expect
native DirectX support.

## Functional and visual parity

The automated suite checks fixed steps, contact/collision signatures, and
draw/visibility/UI counters. These are necessary **but not sufficient** to
claim visually identical frames. Before selecting a platform default:

1. Use identical resolution, workload, random seed, frame count, renderer
   settings (including VSync off), SDL version, display and GPU conditions.
2. Run matching fixed/adaptive profiles, confirm no invariant mismatches,
   examine all phase tails and outliers, and re-run in reverse/A/B/A order.
3. Capture the **same deterministic frame/state** with each backend in a
   dedicated **non-timed** visual validation run, compare images side by side
   and inspect pixel differences at sprite edges, alpha, rotation, fonts and
   texture tint. A normal desktop screenshot taken at arbitrary times is
   **not** an exact frame equivalence check.
4. Reject backends with missing features, incorrect transparency, different
   sprite order, display failures or unacceptable visual differences even
   when their p95 is lower.
5. Validate on different graphics vendors and Linux X11/Wayland environments
   before claiming a universal default.

The current automatic benchmark **does not capture or diff pixels**. Its
rankings must not be cited as full visual-equivalence proof. A future separate
deterministic SDL readback/screenshot facility can automate that validation
without perturbing the timed benchmark.

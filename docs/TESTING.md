# Testing XEngine before a release

XEngine has **two complementary test layers**. A passing run increases
confidence in a change, but does not guarantee that every game, GPU, operating
system or long-running session is stable.

## 1. Fast Python regression suite

From the repository root:

```bash
python -m unittest discover -s tests -p 'test_*.py' -q
```

These tests cover build tools, configurations, source-level invariants,
documentation and previously reported regressions. Many intentionally avoid
compiling or launching SDL2; passing this suite alone is **not** proof that
the actual engine starts or runs correctly.

## 2. Real SDL2 integration test

With the native GCC toolchain and SDL2, SDL2_image, SDL2_ttf, SDL2_mixer and
`nlohmann/json` headers installed (see [Developer Guide](DEVELOPER_GUIDE.md)):

```bash
python tools/test_engine_integration.py --config Debug
python tools/test_engine_integration.py --config Release
```

The runner compiles the **actual `engine/src/*.cpp` files** into `libXEngine.a`
using `tools/build.py`'s production flags, compiles
`tests/integration/engine_smoke.cpp`, links to SDL2, and runs the executable
as a separate process. No engine services are mocked. Its working directory
is the BlankGame template's assets folder, containing real PNG and TTF test
fixtures. An unsuccessful exit, an unmet assertion or a timeout is a failure.

The integration test currently exercises:

- SDL2/SDL_image/SDL_ttf/SDL_mixer initialization and window/render startup;
- scene registration, activation and actual frame callbacks;
- parent-cycle and self-parenting rejection;
- `GameObject` rename and `Scene::Find` index consistency;
- world-space translation under a scaled and rotated parent;
- texture/font cache lookup aliases and duplicate-key rejection;
- creating and destroying objects during `Update`;
- `Engine::Stop()` inside `Update`, followed by exactly one cleanup callback.

Run a short local soak test with a few hundred or thousand rendered frames:

```bash
python tools/test_engine_integration.py --config Debug --frames 600 --timeout 120
python tools/test_engine_integration.py --config Release --frames 600 --timeout 120
```

The default is 16 frames; CI runs 120 per build configuration. The upper bound
is 20,000 frames and the runner kills a hung test after `--timeout` seconds.
Tests are headless by default (`SDL_VIDEODRIVER=dummy`,
`SDL_AUDIODRIVER=dummy`, `SDL_RENDER_DRIVER=software`); override the video
driver to exercise a different display system. On a Linux build agent with
Xvfb, for example:

```bash
SDL_VIDEODRIVER=x11 xvfb-run -a python tools/test_engine_integration.py --config Debug
```

These tests only support **native Windows UCRT64 and Linux**, not cross-built
executables, and deliberately request the SDL **software renderer** for
reproducible execution without a GPU. They do not test Direct3D/OpenGL,
interactive mouse/UI behavior, long-term memory growth, real audio playback,
or behavior across different hardware. Use the benchmark game and manual
testing on actual machines for those areas.

Both native test commands use `--no-doc` internally: **they do not regenerate
local Doxygen HTML**. The published reference remains available at
<https://the-33.github.io/XEngine/>. A normal engine build with `--no-doc`
also tells the optional VS Code Doxygen Hover extension to prefer online
documentation; see [VS Code integration](VSCODE_INTEGRATION.md).

## Continuous integration on GitHub

The repository includes `.github/workflows/test-engine.yml`. It runs on
pushes and pull requests and builds a four-way matrix:

| OS | Debug | Release |
|---|---|---|
| Ubuntu Linux + Xvfb | Python suite + native SDL2 test (120 frames) | Python suite + native SDL2 test (120 frames) |
| Windows + MSYS2 UCRT64 | Python suite + native SDL2 test (120 frames) | Python suite + native SDL2 test (120 frames) |

GitHub Actions **Test XEngine → Run workflow** also accepts `soak_frames`:
set, for example, `2000` to run an additional integration pass on all four
matrix configurations, or `0` (default) to skip it. The runner enforces the
same range of 2–20,000 frames. The publishing workflow for Doxygen/GitHub
Pages runs separately and is not replaced by this test workflow.

## What counts as a stable release?

Before shipping an engine version, run the fast suite and native integration
tests in **both Debug and Release on Windows and Linux**, check CI results,
and perform targeted manual play/benchmark testing for the features that
changed. For changes affecting lifecycle, physics, rendering or resource
management, extend the tests with reproductions of the exact bug.

Passing tests is **evidence of stability, not a guarantee of no bugs**.
Each iteration must also review whether an engine version bump is appropriate
according to [Versioning](VERSIONING.md); adding tests alone does not
automatically require a release or a higher version number.

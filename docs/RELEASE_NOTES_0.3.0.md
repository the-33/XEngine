# XEngine 0.3.0 — Release notes

Previous development release: **0.2.1**. The repository previously carried an
incorrect `2.0.1` label; this snapshot corrects the official root `VERSION`
and the generated-documentation source to **0.3.0**.

## New APIs

- `UI->Line(start, end, color, thickness)` and the anchored
  `UI->Line(layout, color, thickness, parent)` draw configurable-width lines
  in the UI command queue. Anchored line thickness follows the canvas scaler.
- `UI->SetLayer(int)`, `UI->PushLayer(int)`, `UI->PopLayer()` set a numeric UI
  draw/input layer. Higher layers draw above lower layers; ties retain
  submission order.
- All 13 immediate-mode UI element families accept a layer directly after the
  rectangle (or after both line endpoints), for example
  `UI->Button("play", bounds, 10, "Play")`. The previous layer is restored
  automatically after that call.
- Overlapping interactive controls use topmost hit-testing: higher layers win,
  and the last submitted control wins within one layer. Because this is an
  immediate-mode API, hit ownership is determined using the previous frame's
  completed control list. Dropdown popups draw last within their own layer.

## Compatibility and other fixes

- Existing UI method signatures and `PushLayer`/`PopLayer` scopes remain valid;
  **no game-source migration is required**.
- Includes cumulative improvements to DPI awareness, crisp UI text, canvas
  layout validation, debug gizmos, engine log captions, Windows console output,
  and related integration/tooling tests. Consult `README_PARCHE.md` for details.
- Save-game version `3` and `.animation.json` / `.animator.json` asset-format
  versions `1` are independent of the engine release and are unchanged.

## Validation

- Python regression suite and SDL-independent C++ harnesses were run on the
  development snapshot. The suite must be rerun after the version correction.
- Native SDL2 builds and integration smoke tests on Windows and Linux, Debug
  and Release, are still required before publishing a final tagged release.

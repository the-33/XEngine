# XEngine 0.4.0 — Release notes

Previous development release: **0.3.0**. This development release introduces
a compatible retained-widget UI layer alongside the existing immediate-mode API.

## New public UI APIs

- `UI->CreateCanvas("MainMenu")` creates or retrieves a named canvas that owns
  UI elements. `canvas.Create<UIButton>("play", bounds)` returns a stable
  widget reference until the element or canvas is destroyed.
- `UI->Find<UIButton>("MainMenu/play")` and
  `canvas.Find<UIButton>("play")` retrieve controls by typed keys from
  unrelated game scripts. Wrong or missing types return `nullptr`.
- `UIWidgetFluent<T>` adds chainable style, position, anchor, layer,
  visibility and pointer/focus event methods returning `T&`. Compatible
  controls expose `SetCornerRadius` (uniform, top/bottom and four-corner),
  color, border and font settings; specialized widget methods configure
  selection, editable text, slider geometry, toggle tracks and image tints.
- `UIButton`, `UIImageButton`, `UIPanel`, `UILabel`, `UICheckbox`, `UISlider`,
  `UIToggle`, `UITextInput`, `UIDropdown`, `UIProgressBar`, `UIImage`, `UILine`
  and `UIInvisibleButton` form
  the initial retained-widget family. The canvas submits visible widgets
  automatically after input polling; no manual per-frame Draw call is needed.
- `OnClick`, `OnPointerDown/Up`, `OnHoverEnter/Exit`, `OnFocus/Blur`,
  `OnValueChanged`, `OnEndEdit`, `OnSubmit`, `OnOpen/Close` and slider
  `OnDragStart/End` are available where relevant. Callback dispatch after
  widget traversal avoids collection invalidation if user callbacks remove
  widgets or request scene changes. Value controls expose `SetValue` and
  `SetValueWithoutNotify` for programmatic updates.

## Compatibility and lifecycle

- Existing `UIManager::Button`, `TextInput`, `Dropdown`, `Slider`, `SetCanvas`,
  `PushStyle`, numeric layers and all other immediate-mode APIs are unchanged.
- The old `UICanvas` remains a resolution-scaling settings struct. Each
  retained `UIWidgetCanvas` can choose a scaler with `SetScaler`.
- Canvases default to scene lifetime; `SetPersistent(true)` retains a canvas
  across scene replacements. Removing a widget/canvas invalidates its
  previously returned nonowning C++ references and pointers.
- The game save schema, animation/controller JSON schemas and game project
  version numbers have not changed. No game code migration is required.

## Verification and release status

- The new SDL-independent C++20 compilation/runtime harness verifies real
  fluent setter definitions, typed key lookup, callback deferral, individual
  corner radii, anchored rendering dispatch and controlled values.
- Existing Python regression suite, version consistency tests and static
  engine integration guards are part of the validation gate.
- The environment lacks native SDL2 development libraries and Doxygen;
  actual Windows/Linux Debug and Release builds, visual UI interaction checks,
  smoke tests, and both rendered documentation references remain **pending**.
  Do not claim those platform checks have passed until they have been run.

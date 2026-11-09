# XEngine — Engine Developer Reference

This is the **complete codebase reference** for contributors modifying
XEngine itself. It includes public APIs, protected/private implementation
members, internal engine headers and `.cpp` implementation files. Internal
fields and symbols shown here are **not** automatically supported gameplay
APIs; they may change between engine versions.

If you are implementing a game instead, use the **Game Developer Reference**
at `build/docs/game/html/index.html` and the engine's public headers.

## Start here

- @subpage xengine_engine_internals "Architecture, ownership and extension points"
- @subpage xengine_documentation_workflow "Maintaining XEngine documentation"
- @subpage xengine_engine_manuals "Developer manuals and current guides"
- @subpage xengine_game_reference "Game API and feature tutorials"
- @subpage xengine_animation_internals "Animation evaluator and property registry"

## Explore the implementation

Use the **Files** list to navigate `engine/src/*.cpp` and internal headers;
use **Classes** for private and public members and **Modules** for the
subsystem groupings. Search is available for individual symbols. The
generated source browser is a navigation tool, not a replacement for
building and testing the engine.

For every public change, preserve the distinction between an implementation
detail and a supported developer API. Expose validated state via public
methods or `Property`/`PropertyRO`; do not encourage game code to access
internal backing fields or their adapter type aliases.

### File Members versus class members

The Doxygen **File Members** section indexes declarations that belong to an
individual translation unit or header rather than to a class. Its **Variables**
view is not an inventory of every engine field: most engine state is owned
by classes, whose fields are documented in **Classes**. It is normal for
File Members → Variables to contain relatively few entries even with
`EXTRACT_ALL`, `EXTRACT_PRIVATE` and `EXTRACT_STATIC` enabled. For example,
the engine currently exposes a small set of file-scope UI state in
`UIManager.cpp`; class-owned state belongs in the corresponding class page.
Use the source browser or XML reference to audit private code symbols.

### Retained widgets on keyed canvases (engine 0.4.0)

`engine/include/UIWidgets.h` supplies the persistent widget hierarchy and
`engine/src/UIWidgets.cpp` implements canvas management on `UIManager`.
This does **not** introduce a second renderer: concrete `UIButton`,
`UIImageButton`, `UIPanel`, `UILabel`, `UICheckbox`, `UISlider`, `UIToggle`,
`UITextInput`, `UIDropdown`, `UIProgressBar`, `UIImage`, `UILine` and
`UIInvisibleButton` call the existing
styled UIManager overloads, preserving input and immediate/replayed drawing.

- `UIWidget` owns a `Canvas/Key` runtime ID, style, geometry, optional copied
  parent, layer, visibility and common focus/pointer event handlers.
  `UIWidgetFluent<Derived>` provides fluent CRTP setters returning `Derived&`;
  concrete classes add real type-specific properties and value callbacks.
  Capabilities such as rounded corners are constrained for labels/textures,
  which do not own a rounded rectangular frame.
- `UIWidgetCanvas` keeps owned `unique_ptr<UIWidget>` in a keyed map and
  separately preserves insertion order. Duplicate/empty keys are rejected;
  typed `Find<T>` checks RTTI. `UIManager` owns the canvases by key and also
  supports `Find<T>("Canvas/Key")` from any script. References stay stable
  across map rehashes but **not** element removal or scene destruction.
- `Engine::PollInput_` calls `UIManager::Begin()` after input polling and
  before scene Update. The last step of `Begin()` now submits visible canvases
  with their own UICanvas scalers and per-widget appearance/layer. Completed
  callbacks are queued by value and dispatched **after** all canvases are
  traversed, so an OnClick can remove elements or schedule a scene change
  without invalidating the traversal. `UIManager::End()` still flushes the
  commands, sorted by numeric layer, as before.
- `ButtonBehavior_` records per-frame hover/held/click/pointer-up/down
  snapshots alongside its established topmost-hit/modal filtering; wrappers
  use the original `bool` return to emit `OnClick`/`OnValueChanged` only on
  genuine interaction. Text fields additionally detect edit completion,
  with `mSubmittedTextId` distinguishing Enter/UI Submit from focus loss.
  Programmatic `SetValue` notifies if changed, whereas `WithoutNotify`
  never notifies. Pointer/keyboard callbacks carry copied values.
- When a scene is replaced, `SceneManager::ApplyPendingScene()` destroys all
  canvases without `SetPersistent(true)`. `UIManager::Shutdown()` destroys
  every canvas. Initial scene activation (no outgoing scene) keeps UI created
  during game initialization. A canvas's `SetScaler` is per-canvas; the old
  plain `UICanvas` value struct and `UIManager::SetCanvas` remain supported.

The retained API does not yet implement curved texture clipping, serialized
inspector events, or a nested layout/parent hierarchy. Do not expose
nonfunctional styling methods for those future capabilities. The production
header is compiled and exercised against an SDL-independent test manager in
`tests/test_ui_widget_canvas.py`; true SDL2 Windows/Linux Debug and Release
tests are still required before declaring platform validation complete.

### Screen-space UI geometry and focus

`UIManager` queues immediate-mode widget commands, including styled rounded
rectangles and vector dropdown chevrons. `RenderManager::DrawRoundedRectScreen`
and `DrawLineScreen` submit their geometry through SDL and also record distinct
`DrawCmd::Type::ScreenRoundedRect` and `ScreenLine` commands for the deferred
snapshot path. These commands bypass camera projection, use drawable pixels
and count as UI work. Keep the immediate and recorded paths equivalent when
changing their drawing logic.

TextInput's internal selection and caret use `UIManager::PushSolidRect_` to
queue a filled screen rectangle **without** inheriting border width or corner
radius. Its caret phase uses `SDL_GetTicks` through the SDL-independent
`xengine::detail::ui::CaretVisible` helper; editing or cursor/anchor changes
reset the visible phase. Keep the UTF-8 byte-boundary and input/clipboard
logic independent of this visual state.

Dropdown highlights deliberately reuse one rounded popup *interior* instead
of giving each row four rounded corners. `DropdownRowClip` snaps that common
silhouette and derives contiguous, pixel-aligned row clips after subtracting
the style's border width. `UIManager` queues `ClippedRoundedFill` commands;
`RenderManager::DrawClippedRoundedFillScreen_` records a clipped
`ScreenRoundedRect` command or temporarily sets SDL's clip rectangle in the
immediate path, restoring any previous clip afterwards. The popup's outline
is queued **after** row highlights/text so no highlight can cover it. This
path preserves numeric layer ordering and the existing pointer-hit regions.

`UIStyle::SetCornerRadius` has one-, two- and four-value overloads. The
legacy `cornerRadius` float remains intact; the two/four-value overloads
enable a stored four-corner override. The two-value form maps to
`{top, top, bottom, bottom}`. `RadiiForRect_` resolves those values through
the existing pixel/percent unit convention. `FitCornerRadii` normalizes
adjacent radii proportionally to fit every side, and `OffsetCornerRadii`
derives the interior and anti-aliased outline contours. The new private
`RenderManager::DrawRoundedRectCornersScreen_` preserves both immediate
SDL geometry submission and snapshot replay (`hasScreenCornerRadii`), while
the old uniform-radius path/API remains unchanged. Clipped Dropdown rows
also use the same four-corner silhouette when that override is active;
`engine.json` accepts scalar, two-value or four-value `cornerRadius`.

The UI focus list is rebuilt every frame. `SetInitialFocus` retains a requested
ID until the next matching widget registration; `SetAutoSelectFirst(false)`
prevents the normal fallback that selects the first widget. Directional
navigation uses `UINavigation` ID overrides where present and geometric
nearest-neighbour selection otherwise. Explicit targets must exist in the
previous frame's focus list, which prevents navigation to hidden widgets.
Style overrides use a balanced stack, and each queued drawing command
captures the appearance it needs before `PopStyle` restores the base style.
The per-widget `UIStyle` overloads use `ScopedControlStyle`, an RAII push/pop
pair that restores the previous appearance on every return path and forwards
to the existing input and rendering implementations. Maintain matching
overloads for pixel-space `Rect` and anchored `UIRectTransform` widgets.
`UICornerRadiusUnit::Percent` converts the requested radius using the shortest
side of each resolved widget; pixel mode follows canvas scaling instead.
For slider tracks/handles and toggle tracks/knobs, `PushPanel_` can take an
explicit pixel-radius override so they can be pill-shaped independently of
the surrounding control's corner radius. `toggleTrackOnlyHit` changes the
registered hit/focus bounds to the visible switch without changing controller
Submit behavior. The other controls still use their original interaction bounds.

The root `VERSION` identifies the engine release; a game's own version does
not change when the engine is rebuilt. Follow the
[versioning and compatibility guide](../../VERSIONING.md) for release policy.

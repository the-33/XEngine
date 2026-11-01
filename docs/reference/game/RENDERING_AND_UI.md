# Sprites, window, camera and screen-space UI

Use @ref AssetManager to load textures, @ref SpriteRenderer to draw an object,
@ref Camera2D to map world coordinates to the viewport, @ref WindowManager
for resolution/fullscreen and @ref UIManager for immediate-mode controls.

## Renderable objects and resources

A `Transform` changes position but is not drawn by itself. Attach a
`SpriteRenderer`, assign a loaded texture and configure its tint/pivot.
Textures are cached by the asset manager; do not manually delete them.
See @ref RenderManager for lower-level drawing operations.

## Window resolution and aspect

```cpp
#include <XEngine.h>

bool accepted = Window->SetSize(1600, 900);
if (!accepted) { /* Show a settings warning. */ }

Window->isFullscreen = true;
Vec2I logicalSize = Window->size.get();
Vec2I drawablePixels = Window->drawableSize.get();
```

`Window->SetFullscreen(true)` uses **desktop fullscreen** (borderless) and
does not alter the monitor's resolution. If the game needs the screen to
switch to a particular video mode, request **exclusive fullscreen** instead:

```cpp
// Returns false if this exact display mode is unavailable or SDL rejects it.
if (!Window->SetExclusiveFullscreen(1280, 720)) {
    // Show an unsupported-resolution warning and restore previous settings.
}
// Returns to the previously configured windowed size and desktop video mode.
Window->SetFullscreen(false);
```

`SetSize(width, height)` only controls the client-area size while windowed;
in fullscreen it remembers the dimensions for returning to windowed mode.
`SetExclusiveFullscreen` is opt-in so other games using borderless desktop
fullscreen keep their existing behavior. Only exact monitor-supported modes
are accepted, and the available video modes depend on the operating system
and active monitor. **Do not use exclusive fullscreen if your goal is to
render the game at lower quality and upscale it on the same monitor mode.**

### Internal world rendering resolution (pixelated fullscreen or windowed)

`RenderManager::SetRenderResolution(width, height)` changes the **world's
render texture**, independent of the physical screen size. The renderer
composites the low-resolution world image into a window or borderless fullscreen
using nearest-neighbour filtering, with letterbox bars if aspect ratios differ.
`UIManager` buttons, labels, menus and text remain crisp at native display
resolution. The monitor's actual video mode does not change:

```cpp
Window->SetFullscreen(true); // Borderless, desktop resolution stays the same.
if (Window->IsFullscreen() && !Renderer->SetRenderResolution(640, 360)) {
    // Renderer may not support target textures: show an error/restore settings.
}
Vec2I native = Window->GetDrawableSize();       // Physical output pixels.
Vec2I world = Renderer->GetRenderResolution();    // Now 640 x 360 on success.
Vec2I desktop = Window->GetDesktopResolution(); // Desktop video mode of this monitor.

// Restore direct-to-window world drawing (available in either window mode):
Renderer->SetRenderResolution(0, 0);
```

Calls made from an in-frame button/option callback are committed **after**
that frame, preventing the currently bound texture from being destroyed during
rendering. If the chosen video driver lacks texture-target support, the method
returns `false` and the previous target remains intact. The rendering API
handles immediate and recorded/parallel command modes, camera projection,
world-space mouse mapping and display-to-render scaling. Use
`Renderer->DisplayToRender(pixel)` to map output pixels into the letterboxed world
image manually; pixels in the black bars map outside the game render area.

Screen-space rendering and `UIManager` use drawable pixel coordinates, which
can differ from logical window dimensions on HiDPI displays. Mouse hit testing
is scaled automatically inside `UIManager`. `Window->resizeRevision` changes
when the window mode or dimensions change. `Camera2D::matchWindowAspect`
keeps the vertical world span while adapting the horizontal span by default.
Do not assume camera projection and UI pixels are the same coordinate space.

## World versus screen coordinates

```cpp
Camera2D* camera = CurrentCamera;
if (camera) {
    camera->center = Vec2{0.f, 0.f};
    Vec2 pixel = camera->WorldToScreen(Vec2{2.f, 5.f});
    Vec2 world = camera->ScreenToWorld(pixel);
}
```

Change world-space positions through `Transform`; position menus and buttons
in screen space. When resizing, recalculate screen-space layout from the
current `Window->drawableSize` rather than hardcoding coordinates for 1280x720.

## Immediate-mode controls

`UIManager` renders text, panels, buttons, sliders, switches and image controls. Draw them from
your `Behaviour::Render()` or equivalent gameplay rendering hook. Button IDs
must be stable and unique for each interactive control, and the rectangle
must be computed in screen coordinates.

```cpp
Vec2I size = Window->drawableSize.get();
Rect action{static_cast<float>(size.x) - 180.f, 20.f, 160.f, 42.f};
if (UI->Button("pause_button", action, "Pause")) {
    Time->timeScale = 0.f;
}
```

### Native Slider and Toggle

Both controls are part of `UIManager` itself; no game-specific UI helper
headers are necessary. They operate on caller-owned values, return `true`
**only when those values change**, and have both screen-pixel `Rect` and
responsive `UIRectTransform` overloads:

```cpp
// Members of your options panel (keep them between frames):
float volume = 0.8f;
bool fullscreen = false;

// In Behaviour::Render(), every frame while the panel is visible:
if (UI->Slider("options.volume", Rect{100.f, 100.f, 280.f, 40.f},
               &volume, 0.f, 1.f, 0.05f)) {
    Sound->SetMasterVolume(volume); // preview
}
if (UI->Toggle("options.fullscreen", Rect{100.f, 160.f, 150.f, 44.f},
               &fullscreen)) {
    // Save/apply on your own Save button, if desired.
}

// Anchored version of the same native widgets:
UI->Slider("audio.preview", UIRectTransform::Center({280.f, 40.f}),
           &volume, 0.f, 1.f, 0.05f);
UI->Toggle("display.fullscreen", UIRectTransform::BottomRight({150.f, 44.f}),
           &fullscreen);
```

Give each control a unique ID, including when drawing both variants.
The slider supports mouse press/drag, including dragging past either end to
reach its minimum/maximum. Mouse positions are automatically adjusted for
HiDPI displays. Mouse movement snaps to the given positive `step`. When
focused, Left/Right (or `UI/Navigate` horizontal on a gamepad) changes the
value by that step; Submit increases it, while Up/Down and Tab still move
between controls. Toggle changes the bool on mouse click/release or Submit.
Both show the configured focus outline. Values are not saved implicitly:
the game decides whether to store options in `PlayerPrefs` or elsewhere.

### Native Dropdown

`UIManager::Dropdown` is a reusable single-selection list. Pass a stable ID,
rectangle, caller-owned selected index and a vector of option labels. It returns
`true` only when the chosen index changes. Click or UI Submit opens the list;
Up/Down or controller `UI/Navigate` changes the highlighted option; Submit
chooses; Escape/UI Cancel closes without changing the selection. Mouse wheel
scrolls longer lists. The popup draws above other controls and opens upward
when there is more space above it.
Its arrow is a screen-space vector chevron, not a font character: its color,
line thickness and proportions stay consistent across fonts and operating
systems. The arrow uses the current `UIStyle::dropdownArrow` color.

```cpp
// Persistent Behaviour fields, not temporary locals recreated each frame:
std::vector<std::string> sizes{"1920 x 1080", "1280 x 720", "640 x 360"};
int selected = 1;

// Inside Behaviour::Render():
if (UI->Dropdown("options.resolution", Rect{100.f, 140.f, 240.f, 42.f},
                 &selected, sizes, 6)) {
    // The caller decides whether to apply or save this change.
}
```

An anchored `UIRectTransform` overload also exists. If a parent menu handles
Escape itself, check `UI->DropdownConsumedCancel()` during Update and ignore
that Cancel event: it was already used to dismiss the open dropdown.

### Styling, borders and rounded corners

Built-in controls use a reusable `UIManager::UIStyle` containing normal,
hover and active backgrounds, text and focus colors, fonts, padding, corner
radius and border thickness. It also exposes the slider track/fill/handle,
toggle colors, dropdown popup/selection/arrow, and text-input placeholder and
selection colors. The default radius is zero, preserving existing square UIs.
`SetStyle` changes the default appearance; `PushStyle` and `PopStyle` apply a
temporary appearance to just the controls drawn between them:

```cpp
// Once, when configuring the game UI:
UIManager::UIStyle rounded = UI->GetStyle();
rounded.cornerRadius = 10.f;
rounded.borderWidth = 2.f;
rounded.focus = Color{110, 205, 255, 255};
rounded.dropdownArrow = Color{235, 240, 255, 255};
UI->SetStyle(rounded);

// Alternatively, express the radius as a percentage of the widget's
// shortest side; 50% gives a capsule (or a circle for a square widget).
UIManager::UIStyle pill = rounded;
pill.cornerRadiusUnit = UICornerRadiusUnit::Percent;
pill.cornerRadius = 50.f;

// In Behaviour::Render(), every frame that this control is visible:
UI->PushStyle(pill);
UI->Button("menu.play", Rect{50.f, 80.f, 220.f, 50.f}, "Play");
UI->PopStyle();

// In Behaviour::Render(), for a single differently styled button:
UIManager::UIStyle danger = UI->GetStyle();
danger.btn = Color{105, 40, 45, 255};
danger.btnHot = Color{140, 50, 60, 255};
danger.cornerRadius = 16.f;
UI->PushStyle(danger);
bool quit = UI->Button("main.quit", Rect{50.f, 160.f, 220.f, 50.f}, "Quit");
UI->PopStyle();
if (quit) { /* Handle the click. */ }
```

The radius and border thickness apply to buttons, panels, text fields, dropdowns,
sliders, toggles, checkboxes and progress bars. The outline stays inside each
rectangle. Existing `UIButtonColors`, explicit label fonts/colors and
progress-bar colors remain supported. Textures and `ImageButton` images are not
masked to the border radius; use an image with transparency if its actual
picture must have rounded corners. An `ImageButton`'s optional focus frame does
use the configured radius. `UICornerRadiusUnit::Pixels` uses drawable pixels;
`UICornerRadiusUnit::Percent` computes the radius from the **shortest side of
each resolved rectangle** and clamps it to 50%, avoiding over-rounded corners.
The percentage adapts automatically when an anchor changes the control's size;
anchored controls scale pixel radii, borders and text spacing with `UICanvas`.

#### Individual styles without PushStyle/PopStyle

All built-in controls have overloads accepting `const UIManager::UIStyle&`.
For a single widget, pass the style directly; it affects only that call and
automatically restores the previous active style afterwards. This also works
inside a surrounding `PushStyle` scope. The normal and styled overloads can
be freely mixed within the same UI frame.

```cpp
UIManager::UIStyle danger = UI->GetStyle();
danger.btn = Color{105, 40, 45, 255};
danger.btnHot = Color{140, 50, 60, 255};
danger.cornerRadiusUnit = UICornerRadiusUnit::Percent;
danger.cornerRadius = 25.f;

// Only this button has the red appearance and proportional corners.
bool quit = UI->Button("menu.quit", Rect{50.f, 160.f, 220.f, 50.f},
                       danger, "Quit");

// The next control still uses the normal UI appearance.
UI->Button("menu.back", Rect{50.f, 220.f, 220.f, 50.f}, "Back");

// Anchored widgets use the same extra style argument.
UIRectTransform anchored = /* menu layout */;
UI->Toggle("options.fullscreen", anchored, &fullscreen, danger, "Fullscreen");
```

The style argument is placed immediately after the rectangle/layout for
`Button`, `Label` and `Panel`, after the texture for `ImageButton`, and after
the caller-owned value for `Checkbox`, `Toggle`, `Slider` and `TextInput`.
For `Dropdown`, it follows the options vector; for `ProgressBar`, it follows
the rectangle/layout. See each overload in the C++ API reference for the
remaining optional parameters and the versions using `UIRectTransform`.

`Panel` and `ProgressBar` have two styled variants: one takes colors from the
`UIStyle` palette, and one takes explicit colors while still using the style's
corner radius and border width. The progress-bar palette fields are
`progressBack`, `progressFill` and `progressOutline`. For example:

```cpp
UIManager::UIStyle healthStyle = UI->GetStyle();
healthStyle.progressBack = Color{45, 45, 50, 255};
healthStyle.progressFill = Color{210, 65, 80, 255};
healthStyle.cornerRadius = 8.f;

UI->ProgressBar(health01, Rect{24.f, 24.f, 280.f, 28.f}, healthStyle);
UI->Panel(Rect{24.f, 64.f, 280.f, 100.f}, healthStyle);
```

All styled overloads reuse the original input handling, keyboard/controller
navigation and queued rendering. They do not create a separate styling system.

When using a temporary style, always pair `PushStyle` and `PopStyle`, even
inside conditional rendering branches. `SetStyle` changes the base style only:
an active temporary style remains in effect until popped.

The engine's `assets/config/engine.json` accepts these optional startup values:

```json
"UIStyle": {
  "defaultFontKey": "ui_default",
  "cornerRadius": 10.0,
  "borderWidth": 2.0,
  "cornerRadiusUnit": "pixels"
}
```

The full palette is configured in C++ through `UIStyle`. The JSON values do
not affect world-space sprites or the camera. For proportional rounding,
set `cornerRadiusUnit` to `"percent"` and a `cornerRadius` such as `50.0`.

#### Rounded slider and toggle parts

Individual styles can also change the geometry of sliders and toggles without
changing the game-wide appearance. These values are part of `UIManager::UIStyle`:

| Field | Default | Effect |
| --- | --- | --- |
| `sliderShowBackground` | `true` | Draw the large surrounding slider rectangle. |
| `sliderRoundElements` | `false` | Make the track ends round and the handle circular. |
| `sliderTrackThickness` | `8` | Track thickness in drawable UI pixels. |
| `sliderHandleDiameter` | `18` | Handle diameter in drawable UI pixels. |
| `toggleShowBackground` | `true` | Draw the large surrounding toggle rectangle. |
| `toggleRoundElements` | `false` | Make the switch a pill with a circular knob. |
| `toggleTrackOnlyHit` | `false` | Limit pointer clicks and focus bounds to the switch track. |
| `toggleShowStateText` | `true` | Draw ON/OFF or the supplied label next to the switch. |

```cpp
auto volumeStyle = UI->GetStyle();
volumeStyle.sliderShowBackground = false;
volumeStyle.sliderRoundElements = true;
volumeStyle.sliderTrackThickness = 13.f;
volumeStyle.sliderHandleDiameter = 25.f;
UI->Slider("volume", volumeRect, &volume, volumeStyle, 0.f, 1.f, 0.05f);

auto modeStyle = UI->GetStyle();
modeStyle.toggleShowBackground = false;
modeStyle.toggleRoundElements = true;
modeStyle.toggleTrackOnlyHit = true;
modeStyle.toggleShowStateText = false;
UI->Toggle("fullscreen", switchRect, &fullscreen, modeStyle);
UI->Label("Fullscreen", separateLabelRect);
```

When `toggleTrackOnlyHit` is enabled, the adjacent text does not activate the
switch. Controller Submit still activates it when focused. The values scale
with `UICanvas` for anchored controls. The round track/handle settings are
independent of the control's outer `cornerRadius`; old layouts retain their
previous appearance until the new options are enabled.

### Navigation, focus and text fields

Every interactive widget participates in automatic focus navigation by
default: arrows or a controller D-pad/stick move to a nearby control
(except Left/Right while a slider has focus, which adjusts that slider),
Tab/Shift+Tab cycle in draw order, and Enter / the controller South button
activates the focused widget. The focused widget shows a blue outline.
The keyboard/controller player is selected with `UI->SetNavigationPlayer(0)`;
call `UI->SetNavigationEnabled(false)` when a particular screen must not
consume UI navigation. Labels, panels and progress bars never take focus.
You can change the focus outline with `UI->SetFocusColor(...)` and the
held-direction repeat timing with `UI->SetNavigationRepeat(initialMs, repeatMs)`.

Focus on a particular widget with `Focus(id)` after its draw call, or request
an initial widget with `SetInitialFocus(id)` when opening a panel: the request
is applied as soon as that widget is registered. `IsFocused(id)` and
`FocusedId()` report the current selection. By default, focus automatically
falls back to the first available widget. To allow a menu to stay **without**
selection, disable this behavior and clear focus:

```cpp
// When opening a mouse-first menu:
UI->SetAutoSelectFirst(false);
UI->ClearFocus();

// Alternatively, when opening a controller-first menu:
UI->SetInitialFocus("menu.play");
```

With auto-selection disabled, a direction or Tab selects the first eligible
control on the next input. Clicking a control focuses it normally. If you call
`ClearFocus()` while auto-selection is enabled, the first eligible control
will be selected again on the next frame.

`UINavigation` configures each control's directional behavior. There are three
modes: `Automatic` selects the nearest widget geometrically; `Explicit`
follows stable widget IDs; `None` prevents directional/Tab navigation to or
from a widget, while mouse clicks and explicit `Focus(id)` still work.
Missing IDs or empty directions under `Explicit` leave focus unchanged.

```cpp
UINavigation links;
links.mode = UINavigationMode::Explicit;
links.down = "menu.options";
links.up = "menu.quit";
UI->SetNavigation("menu.play", links);

// Another widget can use automatic navigation or be mouse-only:
UI->SetNavigation("menu.decorative_hotspot",
                  UINavigation{UINavigationMode::None});

// Undo the overrides when the menu is destroyed or the IDs are reused:
UI->ClearNavigation("menu.play");
UI->ClearNavigation("menu.decorative_hotspot");
// UI->ClearAllNavigation(); // Also available for a full menu rebuild.
```

Tab/Shift+Tab traverse the registration order, skipping widgets marked `None`.
Explicit directional rules apply only to the arrow keys and `UI/Navigate`;
they do not override Tab order. Sliders still consume Left/Right to adjust
their value, and an expanded dropdown consumes its own navigation until closed.
Navigation rules persist across frames; do not register them anew every frame.

The new `TextInput` edits a **game-owned** UTF-8 string, so its contents can
be saved with your gameplay data (for example, the name in a save slot):

```cpp
// Persistent game/scene state, not a local rebuilt on every Render() call:
std::string playerName = "Player";

// In the scene UI render callback, once per frame:
UI->Label("Player name", Rect{32.f, 30.f, 240.f, 36.f},
          AlignH::Left, AlignV::Middle);
if (UI->TextInput("player_name", Rect{32.f, 70.f, 260.f, 40.f},
                  &playerName, "Enter a name", 32)) {
    // playerName changed this frame; use it directly for the next save.
}
if (UI->Button("save", Rect{32.f, 126.f, 160.f, 40.f}, "Save")) {
    // Persist the current playerName and your other save data.
}
```

Clicking a field or pressing Submit while it has focus enters text editing.
Enter confirms, Escape stops editing while preserving text, and Tab exits to
the next control. Click to place the caret and drag to select text; Shift+arrows,
Shift+Home/End, Ctrl+A, Ctrl+C, Ctrl+X and Ctrl+V work, as does SDL key repeat;
Backspace, Delete, Left/Right and Home/End are supported;
text entry uses SDL's UTF-8 text input events, not scancode-to-character
conversion. `TextInput` returns true when its supplied string changes, and
accepts either `Rect` or responsive `UIRectTransform`. Each widget ID must
remain unique and be registered every frame in which it should be navigable.

SDL window and render calls should stay on the main engine thread. Game code
controls its own interface's layout. For responsive anchored controls, use
`UIRectTransform`: the UI manager resolves anchors automatically against the
current drawable canvas on every call, without manual resize detection.

## Unity-style anchors and canvas scaling

`#include <XEngine.h>` also imports `UIRectTransform`, `UICanvas` and
`UICanvasScaleMode`. Existing `Rect`-based UI calls keep their original
drawable-pixel behavior; anchors are opt-in.

```cpp
// Configure once, e.g. during initialization of your UI behaviour.
UICanvas canvas;
canvas.scaleMode = UICanvasScaleMode::ScaleWithScreenSize;
canvas.referenceResolution = {1920.f, 1080.f};
canvas.matchWidthOrHeight = 0.5f; // 0 = width, 1 = height
UI->SetCanvas(canvas);

// In Behaviour::Render(), recomputed for every resolution/aspect ratio:
if (UI->Button("pause", UIRectTransform::TopRight({180.f, 48.f},
                                                  {24.f, 24.f}), "Pause")) {
    Time->timeScale = 0.f;
}

const UIRectTransform panel = UIRectTransform::Stretch(24.f, 90.f, 24.f, 24.f);
Rect panelPixels = UI->Resolve(panel);
UI->Panel(panelPixels, Color{20,20,20,220});

// Child controls anchor to a resolved parent, not the entire canvas.
UI->Button("ok", UIRectTransform::BottomRight({140.f, 44.f}, {16.f, 16.f}),
           "OK", &panelPixels);
```

Anchor coordinates are normalized **top-left (0,0) → bottom-right (1,1)**,
matching SDL's downward-positive Y coordinates (Unity uses an upward Y axis).
`anchorMin == anchorMax` fixes an element to a point; different min/max
values stretch the element with its parent. `pivot` determines where the
rectangle grows from, `anchoredPosition` offsets that pivot and `sizeDelta`
adds to the anchor region's dimensions. The `TopLeft`, `TopRight`,
`BottomLeft`, `BottomRight`, `Center`, `Anchored` and `Stretch` helpers cover
common layouts, and each field can also be set directly.

`UICanvasScaleMode::ConstantPixelSize` (default) retains offsets and fixed
sizes in framebuffer pixels. `ScaleWithScreenSize` scales pixel offsets and
sizes based on the reference resolution; `matchWidthOrHeight` blends the width
and height ratios logarithmically (Unity Canvas Scaler behavior). Stretched
anchors still follow the actual parent dimensions and aspect ratio. Anchored
buttons, labels and checkboxes scale their rendered text using the same factor;
existing pixel-based calls do not change. Text scaling reuses the font's
cached texture rather than creating a new TTF font size.

For customized widgets or several children, call `UI->Resolve(layout, &parent)`
and pass the resulting `Rect` to any existing drawing or hit-test API. The
anchored overloads for buttons, labels, images, panels, image buttons,
checkboxes, sliders, toggles and progress bars use the exact same resolved rectangles for
rendering and mouse hit testing. This remains an immediate-mode system, not a
retained hierarchy or automatic flow/grid layout.

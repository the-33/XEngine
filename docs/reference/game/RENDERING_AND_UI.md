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

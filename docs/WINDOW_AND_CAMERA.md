# Window resolution, camera and screen-space UI

Window settings, camera projection and UI layout affect one another, but they
serve different purposes. This guide explains which coordinate system to use,
how to react to resolution changes and how XEngine handles fullscreen mode.

For the exact APIs, see `WindowManager`, `Camera2D`, `RenderManager` and
`UIManager` in the [Doxygen reference](API_REFERENCE.md).

## Configure the initial window

The generated game's `assets/config/engine.json` specifies the initial
window mode. For example:

```json
"Window": {
  "width": 1280,
  "height": 720,
  "fullscreen": false,
  "title": "My Game"
}
```

Edit this file for the Debug runtime. In a Release build, XEngine embeds the
runtime configuration into the executable, so rebuild after changing its
values. `config.json` in the game root controls the **build**, not the
runtime window size.

## Change resolution or fullscreen during gameplay

```cpp
#include <XEngine.h> // Exposes the Window convenience accessor and WindowManager

bool accepted = Window->SetSize(1600, 900);
if (!accepted) {
    // The requested dimensions were rejected.
}

Window->size = Vec2I{1280, 720}; // Property setter for windowed dimensions
Vec2I client = Window->size.get();
Vec2I framebuffer = Window->drawableSize.get();

Window->isFullscreen = true;
Window->size = Vec2I{1024, 768}; // Remembered windowed size while fullscreen
Window->isFullscreen = false;
```

`SetSize(width, height)` accepts positive logical dimensions up to 16384 on
each axis. It returns `false` for unsupported requests. In desktop fullscreen,
the requested **windowed** dimensions are retained for when fullscreen ends;
the current fullscreen drawable area is controlled by the display.

`WindowManager` exposes both methods and properties:

| API | Meaning |
|---|---|
| `SetSize(w, h)` | Request new windowed logical dimensions. |
| `GetSize()` / `Window->size` | Actual current logical SDL client-area dimensions. |
| `GetDrawableSize()` / `Window->drawableSize` | Framebuffer pixel dimensions (read-only). |
| `SetFullscreen(bool)` / `Window->isFullscreen` | Change or inspect desktop fullscreen. |
| `Window->windowedSize` | Remembered windowed dimensions (read-only). |
| `Window->resizeRevision` | Revision counter that increments when relevant window state changes. |
| `SetTitle(const char*)` | Update the window caption. |

**Logical coordinates and drawable pixels are not interchangeable.** On a
HiDPI display the framebuffer can contain more pixels than the logical client
area. XEngine's screen-space renderer (including `UIManager`) uses **drawable
pixels**, so lay out UI rectangles with `GetDrawableSize()`/
`Window->drawableSize`. `InputManager::GetMousePos()` returns logical window
coordinates; `UIManager` scales its hit testing to drawable pixels internally.

Apply SDL/window changes on the main thread, where the engine owns its event
processing and renderer. Let XEngine process resize events; do not try to
force the previous size every frame.

## Detect a changed window size

Save the revision you last laid out for and recalculate layout when it changes:

```cpp
std::uint64_t previousRevision = Window->resizeRevision.get();

// During a later gameplay frame:
if (Window->resizeRevision.get() != previousRevision) {
    previousRevision = Window->resizeRevision.get();
    Vec2I currentSize = Window->size.get();
    // Recompute or cache your new UI rectangles from currentSize.
}
```

The revision is a **change indicator**; it is not a display size or a
save-game version. If you draw UI every frame, it is equally valid to compute
the desired rectangle directly from the latest size without caching.

## Work in world space with Camera2D

World-space positions belong to your gameplay and `Transform` values.
`Camera2D` converts between world and screen coordinates and determines the
visible world rectangle. The active scene owns its camera:

```cpp
#include <XEngine.h> // CurrentCamera is an XEngine convenience accessor

Camera2D* camera = CurrentCamera;
if (camera) {
    camera->center = Vec2{0.f, 0.f};
    camera->zoom = 1.5f;
    Vec2 pixel = camera->WorldToScreen(Vec2{3.f, 2.f});
    Vec2 world = camera->ScreenToWorld(pixel);
}
```

`CurrentCamera` is only meaningful when an active scene has a camera. Avoid
keeping a pointer to a camera across scene changes unless you know its
lifetime. **During a scene-builder callback the new scene is not yet active**:
`CurrentCamera` returns `nullptr` there, so use the callback's `Scene*` and
`scene->camera.get()` to configure its camera. The shortcut also returns null
before initialization and during scene transitions or shutdown.
To follow a target:

```cpp
camera->Follow(targetWorldPosition, 5.f, dt);
```

Here `dt` is elapsed **seconds**; it must be provided for time-dependent
smoothing. `ClampToWorld(Rect)` restricts the camera to configured world bounds.

### Aspect ratio and zoom

`Camera2D::matchWindowAspect` is a writable `Property<bool>`, enabled by
default. XEngine keeps the vertical world-view span fixed and adjusts the
horizontal span to the current window aspect ratio. That means changing
between 16:9 and 4:3 does not stretch world geometry.

```cpp
camera->matchWindowAspect = true;       // Default behavior
camera->viewBase = Vec2{20.f, 11.25f};  // Base world-view dimensions
camera->zoom = 1.f;
Rect visible = camera->viewRect.get();
```

Set `matchWindowAspect = false` only if your game deliberately needs a fixed
view rectangle independent of the window proportions. The renderer continues
using the selected SDL backend; changing size or aspect does not switch
rendering backends.

## Screen-space UI

`UIManager` is immediate-mode: each frame your code submits controls at their
current positions. There is no retained automatic layout tree. Legacy `Rect`
widgets use explicit drawable pixels; a button placed at a fixed screen
position can be clipped by a smaller window unless you update its rectangle.
For new interfaces, use `UIRectTransform` to anchor elements to the drawable
canvas or a parent rectangle without manually observing resize events.

For example, inside a gameplay `Behaviour::Render()` callback:

```cpp
Vec2I size = Window->drawableSize.get();
const Rect centeredButton{
    size.x * 0.5f - 100.f,
    size.y * 0.5f - 20.f,
    200.f,
    40.f
};
if (UI->Button("main-menu-start", centeredButton, "Start")) {
    // Start gameplay or request a scene change.
}
```

With anchors the same button becomes:

```cpp
UIRectTransform centered = UIRectTransform::Center({200.f, 40.f});
if (UI->Button("main-menu-start", centered, "Start")) {
    // The center stays centered at any size or aspect ratio.
}
```

To scale button and image sizes, offsets and anchored text alongside the
display resolution, configure the optional canvas scaler once:

```cpp
UICanvas canvas;
canvas.scaleMode = UICanvasScaleMode::ScaleWithScreenSize;
canvas.referenceResolution = {1920.f, 1080.f};
canvas.matchWidthOrHeight = 0.5f;
UI->SetCanvas(canvas);

// Anchor 16 scaled pixels from a panel's bottom-right corner.
Rect panel = UI->Resolve(UIRectTransform::Stretch(30.f, 30.f, 30.f, 30.f));
UI->Button("close", UIRectTransform::BottomRight({120.f, 40.f}, {16.f, 16.f}),
           "Close", &panel);
```

Normalized anchors use (0,0) at the **top-left** and (1,1) at the
**bottom-right** (unlike Unity's upward-positive Y). Fixed anchors follow
the chosen edge/center; separated anchorMin/anchorMax stretch with the parent.
The reference-resolution scaler defaults to off, preserving old games.
See [Sprites, window, camera and screen-space UI](reference/game/RENDERING_AND_UI.md#unity-style-anchors-and-canvas-scaling)
for the full reference and an example with a parent panel.

Use stable, distinct widget IDs. `UI->Button`, `UI->Label`, `UI->Panel` and
other UI calls use screen-space rectangles rather than world-space camera
coordinates. Camera movement should not affect the position of a HUD control.

For text and images, be consistent about whether you call world-space
`RenderManager` methods or their explicit screen-space counterparts. A sprite
attached to a world `GameObject` and a fixed HUD image do **not** share the
same coordinate system.

## Troubleshooting

| Symptom | Check |
|---|---|
| UI appears tiny or offset on HiDPI | Use drawable `Window->drawableSize` for UI rectangles; `UIManager` converts mouse coordinates automatically. |
| UI falls off the edge after resize | Use `UIRectTransform` anchored elements, or recalculate legacy pixel rectangles from the drawable size. |
| Fullscreen changes the apparent window size | Desktop fullscreen uses the display dimensions; check `windowedSize` for the stored windowed resolution. |
| World geometry appears squashed at a new aspect | Check `camera->matchWindowAspect` and your world-space vs screen-space drawing path. |
| A camera pointer stops working after a scene change | Obtain the current scene's camera again; do not hold a stale non-owning pointer. |

Continue with [getting started](GETTING_STARTED.md) or the
[Doxygen class reference](API_REFERENCE.md) for the full set of overloads.

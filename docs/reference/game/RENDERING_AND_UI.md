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

`UIManager` renders text, panels, buttons and image controls. Draw them from
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
checkboxes and progress bars use the exact same resolved rectangles for
rendering and mouse hit testing. This remains an immediate-mode system, not a
retained hierarchy or automatic flow/grid layout.

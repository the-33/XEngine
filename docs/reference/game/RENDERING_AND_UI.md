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

Logical window dimensions are used for ordinary UI positioning; drawable
dimensions can differ on HiDPI displays. `Window->resizeRevision` changes
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
current `Window->size` rather than hardcoding coordinates for 1280x720.

## Immediate-mode controls

`UIManager` renders text, panels, buttons and image controls. Draw them from
your `Behaviour::Render()` or equivalent gameplay rendering hook. Button IDs
must be stable and unique for each interactive control, and the rectangle
must be computed in screen coordinates.

```cpp
Vec2I size = Window->size.get();
Rect action{static_cast<float>(size.x) - 180.f, 20.f, 160.f, 42.f};
if (UI->Button("pause_button", action, "Pause")) {
    Time->timeScale = 0.f;
}
```

SDL window and render calls should stay on the main engine thread. Game code
controls its own interface's responsive layout; XEngine does not automatically
rearrange arbitrary UI widgets after a resize.

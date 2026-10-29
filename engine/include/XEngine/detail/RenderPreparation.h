#pragma once
/** @file RenderPreparation.h
 * @brief Camera-space projection and conservative rotated-rectangle culling.
 */
// Pure CPU camera projection and conservative rotated-rectangle culling.
// No SDL calls or access to live Scene / Texture / Camera objects.
#include <algorithm>
#include <cmath>
#include <cstddef>

/** @brief CPU-only rendering preparation with no SDL or live-scene access. */
namespace xengine::detail::render_preparation
{
struct Rect { float x, y, w, h; };
struct Camera { float centerX, centerY, baseW, baseH, zoom; int width, height; };
struct Prepared { Rect screen{}; float pivotX = 0.f, pivotY = 0.f; bool visible = false; };

inline bool Intersects(Rect a, Rect b) noexcept
{
    return a.x < b.x + b.w && a.x + a.w > b.x &&
           a.y < b.y + b.h && a.y + a.h > b.y;
}

/** @brief Projects a world rectangle and checks visibility against drawable bounds.
 * @param cam Immutable camera snapshot, with dimensions measured in drawable pixels.
 * @param world World rectangle before screen projection.
 * @param rotationDeg Rotation in degrees around the normalized pivot.
 * @param pivotX Normalized horizontal pivot within the sprite rectangle.
 * @param pivotY Normalized vertical pivot within the sprite rectangle.
 * @return Screen-space geometry and visibility; invalid dimensions are invisible.
 */
inline Prepared Prepare(const Camera& cam, Rect world, float rotationDeg,
                        float pivotX, float pivotY) noexcept
{
    Prepared out;
    if (!(cam.width > 0 && cam.height > 0 && cam.baseW > 0.f &&
          cam.baseH > 0.f && cam.zoom > 0.f)) return out;
    const float viewW = cam.baseW / cam.zoom;
    const float viewH = cam.baseH / cam.zoom;
    const float left = cam.centerX - viewW * .5f;
    const float top = cam.centerY - viewH * .5f;
    const float x1 = ((world.x - left) / viewW) * cam.width;
    const float y1 = ((world.y - top) / viewH) * cam.height;
    const float x2 = ((world.x + world.w - left) / viewW) * cam.width;
    const float y2 = ((world.y + world.h - top) / viewH) * cam.height;
    out.screen = {std::min(x1, x2), std::min(y1, y2),
                  std::fabs(x2 - x1), std::fabs(y2 - y1)};
    out.pivotX = out.screen.w * pivotX;
    out.pivotY = out.screen.h * pivotY;
    if (!(out.screen.w > 0.f && out.screen.h > 0.f)) return out;
    Rect bounds = out.screen;
    if (rotationDeg != 0.f)
    {
        const float rad = rotationDeg * 3.14159265358979323846f / 180.f;
        const float c = std::cos(rad), s = std::sin(rad);
        const float cx = bounds.x + out.pivotX;
        const float cy = bounds.y + out.pivotY;
        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        const float xs[4] = {-out.pivotX, bounds.w - out.pivotX,
                             bounds.w - out.pivotX, -out.pivotX};
        const float ys[4] = {-out.pivotY, -out.pivotY,
                             bounds.h - out.pivotY, bounds.h - out.pivotY};
        for (int i = 0; i < 4; ++i)
        {
            const float x = cx + xs[i] * c - ys[i] * s;
            const float y = cy + xs[i] * s + ys[i] * c;
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
        }
        bounds = {minX, minY, maxX - minX, maxY - minY};
    }
    out.visible = Intersects(bounds, {0.f, 0.f, (float)cam.width, (float)cam.height});
    return out;
}
} // namespace RenderPreparation

// Compatibility namespace: existing engine code and internal tests may still
// include the historical forwarding header and use its original spelling.
namespace RenderPreparation = xengine::detail::render_preparation;

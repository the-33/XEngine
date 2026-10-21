#pragma once

// CPU-only sprite geometry. Plain input values are copied from the scene on
// the main thread; this function can run on ParallelExecutor workers without
// touching Transform, Texture, SDL, or any mutable scene object.
#include <algorithm>
#include <cmath>

namespace SpriteProjection {

struct Pose {
    float x = 0.f, y = 0.f;
    float scaleX = 1.f, scaleY = 1.f;
    float rotationX = 0.f, rotationY = 0.f, rotationZ = 0.f;
    float width = 0.f, height = 0.f; // Source region / pixels-per-unit
    float offsetX = 0.f, offsetY = 0.f;
    float pivotX = .5f, pivotY = .5f;
};
struct Rect { float x, y, w, h; };

inline Rect Project(const Pose& pose) noexcept {
    constexpr float radiansPerDegree = 3.14159265358979323846f / 180.f;
    // Cos(0) == 1. Skip two transcendentals for typical un-tilted sprites.
    // Keep the original clamp for rotations that go past 90 degrees.
    const float xFactor = pose.rotationY == 0.f ? 1.f :
        std::max(0.f, std::cos(pose.rotationY * radiansPerDegree));
    const float yFactor = pose.rotationX == 0.f ? 1.f :
        std::max(0.f, std::cos(pose.rotationX * radiansPerDegree));
    const float worldW = pose.width * (pose.scaleX * xFactor);
    const float worldH = pose.height * (pose.scaleY * yFactor);

    // Without a local offset, the Z angle need not be evaluated here. Z
    // still reaches RenderPreparation / SDL for rotation about the pivot.
    float offRotX = 0.f, offRotY = 0.f;
    if (pose.offsetX != 0.f || pose.offsetY != 0.f) {
        const float angleZ = pose.rotationZ * radiansPerDegree;
        const float offX = pose.offsetX * pose.scaleX;
        const float offY = pose.offsetY * pose.scaleY;
        if (pose.rotationZ == 0.f) {
            offRotX = offX;
            offRotY = offY;
        } else {
            const float cosZ = std::cos(angleZ), sinZ = std::sin(angleZ);
            offRotX = offX * cosZ - offY * sinZ;
            offRotY = offX * sinZ + offY * cosZ;
        }
    }
    return {pose.x + offRotX - worldW * pose.pivotX,
            pose.y + offRotY - worldH * pose.pivotY,
            worldW, worldH};
}
} // namespace SpriteProjection

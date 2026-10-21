#pragma once
#include <cmath>

namespace XEngineBenchmark {
    // Pure, deterministic Euler angles for the RenderManager pseudo-3D path.
    // X and Y compress sprite height/width through cosine; Z rotates in-screen.
    // Groups 0..4 are X-only, Y-only, Z-only, XYZ and XY respectively.
    // Keep X/Y within (-90, 90) so neither drawing path receives a
    // degenerate zero-sized rectangle at exactly 90 degrees.
    inline Vec3 BenchmarkAxisRotation(unsigned index, unsigned frame) noexcept {
        constexpr float halfPi = 1.57079632679489661923f;
        const float phase = 0.0185f * static_cast<float>(frame) +
                            0.097f * static_cast<float>(index);
        const float nearEdge = (index / 5u) % 11u == 0u ? 89.f : 78.f;
        const float x = nearEdge * std::sin(phase + halfPi);
        const float y = nearEdge * std::cos(phase * 0.71f);
        const float z = std::fmod(47.f * static_cast<float>(index % 360u) +
                                  0.91f * static_cast<float>(frame), 360.f);
        switch (index % 5u) {
            case 0u: return Vec3{x, 0.f, 0.f}; // X only
            case 1u: return Vec3{0.f, y, 0.f}; // Y only
            case 2u: return Vec3{0.f, 0.f, z}; // Z only
            case 3u: return Vec3{x, y, z};    // All three axes
            default: return Vec3{x, y, 0.f}; // Combined XY tilt
        }
    }
}

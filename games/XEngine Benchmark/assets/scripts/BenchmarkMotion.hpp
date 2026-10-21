#pragma once
#include <XEngine.h>
#include <cmath>

namespace XEngineBenchmark {
    class BenchmarkMotion final : public Behaviour {
        Vec3 center_{};
        float phase_ = 0;
        unsigned frame_ = 0;
    public:
        explicit BenchmarkMotion(Vec3 center, float phase) : center_(center), phase_(phase) {}
        void Update(float) override {
            // Frame-indexed deterministic workload independent of wall-clock FPS.
            ++frame_;
            const float a = 0.023f * static_cast<float>(frame_) + phase_;
            if (Transform* t = gameObject->transform.get())
                t->position = Vec3{center_.x + 0.11f * std::sin(a),
                                   center_.y + 0.09f * std::cos(a * 0.83f), center_.z};
        }
    };
}

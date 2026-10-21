#pragma once
#include <XEngine.h>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace XEngineBenchmark {
    // World-space layouts change at a fixed frame count, never by elapsed
    // seconds or by the selected broadphase. The four spatial anchors keep
    // global scene bounds similar while the remaining colliders concentrate.
    inline unsigned LayoutPhase(const std::string& layout, unsigned frame,
                                unsigned phaseFrames) noexcept {
        if (layout == "clustered") return 1;
        if (layout == "spread") return 2;
        if (layout == "alternating") return (frame / phaseFrames) % 2;
        if (layout == "cycling") return (frame / phaseFrames) % 3;
        return 0;
    }

    class BenchmarkDistribution final : public Behaviour {
        std::vector<std::pair<GameObject*, Vec3>> objects_;
        std::string layout_;
        unsigned period_ = 120;
        unsigned frame_ = 0;
        unsigned previousPhase_ = 3;

        static Vec3 Position(unsigned phase, std::size_t index, const Vec3& home) noexcept {
            if (phase == 0) return home;
            // Keep the bounding extent comparable to the original lattice.
            // Most objects cluster into a dense region in phase 1 and into
            // two distant dense regions in phase 2. Positions are independent
            // of the OS scheduler and of the number of physics steps taken.
            if (index < 4) {
                constexpr float x[4] = {-9.8f, 9.8f, -9.8f, 9.8f};
                constexpr float y[4] = {-5.5f, -5.5f, 5.5f, 5.5f};
                return {x[index], y[index], home.z};
            }
            const std::size_t i = index - 4;
            if (phase == 1) {
                return {-0.6f + float(i % 32) * 0.040f,
                        -0.6f + float((i / 32) % 32) * 0.040f, home.z};
            }
            const float cx = (i % 2) ? 6.5f : -6.5f;
            return {cx + float((i / 2) % 18) * 0.065f,
                    -0.75f + float((i / 36) % 24) * 0.065f, home.z};
        }
    public:
        BenchmarkDistribution(std::vector<std::pair<GameObject*, Vec3>> objects,
                              std::string layout, unsigned phaseFrames)
            : objects_(std::move(objects)), layout_(std::move(layout)),
              period_(phaseFrames) {}

        void Update(float) override {
            const unsigned phase = LayoutPhase(layout_, frame_++, period_);
            if (phase == previousPhase_) return;
            previousPhase_ = phase;
            for (std::size_t i = 0; i < objects_.size(); ++i) {
                if (objects_[i].first) {
                    if (Transform* t = objects_[i].first->transform.get())
                        t->position = Position(phase, i, objects_[i].second);
                }
            }
        }
    };
}

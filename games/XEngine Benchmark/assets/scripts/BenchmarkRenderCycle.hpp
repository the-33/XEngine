#pragma once
#include <XEngine.h>
#include <algorithm>
#include <array>
#include <vector>
#include <utility>

namespace XEngineBenchmark {
// Enables 55%, 75%, 100%, 65% of visual-only objects in repeated phases.
// The controller stays active; inactive children do not need to Update().
class BenchmarkRenderCycle final : public Behaviour {
    std::vector<GameObject*> objects_;
    unsigned phaseFrames_;
    unsigned frame_ = 0;
    unsigned lastActive_ = ~0u;
public:
    BenchmarkRenderCycle(std::vector<GameObject*> objects, unsigned phaseFrames)
      : objects_(std::move(objects)), phaseFrames_(phaseFrames) {}
    void Update(float) override {
        constexpr std::array<unsigned, 4> percentages{55u, 75u, 100u, 65u};
        const unsigned phase = (frame_++ / phaseFrames_) % percentages.size();
        const unsigned active = static_cast<unsigned>(objects_.size() * percentages[phase] / 100u);
        if (active == lastActive_) return;
        for (unsigned i = 0; i < objects_.size(); ++i)
            objects_[i]->SetActive(i < active);
        lastActive_ = active;
    }
};
} // namespace XEngineBenchmark

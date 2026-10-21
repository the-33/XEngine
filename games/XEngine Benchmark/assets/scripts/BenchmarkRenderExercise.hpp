#pragma once
#include <XEngine.h>
#include "BenchmarkAxisRotations.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace XEngineBenchmark {
    // Deterministic visual workload; mutable render state stays on the main thread.
    // Only the CPU projection/culling snapshot may run on the worker pool.
    class BenchmarkRenderAnimation final : public Behaviour {
        SpriteRenderer* sprite_;
        unsigned index_;
        unsigned frame_ = 0;
        Vec3 center_;
        bool transform_, tint_, culling_, axisRotation_;
    public:
        BenchmarkRenderAnimation(SpriteRenderer* sprite, unsigned index, Vec3 center,
                                 bool transform, bool tint, bool culling, bool axisRotation = false)
            : sprite_(sprite), index_(index), center_(center),
              transform_(transform), tint_(tint), culling_(culling), axisRotation_(axisRotation) {}
        void Update(float) override {
            ++frame_;
            const float step = 0.031f * float(frame_) + 0.119f * float(index_);
            if (transform_) {
                gameObject->transform->localRotation = axisRotation_
                    ? BenchmarkAxisRotation(index_, frame_)
                    : Vec3{0.f, 0.f, std::fmod(float(index_ * 47u) +
                        float(frame_) * (0.7f + float(index_ % 4u)), 360.f)};
                sprite_->pivot01 = Vec2{0.5f + 0.35f * std::sin(step),
                                        0.5f + 0.35f * std::cos(step)};
                sprite_->flipX = ((frame_ / 37u + index_) % 2u) == 0u;
            }
            if (tint_) {
                sprite_->tint = Color{
                    static_cast<std::uint8_t>(40u + ((index_ * 31u + frame_) % 216u)),
                    static_cast<std::uint8_t>(30u + ((index_ * 59u + frame_ * 2u) % 226u)),
                    static_cast<std::uint8_t>(55u + ((index_ * 79u + frame_ * 3u) % 201u)),
                    static_cast<std::uint8_t>(48u + ((index_ * 37u + frame_) % 208u))};
            }
            if (culling_) {
                gameObject->transform->position = Vec3{
                    center_.x + 0.18f * std::sin(step), center_.y + 0.13f * std::cos(step), center_.z};
            }
        }
    };

    // Exercises additional RenderManager APIs while preserving a pure sprite
    // baseline for threshold crossover estimates.
    class BenchmarkRenderExercise final : public Behaviour {
        Scene* scene_;
        unsigned spriteCount_;
        const Texture* image_;
        const Texture* second_;
        const Font* font_;
        unsigned frame_ = 0;
    public:
        BenchmarkRenderExercise(Scene* scene, unsigned sprites, const Texture* image,
                                const Texture* second, const Font* font)
            : scene_(scene), spriteCount_(sprites), image_(image), second_(second), font_(font) {}
        void Update(float) override {
            ++frame_;
            if (scene_ && scene_->camera.get()) {
                scene_->camera->center = Vec2{0.13f * std::sin(0.017f * float(frame_)),
                                               0.10f * std::cos(0.013f * float(frame_))};
                scene_->camera->zoom = 1.f + 0.10f * std::sin(0.009f * float(frame_));
            }
        }
        void Render() override {
            // Keep extra world commands proportional to the requested sprite count.
            // Include rotations, lines, filled/outlined rectangles and transparency.
            const unsigned count = std::max(1u, spriteCount_ / 20u);
            for (unsigned i = 0; i < count; ++i) {
                const float x = -9.6f + float(i % 48u) * 0.4f;
                const float y = -5.25f + float((i / 48u) % 24u) * 0.38f;
                const Color color{static_cast<std::uint8_t>((i * 29u + frame_) % 256u),
                                  static_cast<std::uint8_t>((i * 53u) % 256u),
                                  static_cast<std::uint8_t>((i * 71u) % 256u), 136u};
                if (i % 3u == 0u)
                    Renderer->DrawRect(Rect{x, y, 0.18f, 0.22f}, float((i * 17u + frame_) % 360u),
                                       Vec2{0.2f, 0.8f}, color, true);
                else if (i % 3u == 1u)
                    Renderer->DrawRect(Rect{x, y, 0.22f, 0.16f}, float((i * 19u) % 360u),
                                       Vec2{0.5f, 0.5f}, color, false);
                else
                    Renderer->DrawLine(x, y, x + 0.27f, y + 0.20f, color);
            }
            Renderer->DrawRectScreen(Rect{12.f, 12.f, 48.f, 18.f}, Color{160, 80, 220, 180}, true);
            if (image_)
                Renderer->DrawImageScreen(*image_, Rect{65.f, 12.f, 28.f, 28.f}, nullptr,
                                         Color{180, 220, 120, 140}, true, false);
            if (second_)
                Renderer->DrawImageScreen(*second_, Rect{96.f, 12.f, 28.f, 28.f}, nullptr,
                                         Color{80, 160, 255, 220}, false, true);
            if (font_)
                Renderer->DrawTextScreen(*font_, "Render workload", 130.f, 12.f, Color::White());
        }
    };
}

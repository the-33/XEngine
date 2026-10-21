#pragma once
#include <XEngine.h>
#include "BenchmarkMotion.hpp"
#include "BenchmarkDistribution.hpp"
#include "BenchmarkRenderExercise.hpp"
#include "BenchmarkAxisRotations.hpp"
#include "BenchmarkRenderCycle.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <random>
#include <string>

namespace XEngineBenchmark {
    struct Options {
        std::string scenario = "balanced";
        unsigned frames = 360;
        unsigned warmup = 90;
        unsigned seed = 12345;
        int colliderOverride = -1; // -1 = scenario default
        int spriteOverride = -1; // -1 = scenario default
        std::string layout = "static";
        std::string renderWorkload = "baseline";
        bool renderCycle = false;
        unsigned phaseFrames = 120;
    };
    struct StateData {
        Options options{};
        unsigned frame = 0;
        bool quitRequested = false;
    };
    inline StateData& State() { static StateData state; return state; }
    inline void SetOptions(const Options& opts) { State() = StateData{opts, 0, false}; }
    inline bool IsKnownScenario(const std::string& scenario) {
        return scenario == "light" || scenario == "render" || scenario == "collision" ||
               scenario == "balanced" || scenario == "stress";
    }
    struct Workload {
        unsigned sprites;
        unsigned colliders;
        unsigned bodies;
        unsigned moving;
    };
    inline bool IsKnownLayout(const std::string& layout) {
        return layout == "static" || layout == "clustered" || layout == "spread" ||
               layout == "alternating" || layout == "cycling";
    }
    inline bool IsKnownRenderWorkload(const std::string& workload) {
        return workload == "baseline" || workload == "transforms" ||
               workload == "tint" || workload == "culling" ||
               workload == "mixed" || workload == "rotations_xyz";
    }
    inline Workload GetWorkload(const std::string& scenario) {
        if (scenario == "light") return {120, 64, 12, 30};
        if (scenario == "render") return {2200, 0, 0, 240};
        if (scenario == "collision") return {220, 740, 0, 160};
        if (scenario == "stress") return {2600, 1050, 240, 320};
        return {1300, 520, 96, 220};
    }
    inline void BuildBenchmarkScene(Scene* scene) {
        if (!scene) return;
        const auto& opts = State().options;
        Workload load = GetWorkload(opts.scenario);
        if (opts.spriteOverride >= 0) load.sprites = static_cast<unsigned>(opts.spriteOverride);
        if (opts.colliderOverride >= 0) {
            load.colliders = static_cast<unsigned>(opts.colliderOverride);
            load.bodies = std::min(load.bodies, load.colliders);
        }
        std::mt19937 rng(opts.seed);
        std::uniform_real_distribution<float> jitter(-0.045f, 0.045f);
        std::array<const Texture*, 4> textures{
            Assets->GetTextureByKey("benchmark_circle"),
            Assets->GetTextureByKey("benchmark_square"),
            Assets->GetTextureByKey("benchmark_diamond"),
            Assets->GetTextureByKey("benchmark_triangle")
        };
        if (Camera2D* camera = scene->camera.get()) {
            camera->center = Vec2{0.f, 0.f};
            camera->viewBase = Vec2{22.f, 12.375f};
        }
        const bool stressRender = opts.renderWorkload != "baseline";
        const bool axisRotation = opts.renderWorkload == "rotations_xyz";
        const bool transformations = opts.renderWorkload == "transforms" || opts.renderWorkload == "mixed" || axisRotation;
        const bool tint = opts.renderWorkload == "tint" || opts.renderWorkload == "mixed";
        const bool culling = opts.renderWorkload == "culling" || opts.renderWorkload == "mixed";
        const unsigned all = std::max(load.sprites, load.colliders);
        std::vector<GameObject*> renderCycleObjects;
        if (opts.renderCycle) renderCycleObjects.reserve(load.sprites);
        std::vector<std::pair<GameObject*, Vec3>> colliderObjects;
        if (opts.layout != "static") colliderObjects.reserve(load.colliders);
        for (unsigned i = 0; i < all; ++i) {
            // Colliders use a regular overlapping lattice to stress broadphase;
            // visual-only sprites fill the same viewport on a denser lattice.
            const bool hasCollision = i < load.colliders;
            const float x = hasCollision
                ? -9.8f + static_cast<float>(i % 42) * 0.47f
                : -10.f + static_cast<float>(i % 94) * 0.214f;
            const float y = hasCollision
                ? -5.6f + static_cast<float>((i / 42) % 24) * 0.47f
                : -5.4f + static_cast<float>((i / 94) % 52) * 0.21f;
            Vec3 position{x + jitter(rng), y + jitter(rng), hasCollision ? 1.f : 0.f};
            if (culling && i < load.sprites && !hasCollision) {
                // A fixed fraction lies beyond the camera or clips its edges.
                // Never relocate physics participants just to stress culling.
                if (i % 5 == 0) position.x += 24.f;
                else if (i % 7 == 0) position.y -= 14.f;
                else if (i % 11 == 0) position.x = 10.86f;
            }
            GameObject* object = scene->CreateObject(hasCollision ? "Collider" : "Sprite");
            object->transform->position = position;
            if (opts.renderCycle && i < load.sprites && !hasCollision)
                renderCycleObjects.push_back(object);
            if (i < load.sprites) {
                const Texture* tex = textures[i % textures.size()];
                if (tex) {
                    auto* sprite = object->AddComponent<SpriteRenderer>();
                    sprite->sprite = tex;
                    sprite->source = RectI{0, 0, tex->Width(), tex->Height()};
                    object->transform->localScale = transformations
                        ? Vec2{0.18f + 0.055f * float(i % 5), 0.16f + 0.047f * float(i % 7)}
                        : Vec2{0.28f, 0.28f};
                    if (transformations) {
                        object->transform->localRotation = axisRotation
                            ? BenchmarkAxisRotation(i, 0u)
                            : Vec3{0.f, 0.f, float((i * 47) % 360)};
                        sprite->pivot01 = Vec2{0.1f + 0.2f * float(i % 5), 0.1f + 0.2f * float((i / 5) % 5)};
                        sprite->offset = Vec2{0.025f * float(int(i % 3) - 1), 0.03f * float(int(i % 5) - 2)};
                        sprite->flipX = (i % 3 == 0);
                        sprite->flipY = (i % 7 == 0);
                        if (i % 17 == 0) {
                            sprite->source = RectI{0, 0, std::max(1, tex->Width() / 2),
                                                          std::max(1, tex->Height() / 2)};
                        }
                    }
                    if (tint) {
                        sprite->tint = Color{static_cast<std::uint8_t>(45 + (i * 31u) % 211u),
                                             static_cast<std::uint8_t>(60 + (i * 59u) % 196u),
                                             static_cast<std::uint8_t>(38 + (i * 79u) % 218u),
                                             static_cast<std::uint8_t>(72 + (i * 37u) % 184u)};
                    }
                    if (stressRender && !hasCollision && i % 4 == 0)
                        object->AddComponent<BenchmarkRenderAnimation>(sprite, i, position,
                            transformations, tint, culling, axisRotation);
                }
            }
            if (hasCollision) {
                if (opts.layout != "static") colliderObjects.emplace_back(object, position);
                auto* col = object->AddComponent<Collider2D>();
                col->shape = (i % 3 == 0) ? Collider2D::Shape::Circle : Collider2D::Shape::Box;
                // Local units must account for the sprite's Transform scale.
                const float factor = (i < load.sprites) ? (1.f / 0.28f) : 1.f;
                col->size = Vec2{0.60f * factor, 0.60f * factor};
                col->radius = 0.30f * factor;
                col->isTrigger = true; // Detect without physical contact-solving distortions.
                col->showGizmo = false;
                if (i < load.bodies) {
                    auto* body = object->AddComponent<RigidBody2D>();
                    body->bodyType = RigidBody2D::BodyType::Kinematic;
                    body->gravityScale = 0.f;
                    body->velocity = Vec2{0.008f * float(i % 3), -0.006f * float(i % 2)};
                }
            }
            // Motion is excluded from physics-controlled objects.
            if (!stressRender && i >= load.bodies && i < load.moving)
                object->AddComponent<BenchmarkMotion>(position, 0.121f * float(i));
        }
        if (opts.renderCycle) {
            auto* cycle = scene->CreateObject("BenchmarkRenderCycle");
            cycle->AddComponent<BenchmarkRenderCycle>(std::move(renderCycleObjects), opts.phaseFrames);
        }
        if (opts.renderWorkload == "mixed") {
            auto* renderExercise = scene->CreateObject("BenchmarkRenderExercise");
            renderExercise->AddComponent<BenchmarkRenderExercise>(scene, load.sprites,
                textures[0], textures[1], Assets->GetFontByKey("benchmark_ui"));
        }
        if (opts.layout != "static") {
            auto* controller = scene->CreateObject("BenchmarkDistribution");
            controller->AddComponent<BenchmarkDistribution>(
                std::move(colliderObjects), opts.layout, opts.phaseFrames);
        }
        std::printf("[BENCH] built: sprites=%u colliders=%u bodies=%u animated=%u seed=%u layout=%s phase_frames=%u render_workload=%s\n",
                    load.sprites, load.colliders, load.bodies,
                    load.moving > load.bodies ? load.moving - load.bodies : 0u, opts.seed,
                    opts.layout.c_str(), opts.phaseFrames, opts.renderWorkload.c_str());
    }
}

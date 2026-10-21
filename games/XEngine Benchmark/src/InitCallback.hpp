#pragma once
#include <XEngine.h>
#include <SDL.h>
#include "BenchmarkScene.hpp"
#include <cstdio>

namespace XEngineBenchmark {
    inline bool LoadAssets() {
        bool loaded = true;
        loaded &= Assets->LoadTexture("textures/circle.png", "benchmark_circle", 40.f) != nullptr;
        loaded &= Assets->LoadTexture("textures/square.png", "benchmark_square", 40.f) != nullptr;
        loaded &= Assets->LoadTexture("textures/diamond.png", "benchmark_diamond", 40.f) != nullptr;
        loaded &= Assets->LoadTexture("textures/triangle.png", "benchmark_triangle", 40.f) != nullptr;
        if (State().options.renderWorkload == "mixed") {
            loaded &= Assets->LoadFont("fonts/ui-default.ttf", "benchmark_ui", 16) != nullptr;
        }
        return loaded;
    }
    inline void InitGame(Engine&) {
        if (!LoadAssets()) {
            std::fputs("[BENCH] Failed to load benchmark textures\n", stderr);
            SDL_Event e{}; e.type = SDL_QUIT; SDL_PushEvent(&e);
            return;
        }
        if (State().options.renderWorkload == "mixed") {
            Renderer->SetBackground(Assets->GetTextureByKey("benchmark_square"),
                RenderManager::BackgroundMode::RepeatScaled, Color{130, 130, 170, 200});
        }
        Scenes->Register("benchmark", BuildBenchmarkScene);
        Scenes->SetActive("benchmark");
    }
    inline void CountBenchmarkFrame(float) {
        auto& state = State();
        if (++state.frame >= state.options.frames + state.options.warmup && !state.quitRequested) {
            state.quitRequested = true;
            // Delivered on the next PollInput; engine closes normally and
            // writes its full profile CSV during loop shutdown.
            SDL_Event event{};
            event.type = SDL_QUIT;
            SDL_PushEvent(&event);
        }
    }
}

#include <XEngine.h>
#include "GameBuild.generated.h"
#include "InitCallback.hpp"
#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <stdexcept>

namespace {
    bool parseUnsigned(const char* value, unsigned& output) {
        try {
            const std::string input(value);
            if (input.empty() || input[0] == '-') return false;
            std::size_t used = 0;
            const auto n = std::stoull(input, &used);
            if (used != input.size() || n > 1000000) return false;
            output = static_cast<unsigned>(n);
            return true;
        } catch (...) { return false; }
    }
    void usage() {
        std::puts("XEngine Benchmark --scenario light|render|collision|balanced|stress "
                  "--mode single|collision|parallel [--frames N] [--warmup N] [--seed N] "
                  "[--timing realtime|deterministic] [--fixed-steps N (1..16)] "
                  "[--broadphase bruteforce|grid|auto] [--cell-size N|auto] [--colliders N] "
                  "[--layout static|clustered|spread|alternating|cycling] [--phase-frames N] "
                  "[--sprites N] [--render-threshold N] [--render-selection fixed|adaptive] [--render-cycle 0|1] [--sprite-projection main|workers] [--render-workload baseline|transforms|tint|culling|mixed|rotations_xyz]");
    }
}

int main(int argc, char** argv) {
    using namespace XEngineBenchmark;
    Options opts;
    std::string mode = "parallel";
    std::string timing = "realtime";
    unsigned fixedSteps = 1;
    unsigned renderThreshold = RenderDiagnostics::kParallelThreshold;
    std::string spriteProjection = "workers";
    std::string renderSelection = "fixed";
    std::string broadphase;
    float cellSize = 2.f;
    bool autoCellSize = false;
    bool broadphaseOverride = false;
    bool cellSizeOverride = false;
    for (int i = 1; i < argc; ++i) {
        const std::string key(argv[i]);
        if (key == "--help" || key == "-h") { usage(); return 0; }
        if (i + 1 >= argc) { usage(); return 2; }
        const char* value = argv[++i];
        if (key == "--scenario") opts.scenario = value;
        else if (key == "--mode") mode = value;
        else if (key == "--frames") { if (!parseUnsigned(value, opts.frames)) return 2; }
        else if (key == "--warmup") { if (!parseUnsigned(value, opts.warmup)) return 2; }
        else if (key == "--seed") { if (!parseUnsigned(value, opts.seed)) return 2; }
        else if (key == "--colliders") { unsigned count; if (!parseUnsigned(value, count) || count > 3000) return 2; opts.colliderOverride = static_cast<int>(count); }
        else if (key == "--sprites") { unsigned count; if (!parseUnsigned(value, count) || count > 10000) return 2; opts.spriteOverride = static_cast<int>(count); }
        else if (key == "--render-workload") opts.renderWorkload = value;
        else if (key == "--render-selection") renderSelection = value;
        else if (key == "--render-cycle") { unsigned flag; if (!parseUnsigned(value, flag) || flag > 1) return 2; opts.renderCycle = flag != 0; }
        else if (key == "--sprite-projection") spriteProjection = value;
        else if (key == "--render-threshold") { if (!parseUnsigned(value, renderThreshold) || renderThreshold < 1 || renderThreshold > 100000) return 2; }
        else if (key == "--layout") opts.layout = value;
        else if (key == "--phase-frames") { if (!parseUnsigned(value, opts.phaseFrames)) return 2; }
        else if (key == "--timing") timing = value;
        else if (key == "--broadphase") { broadphase = value; broadphaseOverride = true; }
        else if (key == "--cell-size") {
            try {
                const std::string v = value;
                std::size_t end = 0;
                if (v == "auto") autoCellSize = true;
                else {
                    cellSize = std::stof(v, &end);
                    if (end != v.size()) return 2;
                    autoCellSize = false;
                }
                cellSizeOverride = true;
            } catch (...) { return 2; }
        }
        else if (key == "--fixed-steps") { if (!parseUnsigned(value, fixedSteps)) return 2; }
        else { std::fprintf(stderr,"Unknown argument: %s\n", key.c_str()); return 2; }
    }
    if ((mode != "single" && mode != "collision" && mode != "parallel") ||
        !IsKnownScenario(opts.scenario) || !IsKnownLayout(opts.layout) ||
        !IsKnownRenderWorkload(opts.renderWorkload) ||
        (spriteProjection != "main" && spriteProjection != "workers") ||
        (renderSelection != "fixed" && renderSelection != "adaptive") ||
        opts.phaseFrames == 0 || opts.phaseFrames > 100000 ||
        opts.frames == 0 || opts.frames > 100000 ||
        opts.warmup > 100000 || opts.frames + opts.warmup > 100000 ||
        (timing != "realtime" && timing != "deterministic") ||
        (broadphaseOverride && broadphase != "bruteforce" && broadphase != "grid" && broadphase != "auto") ||
        !std::isfinite(cellSize) || cellSize < 0.05f || cellSize > 4096.f ||
        fixedSteps < 1 || fixedSteps > 16 || (timing == "realtime" && fixedSteps != 1)) {
        usage(); return 2;
    }
    SetOptions(opts);

    try {
#if defined(XENGINE_DEBUG)
        std::filesystem::current_path(XEngineGameBuild::DevelopmentAssetsPath());
#else
        char* base = SDL_GetBasePath();
        if (!base) { std::fputs("Missing application path\n", stderr); return 2; }
        std::filesystem::current_path(base);
        SDL_free(base);
#endif
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Cannot locate benchmark assets: %s\n", e.what()); return 2;
    }
    std::printf("[BENCH] scenario=%s mode=%s frames=%u warmup=%u seed=%u colliders=%d\n",
                opts.scenario.c_str(), mode.c_str(), opts.frames, opts.warmup,
                opts.seed, opts.colliderOverride);
    std::printf("[BENCH-LAYOUT] layout=%s phase_frames=%u\n",
                opts.layout.c_str(), opts.phaseFrames);
    // Read and verify the mode JSON, add only the benchmark broadphase overrides,
    // then pass the verified effective configuration to the engine. In contrast to
    // Engine::Start(path), StartFromJson does not silently fall back to defaults
    // if the configuration cannot be parsed.  This is essential for A/B tests.
    const std::filesystem::path configPath = std::filesystem::path("config") / (mode + ".json");
    std::ifstream configFile(configPath, std::ios::binary);
    if (!configFile) {
        std::fprintf(stderr, "[BENCH-CONFIG] Missing config: %s\n", configPath.string().c_str());
        return 2;
    }
    const std::string configText(std::istreambuf_iterator<char>{configFile},
                                 std::istreambuf_iterator<char>{});
    std::string runtimeConfig;
    try {
        auto config = nlohmann::json::parse(configText);
        const auto& engine = config.at("Engine");
        const auto& renderer = config.at("Renderer");
        const bool threading = engine.at("multiThreading").get<bool>();
        const bool renderParallel = renderer.at("parallelPreparation").get<bool>();
        const bool vsync = renderer.at("vsync").get<bool>();
        const bool profile = engine.at("profileCsv").get<bool>();
        if (threading != (mode != "single") || renderParallel != (mode == "parallel") ||
            vsync || !profile) {
            std::fprintf(stderr, "[BENCH-CONFIG] Invalid flags for mode '%s' in %s\n",
                         mode.c_str(), configPath.string().c_str());
            return 2;
        }
        // Read the mode-specific JSON unless the caller explicitly provided
        // an override. The automatic runner always supplies both flags.
        const auto& collision = config.at("Collision");
        const auto configuredBroadphase = collision.at("broadphase").get<std::string>();
        const auto& cellJson = collision.at("cellSize");
        bool configuredAutoSize = cellJson.is_string() && cellJson.get<std::string>() == "auto";
        const float configuredCellSize = configuredAutoSize ? 2.f : cellJson.get<float>();
        if ((configuredBroadphase != "bruteforce" && configuredBroadphase != "grid" && configuredBroadphase != "auto") ||
            !std::isfinite(configuredCellSize) ||
            configuredCellSize < 0.05f || configuredCellSize > 4096.f) {
            std::fprintf(stderr, "[BENCH-CONFIG] Invalid Collision settings in %s\n",
                         configPath.string().c_str());
            return 2;
        }
        if (!broadphaseOverride) broadphase = configuredBroadphase;
        if (!cellSizeOverride) {
            cellSize = configuredCellSize;
            autoCellSize = configuredAutoSize;
        }
        if (autoCellSize && broadphase != "auto") {
            std::fputs("[BENCH-CONFIG] cellSize=auto requires broadphase=auto\n", stderr);
            return 2;
        }
        config["Renderer"]["parallelMinCommands"] = renderThreshold;
        config["Renderer"]["parallelSelection"] = renderSelection;
        config["Collision"]["broadphase"] = broadphase;
        config["Collision"]["cellSize"] = autoCellSize ? nlohmann::json("auto") : nlohmann::json(cellSize);
        runtimeConfig = config.dump();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[BENCH-CONFIG] Invalid JSON at %s: %s\n",
                     configPath.string().c_str(), e.what());
        return 2;
    }
    if (!Engine::StartFromJson(runtimeConfig)) {
        std::fprintf(stderr, "[BENCH-CONFIG] XEngine failed to initialize with %s\n",
                     configPath.string().c_str());
        return 1;
    }
    Renderer->SetDeferredSpriteProjection(spriteProjection == "workers");
    std::printf("[BENCH-SPRITE-PROJECTION] mode=%s\n", spriteProjection.c_str());
    // Deterministic timing is enabled only for this benchmark process, after
    // the mode-specific JSON has initialized the engine and BEFORE Engine::Run.
    const unsigned requestedSteps = timing == "deterministic" ? fixedSteps : 0;
    if (!Engine::ConfigureDeterministicFixedSteps(requestedSteps)) {
        std::fputs("[BENCH-TIMING] Cannot configure the fixed-step scheduler\n", stderr);
        Engine::Stop();
        return 2;
    }
    std::printf("[BENCH-RENDER] threshold=%u sprites=%u workload=%s selection=%s cycle=%d\n", renderThreshold,
                opts.spriteOverride >= 0 ? static_cast<unsigned>(opts.spriteOverride) :
                GetWorkload(opts.scenario).sprites, opts.renderWorkload.c_str(), renderSelection.c_str(), opts.renderCycle);
    if (autoCellSize)
        std::printf("[BENCH-BROADPHASE] mode=%s cell_size=auto\n", broadphase.c_str());
    else
        std::printf("[BENCH-BROADPHASE] mode=%s cell_size=%.3f\n", broadphase.c_str(), cellSize);
    std::printf("[BENCH-TIMING] mode=%s fixed_steps_per_frame=%u\n",
                timing.c_str(), requestedSteps);
    // The runner requires this handshake: these flags were validated and
    // Engine::StartFromJson successfully consumed the same bytes.
    std::printf("[BENCH-CONFIG] mode=%s multithreading=%d render_parallel=%d vsync=0 profile_csv=1\n",
                mode.c_str(), mode != "single", mode == "parallel");
    Engine::SetInitCallback(InitGame);
    Engine::SetUpdateCallback(CountBenchmarkFrame);
    return Engine::Run();
}

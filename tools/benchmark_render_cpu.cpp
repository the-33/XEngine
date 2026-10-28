// Deterministic CPU-only approximation of sprite preparation and pair checks.
// Does NOT time SDL draw calls, GPU submission or SDL_RenderPresent.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
struct Options {
    std::size_t sprites = 2048;
    std::size_t colliders = 256;
    std::size_t workers = 3;
    std::size_t repeats = 30;
    std::size_t warmup = 5;
    std::string csv = "render_cpu_benchmark.csv";
};

Options Parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        if (i + 1 >= argc) throw std::invalid_argument("missing option value");
        const std::string flag = argv[i];
        const std::string value = argv[++i];
        if (flag == "--csv") { options.csv = value; continue; }
        const std::size_t numeric = std::stoull(value);
        if (flag == "--sprites") options.sprites = numeric;
        else if (flag == "--colliders") options.colliders = numeric;
        else if (flag == "--workers") options.workers = numeric;
        else if (flag == "--repeats") options.repeats = numeric;
        else if (flag == "--warmup") options.warmup = numeric;
        else throw std::invalid_argument("unknown option: " + flag);
    }
    if (options.sprites == 0 || options.colliders == 0 || options.workers == 0 ||
        options.repeats == 0 || options.sprites > 1000000 || options.colliders > 100000)
        throw std::invalid_argument("invalid workload or repetition count");
    return options;
}

// Each element is independent. Threads write disjoint partitions, and the
// checksum is computed sequentially in both modes to preserve equivalence.
std::uint64_t Run(const Options& opts, bool parallel) {
    const std::size_t count = opts.sprites + opts.colliders;
    std::vector<std::uint64_t> prepared(count);
    const auto worker = [&](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            std::uint64_t x = (static_cast<std::uint64_t>(i) + 1u) * 2654435761ULL;
            for (int k = 0; k < 24; ++k) {
                x ^= x >> 13;
                x *= 0x9E3779B185EBCA87ULL;
                x ^= x >> 17;
            }
            if (i >= opts.sprites) {
                // Extra deterministic CPU work representative of broad-phase
                // candidate checks (not a complete collision engine).
                for (int k = 0; k < 16; ++k)
                    x += ((i * 17u + static_cast<std::size_t>(k)) % 97u);
            }
            prepared[i] = x;
        }
    };
    if (parallel && opts.workers > 1) {
        const std::size_t threads = std::min(count, opts.workers);
        std::vector<std::thread> jobs;
        jobs.reserve(threads);
        for (std::size_t t = 0; t < threads; ++t) {
            const std::size_t begin = count * t / threads;
            const std::size_t end = count * (t + 1) / threads;
            jobs.emplace_back(worker, begin, end);
        }
        for (auto& thread : jobs) thread.join();
    } else worker(0, count);

    std::uint64_t checksum = 0;
    for (const auto value : prepared) checksum += value;
    return checksum;
}
} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = Parse(argc, argv);
        std::ofstream csv(options.csv);
        if (!csv) throw std::runtime_error("could not open CSV output");
        csv << "mode,repetition,total_ms,checksum\n";
        std::uint64_t reference = 0;
        bool identical = true;
        for (const bool parallel : {false, true}) {
            const char* mode = parallel ? "parallel" : "single";
            for (std::size_t i = 0; i < options.warmup; ++i) {
                reference = Run(options, parallel);
            }
            for (std::size_t i = 1; i <= options.repeats; ++i) {
                const auto start = std::chrono::steady_clock::now();
                const auto checksum = Run(options, parallel);
                const auto stop = std::chrono::steady_clock::now();
                if (i == 1 && !parallel) reference = checksum;
                identical &= checksum == reference;
                const double milliseconds =
                    std::chrono::duration<double, std::milli>(stop - start).count();
                csv << mode << ',' << i << ',' << milliseconds << ',' << checksum << '\n';
            }
        }
        std::cout << "[CHECKSUM] identical=" << static_cast<int>(identical) << '\n';
        std::cout << "[INFO] This synthetic CPU benchmark does NOT time SDL or GPU work.\n";
        return identical ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[ERROR] " << error.what() << '\n';
        return 2;
    }
}

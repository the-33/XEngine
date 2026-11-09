#include "RenderManager.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

#include "SDL.h"
#include "SDL_ttf.h"
#include "WindowManager.h"
#include "Camera2D.h"
#include "XEngine/detail/RenderPreparation.h"
#include "XEngine/detail/RendererBackendPolicy.h"
#include "XEngine/detail/DebugOverlayProjection.h"
#include <chrono>
#include <algorithm>
#include <array>
#include <cstdio>
#include "ErrorHandler.h"
#include "TimeManager.h"

/** @brief Sdl intersect in RenderManager.cpp.
 * @param a First input operand.
 * @param b Second input operand.
 * @param out Caller-provided output destination.
 * @return True if the operation succeeded or its condition holds; otherwise false.
 */
static inline bool SDL_Intersect(const SDL_Rect& a, const SDL_Rect& b, SDL_Rect& out) {
    const int x1 = std::max(a.x, b.x);
    const int y1 = std::max(a.y, b.y);
    const int x2 = std::min(a.x + a.w, b.x + b.w);
    const int y2 = std::min(a.y + a.h, b.y + b.h);
    out.x = x1; out.y = y1;
    out.w = std::max(0, x2 - x1);
    out.h = std::max(0, y2 - y1);
    return out.w > 0 && out.h > 0;
}

/** @brief Namespace collecting @135161076207236321274265112362354261222357326134 functionality for XEngine.
 */
namespace {
constexpr int kRoundedArcSteps_ = 20;
constexpr int kRoundedVertices_ = (kRoundedArcSteps_ + 1) * 4;

void RoundedPath_(const Rect& rect, float radius,
                  std::array<SDL_FPoint, kRoundedVertices_>& points) noexcept
{
    constexpr float pi = 3.14159265358979323846f;
    const float centersX[4]{rect.x + rect.w - radius, rect.x + rect.w - radius,
                            rect.x + radius, rect.x + radius};
    const float centersY[4]{rect.y + radius, rect.y + rect.h - radius,
                            rect.y + rect.h - radius, rect.y + radius};
    for (int corner = 0; corner < 4; ++corner) {
        for (int step = 0; step <= kRoundedArcSteps_; ++step) {
            const float angle = (float(corner) - 1.f +
                                 float(step) / float(kRoundedArcSteps_)) * pi * 0.5f;
            const int index = corner * (kRoundedArcSteps_ + 1) + step;
            points[index] = {centersX[corner] + std::cos(angle) * radius,
                             centersY[corner] + std::sin(angle) * radius};
        }
    }
}

SDL_Color VertexColor_(Color color) noexcept
{
    return {static_cast<Uint8>(color.r), static_cast<Uint8>(color.g),
            static_cast<Uint8>(color.b), static_cast<Uint8>(color.a)};
}

Rect InsetRoundedRect_(const Rect& rect, float inset) noexcept
{
    return {rect.x + inset, rect.y + inset,
            rect.w - 2.f * inset, rect.h - 2.f * inset};
}

// Interpolating vertex alpha across a one-pixel ring provides coverage
// antialiasing for SDL_RenderGeometry on backends without multisampling.
void RoundedBand_(SDL_Renderer* renderer, const Rect& outside, float outerRadius,
                  Color outerColor, const Rect& inside, float innerRadius,
                  Color innerColor) noexcept
{
    if (outside.w <= 0.f || outside.h <= 0.f ||
        inside.w <= 0.f || inside.h <= 0.f) return;
    std::array<SDL_FPoint, kRoundedVertices_> outerPath{}, innerPath{};
    RoundedPath_(outside, outerRadius, outerPath);
    RoundedPath_(inside, innerRadius, innerPath);
    std::array<SDL_Vertex, kRoundedVertices_ * 2> vertices{};
    std::array<int, kRoundedVertices_ * 6> indices{};
    const SDL_Color outsideTint = VertexColor_(outerColor);
    const SDL_Color insideTint = VertexColor_(innerColor);
    for (int i = 0; i < kRoundedVertices_; ++i) {
        const int next = (i + 1) % kRoundedVertices_;
        vertices[i * 2] = {outerPath[i], outsideTint, {0.f, 0.f}};
        vertices[i * 2 + 1] = {innerPath[i], insideTint, {0.f, 0.f}};
        const int first = i * 6;
        indices[first] = i * 2;
        indices[first + 1] = next * 2;
        indices[first + 2] = i * 2 + 1;
        indices[first + 3] = i * 2 + 1;
        indices[first + 4] = next * 2;
        indices[first + 5] = next * 2 + 1;
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(),
                       static_cast<int>(vertices.size()), indices.data(),
                       static_cast<int>(indices.size()));
}

void FilledPolygon_(SDL_Renderer* renderer, const Rect& rect, float radius,
                    Color color) noexcept
{
    if (color.a <= 0 || rect.w <= 0.f || rect.h <= 0.f) return;
    constexpr float feather = 0.5f;
    const bool antialias = rect.w > 2.f && rect.h > 2.f;
    const Rect core = antialias ? InsetRoundedRect_(rect, feather) : rect;
    const float coreRadius = std::max(0.f, radius - (antialias ? feather : 0.f));
    std::array<SDL_FPoint, kRoundedVertices_> path{};
    RoundedPath_(core, coreRadius, path);
    std::array<SDL_Vertex, kRoundedVertices_ + 1> vertices{};
    vertices[0] = {{core.x + core.w * 0.5f, core.y + core.h * 0.5f},
                   VertexColor_(color), {0.f, 0.f}};
    for (int i = 0; i < kRoundedVertices_; ++i)
        vertices[i + 1] = {path[i], VertexColor_(color), {0.f, 0.f}};
    std::array<int, kRoundedVertices_ * 3> indices{};
    for (int i = 0; i < kRoundedVertices_; ++i) {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = (i + 1) % kRoundedVertices_ + 1;
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(),
                       static_cast<int>(vertices.size()), indices.data(),
                       static_cast<int>(indices.size()));
    if (antialias) {
        Color transparent = color;
        transparent.a = 0;
        RoundedBand_(renderer, InsetRoundedRect_(rect, -feather),
                     radius + feather, transparent,
                     core, coreRadius, color);
    }
}

void RoundedOutline_(SDL_Renderer* renderer, const Rect& rect, float radius,
                     float thickness, Color color) noexcept
{
    if (color.a <= 0 || thickness <= 0.f || rect.w <= 0.f || rect.h <= 0.f) return;
    const float innerWidth = rect.w - 2.f * thickness;
    const float innerHeight = rect.h - 2.f * thickness;
    if (innerWidth <= 0.f || innerHeight <= 0.f) {
        FilledPolygon_(renderer, rect, radius, color);
        return;
    }
    constexpr float feather = 0.5f;
    const Rect outside = InsetRoundedRect_(rect, -feather);
    const Rect outerCore = InsetRoundedRect_(rect, feather);
    const float outerCoreRadius = std::max(0.f, radius - feather);
    const Rect innerCore = InsetRoundedRect_(rect, thickness - feather);
    const float innerCoreRadius = std::max(0.f, radius - thickness + feather);
    const Rect inside = InsetRoundedRect_(rect, thickness + feather);
    const float innerRadius = std::max(0.f, radius - thickness - feather);
    Color transparent = color;
    transparent.a = 0;
    // Feather outside edge, opaque center, feather inside edge.
    RoundedBand_(renderer, outside, radius + feather, transparent,
                 outerCore, outerCoreRadius, color);
    if (thickness > 1.f)
        RoundedBand_(renderer, outerCore, outerCoreRadius, color,
                     innerCore, innerCoreRadius, color);
    RoundedBand_(renderer, innerCore, innerCoreRadius, color,
                 inside, innerRadius, transparent);
}

// Four-corner geometry shares the same SDL_RenderGeometry path as the
// uniform-radius primitive. Each arc uses its own center and radius; side
// fitting prevents adjacent arcs from crossing on narrow controls.
/** @brief Generate the clockwise arc vertices for an independently rounded rectangle.
 * @param rect Bounds in physical drawable pixels.
 * @param radii Four corner radii, normalized to fit each side.
 * @param points Caller-owned output vertex positions.
 */
void RoundedPathCorners_(const Rect& rect, xengine::detail::ui::CornerRadii radii,
                         std::array<SDL_FPoint, kRoundedVertices_>& points) noexcept
{
    using xengine::detail::ui::FitCornerRadii;
    radii = FitCornerRadii(radii, rect.w, rect.h);
    constexpr float pi = 3.14159265358979323846f;
    const float rs[4]{radii.topRight, radii.bottomRight,
                      radii.bottomLeft, radii.topLeft};
    const float centersX[4]{rect.x + rect.w - rs[0], rect.x + rect.w - rs[1],
                            rect.x + rs[2], rect.x + rs[3]};
    const float centersY[4]{rect.y + rs[0], rect.y + rect.h - rs[1],
                            rect.y + rect.h - rs[2], rect.y + rs[3]};
    for (int corner = 0; corner < 4; ++corner) {
        for (int step = 0; step <= kRoundedArcSteps_; ++step) {
            const float angle = (float(corner) - 1.f +
                                 float(step) / float(kRoundedArcSteps_)) * pi * 0.5f;
            const int index = corner * (kRoundedArcSteps_ + 1) + step;
            points[index] = {centersX[corner] + std::cos(angle) * rs[corner],
                             centersY[corner] + std::sin(angle) * rs[corner]};
        }
    }
}

/** @brief Draw the anti-aliased ring between two independently rounded contours.
 * @param renderer SDL renderer owning the geometry submission.
 * @param outside Outer contour bounds.
 * @param outerRadii Outer corner radii.
 * @param outerColor Color at the outer vertices.
 * @param inside Inner contour bounds.
 * @param innerRadii Inner corner radii.
 * @param innerColor Color at the inner vertices.
 */
void RoundedBandCorners_(SDL_Renderer* renderer, const Rect& outside,
                         xengine::detail::ui::CornerRadii outerRadii, Color outerColor,
                         const Rect& inside,
                         xengine::detail::ui::CornerRadii innerRadii,
                         Color innerColor) noexcept
{
    if (outside.w <= 0.f || outside.h <= 0.f ||
        inside.w <= 0.f || inside.h <= 0.f) return;
    std::array<SDL_FPoint, kRoundedVertices_> outerPath{}, innerPath{};
    RoundedPathCorners_(outside, outerRadii, outerPath);
    RoundedPathCorners_(inside, innerRadii, innerPath);
    std::array<SDL_Vertex, kRoundedVertices_ * 2> vertices{};
    std::array<int, kRoundedVertices_ * 6> indices{};
    const SDL_Color outsideTint = VertexColor_(outerColor);
    const SDL_Color insideTint = VertexColor_(innerColor);
    for (int i = 0; i < kRoundedVertices_; ++i) {
        const int next = (i + 1) % kRoundedVertices_;
        vertices[i * 2] = {outerPath[i], outsideTint, {0.f, 0.f}};
        vertices[i * 2 + 1] = {innerPath[i], insideTint, {0.f, 0.f}};
        const int first = i * 6;
        indices[first] = i * 2;
        indices[first + 1] = next * 2;
        indices[first + 2] = i * 2 + 1;
        indices[first + 3] = i * 2 + 1;
        indices[first + 4] = next * 2;
        indices[first + 5] = next * 2 + 1;
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(),
                       static_cast<int>(vertices.size()), indices.data(),
                       static_cast<int>(indices.size()));
}

/** @brief Fill a polygon with independently rounded corners and feathered edges.
 * @param renderer SDL renderer owning the geometry submission.
 * @param rect Rectangle bounds in drawable pixels.
 * @param radii Radius at each corner.
 * @param color Fill RGBA color.
 */
void FilledPolygonCorners_(SDL_Renderer* renderer, const Rect& rect,
                           xengine::detail::ui::CornerRadii radii, Color color) noexcept
{
    if (color.a <= 0 || rect.w <= 0.f || rect.h <= 0.f) return;
    using xengine::detail::ui::OffsetCornerRadii;
    constexpr float feather = 0.5f;
    const bool antialias = rect.w > 2.f && rect.h > 2.f;
    const Rect core = antialias ? InsetRoundedRect_(rect, feather) : rect;
    const auto coreRadii = OffsetCornerRadii(radii, antialias ? -feather : 0.f);
    std::array<SDL_FPoint, kRoundedVertices_> path{};
    RoundedPathCorners_(core, coreRadii, path);
    std::array<SDL_Vertex, kRoundedVertices_ + 1> vertices{};
    vertices[0] = {{core.x + core.w * 0.5f, core.y + core.h * 0.5f},
                   VertexColor_(color), {0.f, 0.f}};
    for (int i = 0; i < kRoundedVertices_; ++i)
        vertices[i + 1] = {path[i], VertexColor_(color), {0.f, 0.f}};
    std::array<int, kRoundedVertices_ * 3> indices{};
    for (int i = 0; i < kRoundedVertices_; ++i) {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = (i + 1) % kRoundedVertices_ + 1;
    }
    SDL_RenderGeometry(renderer, nullptr, vertices.data(),
                       static_cast<int>(vertices.size()), indices.data(),
                       static_cast<int>(indices.size()));
    if (antialias) {
        Color transparent = color;
        transparent.a = 0;
        RoundedBandCorners_(renderer, InsetRoundedRect_(rect, -feather),
                            OffsetCornerRadii(radii, feather), transparent,
                            core, coreRadii, color);
    }
}

/** @brief Draw an antialiased inside border respecting four separate radii.
 * @param renderer SDL renderer owning the geometry submission.
 * @param rect Outer border bounds.
 * @param radii Four corner radii in physical pixels.
 * @param thickness Positive border width in physical pixels.
 * @param color Outline RGBA color.
 */
void RoundedOutlineCorners_(SDL_Renderer* renderer, const Rect& rect,
                            xengine::detail::ui::CornerRadii radii,
                            float thickness, Color color) noexcept
{
    if (color.a <= 0 || thickness <= 0.f || rect.w <= 0.f || rect.h <= 0.f) return;
    if (rect.w <= 2.f * thickness || rect.h <= 2.f * thickness) {
        FilledPolygonCorners_(renderer, rect, radii, color);
        return;
    }
    using xengine::detail::ui::OffsetCornerRadii;
    constexpr float feather = 0.5f;
    const Rect outside = InsetRoundedRect_(rect, -feather);
    const Rect outerCore = InsetRoundedRect_(rect, feather);
    const Rect innerCore = InsetRoundedRect_(rect, thickness - feather);
    const Rect inside = InsetRoundedRect_(rect, thickness + feather);
    Color transparent = color;
    transparent.a = 0;
    RoundedBandCorners_(renderer, outside, OffsetCornerRadii(radii, feather),
                        transparent, outerCore, OffsetCornerRadii(radii, -feather), color);
    if (thickness > 1.f)
        RoundedBandCorners_(renderer, outerCore, OffsetCornerRadii(radii, -feather),
                            color, innerCore,
                            OffsetCornerRadii(radii, -thickness + feather), color);
    RoundedBandCorners_(renderer, innerCore,
                        OffsetCornerRadii(radii, -thickness + feather), color,
                        inside, OffsetCornerRadii(radii, -thickness - feather), transparent);
}
}

namespace {
// The benchmark installer writes this preference to a per-user, per-machine
// location, never into the game's packaged assets/config/engine.json.
std::filesystem::path LocalRendererRankingPath_() {
#if defined(_WIN32)
    if (const char* base = std::getenv("LOCALAPPDATA"))
        if (*base) return std::filesystem::path(base) / "XEngine" / "render_backend_ranking.json";
#else
    if (const char* base = std::getenv("XDG_CONFIG_HOME"))
        if (*base) return std::filesystem::path(base) / "xengine" / "render_backend_ranking.json";
    if (const char* base = std::getenv("HOME"))
        if (*base) return std::filesystem::path(base) / ".config" / "xengine" /
                          "render_backend_ranking.json";
#endif
    return {};
}

std::vector<std::string> ReadMeasuredBackendOrder_(SDL_Window* window) noexcept {
    std::vector<std::string> names;
    try {
        const auto path = LocalRendererRankingPath_();
        if (path.empty() || !std::filesystem::is_regular_file(path)) return names;
        if (std::filesystem::file_size(path) > 65536)
            throw std::runtime_error("ranking file exceeds 64 KiB");
        std::ifstream stream(path);
        if (!stream) throw std::runtime_error("cannot open ranking file");
        const auto record = nlohmann::json::parse(stream);
        const auto& display = record.at("display");
#if defined(_WIN32)
        constexpr const char* platform = "windows";
#else
        constexpr const char* platform = "linux";
#endif
        if (record.at("schema").get<int>() != 1 ||
            record.at("platform").get<std::string>() != platform ||
            record.at("ranking_valid").get<bool>() != true)
            throw std::runtime_error("ranking platform/schema/validity mismatch");

        SDL_version version{};
        SDL_GetVersion(&version);
        const std::string sdlVersion = std::to_string(version.major) + "." +
            std::to_string(version.minor) + "." + std::to_string(version.patch);
        const char* video = SDL_GetCurrentVideoDriver();
        const int displayIndex = window ? SDL_GetWindowDisplayIndex(window) : -1;
        SDL_DisplayMode mode{};
        int width = 0, height = 0;
        if (window) SDL_GetWindowSize(window, &width, &height);
        if (!video || displayIndex < 0 ||
            SDL_GetCurrentDisplayMode(displayIndex, &mode) != 0 ||
            display.at("sdl_version").get<std::string>() != sdlVersion ||
            display.at("video_driver").get<std::string>() != video ||
            display.at("display_index").get<int>() != displayIndex ||
            display.at("display_refresh_hz").get<int>() != mode.refresh_rate ||
            display.at("window_width").get<int>() != width ||
            display.at("window_height").get<int>() != height ||
            (display.contains("window_fullscreen") &&
             display.at("window_fullscreen").get<bool>() !=
                 ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0)))
            throw std::runtime_error("SDL version, video session, display or window size changed");

        const auto& ranked = record.at("ranked_backends");
        if (!ranked.is_array() || ranked.size() < 2 || ranked.size() > 32)
            throw std::runtime_error("invalid ranked backend count");
        std::set<std::string> unique;
        for (const auto& item : ranked) {
            const std::string name = item.get<std::string>();
            if (name.empty() || name.size() > 64 ||
                !std::all_of(name.begin(), name.end(), [](unsigned char character) {
                    return std::isalnum(character) || character == '_' || character == '-';
                }) || !unique.insert(RendererBackendPolicy::Normalize(name)).second)
                throw std::runtime_error("invalid or duplicate renderer name");
            names.push_back(RendererBackendPolicy::Normalize(name));
        }
        Console.Log("[RENDER-BACKEND] benchmark-priority=loaded platform=" +
                    std::string(platform) + " count=" + std::to_string(names.size()) +
                    " first=" + names.front(), "Engine - Render");
    } catch (const std::exception& error) {
        Console.Warning("[RENDER-BACKEND] ignoring local benchmark ranking: " +
                        std::string(error.what()), "Engine - Render");
        names.clear();
    }
    return names;
}
} // namespace

bool RenderManager::ResolveRendererBackend_(const std::string& requested,
                                             bool allowFallback, bool strictHint) noexcept
{
    mRequestedRendererBackend = RendererBackendPolicy::Normalize(requested);
    if (mRequestedRendererBackend.empty()) mRequestedRendererBackend = "auto";
    mPreferredRendererDriverIndex = -1;
    mRendererCandidates.clear();
    const std::vector<std::string> measuredOrder = strictHint ?
        std::vector<std::string>{} : ReadMeasuredBackendOrder_(mWindow ? mWindow->SDL() : nullptr);
    const auto plan = RendererBackendPolicy::MakePlan(
        requested,
#if defined(_WIN32)
        true,
#else
        false,
#endif
        allowFallback, strictHint, measuredOrder);
    mFirstChoiceRendererBackend = plan.names.empty() ? "SDL default" : plan.names.front();

    std::string available;
    const int count = SDL_GetNumRenderDrivers();
    std::vector<BackendCandidate> installed;
    for (int i = 0; i < count; ++i)
    {
        SDL_RendererInfo info{};
        if (SDL_GetRenderDriverInfo(i, &info) != 0 || !info.name) continue;
        if (!available.empty()) available += ", ";
        available += info.name;
        installed.push_back({i, RendererBackendPolicy::Normalize(info.name)});
    }
    for (const auto& name : plan.names)
    {
        const auto found = std::find_if(installed.begin(), installed.end(),
            [&](const BackendCandidate& candidate) { return candidate.name == name; });
        if (found != installed.end()) mRendererCandidates.push_back(*found);
    }
    if (!strictHint && allowFallback && mRequestedRendererBackend != "auto" &&
        (mRendererCandidates.empty() ||
         mRendererCandidates.front().name != mRequestedRendererBackend))
        Console.Warning("[RENDER-BACKEND] requested=" + mRequestedRendererBackend +
                        " is not installed; trying fallback", "Engine - Render");
    if (plan.allowSdlDefault) mRendererCandidates.push_back({-1, "SDL default"});
    if (!mRendererCandidates.empty()) return true;

    const std::string error = "Requested SDL renderer backend '" +
        requested + "' is unavailable on this platform. Available: " +
        (available.empty() ? "(none)" : available);
    LogError("Engine - Render", "[RENDER-BACKEND] Init(): " + error);
    return false;
}

bool RenderManager::CreateInitialRenderer_(std::uint32_t flags) noexcept
{
    // Preserve requested acceleration/VSync across every backend first;
    // only relax these flags if none of the drivers can initialize.
    for (int pass = 0; pass < (flags ? 2 : 1); ++pass)
    {
        const Uint32 attemptFlags = pass == 0 ? flags : 0;
        for (const BackendCandidate& candidate : mRendererCandidates)
        {
            SDL_Renderer* candidateRenderer = SDL_CreateRenderer(
                mWindow->SDL(), candidate.index, attemptFlags);
            if (!candidateRenderer)
            {
                Console.Warning("[RENDER-BACKEND] backend=" + candidate.name +
                                " flags=" + std::to_string(attemptFlags) +
                                " failed: " + SDL_GetError(), "Engine - Render");
                continue;
            }

            SDL_RendererInfo info{};
            if (SDL_GetRendererInfo(candidateRenderer, &info) != 0 || !info.name ||
                (candidate.index >= 0 &&
                 RendererBackendPolicy::Normalize(info.name) != candidate.name))
            {
                Console.Warning("[RENDER-BACKEND] cannot verify backend=" +
                                candidate.name + ": " + SDL_GetError(), "Engine - Render");
                SDL_DestroyRenderer(candidateRenderer);
                continue;
            }

            // An SDL-default attempt also records its concrete driver for
            // subsequent SetVSync() recreation.
            mPreferredRendererDriverIndex = candidate.index;
            if (mPreferredRendererDriverIndex < 0)
            {
                for (int index = 0, count = SDL_GetNumRenderDrivers(); index < count; ++index)
                {
                    SDL_RendererInfo driverInfo{};
                    if (SDL_GetRenderDriverInfo(index, &driverInfo) == 0 && driverInfo.name &&
                        RendererBackendPolicy::Normalize(driverInfo.name) ==
                            RendererBackendPolicy::Normalize(info.name))
                    {
                        mPreferredRendererDriverIndex = index;
                        break;
                    }
                }
            }
            mRenderer = candidateRenderer;
            if (pass != 0 || candidate.name != mRendererCandidates.front().name ||
                (mFirstChoiceRendererBackend != "SDL default" &&
                 RendererBackendPolicy::Normalize(info.name) != mFirstChoiceRendererBackend))
                Console.Warning("[RENDER-BACKEND] fallback from=" + mFirstChoiceRendererBackend +
                                " to=" + info.name + " flags=" +
                                std::to_string(attemptFlags), "Engine - Render");
            // Re-creation after a VSync change should retry the currently
            // working driver before visiting compatibility alternatives.
            auto active = std::find_if(mRendererCandidates.begin(), mRendererCandidates.end(),
                [&](const BackendCandidate& entry) {
                    return entry.index == candidate.index && entry.name == candidate.name;
                });
            if (active != mRendererCandidates.end())
            {
                active->index = mPreferredRendererDriverIndex;
                active->name = RendererBackendPolicy::Normalize(info.name);
                std::rotate(mRendererCandidates.begin(), active, active + 1);
            }
            return true;
        }
    }
    return false;
}

bool RenderManager::Init(const Config& cfg) noexcept
{
    mWindow = WindowManager::GetInstancePtr();

    if (mWindow == nullptr) return false;

    mAccelerated = cfg.accelerated;
    mVSync = cfg.vsync;
    mClearColor = cfg.bgColor;

    // Explicit process hints (e.g. SDL_RENDER_DRIVER during benchmarks) take
    // precedence over the platform-specific JSON choice. Enumerating SDL
    // renderer indices prevents an unsupported explicit choice from silently
    // falling back to a different driver.
    const char* hint = SDL_GetHint(SDL_HINT_RENDER_DRIVER);
    const bool hasHint = hint && *hint;
    if (!ResolveRendererBackend_(hasHint ? hint : cfg.backend,
                                 cfg.backendFallback, hasHint)) return false;

    // Create SDL renderer
    Uint32 flags = 0;
    if (mAccelerated) flags |= SDL_RENDERER_ACCELERATED;
    if (mVSync)       flags |= SDL_RENDERER_PRESENTVSYNC;

    if (!CreateInitialRenderer_(flags))
    {
        LogError("Engine - Render", "Init(): SDL_CreateRenderer failed for all allowed backends (requested '" +
                 mRequestedRendererBackend + "').", SDL_GetError());
        return false;
    }
    SDL_RendererInfo rendererInfo{};
    if (SDL_GetRendererInfo(mRenderer, &rendererInfo) != 0 || !rendererInfo.name)
    {
        LogError("Engine - Render", "Init(): SDL_GetRendererInfo() failed.", SDL_GetError());
        SDL_DestroyRenderer(mRenderer);
        mRenderer = nullptr;
        return false;
    }
    Console.Log("[RENDER-BACKEND] requested=" + mRequestedRendererBackend +
                " source=" + (hasHint ? "SDL_HINT_RENDER_DRIVER" : "config") +
                " actual=" + rendererInfo.name +
                " flags=" + std::to_string(rendererInfo.flags) +
                " driver_index=" + std::to_string(mPreferredRendererDriverIndex),
                "Engine - Render");
    mFirstChoiceRendererBackend = RendererBackendPolicy::Normalize(rendererInfo.name);

    // Alpha blending for DrawRect/DrawLine and alpha textures
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    const Vec2I drawable = mWindow->GetDrawableSize();
    mWinW = drawable.x; mWinH = drawable.y;

    CreateWhiteTexture_();

    return true;
}

void RenderManager::CommitRenderTarget_(SDL_Texture* target, int width, int height) noexcept
{
    // A texture may still be the active target of SDL. Unbind before destroying it.
    if (mRenderer && mRenderTarget && SDL_GetRenderTarget(mRenderer) == mRenderTarget)
        (void)SDL_SetRenderTarget(mRenderer, nullptr);
    if (mRenderTarget) SDL_DestroyTexture(mRenderTarget);
    mRenderTarget = target;
    mRenderWidth = target ? width : 0;
    mRenderHeight = target ? height : 0;
    mWorldTargetActive = false;
}

void RenderManager::ApplyPendingRenderTarget_() noexcept
{
    if (!mHasPendingRenderTarget) return;
    CommitRenderTarget_(mPendingRenderTarget, mPendingWidth, mPendingHeight);
    mPendingRenderTarget = nullptr;
    mPendingWidth = mPendingHeight = 0;
    mHasPendingRenderTarget = false;
}

bool RenderManager::SetRenderResolution(int width, int height) noexcept
{
    // (0, 0) disables the offscreen target; a single zero dimension is invalid.
    if (!mRenderer || ((width == 0) != (height == 0)) ||
        width < 0 || height < 0 || width > 8192 || height > 8192) return false;

    SDL_Texture* target = nullptr;
    if (width != 0) {
        target = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_RGBA8888,
                                   SDL_TEXTUREACCESS_TARGET, width, height);
        if (!target) {
            LogError("Engine - Render",
                     "Could not allocate an SDL render-target texture.", SDL_GetError());
            return false;
        }
        // Enlarging the finished low-resolution image must retain its pixels.
        if (SDL_SetTextureScaleMode(target, SDL_ScaleModeNearest) != 0) {
            LogError("Engine - Render",
                     "Could not configure nearest-neighbour upscaling.", SDL_GetError());
            SDL_DestroyTexture(target);
            return false;
        }
        SDL_SetTextureBlendMode(target, SDL_BLENDMODE_NONE);
    }

    // Options can be saved from a button callback while this very frame is
    // being recorded/drawn. Keep its old target until the frame is presented.
    if (mFrameOpen) {
        if (mPendingRenderTarget) SDL_DestroyTexture(mPendingRenderTarget);
        mPendingRenderTarget = target;
        mPendingWidth = width;
        mPendingHeight = height;
        mHasPendingRenderTarget = true;
    } else {
        if (mPendingRenderTarget) SDL_DestroyTexture(mPendingRenderTarget);
        mPendingRenderTarget = nullptr;
        mHasPendingRenderTarget = false;
        CommitRenderTarget_(target, width, height);
    }
    return true;
}

Vec2I RenderManager::GetRenderResolution() const noexcept
{
    if (mRenderTarget) return {mRenderWidth, mRenderHeight};
    return mWindow ? mWindow->GetDrawableSize() : Vec2I{0, 0};
}

RectI RenderManager::PresentationRect_() const noexcept
{
    if (!mRenderer) return {0, 0, 0, 0};
    int outputWidth = 0, outputHeight = 0;
    if (SDL_GetRendererOutputSize(mRenderer, &outputWidth, &outputHeight) != 0 ||
        outputWidth <= 0 || outputHeight <= 0) return {0, 0, 0, 0};
    if (!mRenderTarget) return {0, 0, outputWidth, outputHeight};

    // Fit without stretching the game when the monitor aspect ratio differs.
    const double scale = std::min(static_cast<double>(outputWidth) / mRenderWidth,
                                  static_cast<double>(outputHeight) / mRenderHeight);
    const int scaledWidth = std::max(1, static_cast<int>(std::lround(mRenderWidth * scale)));
    const int scaledHeight = std::max(1, static_cast<int>(std::lround(mRenderHeight * scale)));
    return {(outputWidth - scaledWidth) / 2, (outputHeight - scaledHeight) / 2,
            scaledWidth, scaledHeight};
}

Vec2 RenderManager::DisplayToRender(Vec2 displayPoint) const noexcept
{
    if (!mRenderTarget) return displayPoint;
    const RectI fit = PresentationRect_();
    if (fit.w <= 0 || fit.h <= 0) return displayPoint;
    return {(displayPoint.x - fit.x) * mRenderWidth / fit.w,
            (displayPoint.y - fit.y) * mRenderHeight / fit.h};
}

void RenderManager::BeginScreenPass_() noexcept
{
    if (!mRenderer) return;
    if (mWorldTargetActive && mRenderTarget) {
        mWorldTargetActive = false;
        if (SDL_SetRenderTarget(mRenderer, nullptr) != 0) {
            LogError("Engine - Render",
                     "Could not restore the window render target.", SDL_GetError());
            return;
        }
        SDL_SetRenderDrawColor(mRenderer, 0, 0, 0, 255);
        SDL_RenderClear(mRenderer); // Black letterbox bars if aspect ratios differ.
        const RectI fit = PresentationRect_();
        const SDL_Rect dst{fit.x, fit.y, fit.w, fit.h};
        if (dst.w > 0 && dst.h > 0)
            (void)SDL_RenderCopy(mRenderer, mRenderTarget, nullptr, &dst);
    }

    // El mundo usa la resolución interna; la UI se dibuja sobre la ventana real.
    int outputWidth = 0;
    int outputHeight = 0;
    if (SDL_GetRendererOutputSize(mRenderer, &outputWidth, &outputHeight) == 0 &&
        outputWidth > 0 && outputHeight > 0) {
        mWinW = outputWidth;
        mWinH = outputHeight;
    }
    // Debug lines are deliberately excluded from the internal render texture.
    // They use this native-pixel pass, before screen-space UI widgets.
    DrawDeferredDebugOnScreen_();
}

void RenderManager::DrawDeferredDebugOnScreen_() noexcept
{
    if (mDeferredDebugCmds.empty()) return;
    if (!mRenderer || !mActiveCam) {
        mDeferredDebugCmds.clear();
        return;
    }
    const Vec2I virtualSize = GetRenderResolution();
    const RectI output = mRenderTarget ? PresentationRect_()
                                       : RectI{0, 0, mWinW, mWinH};
    if (virtualSize.x <= 0 || virtualSize.y <= 0 ||
        output.w <= 0 || output.h <= 0) {
        mDeferredDebugCmds.clear();
        return;
    }

    const auto start = std::chrono::steady_clock::now();
    const DrawPhase savedPhase = mDrawPhase;
    mDrawPhase = DrawPhase::Debug;
    const auto toPhysical = [&](float x, float y) noexcept {
        const Vec2 virtualPixel = mActiveCam->WorldToScreen(
            x, y, virtualSize.x, virtualSize.y);
        return XEngineDebugOverlay::ProjectToOutput(
            {virtualPixel.x, virtualPixel.y}, virtualSize.x, virtualSize.y,
            output.x, output.y, output.w, output.h);
    };
    const auto drawLine = [&](float x1, float y1, float x2, float y2,
                              Color color) noexcept {
        const auto a = toPhysical(x1, y1);
        const auto b = toPhysical(x2, y2);
        // Constant physical thickness, independent of the render texture and
        // of UICanvas::ScaleWithScreenSize. The 2px quad avoids dropped edges.
        DrawLineScreenImmediate_(a.x, a.y, b.x, b.y, color, 2.f);
    };

    for (const DebugCmd& c : mDeferredDebugCmds) {
        if (c.type == DebugCmd::Type::Line) {
            drawLine(c.x1, c.y1, c.x2, c.y2, c.color);
            continue;
        }
        const auto a = toPhysical(c.rect.x, c.rect.y);
        const auto b = toPhysical(c.rect.x + c.rect.w, c.rect.y + c.rect.h);
        if (c.filled) {
            const SDL_FRect rect{std::min(a.x, b.x), std::min(a.y, b.y),
                                 std::fabs(b.x - a.x), std::fabs(b.y - a.y)};
            SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(mRenderer, c.color.r, c.color.g,
                                   c.color.b, c.color.a);
            SDL_RenderFillRectF(mRenderer, &rect);
            ++nDrawCallsThisFrame;
            CountSubmission_();
        } else {
            drawLine(c.rect.x, c.rect.y, c.rect.x + c.rect.w, c.rect.y, c.color);
            drawLine(c.rect.x + c.rect.w, c.rect.y,
                     c.rect.x + c.rect.w, c.rect.y + c.rect.h, c.color);
            drawLine(c.rect.x + c.rect.w, c.rect.y + c.rect.h,
                     c.rect.x, c.rect.y + c.rect.h, c.color);
            drawLine(c.rect.x, c.rect.y + c.rect.h,
                     c.rect.x, c.rect.y, c.color);
        }
    }
    mDeferredDebugCmds.clear();
    mDrawPhase = savedPhase;
    mDebugMs += std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

void RenderManager::CreateWhiteTexture_() noexcept
{
    // 1x1 white texture is tied to the SDL_Renderer lifetime.
    mWhite = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, 1, 1);
    if (mWhite && SDL_SetRenderTarget(mRenderer, mWhite) == 0) {
        SDL_SetTextureBlendMode(mWhite, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(mRenderer, 255, 255, 255, 255);
        SDL_RenderClear(mRenderer);
        SDL_SetRenderTarget(mRenderer, nullptr);
    }
    else
    {
        if (mWhite) { SDL_DestroyTexture(mWhite); mWhite = nullptr; }
        // Compatibility renderers may not support target textures. Rect/line
        // rendering still requires a valid white texture after fallback.
        mWhite = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_STATIC, 1, 1);
        if (mWhite)
        {
            const Uint32 white = 0xffffffffu;
            if (SDL_UpdateTexture(mWhite, nullptr, &white, sizeof(white)) != 0)
            {
                SDL_DestroyTexture(mWhite);
                mWhite = nullptr;
            }
            else SDL_SetTextureBlendMode(mWhite, SDL_BLENDMODE_BLEND);
        }
        if (!mWhite)
            Console.Warning("[RENDER-BACKEND] white draw texture unavailable: " +
                            std::string(SDL_GetError()), "Engine - Render");
    }
}

RenderManager::BackendDiagnostics RenderManager::QueryBackendDiagnostics() const
{
    BackendDiagnostics result;
    SDL_version version{};
    SDL_GetVersion(&version);
    result.sdlVersion = std::to_string(version.major) + "." +
                        std::to_string(version.minor) + "." +
                        std::to_string(version.patch);
    if (const char* video = SDL_GetCurrentVideoDriver()) result.videoDriver = video;
    if (const char* hint = SDL_GetHint(SDL_HINT_RENDER_DRIVER)) result.renderDriverHint = hint;
    if (const char* hint = SDL_GetHint(SDL_HINT_RENDER_VSYNC)) result.renderVsyncHint = hint;
    if (mRenderer) {
        SDL_RendererInfo info{};
        if (SDL_GetRendererInfo(mRenderer, &info) == 0) {
            result.rendererInfoAvailable = true;
            if (info.name) result.rendererName = info.name;
            result.rendererFlags = info.flags;
            result.accelerated = (info.flags & SDL_RENDERER_ACCELERATED) != 0;
            result.software = (info.flags & SDL_RENDERER_SOFTWARE) != 0;
            result.vsync = (info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
            result.targetTexture = (info.flags & SDL_RENDERER_TARGETTEXTURE) != 0;
            result.maxTextureWidth = info.max_texture_width;
            result.maxTextureHeight = info.max_texture_height;
        } else {
            result.rendererInfoError = SDL_GetError();
        }
        SDL_GetRendererOutputSize(mRenderer, &result.rendererOutputWidth,
                                  &result.rendererOutputHeight);
    }
    SDL_Window* window = mWindow ? mWindow->SDL() : nullptr;
    if (window) {
        result.windowFlags = SDL_GetWindowFlags(window);
        SDL_GetWindowSize(window, &result.windowWidth, &result.windowHeight);
        result.displayIndex = SDL_GetWindowDisplayIndex(window);
        SDL_DisplayMode display{};
        if (result.displayIndex >= 0 &&
            SDL_GetCurrentDisplayMode(result.displayIndex, &display) == 0)
            result.displayRefreshHz = display.refresh_rate;
    }
    return result;
}

void RenderManager::Shutdown() noexcept
{
    ClearTextCache_();
    mDebugCmds.clear();
    mDeferredDebugCmds.clear();
    mTextureTintCache.clear();
    if (mPendingRenderTarget) SDL_DestroyTexture(mPendingRenderTarget);
    mPendingRenderTarget = nullptr;
    mHasPendingRenderTarget = false;
    CommitRenderTarget_(nullptr, 0, 0);
    mFrameOpen = false;
    if (mWhite) { SDL_DestroyTexture(mWhite); mWhite = nullptr; }
    if (mRenderer) { SDL_DestroyRenderer(mRenderer); mRenderer = nullptr; }
    mActiveCam = nullptr;
    mWindow = nullptr;
    mWinW = mWinH = 0;
}

void RenderManager::Begin(const Camera2D& cam) noexcept
{
    mFrameOpen = true;
    mDeferredDebugCmds.clear();
    const auto phaseStart = std::chrono::steady_clock::now();
    mDrawPhase = DrawPhase::Background;
    mTextureTintCache.clear();
    ++mTextCacheFrame;
    mActiveCam = &cam;
    const Vec2 view = cam.GetViewBase();
    const float zoom = cam.GetZoom();
    const Vec2 center = cam.GetCenter();
    if (mWorkEstimateMode == RenderDiagnostics::WorkEstimate::Visibility)
        mSelectorViewBounds = RenderDiagnostics::MakeViewBounds(
            {center.x, center.y, view.x / zoom, view.y / zoom});

    const Vec2I renderSize = GetRenderResolution();
    mWinW = renderSize.x; mWinH = renderSize.y;

    if (mRenderer) {
        mWorldTargetActive = mRenderTarget &&
            SDL_SetRenderTarget(mRenderer, mRenderTarget) == 0;
        if (mRenderTarget && !mWorldTargetActive) {
            LogError("Engine - Render", "Begin(): Could not bind world render target.", SDL_GetError());
            (void)SDL_SetRenderTarget(mRenderer, nullptr);
        }
        SDL_SetRenderDrawColor(mRenderer, mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a);
        SDL_RenderClear(mRenderer);
    }

    DrawBackground_(cam);
    mBackgroundMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - phaseStart).count();
    mDrawPhase = DrawPhase::World;
}

void RenderManager::End() noexcept
{
    mLastSdlPresentMs = 0.f;
    if (!mRenderer) return;
    BeginScreenPass_();

    // Measure only the SDL presentation call, not the cleanup below. Keep the
    // two clock reads out of ordinary frames when CSV profiling is disabled.
    const auto presentStart = mMeasureSdlPresent
        ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    // Delegate present to WindowManager to centralize backend-specific behavior.
    if (mWindow) mWindow->Present(mRenderer);
    else         SDL_RenderPresent(mRenderer);
    if (mMeasureSdlPresent)
        mLastSdlPresentMs = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - presentStart).count();

    for (SDL_Texture* t : mTempTextTextures) SDL_DestroyTexture(t);
    mTempTextTextures.clear();
    while (mTextCache.size() > 128)
    {
        auto oldest = std::min_element(mTextCache.begin(), mTextCache.end(),
            [](const auto& a, const auto& b) {
                return a.second.lastUsed < b.second.lastUsed;
            });
        if (oldest == mTextCache.end()) break;
        SDL_DestroyTexture(oldest->second.texture);
        mTextCache.erase(oldest);
    }

	for (SDL_Surface* s : mTempTextSurfaces) SDL_FreeSurface(s);
	mTempTextSurfaces.clear();

    mActiveCam = nullptr;
    mFrameOpen = false;
    ApplyPendingRenderTarget_();
    // Frame phase profiling is owned by Engine (steady_clock).
}

void RenderManager::SetVSync(bool enabled) noexcept
{
    if (mVSync == enabled) return;
    mVSync = enabled;

    // SDL cannot change VSYNC on an existing renderer on every platform.
    // Robust option: recreate the renderer with the requested acceleration and VSYNC settings (shutdown and reinitialize)
    if (!mWindow || !mRenderer) return;

    // Render targets belong to the renderer that created them. Reallocate the
    // low-resolution world target after recreating the renderer for VSync.
    const int renderWidth = mRenderWidth, renderHeight = mRenderHeight;
    if (mPendingRenderTarget) SDL_DestroyTexture(mPendingRenderTarget);
    mPendingRenderTarget = nullptr;
    mHasPendingRenderTarget = false;
    CommitRenderTarget_(nullptr, 0, 0);
    ClearTextCache_();
    mTextureTintCache.clear();
    if (mWhite) { SDL_DestroyTexture(mWhite); mWhite = nullptr; }
    SDL_DestroyRenderer(mRenderer);
    mRenderer = nullptr;

    Uint32 flags = 0;
    if (mAccelerated) flags |= SDL_RENDERER_ACCELERATED;
    if (mVSync)       flags |= SDL_RENDERER_PRESENTVSYNC;

    if (!CreateInitialRenderer_(flags))
    {
        LogError("Engine - Render", "SetVSync(): Could not recreate any allowed renderer backend.",
                 SDL_GetError());
        return;
    }
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    CreateWhiteTexture_();
    if (renderWidth > 0 && !SetRenderResolution(renderWidth, renderHeight))
        LogError("Engine - Render", "SetVSync(): World render resolution could not be restored.");
    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(mRenderer, &info) == 0 && info.name)
    {
        Console.Log("[RENDER-BACKEND] recreated actual=" + std::string(info.name) +
                    " flags=" + std::to_string(info.flags) +
                    " driver_index=" + std::to_string(mPreferredRendererDriverIndex),
                    "Engine - Render");
        mFirstChoiceRendererBackend = RendererBackendPolicy::Normalize(info.name);
    }
}

inline SDL_FRect RenderManager::WorldRectToScreen(const Camera2D& cam, int winW, int winH, const Rect& wrect) noexcept
{
    Vec2 s1 = cam.WorldToScreen(wrect.x, wrect.y, winW, winH);
    Vec2 s2 = cam.WorldToScreen(wrect.x + wrect.w, wrect.y + wrect.h, winW, winH);

    SDL_FRect r;
    r.x = std::min(s1.x, s2.x);
    r.y = std::min(s1.y, s2.y);
    r.w = std::fabs(s2.x - s1.x);
    r.h = std::fabs(s2.y - s1.y);
    return r;
}

/** @brief Rotated aabb in RenderManager.cpp.
 * @param dst Dst supplied to RenderManager.cpp.
 * @param center Center supplied to RenderManager.cpp.
 * @param deg Deg supplied to RenderManager.cpp.
 * @return Result of rotated aabb in RenderManager.cpp.
 */
static inline Rect RotatedAABB_(const SDL_FRect& dst, const SDL_FPoint& center, float deg) noexcept
{
    const float rad = deg * 3.14159265f / 180.f;
    const float c = std::cos(rad), s = std::sin(rad);

    const float cx = dst.x + center.x;
    const float cy = dst.y + center.y;

    const float xs[4] = { -center.x,        dst.w - center.x, dst.w - center.x, -center.x };
    const float ys[4] = { -center.y,       -center.y,       dst.h - center.y,  dst.h - center.y };

    float minx = 1e30f, miny = 1e30f;
    float maxx = -1e30f, maxy = -1e30f;

    for (int i = 0; i < 4; ++i) {
        const float rx = xs[i] * c - ys[i] * s;
        const float ry = xs[i] * s + ys[i] * c;
        const float px = cx + rx;
        const float py = cy + ry;
        if (px < minx) minx = px; if (px > maxx) maxx = px;
        if (py < miny) miny = py; if (py > maxy) maxy = py;
    }
    return Rect{ minx, miny, maxx - minx, maxy - miny };
}

void RenderManager::InvalidateTextureTint_(SDL_Texture* texture) noexcept
{
    mTextureTintCache.erase(texture);
}

void RenderManager::CountSubmission_() noexcept
{
    switch (mDrawPhase)
    {
        case DrawPhase::Background: ++mBackgroundDraws; break;
        case DrawPhase::World: ++mWorldDraws; break;
        case DrawPhase::Debug: ++mDebugDraws; break;
        case DrawPhase::UI: ++mUiDraws; break;
    }
}

void RenderManager::ApplyTextureTint_(SDL_Texture* texture, Color color) noexcept
{
    if (!texture) return;
    const auto it = mTextureTintCache.find(texture);
    if (it != mTextureTintCache.end() &&
        it->second.r == color.r && it->second.g == color.g &&
        it->second.b == color.b && it->second.a == color.a)
    {
        ++mTintCacheHits;
        return;
    }
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture, color.a);
    mTextureTintCache[texture] = color;
    ++mTintUpdates;
}

void RenderManager::DrawTextureImmediate_(const Texture& tex,
    const Rect& dstWorld,
    const Rect* srcPixels,
    float rotationDeg,
    Vec2 pivot01,
    Color tint,
    bool flipX,
    bool flipY) noexcept
{
    if ((tint.r < 0 || tint.r > 255) || (tint.g < 0 || tint.g > 255) || (tint.b < 0 || tint.b > 255) || (tint.a < 0 || tint.a > 255))
    {
        LogWarning("Engine - Render", "DrawTexture(): Tint color not valid.");
        return;
    }

    if (!mRenderer || !mActiveCam || tex.GetSDL() == nullptr) return;
    // 1) Mundo -> Pantalla
    SDL_FRect dst = WorldRectToScreen(*mActiveCam, mWinW, mWinH, dstWorld);

    // 2) Rotation center (pivot) in screen-space rectangle pixels
    SDL_FPoint center{ dst.w * pivot01.x, dst.h * pivot01.y };

    // 3) Precise rotated-quad culling using the helper
    Rect aabbRot = RotatedAABB_(dst, center, rotationDeg);
    Rect viewport{ 0.f, 0.f, (float)mWinW, (float)mWinH };
    if (!Intersects(aabbRot, viewport)) { ++mImmediateCulledCommands; return; }

    // 4) Source rectangle (texture pixels)
    SDL_Rect sdlSrc;
    const SDL_Rect* pSrc = nullptr;
    if (srcPixels) {
        sdlSrc.x = (int)srcPixels->x;
        sdlSrc.y = (int)srcPixels->y;
        sdlSrc.w = (int)(srcPixels->w == 0 ? tex.Width() : srcPixels->w);
        sdlSrc.h = (int)(srcPixels->h == 0 ? tex.Height() : srcPixels->h);
        pSrc = &sdlSrc;
    }

    // Time the actual world-texture SDL submission only when profiling is requested.
    const bool timed = mMeasureSpriteSubmissions && mDrawPhase == DrawPhase::World;
    const auto spriteStart = timed ? std::chrono::steady_clock::now()
                                   : std::chrono::steady_clock::time_point{};
    // 5) Color/alpha modulation
    ApplyTextureTint_(tex.GetSDL(), tint);

    // 6) Flip + Draw
    SDL_RendererFlip sdlFlip = SDL_FLIP_NONE;
    if (flipX) sdlFlip = (SDL_RendererFlip)(sdlFlip | SDL_FLIP_HORIZONTAL);
    if (flipY) sdlFlip = (SDL_RendererFlip)(sdlFlip | SDL_FLIP_VERTICAL);

    SDL_RenderCopyExF(mRenderer, tex.GetSDL(), pSrc, &dst, rotationDeg, &center, sdlFlip);
    if (timed) mSpriteSubmitMs += std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - spriteStart).count();
    ++nDrawCallsThisFrame;
    CountSubmission_();
    nRenderedSpritesThisFrame++;
}

void RenderManager::DrawTexturePrepared_(const DrawCmd& cmd) noexcept
{
    if (!mRenderer || !cmd.tex || !cmd.tex->GetSDL()) return;
    const Texture& tex = *cmd.tex;
    const Rect& d = cmd.preparedScreen;
    const SDL_FRect dst{d.x, d.y, d.w, d.h};
    const SDL_FPoint center{cmd.preparedPivot.x, cmd.preparedPivot.y};
    SDL_Rect sdlSrc{};
    const SDL_Rect* src = nullptr;
    if (cmd.hasSrc)
    {
        sdlSrc = {(int)cmd.srcPx.x, (int)cmd.srcPx.y,
                  (int)(cmd.srcPx.w == 0.f ? tex.Width() : cmd.srcPx.w),
                  (int)(cmd.srcPx.h == 0.f ? tex.Height() : cmd.srcPx.h)};
        src = &sdlSrc;
    }
    const bool timed = mMeasureSpriteSubmissions && mDrawPhase == DrawPhase::World;
    const auto spriteStart = timed ? std::chrono::steady_clock::now()
                                   : std::chrono::steady_clock::time_point{};
    ApplyTextureTint_(tex.GetSDL(), cmd.color);
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (cmd.flipX) flip = (SDL_RendererFlip)(flip | SDL_FLIP_HORIZONTAL);
    if (cmd.flipY) flip = (SDL_RendererFlip)(flip | SDL_FLIP_VERTICAL);
    SDL_RenderCopyExF(mRenderer, tex.GetSDL(), src, &dst, cmd.rotationDeg, &center, flip);
    if (timed) mSpriteSubmitMs += std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - spriteStart).count();
    ++nDrawCallsThisFrame;
    CountSubmission_();
    ++nRenderedSpritesThisFrame;
}

void RenderManager::PrepareSnapshot(Snapshot& snapshot, ParallelExecutor* workers,
                                    std::size_t minCommands) noexcept
{
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    mLastWaitMs = 0.f;
    mLastPreparedCount = 0;
    mLastCulledCount = 0;
    mLastWorkers = 0;
    if (!snapshot.valid) return;

    const RenderPreparation::Camera camera{
        snapshot.cam.centerX, snapshot.cam.centerY,
        snapshot.cam.baseViewW, snapshot.cam.baseViewH,
        snapshot.cam.zoom, snapshot.winW, snapshot.winH};
    // Worker callbacks access only immutable camera data and disjoint command slots.
    // In particular: no SDL, no asset access, no scene object or live Camera reads.
    const auto prepare = [&](std::size_t index) noexcept
    {
        DrawCmd& command = snapshot.cmds[index];
        if (command.type != DrawCmd::Type::WorldTex &&
            command.type != DrawCmd::Type::WorldRect) return;
        // All sprite geometry is projected from an immutable value snapshot.
        // No live scene objects, textures, rendering APIs or mutable camera access here.
        if (command.type == DrawCmd::Type::WorldTex && command.hasDeferredSpritePose) {
            const auto projected = SpriteProjection::Project(snapshot.spritePoses[command.spritePoseIndex]);
            command.dstWorld = {projected.x, projected.y, projected.w, projected.h};
        }
        const Rect& r = (command.type == DrawCmd::Type::WorldTex)
                          ? command.dstWorld : command.worldRect;
        const auto result = RenderPreparation::Prepare(camera,
            {r.x, r.y, r.w, r.h}, command.rotationDeg,
            command.pivot01.x, command.pivot01.y);
        command.hasPreparedGeometry = true;
        command.preparedVisible = result.visible;
        command.preparedScreen = {result.screen.x, result.screen.y,
                                  result.screen.w, result.screen.h};
        command.preparedPivot = {result.pivotX, result.pivotY};
    };
    if (workers && workers->WorkerCount() && snapshot.cmds.size() >= minCommands)
    {
        // Chunking limits atomic queue contention for high sprite counts.
        constexpr std::size_t chunkSize = 64;
        const auto chunks = (snapshot.cmds.size() + chunkSize - 1) / chunkSize;
        workers->Run(chunks, [&](std::size_t chunk) {
            const auto start = chunk * chunkSize;
            const auto end = std::min(start + chunkSize, snapshot.cmds.size());
            for (auto i = start; i < end; ++i) prepare(i);
        });
        mLastWaitMs = workers->LastWaitMs();
        mLastWorkers = workers->WorkerCount();
    }
    else
    {
        for (std::size_t i = 0; i < snapshot.cmds.size(); ++i) prepare(i);
    }
    for (const auto& cmd : snapshot.cmds)
    {
        if (cmd.hasPreparedGeometry)
        {
            ++mLastPreparedCount;
            if (!cmd.preparedVisible) ++mLastCulledCount;
        }
    }
    mLastPrepareMs = std::chrono::duration<float, std::milli>(
        Clock::now() - start).count();
}

void RenderManager::RenderSnapshot(const Snapshot& snap) noexcept
{
    if (!snap.valid) return;

    // Build a temporary camera using the captured state
    Camera2D camTmp;
    camTmp.mCenterX = snap.cam.centerX;
    camTmp.mCenterY = snap.cam.centerY;
    camTmp.mBaseViewWidth = snap.cam.baseViewW;
    camTmp.mBaseViewHeight = snap.cam.baseViewH;
    camTmp.mZoom = snap.cam.zoom;

    const Rect* pSrc = nullptr;

    Begin(camTmp);

    for (std::size_t index = 0; index < snap.cmds.size(); ++index)
    {
        // The engine records an explicit boundary between world and UI.
        // Do not infer it from ScreenRect: a scene may submit screen effects.
        if (index == snap.uiStartIndex)
            BeginScreenPass_();
        const auto& c = snap.cmds[index];
        // Gizmos were recorded with the world, but must not be rasterized
        // into its low-resolution texture. Replay them at native output size
        // at the world/UI boundary, preserving their camera-space alignment.
        if (c.debugDraw && c.type == DrawCmd::Type::WorldLine) {
            mDeferredDebugCmds.push_back(DebugCmd{
                DebugCmd::Type::Line, {}, c.x1, c.y1, c.x2, c.y2, c.color, false});
            continue;
        }
        if (c.debugDraw && c.type == DrawCmd::Type::WorldRect) {
            mDeferredDebugCmds.push_back(DebugCmd{
                DebugCmd::Type::RectI, c.worldRect, 0.f, 0.f, 0.f, 0.f,
                c.color, c.filled});
            continue;
        }
        mDrawPhase = c.debugDraw ? DrawPhase::Debug :
            (c.type == DrawCmd::Type::ScreenRect ||
             c.type == DrawCmd::Type::ScreenRoundedRect ||
             c.type == DrawCmd::Type::ScreenLine ||
             c.type == DrawCmd::Type::ScreenImage ||
             c.type == DrawCmd::Type::ScreenText ? DrawPhase::UI : DrawPhase::World);
        switch (c.type)
        {
        case DrawCmd::Type::WorldTex:
            if (c.hasPreparedGeometry)
            {
                if (c.preparedVisible && c.tex) DrawTexturePrepared_(c);
            }
            else if (c.tex)
            {
                pSrc = c.hasSrc ? &c.srcPx : nullptr;
                DrawTextureImmediate_(*c.tex, c.dstWorld, pSrc, c.rotationDeg, c.pivot01, c.color, c.flipX, c.flipY);
            }
            break;

        case DrawCmd::Type::WorldRect:
            if (!c.hasPreparedGeometry || c.preparedVisible)
                DrawRectImmediate_(c.worldRect, c.rotationDeg, c.pivot01, c.color, c.filled);
            break;

        case DrawCmd::Type::WorldLine:
            DrawLineImmediate_(c.x1, c.y1, c.x2, c.y2, c.color);
            break;

        case DrawCmd::Type::ScreenRect:
            DrawRectScreenImmediate_(c.dstScreen, c.color, c.filled);
            break;

        case DrawCmd::Type::ScreenRoundedRect:
            if (c.hasScreenCornerRadii)
                DrawRoundedRectCornersScreenImmediate_(
                    c.dstScreen, c.color, c.screenOutline, c.screenCornerRadii,
                    c.screenStrokeWidth, c.filled,
                    c.hasScreenClip ? &c.screenClip : nullptr);
            else
                DrawRoundedRectScreenImmediate_(c.dstScreen, c.color, c.screenOutline,
                                                c.screenCornerRadius, c.screenStrokeWidth,
                                                c.filled,
                                                c.hasScreenClip ? &c.screenClip : nullptr);
            break;

        case DrawCmd::Type::ScreenLine:
            DrawLineScreenImmediate_(c.x1, c.y1, c.x2, c.y2,
                                     c.color, c.screenStrokeWidth);
            break;

        case DrawCmd::Type::ScreenImage:
            pSrc = c.hasSrc ? &c.srcPx : nullptr;
            DrawImageScreenImmediate_(*c.tex, c.dstScreen, pSrc, c.color, c.flipX, c.flipY);
            break;

        case DrawCmd::Type::ScreenText:
            DrawTextScreenImmediate_(*c.font, c.text, c.textX, c.textY,
                                     c.color, c.textScale);
            
            break;
        }
    }
    // No UI commands is valid: the world/UI boundary can be at cmd.size().
    if (!mDeferredDebugCmds.empty()) BeginScreenPass_();
    mDrawPhase = DrawPhase::World;
}

void RenderManager::BeginRecord(const Camera2D* cam, Snapshot& out) noexcept
{
    mFrameOpen = true;
    out.valid = false;
    out.cmds.clear();
    out.uiStartIndex = 0;
    out.spritePoses.clear();

    if (mWindow)
    {
        Vec2I d = GetRenderResolution();
        out.winW = d.x;
        out.winH = d.y;
        mWinW = d.x;
        mWinH = d.y;
    }
    else
    {
        out.winW = mWinW;
        out.winH = mWinH;
    }

    if (!cam) return;

    // Copy plain camera state (thread-safe)
    out.cam.centerX = cam->mCenterX;
    out.cam.centerY = cam->mCenterY;
    out.cam.baseViewW = cam->mBaseViewWidth;
    out.cam.baseViewH = cam->mBaseViewHeight;
    out.cam.zoom = cam->mZoom;
    if (mWorkEstimateMode == RenderDiagnostics::WorkEstimate::Visibility)
        mSelectorViewBounds = RenderDiagnostics::MakeViewBounds(
            {out.cam.centerX, out.cam.centerY,
             out.cam.baseViewW / out.cam.zoom,
             out.cam.baseViewH / out.cam.zoom});

    // activar recording
    snapRecord = &out;
}

void RenderManager::EndRecord() noexcept
{
    if (snapRecord) snapRecord->valid = true;
    snapRecord = nullptr;
}


void RenderManager::DrawSprite_(const Texture& tex, const SpriteProjection::Pose& pose,
                                const Rect& srcPx, Color tint,
                                bool flipX, bool flipY) noexcept
{
    if (snapRecord && mDeferSpriteProjection) {
        if (mDrawPhase != DrawPhase::Debug) {
            ++mPublicWorldCommandsThisFrame;
            if (mWorkEstimateMode != RenderDiagnostics::WorkEstimate::Off) {
                const auto units = RenderDiagnostics::SpriteUnits(
                    pose.rotationX != 0.f || pose.rotationY != 0.f,
                    pose.rotationZ != 0.f,
                    pose.offsetX != 0.f || pose.offsetY != 0.f);
                mPublicWorldWorkUnitsThisFrame += units;
                const bool checkVisibility = NeedsVisibilityEstimate_();
                const bool offscreen = checkVisibility &&
                    RenderDiagnostics::DefinitelyOffscreenSprite(mSelectorViewBounds,
                        pose.x, pose.y, pose.width, pose.height,
                        pose.scaleX, pose.scaleY, pose.offsetX, pose.offsetY,
                        pose.pivotX, pose.pivotY);
                mSkippedVisibilitySprites +=
                    mWorkEstimateMode == RenderDiagnostics::WorkEstimate::Visibility && !checkVisibility ? 1u : 0u;
                AccumulateEffectiveWork_(RenderDiagnostics::EffectiveUnits(units, offscreen));
                mEstimatedOffscreenSprites += offscreen ? 1u : 0u;
            }
        }
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::WorldTex;
        c.tex = &tex;
        c.hasSrc = true;
        c.srcPx = srcPx;
        c.rotationDeg = pose.rotationZ;
        c.pivot01 = {pose.pivotX, pose.pivotY};
        c.color = tint;
        c.flipX = flipX;
        c.flipY = flipY;
        c.hasDeferredSpritePose = true;
        c.spritePoseIndex = static_cast<std::uint32_t>(snapRecord->spritePoses.size());
        snapRecord->spritePoses.push_back(pose);
        return;
    }
    // Immediate draw and A/B 'main' mode use the exact same pure CPU
    // projection as workers mode. This isolates deferral in the benchmark.
    const auto projected = SpriteProjection::Project(pose);
    const Rect dst{projected.x, projected.y, projected.w, projected.h};
    if (mWorkEstimateMode != RenderDiagnostics::WorkEstimate::Off) {
        mSpriteWorkOverride = RenderDiagnostics::SpriteUnits(
            pose.rotationX != 0.f || pose.rotationY != 0.f,
            pose.rotationZ != 0.f,
            pose.offsetX != 0.f || pose.offsetY != 0.f);
        const bool checkVisibility = NeedsVisibilityEstimate_();
        const bool offscreen = checkVisibility &&
            RenderDiagnostics::DefinitelyOffscreenSprite(mSelectorViewBounds,
                pose.x, pose.y, pose.width, pose.height,
                pose.scaleX, pose.scaleY, pose.offsetX, pose.offsetY,
                pose.pivotX, pose.pivotY);
        mSkippedVisibilitySprites +=
            mWorkEstimateMode == RenderDiagnostics::WorkEstimate::Visibility && !checkVisibility ? 1u : 0u;
        mSpriteEffectiveWorkOverride = RenderDiagnostics::EffectiveUnits(mSpriteWorkOverride, offscreen);
        mEstimatedOffscreenSprites += offscreen ? 1u : 0u;
    }
    DrawTexture(tex, dst, &srcPx, pose.rotationZ,
                {pose.pivotX, pose.pivotY}, tint, flipX, flipY);
    mSpriteWorkOverride = 0;
    mSpriteEffectiveWorkOverride = 0;
}

void RenderManager::DrawTexture(const Texture& tex, const Rect& dstWorld, const Rect* srcPixels, float rotationDeg, Vec2 pivot01, Color tint, bool flipX, bool flipY) noexcept
{
    if (mDrawPhase != DrawPhase::Debug) {
        ++mPublicWorldCommandsThisFrame;
        if (mWorkEstimateMode != RenderDiagnostics::WorkEstimate::Off) {
            const auto units = mSpriteWorkOverride ? mSpriteWorkOverride :
                RenderDiagnostics::RectangleUnits(rotationDeg != 0.f);
            mPublicWorldWorkUnitsThisFrame += units;
            AccumulateEffectiveWork_(mSpriteWorkOverride ? mSpriteEffectiveWorkOverride : units);
        }
    }
	// --- RECORDING MODE ---
    if (snapRecord)
    {
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::WorldTex;
        c.tex = &tex;
        c.dstWorld = dstWorld;
        c.hasSrc = (srcPixels != nullptr);
        if (srcPixels) c.srcPx = *srcPixels;
        c.rotationDeg = rotationDeg;
        c.pivot01 = pivot01;
        c.color = tint;
        c.flipX = flipX;
        c.flipY = flipY;
		return;
    }

	// --- IMMEDIATE MODE ---
	DrawTextureImmediate_(tex, dstWorld, srcPixels, rotationDeg, pivot01, tint, flipX, flipY);
}

void RenderManager::DrawRect(const Rect& worldRect, const Color color, const bool filled) noexcept
{
    DrawRect(worldRect, /*rotationDeg=*/0.f, /*pivot01=*/{ 0.5f, 0.5f }, color, filled);
}

void RenderManager::DrawRect(const Rect& worldRect, float rotationDeg, Vec2 pivot01, Color color, bool filled) noexcept
{
    if (mDrawPhase != DrawPhase::Debug) {
        ++mPublicWorldCommandsThisFrame;
        if (mWorkEstimateMode != RenderDiagnostics::WorkEstimate::Off) {
            const auto units = RenderDiagnostics::RectangleUnits(rotationDeg != 0.f);
            mPublicWorldWorkUnitsThisFrame += units;
            AccumulateEffectiveWork_(units);
        }
    }
    // --- RECORDING MODE ---
    if (snapRecord)
    {
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::WorldRect;
        c.debugDraw = (mDrawPhase == DrawPhase::Debug);
        c.color = color;
        c.worldRect = worldRect;
        c.filled = filled;

        // Reutilizamos estos campos aunque el comentario ponga "WorldTex"
        c.rotationDeg = rotationDeg;
        c.pivot01 = pivot01;

        return;
    }

    // --- IMMEDIATE MODE ---
    DrawRectImmediate_(worldRect, rotationDeg, pivot01, color, filled);
}

void RenderManager::DrawLine(float x1, float y1, float x2, float y2, Color color) noexcept
{
    // --- RECORDING MODE ---
    if (snapRecord)
    {
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::WorldLine;
        c.debugDraw = (mDrawPhase == DrawPhase::Debug);
        c.color = color;
        c.x1 = x1; c.y1 = y1;
        c.x2 = x2; c.y2 = y2;

        return;
    }

    // --- IMMEDIATE MODE ---
    DrawLineImmediate_(x1, y1, x2, y2, color);
}

void RenderManager::DrawRectScreen(const Rect& screenRect, Color color, bool filled) noexcept
{
    // --- RECORDING MODE ---
    if (snapRecord)
    {
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::ScreenRect;
        c.color = color;
        c.dstScreen = screenRect;
        c.filled = filled; // also reused here

        return;
    }

    // --- IMMEDIATE MODE ---
    // Screen-space submissions are UI even when issued during Scene::Render.
    // Preserve the caller phase; the snapshot replay path already uses UI.
    const DrawPhase previousPhase = mDrawPhase;
    mDrawPhase = DrawPhase::UI;
    DrawRectScreenImmediate_(screenRect, color, filled);
    mDrawPhase = previousPhase;

}

void RenderManager::DrawRoundedRectScreen(const Rect& screenRect, Color fill,
                                          Color outline, float cornerRadius,
                                          float borderWidth, bool filled) noexcept
{
    if (snapRecord) {
        DrawCmd& command = snapRecord->cmds.emplace_back();
        command.type = DrawCmd::Type::ScreenRoundedRect;
        command.color = fill;
        command.screenOutline = outline;
        command.dstScreen = screenRect;
        command.screenCornerRadius = cornerRadius;
        command.screenStrokeWidth = borderWidth;
        command.filled = filled;
        return;
    }
    const DrawPhase previousPhase = mDrawPhase;
    mDrawPhase = DrawPhase::UI;
    DrawRoundedRectScreenImmediate_(screenRect, fill, outline,
                                    cornerRadius, borderWidth, filled);
    mDrawPhase = previousPhase;
}

void RenderManager::DrawRoundedRectCornersScreen_(const Rect& screenRect,
    Color fill, Color outline, xengine::detail::ui::CornerRadii radii,
    float borderWidth, bool filled) noexcept
{
    if (snapRecord) {
        DrawCmd& command = snapRecord->cmds.emplace_back();
        command.type = DrawCmd::Type::ScreenRoundedRect;
        command.color = fill;
        command.screenOutline = outline;
        command.dstScreen = screenRect;
        command.screenCornerRadii = radii;
        command.hasScreenCornerRadii = true;
        command.screenStrokeWidth = borderWidth;
        command.filled = filled;
        return;
    }
    const DrawPhase previousPhase = mDrawPhase;
    mDrawPhase = DrawPhase::UI;
    DrawRoundedRectCornersScreenImmediate_(screenRect, fill, outline,
                                           radii, borderWidth, filled);
    mDrawPhase = previousPhase;
}

void RenderManager::DrawClippedRoundedFillScreen_(const Rect& screenRect,
                                                  const Rect& clip, Color fill,
                                                  float cornerRadius) noexcept
{
    if (snapRecord) {
        DrawCmd& command = snapRecord->cmds.emplace_back();
        command.type = DrawCmd::Type::ScreenRoundedRect;
        command.color = fill;
        command.screenOutline = Color{0, 0, 0, 0};
        command.dstScreen = screenRect;
        command.screenCornerRadius = cornerRadius;
        command.screenStrokeWidth = 0.f;
        command.filled = true;
        command.screenClip = clip;
        command.hasScreenClip = true;
        return;
    }
    const DrawPhase previousPhase = mDrawPhase;
    mDrawPhase = DrawPhase::UI;
    DrawRoundedRectScreenImmediate_(screenRect, fill, Color{0, 0, 0, 0},
                                    cornerRadius, 0.f, true, &clip);
    mDrawPhase = previousPhase;
}

void RenderManager::DrawClippedRoundedFillScreen_(const Rect& screenRect,
    const Rect& clip, Color fill, xengine::detail::ui::CornerRadii radii) noexcept
{
    if (snapRecord) {
        DrawCmd& command = snapRecord->cmds.emplace_back();
        command.type = DrawCmd::Type::ScreenRoundedRect;
        command.color = fill;
        command.screenOutline = Color{0, 0, 0, 0};
        command.dstScreen = screenRect;
        command.screenCornerRadii = radii;
        command.hasScreenCornerRadii = true;
        command.screenStrokeWidth = 0.f;
        command.filled = true;
        command.screenClip = clip;
        command.hasScreenClip = true;
        return;
    }
    const DrawPhase previousPhase = mDrawPhase;
    mDrawPhase = DrawPhase::UI;
    DrawRoundedRectCornersScreenImmediate_(screenRect, fill, Color{0, 0, 0, 0},
                                           radii, 0.f, true, &clip);
    mDrawPhase = previousPhase;
}

void RenderManager::DrawLineScreen(float x1, float y1, float x2, float y2,
                                   Color color, float thickness) noexcept
{
    if (snapRecord) {
        DrawCmd& command = snapRecord->cmds.emplace_back();
        command.type = DrawCmd::Type::ScreenLine;
        command.x1 = x1;
        command.y1 = y1;
        command.x2 = x2;
        command.y2 = y2;
        command.color = color;
        command.screenStrokeWidth = thickness;
        return;
    }
    const DrawPhase previousPhase = mDrawPhase;
    mDrawPhase = DrawPhase::UI;
    DrawLineScreenImmediate_(x1, y1, x2, y2, color, thickness);
    mDrawPhase = previousPhase;
}

void RenderManager::DrawImageScreen(const Texture& tex, const Rect& dstScreen, const Rect* srcPixels, Color tint, bool flipX, bool flipY) noexcept
{
    // --- RECORDING MODE ---
    if (snapRecord)
    {
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::ScreenImage;

        c.tex = &tex;
        c.dstScreen = dstScreen;

        c.hasSrc = (srcPixels != nullptr);
        if (srcPixels) c.srcPx = *srcPixels;

        c.color = tint;
        c.flipX = flipX;
        c.flipY = flipY;

        return;
    }

    // --- IMMEDIATE MODE ---
    const DrawPhase previousPhase = mDrawPhase;
    mDrawPhase = DrawPhase::UI;
    DrawImageScreenImmediate_(tex, dstScreen, srcPixels, tint, flipX, flipY);
    mDrawPhase = previousPhase;
}

void RenderManager::DrawTextScreen(const Font& font, const std::string& text,
                                   float x_px, float y_px, Color color, float scale) noexcept
{
    if (text.empty()) return;
    scale = std::isfinite(scale) ? std::clamp(scale, 0.01f, 32.f) : 1.f;

    // --- RECORDING MODE ---
    if (snapRecord)
    {
        DrawCmd& c = snapRecord->cmds.emplace_back();
        c.type = DrawCmd::Type::ScreenText;

        c.font = &font;
        c.text = text;
        c.textX = x_px;
        c.textY = y_px;
        c.color = color;
        c.textScale = scale;

        return;
    }

    // --- IMMEDIATE MODE ---
    const DrawPhase previousPhase = mDrawPhase;
    mDrawPhase = DrawPhase::UI;
    DrawTextScreenImmediate_(font, text, x_px, y_px, color, scale);
    mDrawPhase = previousPhase;
}

void RenderManager::ClearTextCache_() noexcept
{
    for (auto& entry : mTextCache)
        if (entry.second.texture) SDL_DestroyTexture(entry.second.texture);
    mTextCache.clear();
    for (auto& variant : mScaledFonts)
        if (variant.second) TTF_CloseFont(variant.second);
    mScaledFonts.clear();
}

TTF_Font* RenderManager::FontForScreenScale_(const Font& font, float scale) noexcept
{
    TTF_Font* original = font.GetSDL();
    if (!original || !std::isfinite(scale) || scale <= 0.f ||
        font.mPointSize <= 0 || font.mSourcePath.empty()) return original;

    const int pointSize = std::clamp(
        static_cast<int>(std::lround(static_cast<double>(font.mPointSize) * scale)),
        1, 512);
    if (pointSize == font.mPointSize) return original;
    const std::string key = std::to_string(
        reinterpret_cast<std::uintptr_t>(original)) + ":" +
        std::to_string(pointSize);
    if (const auto found = mScaledFonts.find(key); found != mScaledFonts.end())
        return found->second;

    TTF_Font* scaled = TTF_OpenFont(font.mSourcePath.c_str(), pointSize);
    if (!scaled) return original; // Keep embedded/unavailable-font fallback usable.
    TTF_SetFontStyle(scaled, TTF_GetFontStyle(original));
    TTF_SetFontOutline(scaled, TTF_GetFontOutline(original));
    TTF_SetFontHinting(scaled, TTF_GetFontHinting(original));
    TTF_SetFontKerning(scaled, TTF_GetFontKerning(original));
    if (mScaledFonts.size() >= 32) {
        for (auto& variant : mScaledFonts) TTF_CloseFont(variant.second);
        mScaledFonts.clear();
    }
    mScaledFonts.emplace(key, scaled);
    return scaled;
}

Vec2I RenderManager::MeasureTextScreen(const Font& font, const std::string& text,
                                      float scale) noexcept
{
    if (!font.GetSDL() || text.empty()) return {0, 0};
    const float safeScale = std::clamp(
        std::isfinite(scale) ? scale : 1.f, 0.05f, 8.f);
    TTF_Font* scaled = FontForScreenScale_(font, safeScale);
    int width = 0, height = 0;
    if (!scaled || TTF_SizeUTF8(scaled, text.c_str(), &width, &height) != 0)
        return {0, 0};
    if (scaled != font.GetSDL()) return {width, height};
    return {static_cast<int>(std::lround(width * safeScale)),
            static_cast<int>(std::lround(height * safeScale))};
}

void RenderManager::DrawTextScreenImmediate_(const Font& font, const std::string& text,
                                              float x, float y, Color color, float scale) noexcept
{
    if (!mRenderer || !mWindow || !font.GetSDL() || text.empty()) return;
    // The screen pass uses the actual SDL renderer output, not the internal
    // world render target or a DPI-virtualized logical window size.
    const Vec2I size{mWinW, mWinH};
    if (size.x <= 0 || size.y <= 0) return;
    const float safeScale = std::clamp(
        std::isfinite(scale) ? scale : 1.f, 0.05f, 8.f);
    TTF_Font* rasterFont = FontForScreenScale_(font, safeScale);
    if (!rasterFont) return;
    const bool rasterAtOutputSize = rasterFont != font.GetSDL();
    const auto outputExtent = [&](int pixels) -> int {
        return std::max(1, rasterAtOutputSize ? pixels :
            static_cast<int>(std::lround(static_cast<double>(pixels) * safeScale)));
    };
    const int drawX = static_cast<int>(std::lround(x));
    const int drawY = static_cast<int>(std::lround(y));
    // Cache only bounded-size text to avoid retaining arbitrarily large content.
    const bool cacheable = text.size() <= 160;
    std::string key;
    if (cacheable)
    {
        key = std::to_string(reinterpret_cast<std::uintptr_t>(font.GetSDL())) + ":" +
              std::to_string(static_cast<int>(std::lround(safeScale * 1024.f))) + ":" +
              std::to_string(color.r) + "," + std::to_string(color.g) + "," +
              std::to_string(color.b) + "," + std::to_string(color.a) + ":" + text;
        auto found = mTextCache.find(key);
        if (found != mTextCache.end())
        {
            found->second.lastUsed = mTextCacheFrame;
            const auto& e = found->second;
            const int drawnWidth = outputExtent(e.width);
            const int drawnHeight = outputExtent(e.height);
            if (drawX >= size.x || drawY >= size.y ||
                drawX + drawnWidth <= 0 || drawY + drawnHeight <= 0) return;
            SDL_Rect dst{drawX, drawY, drawnWidth, drawnHeight};
            SDL_RenderCopy(mRenderer, e.texture, nullptr, &dst);
            ++nUIDrawCallsThisFrame;
            CountSubmission_();
            return;
        }
    }
    int width = 0, height = 0;
    if (TTF_SizeUTF8(rasterFont, text.c_str(), &width, &height) == 0)
    {
        if (drawX >= size.x || drawY >= size.y ||
            drawX + outputExtent(width) <= 0 ||
            drawY + outputExtent(height) <= 0) return;
    }
    SDL_Surface* surface = TTF_RenderUTF8_Blended(rasterFont, text.c_str(),
        SDL_Color{color.r,color.g,color.b,color.a});
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(mRenderer, surface);
    width = surface->w;
    height = surface->h;
    SDL_FreeSurface(surface);
    if (!texture) return;
    if (!rasterAtOutputSize && std::fabs(safeScale - 1.f) > 0.001f)
        (void)SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    SDL_Rect dst{drawX, drawY, outputExtent(width), outputExtent(height)};
    SDL_RenderCopy(mRenderer, texture, nullptr, &dst);
    ++nUIDrawCallsThisFrame;
    CountSubmission_();
    if (!cacheable)
    {
        mTempTextTextures.push_back(texture);
        return;
    }
    mTextCache.emplace(std::move(key), TextCacheEntry{texture, width, height, mTextCacheFrame});
}

void RenderManager::DrawRectImmediate_(const Rect& worldRect,
    const float rotationDeg,
    const Vec2 pivot01,
    const Color color,
    const bool filled) noexcept
{
    if (!mRenderer || !mActiveCam) return;

    if ((color.r < 0 || color.r > 255) || (color.g < 0 || color.g > 255) || (color.b < 0 || color.b > 255) || (color.a < 0 || color.a > 255))
    {
        LogWarning("Engine - Render", "DrawRect(): Color not valid.");
        return;
    }

    // 1) World to screen (unrotated rectangle AABB)
    SDL_FRect dst = WorldRectToScreen(*mActiveCam, mWinW, mWinH, worldRect);

    // 2) Rotation center (pivot) in pixels
    SDL_FPoint center{ dst.w * pivot01.x, dst.h * pivot01.y };

    // 3) Cull using the rotated quad AABB (precise)
    Rect aabbRot = RotatedAABB_(dst, center, rotationDeg);
    Rect viewport{ 0.f, 0.f, (float)mWinW, (float)mWinH };
    if (!Intersects(aabbRot, viewport)) { ++mImmediateCulledCommands; return; }

    // 4) Dibujo
    if (filled) {
        // Fill: use the white 1x1 texture with color modulation
        if (!mWhite) return;
        ApplyTextureTint_(mWhite, color);

        SDL_RenderCopyExF(mRenderer, mWhite, nullptr, &dst, rotationDeg, &center, SDL_FLIP_NONE);

    }
    else {
        // Border: rotate four corners and draw lines
        const float rad = rotationDeg * 3.14159265f / 180.f;
        const float c = std::cos(rad), s = std::sin(rad);
        const float cx = dst.x + center.x, cy = dst.y + center.y;

        SDL_FPoint p[5];
        const float xs[4] = { -center.x,        dst.w - center.x, dst.w - center.x, -center.x };
        const float ys[4] = { -center.y,       -center.y,       dst.h - center.y,  dst.h - center.y };
        for (int i = 0; i < 4; ++i) {
            const float rx = xs[i] * c - ys[i] * s;
            const float ry = xs[i] * s + ys[i] * c;
            p[i].x = cx + rx;
            p[i].y = cy + ry;
        }
        p[4] = p[0]; // cerrar

        SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(mRenderer, color.r, color.g, color.b, color.a);

        SDL_RenderDrawLinesF(mRenderer, p, 5);
    }
	nDrawCallsThisFrame++;
    CountSubmission_();
}

void RenderManager::DrawLineImmediate_(float x1, float y1, float x2, float y2, Color color) noexcept
{
    if ((color.r < 0 || color.r > 255) || (color.g < 0 || color.g > 255) || (color.b < 0 || color.b > 255) || (color.a < 0 || color.a > 255))
    {
        LogWarning("Engine - Render", "DrawLine(): Color not valid.");
        return;
    }

    if (!mRenderer || !mActiveCam) return;

    Vec2 s1 = mActiveCam->WorldToScreen(x1, y1, mWinW, mWinH);
    Vec2 s2 = mActiveCam->WorldToScreen(x2, y2, mWinW, mWinH);
    // Conservative AABB test: SDL would otherwise be called for fully invisible lines.
    if (std::max(s1.x, s2.x) < 0.f || std::min(s1.x, s2.x) >= float(mWinW) ||
        std::max(s1.y, s2.y) < 0.f || std::min(s1.y, s2.y) >= float(mWinH)) { ++mImmediateCulledCommands; return; }

    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(mRenderer, color.r, color.g, color.b, color.a);

    SDL_RenderDrawLineF(mRenderer, s1.x, s1.y, s2.x, s2.y);
    // Adjacent rounded-collider segments share vertices, but SDL backends can
    // leave a pixel-sized hole where independently rasterized lines meet.
    if (mDrawPhase == DrawPhase::Debug) {
        SDL_RenderDrawPointF(mRenderer, s1.x, s1.y);
        SDL_RenderDrawPointF(mRenderer, s2.x, s2.y);
    }
	nDrawCallsThisFrame++;
    CountSubmission_();
}

void RenderManager::DrawRectScreenImmediate_(const Rect& r, Color color, bool filled) noexcept
{
    if ((color.r < 0 || color.r > 255) || (color.g < 0 || color.g > 255) || (color.b < 0 || color.b > 255) || (color.a < 0 || color.a > 255))
    {
        LogWarning("Engine - Render", "DrawRectScreen(): Color not valid.");
        return;
    }

    if (!mRenderer || !mWindow) return;

    const Vec2I wS{mWinW, mWinH};
    if (wS.x <= 0 || wS.y <= 0) return;

    SDL_Rect sr{ (int)std::floor(r.x), (int)std::floor(r.y),
                 (int)std::ceil(r.w), (int)std::ceil(r.h) };
    if (sr.w <= 0 || sr.h <= 0) return;
    
    SDL_Rect screen{ 0, 0, wS.x, wS.y };
    SDL_Rect clipped;

    if (!SDL_Intersect(sr, screen, clipped)) return;

    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(mRenderer, color.r, color.g, color.b, color.a);
    if (filled) SDL_RenderFillRect(mRenderer, &clipped);
    else        SDL_RenderDrawRect(mRenderer, &clipped);
	nUIDrawCallsThisFrame++;
    CountSubmission_();
}

void RenderManager::DrawRoundedRectScreenImmediate_(const Rect& r, Color fill,
    Color outline, float cornerRadius, float borderWidth, bool filled,
    const Rect* clip) noexcept
{
    if (!mRenderer || !mWindow || r.w <= 0.f || r.h <= 0.f ||
        !std::isfinite(r.x) || !std::isfinite(r.y) ||
        !std::isfinite(r.w) || !std::isfinite(r.h)) return;
    if (r.x >= float(mWinW) || r.y >= float(mWinH) ||
        r.x + r.w <= 0.f || r.y + r.h <= 0.f) return;

    // Align shape edges with physical pixels. Fractional canvas scales (e.g.
    // 1.5x) otherwise put one-pixel borders between pixel columns/rows.
    const float left = std::round(r.x);
    const float top = std::round(r.y);
    const Rect snapped{left, top,
                       std::max(1.f, std::round(r.x + r.w) - left),
                       std::max(1.f, std::round(r.y + r.h) - top)};
    const float radius = std::clamp(
        std::round(std::isfinite(cornerRadius) ? cornerRadius : 0.f),
        0.f, std::min(snapped.w, snapped.h) * 0.5f);
    const float requestedStroke = std::isfinite(borderWidth) ? borderWidth : 0.f;
    const float stroke = requestedStroke > 0.f
        ? std::clamp(std::max(1.f, std::round(requestedStroke)),
                     0.f, std::min(snapped.w, snapped.h) * 0.5f)
        : 0.f;

    // SDL clipping is applied only to this one draw and then restored. A
    // dropdown row draws the complete rounded popup interior behind a
    // rectangular clip; its corners therefore match the actual popup border
    // without applying four independent rounded corners to every row.
    SDL_Rect oldClip{};
    const bool hadClip = SDL_RenderIsClipEnabled(mRenderer) == SDL_TRUE;
    if (clip) {
        SDL_RenderGetClipRect(mRenderer, &oldClip);
        SDL_Rect requested{
            static_cast<int>(std::round(clip->x)),
            static_cast<int>(std::round(clip->y)),
            static_cast<int>(std::round(clip->w)),
            static_cast<int>(std::round(clip->h))};
        SDL_Rect clipped = requested;
        if (hadClip && !SDL_Intersect(requested, oldClip, clipped)) return;
        if (clipped.w <= 0 || clipped.h <= 0 ||
            SDL_RenderSetClipRect(mRenderer, &clipped) != 0) return;
    }
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    if (filled) FilledPolygon_(mRenderer, snapped, radius, fill);
    RoundedOutline_(mRenderer, snapped, radius, stroke, outline);
    if (clip) SDL_RenderSetClipRect(mRenderer, hadClip ? &oldClip : nullptr);
    ++nUIDrawCallsThisFrame;
    CountSubmission_();
}

void RenderManager::DrawRoundedRectCornersScreenImmediate_(const Rect& r, Color fill,
    Color outline, xengine::detail::ui::CornerRadii radii, float borderWidth,
    bool filled, const Rect* clip) noexcept
{
    if (!mRenderer || !mWindow || r.w <= 0.f || r.h <= 0.f ||
        !std::isfinite(r.x) || !std::isfinite(r.y) ||
        !std::isfinite(r.w) || !std::isfinite(r.h)) return;
    if (r.x >= float(mWinW) || r.y >= float(mWinH) ||
        r.x + r.w <= 0.f || r.y + r.h <= 0.f) return;

    const float left = std::round(r.x);
    const float top = std::round(r.y);
    const Rect snapped{left, top,
                       std::max(1.f, std::round(r.x + r.w) - left),
                       std::max(1.f, std::round(r.y + r.h) - top)};
    radii.topLeft = std::round(radii.topLeft);
    radii.topRight = std::round(radii.topRight);
    radii.bottomRight = std::round(radii.bottomRight);
    radii.bottomLeft = std::round(radii.bottomLeft);
    radii = xengine::detail::ui::FitCornerRadii(radii, snapped.w, snapped.h);
    const float requestedStroke = std::isfinite(borderWidth) ? borderWidth : 0.f;
    const float stroke = requestedStroke > 0.f
        ? std::clamp(std::max(1.f, std::round(requestedStroke)),
                     0.f, std::min(snapped.w, snapped.h) * 0.5f)
        : 0.f;

    SDL_Rect oldClip{};
    const bool hadClip = SDL_RenderIsClipEnabled(mRenderer) == SDL_TRUE;
    if (clip) {
        SDL_RenderGetClipRect(mRenderer, &oldClip);
        const SDL_Rect requested{static_cast<int>(std::round(clip->x)),
                                 static_cast<int>(std::round(clip->y)),
                                 static_cast<int>(std::round(clip->w)),
                                 static_cast<int>(std::round(clip->h))};
        SDL_Rect clipped = requested;
        if (hadClip && !SDL_Intersect(requested, oldClip, clipped)) return;
        if (clipped.w <= 0 || clipped.h <= 0 ||
            SDL_RenderSetClipRect(mRenderer, &clipped) != 0) return;
    }
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    if (filled) FilledPolygonCorners_(mRenderer, snapped, radii, fill);
    RoundedOutlineCorners_(mRenderer, snapped, radii, stroke, outline);
    if (clip) SDL_RenderSetClipRect(mRenderer, hadClip ? &oldClip : nullptr);
    ++nUIDrawCallsThisFrame;
    CountSubmission_();
}

void RenderManager::DrawLineScreenImmediate_(float x1, float y1, float x2, float y2,
                                             Color color, float thickness) noexcept
{
    if (!mRenderer || !mWindow || color.a <= 0 ||
        !std::isfinite(x1) || !std::isfinite(y1) ||
        !std::isfinite(x2) || !std::isfinite(y2) ||
        !std::isfinite(thickness) || thickness <= 0.f) return;
    const float half = thickness * 0.5f;
    if (std::max(x1, x2) + half < 0.f || std::min(x1, x2) - half >= float(mWinW) ||
        std::max(y1, y2) + half < 0.f || std::min(y1, y2) - half >= float(mWinH)) return;
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    if (thickness <= 1.f) {
        SDL_SetRenderDrawColor(mRenderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawLineF(mRenderer, x1, y1, x2, y2);
    } else {
        const float dx = x2 - x1;
        const float dy = y2 - y1;
        const float length = std::hypot(dx, dy);
        if (length <= 0.001f) return;
        const float nx = -dy * half / length;
        const float ny = dx * half / length;
        const float cap = mDrawPhase == DrawPhase::Debug ? 1.f : 0.f;
        const float ax = x1 - dx * cap / length;
        const float ay = y1 - dy * cap / length;
        const float bx = x2 + dx * cap / length;
        const float by = y2 + dy * cap / length;
        const SDL_Color tint = VertexColor_(color);
        const SDL_Vertex vertices[4]{
            {{ax + nx, ay + ny}, tint, {0.f, 0.f}},
            {{ax - nx, ay - ny}, tint, {0.f, 0.f}},
            {{bx + nx, by + ny}, tint, {0.f, 0.f}},
            {{bx - nx, by - ny}, tint, {0.f, 0.f}},
        };
        const int triangles[6]{0, 1, 2, 1, 3, 2};
        SDL_RenderGeometry(mRenderer, nullptr, vertices, 4, triangles, 6);
    }
    if (mDrawPhase == DrawPhase::Debug) ++nDrawCallsThisFrame;
    else ++nUIDrawCallsThisFrame;
    CountSubmission_();
}

void RenderManager::DrawImageScreenImmediate_(const Texture& tex, const Rect& dst, const Rect* src, Color tint, bool flipX, bool flipY) noexcept
{
    if ((tint.r < 0 || tint.r > 255) || (tint.g < 0 || tint.g > 255) || (tint.b < 0 || tint.b > 255) || (tint.a < 0 || tint.a > 255))
    {
        LogWarning("Engine - Render", "DrawImageScreen(): Tint not valid.");
        return;
    }

    if (!mRenderer || !tex.GetSDL()) return;

    int texW = tex.Width();
    int texH = tex.Height();
    if (texW <= 0 || texH <= 0) return;

    SDL_Rect s{};
    if (src) {
        // Clamp >= 0
        int sx = std::max(0, (int)src->x);
        int sy = std::max(0, (int)src->y);
        int sw = std::max(0, (int)src->w);
        int sh = std::max(0, (int)src->h);

        // Clamp to texture size
        if (sx >= texW || sy >= texH || sw <= 0 || sh <= 0) return;
        if (sx + sw > texW) sw = texW - sx;
        if (sy + sh > texH) sh = texH - sy;

        s = { sx, sy, sw, sh };
    }
    else {
        s = { 0, 0, texW, texH };
    }

    const Vec2I wS{mWinW, mWinH};
    if (wS.x <= 0 || wS.y <= 0) return;

    SDL_Rect d{ (int)std::floor(dst.x), (int)std::floor(dst.y),
                (int)std::ceil(dst.w), (int)std::ceil(dst.h) };
    if (d.w <= 0 || d.h <= 0) return;

    SDL_Rect screen{ 0, 0, wS.x, wS.y };
    SDL_Rect di; // dst intersectado
    if (!SDL_Intersect(d, screen, di)) return;

    const float sx = (float)d.w / (float)s.w;
    const float sy = (float)d.h / (float)s.h;

    const int cutL = di.x - d.x;
    const int cutT = di.y - d.y;

    SDL_Rect sAdj = s;
    sAdj.x += (int)std::floor(cutL / sx);
    sAdj.y += (int)std::floor(cutT / sy);

    sAdj.w -= (int)std::floor((d.w - di.w) / sx);
    sAdj.h -= (int)std::floor((d.h - di.h) / sy);

    if (sAdj.x < 0) sAdj.x = 0;
    if (sAdj.y < 0) sAdj.y = 0;
    if (sAdj.x + sAdj.w > texW) sAdj.w = texW - sAdj.x;
    if (sAdj.y + sAdj.h > texH) sAdj.h = texH - sAdj.y;
    if (sAdj.w <= 0 || sAdj.h <= 0) return;

    SDL_Rect dFinal = di;

    ApplyTextureTint_(tex.GetSDL(), tint);
    const SDL_RendererFlip f = static_cast<SDL_RendererFlip>(
        (flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE) |
        (flipY ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE)
        );

    SDL_RenderCopyEx(mRenderer, tex.GetSDL(), &sAdj, &dFinal, 0.0, nullptr, f);
	nUIDrawCallsThisFrame++;
    CountSubmission_();
}

bool RenderManager::Intersects(const Rect& a, const Rect& b) noexcept
{
    const float ax2 = a.x + a.w;
    const float ay2 = a.y + a.h;
    const float bx2 = b.x + b.w;
    const float by2 = b.y + b.h;

    if (ax2 <= b.x) return false;
    if (bx2 <= a.x) return false;
    if (ay2 <= b.y) return false;
    if (by2 <= a.y) return false;
    return true;
}   

void RenderManager::DrawDebugLine(float x1, float y1, float x2, float y2, Color color) noexcept
{
    mDebugCmds.push_back(DebugCmd{ DebugCmd::Type::Line, {}, x1, y1, x2, y2, color, false });
}

void RenderManager::DrawDebugRect(const Rect& worldRect, const Color color, bool filled) noexcept
{
    DebugCmd cmd;
    cmd.type = DebugCmd::Type::RectI;
    cmd.rect = worldRect;
    cmd.color = color;
    cmd.filled = filled;
    mDebugCmds.push_back(cmd);
}

void RenderManager::FlushDebug() noexcept
{
    // Recording retains world-space debug commands in the snapshot. Immediate
    // mode defers them to BeginScreenPass_, after the world texture is scaled.
    if (!mRenderer) return;
    if (!mActiveCam && !snapRecord) return;
    const auto start = std::chrono::steady_clock::now();
    const DrawPhase saved = mDrawPhase;
    mDrawPhase = DrawPhase::Debug;
    nDebugDrawCallsThisFrame = 0;

    if (!snapRecord) {
        mDeferredDebugCmds.insert(mDeferredDebugCmds.end(),
                                  mDebugCmds.begin(), mDebugCmds.end());
        nDebugDrawCallsThisFrame = static_cast<int>(mDebugCmds.size());
        mDebugCmds.clear();
        mDrawPhase = saved;
        mDebugMs = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        return;
    }

    for (const auto& c : mDebugCmds) {
        switch (c.type) {
        case DebugCmd::Type::Line:
            DrawLine(c.x1, c.y1, c.x2, c.y2, c.color);
            break;
        case DebugCmd::Type::RectI:
            DrawRect(c.rect, c.color, c.filled);
            break;
        }
        nDebugDrawCallsThisFrame++;
    }
    mDebugCmds.clear();
    mDrawPhase = saved;
    mDebugMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

void RenderManager::SetBackground(Texture* tex, BackgroundMode mode, Color tint) noexcept
{
    mBackgroundTexture = tex;
    mBgMode = (tex ? mode : BackgroundMode::None);
    mBackgroundTint = tint;
}

void RenderManager::ClearBackground() noexcept
{
    mBackgroundTexture = nullptr;
    mBgMode = BackgroundMode::None;
    mBackgroundTint = { 255,255,255,255 };
}

void RenderManager::ClearParallax() noexcept
{
    mBgLayers.clear();
    mHasPrevCam = false;
}

void RenderManager::AddParallaxLayer(const BackgroundLayer& layer) noexcept
{
    if (!layer.tex) return;
    mBgLayers.push_back(layer);
}

Rect RenderManager::ComputeFitRect_(float winW, float winH, float texW, float texH) noexcept
{
    if (texW <= 0.f || texH <= 0.f || winW <= 0.f || winH <= 0.f) return { 0,0,0,0 };

    const float sx = winW / texW;
    const float sy = winH / texH;
    const float s = (sx < sy) ? sx : sy;

    const float w = texW * s;
    const float h = texH * s;
    return Rect{ (winW - w) * 0.5f, (winH - h) * 0.5f, w, h };
}

Rect RenderManager::ComputeFillRect_(float winW, float winH, float texW, float texH) noexcept
{
    if (texW <= 0.f || texH <= 0.f || winW <= 0.f || winH <= 0.f) return { 0,0,0,0 };

    const float sx = winW / texW;
    const float sy = winH / texH;
    const float s = (sx > sy) ? sx : sy;

    const float w = texW * s;
    const float h = texH * s;
    return Rect{ (winW - w) * 0.5f, (winH - h) * 0.5f, w, h };
}

inline float RenderManager::WrapOffset_(float x, float period) noexcept
{
    if (period <= 0.f) return 0.f;
    // wrap a [0, period)
    float r = std::fmod(x, period);
    if (r < 0.f) r += period;
    return r;
}

// ADAPT TO THE ACTIVE CAMERA
Vec2 RenderManager::GetCamPos_(const Camera2D& cam) const noexcept
{
    return cam.GetCenter();
}

void RenderManager::DrawBackground_(const Camera2D& cam) noexcept
{
    if (!mRenderer) return;
    if (mWinW <= 0 || mWinH <= 0) return;

    const bool useLayers = !mBgLayers.empty();
    const Vec2 camPos = GetCamPos_(cam);

    // ------------------------------------------------------------
    // 1) Physical tilemap in WORLD space (stationary as the player moves)
    // ------------------------------------------------------------
    auto drawPhysicalWorldTiles = [&](Texture* tex,
        Vec2 worldOrigin,
        Vec2 scale,
        Color tint,
        bool flipX, bool flipY,
        bool seamSafe,
        bool useBounds,
        Rect boundsWorld) noexcept
        {
            if (!tex || !tex->GetSDL() || !mActiveCam) return;

            float ppu = tex->pixelsPerUnit;
            if (ppu <= 0.f) ppu = 1.f;

            // Tile size in WORLD units
            const float tileW = (tex->Width() / ppu) * scale.x;
            const float tileH = (tex->Height() / ppu) * scale.y;
            if (tileW <= 0.f || tileH <= 0.f) return;

            // Approximate world-space viewport (if ScreenToWorld exists, this is an approximation)
            Vec2 w0 = cam.ScreenToWorld(0.f, 0.f, mWinW, mWinH);
            Vec2 w1 = cam.ScreenToWorld((float)mWinW, (float)mWinH, mWinW, mWinH);

            // AABB en mundo
            float minx = std::min(w0.x, w1.x);
            float miny = std::min(w0.y, w1.y);
            float maxx = std::max(w0.x, w1.x);
            float maxy = std::max(w0.y, w1.y);

            Rect viewWorld{ minx, miny, maxx - minx, maxy - miny };


            Rect area = viewWorld;
            if (useBounds && boundsWorld.w > 0.f && boundsWorld.h > 0.f) {
                float ax1 = std::max(area.x, boundsWorld.x);
                float ay1 = std::max(area.y, boundsWorld.y);
                float ax2 = std::min(area.x + area.w, boundsWorld.x + boundsWorld.w);
                float ay2 = std::min(area.y + area.h, boundsWorld.y + boundsWorld.h);
                if (ax2 <= ax1 || ay2 <= ay1) return;
                area = Rect{ ax1, ay1, ax2 - ax1, ay2 - ay1 };
            }

            // SeamSafe: add one tile around the visible region
            if (seamSafe) {
                area.x -= tileW; area.y -= tileH;
                area.w += tileW * 2.f; area.h += tileH * 2.f;
            }

            // indices to draw
            const int x0 = (int)std::floor((area.x - worldOrigin.x) / tileW);
            const int y0 = (int)std::floor((area.y - worldOrigin.y) / tileH);
            const int x1 = (int)std::ceil(((area.x + area.w) - worldOrigin.x) / tileW);
            const int y1 = (int)std::ceil(((area.y + area.h) - worldOrigin.y) / tileH);

            for (int ty = y0; ty < y1; ++ty)
            {
                // Mirrored repeat on Y when flipY is enabled
                const bool fy = flipY ? ((ty & 1) != 0) : false;

                for (int tx = x0; tx < x1; ++tx)
                {
                    const bool fx = flipX ? ((tx & 1) != 0) : false;

                    Rect dstWorld{
                        worldOrigin.x + tx * tileW,
                        worldOrigin.y + ty * tileH,
                        tileW,
                        tileH
                    };

                    DrawTextureImmediate_(*tex, dstWorld, nullptr, 0.f, { 0.5f,0.5f }, tint, fx, fy);
                }
            }
        };

    // ------------------------------------------------------------
    // 2) Wrap helper
    // ------------------------------------------------------------
    auto wrapOffset = [&](float x, float period) noexcept -> float
        {
            if (period <= 0.f) return 0.f;
            float r = std::fmod(x, period);
            if (r < 0.f) r += period;
            return r;
        };

    // ------------------------------------------------------------
    // 3) Tiling in SCREEN pixels with mirrored-repeat flipping
    // ------------------------------------------------------------
    auto drawTiledScreen = [&](Texture* tex,
        float tileW, float tileH,
        Vec2 offsetPx,
        Color tint,
        bool flipX, bool flipY,
        bool seamSafe) noexcept
        {
            if (!tex || !tex->GetSDL()) return;
            if (tileW <= 0.f || tileH <= 0.f) return;

            const float winW = (float)mWinW;
            const float winH = (float)mWinH;

            // stable integer tile index (for flip parity)
            const int baseIx = (int)std::floor(offsetPx.x / tileW);
            const int baseIy = (int)std::floor(offsetPx.y / tileH);

            const float fracX = offsetPx.x - (float)baseIx * tileW; // [0..tileW) for a normalized offset
            const float fracY = offsetPx.y - (float)baseIy * tileH;

            const int extraX = seamSafe ? 1 : 0;
            const int extraY = seamSafe ? 1 : 0;

            const float startX = -fracX - (float)extraX * tileW;
            const float startY = -fracY - (float)extraY * tileH;

            const int nx = (int)std::ceil((winW - startX) / tileW) + 1;
            const int ny = (int)std::ceil((winH - startY) / tileH) + 1;

            for (int y = 0; y < ny; ++y)
            {
                const int tileIy = baseIy + (y - extraY);
                const bool fy = flipY ? ((tileIy & 1) != 0) : false;

                for (int x = 0; x < nx; ++x)
                {
                    const int tileIx = baseIx + (x - extraX);
                    const bool fx = flipX ? ((tileIx & 1) != 0) : false;

                    Rect r{ startX + x * tileW, startY + y * tileH, tileW, tileH };
                    DrawImageScreenImmediate_(*tex, r, nullptr, tint, fx, fy);
                }
            }
        };

    // ------------------------------------------------------------
    // 4) Draw in simple mode (no physical tiling)
    // ------------------------------------------------------------
    auto drawOne = [&](Texture* tex,
        BackgroundMode mode,
        Color tint,
        Vec2 offsetPx,
        Vec2 scale,
        bool flipX,
        bool flipY,
        bool seamSafe) noexcept
        {
            if (!tex || !tex->GetSDL()) return;

            const float winW = (float)mWinW;
            const float winH = (float)mWinH;

            const float tw = (float)tex->Width();
            const float th = (float)tex->Height();
            if (tw <= 0.f || th <= 0.f) return;

            Rect dst{ 0.f, 0.f, winW, winH };

            switch (mode)
            {
            case BackgroundMode::None:
                return;

            case BackgroundMode::Stretch:
                dst = Rect{ offsetPx.x, offsetPx.y, winW, winH };
                DrawImageScreenImmediate_(*tex, dst, nullptr, tint, flipX, flipY);
                return;

            case BackgroundMode::Fit:
                dst = ComputeFitRect_(winW, winH, tw, th);
                dst.x += offsetPx.x; dst.y += offsetPx.y;
                DrawImageScreenImmediate_(*tex, dst, nullptr, tint, flipX, flipY);
                return;

            case BackgroundMode::Fill:
                dst = ComputeFillRect_(winW, winH, tw, th);
                dst.x += offsetPx.x; dst.y += offsetPx.y;
                DrawImageScreenImmediate_(*tex, dst, nullptr, tint, flipX, flipY);
                return;

            case BackgroundMode::Center:
                dst = Rect{ (winW - tw) * 0.5f + offsetPx.x, (winH - th) * 0.5f + offsetPx.y, tw, th };
                DrawImageScreenImmediate_(*tex, dst, nullptr, tint, flipX, flipY);
                return;

            case BackgroundMode::Repeat:
            case BackgroundMode::RepeatScaled:
            {
                float tileW = tw;
                float tileH = th;

                if (mode == BackgroundMode::RepeatScaled) {
                    tileW = tw * scale.x;
                    tileH = th * scale.y;
                    if (tileW <= 0.f || tileH <= 0.f) return;
                }

                // Use tiled screen with mirrored repeat
                drawTiledScreen(tex, tileW, tileH, offsetPx, tint, flipX, flipY, seamSafe);
                return;
            }

            default:
                return;
            }
        };

    // ------------------------------------------------------------
    // Simple case (without layers)
    // ------------------------------------------------------------
    if (!useLayers)
    {
        if (mBackgroundTexture && mBgMode != BackgroundMode::None)
            drawOne(mBackgroundTexture, mBgMode, mBackgroundTint,
                Vec2{ 0.f, 0.f }, Vec2{ 1.f, 1.f }, false, false, true);
        return;
    }

    // ------------------------------------------------------------
    // Layer order
    // ------------------------------------------------------------
    std::vector<BackgroundLayer> layers = mBgLayers;
    std::sort(layers.begin(), layers.end(),
        [](const BackgroundLayer& a, const BackgroundLayer& b) { return a.order < b.order; });

    // ------------------------------------------------------------
    // Dibujo capas
    // ------------------------------------------------------------
    for (const auto& L : layers)
    {
        if (!L.tex || !L.tex->GetSDL()) continue;

        float ppu = L.tex->pixelsPerUnit;
        if (ppu <= 0.f) ppu = 1.f;

        // Parallax en WORLD units -> px (screen offset)
        const float parX = -camPos.x * L.parallax * ppu;
        const float parY = -camPos.y * (L.parallaxY ? L.parallax : 0.f) * ppu;

        // --------------------------------------------------------
        // ACTUAL PHYSICAL TILING (WORLD SPACE)
        // --------------------------------------------------------
        if (L.mode == BackgroundMode::TileWorldPhysical || L.mode == BackgroundMode::TileWorldPhysicalScaled)
        {
            Vec2 scl = (L.mode == BackgroundMode::TileWorldPhysicalScaled) ? L.scale : Vec2{ 1.f, 1.f };

            // Offset en mundo usado como origen
            Vec2 worldOrigin = L.offsetPx;

            // Bounds: disabled by default
            const bool useBounds = false;
            Rect boundsWorld{ 0,0,0,0 };

            drawPhysicalWorldTiles(L.tex, worldOrigin, scl, L.tint, L.flipX, L.flipY, L.seamSafe, useBounds, boundsWorld);
            continue;
        }

        // --------------------------------------------------------
        // RESTO MODOS (screen-space): offsetPx en px
        // --------------------------------------------------------
        Vec2 offsetPx{ 0.f, 0.f };

        offsetPx = Vec2{ parX + L.offsetPx.x, parY + L.offsetPx.y };

        drawOne(L.tex, L.mode, L.tint, offsetPx, L.scale, L.flipX, L.flipY, L.seamSafe);
    }
}


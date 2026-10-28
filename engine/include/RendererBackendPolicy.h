#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

// Renderer backend policy has no SDL dependencies, so its order can be
// regression-tested even where the platform SDK is unavailable.
namespace RendererBackendPolicy
{
    struct Plan
    {
        std::vector<std::string> names;
        bool allowSdlDefault = false;
    };

    inline std::string Normalize(std::string name)
    {
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
        return name;
    }

    inline Plan MakePlan(std::string requested, bool windows,
                        bool fallbackEnabled, bool strictHint,
                        const std::vector<std::string>& measuredPriority = {})
    {
        Plan plan;
        requested = Normalize(requested);
        const bool automatic = requested.empty() || requested == "auto";
        if (strictHint)
        {
            if (automatic) plan.allowSdlDefault = true;
            else plan.names.push_back(requested);
            return plan;
        }

        if (!automatic) plan.names.push_back(requested);
        if (automatic || fallbackEnabled)
        {
            for (const auto& raw : measuredPriority)
            {
                const auto name = Normalize(raw);
                if (name.empty() || name == "auto" ||
                    std::find(plan.names.begin(), plan.names.end(), name) != plan.names.end())
                    continue;
                plan.names.push_back(name);
            }
        }
        if (windows && (automatic || fallbackEnabled))
        {
            // Compatibility-oriented fallback when a benchmark preference is
            // unavailable, incomplete or an installed driver fails.
            for (const char* name : {"direct3d11", "direct3d", "direct3d12",
                                     "opengles2", "opengl", "software"})
            {
                if (std::find(plan.names.begin(), plan.names.end(), name) ==
                    plan.names.end()) plan.names.emplace_back(name);
            }
        }
        // A final SDL-managed attempt handles unfamiliar driver names and
        // preserves the former automatic choice on non-Windows platforms.
        plan.allowSdlDefault = automatic || fallbackEnabled;
        return plan;
    }
}

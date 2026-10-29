#pragma once
/** @file RendererBackendPolicy.h
 * @brief SDL-independent ordering rules for trying graphics renderer backends.
 */

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

/** @brief Builds the ordered backend candidate list before renderer creation.
 * @details This policy only chooses names. RenderManager actually tries each
 * candidate, so listing a backend never guarantees that SDL can initialize it.
 */
namespace RendererBackendPolicy
{
    /** @brief Ordered driver names and permission to let SDL choose its default.
     */
    struct Plan
    {
        std::vector<std::string> names; ///< Driver names to try, in order and without duplicates.
        bool allowSdlDefault = false;   ///< Whether a final SDL-managed attempt is allowed.
    };

    /** @brief Lowercases an SDL driver name for case-insensitive matching.
     */
    inline std::string Normalize(std::string name)
    {
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
        return name;
    }

    /** @brief Constructs the preferred renderer-creation order.
     * @param requested Explicit SDL backend name, `auto`, or empty for automatic selection.
     * @param windows Whether the **target** platform is Windows.
     * @param fallbackEnabled Append alternative drivers if the requested one fails.
     * @param strictHint Restrict selection to the requested backend, or SDL's
     *                   default when the request is automatic, ignoring fallbacks.
     * @param measuredPriority Optional benchmark-derived priority list, tried
     *                         before built-in Windows compatibility fallbacks.
     * @return Candidate sequence with duplicates removed and SDL fallback flag.
     * @note Windows built-in priority is D3D11, D3D, D3D12, GLES2, GL,
     *       then software. Other platforms use the measured order followed by
     *       SDL's default; an explicit backend is always tried first.
     */
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

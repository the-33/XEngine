#pragma once

// Debug gizmos are attached to world coordinates, but rasterized over the
// physical output after the low-resolution world texture has been presented.
// Keep this mapping independent of SDL for deterministic regression tests.
/** @brief Namespace collecting x engine debug overlay functionality for XEngine.
 */
namespace XEngineDebugOverlay {
/** @brief Point data and utilities used by XEngineDebugOverlay.
 */
struct Point {
    /** @brief Configured x value used by Point.
     */
    float x = 0.f;
    /** @brief Configured y value used by Point.
     */
    float y = 0.f;
};

/** @brief Project to output in XEngineDebugOverlay.
 * @param virtualPixel Virtual pixel supplied to XEngineDebugOverlay.
 * @param renderWidth Render width supplied to XEngineDebugOverlay.
 * @param renderHeight Render height supplied to XEngineDebugOverlay.
 * @param outputX Output x supplied to XEngineDebugOverlay.
 * @param outputY Output y supplied to XEngineDebugOverlay.
 * @param outputWidth Output width supplied to XEngineDebugOverlay.
 * @param outputHeight Output height supplied to XEngineDebugOverlay.
 * @return Result of project to output in XEngineDebugOverlay.
 */
inline Point ProjectToOutput(Point virtualPixel, int renderWidth, int renderHeight,
                             int outputX, int outputY,
                             int outputWidth, int outputHeight) noexcept
{
    if (renderWidth <= 0 || renderHeight <= 0 ||
        outputWidth <= 0 || outputHeight <= 0) return virtualPixel;
    return {static_cast<float>(outputX) + virtualPixel.x *
                (static_cast<float>(outputWidth) / renderWidth),
            static_cast<float>(outputY) + virtualPixel.y *
                (static_cast<float>(outputHeight) / renderHeight)};
}
} // namespace XEngineDebugOverlay

#pragma once
/** @file WindowManager.h
 * @brief SDL window size, framebuffer dimensions and fullscreen state.
 */

#include <cstdint>

#include "Singleton.h"
#include "BaseTypes.h"
#include "RenderManager.h"
#include "InputManager.h"
#include "Engine.h"
#include "Property.h"

// Forward declarations
struct SDL_Window;
union  SDL_Event;

/** @brief Controls title, logical window resolution, framebuffer and desktop fullscreen.
 * @ingroup xengine_render
 */
class WindowManager : public Singleton<WindowManager>
{
    friend class Singleton<WindowManager>;
	friend class RenderManager;
    friend class Engine;
    friend class InputManager;

private:
    struct Config
    {
        int width = 1280;
        int height = 720;
        bool fullscreen = false;
        const char* title = "Game";
    };

    bool Init(const Config& conf) noexcept;
    void Shutdown() noexcept;

    void Present(SDL_Renderer* r) noexcept;
    bool ShouldClose() const noexcept;

    SDL_Window* SDL() const noexcept;                 // access to SDL_Window*
    void ProcessEvent(const SDL_Event& e) noexcept;   // marca wantClose, cachea size/fullscreen

    WindowManager() = default;
    ~WindowManager() = default;

    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;
    WindowManager(WindowManager&&) = delete;
    WindowManager& operator=(WindowManager&&) = delete;

    SDL_Window* windowHandle = nullptr;
    bool wantClose = false;
    int width = 0, height = 0;
    int windowedWidth = 0, windowedHeight = 0;
    bool fullscreen = false;
    std::uint64_t sizeRevision_ = 0;

    void SetSizeProperty(Vec2I newSize) noexcept { (void)SetSize(newSize.x, newSize.y); }
    void SetFullscreenProperty(bool value) noexcept { SetFullscreen(value); }
    Vec2I GetWindowedSize() const noexcept { return { windowedWidth, windowedHeight }; }
    std::uint64_t GetSizeRevision() const noexcept { return sizeRevision_; }

public:
    /** @brief Returns the actual logical size of the SDL client area, not HiDPI framebuffer pixels.
     */
    Vec2I GetSize() const noexcept;         // logical coordinates (points)
    /** Set the windowed resolution (1..16384 each axis). In fullscreen, queue it for restoration. */
    bool SetSize(int newWidth, int newHeight) noexcept;
    /** @brief Returns drawable pixel dimensions; may differ from logical window size on HiDPI displays.
     */
    Vec2I GetDrawableSize() const noexcept; // framebuffer (pixels)
    /** @brief Changes the title of an initialized SDL window.
     * @param title Text used as the window title.
     */
    void SetTitle(const char* title) noexcept;

    /** @brief Enables or disables desktop fullscreen while preserving the requested windowed resolution.
     * @param enabled Whether to enable the named feature.
     */
    void SetFullscreen(bool enabled) noexcept; // change at runtime if the backend supports it
    /** @brief Reports the cached fullscreen state after the last successful transition.
     */
    bool IsFullscreen() const noexcept;        // return cached state

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `size`; use the corresponding instance member in gameplay code.
     */
    using SizeProp = Property<WindowManager, Vec2I, &WindowManager::GetSize,
                              &WindowManager::SetSizeProperty>;
    /** @endcond */
    /** @brief Read/write logical window size; setter validates dimensions and remembers them during fullscreen.
     */
    SizeProp size{ this };
    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `drawable size`; use the corresponding instance member in gameplay code.
     */
    using DrawableSizeProp = PropertyRO<WindowManager, Vec2I, &WindowManager::GetDrawableSize>;
    /** @endcond */
    /** @brief Read-only framebuffer pixel size, which may differ from logical dimensions on HiDPI screens.
     */
    DrawableSizeProp drawableSize{ this };
    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `fullscreen`; use the corresponding instance member in gameplay code.
     */
    using FullscreenProp = Property<WindowManager, bool, &WindowManager::IsFullscreen,
                                    &WindowManager::SetFullscreenProperty>;
    /** @endcond */
    /** @brief Read/write desktop fullscreen state.
     */
    FullscreenProp isFullscreen{ this };
    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `windowed size`; use the corresponding instance member in gameplay code.
     */
    using WindowedSizeProp = PropertyRO<WindowManager, Vec2I, &WindowManager::GetWindowedSize>;
    /** @endcond */
    /** @brief Read-only window dimensions to restore when leaving fullscreen.
     */
    WindowedSizeProp windowedSize{ this };
    /** @cond XENGINE_INTERNAL */
    /** Increments when logical window size, requested windowed size or fullscreen changes. */
    using ResizeRevisionProp = PropertyRO<WindowManager, std::uint64_t,
                                          &WindowManager::GetSizeRevision>;
    /** @endcond */
    /** @brief Monotonic change counter used to detect window size and fullscreen transitions.
     */
    ResizeRevisionProp resizeRevision{ this };
};

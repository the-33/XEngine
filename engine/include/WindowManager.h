/** @brief Grant Singleton< WindowManager > access to the internal state of WindowManager.
 */
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

/** @brief Controls title, window resolution, framebuffer and fullscreen modes.
 * @ingroup xengine_render
 */
class WindowManager : public Singleton<WindowManager>
{
    /** @brief Permit the singleton base to construct and release the WindowManager manager.
     */
    friend class Singleton<WindowManager>;
	/** @brief Grant RenderManager access to the internal state of WindowManager.
	 */
	friend class RenderManager;
    /** @brief Grant Engine access to the internal state of WindowManager.
     */
    friend class Engine;
    /** @brief Grant InputManager access to the internal state of WindowManager.
     */
    friend class InputManager;

private:
    /** @brief Internal configuration values for WindowManager.
     */
    struct Config
    {
        /** @brief Configured width value used by Config.
         */
        int width = 1280;
        /** @brief Configured height value used by Config.
         */
        int height = 720;
        /** @brief Configured fullscreen value used by Config.
         */
        bool fullscreen = false;
        /** @brief Configured title value used by Config.
         */
        const char* title = "Game";
    };

    /** @brief Initialize the WindowManager subsystem.
     * @param conf Conf supplied to WindowManager.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Init(const Config& conf) noexcept;
    /** @brief Release resources managed by WindowManager.
     */
    void Shutdown() noexcept;

    /** @brief Present in WindowManager.
     * @param r Target rectangle in the relevant coordinate space.
     */
    void Present(SDL_Renderer* r) noexcept;
    /** @brief Should close in WindowManager.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool ShouldClose() const noexcept;

    /** @brief Sdl in WindowManager.
     * @return Result of sdl in WindowManager.
     */
    SDL_Window* SDL() const noexcept;                 // access to SDL_Window*
    /** @brief Process event in WindowManager.
     * @param e E supplied to WindowManager.
     */
    void ProcessEvent(const SDL_Event& e) noexcept;   // marca wantClose, cachea size/fullscreen

    /** @brief Construct a WindowManager instance.
     */
    WindowManager() = default;
    /** @brief Destroy a WindowManager instance and release owned resources.
     */
    ~WindowManager() = default;

    /** @brief Construct a WindowManager instance.
     * @param other Source object whose copy, move or equality is being requested.
     */
    WindowManager(const WindowManager& other) = delete;
    /** @brief Implement the operator= operation for WindowManager.
     * @return Result of applying the overloaded operator.
     * @param other Source object whose copy, move or equality is being requested.
     */
    WindowManager& operator=(const WindowManager& other) = delete;
    /** @brief Construct a WindowManager instance.
     * @param other Source object whose copy, move or equality is being requested.
     */
    WindowManager(WindowManager&& other) = delete;
    /** @brief Implement the operator= operation for WindowManager.
     * @return Result of applying the overloaded operator.
     * @param other Source object whose copy, move or equality is being requested.
     */
    WindowManager& operator=(WindowManager&& other) = delete;

    /** @brief Configured window handle value used by WindowManager.
     */
    SDL_Window* windowHandle = nullptr;
    /** @brief Configured want close value used by WindowManager.
     */
    bool wantClose = false;
    /** @brief Configured width value used by WindowManager.
     */
    int width = 0, height = 0;
    /** @brief Configured windowed width value used by WindowManager.
     */
    int windowedWidth = 0, windowedHeight = 0;
    /** @brief Configured fullscreen value used by WindowManager.
     */
    bool fullscreen = false;
    bool exclusiveFullscreen = false; ///< True if the last active fullscreen mode changed the display resolution.
    /** @brief Stored size revision value used by WindowManager.
     */
    std::uint64_t sizeRevision_ = 0;

    /** @brief Set size property in WindowManager.
     * @param newSize New size used to bound or select the operation.
     */
    void SetSizeProperty(Vec2I newSize) noexcept { (void)SetSize(newSize.x, newSize.y); }
    /** @brief Set fullscreen property in WindowManager.
     * @param value Value to store, serialize, compare or transform.
     */
    void SetFullscreenProperty(bool value) noexcept { SetFullscreen(value); }
    /** @brief Retrieve the size revision from WindowManager.
     * @return Requested get size revision value or reference.
     */
    std::uint64_t GetSizeRevision() const noexcept { return sizeRevision_; }

public:
    /** @brief Returns the actual logical size of the SDL client area, not HiDPI framebuffer pixels.
     * @return The actual logical size of the sdl client area, not hidpi framebuffer pixels.
     */
    Vec2I GetSize() const noexcept;         // logical coordinates (points)
    /** @brief Returns the saved windowed client size even while fullscreen.
     * Useful for restoring a failed fullscreen/windowed resize transaction.
     * @return The saved windowed client size even while fullscreen.
     */
    Vec2I GetWindowedSize() const noexcept { return { windowedWidth, windowedHeight }; }
    /** @brief Sets the windowed client size (1..16384 on each axis).
     * @param newWidth Requested logical window width.
     * @param newHeight Requested logical window height.
     * @return False for invalid dimensions or absent window; when windowed,
     *         false also indicates that SDL did not produce the exact size.
     * @note In desktop fullscreen the requested dimensions are saved for
     *       restoration; the fullscreen drawable size does not change.
     */
    bool SetSize(int newWidth, int newHeight) noexcept;
    /** @brief Returns drawable pixel dimensions; may differ from logical window size on HiDPI displays.
     * @return Drawable pixel dimensions; may differ from logical window size on hidpi displays.
     */
    Vec2I GetDrawableSize() const noexcept; // framebuffer (pixels)
    /** @brief Returns the desktop video-mode resolution of this window's monitor.
     * Unlike GetDrawableSize(), this does not change when the game window is
     * resized. Useful for choosing an aspect ratio for internal rendering.
     * @return Desktop width/height in pixels; (0, 0) if unavailable.
     */
    Vec2I GetDesktopResolution() const noexcept;
    /** @brief Sets the window caption when the window and title pointer are valid.
     * @param title Text used as the window title.
     */
    void SetTitle(const char* title) noexcept;

    /** @brief Requests SDL desktop fullscreen without changing stored windowed size.
     * @param enabled Whether to enable the named feature.
     * @note If SDL rejects the transition, the cached fullscreen state is unchanged.
     */
    void SetFullscreen(bool enabled) noexcept; // change at runtime if the backend supports it
    /** @brief Enters exclusive fullscreen at an actual display video mode.
     * Unlike SetFullscreen(true), which keeps the desktop display resolution,
     * this method requests a physical display mode change. Unsupported modes
     * are rejected rather than silently substituting a different resolution.
     * The windowed dimensions are preserved for when fullscreen is exited.
     * @param newWidth Requested fullscreen video-mode width in pixels.
     * @param newHeight Requested fullscreen video-mode height in pixels.
     * @return True if SDL accepts the exact mode and fullscreen transition;
     *         false if invalid, unsupported or rejected by the display driver.
     * @note Available modes depend on the monitor, operating system and
     *       SDL video backend. Call on the main engine thread.
     */
    bool SetExclusiveFullscreen(int newWidth, int newHeight) noexcept;
    /** @brief Whether the last successful fullscreen request was exclusive.
     * @return True only while exclusive fullscreen is active.
     */
    bool IsExclusiveFullscreen() const noexcept { return fullscreen && exclusiveFullscreen; }
    /** @brief Reports the cached fullscreen state after the last successful transition.
     * @return True when is fullscreen; otherwise false.
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

/** @name Co-declared engine member descriptions
 * @brief Members sharing declarations with other fields.
 * @{ */
/** @var int WindowManager::height
 * @brief Camera height in the projection coordinate space.
 */

/** @var int WindowManager::windowedHeight
 * @brief Windowed height used by WindowManager.
 */
/** @} */

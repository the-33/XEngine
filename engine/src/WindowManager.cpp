#include "WindowManager.h"

#include "SDL.h"
#include "ErrorHandler.h"

#if defined(_WIN32)
#include "SDL_syswm.h"
#endif

/** @brief As sdl in WindowManager.cpp.
 * @param p P supplied to WindowManager.cpp.
 * @return Result of as sdl in WindowManager.cpp.
 */
static inline SDL_Window* AsSDL(void* p) noexcept {
    return static_cast<SDL_Window*>(p);
}

bool WindowManager::Init(const Config& conf) noexcept
{
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            LogError("Engine - Window", "Init(): SDL_InitSubSystem(VIDEO) failed.", SDL_GetError());
            return false;
        }
    }

    // Creation flags
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;

    width = conf.width > 0 && conf.width <= 16384 ? conf.width : 1280;
    height = conf.height > 0 && conf.height <= 16384 ? conf.height : 720;
    windowedWidth = width;
    windowedHeight = height;
    fullscreen = false;
    exclusiveFullscreen = false;
    sizeRevision_ = 0;

    SDL_Window* win = SDL_CreateWindow(
        conf.title ? conf.title : "Game",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        flags
    );

    if (!win) {
        LogError("Engine - Window", "Init(): SDL_CreateWindow failed.", SDL_GetError());
        return false;
    }

    windowHandle = win;

    SDL_SetWindowResizable(win, SDL_FALSE);
    // Runtime SetSize is allowed even though user drag-resizing stays disabled.
    // Previous min/max constraints locked this window to its creation resolution.
    SetFullscreen(conf.fullscreen);

    wantClose = false;
    return true;
}

void WindowManager::Shutdown() noexcept
{
    if (windowHandle) {
        SDL_DestroyWindow(AsSDL(windowHandle));
        windowHandle = nullptr;
    }
}

void WindowManager::Present(SDL_Renderer* r) noexcept
{
    if (r) SDL_RenderPresent(r);
}

bool WindowManager::ShouldClose() const noexcept
{
    return wantClose;
}

Vec2I WindowManager::GetSize() const noexcept
{
    if (windowHandle) {
        int w = 0, h = 0;
        SDL_GetWindowSize(AsSDL(windowHandle), &w, &h);
        return { w, h };
    }
    else {
        return { width, height };
    }
}

Vec2I WindowManager::GetDrawableSize() const noexcept 
{
    if (windowHandle) {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(AsSDL(windowHandle), &w, &h);
        return { w, h };
    }
    else return Vector2::Zero;
}

Vec2I WindowManager::GetDesktopResolution() const noexcept
{
    if (!windowHandle) return {0, 0};

#if defined(_WIN32)
    // EnumDisplaySettings devuelve píxeles físicos, sin virtualización por DPI.
    // Se consulta el monitor de la ventana para respetar configuraciones multimonitor.
    SDL_SysWMinfo wmInfo{};
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(AsSDL(windowHandle), &wmInfo) == SDL_TRUE &&
        wmInfo.subsystem == SDL_SYSWM_WINDOWS) {
        const HMONITOR monitor = MonitorFromWindow(
            wmInfo.info.win.window, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEXW monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        DEVMODEW displayMode{};
        displayMode.dmSize = sizeof(displayMode);
        if (monitor && GetMonitorInfoW(monitor, &monitorInfo) &&
            EnumDisplaySettingsW(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &displayMode) &&
            displayMode.dmPelsWidth > 0 && displayMode.dmPelsHeight > 0) {
            return {static_cast<int>(displayMode.dmPelsWidth),
                    static_cast<int>(displayMode.dmPelsHeight)};
        }
    }
#endif

    const int displayIndex = SDL_GetWindowDisplayIndex(AsSDL(windowHandle));
    if (displayIndex < 0) return {0, 0};
    SDL_DisplayMode mode{};
    if (SDL_GetDesktopDisplayMode(displayIndex, &mode) != 0) return {0, 0};
    return {mode.w, mode.h};
}

void WindowManager::SetTitle(const char* title) noexcept
{
    if (windowHandle && title) {
        SDL_SetWindowTitle(AsSDL(windowHandle), title);
    }
}

bool WindowManager::SetSize(int newWidth, int newHeight) noexcept
{
    if (!windowHandle || newWidth <= 0 || newHeight <= 0 ||
        newWidth > 16384 || newHeight > 16384) return false;

    if (fullscreen) {
        const bool changed = windowedWidth != newWidth || windowedHeight != newHeight;
        windowedWidth = newWidth;
        windowedHeight = newHeight;
        if (changed) ++sizeRevision_;
        return true;
    }

    SDL_SetWindowSize(AsSDL(windowHandle), newWidth, newHeight);
    int actualWidth = 0, actualHeight = 0;
    SDL_GetWindowSize(AsSDL(windowHandle), &actualWidth, &actualHeight);
    if (actualWidth != width || actualHeight != height) ++sizeRevision_;
    width = actualWidth;
    height = actualHeight;
    windowedWidth = actualWidth;
    windowedHeight = actualHeight;
    return actualWidth == newWidth && actualHeight == newHeight;
}

void WindowManager::SetFullscreen(bool enabled) noexcept
{
    // An explicit desktop-fullscreen request replaces exclusive fullscreen;
    // do not silently retain an old exclusive video mode.
    if (fullscreen == enabled && (!enabled || !exclusiveFullscreen)) return;
    if (!windowHandle) return;

    Uint32 mode = enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;

    if (SDL_SetWindowFullscreen(AsSDL(windowHandle), mode) != 0) {
        LogError("Engine - Window", "SetFullscreen(): SDL_SetWindowFullscreen failed.", SDL_GetError());
    }
    else {
        fullscreen = enabled;
        exclusiveFullscreen = false;
        ++sizeRevision_;
        if (!enabled) {
            // SDL may restore the pre-fullscreen size; apply the most recent requested size.
            (void)SetSize(windowedWidth, windowedHeight);
        }
    }
}

bool WindowManager::SetExclusiveFullscreen(int newWidth, int newHeight) noexcept
{
    if (!windowHandle || newWidth <= 0 || newHeight <= 0 ||
        newWidth > 16384 || newHeight > 16384) return false;

    SDL_Window* win = AsSDL(windowHandle);
    const int display = SDL_GetWindowDisplayIndex(win);
    if (display < 0) return false;

    SDL_DisplayMode requested{};
    requested.w = newWidth;
    requested.h = newHeight;
    SDL_DisplayMode supported{};
    if (!SDL_GetClosestDisplayMode(display, &requested, &supported) ||
        supported.w != newWidth || supported.h != newHeight)
        return false; // Never pretend a different physical resolution was applied.

    SDL_DisplayMode previous{};
    const bool hadPrevious = SDL_GetWindowDisplayMode(win, &previous) == 0;
    if (SDL_SetWindowDisplayMode(win, &supported) != 0) {
        LogError("Engine - Window",
                 "SDL_SetWindowDisplayMode failed.", SDL_GetError());
        return false;
    }

    if (SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN) != 0) {
        LogError("Engine - Window",
                 "SDL_SetWindowFullscreen failed.", SDL_GetError());
        if (hadPrevious) (void)SDL_SetWindowDisplayMode(win, &previous);
        return false;
    }

    fullscreen = true;
    exclusiveFullscreen = true;
    ++sizeRevision_;
    return true;
}

bool WindowManager::IsFullscreen() const noexcept
{
    return fullscreen;
}

SDL_Window* WindowManager::SDL() const noexcept
{
    return AsSDL(windowHandle);
}

void WindowManager::ProcessEvent(const SDL_Event& e) noexcept
{
    switch (e.type) {
    case SDL_QUIT:
        wantClose = true;
        break;
    case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
            if (!windowHandle || e.window.windowID == SDL_GetWindowID(AsSDL(windowHandle)))
                wantClose = true;
        }
        else if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
            e.window.event == SDL_WINDOWEVENT_RESIZED) {
            if (windowHandle && e.window.windowID == SDL_GetWindowID(AsSDL(windowHandle))) {
                int actualWidth = 0, actualHeight = 0;
                SDL_GetWindowSize(AsSDL(windowHandle), &actualWidth, &actualHeight);
                if (actualWidth != width || actualHeight != height) ++sizeRevision_;
                width = actualWidth;
                height = actualHeight;
                if (!fullscreen) {
                    windowedWidth = actualWidth;
                    windowedHeight = actualHeight;
                }
            }
        }
        break;
    default: break;
    }
}

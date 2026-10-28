#include "WindowManager.h"

#include "SDL.h"
#include "ErrorHandler.h"

static inline SDL_Window* AsSDL(void* p) noexcept {
    return static_cast<SDL_Window*>(p);
}

bool WindowManager::Init(const Config& conf) noexcept
{
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            LogError("WindowManager::Init()", "SDL_InitSubSystem(VIDEO) failed.", SDL_GetError());
            return false;
        }
    }

    // Creation flags
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;

    width = conf.width > 0 && conf.width <= 16384 ? conf.width : 1280;
    height = conf.height > 0 && conf.height <= 16384 ? conf.height : 720;
    windowedWidth = width;
    windowedHeight = height;
    sizeRevision_ = 0;

    SDL_Window* win = SDL_CreateWindow(
        conf.title ? conf.title : "Game",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        flags
    );

    if (!win) {
        LogError("WindowManager::Init()", "SDL_CreateWindow failed.", SDL_GetError());
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
    if (fullscreen == enabled) return;
    if (!windowHandle) return;

    Uint32 mode = enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;

    if (SDL_SetWindowFullscreen(AsSDL(windowHandle), mode) != 0) {
        LogError("WindowManager::SetFullscreen()", "SDL_SetWindowFullscreen failed.", SDL_GetError());
    }
    else {
        fullscreen = enabled;
        ++sizeRevision_;
        if (!enabled) {
            // SDL may restore the pre-fullscreen size; apply the most recent requested size.
            (void)SetSize(windowedWidth, windowedHeight);
        }
    }
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

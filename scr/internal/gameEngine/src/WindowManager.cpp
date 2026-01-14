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

    // Flags de creación
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;

    width = conf.width > 0 ? conf.width : 1280;
    height = conf.height > 0 ? conf.height : 720;

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
    SDL_SetWindowMinimumSize(win, width, height);
    SDL_SetWindowMaximumSize(win, width, height);

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
                // reestablecer el tamaño original
                SDL_SetWindowSize(AsSDL(windowHandle), width, height);
                // y mantener el caché coherente
                int cw, ch; SDL_GetWindowSize(AsSDL(windowHandle), &cw, &ch);
                width = cw; height = ch;
            }
        }
        break;
    default: break;
    }
}

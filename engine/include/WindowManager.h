#pragma once

#include "Singleton.h"
#include "BaseTypes.h"
#include "RenderManager.h"
#include "InputManager.h"
#include "Engine.h"

// Forward declarations
struct SDL_Window;
union  SDL_Event;

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
    bool fullscreen = false;

public:
    Vec2I GetSize() const noexcept;         // logical coordinates (points)
    Vec2I GetDrawableSize() const noexcept; // framebuffer (pixels)
    void SetTitle(const char* title) noexcept;

    void SetFullscreen(bool enabled) noexcept; // change at runtime if the backend supports it
    bool IsFullscreen() const noexcept;        // return cached state
};
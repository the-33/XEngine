#pragma once

#include "Singleton.h"
#include "BaseTypes.h"
#include "SDL.h"
#include "RenderManager.h"
#include "InputManager.h"
#include "Engine.h"

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

    SDL_Window* SDL() const noexcept;                 // acceso al SDL_Window*
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
    Vec2I GetSize() const noexcept;         // lógica (points)
    Vec2I GetDrawableSize() const noexcept; // framebuffer (pixels)
    void SetTitle(const char* title) noexcept;

    void SetFullscreen(bool enabled) noexcept; // cambia a runtime si el backend lo permite
    bool IsFullscreen() const noexcept;        // devuelve estado cacheado
};
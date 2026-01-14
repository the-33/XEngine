#include "ErrorHandler.h"

#include "SDL.h"
#include "Engine.h"

ErrorLogResult LogError(const std::string& caption, 
    const std::string& error
#ifdef _WIN32
    , UINT msgBoxType
#endif
)
{
    if (!Engine::GetInstancePtr_NO_ERROR_MSG()->logErrors) return ErrorLogResult::OK;
#ifdef _WIN32
    int msgBoxResponse = 0;
    if ((msgBoxResponse = MessageBoxA(NULL, error.c_str(), caption.c_str(), msgBoxType)) != 0) return static_cast<ErrorLogResult>(msgBoxResponse);
#endif
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, caption.c_str(), error.c_str(), nullptr);
    }

    SDL_Log("%s: %s\n", caption.c_str(), error.c_str());

    return ErrorLogResult::Error;  
}

ErrorLogResult LogError(const std::string& caption, 
    const std::string& error,
    const char* sdlError
#ifdef _WIN32
    , UINT msgBoxType
#endif
)
{
    if (!Engine::GetInstancePtr_NO_ERROR_MSG()->logErrors) return ErrorLogResult::OK;
#ifdef _WIN32
    int msgBoxResponse = 0;
    if ((msgBoxResponse = MessageBoxA(NULL, (error + "\n" + sdlError).c_str(), caption.c_str(), msgBoxType)) != 0) return static_cast<ErrorLogResult>(msgBoxResponse);
#endif
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, caption.c_str(), (error + "\n" + sdlError).c_str(), nullptr);
    }

    SDL_Log("%s: %s\n", caption.c_str(), (error + "\n" + sdlError).c_str());

    return ErrorLogResult::Error;
}
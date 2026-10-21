#include <XEngine.h>
#include "GameBuild.generated.h"
#include "InitCallback.hpp"
#include <SDL.h>
#include <filesystem>
#include <cstdio>

int main(int, char**)
{
    // Invariant: ./ always means the assets root, in Debug and in shipping.
#if defined(XENGINE_DEBUG)
    std::filesystem::current_path(XEngineGameBuild::DevelopmentAssetsPath());
#else
    char* base = SDL_GetBasePath();
    if (!base) { std::fputs("Cannot determine application path\n", stderr); return 2; }
    std::filesystem::current_path(base);
    SDL_free(base);
#endif

#if defined(XENGINE_RELEASE)
    if (!Engine::StartFromJson(XEngineGameBuild::EmbeddedEngineConfig())) return 1;
#else
    if (!Engine::Start("./config/engine.json")) return 1;
#endif

    Engine::SetInitCallback(MyPrincessRun::InitGame);
    return Engine::Run();
}

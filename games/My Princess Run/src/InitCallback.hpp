#pragma once
#include <XEngine.h>
#include "Level1.hpp"
#include "GameState.hpp"
#include <cstdio>

namespace MyPrincessRun
{
    inline bool LoadGameAssets()
    {
        bool valid = true;
        valid &= (Assets->LoadTexture("textures/ground.png", "ground") != nullptr);
        valid &= (Assets->LoadTexture("textures/princess-run.png", "princess-run") != nullptr);
        valid &= (Assets->LoadTexture("textures/princess-jump.png", "princess-jump") != nullptr);
        valid &= (Assets->LoadTexture("textures/princess-fall.png", "princess-fall") != nullptr);
        valid &= (Assets->LoadTexture("textures/princess-die.png", "princess-die") != nullptr);
        valid &= (Assets->LoadTexture("textures/sky.png", "sky") != nullptr);
        valid &= (Assets->LoadTexture("textures/back-1.png", "rock-big") != nullptr);
        valid &= (Assets->LoadTexture("textures/back-2.png", "rock-small") != nullptr);
        valid &= (Assets->LoadTexture("textures/cloud-1.png", "cloud-big") != nullptr);
        valid &= (Assets->LoadTexture("textures/cloud-2.png", "cloud-small") != nullptr);
        valid &= (Assets->LoadTexture("textures/cactus-1.png", "cactus-1") != nullptr);
        valid &= (Assets->LoadTexture("textures/cactus-2.png", "cactus-2") != nullptr);
        valid &= (Assets->LoadTexture("textures/cactus-3.png", "cactus-3") != nullptr);
        valid &= (Assets->LoadTexture("textures/ui-btn-down.png", "ui-button") != nullptr);
        // The ui_default font is loaded through assets/config/engine.json.
        // This 50-point font variant is used by the death screen.
        valid &= (Assets->LoadFont("fonts/ui-font-default.ttf", "ui_default_big", 50) != nullptr);
        return valid;
    }

    inline void InitGame(Engine& engine)
    {
        (void)engine;
        if (!LoadGameAssets())
        {
            std::fputs("[My Princess Run] Missing assets; see MIGRATION.md\n", stderr);
            return;
        }
        ResetGameState();
        Scenes->Register("level1", BuildLevel1);
        Scenes->SetActive("level1");
    }
}

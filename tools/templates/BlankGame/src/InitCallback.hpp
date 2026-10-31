#pragma once

#include <XEngine.h>
#include <cstdio>
#include "StartScene.hpp"

// Load shared assets here before registering scenes, if needed.
inline void InitGame(Engine& engine)
{
    (void)engine;
    if (!Input->LoadInputSchemes("input/Default.inputschemes.json") ||
        !Input->LoadPlayerProfiles("input/Default.players.json") ||
        !Input->LoadActionAsset("input/Default.inputactions.json"))
        std::fprintf(stderr, "[INPUT] %s\n", Input->GetInputError().c_str());
    else {
        for (PlayerId player = 0; player < 3; ++player)
            Input->PushActionContext(player, "Playing");
    }
    Scenes->Register("StartScene", BuildStartScene);
    Scenes->SetActive("StartScene");
}

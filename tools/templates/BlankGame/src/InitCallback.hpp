#pragma once

#include <XEngine.h>
#include "StartScene.hpp"

// Load shared assets here before registering scenes, if needed.
inline void InitGame(Engine& engine)
{
    (void)engine;
    Scenes->Register("StartScene", BuildStartScene);
    Scenes->SetActive("StartScene");
}

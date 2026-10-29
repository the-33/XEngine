#pragma once
#include <XEngine.h>
#include "gameobjects/ExampleObject.hpp"

inline void BuildStartScene(Scene* scene)
{
    if (!scene) return;
    CreateExampleObject(*scene);
}

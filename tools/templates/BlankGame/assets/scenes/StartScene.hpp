#pragma once

#include "ExampleObject.hpp"
#include <Scene.h>

// La escena solo crea y configura sus objetos.
inline void BuildStartScene(Scene* scene)
{
    if (!scene) return;
    CreateExampleObject(*scene);
}

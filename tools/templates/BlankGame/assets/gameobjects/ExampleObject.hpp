#pragma once
#include <XEngine.h>
#include "scripts/ExampleBehaviour.hpp"

inline GameObject* CreateExampleObject(Scene& scene)
{
    auto* object = scene.CreateObject("ExampleObject");
    if (object) object->AddComponent<ExampleBehaviour>();
    return object;
}

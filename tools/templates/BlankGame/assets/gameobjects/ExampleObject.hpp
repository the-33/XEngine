#pragma once
class Scene;
class GameObject;
GameObject* CreateExampleObject(Scene& scene);

#include "ExampleBehaviour.hpp"
#include <Scene.h>
#include <GameObject.h>

inline GameObject* CreateExampleObject(Scene& scene)
{
    GameObject* object = scene.CreateObject("ExampleObject");
    object->AddComponent<ExampleBehaviour>();
    return object;
}

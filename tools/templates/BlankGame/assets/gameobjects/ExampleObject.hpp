#pragma once
#include <XEngine.h>
#include "scripts/ExampleBehaviour.hpp"

inline GameObject* CreateExampleObject(Scene& scene)
{
    auto* object = scene.CreateObject("ExampleObject");
    if (object) {
        PlayerInputSettings controls;
        controls.player = 0;
        controls.scheme = "KeyboardMouseGamepad";
        controls.mode = InputAssignmentMode::Prefer;
        controls.idealDevice = InputDeviceKind::Gamepad;
        controls.autoJoin = true;
        controls.priority = 100;
        object->AddComponent<PlayerInput>(controls);
        object->AddComponent<ExampleBehaviour>();
    }
    return object;
}

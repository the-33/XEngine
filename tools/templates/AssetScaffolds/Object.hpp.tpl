#pragma once
#include <XEngine.h>
{{EXTRA_INCLUDES}}
inline GameObject* Create{{NAME}}(Scene& scene)
{
    auto* object = scene.CreateObject("{{NAME}}");
    if (!object) return nullptr;
{{SPRITE_SETUP}}{{COLLIDER_SETUP}}{{BEHAVIOUR_SETUP}}    return object;
}

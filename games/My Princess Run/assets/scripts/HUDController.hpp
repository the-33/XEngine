#pragma once
#include <XEngine.h>
#include "GameState.hpp"

namespace MyPrincessRun
{
class HUDController : public Behaviour
{
    void Render() override
    {
        if (gSpeedMultiplier > 0.f)
        {
            UI->Label("Score: " + std::to_string(static_cast<int>(gScore)), 10.f, 10.f);
        }
        else
        {
            if (auto* font = Assets->GetFontByKey("ui_default_big"))
                UI->Label("Score: " + std::to_string(static_cast<int>(gScore)),
                { ((float)Window->GetDrawableSize().x / 2) - (50.f / 2.f), ((float)Window->GetDrawableSize().y / 2) - 80.f, 50.f, 50.f },
                *font,
                Color::DarkGray(),
				AlignH::Center,
				AlignV::Middle,
				0.f
            );

            bool clicked = false;
            if (auto* buttonTexture = Assets->GetTextureByKey("ui-button"))
                clicked = UI->ImageButton(
                    "ui-button",
                    { ((float)Window->GetDrawableSize().x / 2) - (50.f / 2.f), ((float)Window->GetDrawableSize().y / 2) - (50.f / 2.f), 50.f, 50.f },
                    *buttonTexture
                );

            if (clicked)
            {
                Scenes->SetActive("level1");
                gSpeedMultiplier = 1.f;
				gScore = 0.f;
            }
        }
    }
};
}

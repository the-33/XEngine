#pragma once
#include <Behaviour.h>
class HUDController final : public Behaviour
{
public:
    void Update(float dt) override;
};

#include "PlayerController.hpp"
#include "GameState.hpp"
#include <XEngine.h>
#include <string>
inline void HUDController::Update(float)
{
    auto* player = gameObject->scene->Find("Player");
    if (!player) return;
    auto* controller = player->GetComponent<PlayerController>();
    if (!controller) return;
    auto& ui = UIManager::GetInstance();
    ui.ProgressBar(controller->lives / 3.f, Rect(10.f, 10.f, 160.f, 12.f));
    ui.Label("Score: " + std::to_string(BlobQuest::Score()), 11.f, 30.f);
    const int width = Window->GetDrawableSize().x;
    const Rect restart{width - 140.f, 10.f, 120.f, 40.f};
    if (ui.Button("btn_restart", restart, "Restart")) BlobQuest::RestartLevel();
}

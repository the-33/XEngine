#pragma once
#include <XEngine.h>

namespace BlobQuest
{
    // One score for the whole program, even if included in several translation units.
    inline int score = 0;

    inline int Score() noexcept { return score; }
    inline void AddScore(int amount = 1) noexcept { score += amount; }
    inline void ResetScore() noexcept { score = 0; }

    inline void RestartLevel()
    {
        ResetScore();
        Scenes->SetActive("level1");
        if (auto* music = Assets->GetMusicByKey("bg_music"))
            Sound->PlayMusic(music, -1, 0.7f);
    }
}

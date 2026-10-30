#pragma once

#include <XEngine.h>
#include "Level1.hpp"
#include "GameState.hpp"
#include <cstdio>

// Game initialization callback: not tied to any individual scene.
// Allows registering multiple scenes without startup dependencies inside them.
namespace BlobQuest
{
    inline bool LoadGameAssets()
    {
        // Service shortcuts are lightweight accessors, not raw pointers.
        auto* assets = Assets.get();
        bool valid = true;
        valid &= (assets->LoadTexture("textures/player.png", "player", 100.f) != nullptr);
        valid &= (assets->LoadTexture("textures/enemy.png", "enemy", 100.f) != nullptr);
        valid &= (assets->LoadTexture("textures/wall.png", "wall", 100.f) != nullptr);
        valid &= (assets->LoadTexture("textures/coin.png", "coin", 100.f) != nullptr);
        valid &= (assets->LoadTexture("textures/floor.png", "floor", 100.f) != nullptr);
        valid &= (assets->LoadTexture("textures/fireball.png", "fireball", 100.f) != nullptr);

        // The ui_default font is loaded through assets/config/engine.json.
        assets->LoadMusic("audio/music.ogg", "bg_music");
        assets->LoadSFX("audio/coin.wav", "coin");
        assets->LoadSFX("audio/hit.wav", "hit");
        assets->LoadSFX("audio/enemy_hit.wav", "enemy_hit");
        assets->LoadSFX("audio/enemy_die.wav", "enemy_die");
        return valid;
    }

    inline void SetupBackground()
    {
        auto* floor = Assets->GetTextureByKey("floor");
        if (!floor) return;
        const float worldW = floor->Width() / floor->PixelsPerUnit();
        const float worldH = floor->Height() / floor->PixelsPerUnit();
        RenderManager::BackgroundLayer layer;
        layer.mode = RenderManager::BackgroundMode::TileWorldPhysicalScaled;
        layer.tex = floor;
        layer.scale = Vec2{1.f / worldW, 1.f / worldH};
        Renderer->AddParallaxLayer(layer);
    }

    inline void InitGame(Engine& engine)
    {
        (void)engine;
        if (!LoadGameAssets())
        {
            std::fputs("[Blob's Quest] Missing textures in assets/textures; see MIGRATION.md\n", stderr);
            return;
        }
        ResetScore();
        SetupBackground();
        if (auto* music = Assets->GetMusicByKey("bg_music"))
            Sound->PlayMusic(music, -1, 0.7f);

        Scenes->Register("level1", BuildLevel1);
        Scenes->SetActive("level1");
    }
}

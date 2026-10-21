#pragma once

// Scene composition only: objects, positions, counts and camera.
// Asset loading, scene registration and activation belong in src/InitCallback.hpp.
#include "Player.hpp"
#include "Wall.hpp"
#include "Coin.hpp"
#include "HUDController.hpp"
#include "EnemySpawner.hpp"
#include <XEngine.h>

inline void BuildLevel1(Scene* scene)
{
    if (!scene) return;
    CreatePlayer(*scene, Vec3{0.f, 0.f, 1.f});

    // Arena layout: the scene, not the prefabs, determines geometry.
    for (int x = -20; x <= 20; ++x) {
        CreateWallTile(*scene, "WallTop", Vec3{static_cast<float>(x), -20.f, 0.f});
        CreateWallTile(*scene, "WallBot", Vec3{static_cast<float>(x), 20.f, 0.f});
    }
    for (int y = -20; y <= 20; ++y) {
        CreateWallTile(*scene, "WallLeft", Vec3{-21.f, static_cast<float>(y), 0.f});
        CreateWallTile(*scene, "WallRight", Vec3{21.f, static_cast<float>(y), 0.f});
    }
    CreateWallBoundary(*scene, "WallUpCollision", Vec3{0.f, -20.f, 0.f}, Vec2{41.f, 1.f});
    CreateWallBoundary(*scene, "WallDownCollision", Vec3{0.f, 20.f, 0.f}, Vec2{41.f, 1.f});
    CreateWallBoundary(*scene, "WallLeftCollision", Vec3{-21.f, 0.f, 0.f}, Vec2{1.f, 39.f});
    CreateWallBoundary(*scene, "WallRightCollision", Vec3{21.f, 0.f, 0.f}, Vec2{1.f, 39.f});

    for (int i = 0; i < 20; ++i) {
        SpawnCoin(*scene, Vec3{Random->Range(-19.f, 19.f),
                               Random->Range(-19.f, 19.f), 0.f});
    }
    scene->CreateObject("HUD")->AddComponent<HUDController>();
    scene->CreateObject("EnemySpawner")->AddComponent<EnemySpawner>();
    if (Camera2D* camera = scene->camera) {
        camera->center = Vec2{0.f, 0.f};
        camera->viewBase = Vec2{20.f, 11.25f};
    }
}

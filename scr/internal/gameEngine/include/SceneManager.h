#pragma once

#include <memory>
#include <unordered_map>
#include <string>

#include "Singleton.h"
#include "Engine.h"

// Forward declarations
class AssetManager;
class RenderManager;
class SoundManager;
class CollisionManager;
class Scene;

using SceneBuilder = std::function<void(Scene* scene)>;

class SceneManager : public Singleton<SceneManager>
{
    friend class Singleton<SceneManager>;
    friend class Engine;

private:
    SceneManager() = default;
    ~SceneManager() = default;
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    SceneManager(SceneManager&&) = delete;
    SceneManager& operator=(SceneManager&&) = delete;

    AssetManager* mAssets = nullptr;
    RenderManager* mRender = nullptr;
    SoundManager* mSound = nullptr;
    CollisionManager* mCollision = nullptr;

    std::unique_ptr<Scene> mActive;
    std::string mActiveId = "";

    struct Entry
    {
        SceneBuilder builder;
        bool persistent = false;
        std::unique_ptr<Scene> instance; // solo si persistent
    };

    std::unordered_map<std::string, Entry> mRegistry;
    int mUnnamedCounter = 0;

    std::string mPendingSceneId;
    std::unique_ptr<Scene> mPendingScene;
    bool mHasPending = false;

    bool Init() noexcept;

    void Shutdown() noexcept;

    // --- Bucle principal ---
    void FixedUpdate(float fixedDt);
    void Update(float dt);
    void Render();

    bool HasPendingScene() const noexcept;

    void ApplyPendingScene() noexcept;

        std::unique_ptr<Scene> NewEmptyScene();

public:
    // --- Gestión de escenas ---

    void Register(const std::string& id, SceneBuilder builder) noexcept;
    void SetPersistent(const std::string& id, bool persistent) noexcept;

    void SetActive(std::unique_ptr<Scene> s) noexcept;
    bool SetActive(const std::string& id) noexcept;

    // Obtener punteros (no ownership)
    inline Scene* GetActive() noexcept { return mActive.get(); }
    inline const Scene* GetActive() const noexcept { return mActive.get(); }
};
#pragma once
/** @file SceneManager.h
 * @brief Scene registration, activation and persistence choices.
 */

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

/** @brief Public type alias for the scene builder accessor.
 */
using SceneBuilder = std::function<void(Scene* scene)>;

/** @brief Registers scene builders and queues scene activation without invalidating callbacks.
 * @ingroup xengine_core
 */
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
        std::unique_ptr<Scene> instance; // only if persistent
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
    // --- Scene management ---

    /** @brief Registers a named scene-builder callback to construct a scene on activation.
     * @param id Stable identifier chosen by the caller.
     * @param builder Callback invoked to populate the new scene.
     */
    void Register(const std::string& id, SceneBuilder builder) noexcept;
    /** @brief Controls whether the named scene instance is retained across scene switches.
     * @param id Stable identifier chosen by the caller.
     * @param persistent Whether to retain the value across relevant operations.
     */
    void SetPersistent(const std::string& id, bool persistent) noexcept;

    /** @brief Requests scene activation at the engine's safe transition boundary.
     * @param s Scalar multiplier, divisor or input value.
     */
    void SetActive(std::unique_ptr<Scene> s) noexcept;
    /** @brief Requests scene activation at the engine's safe transition boundary.
     * @param id Stable identifier chosen by the caller.
     */
    bool SetActive(const std::string& id) noexcept;

    // Obtener punteros (no ownership)
    /** @brief Returns the active scene as a non-owning pointer; its lifetime changes on scene switches.
     */
    inline Scene* GetActive() noexcept { return mActive.get(); }
    /** @brief Returns the active scene as a non-owning pointer; its lifetime changes on scene switches.
     */
    inline const Scene* GetActive() const noexcept { return mActive.get(); }
};
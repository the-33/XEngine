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

    /** @brief Registers/replaces a builder for future activation of a scene ID.
     * @param id Stable identifier chosen by the caller.
     * @param builder Callback invoked to populate the new scene.
     * @note Empty IDs are replaced by generated `unnamed_N` identifiers.
     */
    void Register(const std::string& id, SceneBuilder builder) noexcept;
    /** @brief Configures caching of a scene instance across future transitions.
     * @param id Stable identifier chosen by the caller.
     * @param persistent True to retain and reactivate the same instance; false
     *                   to rebuild it on subsequent activations.
     * @note Has no effect for an unregistered ID.
     */
    void SetPersistent(const std::string& id, bool persistent) noexcept;

    /** @brief Queues an owned scene instance for activation at the next scene boundary.
     * @param s Scene ownership moved into the manager; do not retain raw pointers to it.
     */
    void SetActive(std::unique_ptr<Scene> s) noexcept;
    /** @brief Queues activation of a registered scene ID at the next scene boundary.
     * @param id Stable identifier chosen by the caller.
     * @return False if the ID is not registered; no scene change is then queued.
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

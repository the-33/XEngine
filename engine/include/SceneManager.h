/** @brief Grant Singleton< SceneManager > access to the internal state of SceneManager.
 */
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
    /** @brief Permit the singleton base to construct and release the SceneManager manager.
     */
    friend class Singleton<SceneManager>;
    /** @brief Grant Engine access to the internal state of SceneManager.
     */
    friend class Engine;

private:
    /** @brief Construct a SceneManager instance.
     */
    SceneManager() = default;
    /** @brief Destroy a SceneManager instance and release owned resources.
     */
    ~SceneManager() = default;
    /** @brief Construct a SceneManager instance.
     * @param other Source object whose copy, move or equality is being requested.
     */
    SceneManager(const SceneManager& other) = delete;
    /** @brief Implement the operator= operation for SceneManager.
     * @return Result of applying the overloaded operator.
     * @param other Source object whose copy, move or equality is being requested.
     */
    SceneManager& operator=(const SceneManager& other) = delete;
    /** @brief Construct a SceneManager instance.
     * @param other Source object whose copy, move or equality is being requested.
     */
    SceneManager(SceneManager&& other) = delete;
    /** @brief Implement the operator= operation for SceneManager.
     * @return Result of applying the overloaded operator.
     * @param other Source object whose copy, move or equality is being requested.
     */
    SceneManager& operator=(SceneManager&& other) = delete;

    /** @brief Stored assets value used by SceneManager.
     */
    AssetManager* mAssets = nullptr;
    /** @brief Stored render value used by SceneManager.
     */
    RenderManager* mRender = nullptr;
    /** @brief Stored sound value used by SceneManager.
     */
    SoundManager* mSound = nullptr;
    /** @brief Stored collision value used by SceneManager.
     */
    CollisionManager* mCollision = nullptr;

    /** @brief Stored active value used by SceneManager.
     */
    std::unique_ptr<Scene> mActive;
    /** @brief Stored active id value used by SceneManager.
     */
    std::string mActiveId = "";

    /** @brief Entry data and utilities used by SceneManager.
     */
    struct Entry
    {
        /** @brief Configured builder value used by Entry.
         */
        SceneBuilder builder;
        /** @brief Configured persistent value used by Entry.
         */
        bool persistent = false;
        /** @brief Configured instance value used by Entry.
         */
        std::unique_ptr<Scene> instance; // only if persistent
    };

    /** @brief Stored registry value used by SceneManager.
     */
    std::unordered_map<std::string, Entry> mRegistry;
    /** @brief Stored unnamed counter value used by SceneManager.
     */
    int mUnnamedCounter = 0;

    /** @brief Stored pending scene id value used by SceneManager.
     */
    std::string mPendingSceneId;
    /** @brief Stored pending scene value used by SceneManager.
     */
    std::unique_ptr<Scene> mPendingScene;
    /** @brief Stored has pending value used by SceneManager.
     */
    bool mHasPending = false;

    /** @brief Initialize the SceneManager subsystem.
     * @return True if the operation succeeded or its condition holds; otherwise false.
     */
    bool Init() noexcept;

    /** @brief Release resources managed by SceneManager.
     */
    void Shutdown() noexcept;

    // --- Bucle principal ---
    /** @brief Fixed update in SceneManager.
     * @param fixedDt Fixed dt supplied to SceneManager.
     */
    void FixedUpdate(float fixedDt);
    /** @brief Update the SceneManager state for the current frame.
     * @param dt Elapsed simulation time for the update, in seconds.
     */
    void Update(float dt);
    /** @brief Late update in SceneManager.
     * @param dt Elapsed simulation time for the update, in seconds.
     */
    void LateUpdate(float dt);
    /** @brief Render in SceneManager.
     */
    void Render();

    /** @brief Test whether SceneManager has pending scene.
     * @return True when has pending scene; otherwise false.
     */
    bool HasPendingScene() const noexcept;

    /** @brief Apply pending scene in SceneManager.
     */
    void ApplyPendingScene() noexcept;

        /** @brief New empty scene in SceneManager.
         * @return Result of new empty scene in SceneManager.
         */
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
     * @return The active scene as a non-owning pointer; its lifetime changes on scene switches.
     */
    inline Scene* GetActive() noexcept { return mActive.get(); }
    /** @brief Returns the active scene as a non-owning pointer; its lifetime changes on scene switches.
     * @return The active scene as a non-owning pointer; its lifetime changes on scene switches.
     */
    inline const Scene* GetActive() const noexcept { return mActive.get(); }
};

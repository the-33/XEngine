#pragma once
/** @file Scene.h
 * @brief Owning scene, entity creation and deferred object destruction.
 */

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "BaseTypes.h"
#include "SceneManager.h"
#include "Engine.h"

// Forward declarations
class AssetManager;
class RenderManager;
class SoundManager;
class CollisionManager;
class Camera2D;
class GameObject;

using EntityID = uint32_t;
/** @brief Public type alias for the instance builder accessor.
 */
using InstanceBuilder = std::function<void(GameObject&, Scene&)>;

/** @brief Owns the active scene's entities and camera; object deletion is deferred safely.
 * @ingroup xengine_core
 */
class Scene
{
	friend class SceneManager;
    friend class Engine;

private:
    Scene() noexcept;

    static std::unique_ptr<Scene> Create();

    // --- Ciclo de vida global ---
    void AwakeAll();       // called once when loading the scene
    void StartAll();       // called before the first frame (after Awake)
    void OnEnableAll();    // if the scene is reactivated

    void Register();

    // --- Bucle principal ---
    void FixedUpdate(float dt);
    void Update(float dt);
    void Render();

    // --- Limpieza ---
    void OnDisableAll();   // if the scene is paused or disabled
    void OnDestroyAll();   // on shutdown

    inline AssetManager* GetAssets() const noexcept { return mAssets; }
    inline RenderManager* GetRender() const noexcept { return mRender; }
    inline SoundManager* GetSound() const noexcept { return mSound; }
    inline CollisionManager* GetCollision() const noexcept { return mCollision; }

    void FlushDestroyQueue();
    void ProcessNewObjects();

    void CollectDescendants_(GameObject* root, std::vector<EntityID>& out);

    friend class SceneManager;
    bool mStarted = false;

    AssetManager* mAssets = nullptr;
    RenderManager* mRender = nullptr;
    SoundManager* mSound = nullptr;
    CollisionManager* mCollision = nullptr;

    EntityID mNextID = 1;
    std::vector<std::unique_ptr<GameObject>> mEntities;
    std::vector<EntityID> mDestroyQueue;
    std::vector<GameObject*> mNew;
    std::vector<GameObject*> mUninitialized;

    std::unordered_map<EntityID, GameObject*> mById;
    std::unordered_map<std::string, GameObject*> mByName;

    std::unordered_map<std::string, int> repeatedNamesCount;

    std::unique_ptr<Camera2D> mCamera;

    bool mDestroyed = false;

    Camera2D* GetCamera() const noexcept { return mCamera.get(); }

public:
    /** @brief Releases the Scene instance and any owned resources.
     */
    ~Scene() noexcept;
    /** @brief Creates a named scene-owned GameObject, optionally parented to another object.
     * @param name Name used to identify the resource or entity.
     * @param parent Optional parent entity; null means no parent.
     */
    GameObject* CreateObject(std::string name, GameObject* parent = nullptr);

    /** @brief Constructs an entity with initial transform and runs the supplied builder callback.
     * @param name Name used to identify the resource or entity.
     * @param build Callback invoked to configure the new entity.
     * @param position Position in the relevant world or local coordinate space.
     * @param rotation Orientation expressed in degrees.
     * @param parent Optional parent entity; null means no parent.
     */
    GameObject* Instantiate(const std::string& name,
        InstanceBuilder build,
        Vec3 position = {},
        Vec3 rotation = {},
        GameObject* parent = nullptr);

    /** @brief Requests destruction of the scene-owned entity identified by EntityID.
     * @param id Stable identifier chosen by the caller.
     */
    void DestroyObject(EntityID id);
    /** @brief Requests destruction of all entities in this scene.
     */
    void DestroyAll();    

    /** @brief Finds a scene entity by identifier or name; returns a non-owning pointer or nullptr.
     * @param id Stable identifier chosen by the caller.
     */
    GameObject* Find(EntityID id) noexcept;
    /** @brief Finds a scene entity by identifier or name; returns a non-owning pointer or nullptr.
     * @param name Name used to identify the resource or entity.
     */
    GameObject* Find(const std::string& name) noexcept;

    /** @brief Returns the scene-owned entity collection for read-only inspection.
     */
    inline const std::vector<std::unique_ptr<GameObject>>& GetEntities() const noexcept { return mEntities; }

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `camera`; use the corresponding instance member in gameplay code.
     */
    using CameraProperty = PropertyRO<Scene, Camera2D*,
		&Scene::GetCamera>;
    /** @endcond */
	/** @brief Camera owned by this scene, used for world-space rendering.
	 */
	CameraProperty camera{ this };
};
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

/** @brief Owns its entities and camera, including deferred single-object destruction.
 * @note `DestroyObject()` queues removal until the engine flushes its queue;
 *       `DestroyAll()` immediately clears the collection.
 * @ingroup xengine_core
 */
class Scene
{
	/** @brief Grant SceneManager access to the internal state of Scene.
	 */
	friend class SceneManager;
    /** @brief Grant Engine access to the internal state of Scene.
     */
    friend class Engine;
    /** @brief Grant GameObject access to the internal state of Scene.
     */
    friend class GameObject;

private:
    /** @brief Construct a Scene instance.
     */
    Scene() noexcept;

    /** @brief Create in Scene.
     * @return Result of create in Scene.
     */
    static std::unique_ptr<Scene> Create();

    // --- Ciclo de vida global ---
    /** @brief Awake all in Scene.
     */
    void AwakeAll();       // called once when loading the scene
    /** @brief Start all in Scene.
     */
    void StartAll();       // called before the first frame (after Awake)
    /** @brief On enable all in Scene.
     */
    void OnEnableAll();    // if the scene is reactivated

    /** @brief Register in Scene.
     */
    void Register();

    // --- Bucle principal ---
    /** @brief Fixed update in Scene.
     * @param dt Elapsed simulation time for the update, in seconds.
     */
    void FixedUpdate(float dt);
    /** @brief Update the Scene state for the current frame.
     * @param dt Elapsed simulation time for the update, in seconds.
     */
    void Update(float dt);
    /** @brief Late update in Scene.
     * @param dt Elapsed simulation time for the update, in seconds.
     */
    void LateUpdate(float dt);
    /** @brief Render in Scene.
     */
    void Render();

    // --- Limpieza ---
    /** @brief On disable all in Scene.
     */
    void OnDisableAll();   // if the scene is paused or disabled
    /** @brief On destroy all in Scene.
     */
    void OnDestroyAll();   // on shutdown

    /** @brief Retrieve the assets from Scene.
     * @return Requested get assets value or reference.
     */
    inline AssetManager* GetAssets() const noexcept { return mAssets; }
    /** @brief Retrieve the render from Scene.
     * @return Requested get render value or reference.
     */
    inline RenderManager* GetRender() const noexcept { return mRender; }
    /** @brief Retrieve the sound from Scene.
     * @return Requested get sound value or reference.
     */
    inline SoundManager* GetSound() const noexcept { return mSound; }
    /** @brief Retrieve the collision from Scene.
     * @return Requested get collision value or reference.
     */
    inline CollisionManager* GetCollision() const noexcept { return mCollision; }

    /** @brief Flush destroy queue in Scene.
     */
    void FlushDestroyQueue();
    /** @brief Process new objects in Scene.
     */
    void ProcessNewObjects();

    /** @brief Collect descendants in Scene.
     * @param root Root supplied to Scene.
     * @param out Caller-provided output destination.
     */
    void CollectDescendants_(GameObject* root, std::vector<EntityID>& out);

    friend class SceneManager;
    /** @brief Stored started value used by Scene.
     */
    bool mStarted = false;

    /** @brief Stored assets value used by Scene.
     */
    AssetManager* mAssets = nullptr;
    /** @brief Stored render value used by Scene.
     */
    RenderManager* mRender = nullptr;
    /** @brief Stored sound value used by Scene.
     */
    SoundManager* mSound = nullptr;
    /** @brief Stored collision value used by Scene.
     */
    CollisionManager* mCollision = nullptr;

    /** @brief Stored next id value used by Scene.
     */
    EntityID mNextID = 1;
    /** @brief Stored entities value used by Scene.
     */
    std::vector<std::unique_ptr<GameObject>> mEntities;
    /** @brief Stored destroy queue value used by Scene.
     */
    std::vector<EntityID> mDestroyQueue;
    /** @brief Stored new value used by Scene.
     */
    std::vector<GameObject*> mNew;
    /** @brief Stored uninitialized value used by Scene.
     */
    std::vector<GameObject*> mUninitialized;

    /** @brief Stored by id value used by Scene.
     */
    std::unordered_map<EntityID, GameObject*> mById;
    /** @brief Stored by name value used by Scene.
     */
    std::unordered_map<std::string, GameObject*> mByName;

    /** @brief Configured repeated names count value used by Scene.
     */
    std::unordered_map<std::string, int> repeatedNamesCount;

    /** @brief Stored camera value used by Scene.
     */
    std::unique_ptr<Camera2D> mCamera;

    /** @brief Stored destroyed value used by Scene.
     */
    bool mDestroyed = false;

    /** @brief Retrieve the camera from Scene.
     * @return Requested get camera value or reference.
     */
    Camera2D* GetCamera() const noexcept { return mCamera.get(); }

public:
    /** @brief Releases the Scene instance and any owned resources.
     */
    ~Scene() noexcept;
    /** @brief Creates a scene-owned entity with a Transform and a unique scene name.
     * @param name Preferred name; duplicates receive a numeric suffix.
     * @param parent Optional parent entity; null means no parent.
     * @return Scene-owned pointer that remains valid until destruction or scene teardown.
     */
    GameObject* CreateObject(std::string name, GameObject* parent = nullptr);

    /** @brief Creates an entity, invokes its builder, then applies world pose.
     * @param name Preferred name; duplicates are automatically renamed.
     * @param build Optional callback receiving the new entity and owning scene.
     * @param position Initial *world-space* XYZ position, applied after the builder.
     * @param rotation Initial *world-space* XYZ rotation in degrees.
     * @param parent Optional parent entity; null means no parent.
     * @note The builder sees the entity before the final world pose is assigned.
     * @return Result of instantiate in Scene.
     */
    GameObject* Instantiate(const std::string& name,
        InstanceBuilder build,
        Vec3 position = {},
        Vec3 rotation = {},
        GameObject* parent = nullptr);

    /** @brief Requests destruction of the scene-owned entity identified by EntityID.
     * @param id Scene-assigned ID (for example, an object's `ID`).
     * @note Actual removal happens during queue flushing, after a frame phase.
     */
    void DestroyObject(EntityID id);
    /** @brief Immediately invokes OnDestroy and removes every scene entity.
     * @warning Invalidates all GameObject and Component pointers from this scene.
     */
    void DestroyAll();    

    /** @brief Finds an entity by its scene-assigned ID; returns nullptr if absent.
     * @param id ID generated by `CreateObject`.
     * @return Requested find value or reference.
     */
    GameObject* Find(EntityID id) noexcept;
    /** @brief Finds an entity by its current scene name, or returns nullptr.
     * @param name Name assigned at creation or through `GameObject::name`.
     * @note Renaming keeps this index synchronized. Names must be unique;
     *       assigning an already-used name leaves the original name unchanged.
     * @return Requested find value or reference.
     */
    GameObject* Find(const std::string& name) noexcept;

    /** @brief Returns the scene-owned entity collection for read-only inspection.
     * @return The scene-owned entity collection for read-only inspection.
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

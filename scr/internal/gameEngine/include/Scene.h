#pragma once

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
using InstanceBuilder = std::function<void(GameObject&, Scene&)>;

class Scene
{
	friend class SceneManager;
    friend class Engine;

private:
    Scene() noexcept;

    static std::unique_ptr<Scene> Create();

    // --- Ciclo de vida global ---
    void AwakeAll();       // llamado una sola vez al cargar la escena
    void StartAll();       // llamado antes del primer frame (tras Awake)
    void OnEnableAll();    // por si la escena se re-activa

    void Register();

    // --- Bucle principal ---
    void FixedUpdate(float dt);
    void Update(float dt);
    void Render();

    // --- Limpieza ---
    void OnDisableAll();   // si la escena se pausa o desactiva
    void OnDestroyAll();   // al cerrar

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

    Camera2D* mCamera = nullptr;

    bool mDestroyed = false;

    Camera2D* GetCamera() const noexcept { return mCamera; }

public:
    ~Scene() noexcept;
    GameObject* CreateObject(std::string name, GameObject* parent = nullptr);

    GameObject* Instantiate(const std::string& name,
        InstanceBuilder build,
        Vec3 position = {},
        Vec3 rotation = {},
        GameObject* parent = nullptr);

    void DestroyObject(EntityID id);
    void DestroyAll();    

    GameObject* Find(EntityID id) noexcept;
    GameObject* Find(const std::string& name) noexcept;

    inline const std::vector<std::unique_ptr<GameObject>>& GetEntities() const noexcept { return mEntities; }

    using CameraProperty = PropertyRO<Scene, Camera2D*,
		&Scene::GetCamera>;
	CameraProperty camera{ this };
};
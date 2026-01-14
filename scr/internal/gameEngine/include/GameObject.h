#pragma once

#include <vector>
#include <memory>
#include <string>

#include "Property.h"
#include "Scene.h"

using EntityID = uint32_t;

// Forward declarations
class Transform;
class CollisionManager;
class Scene;
class Component;
class Behaviour;

struct ContactPoint {
  Vec2 point;      // punto de contacto en mundo
  Vec2 normalA;    // normal saliendo de A hacia B (unitaria)
  Vec2 normalB;    // = -normalA
  float penetration; 
};

struct CollisionInfo2D {
  GameObject* self = nullptr;
  GameObject* other = nullptr;
  Collider2D* selfCollider;
  Collider2D* otherCollider;
  std::vector<ContactPoint> contacts; // ya lo tienes definido
};

class GameObject
{
	friend class Transform;
	friend class CollisionManager;
    friend class Scene;

private:
    explicit GameObject(const std::string& name, EntityID id, Scene* scene) noexcept;

    static std::unique_ptr<GameObject> Create(const std::string& name, EntityID id, Scene* scene);

    GameObject* Parent() const noexcept { return mParent; }
    const std::vector<GameObject*>& Children() const noexcept { return mChildren; }

    // --- Ciclo de vida ---
    void Awake();       // Propaga a componentes + hijos
    void Start();
    void OnEnable();
    void OnDisable();
    void OnDestroy();

    void Register();

    // --- Bucle principal ---
    void FixedUpdate(float dt);
    void Update(float dt);
    void Render();

    void OnCollisionEnter(const CollisionInfo2D&);
    void OnCollisionStay(const CollisionInfo2D&);
    void OnCollisionExit(const CollisionInfo2D&);

    void OnTriggerEnter(const CollisionInfo2D&);
    void OnTriggerStay(const CollisionInfo2D&);
    void OnTriggerExit(const CollisionInfo2D&);

    // Añadir/quitar hijos (se usan desde SetParent)
    void AddChild(GameObject* child) noexcept;      // no transfiere ownership
    void RemoveChild(GameObject* child) noexcept;

    bool IsActiveInHierarchy() const noexcept { return mActive && mParentActive; }
    bool IsActive() const noexcept { return mActive; }

    void AwakeTransform();
	void StartTransform();
    inline Transform* GetTransform() const noexcept { return mTransform; }

    inline EntityID GetID() const noexcept { return id; }
    inline Scene* GetScene() const noexcept { return mScene; }
    inline std::string GetName() const noexcept { return mName; }
    inline void SetName(std::string name) noexcept { mName = name; }
    inline std::string GetTag() const noexcept { return mTag; }
    inline void SetTag(std::string tag) noexcept { mTag = tag; }
    inline GameObject* GetParent() const noexcept { return mParent; }

    std::string mName;
    std::string mTag;

    GameObject* mParent = nullptr;
    std::vector<GameObject*> mChildren;

    EntityID id = 0;
    Scene* mScene = nullptr;
    std::vector<std::unique_ptr<Component>> components;
    Transform* mTransform;

    bool mActive = true;
    bool mParentActive = true;
    bool mAwoken = false;
    bool mStarted = false;

public:
    GameObject() noexcept;

    // Cambia el padre. Si keepWorld==true, convierte local para mantener la misma world pose.
    void SetParent(GameObject* newParent, bool keepWorld = true) noexcept;

    // Enable/Disable se suele propagar a los hijos (opcional)
    void SetActive(bool a) noexcept;
    const void Destroy() noexcept;

    template <class T, class... Args>
    T* AddComponent(Args &&...args);

    template <class T>
    T* GetComponent() noexcept;

    template<class T>
    std::vector<T*> GetComponents() noexcept;

    using ActiveInHierarchyProp = PropertyRO<GameObject, bool, &GameObject::IsActiveInHierarchy>;
    ActiveInHierarchyProp activeInHierarchy { this };

    using ActiveProp = PropertyRO<GameObject, bool, &GameObject::IsActive>;
    ActiveProp activeSelf { this };

	using TransformProp = PropertyRO<GameObject, Transform*, &GameObject::GetTransform>;
    TransformProp transform { this };

	using IDProp = PropertyRO<GameObject, EntityID, &GameObject::GetID>;
    IDProp ID{ this };

	using SceneProp = PropertyRO<GameObject, Scene*, &GameObject::GetScene>;
	SceneProp scene{ this };

	using NameProp = Property<GameObject, std::string, &GameObject::GetName, &GameObject::SetName>;
	NameProp name{ this };

    using TagProp = Property<GameObject, std::string, &GameObject::GetTag, &GameObject::SetTag>;
    TagProp tag{ this };

	using ParentProp = PropertyRO<GameObject, GameObject*, &GameObject::GetParent>;
	ParentProp parent{ this };
};

template<class T, class... Args>
T* GameObject::AddComponent(Args&&... args) {
    if constexpr (T::kUnique) {
        for (auto& c : components)
            if (auto p = dynamic_cast<T*>(c.get()))
                return p;
    }

    auto up = std::make_unique<T>(std::forward<Args>(args)...);
    up->mGameObject = this;
    T* raw = up.get();
    components.emplace_back(std::move(up));

    if constexpr (std::is_same_v<T, Transform>)
    {
        mTransform = raw; // puntero rápido coherente
        if (mAwoken)
			AwakeTransform();
        if (mStarted)
			StartTransform();
    }
    else
    {
        raw->OnAddedToGameObject(mAwoken, mStarted);
    }

    return raw;
}

template<class T>
T* GameObject::GetComponent() noexcept {
    for (auto& c : components)
        if (auto p = dynamic_cast<T*>(c.get()))
            return p;
    return nullptr;
}

template<class T>
std::vector<T*> GameObject::GetComponents() noexcept {
    std::vector<T*> out;
    out.reserve(4); // heurística
    for (auto& c : components)
        if (auto p = dynamic_cast<T*>(c.get()))
            out.push_back(p);
    return out;
}
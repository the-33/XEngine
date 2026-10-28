#pragma once
/** @file GameObject.h
 * @brief Entity hierarchy, lifetime, components and collision event values.
 */

#include <vector>
#include <memory>
#include <string>

#include "Property.h"
#include "Scene.h"

/** @brief Public type alias for the entity id accessor.
 */
using EntityID = uint32_t;

// Forward declarations
class Transform;
class CollisionManager;
class Scene;
class Component;
class Behaviour;

/** @brief Contact position and normal data delivered in collision notifications.
 */
struct ContactPoint {
  /** @brief World-space contact position.
   */
  Vec2 point;      // world-space contact point
  /** @brief Contact normal oriented relative to the first collider.
   */
  Vec2 normalA;    // unit normal pointing from A to B
  /** @brief Contact normal oriented relative to the second collider.
   */
  Vec2 normalB;    // = -normalA
  /** @brief Estimated overlap depth of the colliders.
   */
  float penetration; 
};

/** @brief Collision/trigger event data provided to Behaviour callbacks.
 */
struct CollisionInfo2D {
  /** @brief Value named self used by CollisionInfo2D.
   */
  GameObject* self = nullptr;
  /** @brief Other GameObject involved in this contact.
   */
  GameObject* other = nullptr;
  /** @brief Collider belonging to the receiving GameObject.
   */
  Collider2D* selfCollider;
  /** @brief Collider belonging to the other GameObject.
   */
  Collider2D* otherCollider;
  /** @brief Contact information for this collision event.
   */
  std::vector<ContactPoint> contacts; // already defined
};

/** @brief Named scene entity with a transform, components, parent and active state.
 * @ingroup xengine_core
 */
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

    // --- Lifecycle ---
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

    // Add/remove children (called from SetParent)
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
    /** @brief Constructs a GameObject instance.
     */
    GameObject() noexcept;

    // Change parent. If keepWorld == true, convert local coordinates to preserve the world pose.
    /** @brief Changes the parent of GameObject.
     * @param newParent Replacement parent object, or null to detach.
     * @param keepWorld Preserve the current world transform while changing parent.
     */
    void SetParent(GameObject* newParent, bool keepWorld = true) noexcept;

    // Enable/Disable generally propagates to children (optional)
    /** @brief Changes this entity's active state and notifies enabled behaviours as appropriate.
     * @param a First input operand.
     */
    void SetActive(bool a) noexcept;
    /** @brief Requests deferred destruction; do not retain entity/component pointers across destruction.
     */
    void Destroy() noexcept;

    template <class T, class... Args>
    /** @brief Adds and owns a component; returns an existing component if its type is unique.
     * @param args Arguments supplied to the operation.
     */
    T* AddComponent(Args &&...args);

    template <class T>
    /** @brief Finds the first component compatible with T, or returns nullptr.
     */
    T* GetComponent() noexcept;

    template<class T>
    /** @brief Returns all components compatible with T as non-owning pointers.
     */
    std::vector<T*> GetComponents() noexcept;

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `active in hierarchy`; use the corresponding instance member in gameplay code.
     */
    using ActiveInHierarchyProp = PropertyRO<GameObject, bool, &GameObject::IsActiveInHierarchy>;
    /** @endcond */
    /** @brief Read-only effective active state, including ancestor objects.
     */
    ActiveInHierarchyProp activeInHierarchy { this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `active`; use the corresponding instance member in gameplay code.
     */
    using ActiveProp = PropertyRO<GameObject, bool, &GameObject::IsActive>;
    /** @endcond */
    /** @brief Local active state; an inactive parent still disables descendants.
     */
    ActiveProp activeSelf { this };

	/** @cond XENGINE_INTERNAL */
	/** @brief Property accessor type for `transform`; use the corresponding instance member in gameplay code.
	 */
	using TransformProp = PropertyRO<GameObject, Transform*, &GameObject::GetTransform>;
	/** @endcond */
    /** @brief Non-owning accessor for this object’s Transform component.
     */
    TransformProp transform { this };

	/** @cond XENGINE_INTERNAL */
	/** @brief Public type alias for the idprop accessor.
	 */
	using IDProp = PropertyRO<GameObject, EntityID, &GameObject::GetID>;
	/** @endcond */
    /** @brief Value named ID used by GameObject.
     */
    IDProp ID{ this };

	/** @cond XENGINE_INTERNAL */
	/** @brief Property accessor type for `scene`; use the corresponding instance member in gameplay code.
	 */
	using SceneProp = PropertyRO<GameObject, Scene*, &GameObject::GetScene>;
	/** @endcond */
	/** @brief Non-owning accessor for the Scene that owns this object.
	 */
	SceneProp scene{ this };

	/** @cond XENGINE_INTERNAL */
	/** @brief Property accessor type for `name`; use the corresponding instance member in gameplay code.
	 */
	using NameProp = Property<GameObject, std::string, &GameObject::GetName, &GameObject::SetName>;
	/** @endcond */
	/** @brief Developer-assigned name for debugging and scene lookups.
	 */
	NameProp name{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `tag`; use the corresponding instance member in gameplay code.
     */
    using TagProp = Property<GameObject, std::string, &GameObject::GetTag, &GameObject::SetTag>;
    /** @endcond */
    /** @brief Developer-assigned tag used to classify and find objects.
     */
    TagProp tag{ this };

	/** @cond XENGINE_INTERNAL */
	/** @brief Property accessor type for `parent`; use the corresponding instance member in gameplay code.
	 */
	using ParentProp = PropertyRO<GameObject, GameObject*, &GameObject::GetParent>;
	/** @endcond */
	/** @brief Non-owning reference to the parent object, or null for a root object.
	 */
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
        mTransform = raw; // cached pointer kept consistent
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
    out.reserve(4); // heuristic
    for (auto& c : components)
        if (auto p = dynamic_cast<T*>(c.get()))
            out.push_back(p);
    return out;
}
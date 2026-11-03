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
  /** @brief Receiving GameObject for this collision/trigger callback.
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
	/** @brief Grant Transform access to the internal state of GameObject.
	 */
	friend class Transform;
	/** @brief Grant CollisionManager access to the internal state of GameObject.
	 */
	friend class CollisionManager;
    /** @brief Grant Scene access to the internal state of GameObject.
     */
    friend class Scene;

private:
    /** @brief Construct a GameObject instance.
     * @param name Name of the resource, property or element to access.
     * @param id Stable identifier associated with the requested object or control.
     * @param scene Scene to create, load, or inspect.
     */
    explicit GameObject(const std::string& name, EntityID id, Scene* scene) noexcept;

    /** @brief Create in GameObject.
     * @param name Name of the resource, property or element to access.
     * @param id Stable identifier associated with the requested object or control.
     * @param scene Scene to create, load, or inspect.
     * @return Result of create in GameObject.
     */
    static std::unique_ptr<GameObject> Create(const std::string& name, EntityID id, Scene* scene);

    /** @brief Parent in GameObject.
     * @return Result of parent in GameObject.
     */
    GameObject* Parent() const noexcept { return mParent; }
    /** @brief Children in GameObject.
     * @return Result of children in GameObject.
     */
    const std::vector<GameObject*>& Children() const noexcept { return mChildren; }

    // --- Lifecycle ---
    /** @brief Awake in GameObject.
     */
    void Awake();       // Propaga a componentes + hijos
    /** @brief Start in GameObject.
     */
    void Start();
    /** @brief On enable in GameObject.
     */
    void OnEnable();
    /** @brief On disable in GameObject.
     */
    void OnDisable();
    /** @brief On destroy in GameObject.
     */
    void OnDestroy();

    /** @brief Register in GameObject.
     */
    void Register();

    // --- Bucle principal ---
    /** @brief Fixed update in GameObject.
     * @param dt Elapsed simulation time for the update, in seconds.
     */
    void FixedUpdate(float dt);
    /** @brief Update the GameObject state for the current frame.
     * @param dt Elapsed simulation time for the update, in seconds.
     */
    void Update(float dt);
    /** @brief Late update in GameObject.
     * @param dt Elapsed simulation time for the update, in seconds.
     */
    void LateUpdate(float dt);
    /** @brief Render in GameObject.
     */
    void Render();

    /** @brief On collision enter in GameObject.
     * @param info Info supplied to GameObject.
     */
    void OnCollisionEnter(const CollisionInfo2D& info);
    /** @brief On collision stay in GameObject.
     * @param info Info supplied to GameObject.
     */
    void OnCollisionStay(const CollisionInfo2D& info);
    /** @brief On collision exit in GameObject.
     * @param info Info supplied to GameObject.
     */
    void OnCollisionExit(const CollisionInfo2D& info);

    /** @brief On trigger enter in GameObject.
     * @param info Info supplied to GameObject.
     */
    void OnTriggerEnter(const CollisionInfo2D& info);
    /** @brief On trigger stay in GameObject.
     * @param info Info supplied to GameObject.
     */
    void OnTriggerStay(const CollisionInfo2D& info);
    /** @brief On trigger exit in GameObject.
     * @param info Info supplied to GameObject.
     */
    void OnTriggerExit(const CollisionInfo2D& info);

    // Add/remove children (called from SetParent)
    /** @brief Add child in GameObject.
     * @param child Child supplied to GameObject.
     */
    void AddChild(GameObject* child) noexcept;      // no transfiere ownership
    /** @brief Remove child in GameObject.
     * @param child Child supplied to GameObject.
     */
    void RemoveChild(GameObject* child) noexcept;

    /** @brief Test whether GameObject is active in hierarchy.
     * @return True when is active in hierarchy; otherwise false.
     */
    bool IsActiveInHierarchy() const noexcept { return mActive && mParentActive; }
    /** @brief Test whether GameObject is active.
     * @return True when is active; otherwise false.
     */
    bool IsActive() const noexcept { return mActive; }

    /** @brief Awake transform in GameObject.
     */
    void AwakeTransform();
	/** @brief Start transform in GameObject.
	 */
	void StartTransform();
    /** @brief Retrieve the transform from GameObject.
     * @return Requested get transform value or reference.
     */
    inline Transform* GetTransform() const noexcept { return mTransform; }

    /** @brief Retrieve the id from GameObject.
     * @return Requested get id value or reference.
     */
    inline EntityID GetID() const noexcept { return id; }
    /** @brief Retrieve the scene from GameObject.
     * @return Requested get scene value or reference.
     */
    inline Scene* GetScene() const noexcept { return mScene; }
    /** @brief Retrieve the name from GameObject.
     * @return Requested get name value or reference.
     */
    inline std::string GetName() const noexcept { return mName; }
    /** @brief Set name in GameObject.
     * @param name Name of the resource, property or element to access.
     */
    void SetName(std::string name) noexcept;
    /** @brief Retrieve the tag from GameObject.
     * @return Requested get tag value or reference.
     */
    inline std::string GetTag() const noexcept { return mTag; }
    /** @brief Set tag in GameObject.
     * @param tag Tag supplied to GameObject.
     */
    inline void SetTag(std::string tag) noexcept { mTag = tag; }
    /** @brief Retrieve the parent from GameObject.
     * @return Requested get parent value or reference.
     */
    inline GameObject* GetParent() const noexcept { return mParent; }

    /** @brief Stored name value used by GameObject.
     */
    std::string mName;
    /** @brief Stored tag value used by GameObject.
     */
    std::string mTag;

    /** @brief Stored parent value used by GameObject.
     */
    GameObject* mParent = nullptr;
    /** @brief Stored children value used by GameObject.
     */
    std::vector<GameObject*> mChildren;

    /** @brief Configured id value used by GameObject.
     */
    EntityID id = 0;
    /** @brief Stored scene value used by GameObject.
     */
    Scene* mScene = nullptr;
    /** @brief Configured components value used by GameObject.
     */
    std::vector<std::unique_ptr<Component>> components;
    /** @brief Stored transform value used by GameObject.
     */
    Transform* mTransform;

    /** @brief Stored active value used by GameObject.
     */
    bool mActive = true;
    /** @brief Stored parent active value used by GameObject.
     */
    bool mParentActive = true;
    /** @brief Stored awoken value used by GameObject.
     */
    bool mAwoken = false;
    /** @brief Stored started value used by GameObject.
     */
    bool mStarted = false;

public:
    /** @brief Constructs a GameObject instance.
     */
    GameObject() noexcept;

    // Change parent. If keepWorld == true, convert local coordinates to preserve the world pose.
    /** @brief Reparents this object; optionally preserves its world-space pose.
     * @param newParent Replacement parent object, or null to detach.
     * @param keepWorld Preserve the current world transform while changing parent.
     * @note Self-parenting, ancestor cycles and cross-scene reparenting are
     *       rejected without changing the existing hierarchy.
     */
    void SetParent(GameObject* newParent, bool keepWorld = true) noexcept;

    // Enable/Disable generally propagates to children (optional)
    /** @brief Enables/disables the entity and its effective hierarchy callbacks.
     * @param a Desired local active state. An inactive ancestor still disables it.
     */
    void SetActive(bool a) noexcept;
    /** @brief Queues scene-owned entity destruction for a later safe flush.
     * @warning Do not use this on a standalone GameObject with no owning Scene.
     *          All descendant objects are also removed at flush time.
     */
    void Destroy() noexcept;

    template <class T, class... Args>
    /** @brief Constructs and owns a component of type T.
     * @tparam T Concrete Component subclass; unique types return an existing instance.
     * @param args Constructor arguments forwarded to T.
     * @return Non-owning pointer, invalid after object/scene destruction.
     * @note A component added after Awake/Start may have its lifecycle hooks run immediately.
     * @tparam Args Template type or value used by AddComponent.
     */
    T* AddComponent(Args &&...args);

    template <class T>
    /** @brief Finds the first component compatible with T, or returns nullptr.
     * @return Requested get component value or reference.
     */
    T* GetComponent() noexcept;

    template<class T>
    /** @brief Returns all components compatible with T as non-owning pointers.
     * @return All components compatible with t as non-owning pointers.
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
    /** @brief Read-only scene-assigned entity ID, unique within the owning scene.
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
	/** @brief Display/debug name kept in sync with Scene::Find(name).
	 * @note Names must be unique within a scene; renaming to an occupied name
	 *       is ignored to preserve the lookup invariant.
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

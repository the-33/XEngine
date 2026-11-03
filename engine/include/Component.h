#pragma once
/** @file Component.h
 * @brief Base class and lifetime hooks for components attached to entities.
 */

#include <vector>
#include <memory>
#include <unordered_map>

#include "GameObject.h"

/** @brief Base class for scene-owned components attached to a GameObject.
 *
 * Awake is called once when initialized, Start once before first use, then
 * FixedUpdate/Update/LateUpdate/Render while the owner participates in the engine loop.
 * `gameObject` is a non-owning property; it is null before attachment.
 * @ingroup xengine_core
 */
class Component 
{
	/** @brief Grant GameObject access to the internal state of Component.
	 */
	friend class GameObject;

protected:
    /** @cond XENGINE_INTERNAL */
    /** @brief Internal flag recording whether Awake has run.
     */
    bool mAwoken = false;
    /** @brief Internal flag recording whether Start has run.
     */
    bool mStarted = false;

    /** @brief Called at the start of the component lifecycle.
     */
    void Start_()
    {
        Start();
        mStarted = true;
    }

    /** @brief Called once when the component is attached and awakened.
     */
    void Awake_()
    {
        Awake();
        mAwoken = true;
    }

    /** @brief Internal non-owning pointer to the owning GameObject.
     */
    GameObject* mGameObject = nullptr;
    /** @brief Whether a GameObject permits only one component of this type.
     */
    static constexpr bool kUnique = false;               // NOT unique by default

    /** @brief Returns the game object of Component.
     * @return The game object of component.
     */
    inline GameObject* GetGameObject() const noexcept { return mGameObject; }

    /** @brief Lifecycle callback invoked for added to game object.
     * @param sceneAwoken Whether the owning scene has completed its Awake phase.
     * @param sceneStarted Whether the owning scene has completed its Start phase.
     */
    virtual void OnAddedToGameObject(bool sceneAwoken, bool sceneStarted)
    {
        if (sceneAwoken && !mAwoken) Awake_();
        if (sceneStarted && !mStarted) Start_();
    }
    /** @endcond */

public:
    /** @brief Constructs a Component instance.
     */
    Component() = default;
    /** @brief Releases the Component instance and any owned resources.
     */
    virtual ~Component() = default;

    /** @brief Implements operator= for this value or property type.
     * @param o Object or owner associated with this operation.
     * @return Result of applying the overloaded operator.
     */
    Component& operator=(Component&& o) noexcept
    {
        if (this != &o)
        {
            mAwoken = o.mAwoken;
            mStarted = o.mStarted;
        }
        return *this;
    }

    /** @brief Reports whether unique is currently true.
     * @return True when is unique; otherwise false.
     */
    virtual bool IsUnique() const noexcept { return kUnique; }

    // Lifecycle (Unity-like)
    /** @brief Lifecycle hook for awake; the engine controls invocation order.
     */
    virtual void Awake() {}
    /** @brief Lifecycle hook for start; the engine controls invocation order.
     */
    virtual void Start() {}
    /** @brief Called on scene/object destruction for component cleanup.
     */
    virtual void OnDestroy() {}

    // Bucle
    /** @brief Called during fixed simulation steps with the fixed timestep in seconds.
     * @param dt Elapsed frame or fixed-step duration, in seconds.
     */
    virtual void FixedUpdate(float dt) {}
    /** @brief Called once per frame after fixed updates; argument is scaled elapsed seconds.
     * @param dt Elapsed frame or fixed-step duration, in seconds.
     */
    virtual void Update(float dt) {}
    /** @brief Called after every component Update and before rendering.
     * @param dt Elapsed frame or fixed-step duration, in seconds.
     */
    virtual void LateUpdate(float dt) {}
    /** @brief Lifecycle hook for render; the engine controls invocation order.
     */
    virtual void Render() {}

	/** @cond XENGINE_INTERNAL */
	/** @brief Property accessor type for `game object`; use the corresponding instance member in gameplay code.
	 */
	using GameObjectProp = PropertyRO<Component, GameObject*, &Component::GetGameObject>;
	/** @endcond */
	/** @brief Non-owning pointer to the GameObject that owns this component.
	 */
	GameObjectProp gameObject{ this };
};

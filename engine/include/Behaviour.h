#pragma once
/** @file Behaviour.h
 * @brief Game-script lifecycle, enabled state and collision callbacks.
 */

#include <memory>
#include <type_traits>

#include "Component.h"
#include "Property.h"

class Transform;
class Scene;
struct CollisionInfo2D;

/** @brief Derive gameplay scripts from this class and override the supported lifecycle hooks.
 * @ingroup xengine_core
 */
class Behaviour : public Component
{
    friend class GameObject;
    friend class Component;

private:
    bool IsEnabled() const noexcept { return mEnabled; }

    void SetEnabled(bool e) noexcept
    {
        if (e == mEnabled) return;
        mEnabled = e;

        // Do not fire callbacks before Awake has been called.
        if (!mAwoken) return;

        if (mEnabled)
        {
            if (!mStarted) Start_();
            if (mGameObject && mGameObject->activeInHierarchy) OnEnable();
        }
        else
        {
            OnDisable();
        }
    }

    Transform* GetTransform() const noexcept;
    Scene* GetScene() const noexcept;

    void OnAddedToGameObject(bool sceneAwoken, bool sceneStarted) override
    {
        if (sceneAwoken && !mAwoken) Awake_();
        if (sceneStarted && mEnabled) {
            if (!mStarted) Start_();
            if (mGameObject && mGameObject->activeInHierarchy) OnEnable();
        }
    }

protected:
    /** @brief Internal enabled state; use the public `enabled` property in gameplay code.
     */
    bool mEnabled = true;

    /** @brief Constructs a Behaviour instance.
     */
    Behaviour() = default;

    // Hooks that scripts can override
    /** @brief Lifecycle callback invoked for disable.
     */
    virtual void OnDisable() {}
    /** @brief Lifecycle callback invoked for enable.
     */
    virtual void OnEnable() {}

    /** @brief Lifecycle callback invoked for collision enter.
     */
    virtual void OnCollisionEnter(const CollisionInfo2D&) {}
    /** @brief Lifecycle callback invoked for collision stay.
     */
    virtual void OnCollisionStay(const CollisionInfo2D&) {}
    /** @brief Lifecycle callback invoked for collision exit.
     */
    virtual void OnCollisionExit(const CollisionInfo2D&) {}

    /** @brief Lifecycle callback invoked for trigger enter.
     */
    virtual void OnTriggerEnter(const CollisionInfo2D&) {}
    /** @brief Lifecycle callback invoked for trigger stay.
     */
    virtual void OnTriggerStay(const CollisionInfo2D&) {}
    /** @brief Lifecycle callback invoked for trigger exit.
     */
    virtual void OnTriggerExit(const CollisionInfo2D&) {}

public:
    /** @brief Releases the Behaviour instance and any owned resources.
     */
    virtual ~Behaviour() = default;

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `enabled`; use the corresponding instance member in gameplay code.
     */
    using EnabledProp = Property<Behaviour, bool,
        &Behaviour::IsEnabled,
        &Behaviour::SetEnabled>;
    /** @endcond */

    /** @brief Enable or disable lifecycle updates and gameplay callbacks.
     */
    EnabledProp enabled{ this };

    /** @cond XENGINE_INTERNAL */
    /** @brief Property accessor type for `transform`; use the corresponding instance member in gameplay code.
     */
    using TransformProp = PropertyRO<Behaviour, Transform*, &Behaviour::GetTransform>;
    /** @endcond */
    /** @brief Non-owning convenience accessor to the owning object’s Transform.
     */
    TransformProp transform{ this };

	/** @cond XENGINE_INTERNAL */
	/** @brief Property accessor type for `scene`; use the corresponding instance member in gameplay code.
	 */
	using SceneProp = PropertyRO<Behaviour, Scene*, &Behaviour::GetScene>;
	/** @endcond */
	/** @brief Non-owning convenience accessor to the owner’s active Scene.
	 */
	SceneProp scene{ this };
};

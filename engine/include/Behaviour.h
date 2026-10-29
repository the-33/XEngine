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

/** @brief Gameplay-script component with enable/disable and collision callbacks.
 *
 * Use `gameObject`, `transform` and `scene` for non-owning access to its owner.
 * `enabled` controls lifecycle updates and callbacks; activation of the
 * GameObject hierarchy is an additional independent condition.
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
    /** @brief Called when an initialized enabled behaviour is deactivated.
     */
    virtual void OnDisable() {}
    /** @brief Called when an initialized behaviour becomes enabled in an active hierarchy.
     */
    virtual void OnEnable() {}

    /** @brief Called on the first detected non-trigger contact with another collider.
     */
    virtual void OnCollisionEnter(const CollisionInfo2D&) {}
    /** @brief Called while a non-trigger collider pair continues to overlap.
     */
    virtual void OnCollisionStay(const CollisionInfo2D&) {}
    /** @brief Called after a previously detected non-trigger contact ends.
     */
    virtual void OnCollisionExit(const CollisionInfo2D&) {}

    /** @brief Called when a pair involving a trigger first overlaps.
     */
    virtual void OnTriggerEnter(const CollisionInfo2D&) {}
    /** @brief Called while a trigger pair continues to overlap.
     */
    virtual void OnTriggerStay(const CollisionInfo2D&) {}
    /** @brief Called after a trigger overlap ends.
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

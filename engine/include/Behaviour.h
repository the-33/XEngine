#pragma once

#include <memory>
#include <type_traits>

#include "Component.h"
#include "Property.h"

class Transform;
class Scene;
struct CollisionInfo2D;

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
    bool mEnabled = true;

    Behaviour() = default;

    // Hooks that scripts can override
    virtual void OnDisable() {}
    virtual void OnEnable() {}

    virtual void OnCollisionEnter(const CollisionInfo2D&) {}
    virtual void OnCollisionStay(const CollisionInfo2D&) {}
    virtual void OnCollisionExit(const CollisionInfo2D&) {}

    virtual void OnTriggerEnter(const CollisionInfo2D&) {}
    virtual void OnTriggerStay(const CollisionInfo2D&) {}
    virtual void OnTriggerExit(const CollisionInfo2D&) {}

public:
    virtual ~Behaviour() = default;

    using EnabledProp = Property<Behaviour, bool,
        &Behaviour::IsEnabled,
        &Behaviour::SetEnabled>;

    EnabledProp enabled{ this };

    using TransformProp = PropertyRO<Behaviour, Transform*, &Behaviour::GetTransform>;
    TransformProp transform{ this };

	using SceneProp = PropertyRO<Behaviour, Scene*, &Behaviour::GetScene>;
	SceneProp scene{ this };
};

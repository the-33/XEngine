#pragma once

#include <vector>
#include <memory>
#include <unordered_map>

#include "GameObject.h"

class Component 
{
	friend class GameObject;

protected:
    bool mAwoken = false;
    bool mStarted = false;

    void Start_()
    {
        Start();
        mStarted = true;
    }

    void Awake_()
    {
        Awake();
        mAwoken = true;
    }

    GameObject* mGameObject = nullptr;
    static constexpr bool kUnique = false;               // por defecto NO único

    inline GameObject* GetGameObject() const noexcept { return mGameObject; }

    virtual void OnAddedToGameObject(bool sceneAwoken, bool sceneStarted)
    {
        if (sceneAwoken)  Awake();
        if (sceneStarted) Start_();
    }

public:
    Component() = default;
    virtual ~Component() = default;

    Component& operator=(Component&& o) noexcept
    {
        if (this != &o)
        {
            mAwoken = o.mAwoken;
            mStarted = o.mStarted;
        }
        return *this;
    }

    virtual bool IsUnique() const noexcept { return kUnique; }

    // Ciclo de vida (Unity-like)
    virtual void Awake() {}
    virtual void Start() {}
    virtual void OnDestroy() {}

    // Bucle
    virtual void FixedUpdate(float) {}
    virtual void Update(float) {}
    virtual void Render() {}

	using GameObjectProp = PropertyRO<Component, GameObject*, &Component::GetGameObject>;
	GameObjectProp gameObject{ this };
};
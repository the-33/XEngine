#include "GameObject.h"

#include <typeinfo>
#include <algorithm>

#include "Transform.h"
#include "Collider2D.h"
#include "SpriteRenderer.h"
#include "ErrorHandler.h"
#include "RigidBody2D.h"

GameObject::GameObject(const std::string& name_, EntityID id_, Scene* scene_) noexcept
    : mName(name_)
    , id(id_)
    , mScene(scene_)
    , mTransform(nullptr)
{
    // Ensure one unique Transform per entity
    mTransform = AddComponent<Transform>();
}

GameObject::GameObject() noexcept
    : mName("")
    , id(0)
    , mScene(nullptr)
    , mTransform(nullptr)
{
    mTransform = AddComponent<Transform>();
}

std::unique_ptr<GameObject> GameObject::Create(const std::string& name, EntityID id, Scene* scene)
{
    return std::unique_ptr<GameObject>(new GameObject(name, id, scene));
}

void GameObject::SetParent(GameObject* newParent, bool keepWorld) noexcept
{
    if (newParent == mParent) return;

    // Save world pose if it needs to be preserved
    Vec3 wpos{}, wrot{};
    Vec2 wscl{ 1.f, 1.f };
    if (keepWorld && mTransform) {
        wpos = mTransform->position;   // Property: world position
        wrot = mTransform->rotation;   // Property: world rotation
        wscl = mTransform->scale;      // Property: world scale
    }

    // remove from previous parent
    if (mParent) {
        mParent->RemoveChild(this);
    }

    // assign new parent
    mParent = newParent;
    if (mParent) {
        mParent->AddChild(this);

        const bool wasActive = (mActive && mParentActive);
        mParentActive = (mParent ? (mParent->activeInHierarchy) : true);
        const bool isActiveNow = (mActive && mParentActive);

        if (!wasActive && isActiveNow && mAwoken && mStarted) OnEnable();
        else if (wasActive && !isActiveNow)                   OnDisable();
    }

    // ajusta pose
    if (mTransform) {
        if (keepWorld) {
            // Reapply world transform; Transform calculates local coordinates relative to the new parent
            mTransform->position = wpos;
            mTransform->rotation = wrot;
            mTransform->scale = wscl;
        }
        else {
            // Keep local transforms; world transforms will be recalculated
            mTransform->MarkDirty();
        }
    }
}

void GameObject::AddChild(GameObject* child) noexcept
{
    if (!child || child == this) return;
    // evita duplicados
    if (std::find(mChildren.begin(), mChildren.end(), child) == mChildren.end())
    {
        mChildren.push_back(child);
        child->mParent = this;
        child->mParentActive = mActive && mParentActive;
    }
}

void GameObject::RemoveChild(GameObject* child) noexcept
{
    if (!child) return;
    auto it = std::find(mChildren.begin(), mChildren.end(), child);
    if (it != mChildren.end()) {
        (*it)->mParent = nullptr;   // Break the parent link
        (*it)->mParentActive = true;
        mChildren.erase(it);
    }
}

void GameObject::AwakeTransform()
{
	mTransform->Awake();
}

void GameObject::StartTransform()
{
	mTransform->Start();
}

void GameObject::SetActive(bool a) noexcept
{
    if (mActive == a) return;
    mActive = a;
    if (!mAwoken) return;

    if (mParentActive)
    {
        if (mActive)
        {
            OnEnable();
			if (!mStarted) Start();
        }
        else OnDisable();
    }
}

void GameObject::Destroy() noexcept
{
    mScene->DestroyObject(id);
    mActive = false;
}

void GameObject::Awake()
{
    if (mAwoken) return;
    mAwoken = true;

    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get()))
        {
            c->Awake_();
        }

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
                c->Awake_();
        }
    }
}

void GameObject::Start()
{
    if (mStarted) return;
    mStarted = true;

    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get()))
        {
            c->Start_();
        }

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
            if (b->enabled)
                c->Start_(); 
        }
    }
}

void GameObject::OnEnable()
{
    if (!mAwoken || !mStarted || !mActive || !mParentActive) return;
    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get())) continue;

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
            if (b->mAwoken && b->mStarted && b->enabled)
                b->OnEnable();
        }
    }

    for (auto* ch : mChildren)
        if (ch)
        {
            ch->mParentActive = true;
            //  Do NOT call OnEnable before the child has been initialized
            if (ch->mAwoken && ch->mStarted && ch->mActive)
                ch->OnEnable();
        }
}

void GameObject::OnDisable()
{
    if (!mAwoken || !mStarted) return;
    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get())) continue;

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
            if (b->mAwoken && b->mStarted && b->enabled)
                b->OnDisable();
        }
    }

    for (auto* ch : mChildren) 
        if (ch)
        {
            ch->mParentActive = false;
            if (ch->mAwoken && ch->mStarted && ch->mActive)
                ch->OnDisable();
        }
}

void GameObject::OnDestroy()
{
    for (auto& c : components) if (c) {
        c->OnDestroy();   // desregistro, parar sonidos, etc.
    }

    if (mParent) mParent->RemoveChild(this);
    for (auto* ch : mChildren) if (ch) ch->mParent = nullptr;
    mChildren.clear();
}

void GameObject::Register()
{
    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get())) continue;

        if (auto* col = dynamic_cast<Collider2D*>(c.get())) {
                col->Register();
        }

        if (auto* rb = dynamic_cast<RigidBody2D*>(c.get())) {
            rb->Register();
        }
    }
}

void GameObject::FixedUpdate(float dt)
{
    if (!mAwoken || !mStarted) return;
    if (!mActive || !mParentActive) return;

    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get())) continue;

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
            if (b->mAwoken && b->mStarted && b->enabled)
                b->FixedUpdate(dt);
        }
    }
}

void GameObject::Update(float dt)
{
    if (!mAwoken || !mStarted) return;
    if (!mActive || !mParentActive) return;

    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get())) continue;

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
            if (b->mAwoken && b->mStarted && b->enabled)
                b->Update(dt);
        }
    }
}

void GameObject::Render()
{
    if (!mAwoken || !mStarted) return;
    if (!mActive || !mParentActive) return;

    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get())) continue;

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
            if (b->mAwoken && b->mStarted && b->enabled)
                b->Render();
        }
    }
}

void GameObject::OnCollisionEnter(const CollisionInfo2D& info)
{
    if (!mAwoken || !mStarted) return;
    if (!mActive || !mParentActive) return;

    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get())) continue;

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
            if (b->mAwoken && b->mStarted && b->enabled)
                b->OnCollisionEnter(info);
        }
    }
}

void GameObject::OnCollisionStay(const CollisionInfo2D& info)
{
    if (!mAwoken || !mStarted) return;
    if (!mActive || !mParentActive) return;

    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get())) continue;

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
            if (b->mAwoken && b->mStarted && b->enabled)
                b->OnCollisionStay(info);
        }
    }
}

void GameObject::OnCollisionExit(const CollisionInfo2D& info)
{
    if (!mAwoken || !mStarted) return;
    if (!mActive || !mParentActive) return;

    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get())) continue;

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
            if (b->mAwoken && b->mStarted && b->enabled)
                b->OnCollisionExit(info);
        }
    }
}

void GameObject::OnTriggerEnter(const CollisionInfo2D& info)
{
    if (!mAwoken || !mStarted) return;
    if (!mActive || !mParentActive) return;

    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get())) continue;

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
            if (b->mAwoken && b->mStarted && b->enabled)
                b->OnTriggerEnter(info);
        }
    }
}

void GameObject::OnTriggerStay(const CollisionInfo2D& info)
{
    if (!mAwoken || !mStarted) return;
    if (!mActive || !mParentActive) return;

    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get())) continue;

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
            if (b->mAwoken && b->mStarted && b->enabled)
                b->OnTriggerStay(info);
        }
    }
}

void GameObject::OnTriggerExit(const CollisionInfo2D& info)
{
    if (!mAwoken || !mStarted) return;
    if (!mActive || !mParentActive) return;

    for (auto& c : components)
    {
        if (!c) continue;
        if (dynamic_cast<Transform*>(c.get())) continue;

        if (auto* b = dynamic_cast<Behaviour*>(c.get())) {
            if (b->mAwoken && b->mStarted && b->enabled)
                b->OnTriggerExit(info);
        }
    }
}


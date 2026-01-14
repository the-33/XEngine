#include "SceneManager.h"

#include "AssetManager.h"
#include "RenderManager.h"
#include "SoundManager.h"
#include "CollisionManager.h"
#include "PhysicsManager.h"
#include "Scene.h"

bool SceneManager::Init() noexcept
{
    mAssets = AssetManager::GetInstancePtr();
    mRender = RenderManager::GetInstancePtr();
    mSound = SoundManager::GetInstancePtr();
    mCollision = CollisionManager::GetInstancePtr();

	if (mAssets == nullptr) return false;
	if (mRender == nullptr) return false;
	if (mSound == nullptr) return false;
	if (mCollision == nullptr) return false;

    mRegistry.clear();
    mActive.reset();
    mActiveId.clear();
    mUnnamedCounter = 0;

    return (mAssets && mRender && mSound && mCollision);
}

void SceneManager::Shutdown() noexcept
{
    // Desactivar y destruir activa
    if (mActive) {
        mActive->OnDisableAll();
        mActive->OnDestroyAll();
        mActive.reset();
    }
    mActiveId.clear();

    // Destruir todas las cargadas
    for (auto& kv : mRegistry) {
        if (kv.second.instance) {
            kv.second.instance->OnDestroyAll();
        }
    }
    mRegistry.clear();

    mAssets = nullptr; mRender = nullptr; mSound = nullptr; mCollision = nullptr;
}

std::unique_ptr<Scene> SceneManager::NewEmptyScene()
{
	return Scene::Create();
}

void SceneManager::SetActive(std::unique_ptr<Scene> s) noexcept {
    mPendingScene = std::move(s);
    mHasPending = true;
}

bool SceneManager::SetActive(const std::string& id) noexcept
{
    if (mRegistry.find(id) == mRegistry.end()) return false;
    mPendingSceneId = id;
    mPendingScene.reset();
    mHasPending = true;
    return true;
}

bool SceneManager::HasPendingScene() const noexcept { return mHasPending; }

void SceneManager::Register(const std::string& id, SceneBuilder builder) noexcept
{
    mRegistry[id.empty() ? "unnamed_" + std::to_string(++mUnnamedCounter) : id] = Entry{std::move(builder), false, nullptr};
}

void SceneManager::SetPersistent(const std::string& id, bool persistent) noexcept
{
    auto it = mRegistry.find(id);
    if (it == mRegistry.end()) return;

    it->second.persistent = persistent;
    if (!persistent)
        it->second.instance.reset(); // opcional: al desmarcar, reset
}

void SceneManager::ApplyPendingScene() noexcept
{
    if (!mHasPending) return;

    // --- 1) Desactivar actual ---
    if (mActive) mActive->OnDisableAll();

    const bool hadActive = (mActive != nullptr);
    const bool activeWasPersistent =
        (!mActiveId.empty() &&
            (mRegistry.find(mActiveId) != mRegistry.end()) &&
            mRegistry[mActiveId].persistent);

    if (hadActive && !activeWasPersistent)
    {
        mActive->OnDestroyAll();
    }

    CollisionManager::GetInstancePtr()->ClearAll();
    PhysicsManager::GetInstancePtr()->mBodies.clear();

    // Si la activa era persistente, devuélvela al registry (para reusarla)
    if (!mActiveId.empty())
    {
        auto itOld = mRegistry.find(mActiveId);
        if (itOld != mRegistry.end() && itOld->second.persistent)
        {
            itOld->second.instance = std::move(mActive);
        }
    }

    mActive.reset();
    mActiveId.clear();

    // --- 2) Seleccionar nueva ---
    if (mPendingScene)
    {
        mActive = std::move(mPendingScene);
        mActiveId = "unnamed_" + std::to_string(++mUnnamedCounter);
    }
    else
    {
        auto it = mRegistry.find(mPendingSceneId);
        if (it == mRegistry.end())
        {
            mHasPending = false;
            mPendingSceneId.clear();
            return;
        }

        Entry& e = it->second;
        auto newScene = NewEmptyScene();

        if (e.persistent)
        {
            if (!e.instance)
            {
                e.builder(newScene.get());
				e.instance = std::move(newScene);
            }
            mActive = std::move(e.instance);
        }
        else
        {
            e.builder(newScene.get());
			mActive = std::move(newScene);
        }

        mActiveId = mPendingSceneId;
    }

    mHasPending = false;
    mPendingSceneId.clear();

    // --- 3) Arranque limpio ---
    if (mActive)
    {
        mActive->OnEnableAll();

        if (!mActive->mStarted)
        {
            mActive->AwakeAll();
            mActive->StartAll();
            mActive->mStarted = true;
        }
        else
        {
            mActive->Register();
        }
    }
}

void SceneManager::FixedUpdate(float fixedDt)
{
    if (!mActive) return;
    mActive->FixedUpdate(fixedDt);
}

void SceneManager::Update(float dt)
{
    if (!mActive) return;
    mActive->Update(dt);
}

void SceneManager::Render()
{
    if (!mActive) return;
    mActive->Render();
}

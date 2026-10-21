#include "Scene.h"

#include <algorithm>

#include "WindowManager.h"
#include "AssetManager.h"
#include "RenderManager.h"
#include "SoundManager.h"
#include "CollisionManager.h"
#include "Camera2D.h"
#include "Transform.h"
#include "GameObject.h"

Scene::Scene() noexcept
{
    mAssets = AssetManager::GetInstancePtr();
    mRender = RenderManager::GetInstancePtr();
    mSound = SoundManager::GetInstancePtr();
    mCollision = CollisionManager::GetInstancePtr();

    auto fb = WindowManager::GetInstance().GetDrawableSize(); // px
    float aspect = (fb.y > 0 ? (float)fb.x / (float)fb.y : 16.f / 9.f);

    const float baseWorldWidth = 20.f;          // visible width along X at zoom 1
    const float baseWorldHeight = baseWorldWidth / aspect;

    // Camera2D has a private constructor: Scene is explicitly its friend.
    // std::make_unique cannot call it because instantiation happens in std.
    mCamera.reset(new Camera2D());

    mCamera->viewBase = {baseWorldWidth, baseWorldHeight};
    mCamera->center = { 0, 0 };
    mCamera->zoom = 1.f;

    mNew.clear();
    mUninitialized.clear();
}

std::unique_ptr<Scene> Scene::Create()
{
    return std::unique_ptr<Scene>(new Scene());
}

Scene::~Scene() noexcept
{
    if (mDestroyed) return;
    OnDisableAll();
    OnDestroyAll();
}

GameObject* Scene::CreateObject(std::string name, GameObject* parent)
{
    const std::string base = name;

    // monotonic counter per base name
    int& serial = repeatedNamesCount[base];      // initialize to 0 if absent
    std::string safe = (serial == 0) ? base      // primera vez -> "name"
        : base + std::to_string(serial); // -> "name1", "name2", ...

    // if the name already exists in the scene (loading, external duplicate, etc.),
    // advance until an unused name is found
    while (mByName.count(safe)) {
        safe = base + std::to_string(++serial);
    }
    serial++; // preparamos el siguiente

    const EntityID id = mNextID++;
	auto go = GameObject::Create(safe, id, this);
    GameObject* raw = go.get();

    if (parent) raw->SetParent(parent);

    mEntities.emplace_back(std::move(go));
    mById[id] = raw;
    mByName[raw->GetName()] = raw;

    mNew.push_back(raw);

    return raw;
}

GameObject* Scene::Instantiate(const std::string& name, InstanceBuilder build,
    Vec3 position, Vec3 rotation, GameObject* parent)
{
    GameObject* go = CreateObject(name, nullptr);

    if (parent) go->SetParent(parent);

    if (build) build(*go, *this);   // -> components added here

    if (auto* t = go->GetComponent<Transform>()) {
        t->position = position;
        t->rotation = rotation;
    }

    return go;
}

void Scene::CollectDescendants_(GameObject* root, std::vector<EntityID>& out)
{
    if (!root) return;
    for (auto* ch : root->Children()) {
        if (!ch) continue;
        out.push_back(ch->GetID());
        CollectDescendants_(ch, out);
    }
}

void Scene::DestroyObject(EntityID id)
{
    // Queue for safe execution after Fixed/Update/Render
    mDestroyQueue.emplace_back(id);
}

// ===== Global lifecycle =====
void Scene::AwakeAll()
{
    std::vector<GameObject*> list;
    list.reserve(mEntities.size());
    for (auto& e : mEntities)
        if (e) list.push_back(e.get());

    for (auto* go : list)
        if (go) go->Awake();
}

void Scene::StartAll()
{
    std::vector<GameObject*> list;
    list.reserve(mEntities.size());
    for (auto& e : mEntities)
        if (e) list.push_back(e.get());

    for (auto* go : list)
        if (go) go->Start();
}

void Scene::OnEnableAll()
{
    std::vector<GameObject*> list;
    list.reserve(mEntities.size());
    for (auto& e : mEntities)
        if (e) list.push_back(e.get());

    // GameObject::OnEnable recursively visits descendants. Visit only roots
    // to avoid duplicate activation callbacks on child objects.
    for (auto* go : list)
        if (go && !go->GetParent() && go->activeInHierarchy) go->OnEnable();
}

void Scene::OnDisableAll()
{
    std::vector<GameObject*> list;
    list.reserve(mEntities.size());
    for (auto& e : mEntities)
        if (e) list.push_back(e.get());

    // Global deactivation must likewise visit each hierarchy once.
    for (auto* go : list)
        if (go && !go->GetParent() && go->activeInHierarchy) go->OnDisable();
}

void Scene::OnDestroyAll()
{
    if (mDestroyed) return;
    mDestroyed = true;

    std::vector<GameObject*> list;
    list.reserve(mEntities.size());
    for (auto& e : mEntities)
        if (e) list.push_back(e.get());

    for (auto* go : list)
        if (go) go->OnDestroy();

    mNew.clear();
    mUninitialized.clear();
}

void Scene::DestroyAll()
{
    // Destroy everything immediately (useful for hard resets)
    std::vector<GameObject*> list;
    list.reserve(mEntities.size());
    for (auto& e : mEntities)
        if (e) list.push_back(e.get());

    for (auto* go : list)
        if (go) go->OnDestroy();

    mEntities.clear();
    mDestroyQueue.clear();
    mById.clear();
    mByName.clear();
    mNextID = 1;
}

void Scene::Register()
{
    std::vector<GameObject*> list;
    list.reserve(mEntities.size());
    for (auto& e : mEntities)
        if (e) list.push_back(e.get());

    for (auto* go : list)
        if (go) go->Register();
}

// ===== Bucle principal =====
void Scene::FixedUpdate(float dt)
{
    std::vector<GameObject*> list;
    list.reserve(mEntities.size());
    for (auto& e : mEntities)
        if (e) list.push_back(e.get());

    for (auto* go : list)
        if (go) go->FixedUpdate(dt);
}

void Scene::Update(float dt)
{
    std::vector<GameObject*> list;
    list.reserve(mEntities.size());
    for (auto& e : mEntities)
        if (e) list.push_back(e.get());

    for (auto* go : list)
        if (go) go->Update(dt);
}

void Scene::Render()
{
    // Build a flat list of valid pointers
    std::vector<GameObject*> drawList;
    drawList.reserve(mEntities.size());
    for (auto& e : mEntities)
        if (e) drawList.push_back(e.get());

    // World Z; default to zero if there is no Transform
    auto zOf = [](const GameObject* go) noexcept -> float {
        if (!go) return 0.f;
        if (auto* t = go->mTransform) {
            const Vec3 wp = t->position;   // Property -> Vec3f (world)
            return wp.z;
        }
        return 0.f;
        };

    // Stable order: preserve the previous order for Z ties
    std::stable_sort(drawList.begin(), drawList.end(),
        [&](GameObject* a, GameObject* b) {
            return zOf(a) < zOf(b);
        });

    // Render in order (higher Z is drawn later and appears on top)
    for (auto* e : drawList)
        e->Render();

    RenderManager::GetInstance().FlushDebug();
}

// ===== Utilidades =====n
GameObject* Scene::Find(EntityID id) noexcept
{
    auto it = mById.find(id);
    return (it != mById.end() ? it->second : nullptr);
}

GameObject* Scene::Find(const std::string& name) noexcept
{
    auto it = mByName.find(name);
    return (it != mByName.end() ? it->second : nullptr);
}

// ===== Deferred destruction =====
void Scene::FlushDestroyQueue()
{
    if (mDestroyQueue.empty()) return;

    // 1) Collect ALL IDs to remove (root and descendants)
    std::vector<EntityID> toDelete;
    toDelete.reserve(mDestroyQueue.size() * 4); // heuristic

    for (EntityID id : mDestroyQueue) {
        auto it = mById.find(id);
        if (it == mById.end()) continue;
        GameObject* root = it->second;
        toDelete.push_back(id);
        CollectDescendants_(root, toDelete);
    }

    // 2) Convert to a set for O(1) deletion and deduplication
    std::unordered_set<EntityID> delSet(toDelete.begin(), toDelete.end());

    // 2.5) If entries are in mNew or mPending, remove them to avoid dangling pointers
    auto purgePtrList = [&](std::vector<GameObject*>& v)
        {
            v.erase(std::remove_if(v.begin(), v.end(),
                [&](GameObject* p) {
                    return !p || (delSet.count(p->GetID()) != 0);
                }),
                v.end());
        };

    purgePtrList(mNew);
    purgePtrList(mUninitialized);   // adjust the purgePtrList call if the queue name changes

    // 3) Remove from mEntities (back to front, stable and O(n))
    for (int i = static_cast<int>(mEntities.size()) - 1; i >= 0; --i) {
        GameObject* go = mEntities[i].get();
        if (!go) continue;

        if (delSet.count(go->GetID()) == 0)
            continue;

        // 3.1) Object OnDestroy callback
        go->OnDestroy();

        // 3.2) Clear indices
        mByName.erase(go->GetName());
        mById.erase(go->GetID());

        // 3.4) Finally remove from the vector (unique_ptr releases memory)
        mEntities.erase(mEntities.begin() + i);
    }

    mDestroyQueue.clear();
}


void Scene::ProcessNewObjects()
{
    if (mNew.empty() && mUninitialized.empty()) return;

    for (auto* go : mNew) mUninitialized.push_back(go);
    mNew.clear();

    // temporary vector for objects not yet active
    std::vector<GameObject*> pending;
    pending.clear();
    pending.reserve(mUninitialized.size());

    for (auto* go : mUninitialized)
    {
        if (go == nullptr) continue;

        // Check whether active in the hierarchy
        const bool activeInHierarchy =
            (go->Parent() != nullptr
                ? (go->Parent()->activeSelf && go->Parent()->IsActiveInHierarchy()) // caso padre anidado
                : true)
            && go->activeSelf;

        if (activeInHierarchy)
        {
            go->Awake();     // only acts if not already initialized
            go->Start();     // idem
            go->OnEnable();  // propaga a componentes e hijos
        }
        else
        {
            // Not active yet: keep pending and retry later
            pending.push_back(go);
        }
    }

    // Replace the queue with pending entries
    mUninitialized.swap(pending);
}

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

    const float baseWorldWidth = 20.f;          // lo que "cabe" en X a zoom 1
    const float baseWorldHeight = baseWorldWidth / aspect;

    mCamera = new Camera2D();

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

    // contador monotónico por "base"
    int& serial = repeatedNamesCount[base];      // crea con 0 si no existe
    std::string safe = (serial == 0) ? base      // primera vez -> "name"
        : base + std::to_string(serial); // -> "name1", "name2", ...

    // si por cualquier motivo ya existe en la escena (carga, duplicado externo),
    // avanzamos hasta encontrar un hueco
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

    if (build) build(*go, *this);   // -> componentes añadidos aquí

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
    // Encolamos; se ejecuta al final de Fixed/Update/Render seguros
    mDestroyQueue.emplace_back(id);
}

// ===== Ciclo de vida global =====
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

    for (auto* go : list)
        if (go) go->OnEnable();
}

void Scene::OnDisableAll()
{
    std::vector<GameObject*> list;
    list.reserve(mEntities.size());
    for (auto& e : mEntities)
        if (e) list.push_back(e.get());

    for (auto* go : list)
        if (go) go->OnDisable();
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
    // Destruir todo inmediatamente (útil para resets duros)
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
    // Construir una lista plana de punteros válidos
    std::vector<GameObject*> drawList;
    drawList.reserve(mEntities.size());
    for (auto& e : mEntities)
        if (e) drawList.push_back(e.get());

    // Z de mundo; si no hay transform por cualquier motivo, z=0
    auto zOf = [](const GameObject* go) noexcept -> float {
        if (!go) return 0.f;
        if (auto* t = go->mTransform) {
            const Vec3 wp = t->position;   // Property -> Vec3f (world)
            return wp.z;
        }
        return 0.f;
        };

    // Orden estable: preservar el orden previo para empates de Z
    std::stable_sort(drawList.begin(), drawList.end(),
        [&](GameObject* a, GameObject* b) {
            return zOf(a) < zOf(b);
        });

    // Render en orden (los Z altos se dibujan los últimos = por encima)
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

// ===== Destrucción diferida =====
void Scene::FlushDestroyQueue()
{
    if (mDestroyQueue.empty()) return;

    // 1) Recolectar TODOS los ids a eliminar (raíz + descendientes)
    std::vector<EntityID> toDelete;
    toDelete.reserve(mDestroyQueue.size() * 4); // heurística

    for (EntityID id : mDestroyQueue) {
        auto it = mById.find(id);
        if (it == mById.end()) continue;
        GameObject* root = it->second;
        toDelete.push_back(id);
        CollectDescendants_(root, toDelete);
    }

    // 2) Pasar a set para borrado O(1) y evitar duplicados
    std::unordered_set<EntityID> delSet(toDelete.begin(), toDelete.end());

    // 2.5) Si están en mNew o mPending, sacarlos para no dejar punteros colgando
    auto purgePtrList = [&](std::vector<GameObject*>& v)
        {
            v.erase(std::remove_if(v.begin(), v.end(),
                [&](GameObject* p) {
                    return !p || (delSet.count(p->GetID()) != 0);
                }),
                v.end());
        };

    purgePtrList(mNew);
    purgePtrList(mUninitialized);   // si la cola se llamara diferente, ajustar la llamada a purgePtrList

    // 3) Borrado en mEntities (de atrás hacia delante, estable y O(n))
    for (int i = static_cast<int>(mEntities.size()) - 1; i >= 0; --i) {
        GameObject* go = mEntities[i].get();
        if (!go) continue;

        if (delSet.count(go->GetID()) == 0)
            continue;

        // 3.1) OnDestroy del objeto
        go->OnDestroy();

        // 3.2) Limpiar índices
        mByName.erase(go->GetName());
        mById.erase(go->GetID());

        // 3.4) Finalmente, eliminar del vector (libera memoria por unique_ptr)
        mEntities.erase(mEntities.begin() + i);
    }

    mDestroyQueue.clear();
}


void Scene::ProcessNewObjects()
{
    if (mNew.empty() && mUninitialized.empty()) return;

    for (auto* go : mNew) mUninitialized.push_back(go);
    mNew.clear();

    // vector temporal para los que aún no están activos
    std::vector<GameObject*> pending;
    pending.clear();
    pending.reserve(mUninitialized.size());

    for (auto* go : mUninitialized)
    {
        if (go == nullptr) continue;

        // Comprobar si está activo en jerarquía
        const bool activeInHierarchy =
            (go->Parent() != nullptr
                ? (go->Parent()->activeSelf && go->Parent()->IsActiveInHierarchy()) // caso padre anidado
                : true)
            && go->activeSelf;

        if (activeInHierarchy)
        {
            go->Awake();     // solo hace algo si aún no lo está
            go->Start();     // idem
            go->OnEnable();  // propaga a componentes e hijos
        }
        else
        {
            // Aún no activo -> dejar en pendientes para intentar más tarde
            pending.push_back(go);
        }
    }

    // Sustituir la cola por los pendientes
    mUninitialized.swap(pending);
}

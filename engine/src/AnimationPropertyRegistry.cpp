#include "AnimationPropertyRegistry.h"

#include "AssetManager.h"
#include "Behaviour.h"
#include "GameObject.h"
#include "Scene.h"
#include "SpriteRenderer.h"
#include "Transform.h"

#include <unordered_map>

namespace {
struct PropertyEntry {
    std::function<AnimationValue(Component&)> read;
    std::function<void(Component&, const AnimationValue&)> write;
};
struct ComponentEntry {
    std::function<Component*(GameObject&, unsigned)> resolve;
    std::unordered_map<std::string, PropertyEntry> properties;
};

std::unordered_map<std::string, ComponentEntry>& Registry()
{
    static std::unordered_map<std::string, ComponentEntry> registrations;
    return registrations;
}

GameObject* ResolveTarget(GameObject& owner, std::string_view target)
{
    if (target.empty() || target == "Self" || target == ".") return &owner;
    Scene* scene = owner.scene;
    if (!scene) return nullptr;
    GameObject* current = &owner;
    while (!target.empty()) {
        const auto separator = target.find('/');
        const std::string name(target.substr(0, separator));
        if (name.empty()) return nullptr;
        GameObject* child = scene->Find(name);
        if (!child || static_cast<GameObject*>(child->parent) != current) return nullptr;
        current = child;
        if (separator == std::string_view::npos) break;
        target.remove_prefix(separator + 1);
    }
    return current;
}
} // namespace

bool AnimationPropertyRegistry::RegisterRaw(std::string componentName, std::string propertyName,
    Resolver resolve, Reader read, Writer write)
{
    if (componentName.empty() || propertyName.empty() || !resolve || !read || !write)
        return false;
    auto& component = Registry()[std::move(componentName)];
    if (component.properties.contains(propertyName)) return false;
    if (!component.resolve) component.resolve = std::move(resolve);
    component.properties.emplace(std::move(propertyName), PropertyEntry{std::move(read), std::move(write)});
    return true;
}

AnimationPropertyBinding AnimationPropertyRegistry::Bind(GameObject& owner,
    std::string_view target, std::string_view componentName,
    std::string_view propertyName, unsigned componentIndex)
{
    RegisterBuiltins();
    const auto entry = Registry().find(std::string(componentName));
    if (entry == Registry().end()) return {};
    const auto property = entry->second.properties.find(std::string(propertyName));
    if (property == entry->second.properties.end()) return {};

    GameObject* targetObject = ResolveTarget(owner, target);
    if (!targetObject || !entry->second.resolve(*targetObject, componentIndex)) return {};

    const auto resolver = entry->second.resolve;
    const auto reader = property->second.read;
    const auto writer = property->second.write;
    Scene* scene = targetObject->scene;
    const EntityID id = targetObject->ID;
    const bool sameObject = targetObject == &owner;
    const auto find = [scene, id, &owner, sameObject, resolver, componentIndex]() -> Component* {
        GameObject* target = scene ? scene->Find(id) : (sameObject ? &owner : nullptr);
        return target ? resolver(*target, componentIndex) : nullptr;
    };
    return {
        [find, reader]() -> AnimationValue {
            if (auto* component = find()) return reader(*component);
            return 0.f;
        },
        [find, writer](const AnimationValue& value) {
            if (auto* component = find()) writer(*component, value);
        },
        [find]() { return find() != nullptr; }
    };
}

void AnimationPropertyRegistry::RegisterBuiltins()
{
    static const bool registered = [] {
        Register<Transform, Vec3>("Transform", "localPosition",
            [](const Transform& t) { return static_cast<Vec3>(t.localPosition); },
            [](Transform& t, Vec3 value) { t.localPosition = value; });
        Register<Transform, Vec2>("Transform", "localScale",
            [](const Transform& t) { return static_cast<Vec2>(t.localScale); },
            [](Transform& t, Vec2 value) { t.localScale = value; });
        Register<Transform, Vec3>("Transform", "localRotation",
            [](const Transform& t) { return static_cast<Vec3>(t.localRotation); },
            [](Transform& t, Vec3 value) { t.localRotation = value; });
        Register<SpriteRenderer, Color>("SpriteRenderer", "tint",
            [](const SpriteRenderer& r) { return static_cast<Color>(r.tint); },
            [](SpriteRenderer& r, Color value) { r.tint = value; });
        Register<SpriteRenderer, RectI>("SpriteRenderer", "source",
            [](const SpriteRenderer& r) { return static_cast<RectI>(r.source); },
            [](SpriteRenderer& r, RectI value) { r.source = value; });
        Register<SpriteRenderer, std::string>("SpriteRenderer", "texturePath",
            [](const SpriteRenderer& r) {
                const auto* assets = AssetManager::GetInstancePtr();
                return assets ? assets->FindTexturePath(static_cast<const Texture*>(r.sprite)) : std::string{};
            },
            [](SpriteRenderer& r, const std::string& path) {
                auto* assets = AssetManager::GetInstancePtr();
                if (assets && !path.empty())
                    if (const auto* texture = assets->LoadTexture(path)) r.sprite = texture;
            });
        Register<SpriteRenderer, Vec2>("SpriteRenderer", "offset",
            [](const SpriteRenderer& r) { return static_cast<Vec2>(r.offset); },
            [](SpriteRenderer& r, Vec2 value) { r.offset = value; });
        Register<SpriteRenderer, bool>("SpriteRenderer", "flipX",
            [](const SpriteRenderer& r) { return static_cast<bool>(r.flipX); },
            [](SpriteRenderer& r, bool value) { r.flipX = value; });
        Register<SpriteRenderer, bool>("SpriteRenderer", "flipY",
            [](const SpriteRenderer& r) { return static_cast<bool>(r.flipY); },
            [](SpriteRenderer& r, bool value) { r.flipY = value; });
        Register<Behaviour, bool>("Behaviour", "enabled",
            [](const Behaviour& b) { return static_cast<bool>(b.enabled); },
            [](Behaviour& b, bool value) { b.enabled = value; });
        return true;
    }();
    (void)registered;
}

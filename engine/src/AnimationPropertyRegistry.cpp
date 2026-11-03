#include "AnimationPropertyRegistry.h"

#include "AssetManager.h"
#include "Animator.h"
#include "Behaviour.h"
#include "Collider2D.h"
#include "GameObject.h"
#include "PlayerInput.h"
#include "RigidBody2D.h"
#include "Scene.h"
#include "SpriteRenderer.h"
#include "Transform.h"

#include <bit>
#include <cstdint>
#include <unordered_map>

/** @brief Namespace collecting @007226376234012073234340277225102051130077235030 functionality for XEngine.
 */
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

template<class T>
void RegisterEnabled(const char* componentName)
{
    AnimationPropertyRegistry::Register<T, bool>(componentName, "enabled",
        [](const T& component) { return static_cast<bool>(component.enabled); },
        [](T& component, bool value) { component.enabled = value; });
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
        RegisterEnabled<SpriteRenderer>("SpriteRenderer");
        Register<Behaviour, bool>("Behaviour", "enabled",
            [](const Behaviour& b) { return static_cast<bool>(b.enabled); },
            [](Behaviour& b, bool value) { b.enabled = value; });

        // All public Collider2D configuration properties are available to
        // animation. Integer enums and layer masks use stepped Int tracks.
        Register<Collider2D, int>("Collider2D", "shape",
            [](const Collider2D& c) { return static_cast<int>(c.shape.get()); },
            [](Collider2D& c, int value) {
                if (value >= 0 && value <= static_cast<int>(Collider2D::Shape::Capsule))
                    c.shape = static_cast<Collider2D::Shape>(value);
            });
        Register<Collider2D>("Collider2D", "isTrigger", &Collider2D::isTrigger);
        Register<Collider2D, int>("Collider2D", "layer",
            [](const Collider2D& c) {
                return static_cast<int>(std::bit_cast<std::int32_t>(c.layer.get()));
            },
            [](Collider2D& c, int value) {
                c.layer = std::bit_cast<LayerBits>(static_cast<std::int32_t>(value));
            });
        Register<Collider2D, int>("Collider2D", "mask",
            [](const Collider2D& c) {
                return static_cast<int>(std::bit_cast<std::int32_t>(c.mask.get()));
            },
            [](Collider2D& c, int value) {
                c.mask = std::bit_cast<LayerBits>(static_cast<std::int32_t>(value));
            });
        Register<Collider2D>("Collider2D", "showGizmo", &Collider2D::showGizmo);
        Register<Collider2D>("Collider2D", "gizmoColor", &Collider2D::gizmoColor);
        Register<Collider2D>("Collider2D", "localOffset", &Collider2D::localOffset);
        Register<Collider2D>("Collider2D", "inheritRotation", &Collider2D::inheritRotation);
        Register<Collider2D>("Collider2D", "rotationOffset", &Collider2D::rotationOffset);
        Register<Collider2D>("Collider2D", "size", &Collider2D::size);
        Register<Collider2D>("Collider2D", "radius", &Collider2D::radius);
        RegisterEnabled<Collider2D>("Collider2D");

        Register<RigidBody2D, int>("RigidBody2D", "bodyType",
            [](const RigidBody2D& r) { return static_cast<int>(r.bodyType.get()); },
            [](RigidBody2D& r, int value) {
                if (value >= 0 && value <= static_cast<int>(RigidBody2D::BodyType::Kinematic))
                    r.bodyType = static_cast<RigidBody2D::BodyType>(value);
            });
        Register<RigidBody2D, int>("RigidBody2D", "collisionDetection",
            [](const RigidBody2D& r) { return static_cast<int>(r.collisionDetection.get()); },
            [](RigidBody2D& r, int value) {
                if (value >= 0 &&
                    value <= static_cast<int>(RigidBody2D::CollisionDetection::Continuous))
                    r.collisionDetection = static_cast<RigidBody2D::CollisionDetection>(value);
            });
        Register<RigidBody2D>("RigidBody2D", "mass", &RigidBody2D::mass);
        Register<RigidBody2D>("RigidBody2D", "inertia", &RigidBody2D::inertia);
        Register<RigidBody2D>("RigidBody2D", "gravityScale", &RigidBody2D::gravityScale);
        Register<RigidBody2D>("RigidBody2D", "linearDamping", &RigidBody2D::linearDamping);
        Register<RigidBody2D>("RigidBody2D", "angularDamping", &RigidBody2D::angularDamping);
        Register<RigidBody2D>("RigidBody2D", "restitution", &RigidBody2D::restitution);
        Register<RigidBody2D, int>("RigidBody2D", "constraints",
            [](const RigidBody2D& r) { return static_cast<int>(r.constraints.get()); },
            [](RigidBody2D& r, int value) {
                if (value >= 0 && value <= 7)
                    r.constraints = static_cast<RigidBody2D::Constraints>(value);
            });
        Register<RigidBody2D>("RigidBody2D", "velocity", &RigidBody2D::velocity);
        Register<RigidBody2D>("RigidBody2D", "angularVelocity", &RigidBody2D::angularVelocity);
        RegisterEnabled<RigidBody2D>("RigidBody2D");

        Register<Animator, float>("Animator", "speed",
            [](const Animator& a) { return a.GetSpeed(); },
            [](Animator& a, float value) { (void)a.SetSpeed(value); });
        Register<Animator, bool>("Animator", "playing",
            [](const Animator& a) { return a.IsPlaying(); },
            [](Animator& a, bool value) { if (value) a.Resume(); else a.Stop(); });
        RegisterEnabled<Animator>("Animator");
        RegisterEnabled<PlayerInput>("PlayerInput");
        return true;
    }();
    (void)registered;
}

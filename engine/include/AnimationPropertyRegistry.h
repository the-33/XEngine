#pragma once
/** @file AnimationPropertyRegistry.h
 * @brief Type-safe extensibility for property animation on built-in or custom components.
 * @ingroup xengine_render
 */

#include "Animation.h"
#include "Component.h"
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

/** @brief A reusable, safe binding to a registered component property.
 * The binding resolves its target by scene entity ID when accessed, so a
 * destroyed child object never leaves an unchecked dangling component pointer.
 */
struct AnimationPropertyBinding {
    std::function<AnimationValue()> read; ///< Returns the current component value.
    std::function<void(const AnimationValue&)> write; ///< Applies a value through its registered setter.
    std::function<bool()> valid; ///< Rechecks that the target component still exists.

    /** @brief Whether both accessors can currently reach their target. */
    bool IsValid() const { return valid && valid(); }
};

/** @brief Registry of properties that clips are explicitly allowed to animate.
 * The registry never writes to raw field offsets. Registration is typically
 * performed during game initialization, before assets are evaluated.
 */
class AnimationPropertyRegistry final {
public:
    /** @brief Register a property using a typed getter and setter.
     * @tparam T Concrete Component subclass.
     * @tparam V One alternative of AnimationValue.
     * @param componentName Stable name used in JSON tracks.
     * @param propertyName Stable property name used in JSON tracks.
     * @param getter Callable accepting const T& and returning V.
     * @param setter Callable accepting T& and V; may validate the value.
     * @return False for duplicate or invalid component/property names.
     * @warning Register before creating Animators. Registration is main-thread only.
     */
    template<class T, class V, class Getter, class Setter>
    static bool Register(std::string componentName, std::string propertyName,
                         Getter getter, Setter setter)
    {
        static_assert(std::is_base_of_v<Component, T>, "Animation target must be a Component");
        static_assert(std::is_constructible_v<AnimationValue, V>, "Unsupported animation value type");
        const auto resolver = [](GameObject& object, unsigned index) -> Component* {
            const auto items = object.GetComponents<T>();
            return index < items.size() ? items[index] : nullptr;
        };
        const auto read = [getter](Component& component) -> AnimationValue {
            return AnimationValue{std::invoke(getter, static_cast<const T&>(component))};
        };
        const auto write = [setter](Component& component, const AnimationValue& value) {
            if (const auto* typed = std::get_if<V>(&value))
                std::invoke(setter, static_cast<T&>(component), *typed);
        };
        return RegisterRaw(std::move(componentName), std::move(propertyName),
                           resolver, read, write);
    }

    /** @brief Shorthand for a directly writable member of a user component.
     * @tparam T Concrete Component subclass.
     * @tparam V One alternative of AnimationValue.
     * @param componentName Stable JSON component type name.
     * @param propertyName Stable JSON property name.
     * @param member Public, writable data member pointer.
     * @return False when the registration already exists.
     */
    template<class T, class V>
    static bool Register(std::string componentName, std::string propertyName, V T::*member)
    {
        return Register<T, V>(std::move(componentName), std::move(propertyName),
            [member](const T& object) -> V { return object.*member; },
            [member](T& object, V value) { object.*member = std::move(value); });
    }

    /** @brief Resolve a target, component and property into checked accessors.
     * @param owner GameObject carrying the Animator.
     * @param target Self or slash-delimited descendant names.
     * @param componentName Registered component name.
     * @param propertyName Registered property name.
     * @param componentIndex Zero-based index among components of that type.
     * @return An empty binding when no compatible target or registration exists.
     */
    static AnimationPropertyBinding Bind(GameObject& owner, std::string_view target,
        std::string_view componentName, std::string_view propertyName,
        unsigned componentIndex = 0);

    /** @brief Install built-in Transform, Behaviour and SpriteRenderer properties once. */
    static void RegisterBuiltins();

private:
    using Resolver = std::function<Component*(GameObject&, unsigned)>;
    using Reader = std::function<AnimationValue(Component&)>;
    using Writer = std::function<void(Component&, const AnimationValue&)>;
    static bool RegisterRaw(std::string componentName, std::string propertyName,
                            Resolver resolve, Reader read, Writer write);
};

/** @brief Register a public member without writing getters or setters.
 * Call during game initialization (before constructing Animators).
 * @param Type Concrete component class.
 * @param Member Public member to register.
 */
#define XENGINE_ANIMATABLE(Type, Member) \
    ::AnimationPropertyRegistry::Register<Type>(#Type, #Member, &Type::Member)

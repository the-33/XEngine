#pragma once
/** @file Services.h
 * @brief Typed, non-owning Unity-style shortcuts for engine-owned services.
 * @details Included by XEngine.h; direct inclusion is also supported.
 * Accessors are cheap objects, never owners of the underlying managers.
 * Their pointer is obtained on every access, so it reflects engine restarts.
 */

#include "TimeManager.h"
#include "InputManager.h"
#include "AssetManager.h"
#include "SoundManager.h"
#include "SceneManager.h"
#include "RenderManager.h"
#include "UIManager.h"
#include "WindowManager.h"
#include "PhysicsManager.h"
#include "RandomManager.h"
#include "Engine.h"

namespace xengine::detail {
/** @brief Non-owning typed view of one engine singleton.
 * @tparam T Manager type exposing GetInstancePtr().
 * @warning Access managers after engine initialization, and do not retain a
 * pointer across Engine::Stop or a subsequent initialization.
 * @cond XENGINE_INTERNAL
 */
template<class T>
struct ServiceAccess final {
    /** @brief Looks up the current manager pointer (or null if unavailable). */
    [[nodiscard]] T* get() const noexcept { return T::GetInstancePtr(); }
    /** @brief Converts to a manager pointer without transferring ownership. */
    [[nodiscard]] operator T*() const noexcept { return get(); }
    /** @brief Allows conventional shorthand such as Input->KeyDown(...). */
    [[nodiscard]] T* operator->() const noexcept { return get(); }
    /** @brief True while the corresponding manager exists. */
    [[nodiscard]] explicit operator bool() const noexcept {
        return T::GetInstancePtr_NO_ERROR_MSG() != nullptr;
    }
};

/** @brief Read-only view of the active scene camera. */
struct CameraAccess final {
    /** @brief Gets the current camera or null when no engine is active. */
    [[nodiscard]] Camera2D* get() const noexcept {
        Engine* engine = Engine::GetInstancePtr();
        return engine ? engine->camera.get() : nullptr;
    }
    /** @brief Converts to the current camera pointer. */
    [[nodiscard]] operator Camera2D*() const noexcept { return get(); }
    /** @brief Allows CurrentCamera->... with an active camera. */
    [[nodiscard]] Camera2D* operator->() const noexcept { return get(); }
};

/** @brief Read-only view of the current physics fixed timestep. */
struct FixedDeltaAccess final {
    /** @brief Gets the configured fixed timestep, or zero without an engine. */
    [[nodiscard]] float get() const noexcept {
        Engine* engine = Engine::GetInstancePtr();
        return engine ? engine->fixedDelta.get() : 0.f;
    }
    /** @brief Converts to a float representing the current fixed timestep. */
    [[nodiscard]] operator float() const noexcept { return get(); }
};
/** @endcond */
}

/** @brief Non-owning shortcut to TimeManager (available after startup). */
inline constexpr xengine::detail::ServiceAccess<TimeManager> Time{};
/** @brief Non-owning shortcut to InputManager (available after startup). */
inline constexpr xengine::detail::ServiceAccess<InputManager> Input{};
/** @brief Non-owning shortcut to AssetManager (available after startup). */
inline constexpr xengine::detail::ServiceAccess<AssetManager> Assets{};
/** @brief Non-owning shortcut to SoundManager (available after startup). */
inline constexpr xengine::detail::ServiceAccess<SoundManager> Sound{};
/** @brief Non-owning shortcut to SceneManager (available after startup). */
inline constexpr xengine::detail::ServiceAccess<SceneManager> Scenes{};
/** @brief Non-owning shortcut to RenderManager (available after startup). */
inline constexpr xengine::detail::ServiceAccess<RenderManager> Renderer{};
/** @brief Non-owning shortcut to UIManager (available after startup). */
inline constexpr xengine::detail::ServiceAccess<UIManager> UI{};
/** @brief Non-owning shortcut to WindowManager (available after startup). */
inline constexpr xengine::detail::ServiceAccess<WindowManager> Window{};
/** @brief Non-owning shortcut to PhysicsManager (available after startup). */
inline constexpr xengine::detail::ServiceAccess<PhysicsManager> Physics{};
/** @brief Non-owning shortcut to RandomManager (available after startup). */
inline constexpr xengine::detail::ServiceAccess<RandomManager> Random{};
/** @brief Read-only accessor for the current camera; may be null. */
inline constexpr xengine::detail::CameraAccess CurrentCamera{};
/** @brief Read-only accessor for Engine's fixed simulation timestep. */
inline constexpr xengine::detail::FixedDeltaAccess FixedDT{};

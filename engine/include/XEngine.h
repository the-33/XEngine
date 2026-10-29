#pragma once
/** @file XEngine.h
 * @brief Umbrella include and optional manager access macros for XEngine games.
 * @note Macros such as `Input` expand to non-owning manager pointers, not
 *       references. Call them only after the engine is initialized.
 */
/** @defgroup xengine_core Engine and scene lifecycle
 *  Public entities, behaviours, scenes and game-loop services.
 */
/** @defgroup xengine_math Math and transforms
 *  Numeric values, vectors, geometry, colors and hierarchical transforms.
 */
/** @defgroup xengine_render Window, camera, rendering and UI
 *  Main-thread graphics and windowing interfaces.
 */
/** @defgroup xengine_physics Physics and collision
 *  Rigid bodies, colliders, triggers and simulation settings.
 */
/** @defgroup xengine_services Resources, audio, time and input
 *  Managers that provide runtime services to gameplay code.
 */
/** @defgroup xengine_storage Files and player preferences
 *  Raw I/O, explicit atomic save recovery and per-user settings.
 */
/** @defgroup xengine_serialization Binary and JSON serialization
 *  Bounded readers/writers, containers and registered game classes.
 */
/** @defgroup xengine_utilities General utility types
 *  Owner-bound properties and reusable engine helper types.
 */
#include "BuildConfig.h"
#include "Platform.h"

//============= CORE =============
#include "BaseTypes.h"
#include "ErrorHandler.h"
#include "Singleton.h"
#include "Property.h"
#include "FileSystem.h"
#include "AtomicFile.h"
#include "BinaryIO.h"
#include "PlayerPrefs.h"
#include "Assets.h"
#include "KeyCodes.h"

//============= COMPONENT SYSTEM =============
#include "Component.h"
#include "Behaviour.h"
#include "Transform.h"
#include "SpriteRenderer.h"
#include "RigidBody2D.h"
#include "Collider2D.h"
#include "GameObject.h"

//============= MANAGERS =============
#include "WindowManager.h"
#include "RenderManager.h"
#include "InputManager.h"
#include "TimeManager.h"
#include "SoundManager.h"
#include "AssetManager.h"
#include "CollisionManager.h"
#include "UIManager.h"
#include "SceneManager.h"
#include "PhysicsManager.h"
#include "RandomManager.h"

//============= SCENE / CAMERA / ENGINE =============
#include "Camera2D.h"
#include "Scene.h"
#include "Engine.h"

/** @brief Convenience alias for accessing time from gameplay code.
 */
#define Time      TimeManager::GetInstancePtr()
/** @brief Convenience alias for accessing input from gameplay code.
 */
#define Input     InputManager::GetInstancePtr()
/** @brief Convenience alias for accessing assets from gameplay code.
 */
#define Assets    AssetManager::GetInstancePtr()
/** @brief Convenience alias for accessing sound from gameplay code.
 */
#define Sound     SoundManager::GetInstancePtr()
/** @brief Convenience alias for accessing scenes from gameplay code.
 */
#define Scenes    SceneManager::GetInstancePtr()
/** @brief Convenience alias for accessing renderer from gameplay code.
 */
#define Renderer  RenderManager::GetInstancePtr()
/** @brief Convenience alias for accessing ui from gameplay code.
 */
#define UI        UIManager::GetInstancePtr()
/** @brief Convenience alias for accessing window from gameplay code.
 */
#define Window    WindowManager::GetInstancePtr()
/** @brief Convenience alias for accessing physics from gameplay code.
 */
#define Physics   PhysicsManager::GetInstancePtr()
/** @brief Convenience alias for accessing random from gameplay code.
 */
#define Random	  RandomManager::GetInstancePtr()

/** @brief Gets the current scene camera property through the engine singleton.
 * @warning Dereferences Engine::GetInstancePtr(); only use after successful
 *          initialization and when an active scene exists.
 */
#define CurrentCamera Engine::GetInstancePtr()->camera

/** @brief Gets the engine's fixed simulation timestep property.
 * @warning Dereferences the engine singleton; it must already be initialized.
 */
#define FixedDT Engine::GetInstancePtr()->fixedDelta

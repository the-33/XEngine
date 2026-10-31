#pragma once
/** @file XEngine.h
 * @brief Gameplay-first umbrella header for XEngine games.
 * @details The common scene, component, render, physics and runtime services
 * are available in the global namespace. Low-level storage and serialization
 * belong to optional headers under the XEngine/ directory.
 * @note Convenience services such as `Input` are typed, non-owning accessors.
 *       Use them only while the engine is initialized.
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
#include "PlayerInput.h"
#include "TimeManager.h"
#include "SoundManager.h"
#include "AssetManager.h"
#include "CollisionManager.h"
#include "UIRectTransform.h"
#include "UIManager.h"
#include "SceneManager.h"
#include "PhysicsManager.h"
#include "RandomManager.h"

//============= SCENE / CAMERA / ENGINE =============
#include "Camera2D.h"
#include "Scene.h"
#include "Engine.h"

// Typed convenience accessors preserve Time->..., Input->..., etc. without
// global preprocessor substitutions. Prefer .get() when storing a pointer.
#include "XEngine/Services.h"

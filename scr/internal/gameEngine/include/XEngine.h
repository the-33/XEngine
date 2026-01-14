#pragma once

	//============= SDL =============
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"

//============= CORE =============
#include "BaseTypes.h"
#include "ErrorHandler.h"
#include "Singleton.h"
#include "Property.h"
#include "Assets.h"

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

//============= ESCENA / CÁMARA / MOTOR =============
#include "Camera2D.h"
#include "Scene.h"
#include "Engine.h"

#define Time      TimeManager::GetInstancePtr()
#define Input     InputManager::GetInstancePtr()
#define Assets    AssetManager::GetInstancePtr()
#define Sound     SoundManager::GetInstancePtr()
#define Scenes    SceneManager::GetInstancePtr()
#define Renderer  RenderManager::GetInstancePtr()
#define UI        UIManager::GetInstancePtr()
#define Window    WindowManager::GetInstancePtr()
#define Physics   PhysicsManager::GetInstancePtr()
#define Random	  RandomManager::GetInstancePtr()

#define CurrentCamera Engine::GetInstancePtr()->camera

#define FixedDT Engine::GetInstancePtr()->fixedDelta
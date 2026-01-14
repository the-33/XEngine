#include "Behaviour.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"

Transform* Behaviour::GetTransform() const noexcept
{
	return (mGameObject) ? mGameObject->transform : nullptr;
}

Scene* Behaviour::GetScene() const noexcept
{
	return (mGameObject) ? mGameObject->scene : nullptr;
}
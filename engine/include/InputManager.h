#pragma once

#include <string>

#include "Singleton.h"
#include "BaseTypes.h"
#include "Engine.h"
#include "KeyCodes.h"

// Forward declarations
class SceneManager;
class WindowManager;

class InputManager : public Singleton<InputManager>
{
	friend class Singleton<InputManager>;
	friend class Engine;

private:
	struct Config
	{
		bool mouseLocked = false;
		bool mouseHidden = false;
		bool TextInput = false;
	};

	bool Init(const Config& cfg) noexcept;
	void Shutdown() noexcept;
	void Update() noexcept;

	InputManager() = default;
	~InputManager() = default;

	SceneManager* scn = nullptr;
	WindowManager* win = nullptr;

	InputManager(const InputManager&) = delete;
	InputManager& operator=(const InputManager&) = delete;
	InputManager(InputManager&&) = delete;
	InputManager& operator=(InputManager&&) = delete;

	const uint8_t* mCurrKeys = nullptr;
	const uint8_t* mPrevKeys = nullptr;

	uint32_t mCurrMouseButtons = 0, mPrevMouseButtons = 0;
	int mMouseX = 0, mMouseY = 0, mPrevMouseX = 0, mPrevMouseY = 0;
	int mMouseDeltaX = 0, mMouseDeltaY = 0;
	int mWheelX = 0, mWheelY = 0;

	bool mMouseRelative = false;

	std::string mTextInputBuffer;

public:
	// Teclado
	bool KeyDown(Key key) const noexcept;
	bool KeyPressed(Key key) const noexcept;
	bool KeyReleased(Key key) const noexcept;

	// Mouse
	bool MouseDown(MouseButton button) const noexcept;
	bool MousePressed(MouseButton button) const noexcept;
	bool MouseReleased(MouseButton button) const noexcept;

	Vec2I GetMousePos() const noexcept;
	Vec2I GetMouseDelta() const noexcept;
	Vec2 GetMousePosWorld() const noexcept;
	Vec2 GetMouseDeltaWorld() const noexcept;

	Vec2I GetMouseWheel() const noexcept;

	void SetMouseMode(bool relativeLocked, bool visible) noexcept;
	inline bool IsMouseRelative() const noexcept { return mMouseRelative; }

	void SetTextInput(bool enabled) noexcept;
	inline const std::string& GetTextInput() const noexcept { return mTextInputBuffer; }
	void ClearTextInput() noexcept;
};
#pragma once
/** @file InputManager.h
 * @brief Keyboard, mouse, wheel and text input state.
 */

#include <string>

#include "Singleton.h"
#include "BaseTypes.h"
#include "Engine.h"
#include "KeyCodes.h"

// Forward declarations
class SceneManager;
class WindowManager;

/** @brief Reports current and edge-triggered keyboard and mouse state per frame.
 * @ingroup xengine_services
 */
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
	/** @brief Reports whether the scancode is held in the current frame.
	 * @param key Application-defined lookup key.
	 */
	bool KeyDown(Key key) const noexcept;
	/** @brief Reports the key-down edge: pressed now but not in the preceding frame.
	 * @param key Application-defined lookup key.
	 */
	bool KeyPressed(Key key) const noexcept;
	/** @brief Reports the key-up edge: released now after being held last frame.
	 * @param key Application-defined lookup key.
	 */
	bool KeyReleased(Key key) const noexcept;

	// Mouse
	/** @brief Tests whether the specified mouse button is held this frame.
	 * @param button Button identity or mouse button index.
	 */
	bool MouseDown(MouseButton button) const noexcept;
	/** @brief Reports the mouse-button down edge for the current frame.
	 * @param button Button identity or mouse button index.
	 */
	bool MousePressed(MouseButton button) const noexcept;
	/** @brief Reports the mouse-button up edge for the current frame.
	 * @param button Button identity or mouse button index.
	 */
	bool MouseReleased(MouseButton button) const noexcept;

	/** @brief Returns the mouse pos of InputManager.
	 */
	Vec2I GetMousePos() const noexcept;
	/** @brief Returns the mouse delta of InputManager.
	 */
	Vec2I GetMouseDelta() const noexcept;
	/** @brief Converts current mouse position into world coordinates using the current camera.
	 */
	Vec2 GetMousePosWorld() const noexcept;
	/** @brief Converts mouse motion into world-space displacement.
	 */
	Vec2 GetMouseDeltaWorld() const noexcept;

	/** @brief Returns the mouse wheel of InputManager.
	 */
	Vec2I GetMouseWheel() const noexcept;

	/** @brief Changes the mouse mode of InputManager.
	 * @param relativeLocked Whether to use relative/locked mouse input.
	 * @param visible Whether the mouse cursor should remain visible.
	 */
	void SetMouseMode(bool relativeLocked, bool visible) noexcept;
	/** @brief Reports whether mouse relative is currently true.
	 */
	inline bool IsMouseRelative() const noexcept { return mMouseRelative; }

	/** @brief Starts or stops collecting SDL text input into the engine text buffer.
	 * @param enabled Whether to enable the named feature.
	 */
	void SetTextInput(bool enabled) noexcept;
	/** @brief Returns the text input of InputManager.
	 */
	inline const std::string& GetTextInput() const noexcept { return mTextInputBuffer; }
	/** @brief Clears accumulated user text input.
	 */
	void ClearTextInput() noexcept;
};
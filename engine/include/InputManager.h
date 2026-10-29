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

/** @brief Reports SDL keyboard, mouse, wheel and UTF-8 text events per engine frame.
 * @note Edge events are available for the frame in which SDL input is polled.
 * Text and wheel data are cleared at the beginning of each new input update.
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
	/** @brief Reports whether the SDL scancode is held in the current frame.
     * @param key Physical key identity (`Key`, based on SDL scancodes).
	 */
	bool KeyDown(Key key) const noexcept;
	/** @brief Reports the key-down edge: pressed now but not in the preceding frame.
	 * @param key Physical key identity (`Key`, based on SDL scancodes).
	 */
	bool KeyPressed(Key key) const noexcept;
	/** @brief Reports the key-up edge: released now after being held last frame.
	 * @param key Physical key identity (`Key`, based on SDL scancodes).
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

	/** @brief Returns the most recently reported mouse position in window coordinates.
	 * @note In relative mouse mode, motion events update the delta but do not
	 *       update this cached absolute position.
	 */
	Vec2I GetMousePos() const noexcept;
	/** @brief Returns the mouse displacement during the current input frame.
	 * @note Relative-mode movement is summed across SDL motion events; in
	 *       absolute mode the delta is current minus previous frame position.
	 */
	Vec2I GetMouseDelta() const noexcept;
	/** @brief Projects the cached logical-window mouse position into world units.
	 * @note Converts logical window coordinates to drawable pixels first, so
	 *       HiDPI scaling does not offset the camera projection. Without an
	 *       active scene/camera, returns the raw mouse position.
	 */
	Vec2 GetMousePosWorld() const noexcept;
	/** @brief Converts the frame's logical-window mouse displacement to world units.
	 * @details Uses the difference between two screen-to-world positions, so
	 *          camera translation cancels and HiDPI scaling is respected.
	 * @note Without an active scene/camera, returns the unconverted mouse delta.
	 */
	Vec2 GetMouseDeltaWorld() const noexcept;

	/** @brief Returns the last SDL mouse-wheel event's X/Y amounts this frame.
	 */
	Vec2I GetMouseWheel() const noexcept;

	/** @brief Enables/disables relative mouse input and cursor visibility.
	 * @param relativeLocked Whether to use relative/locked mouse input.
	 * @param visible Whether the mouse cursor should remain visible.
	 */
	void SetMouseMode(bool relativeLocked, bool visible) noexcept;
	/** @brief Reports whether mouse relative is currently true.
	 */
	inline bool IsMouseRelative() const noexcept { return mMouseRelative; }

	/** @brief Starts/stops SDL text input, used for text entry rather than key binding.
     * @param enabled True to collect SDL_TEXTINPUT UTF-8 events.
	 */
	void SetTextInput(bool enabled) noexcept;
	/** @brief Returns UTF-8 text accumulated from SDL_TEXTINPUT events in this frame.
	 * @note The reference is invalidated/cleared on the next input update.
	 */
	inline const std::string& GetTextInput() const noexcept { return mTextInputBuffer; }
	/** @brief Clears accumulated user text input.
	 */
	void ClearTextInput() noexcept;
};

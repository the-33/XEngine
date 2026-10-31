#pragma once
/** @file InputManager.h
 * @brief Keyboard, mouse, wheel and text input state.
 */

#include <string>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "Singleton.h"
#include "BaseTypes.h"
#include "Engine.h"
#include "KeyCodes.h"
#include "InputActions.h"

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
	uint8_t* mPrevKeys = nullptr;

	uint32_t mCurrMouseButtons = 0, mPrevMouseButtons = 0;
	int mMouseX = 0, mMouseY = 0, mPrevMouseX = 0, mPrevMouseY = 0;
	int mMouseDeltaX = 0, mMouseDeltaY = 0;
	int mWheelX = 0, mWheelY = 0;

	bool mMouseRelative = false;
	bool mFocused = true;

	std::string mTextInputBuffer;

	struct PadState {
		struct _SDL_GameController* handle = nullptr; ///< Open SDL game-controller handle.
		std::array<uint8_t, 15> buttons{};         ///< Current controller button states.
		std::array<uint8_t, 15> previousButtons{}; ///< Controller button states from the preceding frame.
		std::array<float, 6> axes{};              ///< Normalized controller axis values.
	};
	std::map<GamepadId, PadState> mPads;
	std::map<GamepadId, std::set<PlayerId>> mPadOwners;
	std::set<GamepadId> mJoinedThisFrame;
	std::map<GamepadId, PlayerId> mLastPadOwner;
	std::map<PlayerId, PlayerInputSettings> mPlayers;
	std::map<std::string, InputControlScheme> mSchemes;
	std::map<PlayerId, InputDevice> mLastActive;
	std::vector<GamepadEvent> mPadEvents;
	struct PadListener {
		InputListenerId id; ///< Identifier used to remove this gamepad listener.
		std::function<void(const GamepadEvent&)> callback; ///< Function notified of gamepad events.
	};
	std::vector<PadListener> mPadListeners;
	InputListenerId mNextPadListener = 1;
	std::map<std::string, std::map<std::string, std::unique_ptr<InputAction>>> mActions;
	std::set<std::pair<PlayerId, std::string>> mDisabledMaps;
	std::map<std::string, InputContext> mNamedContexts;
	std::map<PlayerId, std::vector<InputContext>> mContextStacks;
	struct RebindRequest {
		std::string action;       ///< Name of the action being rebound.
		size_t index = 0;         ///< Position of the binding to replace.
		bool rejectConflicts = true; ///< Whether to reject bindings already used elsewhere.
		bool active = false;     ///< Whether a rebinding request is pending.
	} mRebind;
	std::string mLastError;
	uint32_t mPreviousTick = 0;

	void AddGamepad(int deviceIndex) noexcept;
	void RemoveGamepad(GamepadId id) noexcept;
	void NotifyGamepad(GamepadEvent event) noexcept;
	void HandleJoin(GamepadId id) noexcept;
	void MarkActive(InputDevice device) noexcept;
	void EvaluateActions(float dt) noexcept;
	InputValue SampleBinding(const InputBinding& binding, PlayerId player,
	                        InputActionType type) const noexcept;
	void HandleRebindPath(const std::string& path) noexcept;

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

	// --- Direct gamepad input (legacy keyboard/mouse calls remain unchanged) ---
	/** @brief Connected joystick instance IDs in stable ascending order. */
	std::vector<GamepadId> GetConnectedGamepads() const;
	/** @brief Whether a joystick instance is currently connected as a controller. */
	bool IsGamepadConnected(GamepadId id) const noexcept;
	/** @brief SDL-reported name of a connected gamepad, or empty for an unknown ID. */
	std::string GetGamepadName(GamepadId id) const;
	/** @brief Whether a specified controller button is held. */
	bool GamepadDown(GamepadId id, GamepadButton button) const noexcept;
	/** @brief Controller button pressed edge in the current frame. */
	bool GamepadPressed(GamepadId id, GamepadButton button) const noexcept;
	/** @brief Controller button released edge in the current frame. */
	bool GamepadReleased(GamepadId id, GamepadButton button) const noexcept;
	/** @brief Standard SDL axis with optional radial/scalar deadzone. Triggers use [0,1]. */
	float GamepadAxisValue(GamepadId id, GamepadAxis axis, float deadzone = 0.15f) const noexcept;
	/** @brief Convenience spelling of GamepadAxisValue. */
	float GamepadAxis(GamepadId id, ::GamepadAxis axis, float deadzone = 0.15f) const noexcept {
		return GamepadAxisValue(id, axis, deadzone);
	}
	/** @brief This frame's controller events (invalidated at next input update). */
	const std::vector<GamepadEvent>& GetGamepadEvents() const noexcept { return mPadEvents; }
	/** @brief Subscribe to all controller connect/assign/disconnect events. */
	InputListenerId OnGamepadEvent(std::function<void(const GamepadEvent&)> callback);
	/** @brief Subscribe only to physical controller connections. */
	InputListenerId OnGamepadConnected(std::function<void(GamepadId)> callback) {
		return OnGamepadEvent([callback = std::move(callback)](const GamepadEvent& event) {
			if (event.type == GamepadEventType::Connected && callback) callback(event.id);
		});
	}
	/** @brief Subscribe to physical disconnects with prior owning player. */
	InputListenerId OnGamepadDisconnected(std::function<void(GamepadId, PlayerId)> callback) {
		return OnGamepadEvent([callback = std::move(callback)](const GamepadEvent& event) {
			if (event.type == GamepadEventType::Disconnected && callback)
				callback(event.id, event.player);
		});
	}
	/** @brief Subscribe to disconnects when only the device ID is needed. */
	InputListenerId OnGamepadDisconnected(std::function<void(GamepadId)> callback) {
		return OnGamepadDisconnected(
			[callback = std::move(callback)](GamepadId id, PlayerId) {
				if (callback) callback(id);
			});
	}
	/** @brief Unsubscribe a previously registered controller listener. */
	bool RemoveGamepadListener(InputListenerId listener) noexcept;

	// --- Player slots and device routing ---
	/** @brief Reconfigures a player; rejects conflicting exclusive keyboard/mouse ownership. */
	bool ConfigurePlayer(PlayerInputSettings settings);
	/** @brief Remove a logical player and release its device assignments. */
	bool RemovePlayer(PlayerId player);
	/** @brief Attach a physical controller to a player, optionally sharing. */
	bool AssignGamepad(PlayerId player, GamepadId id, bool allowShared = false);
	/** @brief Remove one explicit gamepad-to-player assignment. */
	bool UnassignGamepad(PlayerId player, GamepadId id);
	/** @brief Assign the physical keyboard, with optional consent-based sharing. */
	bool AssignKeyboard(PlayerId player, bool allowShared = false);
	/** @brief Assign the physical mouse, with optional consent-based sharing. */
	bool AssignMouse(PlayerId player, bool allowShared = false);
	/** @brief Get connected gamepads belonging to this player. */
	std::vector<GamepadId> GetPlayerGamepads(PlayerId player) const;
	/** @brief Last player assigned to a controller, even after unplugging. */
	PlayerId GetLastAssignedPlayer(GamepadId id) const noexcept;
	/** @brief Family and ID of last meaningful player input. */
	InputDevice GetLastActiveDevice(PlayerId player = 0) const noexcept;
	/** @brief Currently effective player policy, or null if unregistered. */
	const PlayerInputSettings* GetPlayerSettings(PlayerId player) const noexcept;
	/** @brief Loads a JSON document with a players array of PlayerInputSettings. */
	bool LoadPlayerProfiles(const std::string& path) noexcept;
	/** @brief Loads named input schemes that profiles may reference. */
	bool LoadInputSchemes(const std::string& path) noexcept;

	// --- Action maps, values, interactions and subscriptions ---
	/** @brief Create or find a logical action in a named map. */
	InputAction* AddAction(const std::string& map, const std::string& action,
	                       InputActionType type);
	/** @brief Non-owning action pointer resolved by Map/Name. */
	InputAction* FindAction(const std::string& path) const noexcept;
	/** @brief Typed action lookup; mismatched types yield an empty handle. */
	template<class T> InputActionRef<T> FindAction(const std::string& path) const noexcept {
		static_assert(std::is_same_v<T, bool> || std::is_same_v<T, float> ||
		              std::is_same_v<T, Vec2>, "Actions support bool, float and Vec2");
		InputAction* action = FindAction(path);
		const InputActionType expected = std::is_same_v<T, bool> ? InputActionType::Button :
			(std::is_same_v<T, float> ? InputActionType::Axis1D : InputActionType::Axis2D);
		return InputActionRef<T>(action && action->Type() == expected ? action : nullptr);
	}
	/** @brief Append a validated physical binding to an existing action. */
	bool BindAction(const std::string& path, InputBinding binding);
	/** @brief Enable one map for one player. */
	bool EnableActionMap(const std::string& map, PlayerId player = 0) noexcept;
	/** @brief Disable one map, releasing its currently active actions. */
	bool DisableActionMap(const std::string& map, PlayerId player = 0) noexcept;
	/** @brief Check effective map availability including context and profile. */
	bool IsActionMapEnabled(const std::string& map, PlayerId player = 0) const noexcept;
	/** @brief Pushes a named context declared in an input actions JSON. */
	bool PushActionContext(PlayerId player, const std::string& name);
	/** @brief Pushes a C++ context (maps, priority and exclusivity). */
	bool PushActionContext(PlayerId player, InputContext context);
	/** @brief Removes the topmost context, returning false for an empty stack. */
	bool PopActionContext(PlayerId player = 0) noexcept;
	/** @brief Top of the player's context stack, or nullptr if empty. */
	const InputContext* CurrentActionContext(PlayerId player = 0) const noexcept;
	/** @brief Loads validated JSON action maps; an invalid document leaves existing maps unchanged. */
	bool LoadActionAsset(const std::string& path) noexcept;
	/** @brief Rebinds one binding, rejecting collisions by default. */
	bool RebindAction(const std::string& path, const std::string& newBinding,
	                  size_t bindingIndex = 0, bool rejectConflicts = true);
	/** @brief Capture the next SDL keyboard, mouse button or controller button. */
	bool BeginRebind(const std::string& path, size_t bindingIndex = 0,
	                 bool rejectConflicts = true) noexcept;
	/** @brief Cancel any pending input capture. */
	void CancelRebind() noexcept { mRebind.active = false; }
	/** @brief Whether the next eligible input will attempt a rebind. */
	bool IsRebinding() const noexcept { return mRebind.active; }
	/** @brief Atomically persist current binding paths to user-selected storage. */
	bool SaveBindingOverrides(const std::string& path) const noexcept;
	/** @brief Load and validate an override file for existing actions. */
	bool LoadBindingOverrides(const std::string& path) noexcept;
	/** @brief Read diagnostic text from the last failed input configuration. */
	const std::string& GetInputError() const noexcept { return mLastError; }
};

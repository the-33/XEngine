/** @brief Grant Singleton< InputManager > access to the internal state of InputManager.
 */
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
	/** @brief Permit the singleton base to construct and release the InputManager manager.
	 */
	friend class Singleton<InputManager>;
	/** @brief Grant Engine access to the internal state of InputManager.
	 */
	friend class Engine;

private:
	/** @brief Internal configuration values for InputManager.
	 */
	struct Config
	{
		/** @brief Stored mouse locked value used by Config.
		 */
		bool mouseLocked = false;
		/** @brief Stored mouse hidden value used by Config.
		 */
		bool mouseHidden = false;
		/** @brief Configured text input value used by Config.
		 */
		bool TextInput = false;
	};

	/** @brief Initialize the InputManager subsystem.
	 * @param cfg Cfg supplied to InputManager.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool Init(const Config& cfg) noexcept;
	/** @brief Release resources managed by InputManager.
	 */
	void Shutdown() noexcept;
	/** @brief Update the InputManager state for the current frame.
	 */
	void Update() noexcept;

	/** @brief Construct a InputManager instance.
	 */
	InputManager() = default;
	/** @brief Destroy a InputManager instance and release owned resources.
	 */
	~InputManager() = default;

	/** @brief Configured scn value used by InputManager.
	 */
	SceneManager* scn = nullptr;
	/** @brief Configured win value used by InputManager.
	 */
	WindowManager* win = nullptr;

	/** @brief Construct a InputManager instance.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	InputManager(const InputManager& other) = delete;
	/** @brief Implement the operator= operation for InputManager.
	 * @return Result of applying the overloaded operator.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	InputManager& operator=(const InputManager& other) = delete;
	/** @brief Construct a InputManager instance.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	InputManager(InputManager&& other) = delete;
	/** @brief Implement the operator= operation for InputManager.
	 * @return Result of applying the overloaded operator.
	 * @param other Source object whose copy, move or equality is being requested.
	 */
	InputManager& operator=(InputManager&& other) = delete;

	/** @brief Stored curr keys value used by InputManager.
	 */
	const uint8_t* mCurrKeys = nullptr;
	/** @brief Stored prev keys value used by InputManager.
	 */
	uint8_t* mPrevKeys = nullptr;
	// SDL2 reserva 512 posiciones para el estado de sus scancodes.
	/** @brief Stored repeated keys value used by InputManager.
	 */
	std::array<uint8_t, 512> mRepeatedKeys{};

	/** @brief Stored curr mouse buttons value used by InputManager.
	 */
	uint32_t mCurrMouseButtons = 0, mPrevMouseButtons = 0;
	/** @brief Stored mouse x value used by InputManager.
	 */
	int mMouseX = 0, mMouseY = 0, mPrevMouseX = 0, mPrevMouseY = 0;
	/** @brief Stored mouse delta x value used by InputManager.
	 */
	int mMouseDeltaX = 0, mMouseDeltaY = 0;
	/** @brief Stored wheel x value used by InputManager.
	 */
	int mWheelX = 0, mWheelY = 0;

	/** @brief Stored mouse relative value used by InputManager.
	 */
	bool mMouseRelative = false;
	/** @brief Stored focused value used by InputManager.
	 */
	bool mFocused = true;

	/** @brief Stored text input buffer value used by InputManager.
	 */
	std::string mTextInputBuffer;

	/** @brief Runtime state of an input device or interactive control.
	 */
	struct PadState {
		struct _SDL_GameController* handle = nullptr; ///< Open SDL game-controller handle.
		std::array<uint8_t, 15> buttons{};         ///< Current controller button states.
		std::array<uint8_t, 15> previousButtons{}; ///< Controller button states from the preceding frame.
		std::array<float, 6> axes{};              ///< Normalized controller axis values.
	};
	/** @brief Stored pads value used by InputManager.
	 */
	std::map<GamepadId, PadState> mPads;
	/** @brief Stored pad owners value used by InputManager.
	 */
	std::map<GamepadId, std::set<PlayerId>> mPadOwners;
	/** @brief Stored joined this frame value used by InputManager.
	 */
	std::set<GamepadId> mJoinedThisFrame;
	/** @brief Stored last pad owner value used by InputManager.
	 */
	std::map<GamepadId, PlayerId> mLastPadOwner;
	/** @brief Stored players value used by InputManager.
	 */
	std::map<PlayerId, PlayerInputSettings> mPlayers;
	/** @brief Stored profile load in progress value used by InputManager.
	 */
	bool mProfileLoadInProgress = false;
	/** @brief Stored schemes value used by InputManager.
	 */
	std::map<std::string, InputControlScheme> mSchemes;
	/** @brief Stored last active value used by InputManager.
	 */
	std::map<PlayerId, InputDevice> mLastActive;
	/** @brief Stored pad events value used by InputManager.
	 */
	std::vector<GamepadEvent> mPadEvents;
	/** @brief Pad listener data and utilities used by InputManager.
	 */
	struct PadListener {
		InputListenerId id; ///< Identifier used to remove this gamepad listener.
		std::function<void(const GamepadEvent&)> callback; ///< Function notified of gamepad events.
	};
	/** @brief Stored pad listeners value used by InputManager.
	 */
	std::vector<PadListener> mPadListeners;
	/** @brief Stored next pad listener value used by InputManager.
	 */
	InputListenerId mNextPadListener = 1;
	/** @brief Stored actions value used by InputManager.
	 */
	std::map<std::string, std::map<std::string, std::unique_ptr<InputAction>>> mActions;
	/** @brief Stored disabled maps value used by InputManager.
	 */
	std::set<std::pair<PlayerId, std::string>> mDisabledMaps;
	/** @brief Stored named contexts value used by InputManager.
	 */
	std::map<std::string, InputContext> mNamedContexts;
	/** @brief Stored context stacks value used by InputManager.
	 */
	std::map<PlayerId, std::vector<InputContext>> mContextStacks;
	/** @brief Rebind request data and utilities used by InputManager.
	 */
	struct RebindRequest {
		std::string action;       ///< Name of the action being rebound.
		size_t index = 0;         ///< Position of the binding to replace.
		bool rejectConflicts = true; ///< Whether to reject bindings already used elsewhere.
		bool active = false;     ///< Whether a rebinding request is pending.
	/** @brief Stored rebind value used by InputManager.
	 */
	} mRebind;
	/** @brief Stored last error value used by InputManager.
	 */
	std::string mLastError;
	/** @brief Stored previous tick value used by InputManager.
	 */
	uint32_t mPreviousTick = 0;

	/** @brief Add gamepad in InputManager.
	 * @param deviceIndex Device index used to bound or select the operation.
	 */
	void AddGamepad(int deviceIndex) noexcept;
	/** @brief Remove gamepad in InputManager.
	 * @param id Stable identifier associated with the requested object or control.
	 */
	void RemoveGamepad(GamepadId id) noexcept;
	/** @brief Notify gamepad in InputManager.
	 * @param event Incoming SDL or engine event to process.
	 */
	void NotifyGamepad(GamepadEvent event) noexcept;
	/** @brief Handle join in InputManager.
	 * @param id Stable identifier associated with the requested object or control.
	 */
	void HandleJoin(GamepadId id) noexcept;
	/** @brief Mark active in InputManager.
	 * @param device Device supplied to InputManager.
	 */
	void MarkActive(InputDevice device) noexcept;
	/** @brief Evaluate actions in InputManager.
	 * @param dt Elapsed simulation time for the update, in seconds.
	 */
	void EvaluateActions(float dt) noexcept;
	/** @brief Sample binding in InputManager.
	 * @param binding Binding supplied to InputManager.
	 * @param player Logical player whose input is queried or updated.
	 * @param type Type supplied to InputManager.
	 * @return Result of sample binding in InputManager.
	 */
	InputValue SampleBinding(const InputBinding& binding, PlayerId player,
	                        InputActionType type) const noexcept;
	/** @brief Handle rebind path in InputManager.
	 * @param path Filesystem path identifying the resource.
	 */
	void HandleRebindPath(const std::string& path) noexcept;

public:
	// Teclado
	/** @brief Reports whether the SDL scancode is held in the current frame.
     * @param key Physical key identity (`Key`, based on SDL scancodes).
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool KeyDown(Key key) const noexcept;
	/** @brief Reports the key-down edge: pressed now but not in the preceding frame.
	 * @param key Physical key identity (`Key`, based on SDL scancodes).
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool KeyPressed(Key key) const noexcept;
	/** @brief Indica si la tecla se ha pulsado o ha generado repetición SDL este frame.
	 * @param key Tecla física cuyo evento de repetición se consulta.
	 * @note Los caracteres escritos se reciben por GetTextInput(), no por esta función.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool KeyPressedOrRepeated(Key key) const noexcept;
	/** @brief Reports the key-up edge: released now after being held last frame.
	 * @param key Physical key identity (`Key`, based on SDL scancodes).
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool KeyReleased(Key key) const noexcept;

	// Mouse
	/** @brief Tests whether the specified mouse button is held this frame.
	 * @param button Button identity or mouse button index.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool MouseDown(MouseButton button) const noexcept;
	/** @brief Reports the mouse-button down edge for the current frame.
	 * @param button Button identity or mouse button index.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool MousePressed(MouseButton button) const noexcept;
	/** @brief Reports the mouse-button up edge for the current frame.
	 * @param button Button identity or mouse button index.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool MouseReleased(MouseButton button) const noexcept;

	/** @brief Returns the most recently reported mouse position in window coordinates.
	 * @note In relative mouse mode, motion events update the delta but do not
	 *       update this cached absolute position.
	 * @return The most recently reported mouse position in window coordinates.
	 */
	Vec2I GetMousePos() const noexcept;
	/** @brief Returns the mouse displacement during the current input frame.
	 * @note Relative-mode movement is summed across SDL motion events; in
	 *       absolute mode the delta is current minus previous frame position.
	 * @return The mouse displacement during the current input frame.
	 */
	Vec2I GetMouseDelta() const noexcept;
	/** @brief Projects the cached logical-window mouse position into world units.
	 * @note Converts logical window coordinates to drawable pixels first, so
	 *       HiDPI scaling does not offset the camera projection. Without an
	 *       active scene/camera, returns the raw mouse position.
	 * @return Requested get mouse pos world value or reference.
	 */
	Vec2 GetMousePosWorld() const noexcept;
	/** @brief Converts the frame's logical-window mouse displacement to world units.
	 * @details Uses the difference between two screen-to-world positions, so
	 *          camera translation cancels and HiDPI scaling is respected.
	 * @note Without an active scene/camera, returns the unconverted mouse delta.
	 * @return Requested get mouse delta world value or reference.
	 */
	Vec2 GetMouseDeltaWorld() const noexcept;

	/** @brief Returns the last SDL mouse-wheel event's X/Y amounts this frame.
	 * @return The last sdl mouse-wheel event's x/y amounts this frame.
	 */
	Vec2I GetMouseWheel() const noexcept;

	/** @brief Enables/disables relative mouse input and cursor visibility.
	 * @param relativeLocked Whether to use relative/locked mouse input.
	 * @param visible Whether the mouse cursor should remain visible.
	 */
	void SetMouseMode(bool relativeLocked, bool visible) noexcept;
	/** @brief Reports whether mouse relative is currently true.
	 * @return True when is mouse relative; otherwise false.
	 */
	inline bool IsMouseRelative() const noexcept { return mMouseRelative; }

	/** @brief Starts/stops SDL text input, used for text entry rather than key binding.
     * @param enabled True to collect SDL_TEXTINPUT UTF-8 events.
	 */
	void SetTextInput(bool enabled) noexcept;
	/** @brief Returns UTF-8 text accumulated from SDL_TEXTINPUT events in this frame.
	 * @note The reference is invalidated/cleared on the next input update.
	 * @return Utf-8 text accumulated from sdl_textinput events in this frame.
	 */
	inline const std::string& GetTextInput() const noexcept { return mTextInputBuffer; }
	/** @brief Clears accumulated user text input.
	 */
	void ClearTextInput() noexcept;

	// --- Direct gamepad input (legacy keyboard/mouse calls remain unchanged) ---
	/** @brief Connected joystick instance IDs in stable ascending order.
	 * @return Requested get connected gamepads value or reference.
	 */
	std::vector<GamepadId> GetConnectedGamepads() const;
	/** @brief Whether a joystick instance is currently connected as a controller.
	 * @param id Stable identifier associated with the requested object or control.
	 * @return True if a joystick instance is currently connected as a controller; otherwise false.
	 */
	bool IsGamepadConnected(GamepadId id) const noexcept;
	/** @brief SDL-reported name of a connected gamepad, or empty for an unknown ID.
	 * @param id Stable identifier associated with the requested object or control.
	 * @return Requested get gamepad name value or reference.
	 */
	std::string GetGamepadName(GamepadId id) const;
	/** @brief Whether a specified controller button is held.
	 * @param id Stable identifier associated with the requested object or control.
	 * @param button Button supplied to InputManager.
	 * @return True if a specified controller button is held; otherwise false.
	 */
	bool GamepadDown(GamepadId id, GamepadButton button) const noexcept;
	/** @brief Controller button pressed edge in the current frame.
	 * @param id Stable identifier associated with the requested object or control.
	 * @param button Button supplied to InputManager.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool GamepadPressed(GamepadId id, GamepadButton button) const noexcept;
	/** @brief Controller button released edge in the current frame.
	 * @param id Stable identifier associated with the requested object or control.
	 * @param button Button supplied to InputManager.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool GamepadReleased(GamepadId id, GamepadButton button) const noexcept;
	/** @brief Standard SDL axis with optional radial/scalar deadzone. Triggers use [0,1].
	 * @param id Stable identifier associated with the requested object or control.
	 * @param axis Axis supplied to InputManager.
	 * @param deadzone Deadzone supplied to InputManager.
	 * @return Result of gamepad axis value in InputManager.
	 */
	float GamepadAxisValue(GamepadId id, GamepadAxis axis, float deadzone = 0.15f) const noexcept;
	/** @brief Convenience spelling of GamepadAxisValue.
	 * @param id Stable identifier associated with the requested object or control.
	 * @param axis Axis supplied to InputManager.
	 * @param deadzone Deadzone supplied to InputManager.
	 * @return Result of gamepad axis in InputManager.
	 */
	float GamepadAxis(GamepadId id, ::GamepadAxis axis, float deadzone = 0.15f) const noexcept {
		return GamepadAxisValue(id, axis, deadzone);
	}
	/** @brief This frame's controller events (invalidated at next input update).
	 * @return Requested get gamepad events value or reference.
	 */
	const std::vector<GamepadEvent>& GetGamepadEvents() const noexcept { return mPadEvents; }
	/** @brief Subscribe to all controller connect/assign/disconnect events.
	 * @param callback Callable invoked when the relevant event occurs.
	 * @return Result of on gamepad event in InputManager.
	 */
	InputListenerId OnGamepadEvent(std::function<void(const GamepadEvent&)> callback);
	/** @brief Subscribe only to physical controller connections.
	 * @param callback Callable invoked when the relevant event occurs.
	 * @return Result of on gamepad connected in InputManager.
	 */
	InputListenerId OnGamepadConnected(std::function<void(GamepadId)> callback) {
		return OnGamepadEvent([callback = std::move(callback)](const GamepadEvent& event) {
			if (event.type == GamepadEventType::Connected && callback) callback(event.id);
		});
	}
	/** @brief Subscribe to physical disconnects with prior owning player.
	 * @param callback Callable invoked when the relevant event occurs.
	 * @return Result of on gamepad disconnected in InputManager.
	 */
	InputListenerId OnGamepadDisconnected(std::function<void(GamepadId, PlayerId)> callback) {
		return OnGamepadEvent([callback = std::move(callback)](const GamepadEvent& event) {
			if (event.type == GamepadEventType::Disconnected && callback)
				callback(event.id, event.player);
		});
	}
	/** @brief Subscribe to disconnects when only the device ID is needed.
	 * @param callback Callable invoked when the relevant event occurs.
	 * @return Result of on gamepad disconnected in InputManager.
	 */
	InputListenerId OnGamepadDisconnected(std::function<void(GamepadId)> callback) {
		return OnGamepadDisconnected(
			[callback = std::move(callback)](GamepadId id, PlayerId) {
				if (callback) callback(id);
			});
	}
	/** @brief Unsubscribe a previously registered controller listener.
	 * @param listener Listener supplied to InputManager.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool RemoveGamepadListener(InputListenerId listener) noexcept;

	// --- Player slots and device routing ---
	/** @brief Reconfigures a player; rejects conflicting exclusive keyboard/mouse ownership.
	 * @param settings Settings supplied to InputManager.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool ConfigurePlayer(PlayerInputSettings settings);
	/** @brief Remove a logical player and release its device assignments.
	 * @param player Logical player whose input is queried or updated.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool RemovePlayer(PlayerId player);
	/** @brief Attach a physical controller to a player, optionally sharing.
	 * @param player Logical player whose input is queried or updated.
	 * @param id Stable identifier associated with the requested object or control.
	 * @param allowShared Whether to allow shared for this operation.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool AssignGamepad(PlayerId player, GamepadId id, bool allowShared = false);
	/** @brief Remove one explicit gamepad-to-player assignment.
	 * @param player Logical player whose input is queried or updated.
	 * @param id Stable identifier associated with the requested object or control.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool UnassignGamepad(PlayerId player, GamepadId id);
	/** @brief Assign the physical keyboard, with optional consent-based sharing.
	 * @param player Logical player whose input is queried or updated.
	 * @param allowShared Whether to allow shared for this operation.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool AssignKeyboard(PlayerId player, bool allowShared = false);
	/** @brief Assign the physical mouse, with optional consent-based sharing.
	 * @param player Logical player whose input is queried or updated.
	 * @param allowShared Whether to allow shared for this operation.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool AssignMouse(PlayerId player, bool allowShared = false);
	/** @brief Get connected gamepads belonging to this player.
	 * @param player Logical player whose input is queried or updated.
	 * @return Requested get player gamepads value or reference.
	 */
	std::vector<GamepadId> GetPlayerGamepads(PlayerId player) const;
	/** @brief Last player assigned to a controller, even after unplugging.
	 * @param id Stable identifier associated with the requested object or control.
	 * @return Requested get last assigned player value or reference.
	 */
	PlayerId GetLastAssignedPlayer(GamepadId id) const noexcept;
	/** @brief Family and ID of last meaningful player input.
	 * @param player Logical player whose input is queried or updated.
	 * @return Requested get last active device value or reference.
	 */
	InputDevice GetLastActiveDevice(PlayerId player = 0) const noexcept;
	/** @brief Currently effective player policy, or null if unregistered.
	 * @param player Logical player whose input is queried or updated.
	 * @return Requested get player settings value or reference.
	 */
	const PlayerInputSettings* GetPlayerSettings(PlayerId player) const noexcept;
	/** @brief Loads a JSON document with a players array of PlayerInputSettings.
	 * @param path Filesystem path identifying the resource.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool LoadPlayerProfiles(const std::string& path) noexcept;
	/** @brief Loads named input schemes that profiles may reference.
	 * @param path Filesystem path identifying the resource.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool LoadInputSchemes(const std::string& path) noexcept;

	// --- Action maps, values, interactions and subscriptions ---
	/** @brief Create or find a logical action in a named map.
	 * @param map Map supplied to InputManager.
	 * @param action Action supplied to InputManager.
	 * @param type Type supplied to InputManager.
	 * @return Result of add action in InputManager.
	 */
	InputAction* AddAction(const std::string& map, const std::string& action,
	                       InputActionType type);
	/** @brief Non-owning action pointer resolved by Map/Name.
	 * @param path Filesystem path identifying the resource.
	 * @return Requested find action value or reference.
	 */
	InputAction* FindAction(const std::string& path) const noexcept;
	/** @brief Typed action lookup; mismatched types yield an empty handle.
	 * @param path Filesystem path identifying the resource.
	 * @return Requested find action value or reference.
	 */
	template<class T> InputActionRef<T> FindAction(const std::string& path) const noexcept {
		static_assert(std::is_same_v<T, bool> || std::is_same_v<T, float> ||
		              std::is_same_v<T, Vec2>, "Actions support bool, float and Vec2");
		InputAction* action = FindAction(path);
		const InputActionType expected = std::is_same_v<T, bool> ? InputActionType::Button :
			(std::is_same_v<T, float> ? InputActionType::Axis1D : InputActionType::Axis2D);
		return InputActionRef<T>(action && action->Type() == expected ? action : nullptr);
	}
	/** @brief Append a validated physical binding to an existing action.
	 * @param path Filesystem path identifying the resource.
	 * @param binding Binding supplied to InputManager.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool BindAction(const std::string& path, InputBinding binding);
	/** @brief Enable one map for one player.
	 * @param map Map supplied to InputManager.
	 * @param player Logical player whose input is queried or updated.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool EnableActionMap(const std::string& map, PlayerId player = 0) noexcept;
	/** @brief Disable one map, releasing its currently active actions.
	 * @param map Map supplied to InputManager.
	 * @param player Logical player whose input is queried or updated.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool DisableActionMap(const std::string& map, PlayerId player = 0) noexcept;
	/** @brief Check effective map availability including context and profile.
	 * @param map Map supplied to InputManager.
	 * @param player Logical player whose input is queried or updated.
	 * @return True when is action map enabled; otherwise false.
	 */
	bool IsActionMapEnabled(const std::string& map, PlayerId player = 0) const noexcept;
	/** @brief Pushes a named context declared in an input actions JSON.
	 * @param player Logical player whose input is queried or updated.
	 * @param name Name of the resource, property or element to access.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool PushActionContext(PlayerId player, const std::string& name);
	/** @brief Pushes a C++ context (maps, priority and exclusivity).
	 * @param player Logical player whose input is queried or updated.
	 * @param context Context supplied to InputManager.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool PushActionContext(PlayerId player, InputContext context);
	/** @brief Removes the topmost context, returning false for an empty stack.
	 * @param player Logical player whose input is queried or updated.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool PopActionContext(PlayerId player = 0) noexcept;
	/** @brief Top of the player's context stack, or nullptr if empty.
	 * @param player Logical player whose input is queried or updated.
	 * @return Result of current action context in InputManager.
	 */
	const InputContext* CurrentActionContext(PlayerId player = 0) const noexcept;
	/** @brief Loads validated JSON action maps; an invalid document leaves existing maps unchanged.
	 * @param path Filesystem path identifying the resource.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool LoadActionAsset(const std::string& path) noexcept;
	/** @brief Rebinds one binding, rejecting collisions by default.
	 * @param path Filesystem path identifying the resource.
	 * @param newBinding New binding supplied to InputManager.
	 * @param bindingIndex Binding index used to bound or select the operation.
	 * @param rejectConflicts Reject conflicts supplied to InputManager.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool RebindAction(const std::string& path, const std::string& newBinding,
	                  size_t bindingIndex = 0, bool rejectConflicts = true);
	/** @brief Capture the next SDL keyboard, mouse button or controller button.
	 * @param path Filesystem path identifying the resource.
	 * @param bindingIndex Binding index used to bound or select the operation.
	 * @param rejectConflicts Reject conflicts supplied to InputManager.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool BeginRebind(const std::string& path, size_t bindingIndex = 0,
	                 bool rejectConflicts = true) noexcept;
	/** @brief Cancel any pending input capture. */
	void CancelRebind() noexcept { mRebind.active = false; }
	/** @brief Whether the next eligible input will attempt a rebind.
	 * @return True if the next eligible input will attempt a rebind; otherwise false.
	 */
	bool IsRebinding() const noexcept { return mRebind.active; }
	/** @brief Atomically persist current binding paths to user-selected storage.
	 * @param path Filesystem path identifying the resource.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool SaveBindingOverrides(const std::string& path) const noexcept;
	/** @brief Load and validate an override file for existing actions.
	 * @param path Filesystem path identifying the resource.
	 * @return True if the operation succeeded or its condition holds; otherwise false.
	 */
	bool LoadBindingOverrides(const std::string& path) noexcept;
	/** @brief Read diagnostic text from the last failed input configuration.
	 * @return Requested get input error value or reference.
	 */
	const std::string& GetInputError() const noexcept { return mLastError; }
};

/** @name Co-declared engine member descriptions
 * @brief Members sharing declarations with other fields.
 * @{ */
/** @var uint32_t InputManager::mPrevMouseButtons
 * @brief Stored prev mouse buttons state owned by InputManager.
 */

/** @var int InputManager::mMouseY
 * @brief Stored mouse y state owned by InputManager.
 */

/** @var int InputManager::mPrevMouseX
 * @brief Stored prev mouse x state owned by InputManager.
 */

/** @var int InputManager::mPrevMouseY
 * @brief Stored prev mouse y state owned by InputManager.
 */

/** @var int InputManager::mMouseDeltaY
 * @brief Stored mouse delta y state owned by InputManager.
 */

/** @var int InputManager::mWheelY
 * @brief Stored wheel y state owned by InputManager.
 */

/** @var struct InputManager::RebindRequest InputManager::mRebind
 * @brief Stored rebind state owned by InputManager.
 */
/** @} */

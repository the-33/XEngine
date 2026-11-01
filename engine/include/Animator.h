#pragma once
/** @file Animator.h
 * @brief Runtime state-machine component for reusable AnimationClip assets.
 * @ingroup xengine_render
 */

#include "Animation.h"
#include "AnimationPropertyRegistry.h"
#include "Behaviour.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/** @brief Plays an AnimatorController on a GameObject and its descendants.
 *
 * Animator evaluates in LateUpdate, after gameplay Update and before Render.
 * Parameter changes and Play() take effect on the next evaluation. All calls
 * and animation events execute on the engine's main thread.
 *
 * @par Example
 * @code
 * auto* animator = player->AddComponent<Animator>();
 * animator->SetController("animators/player.animator.json");
 * animator->SetFloat("Speed", 2.5f);
 * animator->SetTrigger("Jump");
 * animator->OnEvent([](std::string_view name, std::string_view state) {
 *     if (name == "Footstep") Console.Log(std::string(state) + ": footstep");
 * });
 * @endcode
 *
 * @note One controller state machine is active per Animator. A base layer's
 * mask/weight can be applied, while multiple parallel state machines and
 * additive layers are not supported in this release.
 */
class Animator final : public Behaviour {
public:
    /** @brief Listener for named animation events; receives event and state names. */
    using EventCallback = std::function<void(std::string_view, std::string_view)>;

    /** @brief Resolve a controller from the AssetManager cache and start its entry state.
     * @param assetPath Controller JSON path relative to the game assets folder.
     * @return True when the controller and entry clip were loaded.
     */
    bool SetController(const std::string& assetPath);
    /** @brief Set a shared immutable controller without taking ownership.
     * @param controller Cached or otherwise lifetime-stable controller.
     * @return False when the pointer is null or its layers are unsupported.
     */
    bool SetController(const AnimatorController* controller);
    /** @brief Currently installed controller, or nullptr. */
    const AnimatorController* GetController() const noexcept { return controller_; }

    /** @brief Immediately start a named state.
     * @param state State name from the installed controller.
     * @param normalizedTime Starting position in [0,1].
     * @return False when the state, controller or clip is missing.
     */
    bool Play(std::string_view state, float normalizedTime = 0.f);
    /** @brief Blend to a state over seconds; uses EaseInOut easing.
     * @param state Destination state name.
     * @param durationSeconds Blend duration; zero switches immediately.
     * @param normalizedOffset Destination starting position in [0,1].
     * @return False when a destination cannot be resolved.
     */
    bool CrossFade(std::string_view state, float durationSeconds, float normalizedOffset = 0.f);
    /** @brief Stop animation updates without discarding the current pose. */
    void Stop() noexcept { playing_ = false; }
    /** @brief Continue from the paused pose. */
    void Resume() noexcept { playing_ = true; }
    /** @brief Set the overall speed multiplier (nonnegative).
     * @param speed Nonnegative finite playback rate.
     * @return False for NaN, infinity or negative values.
     */
    bool SetSpeed(float speed) noexcept;
    /** @brief Return the nonnegative playback speed multiplier. */
    float GetSpeed() const noexcept { return speed_; }
    /** @brief Whether the Animator is advancing its current clip. */
    bool IsPlaying() const noexcept { return playing_; }
    /** @brief Whether a blend between two states is currently active. */
    bool IsInTransition() const noexcept { return blend_.has_value(); }
    /** @brief Name of the current state, or an empty view without a controller. */
    std::string_view GetCurrentState() const noexcept;
    /** @brief Progress of the current state, allowing values over one on loops. */
    float GetNormalizedTime() const noexcept;

    /** @brief Assign a declared Boolean parameter; return false if unavailable.
     * @param name Declared Bool parameter.
     * @param value Boolean state.
     * @return Whether the value was accepted.
     */
    bool SetBool(std::string_view name, bool value);
    /** @brief Assign a declared integer parameter.
     * @param name Declared Int parameter.
     * @param value Integer state.
     * @return Whether the value was accepted.
     */
    bool SetInt(std::string_view name, int value);
    /** @brief Assign a declared floating-point parameter.
     * @param name Declared Float parameter.
     * @param value Finite parameter value.
     * @return Whether the value was accepted.
     */
    bool SetFloat(std::string_view name, float value);
    /** @brief Activate a declared Trigger parameter until it is consumed.
     * @param name Declared Trigger parameter.
     * @return Whether the trigger exists.
     */
    bool SetTrigger(std::string_view name);
    /** @brief Clear a pending Trigger parameter without causing a transition.
     * @param name Declared Trigger parameter.
     * @return Whether the trigger exists.
     */
    bool ResetTrigger(std::string_view name);
    /** @brief Subscribe to AnimationClip events.
     * @param callback Main-thread callback receiving event and state names.
     * @return Removable listener ID or zero for an empty callback.
     */
    unsigned OnEvent(EventCallback callback);
    /** @brief Remove a previously subscribed event listener.
     * @param id Listener token returned by OnEvent.
     * @return True when an existing listener was removed.
     */
    bool RemoveEventListener(unsigned id);

protected:
    /** @brief Advance the active state, evaluate properties and dispatch events. */
    void LateUpdate(float dt) override;
    /** @brief Release cached property bindings and animation event listeners. */
    void OnDestroy() override;

private:
    /** @brief Stored value of a declared Bool, Int, Float or Trigger parameter. */
    using ParameterValue = std::variant<bool, int, float>;

    /** @brief Playback cursor and resolved asset for one state. */
    struct Playhead {
        const AnimatorState* state = nullptr; ///< Non-owning state declaration in the controller.
        const AnimationClip* clip = nullptr; ///< Cached clip owned by AssetManager.
        float elapsed = 0.f; ///< Unwrapped playback position, in seconds.
        float duration = 0.f; ///< Effective clip duration, in seconds.
    };

    /** @brief Playback cursors and blending options during an active transition. */
    struct ActiveBlend {
        const AnimatorTransition* transition = nullptr; ///< Non-owning controller edge, if any.
        Playhead from; ///< Outgoing state cursor.
        Playhead to; ///< Incoming state cursor.
        float elapsed = 0.f; ///< Seconds elapsed since blending began.
        float duration = 0.f; ///< Total blending time, in seconds.
        AnimationInterpolation curve = AnimationInterpolation::EaseInOut; ///< Easing of the blend weight.
        AnimationSpriteBlend spriteBlend = AnimationSpriteBlend::Switch; ///< How sprites change during blending.
    };

    /** @brief Sampled value together with the checked property target. */
    struct TrackPose {
        AnimationValue value; ///< Sampled property value before or after blending.
        AnimationPropertyBinding binding; ///< Validated getter and setter for the target property.
    };

    /** @brief A removable subscription to clip events. */
    struct EventListener {
        unsigned id; ///< Identifier returned by OnEvent().
        EventCallback callback; ///< Handler invoked on the main thread.
    };

    const AnimatorController* controller_ = nullptr; ///< Shared controller asset, not owned by the Animator.
    Playhead current_; ///< Active state playback cursor.
    std::optional<ActiveBlend> blend_; ///< Active transition, if one is running.
    std::unordered_map<std::string, ParameterValue> parameters_; ///< Per-instance controller parameters.
    std::unordered_map<std::string, AnimationPropertyBinding> bindings_; ///< Resolved animatable properties.
    std::unordered_map<std::string, AnimationValue> originals_; ///< Values captured before animation writes.
    std::vector<EventListener> listeners_; ///< Subscribers to named clip events.
    unsigned nextListenerId_ = 1; ///< Identifier reserved for the next listener.
    float speed_ = 1.f; ///< Runtime playback speed multiplier.
    bool playing_ = true; ///< Whether playback advances during LateUpdate().

    /** @brief Resolve a state name into a valid playback cursor. */
    bool MakePlayhead(std::string_view name, float normalizedTime, Playhead& output);
    /** @brief Wrap or clamp elapsed time according to the clip loop mode. */
    static float SampleTime(const Playhead& head) noexcept;
    /** @brief Effective playback multiplier of a clip and its state. */
    static float ClipSpeed(const Playhead& head) noexcept;
    /** @brief Advance a cursor by the scaled time step. */
    static void Advance(Playhead& head, float dt) noexcept;
    /** @brief Notify subscribers of all events crossed during a time interval. */
    void DispatchEvents(const Playhead& head, float previous, float current);
    /** @brief Evaluate all declared conditions of a transition. */
    bool ConditionsPass(const AnimatorTransition& edge) const;
    /** @brief Whether playback crossed a transition's normalized exit time. */
    bool ExitTimeCrossed(const AnimatorTransition& edge,
                         const Playhead& head, float previous) const noexcept;
    /** @brief Find the highest-priority eligible transition for a state. */
    const AnimatorTransition* FindTransition(const Playhead& head, float previous,
                                            const AnimatorTransition* interrupting = nullptr) const;
    /** @brief Initialize the outgoing and incoming cursors of a transition. */
    bool BeginTransition(const AnimatorTransition& edge, const Playhead& source);
    /** @brief Sample all property tracks of one state. */
    std::unordered_map<std::string, TrackPose> SamplePose(const Playhead& head);
    /** @brief Blend sampled properties and write them through validated bindings. */
    void ApplyPose(const Playhead& from, const Playhead* to, float weight);
    /** @brief Select or cross-fade sprites in a SpriteRenderer. */
    void ApplySprite(const Playhead& head, const Playhead* to, float weight,
                     AnimationSpriteBlend blendMode);
    /** @brief Restore values of tracks no longer driven by the next clip. */
    void RestoreUncontrolled(const Playhead& next);
    /** @brief Validate and assign one declared runtime parameter. */
    bool SetParameter(std::string_view name, AnimatorParameterType type, ParameterValue value);
};

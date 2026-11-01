#pragma once
/** @file Animation.h
 * @brief Reusable clips, typed keyframes, sprite sequences and Animator state-machine assets.
 * @ingroup xengine_render
 */

#include "BaseTypes.h"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/** @brief Supported animation property values. Strings are texture asset paths. */
using AnimationValue = std::variant<bool, int, float, Vec2, Vec3, Color, RectI, std::string>;

/** @brief Interpolation applied until the following keyframe. */
enum class AnimationInterpolation {
    Step,      ///< Hold a value until the following keyframe.
    Linear,    ///< Change at constant speed.
    EaseIn,    ///< Accelerate away from the previous keyframe.
    EaseOut,   ///< Decelerate towards the following keyframe.
    EaseInOut, ///< Smooth both ends of the interval.
    Cubic      ///< Apply Hermite tangents for Float tracks.
};
/** @brief Playback behavior at the end of a clip. */
enum class AnimationLoop {
    Once,         ///< Play once and hold the final value.
    Loop,         ///< Repeat from the beginning.
    PingPong,     ///< Alternate forward and reverse playback.
    ClampForever  ///< Hold the final value indefinitely.
};
/** @brief Duration unit used by an Animator transition. */
enum class AnimationDurationMode {
    Normalized, ///< Fraction of the source clip duration.
    Seconds     ///< Absolute seconds, independent of source length.
};
/** @brief States whose transitions may interrupt an active blend. */
enum class AnimationInterruptionSource {
    None,            ///< Do not interrupt an active transition.
    CurrentState,    ///< Consider transitions from the outgoing state.
    NextState,       ///< Consider transitions from the incoming state.
    CurrentThenNext, ///< Search outgoing, then incoming state transitions.
    NextThenCurrent  ///< Search incoming, then outgoing state transitions.
};
/** @brief The mode used to combine an animation layer with earlier layers. */
enum class AnimationLayerMode {
    Override, ///< Apply weighted output in place of the prior pose.
    Additive  ///< Reserved for future parallel layer composition.
};
/** @brief How a sprite changes while two states blend. */
enum class AnimationSpriteBlend {
    Switch,   ///< Swap the sprite at the transition midpoint.
    CrossFade ///< Composite both sprites with opposing alpha weights.
};
/** @brief How a property behaves when its clip ceases to control it. */
enum class AnimationExitBehavior {
    UseNextState,    ///< Destination controls its tracks; other tracks keep their values.
    KeepLastValue,   ///< Retain the last sampled values when leaving the clip.
    RestoreOriginal  ///< Restore the initial values of tracks absent in the destination.
};

/** @brief One typed property value at a time in seconds. */
struct AnimationKeyframe {
    float time = 0.f; ///< Seconds since the beginning of the clip.
    AnimationValue value = 0.f; ///< Type must match the registered property.
    AnimationInterpolation interpolation = AnimationInterpolation::Linear; ///< Outgoing interpolation.
    float outTangent = 0.f; ///< Cubic Hermite slope leaving this key.
    float inTangent = 0.f; ///< Cubic Hermite slope arriving at the next key.
};

/** @brief Timeline driving one property on the Animator object or one of its children. */
struct AnimationTrack {
    std::string target = "Self"; ///< Self or a slash-delimited descendant-name path.
    std::string component; ///< Registered component type, e.g. Transform.
    std::string property; ///< Registered animatable property.
    unsigned componentIndex = 0; ///< Selects one of several components of the same type.
    std::vector<AnimationKeyframe> keyframes; ///< Strictly sorted, nonnegative times.

    /** @brief Samples a timeline without touching components.
     * @param seconds Absolute position in the clip.
     * @return Interpolated typed property value.
     */
    AnimationValue Sample(float seconds) const;
};

/** @brief One cropped frame in pixel coordinates and its duration. */
struct AnimationSpriteFrame {
    RectI source{}; ///< Rectangle within the sprite texture.
    float duration = 0.1f; ///< Seconds, strictly positive.
};

/** @brief Sprite sequence generated from a rectangular sprite sheet. */
struct AnimationSpriteSheet {
    std::string texture; ///< AssetManager-relative texture path.
    std::string target = "Self"; ///< Object containing the SpriteRenderer.
    unsigned componentIndex = 0; ///< SpriteRenderer instance.
    int columns = 0; ///< Number of columns; zero means use cellWidth.
    int rows = 0; ///< Number of rows; zero means use cellHeight.
    int cellWidth = 0; ///< Cell width in pixels, alternatively to columns.
    int cellHeight = 0; ///< Cell height in pixels, alternatively to rows.
    int startColumn = 0; ///< Inclusive first column.
    int startRow = 0; ///< Inclusive first row.
    int endColumn = 0; ///< Inclusive final column.
    int endRow = 0; ///< Inclusive final row.
    bool vertical = false; ///< Column-major when true; row-major otherwise.
    float frameDuration = 0.1f; ///< Default duration of every generated frame.
    std::vector<float> frameDurations; ///< Optional per-frame overrides.

    /** @brief Expand a sheet into validated source rectangles using texture dimensions.
     * @param textureWidth Width of the loaded texture, in pixels.
     * @param textureHeight Height of the loaded texture, in pixels.
     * @param error Output diagnostic; cleared on success.
     * @return Empty vector and nonempty error when dimensions or cell order are invalid.
     */
    std::vector<AnimationSpriteFrame> BuildFrames(int textureWidth, int textureHeight,
                                                   std::string& error) const;
};

/** @brief Named event dispatched when playback crosses a position in a clip. */
struct AnimationEvent {
    float time = 0.f; ///< Event position in seconds.
    std::string name; ///< Developer-defined event identifier.
};

/** @brief Immutable animation asset containing property timelines and/or sprite frames. */
struct AnimationClip {
    std::string name; ///< Display and diagnostic name.
    float duration = 0.f; ///< Explicit duration, or derived from frames/tracks when zero.
    float speed = 1.f; ///< Nonnegative playback multiplier.
    AnimationLoop loop = AnimationLoop::Loop; ///< Repetition and final-pose behavior.
    AnimationExitBehavior exitBehavior = AnimationExitBehavior::UseNextState; ///< Handling of abandoned properties.
    std::vector<AnimationTrack> tracks; ///< Independently sampled component properties.
    std::optional<AnimationSpriteSheet> spritesheet; ///< Optional frame sequence from a sprite atlas.
    std::vector<AnimationEvent> events; ///< Time-ordered named callbacks.

    /** @brief Effective duration in seconds, before state and Animator speed multipliers. */
    float Duration() const noexcept;
    /** @brief Parse and validate a versioned animation asset; never touches disk.
     * @param text UTF-8 JSON document.
     * @param error Output diagnostic; cleared on success.
     * @return Validated asset on success; empty optional on failure.
     */
    static std::optional<AnimationClip> FromJson(std::string_view text, std::string& error);
};

/** @brief Parameter kinds accepted by transition conditions. */
enum class AnimatorParameterType {
    Bool,   ///< Boolean state retained until changed.
    Int,    ///< Integer threshold and equality comparisons.
    Float,  ///< Floating-point threshold and equality comparisons.
    Trigger ///< One-shot Boolean consumed by a transition.
};
/** @brief Comparison operators; a trigger only supports Triggered. */
enum class AnimatorComparison {
    Equal,        ///< Equality comparison.
    NotEqual,     ///< Inequality comparison.
    Greater,      ///< Strictly greater than the threshold.
    Less,         ///< Strictly less than the threshold.
    GreaterEqual, ///< Greater than or equal to the threshold.
    LessEqual,    ///< Less than or equal to the threshold.
    Triggered     ///< A pending Trigger parameter is set.
};

/** @brief Parameter value and declaration shared by all instances of a controller. */
struct AnimatorParameter {
    std::string name; ///< Unique name in the controller.
    AnimatorParameterType type = AnimatorParameterType::Bool; ///< Expected parameter type.
    std::variant<bool, int, float> defaultValue = false; ///< Initial value of each Animator instance.
};

/** @brief One condition evaluated against the Animator's runtime parameters. */
struct AnimatorCondition {
    std::string parameter; ///< Referenced controller parameter name.
    AnimatorComparison comparison = AnimatorComparison::Equal; ///< Predicate operator.
    std::variant<bool, int, float> value = false; ///< Comparison constant.
};

/** @brief State-machine edge with normalized/absolute duration, exit time and interruption policy. */
struct AnimatorTransition {
    std::string from; ///< State name or AnyState.
    std::string to; ///< Destination state name.
    bool hasExitTime = false; ///< Require source playhead to cross exitTime.
    float exitTime = 1.f; ///< Normalized progress; values over 1 target later loops.
    AnimationDurationMode durationMode = AnimationDurationMode::Normalized; ///< Duration unit selector.
    float duration = 0.1f; ///< Seconds or fraction of source clip duration.
    float offset = 0.f; ///< Normalized destination clip start position.
    AnimationInterpolation blendCurve = AnimationInterpolation::EaseInOut; ///< Easing for transition weight.
    AnimationInterruptionSource interruptionSource = AnimationInterruptionSource::None; ///< Interruption search policy.
    bool orderedInterruption = true; ///< Only earlier/higher-priority edges may interrupt.
    AnimationSpriteBlend spriteBlend = AnimationSpriteBlend::Switch; ///< Sprite blending policy.
    bool canTransitionToSelf = false; ///< Permit a state to transition to itself.
    std::vector<AnimatorCondition> conditions; ///< All conditions must match (AND).
};

/** @brief Named state referencing an AnimationClip asset. */
struct AnimatorState {
    std::string name; ///< Unique name referenced by edges.
    std::string clip; ///< AssetManager-relative path to the clip JSON.
    float speed = 1.f; ///< Per-state playback speed multiplier.
};

/** @brief Runtime layer definition; optional property masks use target paths. */
struct AnimatorLayer {
    std::string name = "Base"; ///< Layer identifier displayed in authoring tools.
    AnimationLayerMode mode = AnimationLayerMode::Override; ///< Composition policy of the layer.
    float weight = 1.f; ///< Contribution in the range [0,1].
    std::vector<std::string> mask; ///< Empty means every property is permitted.
};

/** @brief Shared controller asset; runtime parameters and playback never live here. */
struct AnimatorController {
    std::string entryState; ///< State entered when the controller loads.
    std::vector<AnimatorParameter> parameters; ///< Parameter schema and defaults.
    std::vector<AnimatorState> states; ///< States and clip references.
    std::vector<AnimatorTransition> transitions; ///< Edges in priority order.
    std::vector<AnimatorLayer> layers; ///< Optional layer definitions (base Override is supported).

    /** @brief Find a state by its stable user-defined name.
     * @param name State name.
     * @return Pointer to the matching state or nullptr.
     */
    const AnimatorState* FindState(std::string_view name) const noexcept;
    /** @brief Parse a versioned controller and validate names and state edges.
     * @param text UTF-8 JSON document.
     * @param error Output diagnostic; cleared on success.
     * @return Validated controller or empty optional on failure.
     */
    static std::optional<AnimatorController> FromJson(std::string_view text, std::string& error);
};

/** @brief Blend two values of the same registered type; discrete values switch at weight 0.5.
 * @param from Value of the source state.
 * @param to Value of the destination state.
 * @param weight Blend weight in [0,1].
 * @return Interpolated or selected variant value.
 */
AnimationValue BlendAnimationValues(const AnimationValue& from, const AnimationValue& to, float weight);
/** @brief Map transition progress to the selected easing curve.
 * @param curve Easing mode selected by the transition.
 * @param progress Fraction of the transition duration elapsed.
 * @return Clamped blend weight in [0,1].
 */
float AnimationBlendWeight(AnimationInterpolation curve, float progress) noexcept;

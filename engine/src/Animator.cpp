#include "Animator.h"

#include "AssetManager.h"
#include "ErrorHandler.h"
#include "GameObject.h"
#include "Scene.h"
#include "SpriteRenderer.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr float kEpsilon = 0.00001f;

std::string TrackKey(const AnimationTrack& track)
{
    return track.target + '\x1f' + track.component + '\x1f' +
           std::to_string(track.componentIndex) + '\x1f' + track.property;
}

bool IsValidNumber(float value) { return std::isfinite(value) && value >= 0.f; }

float WrappedSeconds(const AnimationClip& clip, float elapsed, float length)
{
    if (length <= 0.f) return 0.f;
    switch (clip.loop) {
    case AnimationLoop::Loop:
        return std::fmod(elapsed, length);
    case AnimationLoop::PingPong: {
        const float phase = std::fmod(elapsed, length * 2.f);
        return phase <= length ? phase : 2.f*length - phase;
    }
    case AnimationLoop::ClampForever:
    case AnimationLoop::Once:
        return std::min(elapsed, length);
    }
    return 0.f;
}

GameObject* FindChild(GameObject& owner, std::string_view path)
{
    if (path.empty() || path == "Self" || path == ".") return &owner;
    Scene* scene = owner.scene;
    if (!scene) return nullptr;
    GameObject* cursor = &owner;
    while (!path.empty()) {
        const auto separator = path.find('/');
        const std::string name(path.substr(0, separator));
        auto* candidate = scene->Find(name);
        if (!candidate || static_cast<GameObject*>(candidate->parent) != cursor)
            return nullptr;
        cursor = candidate;
        if (separator == std::string_view::npos) break;
        path.remove_prefix(separator + 1);
    }
    return cursor;
}

bool EvaluateCondition(const AnimatorCondition& condition,
                       const std::variant<bool,int,float>& parameter)
{
    if (parameter.index() != condition.value.index()) return false;
    if (condition.comparison == AnimatorComparison::Triggered)
        return std::get<bool>(parameter);
    const auto compare = [&condition](auto left, auto right) {
        switch (condition.comparison) {
        case AnimatorComparison::Equal: return left == right;
        case AnimatorComparison::NotEqual: return left != right;
        case AnimatorComparison::Greater: return left > right;
        case AnimatorComparison::Less: return left < right;
        case AnimatorComparison::GreaterEqual: return left >= right;
        case AnimatorComparison::LessEqual: return left <= right;
        case AnimatorComparison::Triggered: return false;
        }
        return false;
    };
    return std::visit([&](auto left) {
        using T = decltype(left);
        return compare(left, std::get<T>(condition.value));
    }, parameter);
}
} // namespace

bool Animator::SetController(const std::string& assetPath)
{
    auto* assets = AssetManager::GetInstancePtr();
    if (!assets) return false;
    return SetController(assets->LoadAnimatorController(assetPath));
}

bool Animator::SetController(const AnimatorController* controller)
{
    if (!controller) return false;
    if (controller->layers.size() > 1 ||
        (!controller->layers.empty() && controller->layers.front().mode == AnimationLayerMode::Additive)) {
        Console.Warning("Parallel/additive Animator layers are not supported yet", "Animator");
        return false;
    }
    controller_ = controller;
    parameters_.clear();
    bindings_.clear();
    originals_.clear();
    blend_.reset();
    for (const auto& param : controller->parameters)
        parameters_.emplace(param.name, param.defaultValue);
    return Play(controller->entryState);
}

bool Animator::MakePlayhead(std::string_view name, float normalizedTime, Playhead& output)
{
    if (!controller_ || !IsValidNumber(normalizedTime) || normalizedTime > 1.f)
        return false;
    const auto* state = controller_->FindState(name);
    auto* assets = AssetManager::GetInstancePtr();
    if (!state || !assets) return false;
    const auto* clip = assets->LoadAnimation(state->clip);
    if (!clip) {
        Console.Error("Cannot load clip: " + state->clip, "Animator");
        return false;
    }
    float duration = clip->Duration();
    if (clip->spritesheet) {
        const auto& sheet = *clip->spritesheet;
        const auto* texture = assets->LoadTexture(sheet.texture);
        if (!texture) return false;
        std::string error;
        const auto frames = sheet.BuildFrames(texture->Width(), texture->Height(), error);
        if (frames.empty()) {
            Console.Warning("Invalid spritesheet for '" + state->name + "': " + error, "Animator");
            return false;
        }
        if (clip->duration <= 0.f) {
            float sheetDuration = 0.f;
            for (const auto& frame : frames) sheetDuration += frame.duration;
            duration = std::max(duration, sheetDuration);
        }
    }
    if (!std::isfinite(duration) || duration <= 0.f) return false;
    output = Playhead{state, clip, duration * normalizedTime, duration};
    return true;
}

bool Animator::Play(std::string_view state, float normalizedTime)
{
    Playhead next;
    if (!MakePlayhead(state, normalizedTime, next)) return false;
    if (current_.clip && current_.clip->exitBehavior == AnimationExitBehavior::RestoreOriginal)
        RestoreUncontrolled(next);
    current_ = next;
    blend_.reset();
    playing_ = true;
    return true;
}

bool Animator::CrossFade(std::string_view state, float durationSeconds, float normalizedOffset)
{
    if (!IsValidNumber(durationSeconds)) return false;
    Playhead destination;
    if (!MakePlayhead(state, normalizedOffset, destination)) return false;
    if (!current_.clip || durationSeconds == 0.f) return Play(state, normalizedOffset);
    blend_ = ActiveBlend{nullptr, current_, destination, 0.f, durationSeconds,
                         AnimationInterpolation::EaseInOut, AnimationSpriteBlend::CrossFade};
    playing_ = true;
    return true;
}

bool Animator::SetSpeed(float speed) noexcept
{
    if (!IsValidNumber(speed)) return false;
    speed_ = speed;
    return true;
}

std::string_view Animator::GetCurrentState() const noexcept
{
    return current_.state ? std::string_view(current_.state->name) : std::string_view{};
}

float Animator::GetNormalizedTime() const noexcept
{
    const float length = current_.duration;
    return length > 0.f ? current_.elapsed / length : 0.f;
}

bool Animator::SetParameter(std::string_view name, AnimatorParameterType type, ParameterValue value)
{
    if (!controller_) return false;
    const auto declaration = std::find_if(controller_->parameters.begin(), controller_->parameters.end(),
        [name, type](const auto& param) { return param.name == name && param.type == type; });
    if (declaration == controller_->parameters.end()) return false;
    parameters_[std::string(name)] = std::move(value);
    return true;
}

bool Animator::SetBool(std::string_view name, bool value)
{ return SetParameter(name, AnimatorParameterType::Bool, value); }
bool Animator::SetInt(std::string_view name, int value)
{ return SetParameter(name, AnimatorParameterType::Int, value); }
bool Animator::SetFloat(std::string_view name, float value)
{ return IsValidNumber(std::abs(value)) && SetParameter(name, AnimatorParameterType::Float, value); }
bool Animator::SetTrigger(std::string_view name)
{ return SetParameter(name, AnimatorParameterType::Trigger, true); }
bool Animator::ResetTrigger(std::string_view name)
{ return SetParameter(name, AnimatorParameterType::Trigger, false); }

unsigned Animator::OnEvent(EventCallback callback)
{
    if (!callback) return 0;
    const unsigned id = nextListenerId_++;
    listeners_.push_back({id, std::move(callback)});
    return id;
}

bool Animator::RemoveEventListener(unsigned id)
{
    const auto it = std::remove_if(listeners_.begin(), listeners_.end(),
        [id](const auto& listener) { return listener.id == id; });
    if (it == listeners_.end()) return false;
    listeners_.erase(it, listeners_.end());
    return true;
}

float Animator::SampleTime(const Playhead& head) noexcept
{
    return head.clip ? WrappedSeconds(*head.clip, head.elapsed, head.duration) : 0.f;
}

float Animator::ClipSpeed(const Playhead& head) noexcept
{
    return head.clip && head.state ? head.clip->speed * head.state->speed : 0.f;
}

void Animator::Advance(Playhead& head, float dt) noexcept
{
    if (!head.clip || dt <= 0.f) return;
    head.elapsed += dt * ClipSpeed(head);
    if (head.clip->loop == AnimationLoop::Once || head.clip->loop == AnimationLoop::ClampForever)
        head.elapsed = std::min(head.elapsed, head.duration);
    if (!std::isfinite(head.elapsed)) head.elapsed = 0.f;
}

void Animator::DispatchEvents(const Playhead& head, float previous, float current)
{
    if (!head.clip || listeners_.empty() || current <= previous) return;
    const float length = head.duration;
    if (length <= 0.f) return;
    // Bound the work after a debugger pause or an unexpectedly long frame.
    const float end = std::min(current, previous + length * 32.f);
    for (const auto& event : head.clip->events) {
        const int cycleStart = std::max(0, static_cast<int>(std::floor(previous / length)) - 1);
        const int cycleEnd = std::min(cycleStart + 34, static_cast<int>(std::ceil(end / length)) + 1);
        for (int cycle = cycleStart; cycle <= cycleEnd; ++cycle) {
            if (head.clip->loop != AnimationLoop::Loop &&
                head.clip->loop != AnimationLoop::PingPong && cycle > 0) break;
            const float absolute = cycle*length +
                ((head.clip->loop == AnimationLoop::PingPong && cycle % 2) ? length-event.time : event.time);
            if (absolute <= previous || absolute > end) continue;
            // Copy callbacks: listeners may unsubscribe while handling an event.
            auto callbacks = listeners_;
            for (const auto& listener : callbacks)
                if (listener.callback) listener.callback(event.name, head.state->name);
        }
    }
}

bool Animator::ConditionsPass(const AnimatorTransition& edge) const
{
    for (const auto& condition : edge.conditions) {
        const auto value = parameters_.find(condition.parameter);
        if (value == parameters_.end() || !EvaluateCondition(condition, value->second))
            return false;
    }
    return true;
}

bool Animator::ExitTimeCrossed(const AnimatorTransition& edge, const Playhead& head,
                              float previous) const noexcept
{
    if (!edge.hasExitTime) return true;
    if (!head.clip || head.duration <= 0.f) return false;
    const float before = previous / head.duration;
    const float after = head.elapsed / head.duration;
    if (edge.exitTime > 1.f || head.clip->loop == AnimationLoop::Once ||
        head.clip->loop == AnimationLoop::ClampForever)
        return before < edge.exitTime && after >= edge.exitTime;
    return std::floor(before - edge.exitTime) < std::floor(after - edge.exitTime);
}

const AnimatorTransition* Animator::FindTransition(const Playhead& head, float previous,
    const AnimatorTransition* interrupting) const
{
    if (!controller_ || !head.state || !head.clip) return nullptr;
    for (const auto& edge : controller_->transitions) {
        if (edge.from != "AnyState" && edge.from != head.state->name) continue;
        if (edge.to == head.state->name && !edge.canTransitionToSelf) continue;
        if (!ExitTimeCrossed(edge, head, previous) || !ConditionsPass(edge)) continue;
        if (interrupting && interrupting->orderedInterruption && &edge >= interrupting)
            continue;
        return &edge;
    }
    return nullptr;
}

bool Animator::BeginTransition(const AnimatorTransition& edge, const Playhead& source)
{
    Playhead destination;
    if (!MakePlayhead(edge.to, edge.offset, destination)) return false;
    const float duration = edge.durationMode == AnimationDurationMode::Normalized
        ? edge.duration * source.duration : edge.duration;
    if (duration <= kEpsilon) {
        if (source.clip->exitBehavior == AnimationExitBehavior::RestoreOriginal)
            RestoreUncontrolled(destination);
        current_ = destination;
        blend_.reset();
    } else {
        current_ = source;
        blend_ = ActiveBlend{&edge, source, destination, 0.f, duration,
                             edge.blendCurve, edge.spriteBlend};
    }
    // Consume only the triggers responsible for this specific transition.
    for (const auto& condition : edge.conditions)
        if (condition.comparison == AnimatorComparison::Triggered)
            parameters_[condition.parameter] = false;
    return true;
}

std::unordered_map<std::string, Animator::TrackPose> Animator::SamplePose(const Playhead& head)
{
    std::unordered_map<std::string, TrackPose> result;
    if (!head.clip || !gameObject) return result;
    const auto* layer = controller_ && !controller_->layers.empty() ? &controller_->layers.front() : nullptr;
    const float now = SampleTime(head);
    for (const auto& track : head.clip->tracks) {
        if (layer && !layer->mask.empty() &&
            std::find(layer->mask.begin(), layer->mask.end(), track.target) == layer->mask.end())
            continue;
        const auto key = TrackKey(track);
        auto& binding = bindings_[key];
        if (!binding.IsValid()) binding = AnimationPropertyRegistry::Bind(
            *gameObject, track.target, track.component, track.property, track.componentIndex);
        if (!binding.IsValid()) continue;
        if (!originals_.contains(key)) originals_.emplace(key, binding.read());
        result.emplace(key, TrackPose{track.Sample(now), binding});
    }
    return result;
}

void Animator::ApplyPose(const Playhead& from, const Playhead* to, float weight)
{
    auto oldPose = SamplePose(from);
    const float layerWeight = controller_ && !controller_->layers.empty()
        ? controller_->layers.front().weight : 1.f;
    if (!to) {
        for (auto& [key, pose] : oldPose) {
            const auto original = originals_.find(key);
            pose.binding.write(original == originals_.end() ? pose.value :
                BlendAnimationValues(original->second, pose.value, layerWeight));
        }
        return;
    }
    auto newPose = SamplePose(*to);
    const float mixedWeight = std::clamp(weight * layerWeight, 0.f, 1.f);
    for (auto& [key, pose] : oldPose) {
        const auto destination = newPose.find(key);
        if (destination == newPose.end()) {
            pose.binding.write(pose.value);
        } else {
            pose.binding.write(BlendAnimationValues(pose.value, destination->second.value, mixedWeight));
            newPose.erase(destination);
        }
    }
    for (auto& [key, pose] : newPose) {
        const auto initial = originals_.find(key);
        pose.binding.write(initial == originals_.end() ? pose.value :
            BlendAnimationValues(initial->second, pose.value, mixedWeight));
    }
}

void Animator::RestoreUncontrolled(const Playhead& next)
{
    if (!next.clip) return;
    std::unordered_map<std::string, bool> nextKeys;
    for (const auto& track : next.clip->tracks) nextKeys.emplace(TrackKey(track), true);
    for (const auto& [key, original] : originals_)
        if (!nextKeys.contains(key)) {
            const auto binding = bindings_.find(key);
            if (binding != bindings_.end() && binding->second.IsValid())
                binding->second.write(original);
        }
}

void Animator::ApplySprite(const Playhead& head, const Playhead* to, float weight,
                           AnimationSpriteBlend blendMode)
{
    if (!gameObject) return;
    const auto sheetFor = [](const Playhead& playback) -> const AnimationSpriteSheet* {
        return playback.clip && playback.clip->spritesheet ? &*playback.clip->spritesheet : nullptr;
    };
    const auto* currentSheet = sheetFor(head);
    const auto* nextSheet = to ? sheetFor(*to) : nullptr;
    if (!currentSheet && !nextSheet) return;

    const auto spriteFor = [this](const AnimationSpriteSheet* sheet) -> SpriteRenderer* {
        if (!sheet) return nullptr;
        auto* target = FindChild(*gameObject, sheet->target);
        if (!target) return nullptr;
        const auto all = target->GetComponents<SpriteRenderer>();
        return sheet->componentIndex < all.size() ? all[sheet->componentIndex] : nullptr;
    };
    const auto sample = [](const Playhead& head, const AnimationSpriteSheet& sheet,
                           const Texture*& texture, RectI& source) -> bool {
        auto* assets = AssetManager::GetInstancePtr();
        if (!assets) return false;
        texture = assets->LoadTexture(sheet.texture);
        if (!texture) return false;
        std::string problem;
        const auto frames = sheet.BuildFrames(texture->Width(), texture->Height(), problem);
        if (frames.empty()) return false;
        const float length = SampleTime(head);
        float cursor = length;
        size_t frameIndex = 0;
        while (frameIndex + 1 < frames.size() && cursor >= frames[frameIndex].duration) {
            cursor -= frames[frameIndex].duration;
            ++frameIndex;
        }
        source = frames[frameIndex].source;
        return true;
    };

    const bool chooseTo = nextSheet && (!currentSheet || weight >= 0.5f);
    const auto& selected = chooseTo ? *to : head;
    const auto* selectedSheet = chooseTo ? nextSheet : currentSheet;
    auto* renderer = spriteFor(selectedSheet);
    if (!renderer) return;
    const Texture* texture = nullptr;
    RectI source{};
    if (!sample(selected, *selectedSheet, texture, source)) return;
    renderer->sprite = texture;
    renderer->source = source;

    if (blendMode == AnimationSpriteBlend::CrossFade && currentSheet && nextSheet &&
        currentSheet->target == nextSheet->target &&
        currentSheet->componentIndex == nextSheet->componentIndex) {
        const Texture* fromTexture = nullptr;
        RectI fromSource{};
        const Texture* toTexture = nullptr;
        RectI toSource{};
        if (sample(head, *currentSheet, fromTexture, fromSource) &&
            sample(*to, *nextSheet, toTexture, toSource)) {
            renderer->sprite = toTexture;
            renderer->source = toSource;
            renderer->SetAnimationBlend(fromTexture, fromSource, weight);
            return;
        }
    }
    renderer->ClearAnimationBlend();
}

void Animator::LateUpdate(float dt)
{
    if (!playing_ || !current_.clip || !std::isfinite(dt) || dt < 0.f) return;
    const float scaled = dt * speed_;
    if (blend_) {
        auto& active = *blend_;
        const float previousFrom = active.from.elapsed;
        const float previousTo = active.to.elapsed;
        Advance(active.from, scaled);
        Advance(active.to, scaled);
        active.elapsed += scaled;
        DispatchEvents(active.from, previousFrom, active.from.elapsed);
        DispatchEvents(active.to, previousTo, active.to.elapsed);

        if (active.transition && active.transition->interruptionSource != AnimationInterruptionSource::None) {
            const auto policy = active.transition->interruptionSource;
            const auto searchFrom = [&] { return FindTransition(active.from, previousFrom, active.transition); };
            const auto searchTo = [&] { return FindTransition(active.to, previousTo, active.transition); };
            const AnimatorTransition* candidate = nullptr;
            Playhead origin;
            const bool firstNext = policy == AnimationInterruptionSource::NextState ||
                                   policy == AnimationInterruptionSource::NextThenCurrent;
            if (firstNext) {
                candidate = searchTo(); origin = active.to;
                if (!candidate && policy == AnimationInterruptionSource::NextThenCurrent) {
                    candidate = searchFrom(); origin = active.from;
                }
            } else {
                candidate = searchFrom(); origin = active.from;
                if (!candidate && policy == AnimationInterruptionSource::CurrentThenNext) {
                    candidate = searchTo(); origin = active.to;
                }
            }
            if (candidate && BeginTransition(*candidate, origin)) return;
        }

        const float progress = active.duration > 0.f ?
            std::min(1.f, active.elapsed / active.duration) : 1.f;
        const float weight = AnimationBlendWeight(active.curve, progress);
        ApplyPose(active.from, &active.to, weight);
        ApplySprite(active.from, &active.to, weight, active.spriteBlend);
        if (progress >= 1.f) {
            if (active.from.clip->exitBehavior == AnimationExitBehavior::RestoreOriginal)
                RestoreUncontrolled(active.to);
            current_ = active.to;
            blend_.reset();
        }
        return;
    }

    const float previous = current_.elapsed;
    Advance(current_, scaled);
    DispatchEvents(current_, previous, current_.elapsed);
    if (const auto* edge = FindTransition(current_, previous)) {
        if (BeginTransition(*edge, current_)) {
            if (blend_) {
                ApplyPose(blend_->from, &blend_->to, 0.f);
                ApplySprite(blend_->from, &blend_->to, 0.f, blend_->spriteBlend);
            } else {
                ApplyPose(current_, nullptr, 1.f);
                ApplySprite(current_, nullptr, 1.f, AnimationSpriteBlend::Switch);
            }
            return;
        }
    }
    ApplyPose(current_, nullptr, 1.f);
    ApplySprite(current_, nullptr, 1.f, AnimationSpriteBlend::Switch);
}

void Animator::OnDestroy()
{
    if (!gameObject) return;
    for (auto* renderer : gameObject->GetComponents<SpriteRenderer>())
        if (renderer) renderer->ClearAnimationBlend();
}

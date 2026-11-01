#include "Animation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_set>

namespace {
using Json = nlohmann::json;

template<class T> T Member(const Json& object, const char* key, T fallback)
{
    return object.contains(key) ? object.at(key).get<T>() : fallback;
}

float Positive(const Json& object, const char* key, float fallback, bool allowZero = false)
{
    const float value = Member(object, key, fallback);
    if (!std::isfinite(value) || value < 0.f || (!allowZero && value == 0.f))
        throw std::runtime_error(std::string("Invalid positive value: ") + key);
    return value;
}

template<class E> E ParseEnum(const std::string& text,
    std::initializer_list<std::pair<const char*, E>> choices)
{
    for (const auto& [name, value] : choices) if (text == name) return value;
    throw std::runtime_error("Unknown animation setting: " + text);
}

AnimationInterpolation ParseInterpolation(const std::string& name)
{
    return ParseEnum<AnimationInterpolation>(name, {
        {"Step", AnimationInterpolation::Step}, {"Linear", AnimationInterpolation::Linear},
        {"EaseIn", AnimationInterpolation::EaseIn}, {"EaseOut", AnimationInterpolation::EaseOut},
        {"EaseInOut", AnimationInterpolation::EaseInOut}, {"Cubic", AnimationInterpolation::Cubic}
    });
}

AnimationValue ParseValue(const Json& value, const std::string& type)
{
    if (type == "Bool" || (type.empty() && value.is_boolean())) return value.get<bool>();
    if (type == "Int" || (type.empty() && value.is_number_integer())) return value.get<int>();
    if (type == "Float" || (type.empty() && value.is_number())) return value.get<float>();
    if (type == "String" || (type.empty() && value.is_string())) return value.get<std::string>();
    if (type == "Vec2" || (type.empty() && value.is_array() && value.size() == 2))
        return Vec2{value.at(0).get<float>(), value.at(1).get<float>()};
    if (type == "Vec3" || (type.empty() && value.is_array() && value.size() == 3))
        return Vec3{value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
    if (type == "Color" && value.is_array() && value.size() == 4) {
        const auto channel = [&value](size_t i) {
            const int v = value.at(i).get<int>();
            if (v < 0 || v > 255) throw std::runtime_error("Color channel outside 0..255");
            return static_cast<uint8_t>(v);
        };
        return Color{channel(0), channel(1), channel(2), channel(3)};
    }
    if (type == "RectI" && value.is_array() && value.size() == 4)
        return RectI{value.at(0).get<int>(), value.at(1).get<int>(),
                     value.at(2).get<int>(), value.at(3).get<int>()};
    throw std::runtime_error("Unsupported animation value type: " + type);
}

bool FiniteValue(const AnimationValue& value)
{
    if (const auto* v = std::get_if<float>(&value)) return std::isfinite(*v);
    if (const auto* v = std::get_if<Vec2>(&value))
        return std::isfinite(v->x) && std::isfinite(v->y);
    if (const auto* v = std::get_if<Vec3>(&value))
        return std::isfinite(v->x) && std::isfinite(v->y) && std::isfinite(v->z);
    return true;
}

float Hermite(float a, float b, float tangent0, float tangent1, float t, float duration)
{
    const float t2 = t * t, t3 = t2 * t;
    return (2*t3 - 3*t2 + 1)*a + (t3 - 2*t2 + t)*(tangent0*duration) +
           (-2*t3 + 3*t2)*b + (t3 - t2)*(tangent1*duration);
}

int FrameIndex(const AnimationSpriteSheet& sheet, int col, int row, int columns, int rows)
{
    return sheet.vertical ? col * rows + row : row * columns + col;
}

std::pair<int,int> CellAt(const AnimationSpriteSheet& sheet, int index, int columns, int rows)
{
    return sheet.vertical ? std::pair{index / rows, index % rows}
                          : std::pair{index % columns, index / columns};
}

std::variant<bool,int,float> ParseParameterValue(const Json& data, AnimatorParameterType type)
{
    switch (type) {
    case AnimatorParameterType::Bool:
    case AnimatorParameterType::Trigger: return data.get<bool>();
    case AnimatorParameterType::Int: return data.get<int>();
    case AnimatorParameterType::Float: return data.get<float>();
    }
    return false;
}
} // namespace

float AnimationBlendWeight(AnimationInterpolation curve, float progress) noexcept
{
    const float t = std::clamp(progress, 0.f, 1.f);
    switch (curve) {
    case AnimationInterpolation::Step: return t >= 1.f ? 1.f : 0.f;
    case AnimationInterpolation::Linear: return t;
    case AnimationInterpolation::EaseIn: return t*t;
    case AnimationInterpolation::EaseOut: return 1.f-(1.f-t)*(1.f-t);
    case AnimationInterpolation::EaseInOut:
    case AnimationInterpolation::Cubic: return t*t*(3.f-2.f*t);
    }
    return t;
}

AnimationValue BlendAnimationValues(const AnimationValue& a, const AnimationValue& b, float weight)
{
    const float t = std::clamp(weight, 0.f, 1.f);
    if (a.index() != b.index()) return t < 0.5f ? a : b;
    if (const auto* x = std::get_if<float>(&a)) return *x + (std::get<float>(b)-*x)*t;
    if (const auto* x = std::get_if<Vec2>(&a)) {
        const auto y = std::get<Vec2>(b);
        return Vec2{x->x + (y.x-x->x)*t, x->y + (y.y-x->y)*t};
    }
    if (const auto* x = std::get_if<Vec3>(&a)) {
        const auto y = std::get<Vec3>(b);
        return Vec3{x->x + (y.x-x->x)*t, x->y + (y.y-x->y)*t, x->z + (y.z-x->z)*t};
    }
    if (const auto* x = std::get_if<Color>(&a)) {
        const Color y = std::get<Color>(b);
        const auto mix = [t](uint8_t l, uint8_t r) {
            return static_cast<uint8_t>(std::lround(l + (r - l)*t));
        };
        return Color{mix(x->r,y.r),mix(x->g,y.g),mix(x->b,y.b),mix(x->a,y.a)};
    }
    return t < 0.5f ? a : b;
}

AnimationValue AnimationTrack::Sample(float time) const
{
    if (keyframes.empty()) return 0.f;
    if (time <= keyframes.front().time) return keyframes.front().value;
    if (time >= keyframes.back().time) return keyframes.back().value;
    const auto upper = std::upper_bound(keyframes.begin(), keyframes.end(), time,
        [](float x, const AnimationKeyframe& key) { return x < key.time; });
    const auto& left = *(upper - 1);
    const auto& right = *upper;
    const float span = right.time - left.time;
    const float t = (time - left.time) / span;
    if (left.interpolation == AnimationInterpolation::Cubic) {
        if (const auto* a = std::get_if<float>(&left.value))
            if (const auto* b = std::get_if<float>(&right.value))
                return Hermite(*a, *b, left.outTangent, right.inTangent, t, span);
    }
    return BlendAnimationValues(left.value, right.value,
                                AnimationBlendWeight(left.interpolation, t));
}

std::vector<AnimationSpriteFrame> AnimationSpriteSheet::BuildFrames(
    int textureWidth, int textureHeight, std::string& error) const
{
    error.clear();
    std::vector<AnimationSpriteFrame> output;
    if (textureWidth <= 0 || textureHeight <= 0) { error = "Invalid sprite texture size"; return output; }
    const int countX = columns ? columns : cellWidth ? textureWidth / cellWidth : 0;
    const int countY = rows ? rows : cellHeight ? textureHeight / cellHeight : 0;
    if (countX <= 0 || countY <= 0 || countX > 4096 || countY > 4096 ||
        textureWidth % countX || textureHeight % countY ||
        (cellWidth && textureWidth / countX != cellWidth) ||
        (cellHeight && textureHeight / countY != cellHeight) ||
        startColumn < 0 || startRow < 0 || endColumn < 0 || endRow < 0 ||
        startColumn >= countX || endColumn >= countX ||
        startRow >= countY || endRow >= countY ||
        !std::isfinite(frameDuration) || frameDuration <= 0.f) {
        error = "Invalid spritesheet grid, cell size or duration";
        return output;
    }
    const int first = FrameIndex(*this, startColumn, startRow, countX, countY);
    const int last = FrameIndex(*this, endColumn, endRow, countX, countY);
    const int count = last - first + 1;
    if (count <= 0 || count > 4096 ||
        (!frameDurations.empty() && frameDurations.size() != static_cast<size_t>(count))) {
        error = "Invalid spritesheet range or frame duration count";
        return output;
    }
    const int w = textureWidth / countX, h = textureHeight / countY;
    output.reserve(count);
    for (int index = first; index <= last; ++index) {
        const auto [col, row] = CellAt(*this, index, countX, countY);
        const float duration = frameDurations.empty() ? frameDuration : frameDurations[index-first];
        if (!std::isfinite(duration) || duration <= 0.f) {
            error = "Invalid per-frame duration";
            return {};
        }
        output.push_back({RectI{col*w, row*h, w, h}, duration});
    }
    return output;
}

float AnimationClip::Duration() const noexcept
{
    if (duration > 0.f) return duration;
    float result = 0.f;
    for (const auto& track : tracks)
        if (!track.keyframes.empty()) result = std::max(result, track.keyframes.back().time);
    if (spritesheet) {
        const auto& sheet = *spritesheet;
        // A pixel-sized grid's full column/row count is only known after
        // loading the texture. Animator derives its exact length then.
        const int cols = sheet.columns ? sheet.columns : 1;
        const int rows = sheet.rows ? sheet.rows : 1;
        const bool completeGrid = sheet.columns > 0 && sheet.rows > 0;
        const int count = completeGrid
            ? std::max(0, FrameIndex(sheet, sheet.endColumn, sheet.endRow, cols, rows) -
                          FrameIndex(sheet, sheet.startColumn, sheet.startRow, cols, rows) + 1)
            : 1;
        if (!sheet.frameDurations.empty()) {
            float total = 0.f;
            for (float t : sheet.frameDurations) total += t;
            result = std::max(result, total);
        } else result = std::max(result, count * sheet.frameDuration);
    }
    return result;
}

std::optional<AnimationClip> AnimationClip::FromJson(std::string_view source, std::string& error)
{
    try {
        if (source.size() > 4*1024*1024) throw std::runtime_error("Animation exceeds 4 MiB");
        const Json json = Json::parse(source.begin(), source.end());
        if (!json.is_object() || json.value("version", 0) != 1)
            throw std::runtime_error("Unsupported animation format version");
        AnimationClip clip;
        clip.name = json.at("name").get<std::string>();
        if (clip.name.empty()) throw std::runtime_error("Animation name cannot be empty");
        clip.duration = Positive(json, "duration", 0.f, true);
        clip.speed = Positive(json, "speed", 1.f, true);
        clip.loop = ParseEnum<AnimationLoop>(json.value("loop", "Loop"), {
            {"Once",AnimationLoop::Once},{"Loop",AnimationLoop::Loop},
            {"PingPong",AnimationLoop::PingPong},{"ClampForever",AnimationLoop::ClampForever}});
        clip.exitBehavior = ParseEnum<AnimationExitBehavior>(json.value("exitBehavior", "UseNextState"), {
            {"UseNextState",AnimationExitBehavior::UseNextState},
            {"KeepLastValue",AnimationExitBehavior::KeepLastValue},
            {"RestoreOriginal",AnimationExitBehavior::RestoreOriginal}});

        for (const auto& item : json.value("tracks", Json::array())) {
            AnimationTrack track;
            track.target = item.value("target", "Self");
            track.component = item.at("component").get<std::string>();
            track.property = item.at("property").get<std::string>();
            track.componentIndex = item.value("componentIndex", 0u);
            const std::string type = item.value("type", "");
            for (const auto& key : item.at("keyframes")) {
                AnimationKeyframe frame;
                frame.time = Positive(key, "time", 0.f, true);
                frame.value = ParseValue(key.at("value"), type);
                frame.interpolation = ParseInterpolation(key.value("interpolation", "Linear"));
                frame.outTangent = Member(key, "outTangent", 0.f);
                frame.inTangent = Member(key, "inTangent", 0.f);
                if (!FiniteValue(frame.value) || !std::isfinite(frame.outTangent) ||
                    !std::isfinite(frame.inTangent)) throw std::runtime_error("Nonfinite keyframe value");
                if (!track.keyframes.empty() &&
                    (frame.time <= track.keyframes.back().time ||
                     frame.value.index() != track.keyframes.back().value.index()))
                    throw std::runtime_error("Keyframe times or value types mismatch");
                track.keyframes.push_back(std::move(frame));
            }
            if (track.component.empty() || track.property.empty() || track.keyframes.empty())
                throw std::runtime_error("Incomplete animation track");
            clip.tracks.push_back(std::move(track));
            if (clip.tracks.size() > 256) throw std::runtime_error("Too many animation tracks");
        }

        if (json.contains("spritesheet")) {
            const auto& item = json.at("spritesheet");
            AnimationSpriteSheet sheet;
            sheet.texture = item.at("texture").get<std::string>();
            sheet.target = item.value("target", "Self");
            sheet.componentIndex = item.value("componentIndex", 0u);
            const auto grid = item.value("grid", Json::object());
            sheet.columns = grid.value("columns", 0);
            sheet.rows = grid.value("rows", 0);
            sheet.cellWidth = grid.value("cellWidth", 0);
            sheet.cellHeight = grid.value("cellHeight", 0);
            const auto first = item.at("start");
            const auto last = item.at("end");
            if (!first.is_array() || !last.is_array() || first.size()!=2 || last.size()!=2)
                throw std::runtime_error("Sprite start/end must contain [column,row]");
            sheet.startColumn = first[0].get<int>(); sheet.startRow = first[1].get<int>();
            sheet.endColumn = last[0].get<int>(); sheet.endRow = last[1].get<int>();
            sheet.vertical = ParseEnum<bool>(item.value("direction", "Horizontal"), {
                {"Horizontal",false},{"Vertical",true}});
            sheet.frameDuration = Positive(item, "frameDuration", 0.1f);
            sheet.frameDurations = item.value("frameDurations", std::vector<float>{});
            if (sheet.texture.empty()) throw std::runtime_error("Spritesheet texture is missing");
            clip.spritesheet = std::move(sheet);
        }

        for (const auto& item : json.value("events", Json::array())) {
            AnimationEvent event;
            event.time = Positive(item, "time", 0.f, true);
            event.name = item.at("name").get<std::string>();
            if (event.name.empty()) throw std::runtime_error("Animation event name is empty");
            clip.events.push_back(std::move(event));
            if (clip.events.size() > 4096) throw std::runtime_error("Too many animation events");
        }
        std::sort(clip.events.begin(), clip.events.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        });
        if (clip.Duration() <= 0.f || !std::isfinite(clip.Duration()))
            throw std::runtime_error("Animation duration is zero or invalid");
        const bool fullGridKnown = !clip.spritesheet ||
            (clip.spritesheet->columns > 0 && clip.spritesheet->rows > 0);
        for (const auto& event : clip.events) if ((clip.duration > 0.f || fullGridKnown) &&
                                                 event.time > clip.Duration())
            throw std::runtime_error("Animation event exceeds clip duration");
        error.clear();
        return clip;
    } catch (const std::exception& ex) {
        error = ex.what();
        return std::nullopt;
    }
}

const AnimatorState* AnimatorController::FindState(std::string_view name) const noexcept
{
    for (const auto& state : states) if (state.name == name) return &state;
    return nullptr;
}

std::optional<AnimatorController> AnimatorController::FromJson(
    std::string_view source, std::string& error)
{
    try {
        if (source.size() > 4*1024*1024) throw std::runtime_error("Controller exceeds 4 MiB");
        const Json json = Json::parse(source.begin(), source.end());
        if (!json.is_object() || json.value("version", 0) != 1)
            throw std::runtime_error("Unsupported AnimatorController version");
        AnimatorController result;
        result.entryState = json.at("entry").get<std::string>();
        std::unordered_set<std::string> stateNames, parameterNames;
        for (const auto& item : json.value("parameters", Json::array())) {
            AnimatorParameter parameter;
            parameter.name = item.at("name").get<std::string>();
            parameter.type = ParseEnum<AnimatorParameterType>(item.at("type").get<std::string>(), {
                {"Bool",AnimatorParameterType::Bool},{"Int",AnimatorParameterType::Int},
                {"Float",AnimatorParameterType::Float},{"Trigger",AnimatorParameterType::Trigger}});
            if (parameter.name.empty() || !parameterNames.insert(parameter.name).second)
                throw std::runtime_error("Duplicate or empty parameter name");
            if (item.contains("default"))
                parameter.defaultValue = ParseParameterValue(item.at("default"), parameter.type);
            else if (parameter.type == AnimatorParameterType::Float) parameter.defaultValue = 0.f;
            else if (parameter.type == AnimatorParameterType::Int) parameter.defaultValue = 0;
            result.parameters.push_back(std::move(parameter));
        }
        for (const auto& item : json.at("states")) {
            AnimatorState state;
            state.name = item.at("name").get<std::string>();
            state.clip = item.at("clip").get<std::string>();
            state.speed = Positive(item, "speed", 1.f, true);
            if (state.name.empty() || state.clip.empty() || !stateNames.insert(state.name).second)
                throw std::runtime_error("Duplicate or incomplete Animator state");
            result.states.push_back(std::move(state));
            if (result.states.size() > 512) throw std::runtime_error("Too many Animator states");
        }
        if (!result.FindState(result.entryState)) throw std::runtime_error("Entry state does not exist");
        for (const auto& item : json.value("transitions", Json::array())) {
            AnimatorTransition transition;
            transition.from = item.at("from").get<std::string>();
            transition.to = item.at("to").get<std::string>();
            transition.hasExitTime = item.value("hasExitTime", false);
            transition.exitTime = Positive(item, "exitTime", 1.f, true);
            transition.durationMode = ParseEnum<AnimationDurationMode>(item.value("durationMode", "Normalized"), {
                {"Normalized",AnimationDurationMode::Normalized},{"Seconds",AnimationDurationMode::Seconds}});
            transition.duration = Positive(item, "duration", 0.1f, true);
            transition.offset = Positive(item, "offset", 0.f, true);
            transition.blendCurve = ParseInterpolation(item.value("blendCurve", "EaseInOut"));
            transition.interruptionSource = ParseEnum<AnimationInterruptionSource>(
                item.value("interruptionSource", "None"), {
                {"None",AnimationInterruptionSource::None},
                {"CurrentState",AnimationInterruptionSource::CurrentState},
                {"NextState",AnimationInterruptionSource::NextState},
                {"CurrentThenNext",AnimationInterruptionSource::CurrentThenNext},
                {"NextThenCurrent",AnimationInterruptionSource::NextThenCurrent}});
            transition.orderedInterruption = item.value("orderedInterruption", true);
            transition.spriteBlend = ParseEnum<AnimationSpriteBlend>(item.value("spriteBlend", "Switch"), {
                {"Switch",AnimationSpriteBlend::Switch},{"CrossFade",AnimationSpriteBlend::CrossFade}});
            transition.canTransitionToSelf = item.value("canTransitionToSelf", false);
            for (const auto& condition : item.value("conditions", Json::array())) {
                AnimatorCondition c;
                c.parameter = condition.at("parameter").get<std::string>();
                c.comparison = ParseEnum<AnimatorComparison>(condition.value("comparison", "Equal"), {
                    {"Equal",AnimatorComparison::Equal},{"NotEqual",AnimatorComparison::NotEqual},
                    {"Greater",AnimatorComparison::Greater},{"Less",AnimatorComparison::Less},
                    {"GreaterEqual",AnimatorComparison::GreaterEqual},
                    {"LessEqual",AnimatorComparison::LessEqual},
                    {"Triggered",AnimatorComparison::Triggered}});
                const auto declaration = std::find_if(result.parameters.begin(), result.parameters.end(),
                    [&c](const auto& parameter) { return parameter.name == c.parameter; });
                if (declaration == result.parameters.end())
                    throw std::runtime_error("Transition references an unknown parameter");
                if (declaration->type == AnimatorParameterType::Trigger) {
                    if (c.comparison != AnimatorComparison::Triggered)
                        throw std::runtime_error("Triggers require the Triggered comparison");
                    c.value = true;
                } else {
                    if (!condition.contains("value")) throw std::runtime_error("Missing condition value");
                    c.value = ParseParameterValue(condition.at("value"), declaration->type);
                }
                transition.conditions.push_back(std::move(c));
            }
            if ((transition.from != "AnyState" && !result.FindState(transition.from)) ||
                !result.FindState(transition.to) || transition.offset >= 1.f ||
                (transition.from == "AnyState" && transition.conditions.empty()) ||
                (!transition.hasExitTime && transition.conditions.empty()) ||
                (transition.from == transition.to && !transition.canTransitionToSelf))
                throw std::runtime_error("Invalid Animator transition source, destination or conditions");
            result.transitions.push_back(std::move(transition));
            if (result.transitions.size() > 4096) throw std::runtime_error("Too many Animator transitions");
        }
        for (const auto& item : json.value("layers", Json::array())) {
            AnimatorLayer layer;
            layer.name = item.value("name", "Base");
            layer.mode = ParseEnum<AnimationLayerMode>(item.value("mode", "Override"), {
                {"Override",AnimationLayerMode::Override},{"Additive",AnimationLayerMode::Additive}});
            layer.weight = Positive(item, "weight", 1.f, true);
            if (layer.weight > 1.f) throw std::runtime_error("Layer weight exceeds 1");
            layer.mask = item.value("mask", std::vector<std::string>{});
            result.layers.push_back(std::move(layer));
        }
        if (result.layers.empty()) result.layers.emplace_back();
        error.clear();
        return result;
    } catch (const std::exception& ex) {
        error = ex.what();
        return std::nullopt;
    }
}

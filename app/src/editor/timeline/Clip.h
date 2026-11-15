#pragma once

#include "core/ids/Ids.h"
#include "core/time/TimelineTime.h"
#include "editor/animation/AnimationChannel.h"
#include <string>
#include <optional>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace catchim::editor {

enum class ClipType {
    Video,
    Audio,
    Text,
    Image,
    Sticker,
    Graphic,
    Effect
};

inline const char* clipTypeToString(ClipType type) {
    switch (type) {
        case ClipType::Video: return "video";
        case ClipType::Audio: return "audio";
        case ClipType::Text: return "text";
        case ClipType::Image: return "image";
        case ClipType::Sticker: return "sticker";
        case ClipType::Graphic: return "graphic";
        case ClipType::Effect: return "effect";
    }
    return "video";
}

inline ClipType stringToClipType(std::string_view sv) {
    if (sv == "audio") return ClipType::Audio;
    if (sv == "text") return ClipType::Text;
    if (sv == "image") return ClipType::Image;
    if (sv == "sticker") return ClipType::Sticker;
    if (sv == "graphic") return ClipType::Graphic;
    if (sv == "effect") return ClipType::Effect;
    return ClipType::Video;
}

struct MaskInstance {
    std::string id;
    std::string type{"freeform"};
    nlohmann::json params = nlohmann::json::object();

    bool operator==(const MaskInstance& other) const = default;
};

struct EffectInstance {
    std::string id;
    std::string type{"blur"};
    nlohmann::json params = nlohmann::json::object();
    bool enabled{true};

    bool operator==(const EffectInstance& other) const = default;
};

class Clip {
public:
    Clip(
        core::ClipId id,
        ClipType type,
        std::string name,
        core::TimelineTime startTime,
        core::TimelineTime duration,
        core::TimelineTime trimStart = core::TimelineTime(0),
        core::TimelineTime trimEnd = core::TimelineTime(0)
    );

    const core::ClipId& id() const noexcept { return id_; }
    void setId(core::ClipId id) noexcept { id_ = std::move(id); }
    ClipType type() const noexcept { return type_; }
    const std::string& name() const noexcept { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    core::TimelineTime startTime() const noexcept { return startTime_; }
    void setStartTime(core::TimelineTime time) noexcept { startTime_ = time; }

    core::TimelineTime duration() const noexcept { return duration_; }
    void setDuration(core::TimelineTime dur) noexcept { duration_ = dur; }

    core::TimelineTime endTime() const noexcept { return startTime_ + duration_; }

    core::TimelineTime trimStart() const noexcept { return trimStart_; }
    void setTrimStart(core::TimelineTime trim) noexcept { trimStart_ = trim; }

    core::TimelineTime trimEnd() const noexcept { return trimEnd_; }
    void setTrimEnd(core::TimelineTime trim) noexcept { trimEnd_ = trim; }

    const std::optional<core::TimelineTime>& sourceDuration() const noexcept { return sourceDuration_; }
    void setSourceDuration(std::optional<core::TimelineTime> dur) noexcept { sourceDuration_ = dur; }

    const core::MediaId& mediaId() const noexcept { return mediaId_; }
    void setMediaId(core::MediaId id) noexcept { mediaId_ = std::move(id); }

    bool isHidden() const noexcept { return hidden_; }
    void setHidden(bool hidden) noexcept { hidden_ = hidden; }

    bool isMuted() const noexcept { return muted_; }
    void setMuted(bool muted) noexcept { muted_ = muted; }

    // Parameter dictionary (Transform, Opacity, Text, Audio, etc.)
    const nlohmann::json& params() const noexcept { return params_; }
    nlohmann::json& params() noexcept { return params_; }
    void setParams(nlohmann::json p) { params_ = std::move(p); }

    bool hasParam(const std::string& key) const noexcept {
        return params_.contains(key);
    }

    template <typename T>
    T getParam(const std::string& key, const T& defaultValue) const {
        if (params_.contains(key)) {
            try {
                return params_[key].get<T>();
            } catch (...) {}
        }
        return defaultValue;
    }

    template <typename T>
    void setParam(const std::string& key, const T& value) {
        params_[key] = value;
    }

    // Animation channels
    const std::unordered_map<std::string, AnimationChannel>& animationChannels() const noexcept { return animationChannels_; }
    std::unordered_map<std::string, AnimationChannel>& animationChannels() noexcept { return animationChannels_; }

    const AnimationChannel* findAnimationChannel(const std::string& property) const;
    AnimationChannel* findAnimationChannel(const std::string& property);
    AnimationChannel& getOrCreateAnimationChannel(const std::string& property, double defaultValue = 0.0);
    bool hasAnimationChannel(const std::string& property) const;
    bool removeAnimationChannel(const std::string& property);

    // Masks
    const std::vector<MaskInstance>& masks() const noexcept { return masks_; }
    std::vector<MaskInstance>& masks() noexcept { return masks_; }
    void setMasks(std::vector<MaskInstance> m) { masks_ = std::move(m); }

    // Effects
    const std::vector<EffectInstance>& effects() const noexcept { return effects_; }
    std::vector<EffectInstance>& effects() noexcept { return effects_; }
    void setEffects(std::vector<EffectInstance> e) { effects_ = std::move(e); }

    // Clone helper
    Clip clone(core::ClipId newId) const;

private:
    core::ClipId id_;
    ClipType type_;
    std::string name_;
    core::TimelineTime startTime_;
    core::TimelineTime duration_;
    core::TimelineTime trimStart_;
    core::TimelineTime trimEnd_;
    std::optional<core::TimelineTime> sourceDuration_{std::nullopt};
    core::MediaId mediaId_{core::MediaId::empty()};
    bool hidden_{false};
    bool muted_{false};
    nlohmann::json params_ = nlohmann::json::object();
    std::unordered_map<std::string, AnimationChannel> animationChannels_;
    std::vector<MaskInstance> masks_;
    std::vector<EffectInstance> effects_;
};

} // namespace catchim::editor

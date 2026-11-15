#pragma once

#include "editor/timeline/Timeline.h"
#include "editor/timeline/Clip.h"
#include "core/time/TimelineTime.h"
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <nlohmann/json.hpp>

namespace catchim::editor {

enum class DragPayloadType {
    Media,
    Text,
    Sticker,
    Graphic,
    Effect
};

struct MediaDragPayload {
    std::string id;
    std::string name;
    ClipType mediaType{ClipType::Video};
    core::TimelineTime duration{core::TimelineTime::fromSeconds(5.0)};
    bool hasAudio{false};
    bool hasVideo{true};
    std::string filePath{""};
};

struct TextDragPayload {
    std::string id;
    std::string name{"Text"};
    std::string content{"Default Text"};
};

struct StickerDragPayload {
    std::string id;
    std::string name{"Sticker"};
    std::string stickerId{""};
};

struct GraphicDragPayload {
    std::string id;
    std::string name{"Graphic"};
    std::string definitionId{""};
    nlohmann::json params = nlohmann::json::object();
};

struct EffectDragPayload {
    std::string id;
    std::string name{"Effect"};
    std::string effectType{"blur"};
    std::vector<ClipType> targetElementTypes{ClipType::Video, ClipType::Image};
};

using TimelineDragData = std::variant<
    MediaDragPayload,
    TextDragPayload,
    StickerDragPayload,
    GraphicDragPayload,
    EffectDragPayload
>;

class TimelineDragSource {
public:
    TimelineDragSource() = default;

    void begin(TimelineDragData data) {
        active_ = std::move(data);
    }

    void end() noexcept {
        active_.reset();
    }

    [[nodiscard]] bool isActive() const noexcept {
        return active_.has_value();
    }

    [[nodiscard]] const TimelineDragData* getActive() const noexcept {
        return active_.has_value() ? &active_.value() : nullptr;
    }

    [[nodiscard]] DragPayloadType getActiveType() const;

    /**
     * @brief Checks if the drag payload can be dropped onto a specific track.
     */
    static bool canDropOnTrack(const Track& track, const TimelineDragData& data) noexcept;

    /**
     * @brief Converts drag data to a timeline clip and inserts it onto the specified track.
     */
    static std::optional<core::ClipId> resolveDropOnTrack(
        Timeline& timeline,
        const core::TrackId& trackId,
        core::TimelineTime dropTime,
        const TimelineDragData& data
    );

    /**
     * @brief Checks if the drag payload can be dropped directly onto a clip (e.g. effect or mask).
     */
    static bool canDropOnClip(const Clip& targetClip, const TimelineDragData& data) noexcept;

    /**
     * @brief Attaches dropped effect or parameters onto a clip.
     */
    static bool resolveDropOnClip(Clip& targetClip, const TimelineDragData& data);

private:
    std::optional<TimelineDragData> active_{std::nullopt};
};

} // namespace catchim::editor

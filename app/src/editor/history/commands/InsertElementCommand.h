#pragma once

#include "editor/history/Command.h"
#include "editor/timeline/Timeline.h"
#include "editor/project/Project.h"
#include <optional>
#include <string>

namespace catchim::editor {

struct InsertElementPlacement {
    enum class Mode {
        Explicit,
        Auto,
        NewTrack
    };

    Mode mode{Mode::Auto};
    core::TrackId explicitTrackId{core::TrackId::empty()};
    TrackType autoTrackType{TrackType::Video};
    core::TimelineTime startTime{0};
};

/**
 * @brief Command to insert an element into the timeline, with auto track placement
 * and optional automatic project canvas resolution & frame rate adaptation.
 * Corresponds to web/src/commands/timeline/element/insert-element.ts.
 */
class InsertElementCommand : public EditorCommand {
public:
    InsertElementCommand(
        Timeline& timeline,
        Clip element,
        InsertElementPlacement placement,
        Project* project = nullptr
    );

    bool execute() override;
    bool undo() override;
    [[nodiscard]] std::string name() const override { return "Insert Element"; }

    const core::ClipId& insertedClipId() const noexcept { return insertedClipId_; }
    const core::TrackId& targetTrackId() const noexcept { return targetTrackId_; }

private:
    Timeline& timeline_;
    Clip element_;
    InsertElementPlacement placement_;
    Project* project_{nullptr};

    core::ClipId insertedClipId_;
    core::TrackId targetTrackId_{core::TrackId::empty()};
    core::TrackId createdTrackId_{core::TrackId::empty()};

    std::optional<TimelineTracksSnapshot> savedState_{std::nullopt};
    std::optional<ProjectSettings> savedSettings_{std::nullopt};
};

} // namespace catchim::editor

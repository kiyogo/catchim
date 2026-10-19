#pragma once

#include "editor/history/Command.h"
#include "editor/timeline/Timeline.h"
#include "editor/timeline/ElementUtils.h"
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace catchim::editor {

struct ElementPatch {
    core::TrackId trackId;
    core::ClipId clipId;
    nlohmann::json patchParams{nlohmann::json::object()};
    std::optional<bool> muted;
    std::optional<bool> hidden;
    std::optional<std::string> name;

    ElementPatch() = default;
    ElementPatch(core::TrackId tid, core::ClipId cid, nlohmann::json patch)
        : trackId(std::move(tid)), clipId(std::move(cid)), patchParams(std::move(patch)) {}
};

/**
 * @brief Command to atomically delete multiple timeline elements across multiple tracks.
 * Corresponds to web/src/commands/timeline/element/delete-elements.ts.
 */
class DeleteElementsCommand : public EditorCommand {
public:
    DeleteElementsCommand(
        Timeline& timeline,
        std::vector<ElementLocation> elements
    );

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Delete Elements"; }

    const std::vector<ElementLocation>& elements() const noexcept { return elements_; }

private:
    Timeline& timeline_;
    std::vector<ElementLocation> elements_;
    std::optional<TimelineTracksSnapshot> savedState_{std::nullopt};
};

/**
 * @brief Command to apply batch property updates to multiple elements across multiple tracks.
 * Corresponds to web/src/commands/timeline/element/update-elements.ts.
 */
class UpdateElementsCommand : public EditorCommand {
public:
    UpdateElementsCommand(
        Timeline& timeline,
        std::vector<ElementPatch> updates
    );

    bool execute() override;
    bool undo() override;
    std::string name() const override { return "Update Elements"; }

    const std::vector<ElementPatch>& updates() const noexcept { return updates_; }

private:
    Timeline& timeline_;
    std::vector<ElementPatch> updates_;
    std::optional<TimelineTracksSnapshot> savedState_{std::nullopt};
};

} // namespace catchim::editor

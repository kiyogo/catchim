#pragma once

#include "editor/history/Command.h"
#include "media/MediaLibrary.h"
#include "media/MediaAsset.h"
#include "editor/project/Project.h"
#include "editor/timeline/Timeline.h"
#include "core/ids/Ids.h"
#include <memory>
#include <vector>
#include <optional>
#include <filesystem>

namespace catchim::editor {

/**
 * @brief Command to add a media asset to the MediaLibrary.
 * Automatically elevates (ratchets) the project FPS if the imported asset has higher video FPS.
 * Corresponds to web/src/commands/media/add-media-asset.ts.
 */
class AddMediaAssetCommand : public EditorCommand {
public:
    AddMediaAssetCommand(
        media::MediaLibrary& library,
        std::shared_ptr<media::MediaAsset> asset,
        Project* project = nullptr
    );

    bool execute() override;
    bool undo() override;
    [[nodiscard]] std::string name() const override { return "Add Media Asset"; }

    [[nodiscard]] const core::MediaId& assetId() const noexcept;

private:
    media::MediaLibrary& library_;
    std::shared_ptr<media::MediaAsset> asset_;
    Project* project_{nullptr};
    std::vector<std::shared_ptr<media::MediaAsset>> savedAssets_;
    core::FrameRate previousProjectFps_{30, 1};
    std::optional<core::FrameRate> appliedProjectFps_{std::nullopt};
    bool executed_{false};
    inline static const core::MediaId emptyId_{core::MediaId::empty()};
};

/**
 * @brief Command to remove a media asset from the MediaLibrary.
 * Automatically finds and removes all orphaned timeline clips referencing this asset,
 * with atomic snapshot rollback on undo.
 * Corresponds to web/src/commands/media/remove-media-asset.ts.
 */
class RemoveMediaAssetCommand : public EditorCommand {
public:
    RemoveMediaAssetCommand(
        media::MediaLibrary& library,
        core::MediaId assetId,
        Project* project = nullptr,
        Timeline* timeline = nullptr
    );

    bool execute() override;
    bool undo() override;
    [[nodiscard]] std::string name() const override { return "Remove Media Asset"; }

    [[nodiscard]] const core::MediaId& assetId() const noexcept { return assetId_; }

private:
    media::MediaLibrary& library_;
    core::MediaId assetId_;
    Project* project_{nullptr};
    Timeline* timeline_{nullptr};
    std::shared_ptr<media::MediaAsset> removedAsset_{nullptr};
    std::vector<std::shared_ptr<media::MediaAsset>> savedAssets_;
    std::vector<std::pair<core::SceneId, TimelineTracksSnapshot>> savedSceneSnapshots_;
    std::optional<TimelineTracksSnapshot> savedTimelineSnapshot_{std::nullopt};
    bool executed_{false};
};

/**
 * @brief Command to update a media asset's file path or metadata with undo support.
 */
class UpdateMediaAssetPathCommand : public EditorCommand {
public:
    UpdateMediaAssetPathCommand(
        media::MediaLibrary& library,
        core::MediaId assetId,
        std::filesystem::path newPath
    );
    UpdateMediaAssetPathCommand(
        media::MediaLibrary& library,
        core::MediaId assetId,
        std::shared_ptr<media::MediaAsset> replacementAsset
    );

    bool execute() override;
    bool undo() override;
    [[nodiscard]] std::string name() const override { return "Update Media Asset Path"; }

private:
    media::MediaLibrary& library_;
    core::MediaId assetId_;
    std::filesystem::path newPath_;
    std::filesystem::path previousPath_;
    std::shared_ptr<media::MediaAsset> replacementAsset_;
    std::optional<media::MediaAsset> previousAsset_;
    bool executed_{false};
};

} // namespace catchim::editor

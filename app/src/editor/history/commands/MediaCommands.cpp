#include "MediaCommands.h"
#include "core/time/RationalFrameRate.h"
#include <algorithm>

namespace catchim::editor {

// ============================================================================
// AddMediaAssetCommand
// ============================================================================

AddMediaAssetCommand::AddMediaAssetCommand(
    media::MediaLibrary& library,
    std::shared_ptr<media::MediaAsset> asset,
    Project* project
)
    : library_(library)
    , asset_(std::move(asset))
    , project_(project)
{
}

const core::MediaId& AddMediaAssetCommand::assetId() const noexcept {
    return asset_ ? asset_->id() : emptyId_;
}

bool AddMediaAssetCommand::execute() {
    if (!asset_) {
        return false;
    }

    savedAssets_ = library_.assets();
    library_.addAsset(asset_);

    if (project_ && asset_->type() == media::MediaType::Video && asset_->hasVideo()) {
        previousProjectFps_ = project_->settings().fps;
        double assetFps = core::RationalFrameRateHelper::toFloat(asset_->fps());
        auto raised = core::RationalFrameRateHelper::getRaisedProjectFpsForImportedMedia(
            previousProjectFps_, {assetFps}
        );
        if (raised.has_value()) {
            appliedProjectFps_ = *raised;
            project_->settings().fps = *raised;
            project_->setDirty(true);
        }
    }

    executed_ = true;
    return true;
}

bool AddMediaAssetCommand::undo() {
    if (!executed_ || !asset_) {
        return false;
    }

    library_.removeAsset(asset_->id());

    if (appliedProjectFps_.has_value() && project_) {
        std::vector<double> remainingFps;
        for (const auto& a : library_.assets()) {
            if (a && a->type() == media::MediaType::Video && a->hasVideo()) {
                remainingFps.push_back(core::RationalFrameRateHelper::toFloat(a->fps()));
            }
        }
        auto highestRemaining = core::RationalFrameRateHelper::getHighestImportedVideoFps(remainingFps);
        double appliedFloat = core::RationalFrameRateHelper::toFloat(*appliedProjectFps_);
        if (!highestRemaining.has_value() || *highestRemaining < appliedFloat) {
            project_->settings().fps = previousProjectFps_;
            project_->setDirty(true);
        }
    }

    return true;
}

// ============================================================================
// RemoveMediaAssetCommand
// ============================================================================

RemoveMediaAssetCommand::RemoveMediaAssetCommand(
    media::MediaLibrary& library,
    core::MediaId assetId,
    Project* project,
    Timeline* timeline
)
    : library_(library)
    , assetId_(std::move(assetId))
    , project_(project)
    , timeline_(timeline)
{
}

bool RemoveMediaAssetCommand::execute() {
    removedAsset_ = library_.findAsset(assetId_);
    if (!removedAsset_) {
        return false;
    }

    savedAssets_ = library_.assets();
    savedSceneSnapshots_.clear();
    savedTimelineSnapshot_ = std::nullopt;

    auto removeOrphanedClips = [&](Timeline& tl) {
        std::vector<core::ClipId> toRemove;
        for (const auto* track : tl.allTracks()) {
            if (!track) continue;
            for (const auto& clip : track->clips()) {
                if (clip.mediaId() == assetId_) {
                    toRemove.push_back(clip.id());
                }
            }
        }
        for (const auto& cid : toRemove) {
            tl.removeClip(cid);
        }
    };

    if (project_) {
        for (auto& scene : project_->scenes()) {
            savedSceneSnapshots_.emplace_back(scene.id(), scene.timeline().createSnapshot());
            removeOrphanedClips(scene.timeline());
        }
        project_->setDirty(true);
    } else if (timeline_) {
        savedTimelineSnapshot_ = timeline_->createSnapshot();
        removeOrphanedClips(*timeline_);
    }

    library_.removeAsset(assetId_);
    executed_ = true;
    return true;
}

bool RemoveMediaAssetCommand::undo() {
    if (!executed_ || !removedAsset_) {
        return false;
    }

    library_.addAsset(removedAsset_);

    if (project_) {
        for (const auto& [sceneId, snap] : savedSceneSnapshots_) {
            if (auto* s = project_->findScene(sceneId)) {
                s->timeline().restoreSnapshot(snap);
            }
        }
        project_->setDirty(true);
    } else if (timeline_ && savedTimelineSnapshot_.has_value()) {
        timeline_->restoreSnapshot(*savedTimelineSnapshot_);
    }

    return true;
}

// ============================================================================
// UpdateMediaAssetPathCommand
// ============================================================================

UpdateMediaAssetPathCommand::UpdateMediaAssetPathCommand(
    media::MediaLibrary& library,
    core::MediaId assetId,
    std::filesystem::path newPath
)
    : library_(library)
    , assetId_(std::move(assetId))
    , newPath_(std::move(newPath))
{
}

UpdateMediaAssetPathCommand::UpdateMediaAssetPathCommand(
    media::MediaLibrary& library,
    core::MediaId assetId,
    std::shared_ptr<media::MediaAsset> replacementAsset
)
    : library_(library)
    , assetId_(std::move(assetId))
    , newPath_(replacementAsset ? replacementAsset->filePath() : std::filesystem::path{})
    , replacementAsset_(std::move(replacementAsset))
{
}

bool UpdateMediaAssetPathCommand::execute() {
    auto asset = library_.findAsset(assetId_);
    if (!asset || (replacementAsset_ && asset->type() != replacementAsset_->type())) {
        return false;
    }

    previousPath_ = asset->filePath();
    previousAsset_ = *asset;
    if (replacementAsset_) {
        const auto originalDisplayName = asset->fileName();
        asset->setFilePath(replacementAsset_->filePath());
        asset->setDisplayName(originalDisplayName);
        asset->setFileSize(replacementAsset_->fileSize());
        asset->setDuration(replacementAsset_->duration());
        asset->setDimensions(replacementAsset_->width(), replacementAsset_->height());
        asset->setFps(replacementAsset_->fps());
        asset->setHasAudio(replacementAsset_->hasAudio());
        asset->setHasVideo(replacementAsset_->hasVideo());
        asset->setSampleRate(replacementAsset_->sampleRate());
        asset->setChannels(replacementAsset_->channels());
        asset->setCodec(replacementAsset_->codec());
    } else {
        asset->setFilePath(newPath_);
    }
    executed_ = true;
    return true;
}

bool UpdateMediaAssetPathCommand::undo() {
    if (!executed_) {
        return false;
    }

    auto asset = library_.findAsset(assetId_);
    if (!asset) {
        return false;
    }

    if (previousAsset_) {
        asset->setFilePath(previousAsset_->filePath());
        asset->setDisplayName(previousAsset_->displayName());
        asset->setFileSize(previousAsset_->fileSize());
        asset->setDuration(previousAsset_->duration());
        asset->setDimensions(previousAsset_->width(), previousAsset_->height());
        asset->setFps(previousAsset_->fps());
        asset->setHasAudio(previousAsset_->hasAudio());
        asset->setHasVideo(previousAsset_->hasVideo());
        asset->setSampleRate(previousAsset_->sampleRate());
        asset->setChannels(previousAsset_->channels());
        asset->setCodec(previousAsset_->codec());
    } else {
        asset->setFilePath(previousPath_);
    }
    return true;
}

} // namespace catchim::editor

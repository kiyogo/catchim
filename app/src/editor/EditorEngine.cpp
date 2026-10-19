#include "EditorEngine.h"
#include "project/ProjectSerializer.h"
#include "history/commands/TimelineCommands.h"
#include "history/commands/AdvancedTimelineCommands.h"
#include "history/commands/InsertElementCommand.h"
#include "history/commands/ToggleSourceAudioSeparationCommand.h"
#include <algorithm>

namespace catchim::editor {

EditorEngine::EditorEngine()
    : project_("New project")
{
    playback_.setDuration(project_.totalDuration());

    history_.setOnChangedCallback([this]() {
        if(history_.size()>0)project_.setDirty(true);
        notifyTimelineChanged();
    });
}

void EditorEngine::newProject(std::string name) {
    project_ = Project(std::move(name));
    history_.clear();
    deselectAll();
    playback_.stop();
    playback_.setDuration(project_.totalDuration());
    notifyProjectChanged();
    notifyTimelineChanged();
}

bool EditorEngine::loadProjectFromJson(std::string_view jsonStr) {
    auto res = ProjectSerializer::deserialize(jsonStr);
    if (!res.ok()) return false;

    project_ = res.unwrap();
    history_.clear();
    deselectAll();
    playback_.seek(project_.viewState().playheadTime);
    playback_.setDuration(project_.totalDuration());

    notifyProjectChanged();
    notifyTimelineChanged();
    return true;
}

std::string EditorEngine::saveProjectToJson() const {
    return ProjectSerializer::serialize(project_);
}

void EditorEngine::renameProject(std::string newName) {
    project_.setName(std::move(newName));
    notifyProjectChanged();
}

void EditorEngine::selectClip(core::ClipId clipId, bool additive) {
    if (!additive) {
        selectedClipIds_.clear();
    }
    if (std::find(selectedClipIds_.begin(), selectedClipIds_.end(), clipId) == selectedClipIds_.end()) {
        selectedClipIds_.push_back(std::move(clipId));
    }
    notifySelectionChanged();
}

void EditorEngine::selectClips(const std::vector<core::ClipId>& clipIds, bool additive) {
    if (!additive) {
        selectedClipIds_.clear();
    }
    for (const auto& id : clipIds) {
        if (std::find(selectedClipIds_.begin(), selectedClipIds_.end(), id) == selectedClipIds_.end()) {
            selectedClipIds_.push_back(id);
        }
    }
    notifySelectionChanged();
}

void EditorEngine::deselectAll() {
    if (!selectedClipIds_.empty()) {
        selectedClipIds_.clear();
        notifySelectionChanged();
    }
}

bool EditorEngine::isClipSelected(const core::ClipId& clipId) const {
    return std::find(selectedClipIds_.begin(), selectedClipIds_.end(), clipId) != selectedClipIds_.end();
}

bool EditorEngine::insertElement(Clip clip, core::TimelineTime startTime, std::optional<core::TrackId> explicitTrackId, bool newTrack) {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    TrackType autoTrack = TrackType::Video;
    if (clip.type() == ClipType::Audio) {
        autoTrack = TrackType::Audio;
    } else if (clip.type() == ClipType::Text) {
        autoTrack = TrackType::Text;
    } else if (clip.type() == ClipType::Graphic || clip.type() == ClipType::Sticker) {
        autoTrack = TrackType::Graphic;
    } else if (clip.type() == ClipType::Effect) {
        autoTrack = TrackType::Effect;
    }

    InsertElementPlacement placement;
    placement.autoTrackType=autoTrack;
    if (explicitTrackId.has_value() && !explicitTrackId->isEmpty()) {
        placement.mode = InsertElementPlacement::Mode::Explicit;
        placement.explicitTrackId = *explicitTrackId;
    } else {
        placement.mode = InsertElementPlacement::Mode::Auto;
        placement.autoTrackType = autoTrack;
    }
    placement.startTime = startTime;
    if(newTrack)placement.mode=InsertElementPlacement::Mode::NewTrack;

    core::ClipId cid = clip.id();
    auto cmd = std::make_unique<InsertElementCommand>(*tl, std::move(clip), placement, &project_);
    if (history_.execute(std::move(cmd))) {
        project_.setDirty(true);
        playback_.setDuration(project_.totalDuration());
        selectClip(cid);
        notifyTimelineChanged();
        return true;
    }
    return false;
}

bool EditorEngine::addClip(const core::TrackId& trackId, Clip clip) {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    Track* trk = tl->findTrack(trackId);
    if (trk && trk->acceptsClipType(clip.type()) && trk->canPlace(clip.startTime(), clip.duration(), std::nullopt)) {
        core::ClipId cid = clip.id();
        auto cmd = std::make_unique<AddClipCommand>(*tl, trackId, std::move(clip));
        if (history_.execute(std::move(cmd))) {
            project_.setDirty(true);
            playback_.setDuration(project_.totalDuration());
            selectClip(cid);
            notifyTimelineChanged();
            return true;
        }
        return false;
    }

    // Smart fallback: auto resolve track
    core::TimelineTime start = clip.startTime();
    return insertElement(std::move(clip), start, trackId);
}

bool EditorEngine::moveClip(const core::ClipId& clipId, const core::TrackId& targetTrackId, core::TimelineTime targetTime) {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    auto cmd = std::make_unique<MoveClipCommand>(*tl, clipId, targetTrackId, targetTime);
    if (history_.execute(std::move(cmd))) {
        project_.setDirty(true);
        playback_.setDuration(project_.totalDuration());
        return true;
    }
    return false;
}

bool EditorEngine::trimClipStart(const core::ClipId& clipId, core::TimelineTime newStartTime) {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    auto cmd = std::make_unique<TrimClipCommand>(*tl, clipId, TrimClipCommand::Edge::Start, newStartTime);
    if (history_.execute(std::move(cmd))) {
        project_.setDirty(true);
        playback_.setDuration(project_.totalDuration());
        return true;
    }
    return false;
}

bool EditorEngine::trimClipEnd(const core::ClipId& clipId, core::TimelineTime newDuration) {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    auto cmd = std::make_unique<TrimClipCommand>(*tl, clipId, TrimClipCommand::Edge::End, newDuration);
    if (history_.execute(std::move(cmd))) {
        project_.setDirty(true);
        playback_.setDuration(project_.totalDuration());
        return true;
    }
    return false;
}

bool EditorEngine::splitClip(const core::ClipId& clipId, core::TimelineTime splitTime) {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    auto cmd = std::make_unique<SplitClipCommand>(*tl, clipId, splitTime);
    if (history_.execute(std::move(cmd))) {
        project_.setDirty(true);
        return true;
    }
    return false;
}

bool EditorEngine::splitAtPlayhead() {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    core::TimelineTime curTime = playback_.currentTime();
    bool anySplit = false;

    if (!selectedClipIds_.empty()) {
        for (const auto& cid : selectedClipIds_) {
            const Clip* c = tl->findClip(cid);
            if (c && curTime > c->startTime() && curTime < c->endTime()) {
                if (splitClip(cid, curTime)) anySplit = true;
            }
        }
    } else {
        // Split under playhead on main track or all active tracks
        for (auto* track : tl->allTracks()) {
            for (const auto& c : track->clips()) {
                if (curTime > c.startTime() && curTime < c.endTime()) {
                    if (splitClip(c.id(), curTime)) {
                        anySplit = true;
                        break;
                    }
                }
            }
        }
    }
    return anySplit;
}

bool EditorEngine::splitLeftAtPlayhead() {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    core::TimelineTime curTime = playback_.currentTime();
    for (auto* track : tl->allTracks()) {
        for (const auto& c : track->clips()) {
            if (curTime > c.startTime() && curTime < c.endTime()) {
                auto cmd = std::make_unique<SplitLeftCommand>(*tl, c.id(), curTime);
                if (history_.execute(std::move(cmd))) {
                    project_.setDirty(true);
                    return true;
                }
            }
        }
    }
    return false;
}

bool EditorEngine::splitRightAtPlayhead() {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    core::TimelineTime curTime = playback_.currentTime();
    for (auto* track : tl->allTracks()) {
        for (const auto& c : track->clips()) {
            if (curTime > c.startTime() && curTime < c.endTime()) {
                auto cmd = std::make_unique<SplitRightCommand>(*tl, c.id(), curTime);
                if (history_.execute(std::move(cmd))) {
                    project_.setDirty(true);
                    return true;
                }
            }
        }
    }
    return false;
}

bool EditorEngine::duplicateClip(const core::ClipId& clipId) {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    auto cmd = std::make_unique<DuplicateClipCommand>(*tl, clipId);
    if (history_.execute(std::move(cmd))) {
        project_.setDirty(true);
        return true;
    }
    return false;
}

bool EditorEngine::rippleDelete(const core::ClipId& clipId) {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    auto cmd = std::make_unique<RippleDeleteCommand>(*tl, clipId);
    if (history_.execute(std::move(cmd))) {
        project_.setDirty(true);
        playback_.setDuration(project_.totalDuration());
        return true;
    }
    return false;
}

bool EditorEngine::toggleBookmarkAtPlayhead() {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    bool added = tl->toggleBookmark(playback_.currentTime());
    project_.setDirty(true);
    notifyTimelineChanged();
    return added;
}

bool EditorEngine::deleteSelectedClips() {
    Timeline* tl = activeTimeline();
    if (!tl || selectedClipIds_.empty()) return false;

    std::vector<core::ClipId> toDelete = selectedClipIds_;
    deselectAll();

    for (const auto& cid : toDelete) {
        auto cmd = std::make_unique<DeleteClipCommand>(*tl, cid);
        history_.execute(std::move(cmd));
    }
    project_.setDirty(true);
    playback_.setDuration(project_.totalDuration());
    return true;
}

bool EditorEngine::duplicateSelectedClips() {
    Timeline* tl = activeTimeline();
    if (!tl || selectedClipIds_.empty()) return false;

    std::vector<core::ClipId> newSelected;
    for (const auto& cid : selectedClipIds_) {
        Track* track = tl->findTrackContainingClip(cid);
        if (!track) continue;
        const Clip* clip = track->findClip(cid);
        if (!clip) continue;

        core::TimelineTime newStart = clip->endTime();
        core::ClipId newId = core::ClipId::generate();
        Clip dup = clip->clone(newId);
        dup.setStartTime(newStart);

        if (track->canPlace(newStart, dup.duration())) {
            addClip(track->id(), std::move(dup));
            newSelected.push_back(newId);
        }
    }

    if (!newSelected.empty()) {
        selectClips(newSelected);
    }
    return true;
}

bool EditorEngine::toggleSourceAudioSeparation() {
    Timeline* tl = activeTimeline();
    if (!tl) return false;

    // Find target clip: either selected clip or clip under playhead
    const Clip* targetClip = nullptr;
    core::TrackId targetTrackId;

    if (!selectedClipIds_.empty()) {
        for (const auto& cid : selectedClipIds_) {
            for (auto* track : tl->allTracks()) {
                if (const auto* c = track->findClip(cid)) {
                    if (c->type() == ClipType::Video) {
                        targetClip = c;
                        targetTrackId = track->id();
                        break;
                    }
                }
            }
            if (targetClip) break;
        }
    }

    if (!targetClip) {
        core::TimelineTime curTime = playback_.currentTime();
        for (auto* track : tl->allTracks()) {
            for (const auto& c : track->clips()) {
                if (c.type() == ClipType::Video && curTime >= c.startTime() && curTime <= c.endTime()) {
                    targetClip = &c;
                    targetTrackId = track->id();
                    break;
                }
            }
            if (targetClip) break;
        }
    }

    if (!targetClip) return false;

    auto cmd = std::make_unique<ToggleSourceAudioSeparationCommand>(*tl, targetTrackId, targetClip->id());
    if (history_.execute(std::move(cmd))) {
        project_.setDirty(true);
        notifyTimelineChanged();
        return true;
    }
    return false;
}

void EditorEngine::notifyProjectChanged() {
    if (onProjectChanged_) onProjectChanged_();
}

void EditorEngine::notifyTimelineChanged() {
    playback_.setDuration(project_.totalDuration());
    if (onTimelineChanged_) onTimelineChanged_();
}

void EditorEngine::notifySelectionChanged() {
    if (onSelectionChanged_) onSelectionChanged_();
}

void EditorEngine::update() {
    playback_.update();
}

} // namespace catchim::editor

#pragma once

#include "project/Project.h"
#include "history/CommandHistory.h"
#include "playback/PlaybackController.h"
#include "core/ids/Ids.h"
#include "core/time/TimelineTime.h"
#include "core/errors/Errors.h"
#include <vector>
#include <unordered_set>
#include <functional>
#include <memory>

namespace catchim::editor {

class EditorEngine {
public:
    EditorEngine();
    ~EditorEngine() = default;

    // Project operations
    void newProject(std::string name = "New project");
    bool loadProjectFromJson(std::string_view jsonStr);
    std::string saveProjectToJson() const;
    void renameProject(std::string newName);

    Project& project() noexcept { return project_; }
    const Project& project() const noexcept { return project_; }

    Timeline* activeTimeline() noexcept { return project_.activeTimeline(); }
    const Timeline* activeTimeline() const noexcept { return project_.activeTimeline(); }

    // History
    CommandHistory& history() noexcept { return history_; }
    const CommandHistory& history() const noexcept { return history_; }
    bool undo() { return history_.undo(); }
    bool redo() { return history_.redo(); }
    bool canUndo() const noexcept { return history_.canUndo(); }
    bool canRedo() const noexcept { return history_.canRedo(); }

    // Playback
    PlaybackController& playback() noexcept { return playback_; }
    const PlaybackController& playback() const noexcept { return playback_; }
    void play() { playback_.play(); }
    void pause() { playback_.pause(); }
    void togglePlay() { playback_.toggle(); }
    void seek(core::TimelineTime time) { playback_.seek(time); }

    // Selection
    const std::vector<core::ClipId>& selectedClips() const noexcept { return selectedClipIds_; }
    void selectClip(core::ClipId clipId, bool additive = false);
    void selectClips(const std::vector<core::ClipId>& clipIds, bool additive = false);
    void deselectAll();
    bool isClipSelected(const core::ClipId& clipId) const;

    // Timeline editing commands (Undoable)
    bool addClip(const core::TrackId& trackId, Clip clip);
    bool insertElement(Clip clip, core::TimelineTime startTime, std::optional<core::TrackId> explicitTrackId = std::nullopt, bool newTrack = false);
    bool moveClip(const core::ClipId& clipId, const core::TrackId& targetTrackId, core::TimelineTime targetTime);
    bool trimClipStart(const core::ClipId& clipId, core::TimelineTime newStartTime);
    bool trimClipEnd(const core::ClipId& clipId, core::TimelineTime newDuration);
    bool splitClip(const core::ClipId& clipId, core::TimelineTime splitTime);
    bool splitAtPlayhead();
    bool splitLeftAtPlayhead();
    bool splitRightAtPlayhead();
    bool deleteSelectedClips();
    bool duplicateSelectedClips();
    bool duplicateClip(const core::ClipId& clipId);
    bool rippleDelete(const core::ClipId& clipId);
    bool toggleBookmarkAtPlayhead();
    bool toggleSourceAudioSeparation();

    // Toolbar settings
    bool isSnappingEnabled() const noexcept { return snappingEnabled_; }
    void setSnappingEnabled(bool enabled) { snappingEnabled_ = enabled; }
    void toggleSnapping() { snappingEnabled_ = !snappingEnabled_; }

    bool isRippleEnabled() const noexcept { return rippleEnabled_; }
    void setRippleEnabled(bool enabled) { rippleEnabled_ = enabled; }
    void toggleRipple() { rippleEnabled_ = !rippleEnabled_; }

    // Callbacks & Observers for UI
    void setOnProjectChanged(std::function<void()> cb) { onProjectChanged_ = std::move(cb); }
    void setOnTimelineChanged(std::function<void()> cb) { onTimelineChanged_ = std::move(cb); }
    void setOnSelectionChanged(std::function<void()> cb) { onSelectionChanged_ = std::move(cb); }

    void update();
    void notifyProjectChanged();
    void notifyTimelineChanged();
    void notifySelectionChanged();

private:
    Project project_;
    CommandHistory history_;
    PlaybackController playback_;

    std::vector<core::ClipId> selectedClipIds_;
    bool snappingEnabled_{true};
    bool rippleEnabled_{false};

    std::function<void()> onProjectChanged_;
    std::function<void()> onTimelineChanged_;
    std::function<void()> onSelectionChanged_;
};

} // namespace catchim::editor

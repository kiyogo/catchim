#include "Timeline.h"
#include <algorithm>

namespace catchim::editor {

Timeline::Timeline()
    : mainTrack_(core::TrackId("main-track"), TrackType::Video, "Video")
{
}

std::vector<Track*> Timeline::allTracks() {
    std::vector<Track*> result;
    for (auto& t : overlayTracks_) result.push_back(&t);
    result.push_back(&mainTrack_);
    for (auto& t : audioTracks_) result.push_back(&t);
    return result;
}

std::vector<const Track*> Timeline::allTracks() const {
    std::vector<const Track*> result;
    for (const auto& t : overlayTracks_) result.push_back(&t);
    result.push_back(&mainTrack_);
    for (const auto& t : audioTracks_) result.push_back(&t);
    return result;
}

Track* Timeline::findTrack(const core::TrackId& trackId) {
    if (mainTrack_.id() == trackId) return &mainTrack_;
    for (auto& t : overlayTracks_) {
        if (t.id() == trackId) return &t;
    }
    for (auto& t : audioTracks_) {
        if (t.id() == trackId) return &t;
    }
    return nullptr;
}

const Track* Timeline::findTrack(const core::TrackId& trackId) const {
    if (mainTrack_.id() == trackId) return &mainTrack_;
    for (const auto& t : overlayTracks_) {
        if (t.id() == trackId) return &t;
    }
    for (const auto& t : audioTracks_) {
        if (t.id() == trackId) return &t;
    }
    return nullptr;
}

Track* Timeline::findTrackContainingClip(const core::ClipId& clipId) {
    for (auto* track : allTracks()) {
        if (track->findClip(clipId)) return track;
    }
    return nullptr;
}

const Track* Timeline::findTrackContainingClip(const core::ClipId& clipId) const {
    for (const auto* track : allTracks()) {
        if (track->findClip(clipId)) return track;
    }
    return nullptr;
}

Track& Timeline::addTrack(TrackType type, std::string name) {
    return insertTrack(type, std::move(name));
}

Track& Timeline::insertTrack(
    TrackType type,
    std::string name,
    std::optional<size_t> index,
    std::optional<core::TrackId> specificId
) {
    core::TrackId id = specificId.has_value() ? *specificId : core::TrackId::generate();
    if (type == TrackType::Audio) {
        size_t idx = index.has_value() ? std::min(*index, audioTracks_.size()) : audioTracks_.size();
        auto it = audioTracks_.emplace(audioTracks_.begin() + idx, std::move(id), type, std::move(name));
        return *it;
    } else {
        size_t idx = index.has_value() ? std::min(*index, overlayTracks_.size()) : overlayTracks_.size();
        auto it = overlayTracks_.emplace(overlayTracks_.begin() + idx, std::move(id), type, std::move(name));
        return *it;
    }
}

bool Timeline::removeTrack(const core::TrackId& trackId) {
    return extractTrack(trackId).has_value();
}

std::optional<Track> Timeline::extractTrack(const core::TrackId& trackId) {
    if (mainTrack_.id() == trackId) return std::nullopt; // Main track cannot be deleted

    auto it = std::find_if(overlayTracks_.begin(), overlayTracks_.end(), [&](const Track& t) {
        return t.id() == trackId;
    });
    if (it != overlayTracks_.end()) {
        Track removed = std::move(*it);
        overlayTracks_.erase(it);
        return removed;
    }

    auto itA = std::find_if(audioTracks_.begin(), audioTracks_.end(), [&](const Track& t) {
        return t.id() == trackId;
    });
    if (itA != audioTracks_.end()) {
        Track removed = std::move(*itA);
        audioTracks_.erase(itA);
        return removed;
    }
    return std::nullopt;
}

bool Timeline::addClip(const core::TrackId& trackId, Clip clip) {
    Track* track = findTrack(trackId);
    if (!track) return false;
    return track->insertClip(std::move(clip));
}

std::optional<Clip> Timeline::removeClip(const core::ClipId& clipId) {
    for (auto* track : allTracks()) {
        auto removed = track->removeClip(clipId);
        if (removed.has_value()) return removed;
    }
    return std::nullopt;
}

const Clip* Timeline::findClip(const core::ClipId& clipId) const {
    for (const auto* track : allTracks()) {
        const Clip* c = track->findClip(clipId);
        if (c) return c;
    }
    return nullptr;
}

Clip* Timeline::findClip(const core::ClipId& clipId) {
    for (auto* track : allTracks()) {
        Clip* c = track->findClip(clipId);
        if (c) return c;
    }
    return nullptr;
}

bool Timeline::moveClip(
    const core::ClipId& clipId,
    const core::TrackId& targetTrackId,
    core::TimelineTime targetTime
) {
    Track* srcTrack = findTrackContainingClip(clipId);
    Track* dstTrack = findTrack(targetTrackId);
    if (!srcTrack || !dstTrack) return false;

    Clip* clip = srcTrack->findClip(clipId);
    if (!clip) return false;

    if (!dstTrack->acceptsClipType(clip->type())) return false;

    // Check collision excluding this clip
    std::optional<core::ClipId> exclude = (srcTrack == dstTrack) ? std::optional(clipId) : std::nullopt;
    if (!dstTrack->canPlace(targetTime, clip->duration(), exclude)) {
        return false;
    }

    if (srcTrack == dstTrack) {
        clip->setStartTime(targetTime);
        srcTrack->sortClips();
    } else {
        auto removedOpt = srcTrack->removeClip(clipId);
        if (!removedOpt.has_value()) return false;
        Clip movedClip = std::move(*removedOpt);
        movedClip.setStartTime(targetTime);
        dstTrack->insertClip(std::move(movedClip));
    }
    return true;
}

bool Timeline::trimClipStart(const core::ClipId& clipId, core::TimelineTime newStartTime) {
    Track* track = findTrackContainingClip(clipId);
    if (!track) return false;

    Clip* clip = track->findClip(clipId);
    if (!clip) return false;

    core::TimelineTime oldStart = clip->startTime();
    core::TimelineTime oldEnd = clip->endTime();
    if (newStartTime >= oldEnd) return false;

    core::TimelineTime delta = newStartTime - oldStart;
    core::TimelineTime newDuration = oldEnd - newStartTime;
    core::TimelineTime newTrimStart = clip->trimStart() + delta;

    if (newTrimStart.ticks() < 0) return false;
    if (clip->sourceDuration().has_value() &&
        newTrimStart + newDuration > *clip->sourceDuration()) return false;

    if (!track->canPlace(newStartTime, newDuration, clipId)) return false;

    clip->setStartTime(newStartTime);
    clip->setDuration(newDuration);
    clip->setTrimStart(newTrimStart);
    track->sortClips();
    return true;
}

bool Timeline::trimClipEnd(const core::ClipId& clipId, core::TimelineTime newDuration) {
    if (newDuration.ticks() <= 0) return false;

    Track* track = findTrackContainingClip(clipId);
    if (!track) return false;

    Clip* clip = track->findClip(clipId);
    if (!clip) return false;

    // Never allow a trim gesture to extend a media clip past its source.
    if (clip->sourceDuration().has_value()) {
        const auto available = (*clip->sourceDuration() - clip->trimStart()) / std::max(.01,clip->getParam<double>("speed",1));
        if (available.ticks() <= 0 || newDuration > available) return false;
    }

    if (!track->canPlace(clip->startTime(), newDuration, clipId)) return false;

    clip->setDuration(newDuration);
    return true;
}

std::pair<std::optional<core::ClipId>, std::optional<core::ClipId>> Timeline::splitClip(
    const core::ClipId& clipId,
    core::TimelineTime splitTime
) {
    Track* track = findTrackContainingClip(clipId);
    if (!track) return {std::nullopt, std::nullopt};

    Clip* clip = track->findClip(clipId);
    if (!clip) return {std::nullopt, std::nullopt};

    if (splitTime <= clip->startTime() || splitTime >= clip->endTime()) {
        return {std::nullopt, std::nullopt};
    }

    core::TimelineTime leftDuration = splitTime - clip->startTime();
    core::TimelineTime rightDuration = clip->endTime() - splitTime;

    // Right clip gets new id
    core::ClipId rightId = core::ClipId::generate();
    Clip rightClip = clip->clone(rightId);
    rightClip.setStartTime(splitTime);
    rightClip.setDuration(rightDuration);
    rightClip.setTrimStart(clip->trimStart() + leftDuration);

    // Left clip retains original id
    clip->setDuration(leftDuration);

    track->clips().push_back(std::move(rightClip));
    track->sortClips();

    return {clipId, rightId};
}

bool Timeline::removeBookmark(const core::BookmarkId& id) {
    auto it = std::find_if(bookmarks_.begin(), bookmarks_.end(), [&](const Bookmark& b) {
        return b.id == id;
    });
    if (it != bookmarks_.end()) {
        bookmarks_.erase(it);
        return true;
    }
    return false;
}

bool Timeline::hasBookmarkAt(core::TimelineTime time, core::TimelineTime threshold) const {
    for (const auto& b : bookmarks_) {
        int64_t diff = std::abs(b.time.ticks() - time.ticks());
        if (diff <= threshold.ticks()) return true;
    }
    return false;
}

bool Timeline::toggleBookmark(core::TimelineTime time, core::TimelineTime threshold) {
    auto it = std::find_if(bookmarks_.begin(), bookmarks_.end(), [&](const Bookmark& b) {
        return std::abs(b.time.ticks() - time.ticks()) <= threshold.ticks();
    });

    if (it != bookmarks_.end()) {
        bookmarks_.erase(it);
        return false; // Removed
    } else {
        Bookmark bm;
        bm.time = time;
        bookmarks_.push_back(std::move(bm));
        sortBookmarks();
        return true; // Added
    }
}

std::optional<Bookmark> Timeline::findNextBookmark(core::TimelineTime time) const {
    for (const auto& b : bookmarks_) {
        if (b.time > time) {
            return b;
        }
    }
    return std::nullopt;
}

std::optional<Bookmark> Timeline::findPrevBookmark(core::TimelineTime time) const {
    std::optional<Bookmark> result;
    for (const auto& b : bookmarks_) {
        if (b.time < time) {
            result = b;
        } else {
            break;
        }
    }
    return result;
}

void Timeline::sortBookmarks() {
    std::sort(bookmarks_.begin(), bookmarks_.end(), [](const Bookmark& a, const Bookmark& b) {
        return a.time < b.time;
    });
}

TimelineTracksSnapshot Timeline::createSnapshot() const {
    return TimelineTracksSnapshot{
        overlayTracks_,
        mainTrack_,
        audioTracks_
    };
}

void Timeline::restoreSnapshot(const TimelineTracksSnapshot& snapshot) {
    overlayTracks_ = snapshot.overlayTracks;
    mainTrack_ = snapshot.mainTrack;
    audioTracks_ = snapshot.audioTracks;
}

core::TimelineTime Timeline::totalDuration() const noexcept {
    core::TimelineTime maxTime(0);
    for (const auto* t : allTracks()) {
        maxTime = std::max(maxTime, t->maxEndTime());
    }
    return maxTime;
}

void Timeline::applyRipple(const core::TrackId& trackId, core::TimelineTime afterTime, core::TimelineTime delta) {
    Track* track = findTrack(trackId);
    if (!track) return;

    for (auto& clip : track->clips()) {
        if (clip.startTime() >= afterTime) {
            clip.setStartTime(clip.startTime() + delta);
        }
    }
    track->sortClips();
}

} // namespace catchim::editor

#include "editor/history/commands/InsertElementCommand.h"
#include "editor/timeline/PlacementEngine.h"
#include <cmath>

namespace catchim::editor {

InsertElementCommand::InsertElementCommand(
    Timeline& timeline,
    Clip element,
    InsertElementPlacement placement,
    Project* project
)
    : timeline_(timeline),
      element_(std::move(element)),
      placement_(placement),
      project_(project),
      insertedClipId_(element_.id()) {}

bool InsertElementCommand::execute() {
    savedState_ = timeline_.createSnapshot();
    if (project_) {
        savedSettings_ = project_->settings();
    }

    size_t totalElements = timeline_.mainTrack().clips().size();
    for (const auto& t : timeline_.overlayTracks()) totalElements += t.clips().size();
    for (const auto& t : timeline_.audioTracks()) totalElements += t.clips().size();
    bool isFirstElement = (totalElements == 0);

    // 1. Auto-adjust canvas settings for first visual media element
    if (isFirstElement && project_ &&
        (element_.type() == ClipType::Video || element_.type() == ClipType::Image)) {
        int w = element_.getParam<int>("width", element_.getParam<int>("sourceWidth", 0));
        int h = element_.getParam<int>("height", element_.getParam<int>("sourceHeight", 0));
        if (w > 0 && h > 0) {
            project_->settings().canvasSize = CanvasSize{w, h};
            if (!project_->settings().originalCanvasSize.has_value()) {
                project_->settings().originalCanvasSize = CanvasSize{w, h};
            }
        }

        double fps = element_.getParam<double>("fps", 0.0);
        if (fps > 0.0) {
            int32_t num = static_cast<int32_t>(std::round(fps * 1000.0));
            int32_t den = 1000;
            project_->settings().fps = core::FrameRate{num, den};
        }
    }

    // 2. Resolve placement
    auto createTrack=[this](TrackType type,const std::string& name)->Track& {
        if(createdTrackId_.isEmpty())createdTrackId_=core::TrackId::generate();
        return timeline_.insertTrack(type,name,std::nullopt,createdTrackId_);
    };
    targetTrackId_ = core::TrackId::empty();
    if(placement_.mode==InsertElementPlacement::Mode::NewTrack) {
        targetTrackId_=createTrack(placement_.autoTrackType,"New track").id();
    }

    if (placement_.mode == InsertElementPlacement::Mode::Explicit) {
        Track* t = timeline_.findTrack(placement_.explicitTrackId);
        if (t && t->acceptsClipType(element_.type()) && t->canPlace(placement_.startTime, element_.duration())) {
            targetTrackId_ = t->id();
        }
    }
    
    if (targetTrackId_.isEmpty()) {
        if (placement_.autoTrackType == TrackType::Video) {
            if (timeline_.mainTrack().clips().empty() ||
                timeline_.mainTrack().canPlace(placement_.startTime, element_.duration())) {
                targetTrackId_ = timeline_.mainTrack().id();
            } else {
                for (auto& ot : timeline_.overlayTracks()) {
                    if (ot.acceptsClipType(element_.type()) &&
                        ot.canPlace(placement_.startTime, element_.duration())) {
                        targetTrackId_ = ot.id();
                        break;
                    }
                }
                if (targetTrackId_.isEmpty()) {
                    Track& newOt = createTrack(TrackType::Video, "Video Overlay");
                    targetTrackId_ = newOt.id();
                }
            }
        } else if (placement_.autoTrackType == TrackType::Audio) {
            for (auto& at : timeline_.audioTracks()) {
                if (at.canPlace(placement_.startTime, element_.duration())) {
                    targetTrackId_ = at.id();
                    break;
                }
            }
            if (targetTrackId_.isEmpty()) {
                Track& newAt = createTrack(TrackType::Audio, "Audio Track");
                targetTrackId_ = newAt.id();
            }
        } else {
            for (auto& ot : timeline_.overlayTracks()) {
                if (ot.acceptsClipType(element_.type()) &&
                    ot.canPlace(placement_.startTime, element_.duration())) {
                    targetTrackId_ = ot.id();
                    break;
                }
            }
            if (targetTrackId_.isEmpty()) {
                TrackType tt = (element_.type() == ClipType::Text) ? TrackType::Text :
                               (element_.type() == ClipType::Effect) ? TrackType::Effect : TrackType::Graphic;
                std::string name = (element_.type() == ClipType::Text) ? "Text Track" :
                                   (element_.type() == ClipType::Effect) ? "Effect Track" : "Graphic Track";
                Track& newOt = createTrack(tt, name);
                targetTrackId_ = newOt.id();
            }
        }
    }

    Track* dst = timeline_.findTrack(targetTrackId_);
    if (!dst) return false;

    element_.setStartTime(placement_.startTime);
    insertedClipId_ = element_.id();
    // Retain the command's element template for redo.
    dst->clips().push_back(element_);
    dst->sortClips();

    return true;
}

bool InsertElementCommand::undo() {
    if (!savedState_) {
        return false;
    }
    timeline_.restoreSnapshot(*savedState_);
    if (savedSettings_ && project_) {
        project_->settings() = *savedSettings_;
    }
    return true;
}

} // namespace catchim::editor

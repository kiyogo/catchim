#include "TimelineTracksWidget.h"

#if defined(HAVE_QT6)
#include "ui/icons/UiIcons.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PresetInsertion.h"
#include "editor/clipboard/ClipboardManager.h"
#include "editor/history/commands/ElementCommands.h"
#include "editor/history/commands/TrackCommands.h"
#include "editor/history/commands/BatchCommand.h"
#include "media/decoder/NativeVideoDecoder.h"
#include "audio/AudioPlaybackEngine.h"
#include <QApplication>
#include <QPointer>
#include <QRunnable>
#include <QPainterPath>
#include <QProcess>
#ifdef _WIN32
#include <objbase.h>
#endif
#include <QJsonDocument>
#include "core/time/Timecode.h"
#include "media/waveform/WaveformGenerator.h"
#include "media/probe/MediaProbe.h"
#include "media/MediaAsset.h"
#include "media/probe/MediaProbe.h"
#include "audio/AudioWaveformBarEngine.h"
#include "audio/AudioVolumeLineEngine.h"
#include "editor/timeline/AdvancedSnapEngine.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QContextMenuEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QUrl>
#include <QToolTip>
#include <QHelpEvent>
#include <cmath>

namespace catchim::ui {

TimelineTracksWidget::TimelineTracksWidget(
    editor::EditorEngine& engine,
    media::MediaLibrary& mediaLibrary,
    QWidget* parent
)
    : QWidget(parent)
    , engine_(engine)
    , mediaLibrary_(mediaLibrary)
{
    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    thumbnailPool_.setMaxThreadCount(1);
}

TimelineTracksWidget::~TimelineTracksWidget() {
    thumbnailPool_.clear();
    thumbnailPool_.waitForDone();
}

QImage TimelineTracksWidget::thumbnail(const editor::Clip& clip,double sourceTime) {
    // Stable time buckets survive zoom/scroll and bound the number of seeks.
    const double duration=clip.sourceDuration()?clip.sourceDuration()->toSeconds():clip.duration().toSeconds();
    const double bucket=std::max(.5,duration/48.);
    sourceTime=std::floor(std::max(0.,sourceTime)/bucket)*bucket;
    const auto key=QString::fromStdString(clip.mediaId().str())+":"+QString::number(qRound64(sourceTime*1000));
    if(thumbnails_.contains(key))return thumbnails_.value(key);
    // Do not compete with live playback by seeking dozens of filmstrip frames.
    const auto mediaKey=QString::fromStdString(clip.mediaId().str());
    if(engine_.playback().isPlaying()){
        if(thumbnailCovers_.contains(mediaKey))return thumbnailCovers_.value(mediaKey);
        // Starting playback immediately after import still needs one cover.
        // Permit only one pending seek, then reuse it for the entire strip.
        if(!pendingThumbnails_.isEmpty())return {};
    }
    // First make one cover available. Additional filmstrip seeks are queued
    // only after that cover exists, keeping the first paint and editor input
    // responsive for long clips.
    auto asset=mediaLibrary_.findAsset(clip.mediaId());
    if(!asset||pendingThumbnails_.contains(key)||pendingThumbnails_.size()>=3)return thumbnailCovers_.value(mediaKey);
    pendingThumbnails_.insert(key);
    const auto path=asset->filePath();
    const QPointer<TimelineTracksWidget> target(this);
    thumbnailPool_.start(QRunnable::create([target,path,sourceTime,key] {
#ifdef _WIN32
        const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
#endif
        int width=0,height=0;std::vector<uint8_t> rgba;QImage result;
        auto& decoder=media::NativeVideoDecoder::thumbnailInstance();
        decoder.setPreviewSize(240,136);
        if(decoder.getFrame(path,sourceTime,width,height,rgba,"filmstrip@"+path.string())&&width>0&&height>0)
            result=QImage(rgba.data(),width,height,width*4,QImage::Format_RGBA8888).scaled(120,68,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation).copy();
        QMetaObject::invokeMethod(qApp,[target,key,result] {
            if(!target)return;
            target->pendingThumbnails_.remove(key);
            if(target->thumbnails_.size()>=512)target->thumbnails_.erase(target->thumbnails_.begin());
            target->thumbnails_.insert(key,result);target->update();
            if(!result.isNull())target->thumbnailCovers_.insert(key.left(key.lastIndexOf(':')),result);
        },Qt::QueuedConnection);
#ifdef _WIN32
        if(SUCCEEDED(com))CoUninitialize();
#endif
    }));
    return thumbnailCovers_.value(mediaKey);
}

int TimelineTracksWidget::totalTracksHeight() const {
    auto* tl = engine_.activeTimeline();
    if (!tl) return 200;

    int total = Metrics::timelineContentTopPadding;
    for (const auto* track : tl->allTracks()) {
        int h = Metrics::trackHeightVideo;
        if (track->type() == editor::TrackType::Audio) h = Metrics::trackHeightAudio;
        else if (track->type() != editor::TrackType::Video) h = Metrics::trackHeightText;
        total += h + Metrics::trackGap;
    }
    return std::max(total, 200);
}

QSize TimelineTracksWidget::sizeHint() const {
    double pixelsPerSecond = 50.0 * zoomFactor_;
    double durSec = engine_.project().totalDuration().toSeconds();
    if (durSec < 30.0) durSec = 30.0;
    int w = Metrics::trackLabelsWidth + static_cast<int>(durSec * pixelsPerSecond);
    return QSize(w, totalTracksHeight());
}

core::TimelineTime TimelineTracksWidget::pixelToTime(int pixelX) const {
    int contentX = pixelX - Metrics::trackLabelsWidth + scrollX_;
    double pixelsPerSecond = 50.0 * zoomFactor_;
    double sec = static_cast<double>(contentX) / pixelsPerSecond;
    const auto time = core::TimelineTime::fromSeconds(std::max(0.0, sec));
    const auto duration = engine_.project().totalDuration();
    return duration.ticks() > 0 ? std::min(time, duration) : time;
}

int TimelineTracksWidget::timeToPixel(core::TimelineTime time) const {
    double pixelsPerSecond = 50.0 * zoomFactor_;
    return static_cast<int>(time.toSeconds() * pixelsPerSecond) - scrollX_ + Metrics::trackLabelsWidth;
}

TimelineTracksWidget::HitTestResult TimelineTracksWidget::hitTest(const QPoint& pos) const {
    HitTestResult res;
    auto* tl = engine_.activeTimeline();
    if (!tl) return res;

    int yOffset = Metrics::timelineContentTopPadding - scrollY_;

    for (const auto* track : tl->allTracks()) {
        int trackHeight = Metrics::trackHeightVideo;
        if (track->type() == editor::TrackType::Audio) trackHeight = Metrics::trackHeightAudio;
        else if (track->type() != editor::TrackType::Video) trackHeight = Metrics::trackHeightText;

        if (pos.y() >= yOffset && pos.y() < yOffset + trackHeight) {
            res.track = track;

            if (pos.x() < Metrics::trackLabelsWidth) {
                // Header buttons (Right aligned: Track icon, Hide icon, Mute icon)
                bool hasAudio = (track->type() == editor::TrackType::Video || track->type() == editor::TrackType::Audio);
                bool canHide = (track->type() != editor::TrackType::Audio);

                int trackIconX = Metrics::trackLabelsWidth - 14 - 12; // 86
                int eyeX = trackIconX - 8 - 14;                       // 64
                int muteX = canHide ? (eyeX - 8 - 14) : eyeX;         // 42 or 64

                if (canHide && pos.x() >= eyeX - 4 && pos.x() <= eyeX + 18) {
                    res.isHideBtn = true;
                    return res;
                } else if (hasAudio && pos.x() >= muteX - 4 && pos.x() <= muteX + 18) {
                    res.isMuteBtn = true;
                    return res;
                }
            }

            // Check clips
            for (const auto& clip : track->clips()) {
                int clipLeft = timeToPixel(clip.startTime());
                int clipRight = timeToPixel(clip.endTime());

                if (pos.x() >= clipLeft && pos.x() <= clipRight) {
                    res.clip = &clip;
                    if (std::abs(pos.x() - clipLeft) <= 9) {
                        res.isTrimStart = true;
                    } else if (std::abs(pos.x() - clipRight) <= 9) {
                        res.isTrimEnd = true;
                    }
                    return res;
                }
            }
            return res;
        }
        yOffset += trackHeight + Metrics::trackGap;
    }
    return res;
}

editor::Track* TimelineTracksWidget::findTrackAtY(int y) {
    auto* tl = engine_.activeTimeline();
    if (!tl) return nullptr;
    int yOffset = Metrics::timelineContentTopPadding - scrollY_;
    for (auto* track : tl->allTracks()) {
        int trackHeight = Metrics::trackHeightVideo;
        if (track->type() == editor::TrackType::Audio) trackHeight = Metrics::trackHeightAudio;
        else if (track->type() != editor::TrackType::Video) trackHeight = Metrics::trackHeightText;
        if (y >= yOffset && y < yOffset + trackHeight) return track;
        yOffset += trackHeight + Metrics::trackGap;
    }
    return nullptr;
}

const editor::Track* TimelineTracksWidget::findTrackAtY(int y) const {
    const auto* tl = engine_.activeTimeline();
    if (!tl) return nullptr;
    int yOffset = Metrics::timelineContentTopPadding - scrollY_;
    for (const auto* track : tl->allTracks()) {
        int trackHeight = Metrics::trackHeightVideo;
        if (track->type() == editor::TrackType::Audio) trackHeight = Metrics::trackHeightAudio;
        else if (track->type() != editor::TrackType::Video) trackHeight = Metrics::trackHeightText;
        if (y >= yOffset && y < yOffset + trackHeight) return track;
        yOffset += trackHeight + Metrics::trackGap;
    }
    return nullptr;
}

void TimelineTracksWidget::paintEvent(QPaintEvent* /* event */) {
    QPainter painter(this);
    const auto& palette = Theme::instance().palette();

    // Background tracks container
    painter.fillRect(rect(), palette.panelBackground);

    auto* tl = engine_.activeTimeline();
    if (!tl) return;

    int yOffset = Metrics::timelineContentTopPadding - scrollY_;

    // 1. Draw track rows
    for (const auto* track : tl->allTracks()) {
        int trackHeight = Metrics::trackHeightVideo;
        if (track->type() == editor::TrackType::Audio) trackHeight = Metrics::trackHeightAudio;
        else if (track->type() != editor::TrackType::Video) trackHeight = Metrics::trackHeightText;

        // Track lane background
        QRect trackRect(Metrics::trackLabelsWidth, yOffset, width() - Metrics::trackLabelsWidth, trackHeight);
        painter.fillRect(trackRect, palette.panelBackground);

        // Empty track dashed container (matches web timeline-track.tsx)
        if (track->clips().empty()) {
            int laneW = width() - Metrics::trackLabelsWidth;
            QRect emptyRect(Metrics::trackLabelsWidth, yOffset, laneW - 1, trackHeight - 1);
            auto laneBorder = palette.border;
            laneBorder.setAlphaF(0.5);
            QPen dashPen(laneBorder, 1, Qt::CustomDashLine);
            dashPen.setDashPattern({4, 4});
            painter.setPen(dashPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(emptyRect, 3, 3);
        } else {
            // Draw clips on this track
            for (const auto& clip : track->clips()) {
            int clipX = timeToPixel(clip.startTime());
            int clipW = std::max(4, timeToPixel(clip.endTime()) - clipX);
            QRect clipRect(clipX, yOffset, clipW, trackHeight);

            bool isSelected = engine_.isClipSelected(clip.id());

            // Base color
            QColor clipBg;
            switch (clip.type()) {
                // Media clips use a neutral fallback while filmstrip frames
                // are arriving. A selected clip must not become one solid
                // blue block just because its thumbnails are still pending.
                case editor::ClipType::Video: clipBg = palette.secondary; break;
                case editor::ClipType::Audio: clipBg = palette.clipAudio; break;
                case editor::ClipType::Text: clipBg = palette.clipText; break;
                case editor::ClipType::Graphic: clipBg = palette.clipGraphic; break;
                case editor::ClipType::Effect: clipBg = palette.clipEffect; break;
                case editor::ClipType::Image: clipBg = palette.secondary; break;
                case editor::ClipType::Sticker: clipBg = palette.secondary; break;
            }

            // Fill clip body
            painter.setBrush(clipBg);
            painter.setPen(isSelected ? QPen(palette.primaryAccent, 2.0) : QPen(QColor(0, 0, 0, 100), 1.0));
            painter.drawRoundedRect(clipRect.adjusted(1, 1, -1, -1), 6, 6);

            if(clip.type()==editor::ClipType::Video || clip.type()==editor::ClipType::Image || clip.type()==editor::ClipType::Sticker) {
                painter.save();QPainterPath path;path.addRoundedRect(QRectF(clipRect.adjusted(1,1,-1,-1)),6,6);painter.setClipPath(path);
                const int tileWidth=116;
                const int firstTile=std::max(0,(Metrics::trackLabelsWidth-clipX)/tileWidth);
                const int lastTile=std::min((clipW+tileWidth-1)/tileWidth,(width()-clipX)/tileWidth+1);
                const double sourceDuration=clip.sourceDuration()?clip.sourceDuration()->toSeconds():clip.duration().toSeconds();
                for(int tile=firstTile;tile<lastTile;++tile) {
                    double t=clip.trimStart().toSeconds()+tile*tileWidth/(50.0*zoomFactor_)*clip.getParam<double>("speed",1);
                    if(clip.getParam<bool>("reversed",false))t=clip.trimStart().toSeconds()+clip.duration().toSeconds()*clip.getParam<double>("speed",1)-tile*tileWidth/(50.0*zoomFactor_);
                    t=std::clamp(t,0.,std::max(0.,sourceDuration-.01));
                    if(clip.type()!=editor::ClipType::Video)t=0;
                    auto image=thumbnail(clip,t);
                    if(image.isNull()) image=thumbnailCovers_.value(QString::fromStdString(clip.mediaId().str()));
                    if(!image.isNull()) painter.drawImage(QRect(clipX+tile*tileWidth,yOffset,tileWidth,trackHeight),image);
                }
                painter.restore();
                if(isSelected){painter.setBrush(Qt::NoBrush);painter.setPen(QPen(palette.primaryAccent,2));painter.drawRoundedRect(clipRect.adjusted(1,1,-1,-1),6,6);}
            }

            // Draw left & right resize handle indicators on selected clips
            if (isSelected && clipRect.width() > 18) {
                painter.setBrush(QColor(255, 255, 255, 220));
                painter.setPen(Qt::NoPen);
                int handleH = std::min(16, trackHeight - 12);
                int handleY = yOffset + (trackHeight - handleH) / 2;
                painter.drawRoundedRect(clipRect.left() + 2, handleY, 3, handleH, 1.5, 1.5);
                painter.drawRoundedRect(clipRect.right() - 4, handleY, 3, handleH, 1.5, 1.5);
            }

            // Draw Audio Waveform & Volume line if audio clip
            if (clip.type() == editor::ClipType::Audio) {
                auto wf = media::WaveformGenerator::instance().getWaveform(clip.mediaId());
                double volLinear = audio::AudioPlaybackEngine::resolveEffectiveAudioGain(clip,track->isMuted(),core::TimelineTime(0));
                std::vector<double> gainSamples = { volLinear };

                if (wf && !wf->buckets.empty()) {
                    std::vector<float> amplitudes;
                    amplitudes.reserve(wf->buckets.size());
                    for (const auto& b : wf->buckets) {
                        amplitudes.push_back(b.maxPeak);
                    }

                    double innerH = static_cast<double>(trackHeight - 12);
                    auto bars = audio::AudioWaveformBarEngine::calculateWaveformBars(
                        amplitudes,
                        static_cast<double>(clipW),
                        innerH,
                        gainSamples,
                        clip.duration().toSeconds()
                    );

                    for (const auto& bar : bars) {
                        int bx = clipX + static_cast<int>(bar.x);
                        int by = yOffset + 6 + static_cast<int>(bar.top);
                        int bh = std::max(1, static_cast<int>(bar.height));
                        painter.setPen(bar.isBurnt ? QColor("#ff6e14") : palette.waveformColor);
                        painter.drawLine(bx, by, bx, by + bh);
                    }
                }

                // Draw horizontal volume automation line
                double volDb = (volLinear > 0.0001) ? (20.0 * std::log10(volLinear)) : -60.0;
                double linePosPercent = audio::AudioVolumeLineEngine::getLinePositionPercent(volDb);
                int lineY = yOffset + 6 + static_cast<int>((linePosPercent / 100.0) * (trackHeight - 12));
                QColor volLineColor = (Theme::instance().mode() == ThemeMode::Dark)
                    ? QColor(255, 255, 255, 130) : QColor(0, 0, 0, 130);
                painter.setPen(QPen(volLineColor, 1.0, Qt::DashLine));
                painter.drawLine(clipX, lineY, clipX + clipW, lineY);
            }

            // Clip label text
            if (clipRect.width() > 14) {
                painter.save();
                painter.setClipRect(clipRect.adjusted(2, 2, -2, -2));
                painter.setPen(QColor("#FFFFFF"));
                QFont font("Inter");font.setPixelSize(clip.type()==editor::ClipType::Video?10:12);
                painter.setFont(font);
                QFontMetrics fm(font);
                int availW = clipRect.width() - 16;
                if (availW > 8) {
                    QString label = fm.elidedText(QString::fromStdString(clip.name()), Qt::ElideRight, availW);
                    painter.drawText(clipRect.adjusted(6, 2, -6, -2), Qt::AlignLeft | Qt::AlignTop, label);
                }
                painter.restore();
            }
        }
        }

        yOffset += trackHeight + Metrics::trackGap;
    }

    // 2. Draw Track Labels Column on the left (112px, fixed overlay)
    QRect labelsCol(0, 0, Metrics::trackLabelsWidth, height());
    painter.fillRect(labelsCol, palette.panelBackground);
    painter.setPen(palette.border);
    painter.drawLine(Metrics::trackLabelsWidth - 1, 0, Metrics::trackLabelsWidth - 1, height());

    yOffset = Metrics::timelineContentTopPadding - scrollY_;
    for (const auto* track : tl->allTracks()) {
        int trackHeight = Metrics::trackHeightVideo;
        if (track->type() == editor::TrackType::Audio) trackHeight = Metrics::trackHeightAudio;
        else if (track->type() != editor::TrackType::Video) trackHeight = Metrics::trackHeightText;

        QRect labelCell(0, yOffset, Metrics::trackLabelsWidth - 1, trackHeight);

        // Right-aligned icons layout (matching web OpenCut TrackLabelsPanel):
        // Right margin: 12px. Icon size: 14px. Gap: 8px.
        int iconY = yOffset + (trackHeight - 14) / 2;

        bool hasAudio = (track->type() == editor::TrackType::Video || track->type() == editor::TrackType::Audio);
        bool canHide = (track->type() != editor::TrackType::Audio);

        // Track type icon
        UiIcon tIcon = UiIcon::Video;
        if (track->type() == editor::TrackType::Audio) tIcon = UiIcon::Music;
        else if (track->type() == editor::TrackType::Text) tIcon = UiIcon::Text;
        else if (track->type() == editor::TrackType::Graphic) tIcon = UiIcon::Stickers;
        else if (track->type() == editor::TrackType::Effect) tIcon = UiIcon::Effects;

        int trackIconX = Metrics::trackLabelsWidth - 14 - 12; // 86
        painter.drawPixmap(trackIconX, iconY, UiIcons::getPixmap(tIcon, palette.textSecondary, 14));

        int nextX = trackIconX - 8 - 14; // 64

        if (canHide) {
            UiIcon eyeIcon = track->isHidden() ? UiIcon::EyeOff : UiIcon::Eye;
            QColor eyeCol = track->isHidden() ? palette.destructive : palette.textSecondary;
            painter.drawPixmap(nextX, iconY, UiIcons::getPixmap(eyeIcon, eyeCol, 14));
            nextX -= (8 + 14); // 42
        }

        if (hasAudio) {
            UiIcon muteIcon = track->isMuted() ? UiIcon::VolumeMute : UiIcon::Volume;
            QColor muteCol = track->isMuted() ? palette.destructive : palette.textSecondary;
            painter.drawPixmap(nextX, iconY, UiIcons::getPixmap(muteIcon, muteCol, 14));
        }

        yOffset += trackHeight + Metrics::trackGap;
    }

    // 3. Draw Playhead Vertical Line across all tracks
    int playheadX = timeToPixel(engine_.playback().currentTime());
    if (playheadX >= Metrics::trackLabelsWidth && playheadX < width()) {
        painter.setPen(QPen(palette.primaryAccent, 1.0));
        painter.drawLine(playheadX, 0, playheadX, height());
    }

    // Keep selected trim grips above the playhead. At time zero the playhead
    // sits directly on the clip's left edge and otherwise obscures the grip.
    yOffset = Metrics::timelineContentTopPadding - scrollY_;
    for (const auto* track : tl->allTracks()) {
        int trackHeight = Metrics::trackHeightVideo;
        if (track->type() == editor::TrackType::Audio) trackHeight = Metrics::trackHeightAudio;
        else if (track->type() != editor::TrackType::Video) trackHeight = Metrics::trackHeightText;
        for (const auto& clip : track->clips()) {
            if (engine_.isClipSelected(clip.id())) {
                const int clipLeft = timeToPixel(clip.startTime());
                const int clipRight = timeToPixel(clip.endTime());
                if (clipRight - clipLeft > 18) {
                    const int handleH = std::min(16, trackHeight - 12);
                    const int handleY = yOffset + (trackHeight - handleH) / 2;
                    painter.setBrush(QColor(255, 255, 255, 240));
                    painter.setPen(QPen(palette.primaryAccent, 1));
                    painter.drawRoundedRect(QRect(clipLeft + 2, handleY, 5, handleH), 2, 2);
                    painter.drawRoundedRect(QRect(clipRight - 6, handleY, 5, handleH), 2, 2);
                }
            }
        }
        yOffset += trackHeight + Metrics::trackGap;
    }

    // 4. Draw Snap Indicator Guideline if snapping is active
    if (snapIndicatorX_ >= Metrics::trackLabelsWidth && snapIndicatorX_ < width()) {
        painter.setPen(QPen(palette.primaryAccent, 1.5, Qt::DashLine));
        painter.drawLine(snapIndicatorX_, 0, snapIndicatorX_, height());
    }

    // 5. Draw Drop Indicator (blue vertical line) during drag-over
    if (showDropIndicator_ && !dropIndicatorRect_.isNull()) {
        painter.setPen(QPen(QColor("#38bdf8"), 2, Qt::SolidLine));
        painter.setBrush(QColor(56, 189, 248, 80));
        painter.drawRect(dropIndicatorRect_);
    }
}

void TimelineTracksWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        auto hit = hitTest(event->pos());
        if (hit.track && (hit.isMuteBtn || hit.isHideBtn)) {
            auto* tl=engine_.activeTimeline();
            if (hit.isMuteBtn) engine_.history().execute(std::make_unique<editor::ToggleTrackMuteCommand>(*tl,hit.track->id()));
            else engine_.history().execute(std::make_unique<editor::ToggleTrackVisibilityCommand>(*tl,hit.track->id()));
            update();
            event->accept();
            return;
        }

        if (hit.clip) {
            dragTimeline_=engine_.activeTimeline();dragSnapshot_=dragTimeline_->createSnapshot();dragEdited_=false;
            activeClipId_ = hit.clip->id();
            dragStartPos_ = event->pos();
            dragStartTime_ = hit.clip->startTime();
            dragStartDuration_ = hit.clip->duration();
            dragRefresh_.start();

            if (hit.isTrimStart) {
                dragMode_ = DragMode::TrimmingStart;
            } else if (hit.isTrimEnd) {
                dragMode_ = DragMode::TrimmingEnd;
            } else {
                dragMode_ = DragMode::MovingClip;
                bool additive = (event->modifiers() & Qt::ShiftModifier) || (event->modifiers() & Qt::ControlModifier);
                engine_.selectClip(hit.clip->id(), additive);
                emit clipSelected(hit.clip->id());
            }
            update();
            event->accept();
            return;
        } else {
            // Click on empty space -> Seek
            engine_.deselectAll();
            core::TimelineTime clickTime = pixelToTime(event->pos().x());
            emit seekRequested(clickTime);
            update();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void TimelineTracksWidget::mouseMoveEvent(QMouseEvent* event) {
    if (dragMode_ == DragMode::None) {
        auto hit = hitTest(event->pos());
        if (hit.isTrimStart || hit.isTrimEnd) {
            setCursor(Qt::SizeHorCursor);
        } else if (hit.isMuteBtn || hit.isHideBtn) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    } else if (dragMode_ == DragMode::MovingClip) {
        int deltaX = event->pos().x() - dragStartPos_.x();
        double pixelsPerSecond = 50.0 * zoomFactor_;
        core::TimelineTime deltaTime = core::TimelineTime::fromSeconds(deltaX / pixelsPerSecond);
        core::TimelineTime newStart = dragStartTime_ + deltaTime;
        if (newStart.ticks() >= 0) {
            auto* tl = engine_.activeTimeline();
            if (tl) {
                auto* sourceTrack = tl->findTrackContainingClip(activeClipId_);
                if (sourceTrack) {
                    if (engine_.isSnappingEnabled()) {
                        auto snapPoints = editor::AdvancedSnapEngine::collectAllSnapPoints(*tl, engine_.playback().currentTime(), true, activeClipId_);
                        auto maxSnap = editor::AdvancedSnapEngine::getTimelineSnapThresholdInTicks(zoomFactor_ * 0.5, 12.0);

                        auto snapStart = editor::AdvancedSnapEngine::resolveSortedTimelineSnap(newStart, snapPoints, maxSnap);
                        if (snapStart.hasSnapped) {
                            newStart = snapStart.snappedTime;
                            snapIndicatorX_ = timeToPixel(newStart);
                        } else {
                            auto snapEnd = editor::AdvancedSnapEngine::resolveSortedTimelineSnap(newStart + dragStartDuration_, snapPoints, maxSnap);
                            if (snapEnd.hasSnapped) {
                                newStart = snapEnd.snappedTime - dragStartDuration_;
                                snapIndicatorX_ = timeToPixel(snapEnd.snappedTime);
                            } else {
                                snapIndicatorX_ = -1;
                            }
                        }
                    } else {
                        snapIndicatorX_ = -1;
                    }

                    if (newStart.ticks() >= 0) {
                        // Resolve target track from Y position for cross-track dragging
                        editor::Track* targetTrack = findTrackAtY(event->pos().y());
                        core::TrackId targetTrackId = sourceTrack->id();
                        if (targetTrack && targetTrack != sourceTrack) {
                            // Determine clip type from sourceTrack type for compatibility check
                            editor::ClipType ct = (sourceTrack->type() == editor::TrackType::Audio)
                                ? editor::ClipType::Audio
                                : (sourceTrack->type() == editor::TrackType::Text)
                                    ? editor::ClipType::Text
                                    : editor::ClipType::Video;
                            if (targetTrack->acceptsClipType(ct)) {
                                targetTrackId = targetTrack->id();
                            }
                        }
                        if(tl->moveClip(activeClipId_,targetTrackId,newStart)) {
                            dragEdited_=true;engine_.project().setDirty(true);
                            if(dragRefresh_.elapsed()>=33){dragRefresh_.restart();engine_.notifyTimelineChanged();}
                        }
                    }
                    update();
                }
            }
        }
    } else if (dragMode_ == DragMode::TrimmingStart) {
        core::TimelineTime targetTime = pixelToTime(event->pos().x());
        auto* tl = engine_.activeTimeline();
        if (tl && engine_.isSnappingEnabled()) {
            auto snapPoints = editor::AdvancedSnapEngine::collectAllSnapPoints(*tl, engine_.playback().currentTime(), true, activeClipId_);
            auto maxSnap = editor::AdvancedSnapEngine::getTimelineSnapThresholdInTicks(zoomFactor_ * 0.5, 12.0);
            auto snapRes = editor::AdvancedSnapEngine::resolveSortedTimelineSnap(targetTime, snapPoints, maxSnap);
            if (snapRes.hasSnapped) {
                targetTime = snapRes.snappedTime;
                snapIndicatorX_ = timeToPixel(targetTime);
            } else {
                snapIndicatorX_ = -1;
            }
        } else {
            snapIndicatorX_ = -1;
        }
        if(tl&&tl->trimClipStart(activeClipId_,targetTime)) {
            dragEdited_=true;engine_.project().setDirty(true);
            if(dragRefresh_.elapsed()>=33){dragRefresh_.restart();engine_.notifyTimelineChanged();}
        }
        update();
    } else if (dragMode_ == DragMode::TrimmingEnd) {
        core::TimelineTime targetTime = pixelToTime(event->pos().x());
        auto* tl = engine_.activeTimeline();
        if (tl && engine_.isSnappingEnabled()) {
            auto snapPoints = editor::AdvancedSnapEngine::collectAllSnapPoints(*tl, engine_.playback().currentTime(), true, activeClipId_);
            auto maxSnap = editor::AdvancedSnapEngine::getTimelineSnapThresholdInTicks(zoomFactor_ * 0.5, 12.0);
            auto snapRes = editor::AdvancedSnapEngine::resolveSortedTimelineSnap(targetTime, snapPoints, maxSnap);
            if (snapRes.hasSnapped) {
                targetTime = snapRes.snappedTime;
                snapIndicatorX_ = timeToPixel(targetTime);
            } else {
                snapIndicatorX_ = -1;
            }
        } else {
            snapIndicatorX_ = -1;
        }
        core::TimelineTime newDur = targetTime - dragStartTime_;
        if (newDur.ticks() > 0) {
            if(tl&&tl->trimClipEnd(activeClipId_,newDur)) {
                dragEdited_=true;engine_.project().setDirty(true);
                if(dragRefresh_.elapsed()>=33){dragRefresh_.restart();engine_.notifyTimelineChanged();}
            }
            update();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void TimelineTracksWidget::mouseReleaseEvent(QMouseEvent* event) {
    snapIndicatorX_ = -1;
    if (dragMode_ != DragMode::None) {
        if(dragEdited_&&dragSnapshot_&&dragTimeline_==engine_.activeTimeline())
            engine_.history().execute(std::make_unique<editor::TracksSnapshotCommand>(*dragTimeline_,std::move(*dragSnapshot_),dragTimeline_->createSnapshot(),"Timeline gesture"));
        dragSnapshot_.reset();dragTimeline_=nullptr;dragEdited_=false;
        dragMode_ = DragMode::None;
        setCursor(Qt::ArrowCursor);
        update();
        event->accept();
        return;
    }
    update();
    QWidget::mouseReleaseEvent(event);
}

void TimelineTracksWidget::dragEnterEvent(QDragEnterEvent* event) {
    const auto* mime = event->mimeData();
    if (mime->hasUrls() || mime->hasFormat("application/x-catchim-media-id") || mime->hasFormat("application/x-catchim-preset")) {
        showDropIndicator_ = true;
        event->acceptProposedAction();
    }
}

void TimelineTracksWidget::dragLeaveEvent(QDragLeaveEvent* /*event*/) {
    showDropIndicator_ = false;
    update();
}

void TimelineTracksWidget::dragMoveEvent(QDragMoveEvent* event) {
    const auto* mime = event->mimeData();
    if (!mime->hasUrls() && !mime->hasFormat("application/x-catchim-media-id") && !mime->hasFormat("application/x-catchim-preset")) return;

    QPoint pos = event->position().toPoint();
    core::TimelineTime dropTime = pixelToTime(pos.x());
    const editor::Track* targetTrack = findTrackAtY(pos.y());

    if (targetTrack) {
        int yOffset = Metrics::timelineContentTopPadding - scrollY_;
        for (const auto* tr : engine_.activeTimeline()->allTracks()) {
            int h = Metrics::trackHeightVideo;
            if (tr->type() == editor::TrackType::Audio) h = Metrics::trackHeightAudio;
            else if (tr->type() != editor::TrackType::Video) h = Metrics::trackHeightText;
            if (tr == targetTrack) {
                int xPx = timeToPixel(dropTime);
                dropIndicatorRect_ = QRect(xPx - 1, yOffset, 3, h);
                break;
            }
            yOffset += h + Metrics::trackGap;
        }
    }

    showDropIndicator_ = true;
    update();
    event->acceptProposedAction();
}

void TimelineTracksWidget::dropEvent(QDropEvent* event) {
    const auto* mime = event->mimeData();
    QPoint pos = event->position().toPoint();
    core::TimelineTime dropTime = pixelToTime(pos.x());
    auto* tl = engine_.activeTimeline();
    showDropIndicator_ = false;

    if (!tl) { event->ignore(); return; }

    if(mime->hasFormat("application/x-catchim-preset")) {
        const auto preset=QJsonDocument::fromJson(mime->data("application/x-catchim-preset"));
        if(preset.isObject() && insertUiPreset(engine_,mediaLibrary_,preset.object(),dropTime))event->acceptProposedAction();
        else event->ignore();
        update();return;
    }

    const editor::Track* targetTrack = findTrackAtY(pos.y());
    const auto dropTrackId=targetTrack?std::optional<core::TrackId>(targetTrack->id()):std::nullopt;

    auto placeAssetClip = [&](const media::MediaAsset* asset) {
        if (!asset) return;

        editor::ClipType ct = editor::ClipType::Video;
        if (asset->type() == media::MediaType::Audio)  ct = editor::ClipType::Audio;
        else if (asset->type() == media::MediaType::Image) ct = editor::ClipType::Image;

        editor::Clip clip(
            core::ClipId::generate(),
            ct,
            asset->fileName(),
            dropTime,
            asset->duration()
        );
        clip.setMediaId(asset->id());
        clip.setSourceDuration(asset->duration());
        clip.setParam("hasAudio",asset->hasAudio());
        if (asset->width() > 0 && asset->height() > 0) {
            clip.setParam("sourceWidth", asset->width());
            clip.setParam("sourceHeight", asset->height());
            clip.setParam("width", asset->width());
            clip.setParam("height", asset->height());
        }
        if (asset->fps().toFps() > 0.0) {
            clip.setParam("fps", asset->fps().toFps());
        }

        std::optional<core::TrackId> explicitId;
        const auto* targetTrack=dropTrackId?tl->findTrack(*dropTrackId):nullptr;
        if (targetTrack && targetTrack->acceptsClipType(ct)) {
            explicitId = targetTrack->id();
        }

        engine_.insertElement(std::move(clip),dropTime,explicitId,!dropTrackId.has_value());
        engine_.project().setDirty(true);
        engine_.notifyTimelineChanged();
    };

    // Handle drag from AssetsPanel (media-id MIME)
    if (mime->hasFormat("application/x-catchim-media-id")) {
        QByteArray idBytes = mime->data("application/x-catchim-media-id");
        std::string idStr(idBytes.constData(), idBytes.size());
        core::MediaId mediaId(idStr);
        auto asset = mediaLibrary_.findAsset(mediaId);
        if (asset) {
            placeAssetClip(asset.get());
            event->acceptProposedAction();
            update();
            return;
        }
    }

    // Handle file drop from OS / file manager
    if (mime->hasUrls()) {
        for (const auto& url : mime->urls()) {
            QString localPath = url.toLocalFile();
            if (localPath.isEmpty()) continue;

            auto existing = mediaLibrary_.findAssetByPath(localPath.toStdString());
            if (existing) {
                placeAssetClip(existing.get());
                dropTime = dropTime + existing->duration();
            } else {
                auto probeRes = media::MediaProbe::probe(localPath.toStdString());
                if (probeRes.ok()) {
                    auto asset = probeRes.unwrap();
                    mediaLibrary_.addAsset(asset);
                    auto inserted = mediaLibrary_.findAsset(asset->id());
                    placeAssetClip(inserted.get());
                    dropTime = dropTime + asset->duration();
                }
            }
        }
        event->acceptProposedAction();
        update();
        return;
    }

    event->ignore();
}

void TimelineTracksWidget::contextMenuEvent(QContextMenuEvent* event) {
    const auto hit=hitTest(event->pos());
    auto* tl=engine_.activeTimeline();if(!tl)return;
    const auto& pal=Theme::instance().palette();
    QMenu menu(this);menu.setObjectName("timelineContextMenu");menu.setMinimumWidth(254);
    auto action=[&](UiIcon icon,const QString& title,const QString& shortcut=QString()) {
        auto* a=menu.addAction(UiIcons::get(icon,pal.textSecondary,14),title+(shortcut.isEmpty()?QString():"\t"+shortcut));return a;
    };
    if(hit.clip) {
        const auto id=hit.clip->id();const auto mediaId=hit.clip->mediaId();
        const bool muted=hit.clip->isMuted(),hidden=hit.clip->isHidden();
        const auto type=hit.clip->type();const auto trackId=hit.track->id();
        if(!engine_.isClipSelected(id))engine_.selectClip(id);
        auto* split=action(UiIcon::Split,"Cắt","S");
        const auto time=engine_.playback().currentTime();
        split->setEnabled(time>hit.clip->startTime()&&time<hit.clip->endTime());
        auto* copy=action(UiIcon::Duplicate,"Sao chép","CTRL C");
        auto* duplicate=action(UiIcon::Duplicate,"Nhân đôi","CTRL D");
        auto* mute=action(muted?UiIcon::Volume:UiIcon::VolumeMute,muted?"Bật tiếng":"Tắt tiếng");
        mute->setEnabled(type==editor::ClipType::Audio||type==editor::ClipType::Video);
        auto* detach=action(UiIcon::Unlink,"Tách âm thanh");detach->setEnabled(type==editor::ClipType::Video);
        auto* hide=action(hidden?UiIcon::Eye:UiIcon::EyeOff,hidden?"Hiện":"Ẩn");hide->setEnabled(type!=editor::ClipType::Audio);
        auto* reveal=action(UiIcon::Search,"Hiện media");reveal->setEnabled(!mediaId.isEmpty());
        auto* replace=action(UiIcon::Undo,"Thay media");replace->setEnabled(false);
        menu.addSeparator();
        auto* remove=action(UiIcon::Delete,"Xóa clip","BACKSPACE");remove->setIcon(UiIcons::get(UiIcon::Delete,pal.destructive,14));
        const auto* chosen=menu.exec(event->globalPos());
        if(chosen==copy)editor::ClipboardManager::instance().copy(*tl,engine_.selectedClips());
        else if(chosen==duplicate)engine_.duplicateSelectedClips();
        else if(chosen==split)engine_.splitClip(id,engine_.playback().currentTime());
        else if(chosen==remove)engine_.deleteSelectedClips();
        else if(chosen==detach)engine_.toggleSourceAudioSeparation();
        else if(chosen==reveal)emit mediaRevealRequested(mediaId);
        else if(chosen==mute||chosen==hide) {
            std::vector<editor::ElementPatch> updates;
            for(const auto& selected:engine_.selectedClips()) {
                editor::ElementPatch patch{};patch.trackId=trackId;patch.clipId=selected;
                if(chosen==mute){patch.muted=!muted;patch.patchParams["muted"]=!muted;}
                else patch.hidden=!hidden;
                updates.push_back(std::move(patch));
            }
            engine_.history().execute(std::make_unique<editor::UpdateElementsCommand>(*tl,std::move(updates)));
        }
    } else if(hit.track) {
        const auto trackId=hit.track->id();
        auto* mute=action(UiIcon::VolumeMute,hit.track->isMuted()?"Bật tiếng rãnh":"Tắt tiếng rãnh");
        auto* hide=action(UiIcon::EyeOff,hit.track->isHidden()?"Hiện rãnh":"Ẩn rãnh");
        const auto* chosen=menu.exec(event->globalPos());
        if(chosen==mute)engine_.history().execute(std::make_unique<editor::ToggleTrackMuteCommand>(*tl,trackId));
        else if(chosen==hide)engine_.history().execute(std::make_unique<editor::ToggleTrackVisibilityCommand>(*tl,trackId));
    } else {
        auto* video=action(UiIcon::Plus,"Thêm rãnh video");
        auto* audio=action(UiIcon::Plus,"Thêm rãnh âm thanh");
        auto* text=action(UiIcon::Plus,"Thêm rãnh văn bản");
        menu.addSeparator();auto* paste=action(UiIcon::Duplicate,"Dán","CTRL V");paste->setEnabled(editor::ClipboardManager::instance().hasData());
        const auto* chosen=menu.exec(event->globalPos());
        if(chosen==paste)editor::ClipboardManager::instance().paste(*tl,engine_.history(),pixelToTime(event->pos().x()));
        else if(chosen==video||chosen==audio||chosen==text)engine_.history().execute(std::make_unique<editor::AddTrackCommand>(*tl,chosen==video?editor::TrackType::Video:chosen==audio?editor::TrackType::Audio:editor::TrackType::Text,chosen->text().toStdString()));
    }
}
bool TimelineTracksWidget::event(QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        auto* helpEvent = static_cast<QHelpEvent*>(event);
        auto hit = hitTest(helpEvent->pos());
        if (hit.track) {
            if (hit.isMuteBtn) {
                QToolTip::showText(helpEvent->globalPos(), hit.track->isMuted() ? "Unmute track" : "Mute track", this);
                return true;
            } else if (hit.isHideBtn) {
                QToolTip::showText(helpEvent->globalPos(), hit.track->isHidden() ? "Show track" : "Hide track", this);
                return true;
            } else if (hit.clip) {
                QToolTip::hideText();
                return true;
            }
        }
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    return QWidget::event(event);
}

} // namespace catchim::ui
#endif

#include "PreviewWidget.h"

#if defined(HAVE_QT6)
#include "ui/theme/Theme.h"
#include <QPainter>
#include <QMouseEvent>
#include <QColor>
#include <QFontMetricsF>
#include <QLineF>
#include "editor/history/commands/ElementCommands.h"
#include <cmath>
#include <algorithm>
#include <QPointer>
#include <QCoreApplication>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "media/decoder/NativeVideoDecoder.h"
#ifdef _WIN32
#include <objbase.h>
#endif

namespace catchim::ui {

// One in-flight frame plus one replaceable request: seeking or a slow decoder
// cannot build an ever-growing queue of obsolete video frames.
class PreviewRenderWorker {
public:
    explicit PreviewRenderWorker(PreviewWidget* target):target_(target) {
        thread_=std::thread([this]{run();});
    }
    ~PreviewRenderWorker() {
        {std::lock_guard lock(mutex_);stopping_=true;}
        condition_.notify_one();thread_.join();
    }
    void request(std::shared_ptr<const media::PlaybackSnapshot> snapshot,core::TimelineTime time,QSize size,uint64_t generation,bool playing) {
        {std::lock_guard lock(mutex_);request_={std::move(snapshot),time,size,generation,playing};pending_=true;}
        condition_.notify_one();
    }
private:
    struct Request {std::shared_ptr<const media::PlaybackSnapshot> snapshot;core::TimelineTime time{0};QSize size;uint64_t generation{0};bool playing{false};};
    QPointer<PreviewWidget> target_;
    std::mutex mutex_;std::condition_variable condition_;std::thread thread_;
    bool stopping_{false},pending_{false};Request request_;
    void run() {
#ifdef _WIN32
        struct ComScope {HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);~ComScope(){if(SUCCEEDED(hr))CoUninitialize();}} com;
#endif
        media::NativeVideoDecoder decoder;
        render::RenderEngine renderer(2,2);renderer.setVideoDecoder(&decoder);
        for(;;) {
            Request request;
            {std::unique_lock lock(mutex_);condition_.wait(lock,[&]{return stopping_||pending_;});if(stopping_)return;request=request_;pending_=false;}
            const auto start=std::chrono::steady_clock::now();
            decoder.setPreviewSize(request.size.width(),request.size.height());
            decoder.setPlaybackMode(request.playing);
            renderer.setCanvasSize(request.size.width(),request.size.height());
            const auto& frame=renderer.renderFrame(request.snapshot->project,request.snapshot->library,request.time);
            QImage image;
            if(!frame.rgbaPixels.empty())image=QImage(frame.rgbaPixels.data(),frame.width,frame.height,frame.width*4,QImage::Format_RGBA8888).copy();
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            const auto stats=decoder.statistics();
            QMetaObject::invokeMethod(QCoreApplication::instance(),[target=target_,image=std::move(image),request,ms,stats]{
                if(!target||target->generation_!=request.generation)return;
                target->cachedFrame_=image;target->presentedSeconds_=request.time.toSeconds();
                target->lastRenderMs_=ms;++target->presentedFrames_;target->update();
                target->decoderStatistics_=stats;
            },Qt::QueuedConnection);
        }
    }
};

static QSizeF webTextBounds(const editor::Clip& clip,int canvasW,int canvasH) {
    QFont font(QString::fromStdString(clip.getParam<std::string>("text.fontFamily","Arial")));
    font.setPixelSize(std::max(1,qRound(clip.getParam<double>("text.fontSize",15)*canvasH/90.0)));
    font.setBold(clip.getParam<std::string>("text.fontWeight","normal")=="bold");
    font.setItalic(clip.getParam<std::string>("text.fontStyle","normal")=="italic");
    const QFontMetricsF metrics(font);
    const auto bounds=metrics.boundingRect(QRectF(40,0,canvasW-80,canvasH),Qt::AlignCenter|Qt::TextWordWrap,QString::fromStdString(clip.getParam<std::string>("text.content",clip.name())));
    return bounds.size();
}

PreviewWidget::PreviewWidget(
    editor::EditorEngine& engine,
    render::RenderEngine& renderEngine,
    media::MediaLibrary& mediaLibrary,
    QWidget* parent
)
    : QWidget(parent)
    , engine_(engine)
    , renderEngine_(renderEngine)
    , mediaLibrary_(mediaLibrary)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    worker_=std::make_unique<PreviewRenderWorker>(this);
}
PreviewWidget::~PreviewWidget()=default;

QRect PreviewWidget::canvasRect() const {
    const auto canvas = engine_.project().settings().canvasSize;
    const int cw = canvas.width > 0 ? canvas.width : 1920;
    const int ch = canvas.height > 0 ? canvas.height : 1080;
    const double scale = zoomFactor_ < 0
        ? std::max(.05, std::min(double(width()-16)/cw, double(height()-16)/ch)) : zoomFactor_;
    const int w = qRound(cw*scale), h = qRound(ch*scale);
    return QRect((width()-w)/2+panOffset_.x(), 8+(height()-16-h)/2+panOffset_.y(), w, h);
}

PreviewWidget::SelectionGeometry PreviewWidget::selectionGeometry() const {
    SelectionGeometry geometry;
    const auto* timeline = engine_.activeTimeline();
    if (!timeline || engine_.selectedClips().empty()) return geometry;
    const auto id = engine_.selectedClips().front();
    const auto* clip = timeline->findClip(id);
    const auto* track = timeline->findTrackContainingClip(id);
    const auto time = std::min(engine_.playback().currentTime(), core::TimelineTime::lastFrameTime(
        engine_.project().totalDuration(), engine_.project().settings().fps));
    if (!clip || clip->isHidden() || (track && track->isHidden()) ||
        clip->type() == editor::ClipType::Audio || time < clip->startTime() || time >= clip->endTime()) return geometry;

    const auto canvas = engine_.project().settings().canvasSize;
    const int cw = canvas.width > 0 ? canvas.width : 1920;
    const int ch = canvas.height > 0 ? canvas.height : 1080;
    const auto viewport = canvasRect();
    // Match both stages of rendering: compositor coordinates, then the cached
    // frame blit. In particular, integer preview-size rounding must not shift
    // rotated handles away from the image at fractional zoom / high DPI.
    const QSize output = requestedSize_.isValid() ? requestedSize_ : QSize(cw, ch);
    const double ox = double(output.width())/cw, oy = double(output.height())/ch;
    auto& transform = geometry.localToViewport;
    transform.translate(viewport.x(), viewport.y());
    transform.scale(double(viewport.width())/output.width(), double(viewport.height())/output.height());
    transform.translate(output.width()/2.0+clip->getParam<double>("transform.positionX",0)*ox,
                        output.height()/2.0+clip->getParam<double>("transform.positionY",0)*oy);
    transform.rotate(clip->getParam<double>("transform.rotate",0));
    transform.scale(clip->getParam<double>("transform.scaleX",1)*ox,
                    clip->getParam<double>("transform.scaleY",1)*oy);

    QSizeF source(cw, ch);
    if (clip->type() == editor::ClipType::Video || clip->type() == editor::ClipType::Image || clip->type() == editor::ClipType::Sticker) {
        const auto asset = mediaLibrary_.findAsset(clip->mediaId());
        if (!asset || asset->width() <= 0 || asset->height() <= 0) return {};
        // Visible media dimensions, not canvas dimensions or codec padding.
        source = QSizeF(asset->width(), asset->height());
    } else if (clip->type() == editor::ClipType::Text) {
        // Retain the native text layout's measured selection extent.
        source = clip->getParam<bool>("text.webLayout",false) ? webTextBounds(*clip,cw,ch) : QSizeF(240,100);
    } else if (clip->type() == editor::ClipType::Graphic) {
        const double size = std::min(output.width(),output.height())/4;
        const auto shape = clip->getParam<std::string>("graphic.shape","rectangle");
        source = shape == "rectangle" ? QSizeF(2*size/ox,size/oy) : QSizeF(size/ox,size/oy);
    }
    geometry.localBounds = QRectF(-source.width()/2,-source.height()/2,source.width(),source.height());
    const auto& r = geometry.localBounds;
    geometry.outline = transform.map(QPolygonF{r.topLeft(),r.topRight(),r.bottomRight(),r.bottomLeft()});
    geometry.handles = {transform.map(r.topLeft()),transform.map(QPointF(0,r.top())),transform.map(r.topRight()),
        transform.map(QPointF(r.left(),0)),transform.map(QPointF(r.right(),0)),transform.map(r.bottomLeft()),
        transform.map(QPointF(0,r.bottom())),transform.map(r.bottomRight())};
    const QPointF outward = geometry.handles[1]-transform.map(QPointF(0,0));
    const double length = std::hypot(outward.x(),outward.y());
    geometry.rotationHandle = geometry.handles[1]+(length > 0 ? outward*(20/length) : QPointF(0,-20));
    return geometry;
}

QPolygonF PreviewWidget::selectionOutline() const { return selectionGeometry().outline; }

PreviewWidget::HandleType PreviewWidget::hitTestHandles(const QPointF& mousePos, const SelectionGeometry& geometry) const {
    if (geometry.outline.isEmpty()) return HandleType::None;
    if (QLineF(mousePos,geometry.rotationHandle).length() <= 8) return HandleType::Rotate;
    for (size_t i=0; i<geometry.handles.size(); ++i) {
        const auto delta = mousePos-geometry.handles[i];
        if (std::abs(delta.x()) <= 6 && std::abs(delta.y()) <= 6)
            return static_cast<HandleType>(static_cast<int>(HandleType::TopLeft)+static_cast<int>(i));
    }
    return geometry.outline.containsPoint(mousePos,Qt::OddEvenFill) ? HandleType::Move : HandleType::None;
}

void PreviewWidget::updateFrameIfNeeded() {
    const auto rate=engine_.project().settings().fps;
    const auto duration=engine_.project().totalDuration();
    const auto time=std::min(engine_.playback().currentTime().floorToFrame(rate),core::TimelineTime::lastFrameTime(duration,rate));
    const auto canvas=engine_.project().settings().canvasSize;
    // Decode on the worker, composite at visible resolution (not a full 4K
    // export buffer on every GUI tick). Export retains its original resolution.
#if defined(__EMSCRIPTEN__)
    // Qt's browser backing store already scales the image to the viewport.
    // Keep live RGBA readback/compositing bounded; paused inspection stays sharp.
    const double previewWidth = engine_.playback().isPlaying() ? 480. : 1280.;
    const double previewHeight = engine_.playback().isPlaying() ? 270. : 720.;
#else
    constexpr double previewWidth = 1280., previewHeight = 720.;
#endif
    const double scale=std::min({1.,previewWidth/std::max(1,canvas.width),previewHeight/std::max(1,canvas.height),
        std::max(64,width())*devicePixelRatioF()/std::max(1,canvas.width),
        std::max(36,height())*devicePixelRatioF()/std::max(1,canvas.height)});
    const QSize size(std::max(2,qRound(canvas.width*scale)),std::max(2,qRound(canvas.height*scale)));
    if(!frameDirty_&&time.ticks()==lastRenderedTimeTicks_&&requestedSize_==size)return;
    if(!snapshot_)snapshot_=std::make_shared<media::PlaybackSnapshot>(engine_.project(),mediaLibrary_);
    // Explicit seeks already invalidate the generation. Small audio-clock
    // corrections during play must not discard an otherwise usable frame.
    if(!engine_.playback().isPlaying()&&lastRenderedTimeTicks_>=0&&time.ticks()<lastRenderedTimeTicks_)++generation_;
    worker_->request(snapshot_,time,size,generation_,engine_.playback().isPlaying()&&engine_.playback().playbackRate()==1.);
    lastRenderedTimeTicks_=time.ticks();requestedSize_=size;frameDirty_=false;
}

void PreviewWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const auto& pal = Theme::instance().palette();

    // Viewport background
    painter.fillRect(rect(), pal.panelBackground);

    const QRect canvasRect = this->canvasRect();
    const int dispW=canvasRect.width(), dispH=canvasRect.height();
    const int startX=canvasRect.x(), startY=canvasRect.y();

    // 1. Draw Canvas background color
    std::string customBg = engine_.project().settings().background.color;
    if (customBg.empty()) customBg = "#000000";
    painter.fillRect(canvasRect, QColor(QString::fromStdString(customBg)));

    // 2. Draw cached frame — just a blit, no decode, instant
    if (!cachedFrame_.isNull()) {
        if(lastPaintedSerial_!=presentedFrames_){lastPaintedSerial_=presentedFrames_;++paintedFrames_;}
        painter.drawImage(canvasRect, cachedFrame_);
    }

    // 3. Draw Canvas outer border
    painter.setPen(QPen(pal.border, 1.0));
    painter.drawRect(canvasRect);

    // 4. Draw Safe Zones & Guides (Action Safe 90%, Title Safe 80%, Center Crosshair)
    if (showSafeZones_) {
        // Action Safe (90%)
        int asPadX = static_cast<int>(dispW * 0.05);
        int asPadY = static_cast<int>(dispH * 0.05);
        QRect actionSafeRect(startX + asPadX, startY + asPadY, dispW - 2 * asPadX, dispH - 2 * asPadY);
        painter.setPen(QPen(QColor(56, 189, 248, 140), 1.0, Qt::DashLine));
        painter.drawRect(actionSafeRect);

        // Title Safe (80%)
        int tsPadX = static_cast<int>(dispW * 0.10);
        int tsPadY = static_cast<int>(dispH * 0.10);
        QRect titleSafeRect(startX + tsPadX, startY + tsPadY, dispW - 2 * tsPadX, dispH - 2 * tsPadY);
        painter.setPen(QPen(QColor(250, 204, 21, 140), 1.0, Qt::DashLine));
        painter.drawRect(titleSafeRect);

        // Center Crosshair
        painter.setPen(QPen(QColor(244, 244, 245, 160), 1.0));
        int cx = startX + dispW / 2;
        int cy = startY + dispH / 2;
        painter.drawLine(cx - 16, cy, cx + 16, cy);
        painter.drawLine(cx, cy - 16, cx, cy + 16);
    }

    // 5. Draw Center Alignment Snapping Guides
    if (showCenterGuideX_) {
        painter.setPen(QPen(QColor("#ef4444"), 1.0, Qt::DashLine));
        painter.drawLine(startX + dispW / 2, startY, startX + dispW / 2, startY + dispH);
    }
    if (showCenterGuideY_) {
        painter.setPen(QPen(QColor("#ef4444"), 1.0, Qt::DashLine));
        painter.drawLine(startX, startY + dispH / 2, startX + dispW, startY + dispH / 2);
    }

    // 6. Draw Selected Clip Bounding Box & Transform Handles
    const auto geometry = selectionGeometry();
    if (!geometry.outline.isEmpty()) {
        painter.setPen(QPen(QColor("#38bdf8"),1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(geometry.outline);
        painter.drawLine(geometry.handles[1],geometry.rotationHandle);
        painter.setBrush(QColor("#38bdf8"));
        painter.drawEllipse(geometry.rotationHandle,5,5);
        for (const auto& point : geometry.handles) {
            painter.setPen(QColor("#0284c7"));
            painter.setBrush(Qt::white);
            painter.drawRect(QRectF(point-QPointF(4,4),QSizeF(8,8)));
        }
    }
}

void PreviewWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier))) {
        isPanning_ = true;
        lastMousePos_ = event->pos();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const auto& sel = engine_.selectedClips();
        auto* tl = engine_.activeTimeline();
        if (!sel.empty() && tl) {
            const auto* clip = tl->findClip(sel[0]);
            if (clip) {
                double px = clip->getParam<double>("transform.positionX", 0.0);
                double py = clip->getParam<double>("transform.positionY", 0.0);
                double sx = clip->getParam<double>("transform.scaleX", 1.0);
                double sy = clip->getParam<double>("transform.scaleY", 1.0);

                const auto geometry = selectionGeometry();
                HandleType ht = hitTestHandles(event->position(), geometry);
                if (ht != HandleType::None) {
                    activeHandle_ = ht;
                    activeClipId_ = clip->id();
                    dragStartPos_ = event->pos();
                    origPosX_ = px;
                    origPosY_ = py;
                    origScaleX_ = sx;
                    origScaleY_ = sy;
                    origRotate_ = clip->getParam<double>("transform.rotate", 0.0);
                    dragGeometry_ = geometry;
                    dragParams_ = clip->params();
                    event->accept();
                    return;
                }
            }
        }
    }

    QWidget::mousePressEvent(event);
}

void PreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (isPanning_) {
        QPoint delta = event->pos() - lastMousePos_;
        panOffset_ += delta;
        lastMousePos_ = event->pos();
        update();
        event->accept();
        return;
    }

    if (activeHandle_ != HandleType::None) {
        auto* tl = engine_.activeTimeline();
        if (!tl) return;
        auto* clip = tl->findClip(activeClipId_);
        if (!clip) return;

        int canvasW = engine_.project().settings().canvasSize.width;
        int canvasH = engine_.project().settings().canvasSize.height;
        if (canvasW <= 0) canvasW = 1920;
        if (canvasH <= 0) canvasH = 1080;

        const auto viewport = canvasRect();
        QPoint delta = event->pos() - dragStartPos_;

        if (activeHandle_ == HandleType::Move) {
            double newX = origPosX_ + delta.x() * double(canvasW) / viewport.width();
            double newY = origPosY_ + delta.y() * double(canvasH) / viewport.height();

            // Snapping to center
            showCenterGuideX_ = (std::abs(newX) < 8.0);
            if (showCenterGuideX_) newX = 0.0;

            showCenterGuideY_ = (std::abs(newY) < 8.0);
            if (showCenterGuideY_) newY = 0.0;

            clip->setParam("transform.positionX", newX);
            clip->setParam("transform.positionY", newY);
        } else if (activeHandle_ == HandleType::Rotate) {
            const QPointF center = dragGeometry_.localToViewport.map(QPointF(0,0));
            const QPointF start = QPointF(dragStartPos_)-center, end = event->position()-center;
            const double angle = (std::atan2(end.y(),end.x())-std::atan2(start.y(),start.x()))*180/3.14159265358979323846;
            clip->setParam("transform.rotate", origRotate_+std::remainder(angle,360.));
        } else {
            bool invertible = false;
            const auto inverse = dragGeometry_.localToViewport.inverted(&invertible);
            if (!invertible) return;
            const auto index = static_cast<int>(activeHandle_)-static_cast<int>(HandleType::TopLeft);
            const QPointF anchor = inverse.map(dragGeometry_.handles[index]);
            const QPointF change = inverse.map(event->position())-inverse.map(QPointF(dragStartPos_));
            // Resize around the clip center. Corner handles preserve the
            // original (possibly non-uniform) scale ratio; side handles alter
            // only their own axis. The inverse also handles rotated media.
            const bool horizontal = activeHandle_ == HandleType::MidLeft || activeHandle_ == HandleType::MidRight;
            const bool vertical = activeHandle_ == HandleType::TopMid || activeHandle_ == HandleType::BotMid;
            const double denominator = horizontal ? anchor.x()*anchor.x() : vertical ? anchor.y()*anchor.y()
                : anchor.x()*anchor.x()+anchor.y()*anchor.y();
            if (denominator < 1e-9) return;
            const double numerator = horizontal ? change.x()*anchor.x() : vertical ? change.y()*anchor.y()
                : change.x()*anchor.x()+change.y()*anchor.y();
            const double largest = horizontal ? std::abs(origScaleX_) : vertical ? std::abs(origScaleY_)
                : std::max(std::abs(origScaleX_),std::abs(origScaleY_));
            const double smallest = horizontal ? std::abs(origScaleX_) : vertical ? std::abs(origScaleY_)
                : std::min(std::abs(origScaleX_),std::abs(origScaleY_));
            const double factor = std::clamp(1+numerator/denominator,std::min(1.,.05/std::max(.0001,smallest)),
                std::max(1.,10/std::max(.0001,largest)));
            if (!vertical) clip->setParam("transform.scaleX",origScaleX_*factor);
            if (!horizontal) clip->setParam("transform.scaleY",origScaleY_*factor);
        }

        engine_.project().setDirty(true);
        markFrameDirty();
        updateFrameIfNeeded();
        update();
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (isPanning_) {
        isPanning_ = false;
        event->accept();
        return;
    }

    if (activeHandle_ != HandleType::None) {
        if (auto* timeline = engine_.activeTimeline()) {
            if (auto* clip = timeline->findClip(activeClipId_)) {
                nlohmann::json patch = nlohmann::json::object();
                for (const char* key : {"transform.positionX","transform.positionY","transform.scaleX","transform.scaleY","transform.rotate"}) {
                    if (clip->hasParam(key) && (!dragParams_.contains(key) || dragParams_[key] != clip->params()[key])) {
                        patch[key] = clip->params()[key];
                        if (dragParams_.contains(key)) clip->params()[key] = dragParams_[key];
                        else clip->params().erase(key);
                    }
                }
                if (!patch.empty()) {
                    editor::ElementPatch change{}; change.clipId=activeClipId_; change.patchParams=std::move(patch);
                    engine_.history().execute(std::make_unique<editor::UpdateElementsCommand>(*timeline,std::vector<editor::ElementPatch>{change}));
                }
            }
        }
        activeHandle_ = HandleType::None;
        showCenterGuideX_ = false;
        showCenterGuideY_ = false;
        update();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void PreviewWidget::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        double delta = event->angleDelta().y();
        double factor = (delta > 0) ? 1.15 : (1.0 / 1.15);
        if (zoomFactor_ < 0.0) {
            zoomFactor_ = double(canvasRect().width())/std::max(1,engine_.project().settings().canvasSize.width);
        }
        zoomFactor_ = std::clamp(zoomFactor_ * factor, 0.05, 10.0);
        update();
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

} // namespace catchim::ui
#endif

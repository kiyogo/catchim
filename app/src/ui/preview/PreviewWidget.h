#pragma once

#if defined(HAVE_QT6)
#include <QWidget>
#include <QImage>
#include <QPolygonF>
#include <QTransform>
#include <array>
#include "editor/EditorEngine.h"
#include "render/RenderEngine.h"
#include "media/MediaLibrary.h"
#include "media/PlaybackSnapshot.h"
#include "media/decoder/NativeVideoDecoder.h"

namespace catchim::ui {
class PreviewRenderWorker;

class PreviewWidget : public QWidget {
    Q_OBJECT
public:
    PreviewWidget(
        editor::EditorEngine& engine,
        render::RenderEngine& renderEngine,
        media::MediaLibrary& mediaLibrary,
        QWidget* parent = nullptr
    );
    ~PreviewWidget() override;
    uint64_t presentedFrames() const {return presentedFrames_;}
    uint64_t paintedFrames() const {return paintedFrames_;}
    double presentedSeconds() const {return presentedSeconds_;}
    double lastRenderMilliseconds() const {return lastRenderMs_;}
    const QImage& presentedImage() const {return cachedFrame_;}
    const media::NativeVideoDecoder::Statistics& gpuStatistics() const {return decoderStatistics_;}

    void setZoomFactor(double factor) { zoomFactor_ = factor; update(); }
    double zoomFactor() const noexcept { return zoomFactor_; }
    void fitToScreen() { zoomFactor_ = -1.0; update(); }
    // Logical widget coordinates, shared by painting and pointer hit testing.
    QPolygonF selectionOutline() const;

protected:
    void paintEvent(QPaintEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    enum class HandleType {
        None,
        Move,
        TopLeft, TopMid, TopRight,
        MidLeft, MidRight,
        BotLeft, BotMid, BotRight,
        Rotate
    };

    struct SelectionGeometry {
        QPolygonF outline;
        std::array<QPointF, 8> handles;
        QPointF rotationHandle;
        QTransform localToViewport;
        QRectF localBounds;
    };
    QRect canvasRect() const;
    SelectionGeometry selectionGeometry() const;
    HandleType hitTestHandles(const QPointF& mousePos, const SelectionGeometry& geometry) const;

    editor::EditorEngine& engine_;
    render::RenderEngine& renderEngine_;
    media::MediaLibrary& mediaLibrary_;

    double zoomFactor_{-1.0}; // -1.0 means auto fit
    QPoint panOffset_{0, 0};
    QPoint lastMousePos_;
    bool isPanning_{false};

    HandleType activeHandle_{HandleType::None};
    core::ClipId activeClipId_;
    QPoint dragStartPos_;
    double origPosX_{0.0};
    double origPosY_{0.0};
    double origScaleX_{1.0};
    double origScaleY_{1.0};
    double origRotate_{0.0};
    SelectionGeometry dragGeometry_;
    nlohmann::json dragParams_;

    bool showCenterGuideX_{false};
    bool showCenterGuideY_{false};
    bool showSafeZones_{false};

    // Worker-produced frame cache. Decoding/compositing never runs in paintEvent.
    QImage      cachedFrame_;               // last rendered frame as QImage
    int64_t     lastRenderedTimeTicks_{-1}; // ticks of last render (-1 = never)
    bool        frameDirty_{true};          // true = need re-render
    std::unique_ptr<PreviewRenderWorker> worker_;
    std::shared_ptr<const media::PlaybackSnapshot> snapshot_;
    uint64_t generation_{0},presentedFrames_{0};
    uint64_t paintedFrames_{0},lastPaintedSerial_{0};
    double presentedSeconds_{-1},lastRenderMs_{0};
    QSize requestedSize_;
    media::NativeVideoDecoder::Statistics decoderStatistics_;
    friend class PreviewRenderWorker;

public:
    void toggleSafeZones() { showSafeZones_ = !showSafeZones_; update(); }
    bool isSafeZonesEnabled() const noexcept { return showSafeZones_; }

    // Enqueues/replaces a background request, without blocking the GUI.
    void updateFrameIfNeeded();
    void markFrameDirty() { frameDirty_ = true; snapshot_.reset(); ++generation_; }
    void invalidateSeek() { frameDirty_=true; ++generation_; }
};

} // namespace catchim::ui
#endif

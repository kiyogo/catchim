#pragma once

#if defined(HAVE_QT6)
#include <QWidget>
#include <QHash>
#include <QImage>
#include <QSet>
#include <QThreadPool>
#include <QElapsedTimer>
#include "editor/EditorEngine.h"
#include "media/MediaLibrary.h"

namespace catchim::ui {

class TimelineTracksWidget : public QWidget {
    Q_OBJECT
public:
    TimelineTracksWidget(
        editor::EditorEngine& engine,
        media::MediaLibrary& mediaLibrary,
        QWidget* parent = nullptr
    );
    ~TimelineTracksWidget() override;

    void setZoomFactor(double zoom) { zoomFactor_ = zoom; update(); }
    void setScrollOffset(int x, int y) { scrollX_ = x; scrollY_ = y; update(); }

    int totalTracksHeight() const;
    QSize sizeHint() const override;
    int viewportXAtTime(core::TimelineTime time) const {return timeToPixel(time);}
    core::TimelineTime timeAtViewportX(int x) const {return pixelToTime(x);}

signals:
    void mediaRevealRequested(core::MediaId mediaId);
    void seekRequested(core::TimelineTime time);
    void clipSelected(core::ClipId clipId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool event(QEvent* event) override;

private:
    enum class DragMode { None, MovingClip, TrimmingStart, TrimmingEnd, BoxSelecting };

    struct HitTestResult {
        const editor::Track* track{nullptr};
        const editor::Clip* clip{nullptr};
        bool isTrimStart{false};
        bool isTrimEnd{false};
        bool isMuteBtn{false};
        bool isHideBtn{false};
        bool isLockBtn{false};
    };

    HitTestResult hitTest(const QPoint& pos) const;
    editor::Track* findTrackAtY(int y);
    const editor::Track* findTrackAtY(int y) const;
    core::TimelineTime pixelToTime(int pixelX) const;
    int timeToPixel(core::TimelineTime time) const;
    QImage thumbnail(const editor::Clip& clip, double sourceTime);

    editor::EditorEngine& engine_;
    media::MediaLibrary& mediaLibrary_;

    double zoomFactor_{8.82};
    int scrollX_{0};
    int scrollY_{0};

    DragMode dragMode_{DragMode::None};
    core::ClipId activeClipId_;
    QPoint dragStartPos_;
    core::TimelineTime dragStartTime_;
    core::TimelineTime dragStartDuration_;
    int snapIndicatorX_{-1};
    QRect dropIndicatorRect_;
    bool showDropIndicator_{false};
    std::optional<editor::TimelineTracksSnapshot> dragSnapshot_;
    editor::Timeline* dragTimeline_{nullptr};
    bool dragEdited_{false};
    QElapsedTimer dragRefresh_;
    QHash<QString,QImage> thumbnails_;
    QHash<QString,QImage> thumbnailCovers_;
    QSet<QString> pendingThumbnails_;
    QThreadPool thumbnailPool_;
};

} // namespace catchim::ui
#endif

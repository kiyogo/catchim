#pragma once

#if defined(HAVE_QT6)
#include <QWidget>
#include <QScrollArea>
#include <QScrollBar>
#include "TimelineToolbar.h"
#include "TimelineRulerWidget.h"
#include "TimelineTracksWidget.h"
#include "editor/EditorEngine.h"
#include "media/MediaLibrary.h"

namespace catchim::ui {

class TimelinePanel : public QWidget {
    Q_OBJECT
public:
    TimelinePanel(
        editor::EditorEngine& engine,
        media::MediaLibrary& mediaLibrary,
        QWidget* parent = nullptr
    );

    void refresh();
    void updatePlaybackPosition();
    void revealPlayhead();
    double zoomFactor() const noexcept {return zoomFactor_;}
protected:
    void resizeEvent(QResizeEvent*) override;
    bool eventFilter(QObject*,QEvent*) override;

signals:
    void mediaRevealRequested(core::MediaId mediaId);
    void clipSelected(core::ClipId clipId);

private slots:
    void onZoomChanged(double zoomFactor);
    void onSeekRequested(core::TimelineTime time);

private:
    void setupUi();
    void updateScrollRanges();

    editor::EditorEngine& engine_;
    media::MediaLibrary& mediaLibrary_;

    TimelineToolbar* toolbar_{nullptr};
    TimelineRulerWidget* rulerWidget_{nullptr};
    TimelineTracksWidget* tracksWidget_{nullptr};
    QScrollArea* scrollArea_{nullptr};
    QScrollBar* horizontalBar_{nullptr};
    QScrollBar* verticalBar_{nullptr};
    double zoomFactor_{8.82};
    double previousDuration_{0};
    core::SceneId viewSceneId_;
    int lastPlayheadX_{-1};
};

} // namespace catchim::ui
#endif

#pragma once

#if defined(HAVE_QT6)
#include <QWidget>
#include "editor/EditorEngine.h"

namespace catchim::ui {

class TimelineRulerWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineRulerWidget(editor::EditorEngine& engine, QWidget* parent = nullptr);

    void setZoomFactor(double zoom) { zoomFactor_ = zoom; update(); }
    void setScrollOffset(int offset) { scrollOffset_ = offset; update(); }

signals:
    void seekRequested(core::TimelineTime time);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    core::TimelineTime pixelToTime(int pixelX) const;

    editor::EditorEngine& engine_;
    double zoomFactor_{8.82};
    int scrollOffset_{0};
    bool isDragging_{false};
    int dragOffsetX_{0};
};

} // namespace catchim::ui
#endif

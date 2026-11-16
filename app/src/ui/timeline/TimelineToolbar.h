#pragma once

#if defined(HAVE_QT6)
#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include "editor/EditorEngine.h"
#include "media/MediaLibrary.h"

namespace catchim::ui {

class TimelineToolbar : public QWidget {
    Q_OBJECT
public:
    explicit TimelineToolbar(editor::EditorEngine& engine, media::MediaLibrary& mediaLibrary, QWidget* parent = nullptr);

    void refresh();
    void setZoomFactor(double factor);

signals:
    void zoomChanged(double zoomFactor);

private slots:
    void onSplitClicked();
    void onDeleteClicked();
    void onDuplicateClicked();
    void onSnappingToggled();
    void onRippleToggled();
    void onZoomSliderChanged(int val);

private:
    void setupUi();

    editor::EditorEngine& engine_;
    media::MediaLibrary& mediaLibrary_;

    QPushButton* splitLeftBtn_{nullptr};
    QPushButton* splitBtn_{nullptr};
    QPushButton* splitRightBtn_{nullptr};
    QPushButton* unlinkBtn_{nullptr};
    QPushButton* deleteBtn_{nullptr};
    QPushButton* dupBtn_{nullptr};
    QPushButton* bookmarkBtn_{nullptr};
    QPushButton* sceneCutsBtn_{nullptr};
    bool isDetectingSceneCuts_{false};
    QPushButton* sceneLabelBtn_{nullptr};
    QPushButton* sceneLayersBtn_{nullptr};
    QPushButton* snapBtn_{nullptr};
    QPushButton* rippleBtn_{nullptr};
    QSlider* zoomSlider_{nullptr};
};

} // namespace catchim::ui
#endif

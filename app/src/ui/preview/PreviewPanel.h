#pragma once

#if defined(HAVE_QT6)
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include "PreviewWidget.h"
#include "editor/EditorEngine.h"
#include "render/RenderEngine.h"
#include "media/MediaLibrary.h"

namespace catchim::ui {

class PreviewPanel : public QWidget {
    Q_OBJECT
public:
    PreviewPanel(
        editor::EditorEngine& engine,
        render::RenderEngine& renderEngine,
        media::MediaLibrary& mediaLibrary,
        QWidget* parent = nullptr
    );

    void refresh();
    void updateFrame();        // decode current frame (called from app tick)
    void markFrameDirty();     // force re-render on next updateFrame()
    void toggleSafeZones();
    void toggleFullscreen();

    QSize sizeHint() const override { return QSize(760, 600); }
    QSize minimumSizeHint() const override { return QSize(320, 180); }

private slots:
    void onPlayPauseClicked();
    void onZoomChanged(int index);

private:
    void setupUi();

    editor::EditorEngine& engine_;
    PreviewWidget* previewWidget_{nullptr};
    QLabel* timecodeLabel_{nullptr};
    QPushButton* playPauseBtn_{nullptr};
    QComboBox* zoomCombo_{nullptr};
};

} // namespace catchim::ui
#endif

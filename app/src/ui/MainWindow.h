#pragma once

#if defined(HAVE_QT6)
#include <QMainWindow>
#include <QSplitter>
#include <QTimer>
#include "editor/EditorEngine.h"
#include "media/MediaLibrary.h"
#include "render/RenderEngine.h"
#include "ui/shell/EditorHeader.h"
#include "ui/panels/assets/AssetsPanel.h"
#include "ui/preview/PreviewPanel.h"
#include "ui/panels/properties/PropertiesPanel.h"
#include "ui/timeline/TimelinePanel.h"

#include "audio/PlaybackAudioSession.h"

namespace catchim::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(
        editor::EditorEngine& engine,
        media::MediaLibrary& mediaLibrary,
        render::RenderEngine& renderEngine,
        QWidget* parent = nullptr
    );
    ~MainWindow() override;
    audio::PlaybackAudioSession::Statistics playbackAudioStatistics() const {return audioSession_.statistics();}
    void setPlaybackOutputVolume(float volume){audioSession_.setVolume(volume);}

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onAppTick();
    void onExportRequested();
    void onThemeToggleRequested();
    void onOpenProject();
    void onSaveProject();
    void onOpenProjectsHub();

private:
    void setupUi();
    void setupShortcuts();
    void setupEngineCallbacks();
    void refreshPlaybackSnapshot();

    editor::EditorEngine& engine_;
    media::MediaLibrary& mediaLibrary_;
    render::RenderEngine& renderEngine_;

    audio::PlaybackAudioSession audioSession_;
    std::shared_ptr<const media::PlaybackSnapshot> playbackSnapshot_;

    EditorHeader* header_{nullptr};
    QSplitter* verticalSplitter_{nullptr};
    QSplitter* horizontalSplitter_{nullptr};

    AssetsPanel* assetsPanel_{nullptr};
    PreviewPanel* previewPanel_{nullptr};
    PropertiesPanel* propertiesPanel_{nullptr};
    TimelinePanel* timelinePanel_{nullptr};

    QTimer* tickTimer_{nullptr};
};

} // namespace catchim::ui
#endif

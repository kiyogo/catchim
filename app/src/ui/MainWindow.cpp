#include "MainWindow.h"

#if defined(HAVE_QT6)
#include "app/AppConfig.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EditorLanguage.h"
#include "ui/export/ExportDialog.h"
#include "ui/projects/ProjectsHubDialog.h"
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QFileDialog>
#include <QProgressDialog>
#include <QProcess>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QFile>
#include <QPushButton>
#include <QPainter>
#include <QSplitterHandle>
#include <cmath>

namespace catchim::ui {

class EditorSplitterHandle final:public QSplitterHandle {
public:
    EditorSplitterHandle(Qt::Orientation orientation,QSplitter* parent):QSplitterHandle(orientation,parent) {
        setObjectName("editorSplitterHandle");setMouseTracking(true);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        const auto& palette=Theme::instance().palette();
        painter.fillRect(rect(),palette.panelBackground);
        painter.setPen(Qt::NoPen);painter.setBrush(underMouse()?palette.textSecondary:palette.border);
        if(orientation()==Qt::Vertical)painter.drawRoundedRect(QRectF(width()/2.-16,height()/2.-1,32,2),1,1);
        else painter.drawRoundedRect(QRectF(width()/2.-1,height()/2.-16,2,32),1,1);
    }
    void enterEvent(QEnterEvent* event) override {QSplitterHandle::enterEvent(event);update();}
    void leaveEvent(QEvent* event) override {QSplitterHandle::leaveEvent(event);update();}
};
class EditorSplitter final:public QSplitter {
public:using QSplitter::QSplitter;
protected:QSplitterHandle* createHandle() override {return new EditorSplitterHandle(orientation(),this);}
};

MainWindow::MainWindow(
    editor::EditorEngine& engine,
    media::MediaLibrary& mediaLibrary,
    render::RenderEngine& renderEngine,
    QWidget* parent
)
    : QMainWindow(parent)
    , engine_(engine)
    , mediaLibrary_(mediaLibrary)
    , renderEngine_(renderEngine)
{
    setWindowTitle("Catchim Video Editor");
    resize(1440, 900);

    qApp->setProperty("editorLanguage",QSettings().value("ui/language","vi").toString());
    setupUi();
    setupEngineCallbacks();

    // Listen to theme changes to refresh all sub-panels
    Theme::instance().addListener("MainWindow", [this](ThemeMode /*mode*/) {
        if (header_) header_->refresh();
        if (assetsPanel_) assetsPanel_->refresh();
        if (previewPanel_) previewPanel_->refresh();
        if (timelinePanel_) timelinePanel_->refresh();
        if (propertiesPanel_) propertiesPanel_->refresh();
        if (centralWidget()) centralWidget()->update();
        if (verticalSplitter_) verticalSplitter_->update();
        if (horizontalSplitter_) horizontalSplitter_->update();
        update();
    });

    // 60fps tick timer for playback and rendering
    tickTimer_ = new QTimer(this);
    tickTimer_->setTimerType(Qt::PreciseTimer);
    connect(tickTimer_, &QTimer::timeout, this, &MainWindow::onAppTick);
#if defined(__EMSCRIPTEN__)
    tickTimer_->start(33); // browser video cadence; leave time for media callbacks
#else
    tickTimer_->start(16); // ~60 Hz
#endif

    refreshPlaybackSnapshot();
    applyEditorLanguage(this);
}

MainWindow::~MainWindow() {
    tickTimer_->stop();
    engine_.setOnProjectChanged({});engine_.setOnTimelineChanged({});engine_.setOnSelectionChanged({});
    engine_.playback().setOnPlayStateChanged({});engine_.playback().setOnTimeChanged({});engine_.playback().setOnSeek({});
    Theme::instance().removeListener("MainWindow");
}

void MainWindow::setupUi() {
    auto* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    centralWidget->setAttribute(Qt::WA_StyledBackground, true);
    setCentralWidget(centralWidget);

    auto* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 1. Top Header
    header_ = new EditorHeader(engine_, this);
    header_->setObjectName("editorHeader");
    connect(header_, &EditorHeader::exportClicked, this, &MainWindow::onExportRequested);
    connect(header_, &EditorHeader::themeToggleClicked, this, &MainWindow::onThemeToggleRequested);
    connect(header_, &EditorHeader::openProjectRequested, this, &MainWindow::onOpenProject);
    connect(header_, &EditorHeader::saveProjectRequested, this, &MainWindow::onSaveProject);
    connect(header_, &EditorHeader::projectsHubRequested, this, &MainWindow::onOpenProjectsHub);
    rootLayout->addWidget(header_);

    // 2. Main vertical splitter (Top: Panels, Bottom: Timeline) inside an editor body container
    auto* editorBody = new QWidget(centralWidget);
    editorBody->setObjectName("editorBody");
    editorBody->setAttribute(Qt::WA_StyledBackground, true);
    auto* bodyLayout = new QVBoxLayout(editorBody);
    bodyLayout->setContentsMargins(Metrics::editorMarginLeft, 0, Metrics::editorMarginRight, Metrics::editorMarginBottom);
    bodyLayout->setSpacing(0);

    verticalSplitter_ = new EditorSplitter(Qt::Vertical, editorBody);
    verticalSplitter_->setObjectName("verticalSplitter");
    verticalSplitter_->setHandleWidth(Metrics::splitterGap);

    // 3. Top horizontal splitter (Assets, Preview, Properties)
    horizontalSplitter_ = new EditorSplitter(Qt::Horizontal, verticalSplitter_);
    horizontalSplitter_->setObjectName("horizontalSplitter");
    horizontalSplitter_->setHandleWidth(Metrics::splitterGap);

    assetsPanel_ = new AssetsPanel(engine_, mediaLibrary_, horizontalSplitter_);
    assetsPanel_->setObjectName("assetsPanel");
    assetsPanel_->setProperty("class", "Panel");
    assetsPanel_->setMinimumWidth(280);

    previewPanel_ = new PreviewPanel(engine_, renderEngine_, mediaLibrary_, horizontalSplitter_);
    previewPanel_->setObjectName("previewPanel");
    previewPanel_->setProperty("class", "Panel");
    previewPanel_->setMinimumWidth(380);

    propertiesPanel_ = new PropertiesPanel(engine_, horizontalSplitter_);
    propertiesPanel_->setObjectName("propertiesPanel");
    propertiesPanel_->setProperty("class", "Panel");
    propertiesPanel_->setMinimumWidth(280);

    horizontalSplitter_->addWidget(assetsPanel_);
    horizontalSplitter_->addWidget(previewPanel_);
    horizontalSplitter_->addWidget(propertiesPanel_);

    horizontalSplitter_->setCollapsible(0, false);
    horizontalSplitter_->setCollapsible(1, false);
    horizontalSplitter_->setCollapsible(2, false);

    // Initial sizes matching 1.png (470px Assets, 940px Preview, 470px Properties)
    horizontalSplitter_->setSizes({470, 940, 470});

    // Set horizontal proportions (25% Assets, 50% Preview, 25% Properties)
    horizontalSplitter_->setStretchFactor(0, 1);
    horizontalSplitter_->setStretchFactor(1, 1);
    horizontalSplitter_->setStretchFactor(2, 1);

    verticalSplitter_->addWidget(horizontalSplitter_);

    // 4. Bottom Timeline
    timelinePanel_ = new TimelinePanel(engine_, mediaLibrary_, verticalSplitter_);
    connect(timelinePanel_, &TimelinePanel::mediaRevealRequested, assetsPanel_, &AssetsPanel::revealMedia);
    timelinePanel_->setObjectName("timelinePanel");
    timelinePanel_->setProperty("class", "Panel");
    timelinePanel_->setMinimumHeight(180);
    connect(timelinePanel_, &TimelinePanel::clipSelected, [this](core::ClipId /* id */) {
        propertiesPanel_->refresh();
        applyEditorLanguage(this);
    });

    verticalSplitter_->addWidget(timelinePanel_);
    verticalSplitter_->setCollapsible(0, false);
    verticalSplitter_->setCollapsible(1, false);

    // Initial sizes matching 1.png (448px Panels, 436px Timeline)
    verticalSplitter_->setSizes({440, 440});

    // Proportions (51% Panels, 49% Timeline)
    verticalSplitter_->setStretchFactor(0, 1);
    verticalSplitter_->setStretchFactor(1, 1);

    bodyLayout->addWidget(verticalSplitter_, 1);
    rootLayout->addWidget(editorBody, 1);
}

void MainWindow::setupEngineCallbacks() {
    engine_.setOnProjectChanged([this]() {
        refreshPlaybackSnapshot();
        header_->refresh();
        assetsPanel_->refresh();
        previewPanel_->markFrameDirty(); // invalidate frame cache on project change
        previewPanel_->refresh();
        timelinePanel_->refresh();
        propertiesPanel_->refresh();
        applyEditorLanguage(this);
    });

    engine_.setOnTimelineChanged([this]() {
        refreshPlaybackSnapshot();
        previewPanel_->markFrameDirty(); // invalidate frame cache when clips change
        previewPanel_->refresh();
        timelinePanel_->refresh();
        propertiesPanel_->refresh();
        applyEditorLanguage(this);
    });

    engine_.setOnSelectionChanged([this]() {
        propertiesPanel_->refresh();
        timelinePanel_->refresh();
        applyEditorLanguage(this);
    });

    engine_.playback().setOnPlayStateChanged([this](bool playing) {
        audioSession_.setState(playbackSnapshot_,engine_.playback().currentTime(),playing&&engine_.playback().playbackRate()==1.);
        if(auto* preview=previewPanel_->findChild<PreviewWidget*>())preview->invalidateSeek();
        previewPanel_->refresh();
    });

    engine_.playback().setOnSeek([this](core::TimelineTime t) {
        audioSession_.setState(playbackSnapshot_,t,engine_.playback().isPlaying()&&engine_.playback().playbackRate()==1.);
        if(auto* preview=previewPanel_->findChild<PreviewWidget*>())preview->invalidateSeek();
        previewPanel_->updateFrame();
        timelinePanel_->revealPlayhead();timelinePanel_->updatePlaybackPosition();
    });
}

void MainWindow::refreshPlaybackSnapshot() {
    playbackSnapshot_=std::make_shared<media::PlaybackSnapshot>(engine_.project(),mediaLibrary_);
    audioSession_.setState(playbackSnapshot_,engine_.playback().currentTime(),engine_.playback().isPlaying()&&engine_.playback().playbackRate()==1.);
}

void MainWindow::onAppTick() {
    if (engine_.playback().isPlaying()) {
        // Buffering does not block the UI and does not let video race ahead.
        if(audioSession_.preparing())engine_.playback().synchronizePosition(audioSession_.requestedPosition());
        else if(auto position=audioSession_.audiblePosition()) {
#if defined(__EMSCRIPTEN__)
            // WebAudio packet-ended callbacks arrive in browser event batches.
            // Re-seeking to an unchanged packet position each GUI tick freezes
            // the video clock between batches. Extrapolate between anchors and
            // correct larger drift, keeping video/audio within 100 ms.
            if(std::abs((engine_.playback().currentTime()-*position).toSeconds())>.1)
                engine_.playback().synchronizePosition(*position);
#else
            engine_.playback().synchronizePosition(*position);
#endif
        }
        engine_.update();
    }
    // Also schedule paused seeks/edits; paintEvent only blits a ready image.
    previewPanel_->updateFrame();
    timelinePanel_->updatePlaybackPosition();
}

void MainWindow::onExportRequested() {
    ExportDialog dlg(engine_, mediaLibrary_, renderEngine_, this);
    dlg.exec();
}

void MainWindow::onThemeToggleRequested() {
    auto currentMode = Theme::instance().mode();
    auto newMode = (currentMode == ThemeMode::Dark ? ThemeMode::Light : ThemeMode::Dark);
    Theme::instance().setTheme(newMode);

    auto cfg = app::AppConfig::load();
    cfg.theme = (newMode == ThemeMode::Dark ? "dark" : "light");
    cfg.save();
}

void MainWindow::onOpenProject() {
    QString path = QFileDialog::getOpenFileName(this, "Open Catchim Project", "", "Catchim Project (*.json *.catchim);;All Files (*.*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray fileData = file.readAll();
        if (engine_.loadProjectFromJson(fileData.toStdString())) {
            engine_.notifyProjectChanged();
        }
    }
}

void MainWindow::onSaveProject() {
    QString defaultName = QString::fromStdString(engine_.project().name()) + ".json";
    QString path = QFileDialog::getSaveFileName(this, "Save Catchim Project", defaultName, "Catchim Project (*.json);;All Files (*.*)");
    if (path.isEmpty()) return;

    std::string jsonStr = engine_.saveProjectToJson();
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(jsonStr.data(), jsonStr.size());
        file.close();
    }
}

void MainWindow::onOpenProjectsHub() {
    ProjectsHubDialog dlg(this);
    connect(&dlg, &ProjectsHubDialog::projectSelected, [this](const QString& filePath) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray data = file.readAll();
            if (engine_.loadProjectFromJson(data.toStdString())) {
                engine_.notifyProjectChanged();
            }
        }
    });
    dlg.exec();
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    // Project Save / Open shortcuts
    if (event->key() == Qt::Key_O && (event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier)) {
        onOpenProjectsHub();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_S && (event->modifiers() & Qt::ControlModifier)) {
        onSaveProject();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_O && (event->modifiers() & Qt::ControlModifier)) {
        onOpenProject();
        event->accept();
        return;
    }

    // 1. Playback & Navigation
    if (event->key() == Qt::Key_Space) {
        engine_.togglePlay();
        previewPanel_->refresh();
        event->accept();
        return;
    }

    if (event->modifiers() == Qt::NoModifier) {
        if (event->key() == Qt::Key_J) {
            engine_.playback().setPlaybackRate(-1.5);
            if (!engine_.playback().isPlaying()) engine_.play();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_K) {
            engine_.pause();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_L) {
            engine_.playback().setPlaybackRate(1.5);
            if (!engine_.playback().isPlaying()) engine_.play();
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Left) {
        const auto& fps = engine_.project().settings().fps;
        engine_.playback().stepFrame(-1, fps);
        previewPanel_->refresh();
        timelinePanel_->refresh();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Right) {
        const auto& fps = engine_.project().settings().fps;
        engine_.playback().stepFrame(1, fps);
        previewPanel_->refresh();
        timelinePanel_->refresh();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Home) {
        engine_.seek(core::TimelineTime::zero());
        previewPanel_->refresh();
        timelinePanel_->refresh();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_End) {
        auto* tl = engine_.activeTimeline();
        if (tl) {
            engine_.seek(tl->totalDuration());
            previewPanel_->refresh();
            timelinePanel_->refresh();
        }
        event->accept();
        return;
    }

    // 2. Timeline Editing
    if (event->key() == Qt::Key_S && event->modifiers() == Qt::NoModifier) {
        engine_.splitAtPlayhead();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Q && event->modifiers() == Qt::NoModifier) {
        engine_.splitLeftAtPlayhead();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_W && event->modifiers() == Qt::NoModifier) {
        engine_.splitRightAtPlayhead();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        engine_.deleteSelectedClips();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_R && event->modifiers() == Qt::NoModifier) {
        engine_.toggleRipple();
        timelinePanel_->refresh();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_N && event->modifiers() == Qt::NoModifier) {
        engine_.toggleSnapping();
        timelinePanel_->refresh();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_M && event->modifiers() == Qt::NoModifier) {
        engine_.toggleBookmarkAtPlayhead();
        timelinePanel_->refresh();
        event->accept();
        return;
    }

    // 3. History & Selection
    if (event->matches(QKeySequence::Undo)) {
        engine_.undo();
        event->accept();
        return;
    }

    if (event->matches(QKeySequence::Redo)) {
        engine_.redo();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_D && (event->modifiers() & Qt::ControlModifier)) {
        engine_.duplicateSelectedClips();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_A && (event->modifiers() & Qt::ControlModifier)) {
        auto* tl = engine_.activeTimeline();
        if (tl) {
            std::vector<core::ClipId> allIds;
            for (const auto* track : tl->allTracks()) {
                for (const auto& c : track->clips()) {
                    allIds.push_back(c.id());
                }
            }
            engine_.selectClips(allIds, false);
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        engine_.deselectAll();
        event->accept();
        return;
    }

    // 4. Preview & Export
    if (event->key() == Qt::Key_E && (event->modifiers() & Qt::ControlModifier)) {
        onExportRequested();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_F11 || (event->key() == Qt::Key_F && event->modifiers() == Qt::NoModifier)) {
        previewPanel_->toggleFullscreen();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Z && event->modifiers() == Qt::NoModifier) {
        previewPanel_->toggleSafeZones();
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    // QSplitter retains the user's panel proportions while the window resizes.
}

} // namespace catchim::ui
#endif

#pragma once

#if defined(HAVE_QT6)
#include <QThread>
#if !defined(__EMSCRIPTEN__)
#include <QProcess>
#endif
#include <QString>
#include <QStringList>
#include <atomic>
#include <chrono>
#include "editor/EditorEngine.h"
#include "media/MediaLibrary.h"
#include "render/RenderEngine.h"

namespace catchim::ui {

class ExportWorker : public QThread {
    Q_OBJECT
public:
    ExportWorker(
        const editor::Project& project,
        const media::MediaLibrary& mediaLibrary,
        int width,
        int height,
        double fps,
        int totalFrames,
        const QString& ffmpegPath,
        const QStringList& ffmpegArgs,
        const QString& savePath,
        QObject* parent = nullptr,
        bool includeAudio = false
    );
    ~ExportWorker() override;

    void cancel();

signals:
    void progressUpdated(int currentFrame, int totalFrames, double currentFps, int percent);
    void logOutputReceived(const QString& line);
    void exportFinished(bool success, const QString& message);

protected:
    void run() override;

private:
#if !defined(__EMSCRIPTEN__)
    bool tryDirectExport();
#endif
    editor::Project project_;
    media::MediaLibrary mediaLibrary_;
    int width_{1920};
    int height_{1080};
    double fps_{30.0};
    int totalFrames_{1};
    QString ffmpegPath_;
    QStringList ffmpegArgs_;
    QString savePath_;
    bool includeAudio_{false};

    std::atomic<bool> cancelRequested_{false};
};

} // namespace catchim::ui
#endif

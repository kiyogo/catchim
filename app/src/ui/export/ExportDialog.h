#pragma once

#if defined(HAVE_QT6)
#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <memory>
#include <QPlainTextEdit>
#include "editor/EditorEngine.h"
#include "media/MediaLibrary.h"
#include "render/RenderEngine.h"
#include "ExportWorker.h"

namespace catchim::ui {

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    ExportDialog(
        editor::EditorEngine& engine,
        media::MediaLibrary& mediaLibrary,
        render::RenderEngine& renderEngine,
        QWidget* parent = nullptr
    );
    ~ExportDialog() override;
    void reject() override;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onResolutionChanged();
    void onFormatChanged();
    void onStartExport();
    void onCancelExport();
    void onToggleTechLog();

    void onWorkerProgress(int currentFrame, int totalFrames, double currentFps, int percent);
    void onWorkerLog(const QString& line);
    void onWorkerFinished(bool success, const QString& message);

private:
    void setupUi();
    void updateInfoLabel();
    QSize getTargetResolution() const;
    // Probe available GPU encoders via FFmpeg and populate hwAccelCombo_
    void probeGpuEncoders();

    editor::EditorEngine& engine_;
    media::MediaLibrary& mediaLibrary_;
    render::RenderEngine& renderEngine_;

    QComboBox* resolutionCombo_{nullptr};
    QComboBox* formatCombo_{nullptr};
    QComboBox* qualityCombo_{nullptr};
    QComboBox* audioCombo_{nullptr};
    QComboBox* hwAccelCombo_{nullptr};  // GPU HW encoder selector
    QLabel* gpuStatusLabel_{nullptr};   // Shows detected GPU info
    QLabel* infoLabel_{nullptr};

    QWidget* settingsWidget_{nullptr};
    QWidget* progressWidget_{nullptr};
    QProgressBar* progressBar_{nullptr};
    QLabel* progressStatusLabel_{nullptr};
    QLabel* telemetryFpsLabel_{nullptr};
    QPushButton* toggleLogBtn_{nullptr};
    QPlainTextEdit* techLogBox_{nullptr};

    QPushButton* exportBtn_{nullptr};
    QPushButton* cancelBtn_{nullptr};
    QLabel* statusBanner_{nullptr};
    QWidget* modalBackdrop_{nullptr};

    void showStatusMessage(const QString& msg, const QString& type = "error");

    ExportWorker* worker_{nullptr};
    bool isExporting_{false};
    std::chrono::steady_clock::time_point exportStartTime_;
};

} // namespace catchim::ui
#endif

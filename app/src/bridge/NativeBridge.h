#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace catchim::bridge {

class NativeBridge : public QObject {
    Q_OBJECT

public:
    explicit NativeBridge(QObject* parent = nullptr);
    virtual ~NativeBridge() = default;

    // --- File Dialogs ---
    Q_INVOKABLE QString openFileDialog(const QString& title = "Select File",
                                      const QString& filter = "All Files (*.*)");
    Q_INVOKABLE QStringList openMultipleFilesDialog(const QString& title = "Select Files",
                                                   const QString& filter = "All Files (*.*)");
    Q_INVOKABLE QString saveFileDialog(const QString& title = "Save File As",
                                      const QString& defaultName = "",
                                      const QString& filter = "All Files (*.*)");
    Q_INVOKABLE QString selectFolderDialog(const QString& title = "Select Folder");
    Q_INVOKABLE void openInExplorer(const QString& filePath);

    // --- System Capabilities ---
    Q_INVOKABLE QVariantMap getSystemCapabilities();

    // --- Project Persistence ---
    Q_INVOKABLE QString getProjectsDirectory();
    Q_INVOKABLE QStringList listSavedProjects();
    Q_INVOKABLE QString loadProjectFile(const QString& projectId);
    Q_INVOKABLE bool saveProjectFile(const QString& projectId, const QString& projectJson);
    Q_INVOKABLE bool deleteProjectFile(const QString& projectId);

    // --- Native Export ---
    Q_INVOKABLE void startNativeExport(const QString& projectJson,
                                       const QString& outputPath,
                                       const QString& resolution,
                                       int fps);
    Q_INVOKABLE void cancelNativeExport();

    // --- App Info ---
    Q_INVOKABLE QString getAppVersion() const;
    Q_INVOKABLE bool isNativeEnvironment() const { return true; }

signals:
    void exportProgress(int percent);
    void exportCompleted(bool success, const QString& message, const QString& outputPath);
    void projectSaved(const QString& projectId);
};

} // namespace catchim::bridge

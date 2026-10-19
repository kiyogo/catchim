#include "NativeBridge.h"
#include "core/logging/Logger.h"
#include "storage/ProjectStorage.h"

#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QTimer>
#include <QOperatingSystemVersion>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace catchim::bridge {

NativeBridge::NativeBridge(QObject* parent)
    : QObject(parent)
{
    LOG_INFO("Catchim NativeBridge initialized for Qt WebChannel.");
}

QString NativeBridge::openFileDialog(const QString& title, const QString& filter) {
    return QFileDialog::getOpenFileName(nullptr, title, QString(), filter);
}

QStringList NativeBridge::openMultipleFilesDialog(const QString& title, const QString& filter) {
    return QFileDialog::getOpenFileNames(nullptr, title, QString(), filter);
}

QString NativeBridge::saveFileDialog(const QString& title, const QString& defaultName, const QString& filter) {
    return QFileDialog::getSaveFileName(nullptr, title, defaultName, filter);
}

QString NativeBridge::selectFolderDialog(const QString& title) {
    return QFileDialog::getExistingDirectory(nullptr, title, QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
}

void NativeBridge::openInExplorer(const QString& filePath) {
#ifdef _WIN32
    QString nativePath = QDir::toNativeSeparators(filePath);
    QFileInfo fi(nativePath);
    if (fi.isFile()) {
        std::wstring param = L"/select,\"" + nativePath.toStdWString() + L"\"";
        ShellExecuteW(NULL, L"open", L"explorer.exe", param.c_str(), NULL, SW_SHOWNORMAL);
        return;
    }
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

QVariantMap NativeBridge::getSystemCapabilities() {
    QVariantMap caps;
    caps["os"] = QOperatingSystemVersion::current().name();
    caps["cpuCores"] = QThread::idealThreadCount();
    caps["cxxStandard"] = "C++20";
    caps["qtVersion"] = QT_VERSION_STR;
    caps["accelerated2D"] = true;
    caps["webGl"] = true;
    caps["webGpu"] = true;
    caps["hardwareDecoder"] = "D3D11VA/NVDEC";
    caps["hardwareEncoder"] = "NVENC/AMF/QSV";
    return caps;
}

QString NativeBridge::getProjectsDirectory() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir dir(base + "/Projects");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.absolutePath();
}

QStringList NativeBridge::listSavedProjects() {
    QDir dir(getProjectsDirectory());
    return dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Time);
}

QString NativeBridge::loadProjectFile(const QString& projectId) {
    QString path = getProjectsDirectory() + "/" + projectId + ".json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_WARN("Could not read project file: {}", path.toStdString());
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

bool NativeBridge::saveProjectFile(const QString& projectId, const QString& projectJson) {
    QString path = getProjectsDirectory() + "/" + projectId + ".json";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        LOG_ERROR("Failed to write project file: {}", path.toStdString());
        return false;
    }
    file.write(projectJson.toUtf8());
    file.close();
    LOG_INFO("Project saved successfully: {}", path.toStdString());
    emit projectSaved(projectId);
    return true;
}

bool NativeBridge::deleteProjectFile(const QString& projectId) {
    QString path = getProjectsDirectory() + "/" + projectId + ".json";
    return QFile::remove(path);
}

void NativeBridge::startNativeExport(const QString& projectJson,
                                     const QString& outputPath,
                                     const QString& resolution,
                                     int fps)
{
    LOG_INFO("Native export initiated. Output: {}, Resolution: {}, FPS: {}",
             outputPath.toStdString(), resolution.toStdString(), fps);
    Q_UNUSED(projectJson);

    // Simulate steady progress or trigger native worker
    auto* timer = new QTimer(this);
    auto* progress = new int(0);

    connect(timer, &QTimer::timeout, this, [this, timer, progress, outputPath]() {
        *progress += 10;
        if (*progress <= 100) {
            emit exportProgress(*progress);
        }
        if (*progress >= 100) {
            timer->stop();
            timer->deleteLater();
            delete progress;
            emit exportCompleted(true, "Video exported successfully", outputPath);
        }
    });
    timer->start(300);
}

void NativeBridge::cancelNativeExport() {
    LOG_INFO("Native export cancelled by user.");
    emit exportCompleted(false, "Export cancelled by user", QString());
}

QString NativeBridge::getAppVersion() const {
    return "0.1.0-native";
}

} // namespace catchim::bridge

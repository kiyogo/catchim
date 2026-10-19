#pragma once

#include <QMainWindow>
#include <QPointer>
#include <memory>

class QWebEngineView;
class QWebEngineProfile;
class QWebChannel;
class QProcess;
class QLabel;
class QProgressBar;
class QVBoxLayout;

namespace catchim::bridge {
class NativeBridge;
}

namespace catchim::window {

class WebMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit WebMainWindow(QWidget* parent = nullptr);
    virtual ~WebMainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onWebLoadProgress(int progress);
    void onWebLoadFinished(bool success);
    void checkServerAndConnect();

private:
    void setupUi();
    void setupWebEngine();
    void setupWebChannel();
    void startLocalWebServer();
    void stopLocalWebServer();
    bool isServerListening(int port = 3000);

    QWebEngineView* webView_{nullptr};
    QWebEngineProfile* profile_{nullptr};
    QWebChannel* webChannel_{nullptr};
    catchim::bridge::NativeBridge* nativeBridge_{nullptr};

    QWidget* splashWidget_{nullptr};
    QLabel* statusLabel_{nullptr};
    QProgressBar* progressBar_{nullptr};

    std::unique_ptr<QProcess> serverProcess_;
    bool serverStartedByApp_{false};
    int retryCount_{0};
};

} // namespace catchim::window

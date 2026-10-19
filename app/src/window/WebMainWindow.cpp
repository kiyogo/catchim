#include "WebMainWindow.h"
#include "bridge/NativeBridge.h"
#include "core/logging/Logger.h"

#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebChannel>
#include <QTcpSocket>
#include <QProcess>
#include <QTimer>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>

#ifdef _WIN32
#include <windows.h>
#endif

namespace catchim::window {

WebMainWindow::WebMainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Catchim Video Editor");
    resize(1440, 900);
    setStyleSheet("QMainWindow { background-color: #09090b; }");

    setupUi();
    setupWebEngine();
    setupWebChannel();

    // Check if the web backend is already serving
    checkServerAndConnect();
}

WebMainWindow::~WebMainWindow() {
    stopLocalWebServer();
}

void WebMainWindow::setupUi() {
    auto* central = new QStackedWidget(this);
    setCentralWidget(central);

    // 1. Splash / Loading Screen
    splashWidget_ = new QWidget(this);
    splashWidget_->setStyleSheet("background-color: #09090b; color: #f4f4f5;");
    auto* splashLayout = new QVBoxLayout(splashWidget_);
    splashLayout->setAlignment(Qt::AlignCenter);
    splashLayout->setSpacing(16);

    auto* titleLabel = new QLabel("CATCHIM", splashWidget_);
    titleLabel->setStyleSheet("font-size: 32px; font-weight: bold; letter-spacing: 4px; color: #fafafa;");
    titleLabel->setAlignment(Qt::AlignCenter);

    auto* subLabel = new QLabel("Professional Desktop Video Editor", splashWidget_);
    subLabel->setStyleSheet("font-size: 14px; color: #a1a1aa;");
    subLabel->setAlignment(Qt::AlignCenter);

    progressBar_ = new QProgressBar(splashWidget_);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(15);
    progressBar_->setFixedWidth(320);
    progressBar_->setFixedHeight(6);
    progressBar_->setTextVisible(false);
    progressBar_->setStyleSheet(
        "QProgressBar { background-color: #27272a; border-radius: 3px; border: none; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2563eb, stop:1 #38bdf8); border-radius: 3px; }"
    );

    statusLabel_ = new QLabel("Initializing native engine & web runtime...", splashWidget_);
    statusLabel_->setStyleSheet("font-size: 12px; color: #71717a;");
    statusLabel_->setAlignment(Qt::AlignCenter);

    splashLayout->addStretch();
    splashLayout->addWidget(titleLabel);
    splashLayout->addWidget(subLabel);
    splashLayout->addSpacing(24);
    splashLayout->addWidget(progressBar_);
    splashLayout->addWidget(statusLabel_);
    splashLayout->addStretch();

    central->addWidget(splashWidget_);

    // 2. Web Engine View
    webView_ = new QWebEngineView(this);
    webView_->setStyleSheet("background-color: #09090b;");
    central->addWidget(webView_);

    central->setCurrentWidget(splashWidget_);
}

void WebMainWindow::setupWebEngine() {
    profile_ = QWebEngineProfile::defaultProfile();
    
    // Configure cache and persistent storage in LocalAppData
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/WebEngineCache";
    profile_->setCachePath(cachePath);
    profile_->setPersistentStoragePath(cachePath);

    auto* settings = profile_->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    settings->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
    settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);
    settings->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);

    connect(webView_, &QWebEngineView::loadProgress, this, &WebMainWindow::onWebLoadProgress);
    connect(webView_, &QWebEngineView::loadFinished, this, &WebMainWindow::onWebLoadFinished);
}

void WebMainWindow::setupWebChannel() {
    webChannel_ = new QWebChannel(this);
    nativeBridge_ = new catchim::bridge::NativeBridge(this);

    webChannel_->registerObject("nativeBridge", nativeBridge_);
    webView_->page()->setWebChannel(webChannel_);

    // Automatically inject script to attach nativeBridge when page loads
    QString injectJs = R"(
        (function() {
            if (window.__catchim_bridge_injected) return;
            window.__catchim_bridge_injected = true;

            function initBridge() {
                if (typeof QWebChannel !== 'undefined' && window.qt && window.qt.webChannelTransport) {
                    new QWebChannel(window.qt.webChannelTransport, function(channel) {
                        window.nativeBridge = channel.objects.nativeBridge;
                        console.log('[Catchim Desktop] NativeBridge connected successfully.');
                        window.dispatchEvent(new CustomEvent('nativeBridgeReady', { detail: window.nativeBridge }));
                    });
                } else {
                    setTimeout(initBridge, 100);
                }
            }

            var script = document.createElement('script');
            script.src = 'qrc:///qtwebchannel/qwebchannel.js';
            script.onload = initBridge;
            script.onerror = function() {
                // Fallback direct retry
                initBridge();
            };
            document.head.appendChild(script);
        })();
    )";

    QWebEngineScript script;
    script.setName("CatchimBridgeInjection");
    script.setSourceCode(injectJs);
    script.setInjectionPoint(QWebEngineScript::DocumentReady);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);

    profile_->scripts()->insert(script);
}

bool WebMainWindow::isServerListening(int port) {
    QTcpSocket socket;
    socket.connectToHost("127.0.0.1", static_cast<quint16>(port));
    return socket.waitForConnected(350);
}

void WebMainWindow::checkServerAndConnect() {
    if (isServerListening(3300)) {
        LOG_INFO("Web server detected on 127.0.0.1:3300. Loading editor...");
        statusLabel_->setText("Connecting to editor...");
        progressBar_->setValue(70);
        webView_->load(QUrl("http://127.0.0.1:3300/projects"));
    } else {
        if (!serverProcess_) {
            startLocalWebServer();
        }

        retryCount_++;
        if (retryCount_ > 60) {
            statusLabel_->setText("Error: Server startup timed out. Please check web source.");
            LOG_ERROR("Web server startup timed out after 30 seconds.");
            return;
        }

        int progress = 20 + (retryCount_ * 50 / 60);
        progressBar_->setValue(progress);
        statusLabel_->setText(QString("Starting local background server... (%1s)").arg(retryCount_ / 2));
        QTimer::singleShot(500, this, &WebMainWindow::checkServerAndConnect);
    }
}

void WebMainWindow::startLocalWebServer() {
    LOG_INFO("Starting background web runtime silently...");
    serverProcess_ = std::make_unique<QProcess>(this);
    serverStartedByApp_ = true;

    // Locate the web directory relative to application executable or project root
    QString appDir = QCoreApplication::applicationDirPath();
    QString webDir;

    // Check potential relative locations
    QStringList candidates = {
        appDir + "/../../web",
        appDir + "/../web",
        appDir + "/web",
        "E:/DATA/source/catchim/web"
    };

    for (const auto& path : candidates) {
        if (QDir(path).exists() && QFile::exists(path + "/package.json")) {
            webDir = QDir(path).canonicalPath();
            break;
        }
    }

    if (webDir.isEmpty()) {
        LOG_ERROR("Could not locate web source directory!");
        statusLabel_->setText("Error: Web directory not found.");
        return;
    }

    LOG_INFO("Web directory resolved to: {}", webDir.toStdString());
    serverProcess_->setWorkingDirectory(webDir);

#ifdef _WIN32
    serverProcess_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif

    QString program = "cmd.exe";
    QStringList arguments = {"/c", "npm", "run", "dev"};
    serverProcess_->start(program, arguments);
}

void WebMainWindow::stopLocalWebServer() {
    if (serverStartedByApp_ && serverProcess_ && serverProcess_->state() != QProcess::NotRunning) {
        LOG_INFO("Terminating child web server process tree...");
#ifdef _WIN32
        qint64 pid = serverProcess_->processId();
        QString killCmd = QString("taskkill /F /T /PID %1").arg(pid);
        QProcess::execute(killCmd);
#else
        serverProcess_->kill();
#endif
        serverProcess_->waitForFinished(2000);
    }
}

void WebMainWindow::onWebLoadProgress(int progress) {
    if (progressBar_ && splashWidget_ && splashWidget_->isVisible()) {
        progressBar_->setValue(70 + (progress * 30 / 100));
    }
}

void WebMainWindow::onWebLoadFinished(bool success) {
    if (success) {
        LOG_INFO("Web editor interface loaded successfully!");
        auto* central = qobject_cast<QStackedWidget*>(centralWidget());
        if (central && webView_) {
            central->setCurrentWidget(webView_);
        }
    } else {
        LOG_WARN("Failed to load web editor page. Will retry in 2 seconds...");
        QTimer::singleShot(2000, this, [this]() {
            if (isServerListening(3300)) {
                webView_->load(QUrl("http://127.0.0.1:3300/projects"));
            }
        });
    }
}

void WebMainWindow::closeEvent(QCloseEvent* event) {
    stopLocalWebServer();
    event->accept();
}

void WebMainWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_F5 || (event->key() == Qt::Key_R && event->modifiers() & Qt::ControlModifier)) {
        if (webView_) {
            webView_->reload();
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_F11) {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

} // namespace catchim::window

#include "ExportDialog.h"
#include "ui/widgets/EditorLanguage.h"

#if defined(HAVE_QT6)
#include "ui/icons/UiIcons.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/WebComboBox.h"
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsBlurEffect>
#include <QShowEvent>
#include <QHideEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QApplication>
#include <QTimer>
#include <QFile>
#if !defined(__EMSCRIPTEN__)
#include <QProcess>
#endif
#include <cmath>

namespace catchim::ui {

ExportDialog::ExportDialog(
    editor::EditorEngine& engine,
    media::MediaLibrary& mediaLibrary,
    render::RenderEngine& renderEngine,
    QWidget* parent
)
    : QDialog(parent)
    , engine_(engine)
    , mediaLibrary_(mediaLibrary)
    , renderEngine_(renderEngine)
{
    const auto& pal = Theme::instance().palette();
    setWindowTitle("Xuất dự án");
    setObjectName("exportDialog");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setFixedWidth(440);
    setStyleSheet(QString(R"(
        QDialog#exportDialog { background: transparent; color: %2; }
        QWidget#exportCard { background-color: %1; border: 1px solid %3; border-radius: 12px; }
        QLabel {
            color: %2;
            font-size: 13px;
        }
    )").arg(pal.background.name())
       .arg(pal.textPrimary.name()).arg(pal.border.name()));

    setupUi();
    updateInfoLabel();
}

ExportDialog::~ExportDialog() {
    delete modalBackdrop_;
    if (worker_) {
        worker_->cancel();
        worker_->wait(2000);
    }
}

void ExportDialog::showEvent(QShowEvent* event) {
    applyEditorLanguage(this);
    if (parentWidget() && !modalBackdrop_) {
        auto* host=parentWidget()->window();
        const auto screenshot=host->grab();
        modalBackdrop_=new QWidget(host); modalBackdrop_->setObjectName("exportBackdrop");
        modalBackdrop_->setGeometry(host->rect());
        auto* image=new QLabel(modalBackdrop_); image->setPixmap(screenshot); image->setGeometry(modalBackdrop_->rect());
        auto* blur=new QGraphicsBlurEffect(image); blur->setBlurRadius(6); image->setGraphicsEffect(blur);
        auto* tint=new QWidget(modalBackdrop_); tint->setGeometry(modalBackdrop_->rect());
        tint->setStyleSheet("background:rgba(0,0,0,166);"); tint->setAttribute(Qt::WA_StyledBackground);
    }
    if (modalBackdrop_) {modalBackdrop_->show();modalBackdrop_->raise();}
    if (parentWidget()) {
        auto* host=parentWidget()->window();
        move(host->mapToGlobal(QPoint((host->width()-width())/2,(host->height()-height())/2)));
    }
    QDialog::showEvent(event);
}

void ExportDialog::hideEvent(QHideEvent* event) {
    if (modalBackdrop_) modalBackdrop_->hide();
    QDialog::hideEvent(event);
}

void ExportDialog::setupUi() {
    const auto& pal = Theme::instance().palette();
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12,12,12,12);
    auto* card = new QWidget(this); card->setObjectName("exportCard");
    card->setAttribute(Qt::WA_StyledBackground);
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(20); shadow->setOffset(0,4); shadow->setColor(QColor(0,0,0,40));
    card->setGraphicsEffect(shadow); outer->addWidget(card);
    auto* mainLayout = new QVBoxLayout(card);
    mainLayout->setContentsMargins(1,1,1,12); mainLayout->setSpacing(0);

    auto* header = new QWidget(card); header->setFixedHeight(44);
    header->setStyleSheet(QString("QWidget {border:none;border-bottom:1px solid %1;} QLabel,QPushButton {border:none;}").arg(pal.border.name()));
    auto* headerLayout = new QHBoxLayout(header); headerLayout->setContentsMargins(12,0,12,0);
    auto* title = new QLabel("Xuất dự án",header); title->setStyleSheet("font-weight:500;");
    headerLayout->addWidget(title); headerLayout->addStretch();
    auto* advancedToggle = new QPushButton(header); advancedToggle->setCheckable(true); advancedToggle->setFixedSize(24,24);
    advancedToggle->setIcon(UiIcons::get(UiIcon::Settings,pal.textSecondary,14)); advancedToggle->setToolTip("Tùy chọn bộ mã hóa");
    advancedToggle->setStyleSheet("QPushButton {background:transparent;border:none;padding:0;}"); headerLayout->addWidget(advancedToggle);
    auto* close = new QPushButton("×",header); close->setFixedSize(24,24); close->setToolTip("Đóng");
    close->setStyleSheet("QPushButton {background:transparent;border:none;font-size:20px;padding:0;}");
    connect(close,&QPushButton::clicked,this,&ExportDialog::onCancelExport); headerLayout->addWidget(close);
    mainLayout->addWidget(header);

    settingsWidget_ = new QWidget(card);
    auto* settingsLayout = new QVBoxLayout(settingsWidget_);
    settingsLayout->setContentsMargins(0,0,0,0); settingsLayout->setSpacing(0);
    auto addRow = [this,settingsLayout,&pal](const QString& label, QComboBox* combo) {
        auto* row = new QWidget(settingsWidget_); row->setFixedHeight(53);
        row->setStyleSheet(QString("QWidget {border:none;border-bottom:1px solid %1;} QLabel,QComboBox {border-bottom:none;}").arg(pal.border.name()));
        auto* l = new QHBoxLayout(row); l->setContentsMargins(12,10,12,10); l->setSpacing(12);
        auto* caption = new QLabel(label,row); caption->setStyleSheet(QString("color:%1;font-weight:500;").arg(pal.textSecondary.name()));
        l->addWidget(caption); l->addStretch();
        auto fit=[combo]{combo->setFixedWidth(std::clamp(combo->fontMetrics().horizontalAdvance(combo->currentText())+38,176,292));};
        fit();connect(combo,&QComboBox::currentTextChanged,combo,[fit]{fit();});combo->setFixedHeight(32);
        combo->setStyleSheet("QComboBox{background:transparent;border:none;min-height:0;padding:0;}");
        l->addWidget(combo); settingsLayout->addWidget(row);
    };
    resolutionCombo_ = new WebComboBox(settingsWidget_); resolutionCombo_->setObjectName("exportResolution");
    for (const auto& pair : {qMakePair("Nguồn gốc","source"),qMakePair("480p","480p"),qMakePair("720p","720p"),qMakePair("1080p","1080p"),qMakePair("1440p","1440p"),qMakePair("4K (2160p)","2160p"),qMakePair("8K (4320p)","4320p")})
        resolutionCombo_->addItem(QString::fromUtf8(pair.first),pair.second);
    resolutionCombo_->setCurrentIndex(0);
    connect(resolutionCombo_,&QComboBox::currentIndexChanged,this,&ExportDialog::onResolutionChanged);
    addRow("Độ phân giải",resolutionCombo_);
    formatCombo_ = new WebComboBox(settingsWidget_); formatCombo_->setObjectName("exportFormat");
    formatCombo_->addItem("MP4 (H.264) - Tương thích tốt hơn","mp4");
    formatCombo_->addItem("WebM (VP9) - Dung lượng nhỏ hơn","webm");
    connect(formatCombo_,&QComboBox::currentIndexChanged,this,&ExportDialog::onFormatChanged);
    addRow("Định dạng",formatCombo_);
    qualityCombo_ = new WebComboBox(settingsWidget_); qualityCombo_->setObjectName("exportQuality");
    qualityCombo_->addItem("Cao - Khuyên dùng","high"); qualityCombo_->addItem("Trung bình - Cân bằng","medium");
    qualityCombo_->addItem("Rất cao - Dung lượng lớn nhất","very_high"); qualityCombo_->addItem("Thấp - Dung lượng nhỏ nhất","low");
    addRow("Chất lượng",qualityCombo_);
    audioCombo_ = new WebComboBox(settingsWidget_); audioCombo_->setObjectName("exportAudio");
    audioCombo_->addItem("Bao gồm âm thanh","include"); audioCombo_->addItem("Không có âm thanh","mute");
    addRow("Âm thanh",audioCombo_);
    mainLayout->addWidget(settingsWidget_);

    infoLabel_ = new QLabel(card); infoLabel_->setObjectName("exportOutputSize");
    infoLabel_->setContentsMargins(12,8,12,8); infoLabel_->setStyleSheet(QString("font-size:12px;color:%1;").arg(pal.textSecondary.name()));
    mainLayout->addWidget(infoLabel_);

    // Keep the native encoder controls available without changing the web's four-row form.
    auto* advanced = new QWidget(card);
    auto* advLayout = new QVBoxLayout(advanced); advLayout->setContentsMargins(12,4,12,8); advLayout->setSpacing(8);
    hwAccelCombo_ = new WebComboBox(advanced); hwAccelCombo_->setToolTip("Bộ mã hóa phần cứng");
    gpuStatusLabel_ = new QLabel(advanced); gpuStatusLabel_->setWordWrap(true);
    advLayout->addWidget(hwAccelCombo_); advLayout->addWidget(gpuStatusLabel_);
    mainLayout->addWidget(advanced); advanced->hide();
    connect(advancedToggle,&QPushButton::toggled,this,[this,advanced](bool open){advanced->setVisible(open);adjustSize();});
    probeGpuEncoders();

    progressWidget_ = new QWidget(card);
    auto* progressLayout = new QVBoxLayout(progressWidget_); progressLayout->setContentsMargins(12,8,12,8); progressLayout->setSpacing(8);
    progressStatusLabel_ = new QLabel("Đang chuẩn bị xuất…",progressWidget_); progressStatusLabel_->setWordWrap(true);
    progressBar_ = new QProgressBar(progressWidget_); progressBar_->setRange(0,100); progressBar_->setValue(0); progressBar_->setFixedHeight(16);
    progressBar_->setTextVisible(false);progressBar_->setFixedHeight(6);
    progressBar_->setStyleSheet(QString("QProgressBar{background:%1;border:none;border-radius:3px;} QProgressBar::chunk{background:%2;border-radius:3px;}").arg(pal.secondary.name(),pal.primaryAccent.name()));
    telemetryFpsLabel_ = new QLabel(progressWidget_); telemetryFpsLabel_->setWordWrap(true);
    toggleLogBtn_ = new QPushButton("Xem nhật ký kỹ thuật",progressWidget_);
    connect(toggleLogBtn_,&QPushButton::clicked,this,&ExportDialog::onToggleTechLog);
    techLogBox_ = new QPlainTextEdit(progressWidget_); techLogBox_->setReadOnly(true); techLogBox_->setFixedHeight(90); techLogBox_->hide();
    progressLayout->addWidget(progressStatusLabel_); progressLayout->addWidget(progressBar_);
    progressLayout->addWidget(telemetryFpsLabel_); progressLayout->addWidget(toggleLogBtn_); progressLayout->addWidget(techLogBox_);
    mainLayout->addWidget(progressWidget_); progressWidget_->hide();
    statusBanner_ = new QLabel(card); statusBanner_->setWordWrap(true); statusBanner_->setContentsMargins(12,8,12,8);
    mainLayout->addWidget(statusBanner_); statusBanner_->hide();

    auto* buttons = new QWidget(card); auto* buttonLayout = new QHBoxLayout(buttons);
    buttonLayout->setContentsMargins(12,16,12,0); buttonLayout->setSpacing(8);
    cancelBtn_ = new QPushButton("Hủy",buttons); cancelBtn_->setFixedHeight(32); cancelBtn_->hide();
    connect(cancelBtn_,&QPushButton::clicked,this,&ExportDialog::onCancelExport);
    exportBtn_ = new QPushButton("Xuất",buttons); exportBtn_->setObjectName("startExportButton"); exportBtn_->setFixedHeight(36);
    exportBtn_->setIcon(UiIcons::get(UiIcon::Export,pal.background,16));
    exportBtn_->setStyleSheet(QString("QPushButton {background:%1;color:%2;border:none;border-radius:10px;padding:0 16px;} QPushButton:hover {background:%3;}").arg(pal.textPrimary.name(),pal.background.name(),pal.textSecondary.name()));
    connect(exportBtn_,&QPushButton::clicked,this,&ExportDialog::onStartExport);
    buttonLayout->addWidget(cancelBtn_); buttonLayout->addWidget(exportBtn_,1); mainLayout->addWidget(buttons);
    adjustSize();
}

QSize ExportDialog::getTargetResolution() const {
    QString resCode = resolutionCombo_->currentData().toString();
    int srcW = engine_.project().settings().canvasSize.width;
    int srcH = engine_.project().settings().canvasSize.height;
    if (srcW <= 0 || srcH <= 0) {
        srcW = 1920;
        srcH = 1080;
    }

    if (resCode == "source") return QSize(std::max(2,qRound(srcW/2.)*2),std::max(2,qRound(srcH/2.)*2));
    bool ok=false; const int targetHeight=resCode.chopped(1).toInt(&ok);
    if (!ok || targetHeight<=0) return QSize(srcW,srcH);
    const int targetWidth=std::max(2,qRound((double(targetHeight)*srcW/srcH)/2.0)*2);
    return QSize(targetWidth,targetHeight);
}

void ExportDialog::updateInfoLabel() {
    if (!infoLabel_) return;
    QSize sz = getTargetResolution();
    const auto& fpsSetting = engine_.project().settings().fps;
    double fps = (fpsSetting.denominator > 0) ?
        static_cast<double>(fpsSetting.numerator) / fpsSetting.denominator : 30.0;
    if (fps <= 0.0) fps = 30.0;

    infoLabel_->setText(QString("Kích thước xuất: %1 x %2").arg(sz.width()).arg(sz.height()));
    infoLabel_->setToolTip(QString("Tốc độ khung hình: %1 fps").arg(fps,0,'f',0));
}

void ExportDialog::onResolutionChanged() {
    updateInfoLabel();
}

void ExportDialog::onFormatChanged() {
    if(hwAccelCombo_)hwAccelCombo_->setEnabled(formatCombo_->currentData().toString()!="webm"&&!isExporting_);
    updateInfoLabel();
}

void ExportDialog::probeGpuEncoders() {
#if defined(__EMSCRIPTEN__)
    hwAccelCombo_->clear();
    hwAccelCombo_->addItem("Trình duyệt (WebCodecs)", "webcodecs");
    hwAccelCombo_->setEnabled(false);
    gpuStatusLabel_->setText("Đang dùng bộ mã hóa do trình duyệt cung cấp.");
    gpuStatusLabel_->setStyleSheet("color: #38bdf8; font-size: 11px; font-weight: 600;");
#else
    // Locate FFmpeg binary
    QString ffmpegPath = "ffmpeg";
    static const QString knownWingetFfmpeg = "C:/Users/sonng/AppData/Local/Microsoft/WinGet/Packages/Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe/ffmpeg-9.0.1-full_build/bin/ffmpeg.exe";
    if (QFile::exists(knownWingetFfmpeg)) {
        ffmpegPath = knownWingetFfmpeg;
    }

    // Query available encoders from FFmpeg
    QProcess probe;
    probe.start(ffmpegPath, {"-encoders", "-v", "quiet"});
    probe.waitForFinished(5000);
    QString output = QString::fromUtf8(probe.readAllStandardOutput());

    bool hasAmf   = output.contains("h264_amf");
    bool hasNvenc = output.contains("h264_nvenc");
    bool hasQsv   = output.contains("h264_qsv");

    hwAccelCombo_->clear();
    hwAccelCombo_->addItem("CPU (libx264 — Phần mềm)", "cpu");

    QString detected;
    if (hasAmf) {
        hwAccelCombo_->addItem("AMD (h264_amf) — Có trong FFmpeg", "amf");
        detected = "AMD GPU (h264_amf)";
        hwAccelCombo_->setCurrentIndex(1); // auto-select AMD
    }
    if (hasNvenc) {
        hwAccelCombo_->addItem("NVIDIA (h264_nvenc) — Có trong FFmpeg", "nvenc");
        if (detected.isEmpty()) {
            detected = "NVIDIA GPU (h264_nvenc)";
            hwAccelCombo_->setCurrentIndex(hwAccelCombo_->count() - 1);
        }
    }
    if (hasQsv) {
        hwAccelCombo_->addItem("Intel (h264_qsv) — Có trong FFmpeg", "qsv");
        if (detected.isEmpty()) {
            detected = "Intel GPU (h264_qsv)";
            hwAccelCombo_->setCurrentIndex(hwAccelCombo_->count() - 1);
        }
    }

    if (!detected.isEmpty()) {
        gpuStatusLabel_->setText("FFmpeg hỗ trợ bộ mã hóa: " + detected);
        gpuStatusLabel_->setStyleSheet("color: #22c55e; font-size: 11px; font-weight: 600;");
    } else {
        gpuStatusLabel_->setText("Không có bộ mã hóa phần cứng trong FFmpeg. Sẽ dùng CPU.");
        gpuStatusLabel_->setStyleSheet("color: #f59e0b; font-size: 11px; font-style: italic;");
    }
#endif
}

void ExportDialog::onToggleTechLog() {
    if (!techLogBox_ || !toggleLogBtn_) return;
    bool isVis = techLogBox_->isVisible();
    techLogBox_->setVisible(!isVis);
    toggleLogBtn_->setText(!isVis ? "Ẩn nhật ký kỹ thuật" : "Xem nhật ký kỹ thuật");
    adjustSize();
}

void ExportDialog::reject() {
    if(worker_&&worker_->isRunning()){onCancelExport();return;}
    QDialog::reject();
}

void ExportDialog::onCancelExport() {
    if (isExporting_) {
        if (worker_) {
            worker_->cancel();
        }
        progressStatusLabel_->setText("Đang dừng xuất…");
        telemetryFpsLabel_->setText("Đang chờ bộ mã hóa dừng.");
        exportBtn_->setEnabled(false);
        cancelBtn_->setEnabled(false);
    } else {
        reject();
    }
}

void ExportDialog::onWorkerProgress(int currentFrame, int totalFrames, double currentFps, int percent) {
    if (!isExporting_) return;
    progressBar_->setValue(percent);

    QString hwAccel = formatCombo_->currentData().toString()=="webm"?"cpu":(hwAccelCombo_ ? hwAccelCombo_->currentData().toString() : "cpu");
    QString hwDesc = (hwAccel == "amf") ? "AMD AMF GPU" :
                     ((hwAccel == "nvenc") ? "NVIDIA NVENC GPU" :
                     ((hwAccel == "qsv") ? "Intel QSV GPU" : "CPU"));

    progressStatusLabel_->setText(QString("Đang xuất bằng %1: %2% (%3 / %4 khung hình)")
        .arg(hwDesc).arg(percent).arg(currentFrame).arg(totalFrames));

    const auto& fpsSetting = engine_.project().settings().fps;
    double fps = (fpsSetting.denominator > 0) ?
        static_cast<double>(fpsSetting.numerator) / fpsSetting.denominator : 30.0;
    if (fps <= 0.0) fps = 30.0;

    auto now = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration<double>(now - exportStartTime_).count();
    double etaSec = (currentFps > 0.0 && currentFrame < totalFrames) ? (totalFrames - currentFrame) / currentFps : 0.0;
    double speedMultiplier = currentFps / fps;

    telemetryFpsLabel_->setText(QString("Tốc độ: %1 fps (%2x) • Đã chạy: %3 giây • Còn: ~%4 giây")
        .arg(currentFps, 0, 'f', 1)
        .arg(speedMultiplier, 0, 'f', 1)
        .arg(static_cast<int>(elapsedSec))
        .arg(static_cast<int>(std::round(etaSec))));
}

void ExportDialog::onWorkerLog(const QString& line) {
    if (techLogBox_) {
        techLogBox_->appendPlainText(line);
    }
    if (line.contains("h264_amf") || line.contains("AMF")) {
        if (gpuStatusLabel_) {
            gpuStatusLabel_->setText("Đang sử dụng bộ mã hóa AMD AMF.");
            gpuStatusLabel_->setStyleSheet("color: #22c55e; font-size: 11px; font-weight: 700;");
        }
    }
}

void ExportDialog::onWorkerFinished(bool success, const QString& message) {
    isExporting_ = false;
    settingsWidget_->setEnabled(true);settingsWidget_->show();
    hwAccelCombo_->setEnabled(formatCombo_->currentData().toString()!="webm");
    exportBtn_->setEnabled(true);
    exportBtn_->show();
    cancelBtn_->setEnabled(true);

    if (success) {
        progressBar_->setValue(100);
        QString hwAccel = formatCombo_->currentData().toString()=="webm"?"cpu":(hwAccelCombo_ ? hwAccelCombo_->currentData().toString() : "cpu");
        QString hwDesc = (hwAccel == "amf") ? "AMD AMF GPU" :
                         ((hwAccel == "nvenc") ? "NVIDIA NVENC GPU" :
                         ((hwAccel == "qsv") ? "Intel QSV GPU" : "CPU"));
        progressStatusLabel_->setText(QString("Đã xuất thành công bằng %1!").arg(hwDesc));
        showStatusMessage(QString("Đã xuất thành công:\n%1").arg(message), "success");
        cancelBtn_->setText("Xong");
    } else {
        progressStatusLabel_->setText("Xuất thất bại hoặc đã hủy.");
        showStatusMessage(message, "error");
        cancelBtn_->setText("Đóng");
    }
}

void ExportDialog::onStartExport() {
    if(worker_&&worker_->isRunning())return;
    auto* tl = engine_.activeTimeline();
    if (!tl || tl->allTracks().empty()) {
        showStatusMessage("Chưa có clip trên dòng thời gian để xuất.", "warning");
        return;
    }

    double totalSec = tl->totalDuration().toSeconds();
    if (totalSec <= 0.01) {
        showStatusMessage("Dòng thời gian quá ngắn để xuất.", "warning");
        return;
    }

    QString fmt = formatCombo_->currentData().toString();
    QString ext = (fmt == "webm") ? "webm" : "mp4";
    QString defaultName = QString::fromStdString(engine_.project().name()) + "." + ext;

    QString savePath = QFileDialog::getSaveFileName(
        this,
        "Lưu video",
        defaultName,
        QString("Video (*.%1);;All Files (*.*)").arg(ext)
    );
    if (savePath.isEmpty()) return;

    QSize targetSize = getTargetResolution();
    int width = targetSize.width();
    int height = targetSize.height();

    const auto& fpsSetting = engine_.project().settings().fps;
    double fps = (fpsSetting.denominator > 0) ?
        static_cast<double>(fpsSetting.numerator) / fpsSetting.denominator : 30.0;
    if (fps <= 0.0) fps = 30.0;

    int totalFrames = static_cast<int>(std::ceil(totalSec * fps));
    if (totalFrames <= 0) totalFrames = 1;

    QString quality = qualityCombo_->currentData().toString();
    QString crf = "23";
    if (quality == "low") crf = "28";
    else if (quality == "medium") crf = "23";
    else if (quality == "high") crf = "18";
    else if (quality == "very_high") crf = "14";

    // Locate FFmpeg
    QString ffmpegPath = "ffmpeg";
    static const QString knownWingetFfmpeg = "C:/Users/sonng/AppData/Local/Microsoft/WinGet/Packages/Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe/ffmpeg-9.0.1-full_build/bin/ffmpeg.exe";
    if (QFile::exists(knownWingetFfmpeg)) {
        ffmpegPath = knownWingetFfmpeg;
    }

    const bool includeAudio=audioCombo_&&audioCombo_->currentData().toString()=="include";

    QStringList args;
    args << "-y"
         << "-filter_threads" << "2"
         << "-f" << "rawvideo"
         << "-vcodec" << "rawvideo"
         << "-s" << QString("%1x%2").arg(width).arg(height)
         << "-pix_fmt" << "rgba"
         << "-r" << QString::number(fps, 'f', 2)
         << "-i" << "-";

    if(!includeAudio)args<<"-an";

    QString hwAccel = hwAccelCombo_ ? hwAccelCombo_->currentData().toString() : "cpu";

    if (fmt == "webm") {
        args << "-c:v" << "libvpx-vp9"
             << "-pix_fmt" << "yuv420p"
             << "-deadline" << "good" << "-cpu-used" << "4" << "-row-mt" << "1"
             << "-crf" << crf
             << "-b:v" << "0";
    } else {
        if (hwAccel == "amf") {
            // AMD GPU (AMF/VCE) — optimized for maximum GPU saturation
            const double bpp=quality=="low"?.07:quality=="medium"?.12:quality=="very_high"?.28:.18;
            const QString amfBitrate=QString::number(std::max<qint64>(250000,qRound64(double(width)*height*fps*bpp)));
            args << "-c:v" << "h264_amf"
                 << "-pix_fmt" << "yuv420p"
                 << "-quality" << "speed"
                 << "-rc" << "vbr_latency"
                 << "-async_depth" << "16"
                 << "-usage" << "transcoding"
                 << "-b:v" << amfBitrate;
        } else if (hwAccel == "nvenc") {
            args << "-c:v" << "h264_nvenc"
                 << "-pix_fmt" << "yuv420p"
                 << "-preset" << "p4"
                 << "-tune" << "hq"
                 << "-rc" << "vbr"
                 << "-cq" << crf;
        } else if (hwAccel == "qsv") {
            args << "-c:v" << "h264_qsv"
                 << "-pix_fmt" << "nv12"
                 << "-preset" << "medium"
                 << "-global_quality" << crf;
        } else {
            args << "-c:v" << "libx264"
                 << "-pix_fmt" << "yuv420p"
                 << "-preset" << "fast"
                 << "-crf" << crf;
        }
    }
    args << savePath;

    // Switch UI to exporting state
    isExporting_ = true;
    settingsWidget_->setEnabled(false);settingsWidget_->hide();
    hwAccelCombo_->setEnabled(false);
    cancelBtn_->show();
    progressWidget_->show();
    if (techLogBox_) techLogBox_->clear();
    exportBtn_->setEnabled(false);
    exportBtn_->hide();
    cancelBtn_->setText("Hủy xuất");
    adjustSize();

    if (worker_) {
        worker_->cancel();
        worker_->wait(1000);
        worker_->deleteLater();
        worker_ = nullptr;
    }

    worker_ = new ExportWorker(
        engine_.project(),
        mediaLibrary_,
        width,
        height,
        fps,
        totalFrames,
        ffmpegPath,
        args,
        savePath,
        this,
        includeAudio
    );

    connect(worker_, &ExportWorker::progressUpdated, this, &ExportDialog::onWorkerProgress);
    connect(worker_, &ExportWorker::logOutputReceived, this, &ExportDialog::onWorkerLog);
    connect(worker_, &ExportWorker::exportFinished, this, &ExportDialog::onWorkerFinished);

    exportStartTime_ = std::chrono::steady_clock::now();
    worker_->start();
}

void ExportDialog::showStatusMessage(const QString& msg, const QString& type) {
    if (!statusBanner_) return;
    statusBanner_->setText(msg);
    bool isDark = (Theme::instance().mode() == ThemeMode::Dark);
    if (type == "error") {
        statusBanner_->setStyleSheet(isDark
            ? "background: #450a0a; border: 1px solid #b91c1c; color: #fca5a5; padding: 8px 12px; border-radius: 6px; font-size: 12px; font-weight: 500;"
            : "background: #fef2f2; border: 1px solid #fecaca; color: #b91c1c; padding: 8px 12px; border-radius: 6px; font-size: 12px; font-weight: 500;");
    } else if (type == "warning") {
        statusBanner_->setStyleSheet(isDark
            ? "background: #422006; border: 1px solid #d97706; color: #fde68a; padding: 8px 12px; border-radius: 6px; font-size: 12px; font-weight: 500;"
            : "background: #fffbeb; border: 1px solid #fde68a; color: #b45309; padding: 8px 12px; border-radius: 6px; font-size: 12px; font-weight: 500;");
    } else {
        statusBanner_->setStyleSheet(isDark
            ? "background: #052e16; border: 1px solid #16a34a; color: #86efac; padding: 8px 12px; border-radius: 6px; font-size: 12px; font-weight: 600;"
            : "background: #f0fdf4; border: 1px solid #bbf7d0; color: #15803d; padding: 8px 12px; border-radius: 6px; font-size: 12px; font-weight: 600;");
    }
    statusBanner_->setVisible(true);
}

} // namespace catchim::ui
#endif

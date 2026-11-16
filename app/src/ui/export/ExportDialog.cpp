#include "ExportDialog.h"
#include "export/ExportCanvasGeometryResolver.h"
#include "export/ExportGeometryResolver.h"
#include "editor/project/ProjectDiagnostics.h"
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
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <QLineEdit>
#include <QApplication>
#include <QTimer>
#include <QFile>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif
#if !defined(__EMSCRIPTEN__)
#include <QProcess>
#endif
#include <algorithm>
#include <cmath>

namespace catchim::ui {

#if defined(__EMSCRIPTEN__)
static QString browserExportBackendDescription() {
    const bool gpuCompositor = EM_ASM_INT({ return Module.catchimExportGpuEnabled ? 1 : 0; }) != 0;
    const int encoderEfficiency = EM_ASM_INT({ return Module.catchimExportEncoderPowerEfficient === true ? 1 : Module.catchimExportEncoderPowerEfficient === false ? 0 : -1; });
    if (!gpuCompositor) return "CPU ghép hình + WebCodecs (GPU chưa hoạt động)";
    if (encoderEfficiency == 1) return "WebGPU ghép hình + WebCodecs (trình duyệt báo encoder hiệu quả)";
    if (encoderEfficiency == 0) return "WebGPU ghép hình + WebCodecs (GPU encoder chưa được xác nhận)";
    return "WebGPU ghép hình + WebCodecs (chưa xác định encoder GPU)";
}
#endif

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
    presetCombo_ = new WebComboBox(settingsWidget_); presetCombo_->setObjectName("exportPreset");
    presetCombo_->addItem("Tùy chỉnh", "custom");
    presetCombo_->addItem("YouTube / Full HD", "youtube");
    presetCombo_->addItem("Shorts / Reels", "shorts");
    presetCombo_->addItem("Xem trước nhanh", "draft");
    presetCombo_->addItem("Tệp nhỏ", "small");
    connect(presetCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        const auto preset = presetCombo_->itemData(index).toString();
        if (preset == "custom") return;
        if (preset.startsWith("user:")) {
            const auto values = presetCombo_->itemData(index, Qt::UserRole + 1).toMap();
            resolutionCombo_->setCurrentIndex(resolutionCombo_->findData(values.value("resolution")));
            int aspectIndex = aspectRatioCombo_->findData(values.value("aspectRatio", "source"));
            if (aspectIndex < 0) aspectIndex = aspectRatioCombo_->findData("source");
            aspectRatioCombo_->setCurrentIndex(aspectIndex);
            const auto savedFrameRate = values.value("frameRate", "source");
            int frameRateIndex = frameRateCombo_->findData(savedFrameRate);
            if (frameRateIndex < 0) frameRateIndex = frameRateCombo_->findData("source");
            frameRateCombo_->setCurrentIndex(frameRateIndex);
            formatCombo_->setCurrentIndex(formatCombo_->findData(values.value("format")));
            qualityCombo_->setCurrentIndex(qualityCombo_->findData(values.value("quality")));
            const auto savedBitrate = values.value("videoBitrate", "auto");
            int bitrateIndex = videoBitrateCombo_->findData(savedBitrate);
            if (bitrateIndex < 0) bitrateIndex = videoBitrateCombo_->findData("auto");
            videoBitrateCombo_->setCurrentIndex(bitrateIndex);
            audioCombo_->setCurrentIndex(audioCombo_->findData(values.value("audio")));
            return;
        }
        const bool draft = preset == "draft" || preset == "small";
        resolutionCombo_->setCurrentIndex(resolutionCombo_->findData(draft ? "720p" : "1080p"));
        const QString aspect = preset == "shorts" ? "9:16" : preset == "small" ? "source" : "16:9";
        aspectRatioCombo_->setCurrentIndex(aspectRatioCombo_->findData(aspect));
        frameRateCombo_->setCurrentIndex(frameRateCombo_->findData(
            preset == "small" ? QVariant("source") : QVariant(30.0)));
        formatCombo_->setCurrentIndex(formatCombo_->findData(preset == "small" ? "webm" : "mp4"));
        qualityCombo_->setCurrentIndex(qualityCombo_->findData(preset == "draft" ? "medium" : "high"));
        videoBitrateCombo_->setCurrentIndex(videoBitrateCombo_->findData("auto"));
        audioCombo_->setCurrentIndex(audioCombo_->findData("include"));
    });
    addRow("Preset xuất", presetCombo_);
    resolutionCombo_ = new WebComboBox(settingsWidget_); resolutionCombo_->setObjectName("exportResolution");
    for (const auto& pair : {qMakePair("Nguồn gốc","source"),qMakePair("480p","480p"),qMakePair("720p","720p"),qMakePair("1080p","1080p"),qMakePair("1440p","1440p"),qMakePair("4K (2160p)","2160p"),qMakePair("8K (4320p)","4320p")})
        resolutionCombo_->addItem(QString::fromUtf8(pair.first),pair.second);
    resolutionCombo_->setCurrentIndex(0);
    connect(resolutionCombo_,&QComboBox::currentIndexChanged,this,&ExportDialog::onResolutionChanged);
    addRow("Độ phân giải",resolutionCombo_);
    aspectRatioCombo_ = new WebComboBox(settingsWidget_); aspectRatioCombo_->setObjectName("exportAspectRatio");
    aspectRatioCombo_->addItem("Nguồn gốc", "source");
    aspectRatioCombo_->addItem("16:9 - Ngang", "16:9");
    aspectRatioCombo_->addItem("9:16 - Dọc", "9:16");
    aspectRatioCombo_->addItem("1:1 - Vuông", "1:1");
    aspectRatioCombo_->addItem("4:5 - Dọc mạng xã hội", "4:5");
    aspectRatioCombo_->addItem("4:3 - Cổ điển", "4:3");
    aspectRatioCombo_->addItem("21:9 - Điện ảnh", "21:9");
    connect(aspectRatioCombo_, &QComboBox::currentIndexChanged, this, [this] { updateInfoLabel(); });
    addRow("Tỷ lệ khung hình",aspectRatioCombo_);
    frameRateCombo_ = new WebComboBox(settingsWidget_); frameRateCombo_->setObjectName("exportFrameRate");
    const auto& projectFps = engine_.project().settings().fps;
    const double sourceFps = projectFps.denominator > 0
        ? static_cast<double>(projectFps.numerator) / projectFps.denominator : 30.0;
    const QString sourceFpsLabel = QString::number(sourceFps, 'f', std::abs(sourceFps - std::round(sourceFps)) < 0.001 ? 0 : 3);
    frameRateCombo_->addItem(QString("Nguồn gốc (%1 fps)").arg(sourceFpsLabel), "source");
    for (const auto& pair : {qMakePair("23.976 fps", 23.976), qMakePair("24 fps", 24.0),
             qMakePair("25 fps", 25.0), qMakePair("29.97 fps", 29.97), qMakePair("30 fps", 30.0),
             qMakePair("50 fps", 50.0), qMakePair("59.94 fps", 59.94), qMakePair("60 fps", 60.0),
             qMakePair("120 fps", 120.0)})
        frameRateCombo_->addItem(pair.first, pair.second);
    connect(frameRateCombo_, &QComboBox::currentIndexChanged, this, [this] { updateInfoLabel(); });
    addRow("Tốc độ khung hình", frameRateCombo_);
    formatCombo_ = new WebComboBox(settingsWidget_); formatCombo_->setObjectName("exportFormat");
    formatCombo_->addItem("MP4 (H.264) - Tương thích tốt hơn","mp4");
    formatCombo_->addItem("WebM (VP9) - Dung lượng nhỏ hơn","webm");
    connect(formatCombo_,&QComboBox::currentIndexChanged,this,&ExportDialog::onFormatChanged);
    addRow("Định dạng",formatCombo_);
    qualityCombo_ = new WebComboBox(settingsWidget_); qualityCombo_->setObjectName("exportQuality");
    qualityCombo_->addItem("Cao - Khuyên dùng","high"); qualityCombo_->addItem("Trung bình - Cân bằng","medium");
    qualityCombo_->addItem("Rất cao - Dung lượng lớn nhất","very_high"); qualityCombo_->addItem("Thấp - Dung lượng nhỏ nhất","low");
    connect(qualityCombo_, &QComboBox::currentIndexChanged, this, [this] { updateInfoLabel(); });
    addRow("Chất lượng",qualityCombo_);
    videoBitrateCombo_ = new WebComboBox(settingsWidget_); videoBitrateCombo_->setObjectName("exportBitrate");
    videoBitrateCombo_->setToolTip("Tự động dùng mức chất lượng đã chọn, hoặc đặt bitrate mục tiêu cho video.");
    videoBitrateCombo_->addItem("Tự động theo chất lượng", "auto");
    for (const auto& pair : {qMakePair("1 Mbps", 1'000'000LL), qMakePair("2 Mbps", 2'000'000LL),
             qMakePair("5 Mbps", 5'000'000LL), qMakePair("8 Mbps", 8'000'000LL),
             qMakePair("12 Mbps", 12'000'000LL), qMakePair("20 Mbps", 20'000'000LL),
             qMakePair("35 Mbps", 35'000'000LL), qMakePair("60 Mbps", 60'000'000LL)})
        videoBitrateCombo_->addItem(pair.first, QVariant::fromValue<qlonglong>(pair.second));
    connect(videoBitrateCombo_, &QComboBox::currentIndexChanged, this, [this] {
        const bool automatic = getTargetVideoBitrate() == 0;
        qualityCombo_->setEnabled(automatic);
        updateInfoLabel();
    });
    addRow("Bitrate video", videoBitrateCombo_);
    audioCombo_ = new WebComboBox(settingsWidget_); audioCombo_->setObjectName("exportAudio");
    audioCombo_->addItem("Bao gồm âm thanh","include"); audioCombo_->addItem("Không có âm thanh","mute");
    addRow("Âm thanh",audioCombo_);
    const auto savedPresets = QSettings().value("export/customPresets").toList();
    for (const auto& entry : savedPresets) {
        const auto values = entry.toMap();
        const auto name = values.value("name").toString();
        if (name.isEmpty()) continue;
        presetCombo_->addItem(name, "user:" + name);
        presetCombo_->setItemData(presetCombo_->count() - 1, values, Qt::UserRole + 1);
    }
    auto* savePreset = new QPushButton("Lưu cấu hình hiện tại thành preset", settingsWidget_);
    savePreset->setFixedHeight(30);
    connect(savePreset, &QPushButton::clicked, this, [this] {
        bool accepted = false;
        const auto name = QInputDialog::getText(this, "Lưu preset xuất", "Tên preset:", QLineEdit::Normal, {}, &accepted).trimmed();
        if (!accepted || name.isEmpty()) return;
        QVariantMap values{{"name", name}, {"resolution", resolutionCombo_->currentData()},
            {"aspectRatio", aspectRatioCombo_->currentData()},
            {"frameRate", frameRateCombo_->currentData()},
            {"format", formatCombo_->currentData()}, {"quality", qualityCombo_->currentData()},
            {"videoBitrate", videoBitrateCombo_->currentData()},
            {"audio", audioCombo_->currentData()}};
        auto presets = QSettings().value("export/customPresets").toList();
        presets.erase(std::remove_if(presets.begin(), presets.end(), [&name](const QVariant& preset) {
            return preset.toMap().value("name").toString().compare(name, Qt::CaseInsensitive) == 0;
        }), presets.end());
        presets.push_back(values);
        QSettings().setValue("export/customPresets", presets);
        int index = presetCombo_->findData("user:" + name);
        if (index < 0) { presetCombo_->addItem(name, "user:" + name); index = presetCombo_->count() - 1; }
        presetCombo_->setItemData(index, values, Qt::UserRole + 1);
        presetCombo_->setCurrentIndex(index);
    });
    settingsLayout->addWidget(savePreset);
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
    progressBar_ = new QProgressBar(progressWidget_); progressBar_->setObjectName("exportProgressBar"); progressBar_->setRange(0,100); progressBar_->setValue(0); progressBar_->setFixedHeight(8);
    progressBar_->setTextVisible(false);
    progressBar_->setInvertedAppearance(false);
    progressBar_->setStyleSheet(QString(
        "QProgressBar#exportProgressBar{background-color:%1;border:1px solid %2;border-radius:4px;padding:0px;text-align:left;}"
        "QProgressBar#exportProgressBar::chunk{background-color:%3;border:none;border-radius:3px;min-width:2px;}"
    ).arg(pal.secondary.name(),pal.border.name(),pal.primaryAccent.name()));
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

    bool ok=false;
    const int targetHeight = resCode == "source" ? srcH : resCode.chopped(1).toInt(&ok);
    if (resCode != "source" && (!ok || targetHeight <= 0)) return QSize(srcW,srcH);
    const auto resolved = exporting::ExportCanvasGeometryResolver::resolveExportCanvasSize(
        editor::CanvasSize{srcW,srcH}, targetHeight, getTargetAspectRatio());
    return QSize(resolved.width,resolved.height);
}

double ExportDialog::getTargetAspectRatio() const {
    if (!aspectRatioCombo_ || aspectRatioCombo_->currentData().toString() == "source") {
        const auto& canvas = engine_.project().settings().canvasSize;
        if (canvas.width > 0 && canvas.height > 0)
            return static_cast<double>(canvas.width) / canvas.height;
        return 16.0 / 9.0;
    }
    const QStringList parts = aspectRatioCombo_->currentData().toString().split(':');
    if (parts.size() != 2) return 16.0 / 9.0;
    bool widthOk = false, heightOk = false;
    const double ratioWidth = parts[0].toDouble(&widthOk);
    const double ratioHeight = parts[1].toDouble(&heightOk);
    return widthOk && heightOk && ratioWidth > 0.0 && ratioHeight > 0.0
        ? ratioWidth / ratioHeight : 16.0 / 9.0;
}

void ExportDialog::updateInfoLabel() {
    if (!infoLabel_) return;
    QSize sz = getTargetResolution();
    const double fps = getTargetFrameRate();

    const QString aspect = aspectRatioCombo_ && aspectRatioCombo_->currentData().toString() != "source"
        ? aspectRatioCombo_->currentData().toString() : QString("Gốc");
    const QString bitrate = getTargetVideoBitrateLabel();
    infoLabel_->setText(QString("Kích thước xuất: %1 x %2 • %3 • %4 fps • %5")
        .arg(sz.width()).arg(sz.height()).arg(aspect)
        .arg(fps, 0, 'f', std::abs(fps - std::round(fps)) < 0.001 ? 0 : 3).arg(bitrate));
    infoLabel_->setToolTip(QString("Tỷ lệ khung hình %1; tốc độ %2 fps; bitrate %3")
        .arg(aspect).arg(fps,0,'f',3).arg(bitrate));
}

double ExportDialog::getTargetFrameRate() const {
    if (!frameRateCombo_ || frameRateCombo_->currentData().toString() == "source") {
        const auto& source = engine_.project().settings().fps;
        const double fps = source.denominator > 0
            ? static_cast<double>(source.numerator) / source.denominator : 30.0;
        return std::isfinite(fps) && fps > 0.0 ? fps : 30.0;
    }
    bool ok = false;
    const double fps = frameRateCombo_->currentData().toDouble(&ok);
    return ok && std::isfinite(fps) && fps > 0.0 ? fps : 30.0;
}

qint64 ExportDialog::getTargetVideoBitrate() const {
    if (!videoBitrateCombo_ || videoBitrateCombo_->currentData().toString() == "auto") return 0;
    bool ok = false;
    const qint64 bitrate = videoBitrateCombo_->currentData().toLongLong(&ok);
    return ok && bitrate >= 1'000'000 ? bitrate : 0;
}

QString ExportDialog::getTargetVideoBitrateLabel() const {
    const qint64 bitrate = getTargetVideoBitrate();
    if (bitrate <= 0) return "Bitrate tự động";
    return QString("%1 Mbps").arg(static_cast<double>(bitrate) / 1'000'000.0, 0, 'f',
        bitrate % 1'000'000 == 0 ? 0 : 1);
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
    hwAccelCombo_->addItem("WebCodecs (ưu tiên phần cứng)", "webcodecs");
    hwAccelCombo_->setEnabled(false);
    const bool gpuReady = EM_ASM_INT({ return Module.catchimGpuPreviewAvailable ? 1 : 0; }) != 0;
    const int gpuStatus = EM_ASM_INT({ return Module.catchimWebGpuStatusCode || 0; });
    QString gpuMessage;
    if (gpuReady) gpuMessage = "WebGPU đang hoạt động; mã hóa video phần cứng tùy trình duyệt.";
    else switch (gpuStatus) {
        case 1: gpuMessage = "GPU chưa hoạt động: WebGPU cần HTTPS hoặc localhost."; break;
        case 2: gpuMessage = "GPU chưa hoạt động: trình duyệt không hỗ trợ WebGPU; hãy dùng Chrome/Edge mới."; break;
        case 3: gpuMessage = "GPU chưa hoạt động: không tạo được device. Cập nhật driver và bật Hardware acceleration."; break;
        case 4: gpuMessage = "GPU chưa hoạt động: trình duyệt chỉ cấp GPU phần mềm. Kiểm tra driver và Hardware acceleration."; break;
        case 5: gpuMessage = "GPU device có sẵn nhưng bộ ghép WebGPU không khởi tạo được. Cập nhật driver/trình duyệt."; break;
        case 6: gpuMessage = "GPU bị trình duyệt ngắt. Hãy đóng tab khác đang dùng GPU rồi tải lại ứng dụng."; break;
        default: gpuMessage = "Đang dùng CPU ghép hình: WebGPU không khởi tạo được trên máy này."; break;
    }
    gpuStatusLabel_->setText(gpuMessage);
    gpuStatusLabel_->setStyleSheet(gpuReady ? "color: #22c55e; font-size: 11px; font-weight: 700;" : "color: #f59e0b; font-size: 11px; font-weight: 600;");
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
#if defined(__EMSCRIPTEN__)
        if (browserDownloadReady_) {
            EM_ASM({ Module.catchimExportDownload(); });
            browserDownloadReady_=false;
        }
#endif
        reject();
    }
}

void ExportDialog::onWorkerProgress(int currentFrame, int totalFrames, double currentFps, int percent) {
    if (!isExporting_) return;
    // Use one progress step per frame (rather than only 100 percentage steps),
    // otherwise long exports look frozen for several seconds between updates.
    progressBar_->setRange(0, std::max(1, totalFrames));
    progressBar_->setValue(std::clamp(currentFrame, 0, std::max(1, totalFrames)));

    QString hwAccel = hwAccelCombo_ ? hwAccelCombo_->currentData().toString() : "cpu";
#if !defined(__EMSCRIPTEN__)
    if (formatCombo_->currentData().toString()=="webm") hwAccel = "cpu";
#endif
    QString hwDesc = (hwAccel == "amf") ? "AMD AMF GPU" :
                     ((hwAccel == "nvenc") ? "NVIDIA NVENC GPU" :
                     ((hwAccel == "qsv") ? "Intel QSV GPU" :
                     ((hwAccel == "webcodecs") ? "WebCodecs (ưu tiên encoder phần cứng)" : "CPU")));
#if defined(__EMSCRIPTEN__)
    hwDesc = browserExportBackendDescription();
    if (gpuStatusLabel_) {
        const bool gpuCompositor = EM_ASM_INT({ return Module.catchimExportGpuEnabled ? 1 : 0; }) != 0;
        const int encoderEfficiency = EM_ASM_INT({ return Module.catchimExportEncoderPowerEfficient === true ? 1 : Module.catchimExportEncoderPowerEfficient === false ? 0 : -1; });
        gpuStatusLabel_->setText(gpuCompositor
            ? (encoderEfficiency == 0 ? "Đang ghép hình trên GPU; Chrome báo encoder phần cứng không khả dụng/không hiệu quả." : "Đang ghép hình trên GPU; trạng thái encoder do trình duyệt quyết định.")
            : "Đang dùng CPU ghép hình; GPU WebGPU không khởi tạo được hoặc project cần blend mode CPU.");
        gpuStatusLabel_->setStyleSheet(gpuCompositor && encoderEfficiency == 1 ? "color: #22c55e; font-size: 11px; font-weight: 700;" : "color: #f59e0b; font-size: 11px; font-weight: 600;");
    }
#endif

    progressStatusLabel_->setText(QString("Đang xuất bằng %1: %2% (%3 / %4 khung hình)")
        .arg(hwDesc).arg(percent).arg(currentFrame).arg(totalFrames));

    const double fps = getTargetFrameRate();

    auto now = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration<double>(now - exportStartTime_).count();
    // Worker reports short-interval throughput, which varies with browser scheduling
    // and encoder backpressure. Smooth the displayed rate and base ETA on whole-run
    // throughput so both numbers do not jump on every progress update.
    if (currentFps > 0.0) {
        smoothedExportFps_ = smoothedExportFps_ <= 0.0
            ? currentFps
            : smoothedExportFps_ * 0.75 + currentFps * 0.25;
    }
    const double displayFps = smoothedExportFps_ > 0.0 ? smoothedExportFps_ : currentFps;
    const double averageFps = elapsedSec > 0.0 && currentFrame > 0 ? currentFrame / elapsedSec : displayFps;
    double etaSec = (averageFps > 0.0 && currentFrame < totalFrames) ? (totalFrames - currentFrame) / averageFps : 0.0;
    double speedMultiplier = displayFps / fps;

    telemetryFpsLabel_->setText(QString("Tốc độ: %1 fps (%2x) • Đã chạy: %3 giây • Còn: ~%4 giây")
        .arg(displayFps, 0, 'f', 1)
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
        progressBar_->setValue(progressBar_->maximum());
        QString hwAccel = hwAccelCombo_ ? hwAccelCombo_->currentData().toString() : "cpu";
#if !defined(__EMSCRIPTEN__)
        if (formatCombo_->currentData().toString()=="webm") hwAccel = "cpu";
#endif
        QString hwDesc = (hwAccel == "amf") ? "AMD AMF GPU" :
                         ((hwAccel == "nvenc") ? "NVIDIA NVENC GPU" :
                         ((hwAccel == "qsv") ? "Intel QSV GPU" :
                         ((hwAccel == "webcodecs") ? "WebCodecs (ưu tiên encoder phần cứng)" : "CPU")));
#if defined(__EMSCRIPTEN__)
        hwDesc = browserExportBackendDescription();
#endif
        progressStatusLabel_->setText(QString("Đã xuất thành công bằng %1!").arg(hwDesc));
#if defined(__EMSCRIPTEN__)
        browserDownloadReady_=true;
        showStatusMessage("Video đã sẵn sàng. Nhấn 'Tải video xuống' để lưu file.", "success");
        cancelBtn_->setText("Tải video xuống");
#else
        showStatusMessage(QString("Đã xuất thành công:\n%1").arg(message), "success");
        cancelBtn_->setText("Xong");
#endif
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

    QStringList preflight;
    const auto report = editor::ProjectDiagnostics::analyze(engine_.project());
    if (report.gapCount() > 0)
        preflight << QString("• Có %1 khoảng trống giữa các clip trên cùng rãnh.").arg(report.gapCount());
    if (report.overlapCount > 0)
        preflight << QString("• Có %1 đoạn clip chồng lấn trên cùng rãnh.").arg(report.overlapCount);
    std::size_t missingAssets = 0;
    std::size_t mutedTracks = 0;
    for (const auto* track : tl->allTracks()) {
        if (track->isMuted() && (track->type() == editor::TrackType::Audio || track->type() == editor::TrackType::Video))
            ++mutedTracks;
        for (const auto& clip : track->clips()) {
            if (clip.mediaId().isEmpty()) continue;
            const auto asset = mediaLibrary_.findAsset(clip.mediaId());
            if (!asset || !std::filesystem::exists(asset->filePath())) ++missingAssets;
        }
    }
    if (missingAssets > 0)
        preflight << QString("• Không tìm thấy tệp nguồn của %1 clip.").arg(missingAssets);
    if (mutedTracks > 0)
        preflight << QString("• Có %1 rãnh âm thanh/video đang tắt tiếng.").arg(mutedTracks);
    if (!preflight.isEmpty()) {
        const auto answer = QMessageBox::warning(this, "Kiểm tra trước khi xuất",
            "Phát hiện một số điều cần xem lại:\n\n" + preflight.join("\n") +
                "\n\nBạn có muốn tiếp tục xuất không?",
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
        if (answer != QMessageBox::Yes) return;
    }

    QString fmt = formatCombo_->currentData().toString();
    QString ext = (fmt == "webm") ? "webm" : "mp4";
    QString defaultName = QString::fromStdString(engine_.project().name()) + "." + ext;

    QString savePath;
#if defined(__EMSCRIPTEN__)
    savePath = QString::fromStdString(engine_.project().name()) + "." + ext;
#else
    savePath = QFileDialog::getSaveFileName(
        this,
        "Lưu video",
        defaultName,
        QString("Video (*.%1);;All Files (*.*)").arg(ext)
    );
#endif
    if (savePath.isEmpty()) return;

    QSize targetSize = getTargetResolution();
    int width = targetSize.width();
    int height = targetSize.height();

    const double fps = getTargetFrameRate();

    int totalFrames = static_cast<int>(std::ceil(totalSec * fps));
    if (totalFrames <= 0) totalFrames = 1;
    progressBar_->setRange(0, totalFrames);
    progressBar_->setValue(0);

    QString quality = qualityCombo_->currentData().toString();
    QString crf = "23";
    if (quality == "low") crf = "28";
    else if (quality == "medium") crf = "23";
    else if (quality == "high") crf = "18";
    else if (quality == "very_high") crf = "14";
    const qint64 selectedVideoBitrate = getTargetVideoBitrate();
    const auto exportQuality = quality == "low" ? exporting::ExportQuality::Low
        : quality == "medium" ? exporting::ExportQuality::Medium
        : quality == "very_high" ? exporting::ExportQuality::VeryHigh
        : exporting::ExportQuality::High;
    const qint64 browserVideoBitrate = selectedVideoBitrate > 0
        ? selectedVideoBitrate
        : std::clamp<qint64>(exporting::ExportGeometryResolver::calculateVideoBitrate(width, height, fps, exportQuality),
            1'000'000, 100'000'000);
    const QString bitrateArg = QString::number(selectedVideoBitrate);

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
         << "-r" << QString::number(fps, 'f', 6)
         << "-i" << "-";

    if(!includeAudio)args<<"-an";

    QString hwAccel = hwAccelCombo_ ? hwAccelCombo_->currentData().toString() : "cpu";

    if (fmt == "webm") {
        args << "-c:v" << "libvpx-vp9"
             << "-pix_fmt" << "yuv420p"
             << "-deadline" << "good" << "-cpu-used" << "4" << "-row-mt" << "1";
        if (selectedVideoBitrate > 0) {
            args << "-b:v" << bitrateArg
                 << "-maxrate" << bitrateArg
                 << "-bufsize" << QString::number(selectedVideoBitrate * 2);
        } else {
            args << "-crf" << crf << "-b:v" << "0";
        }
    } else {
        if (hwAccel == "amf") {
            // AMD GPU (AMF/VCE) — optimized for maximum GPU saturation
            const double bpp=quality=="low"?.07:quality=="medium"?.12:quality=="very_high"?.28:.18;
            const QString amfBitrate=selectedVideoBitrate > 0 ? bitrateArg
                : QString::number(std::max<qint64>(250000,qRound64(double(width)*height*fps*bpp)));
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
                 << "-rc" << "vbr";
            if (selectedVideoBitrate > 0)
                args << "-b:v" << bitrateArg << "-maxrate" << bitrateArg << "-bufsize" << QString::number(selectedVideoBitrate * 2);
            else args << "-cq" << crf;
        } else if (hwAccel == "qsv") {
            args << "-c:v" << "h264_qsv"
                 << "-pix_fmt" << "nv12"
                 << "-preset" << "medium";
            if (selectedVideoBitrate > 0)
                args << "-b:v" << bitrateArg << "-maxrate" << bitrateArg << "-bufsize" << QString::number(selectedVideoBitrate * 2);
            else args << "-global_quality" << crf;
        } else {
            args << "-c:v" << "libx264"
                 << "-pix_fmt" << "yuv420p"
                 << "-preset" << "fast";
            if (selectedVideoBitrate > 0)
                args << "-b:v" << bitrateArg << "-maxrate" << bitrateArg << "-bufsize" << QString::number(selectedVideoBitrate * 2);
            else args << "-crf" << crf;
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
        includeAudio,
        browserVideoBitrate
    );

    connect(worker_, &ExportWorker::progressUpdated, this, &ExportDialog::onWorkerProgress);
    connect(worker_, &ExportWorker::logOutputReceived, this, &ExportDialog::onWorkerLog);
    connect(worker_, &ExportWorker::exportFinished, this, &ExportDialog::onWorkerFinished);

    exportStartTime_ = std::chrono::steady_clock::now();
    smoothedExportFps_ = 0.0;
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

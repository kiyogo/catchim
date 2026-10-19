#include "PreviewPanel.h"

#if defined(HAVE_QT6)
#include "ui/icons/UiIcons.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/WebComboBox.h"
#include "core/time/Timecode.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

namespace catchim::ui {

PreviewPanel::PreviewPanel(
    editor::EditorEngine& engine,
    render::RenderEngine& renderEngine,
    media::MediaLibrary& mediaLibrary,
    QWidget* parent
)
    : QWidget(parent)
    , engine_(engine)
{
    setAttribute(Qt::WA_StyledBackground, true);
    previewWidget_ = new PreviewWidget(engine, renderEngine, mediaLibrary, this);
    setupUi();
    refresh();
}

void PreviewPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);
    mainLayout->setSpacing(0);

    mainLayout->addWidget(previewWidget_, 1);

    // Preview Toolbar (Matches web/src/preview/components/toolbar.tsx)
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("previewToolbar");
    toolbar->setFixedHeight(64);
    auto* tbLayout = new QGridLayout(toolbar);
    tbLayout->setContentsMargins(20, 20, 20, 12);
    tbLayout->setHorizontalSpacing(0);
    tbLayout->setColumnStretch(0, 1);
    tbLayout->setColumnStretch(2, 1);

    // Left: Timecode display
    const auto& pal = Theme::instance().palette();
    timecodeLabel_ = new QLabel("00:00:00:00 / 00:00:00:00", toolbar);
    timecodeLabel_->setObjectName("timecodeLabel");
    timecodeLabel_->setTextFormat(Qt::RichText);
    tbLayout->addWidget(timecodeLabel_, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);

    // Center: Play / Pause button (Single ghost icon button)
    playPauseBtn_ = new QPushButton(toolbar);
    playPauseBtn_->setObjectName("previewPlayBtn");
    playPauseBtn_->setFixedSize(32, 32);
    playPauseBtn_->setIcon(UiIcons::get(UiIcon::Play, pal.textPrimary, 18));
    playPauseBtn_->setIconSize(QSize(18, 18));
    playPauseBtn_->setCursor(Qt::PointingHandCursor);
    connect(playPauseBtn_, &QPushButton::clicked, this, &PreviewPanel::onPlayPauseClicked);
    tbLayout->addWidget(playPauseBtn_, 0, 1);

    auto* rightSection = new QWidget(toolbar);
    auto* rightLayout = new QHBoxLayout(rightSection);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    // Right: Zoom combo + Separator + Fullscreen
    zoomCombo_ = new WebComboBox(toolbar);
    zoomCombo_->setObjectName("previewZoomCombo");
    zoomCombo_->addItems({"Vừa khung", "25%", "50%", "75%", "100%", "150%", "200%"});
    zoomCombo_->setFixedWidth(106);
    zoomCombo_->setFixedHeight(28);
    connect(zoomCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PreviewPanel::onZoomChanged);
    rightLayout->addWidget(zoomCombo_);

    auto* sep = new QFrame(toolbar);
    sep->setFixedWidth(1);
    sep->setFixedHeight(16);
    sep->setStyleSheet(QString("background-color: %1;").arg(pal.border.name()));
    rightLayout->addWidget(sep);

    auto* fsBtn = new QPushButton(toolbar);
    fsBtn->setObjectName("previewFsBtn");
    fsBtn->setFixedSize(32, 32);
    fsBtn->setIcon(UiIcons::get(UiIcon::Fullscreen, pal.textSecondary, 16));
    fsBtn->setIconSize(QSize(16, 16));
    fsBtn->setCursor(Qt::PointingHandCursor);
    connect(fsBtn, &QPushButton::clicked, [this]() {
        QWidget* top = window();
        if (top) {
            if (top->isFullScreen()) {
                top->showNormal();
            } else {
                top->showFullScreen();
            }
        }
    });
    rightLayout->addWidget(fsBtn);
    tbLayout->addWidget(rightSection, 0, 2, Qt::AlignRight | Qt::AlignVCenter);

    mainLayout->addWidget(toolbar);
}

void PreviewPanel::refresh() {
    core::TimelineTime curTime = engine_.playback().currentTime();
    core::TimelineTime dur = engine_.project().totalDuration();
    const auto& fps = engine_.project().settings().fps;

    std::string curTc = core::Timecode::format(curTime, core::TimecodeFormat::HH_MM_SS_FF, fps);
    std::string durTc = core::Timecode::format(dur, core::TimecodeFormat::HH_MM_SS_FF, fps);

    const auto& pal = Theme::instance().palette();
    QString tcHtml = QString("<span style=\"color: %1;\">%2</span> <span style=\"color: %3;\">/ %4</span>")
        .arg(pal.primaryAccent.name())
        .arg(QString::fromStdString(curTc))
        .arg(pal.textSecondary.name())
        .arg(QString::fromStdString(durTc));
    timecodeLabel_->setText(tcHtml);
    playPauseBtn_->setIcon(engine_.playback().isPlaying() ?
        UiIcons::get(UiIcon::Pause, pal.textPrimary, 18) :
        UiIcons::get(UiIcon::Play, pal.textPrimary, 18));

    // For non-playback refresh (seek, clip add, selection change):
    // decode the frame now so it shows up immediately
    if (previewWidget_) {
        previewWidget_->updateFrameIfNeeded();
    }
}

void PreviewPanel::updateFrame() {
    core::TimelineTime curTime = engine_.playback().currentTime();
    core::TimelineTime dur = engine_.project().totalDuration();
    const auto& fps = engine_.project().settings().fps;

    std::string curTc = core::Timecode::format(curTime, core::TimecodeFormat::HH_MM_SS_FF, fps);
    std::string durTc = core::Timecode::format(dur, core::TimecodeFormat::HH_MM_SS_FF, fps);
    if (timecodeLabel_) {
        const auto& pal = Theme::instance().palette();
        QString tcHtml = QString("<span style=\"color: %1;\">%2</span> <span style=\"color: %3;\">/ %4</span>")
            .arg(pal.primaryAccent.name())
            .arg(QString::fromStdString(curTc))
            .arg(pal.textSecondary.name())
            .arg(QString::fromStdString(durTc));
        timecodeLabel_->setText(tcHtml);
    }

    // Decode frame if time changed
    if (previewWidget_) {
        previewWidget_->updateFrameIfNeeded();
    }
}

void PreviewPanel::markFrameDirty() {
    if (previewWidget_) {
        previewWidget_->markFrameDirty();
    }
}

void PreviewPanel::onPlayPauseClicked() {
    engine_.togglePlay();
    refresh();
}

void PreviewPanel::onZoomChanged(int index) {
    if (index == 0) {
        previewWidget_->fitToScreen();
    } else {
        static const double factors[] = {-1.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0};
        previewWidget_->setZoomFactor(factors[index]);
    }
}

void PreviewPanel::toggleSafeZones() {
    if (previewWidget_) {
        previewWidget_->toggleSafeZones();
    }
}

void PreviewPanel::toggleFullscreen() {
    QWidget* top = window();
    if (top) {
        if (top->isFullScreen()) {
            top->showNormal();
        } else {
            top->showFullScreen();
        }
    }
}

} // namespace catchim::ui
#endif

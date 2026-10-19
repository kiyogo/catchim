#include "TimelineToolbar.h"

#if defined(HAVE_QT6)
#include "ui/icons/UiIcons.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ScenesDrawer.h"
#include <QHBoxLayout>
#include <QMenu>
#include <QSignalBlocker>

namespace catchim::ui {

TimelineToolbar::TimelineToolbar(editor::EditorEngine& engine, QWidget* parent)
    : QWidget(parent)
    , engine_(engine)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("timelineToolbar");
    setFixedHeight(40);
    setupUi();
    refresh();
}

void TimelineToolbar::setupUi() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(8, 4, 8, 4);
    root->setSpacing(0);
    auto* leftTools = new QWidget(this);
    leftTools->setObjectName("timelineLeftTools");
    auto* layout = new QHBoxLayout(leftTools);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    const auto& pal = Theme::instance().palette();
    auto makeIconBtn = [this, pal](UiIcon icon, const QString& tip) {
        auto* btn = new QPushButton(this);
        btn->setFixedSize(28, 28);
        btn->setIcon(UiIcons::get(icon, pal.textPrimary, 16));
        btn->setIconSize(QSize(16, 16));
        btn->setToolTip(tip);
        return btn;
    };

    // Left tools (Matches web timeline-toolbar.tsx)
    splitBtn_ = makeIconBtn(UiIcon::Split, "Cắt phần tử");
    connect(splitBtn_, &QPushButton::clicked, this, &TimelineToolbar::onSplitClicked);

    splitLeftBtn_ = makeIconBtn(UiIcon::SplitLeft, "Cắt và xóa bên trái");
    connect(splitLeftBtn_, &QPushButton::clicked, [this]() {
        engine_.splitLeftAtPlayhead();
    });

    splitRightBtn_ = makeIconBtn(UiIcon::SplitRight, "Cắt và xóa bên phải");
    connect(splitRightBtn_, &QPushButton::clicked, [this]() {
        engine_.splitRightAtPlayhead();
    });

    unlinkBtn_ = makeIconBtn(UiIcon::Link, "Tách hoặc khôi phục âm thanh gốc");
    unlinkBtn_->setEnabled(false);
    connect(unlinkBtn_, &QPushButton::clicked, [this]() {
        engine_.toggleSourceAudioSeparation();
    });

    dupBtn_ = makeIconBtn(UiIcon::Duplicate, "Nhân đôi phần tử");
    connect(dupBtn_, &QPushButton::clicked, this, &TimelineToolbar::onDuplicateClicked);

    auto* freezeBtn = makeIconBtn(UiIcon::Snow, "Đóng băng khung hình");
    freezeBtn->setEnabled(false);

    deleteBtn_ = makeIconBtn(UiIcon::Delete, "Xóa phần tử");
    connect(deleteBtn_, &QPushButton::clicked, this, &TimelineToolbar::onDeleteClicked);

    auto* leftSep = new QFrame(this);
    leftSep->setFixedWidth(1);
    leftSep->setFixedHeight(16);
    leftSep->setStyleSheet(QString("background-color: %1;").arg(pal.border.name()));

    bookmarkBtn_ = makeIconBtn(UiIcon::Bookmark, "Thêm dấu mốc");
    bookmarkBtn_->setCheckable(true);
    connect(bookmarkBtn_, &QPushButton::clicked, [this]() {
        engine_.toggleBookmarkAtPlayhead();
        refresh();
    });

    auto* curveBtn = makeIconBtn(UiIcon::Chart, "Trình chỉnh sửa đường cong");
    curveBtn->setEnabled(false);

    layout->addWidget(splitBtn_);
    layout->addWidget(splitLeftBtn_);
    layout->addWidget(splitRightBtn_);
    layout->addWidget(unlinkBtn_);
    layout->addWidget(dupBtn_);
    layout->addWidget(freezeBtn);
    layout->addWidget(deleteBtn_);
    layout->addWidget(leftSep);
    layout->addWidget(bookmarkBtn_);
    layout->addWidget(curveBtn);

    root->addWidget(leftTools, 0, Qt::AlignLeft);
    root->addStretch();

    // Center Scene selector (Matches web SplitButton)
    auto* sceneSplitWidget = new QWidget(this);
    sceneSplitWidget->setObjectName("sceneSplitWidget");
    sceneSplitWidget->setFixedHeight(28);
    sceneSplitWidget->setToolTip("Chọn hoặc quản lý cảnh");
    auto* splitLayout = new QHBoxLayout(sceneSplitWidget);
    splitLayout->setContentsMargins(0, 0, 0, 0);
    splitLayout->setSpacing(0);

    sceneLabelBtn_ = new QPushButton("Cảnh chính", sceneSplitWidget);
    sceneLabelBtn_->setObjectName("sceneLabelBtn");
    sceneLabelBtn_->setFixedSize(88, 26);
    sceneLabelBtn_->setToolTip("Cảnh đang hoạt động");
    sceneLabelBtn_->setCursor(Qt::PointingHandCursor);

    sceneLayersBtn_ = new QPushButton(sceneSplitWidget);
    sceneLayersBtn_->setObjectName("sceneLayersBtn");
    sceneLayersBtn_->setFixedSize(36, 26);
    sceneLayersBtn_->setIcon(UiIcons::get(UiIcon::Layers, pal.textSecondary, 14));
    sceneLayersBtn_->setIconSize(QSize(14, 14));
    sceneLayersBtn_->setToolTip("Quản lý cảnh");
    sceneLayersBtn_->setCursor(Qt::PointingHandCursor);

    splitLayout->addWidget(sceneLabelBtn_);
    splitLayout->addWidget(sceneLayersBtn_);
    auto openScenes=[this]{ScenesDrawer drawer(engine_,window());drawer.exec();refresh();};
    connect(sceneLabelBtn_,&QPushButton::clicked,this,openScenes);
    connect(sceneLayersBtn_,&QPushButton::clicked,this,openScenes);
    root->addWidget(sceneSplitWidget);
    root->addStretch();
    auto* rightTools = new QWidget(this);
    rightTools->setObjectName("timelineRightTools");
    auto* rightLayout = new QHBoxLayout(rightTools);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);
    root->addWidget(rightTools, 0, Qt::AlignRight);

    snapBtn_ = makeIconBtn(UiIcon::Magnet, "Bắt dính");
    snapBtn_->setCheckable(true);
    snapBtn_->setChecked(engine_.isSnappingEnabled());
    connect(snapBtn_, &QPushButton::clicked, this, &TimelineToolbar::onSnappingToggled);

    rippleBtn_ = makeIconBtn(UiIcon::Ripple, "Chỉnh sửa ripple");
    rippleBtn_->setCheckable(true);
    rippleBtn_->setChecked(engine_.isRippleEnabled());
    connect(rippleBtn_, &QPushButton::clicked, this, &TimelineToolbar::onRippleToggled);

    rightLayout->addWidget(snapBtn_);
    rightLayout->addWidget(rippleBtn_);

    auto* rightSep = new QFrame(this);
    rightSep->setFixedWidth(1);
    rightSep->setFixedHeight(16);
    rightSep->setStyleSheet(QString("background-color: %1;").arg(pal.border.name()));
    rightLayout->addWidget(rightSep);

    // Zoom slider
    auto* zoomOutBtn = makeIconBtn(UiIcon::ZoomOut, "Thu nhỏ");
    auto* zoomInBtn = makeIconBtn(UiIcon::ZoomIn, "Phóng to");

    zoomSlider_ = new QSlider(Qt::Horizontal, this);
    zoomSlider_->setObjectName("timelineZoomSlider");
    zoomSlider_->setToolTip("Thu phóng dòng thời gian");
    zoomSlider_->setFixedWidth(112);
    zoomSlider_->setRange(0, 100);
    setZoomFactor(8.82);
    connect(zoomSlider_, &QSlider::valueChanged, this, &TimelineToolbar::onZoomSliderChanged);

    connect(zoomOutBtn, &QPushButton::clicked, [this]() {
        zoomSlider_->setValue(std::max(0, zoomSlider_->value() - 5));
    });
    connect(zoomInBtn, &QPushButton::clicked, [this]() {
        zoomSlider_->setValue(std::min(100, zoomSlider_->value() + 5));
    });

    rightLayout->addWidget(zoomOutBtn);
    rightLayout->addWidget(zoomSlider_);
    rightLayout->addWidget(zoomInBtn);
}

void TimelineToolbar::refresh() {
    if(const auto* scene=engine_.project().activeScene()) {
        const auto name=QString::fromStdString(scene->name());
        sceneLabelBtn_->setText(scene->isMain()&&name=="Main scene"?"Cảnh chính":sceneLabelBtn_->fontMetrics().elidedText(name,Qt::ElideRight,78));
        sceneLabelBtn_->setToolTip(name);
    }
    const auto* timeline=engine_.activeTimeline();
    const auto& selected=engine_.selectedClips();
    const auto* clip=timeline&&selected.size()==1?timeline->findClip(selected.front()):nullptr;
    unlinkBtn_->setEnabled(clip&&clip->type()==editor::ClipType::Video&&clip->getParam<bool>("hasAudio",true));
    const bool hasSelection=!selected.empty();splitBtn_->setEnabled(hasSelection);splitLeftBtn_->setEnabled(hasSelection);splitRightBtn_->setEnabled(hasSelection);dupBtn_->setEnabled(hasSelection);deleteBtn_->setEnabled(hasSelection);
    if (timeline) {
        const bool marked = timeline->hasBookmarkAt(engine_.playback().currentTime(), core::TimelineTime::fromSeconds(0.001));
        bookmarkBtn_->setChecked(marked);
        bookmarkBtn_->setToolTip(marked ? "Xóa dấu mốc" : "Thêm dấu mốc");
    }
    snapBtn_->setChecked(engine_.isSnappingEnabled());
    rippleBtn_->setChecked(engine_.isRippleEnabled());
    const auto& pal = Theme::instance().palette();
    bool isDark = (Theme::instance().mode() == ThemeMode::Dark);
    QColor activeIconCol = isDark ? QColor("#ffffff") : pal.primaryAccent;

    if (splitLeftBtn_) splitLeftBtn_->setIcon(UiIcons::get(UiIcon::SplitLeft, pal.textPrimary, 16));
    if (splitBtn_) splitBtn_->setIcon(UiIcons::get(UiIcon::Split, pal.textPrimary, 16));
    if (splitRightBtn_) splitRightBtn_->setIcon(UiIcons::get(UiIcon::SplitRight, pal.textPrimary, 16));
    if (unlinkBtn_) unlinkBtn_->setIcon(UiIcons::get(UiIcon::Link, pal.textPrimary, 16));
    if (dupBtn_) dupBtn_->setIcon(UiIcons::get(UiIcon::Duplicate, pal.textPrimary, 16));
    if (deleteBtn_) deleteBtn_->setIcon(UiIcons::get(UiIcon::Delete, pal.textPrimary, 16));
    if (bookmarkBtn_) bookmarkBtn_->setIcon(UiIcons::get(UiIcon::Bookmark, pal.textPrimary, 16));
    if (snapBtn_) snapBtn_->setIcon(UiIcons::get(UiIcon::Magnet, snapBtn_->isChecked() ? activeIconCol : pal.textPrimary, 16));
    if (rippleBtn_) rippleBtn_->setIcon(UiIcons::get(UiIcon::Ripple, rippleBtn_->isChecked() ? activeIconCol : pal.textPrimary, 16));
    if (sceneLayersBtn_) sceneLayersBtn_->setIcon(UiIcons::get(UiIcon::Layers, pal.textSecondary, 14));
}

void TimelineToolbar::onSplitClicked() {
    engine_.splitAtPlayhead();
}

void TimelineToolbar::onDeleteClicked() {
    engine_.deleteSelectedClips();
}

void TimelineToolbar::onDuplicateClicked() {
    engine_.duplicateSelectedClips();
}

void TimelineToolbar::onSnappingToggled() {
    engine_.setSnappingEnabled(snapBtn_->isChecked());
}

void TimelineToolbar::onRippleToggled() {
    engine_.setRippleEnabled(rippleBtn_->isChecked());
}

void TimelineToolbar::onZoomSliderChanged(int val) {
    double minZoom = 0.02;
    double maxZoom = 100.0;
    double pos = static_cast<double>(val) / 100.0;
    double zoom = minZoom * std::pow(maxZoom / minZoom, pos);
    emit zoomChanged(zoom);
}

void TimelineToolbar::setZoomFactor(double factor) {
    QSignalBlocker guard(zoomSlider_);
    zoomSlider_->setValue(qRound(100*std::log(std::clamp(factor,.02,100.)/.02)/std::log(100./.02)));
}

} // namespace catchim::ui
#endif

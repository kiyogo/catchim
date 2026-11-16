#include "TimelineToolbar.h"

#if defined(HAVE_QT6)
#include "ui/icons/UiIcons.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ScenesDrawer.h"
#include "media/decoder/NativeVideoDecoder.h"
#include "editor/history/commands/BatchCommand.h"
#include "editor/history/commands/ProjectCommands.h"
#include <QHBoxLayout>
#include <QMenu>
#include <QSignalBlocker>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace catchim::ui {
namespace {

constexpr int kSceneAnalysisWidth = 64;
constexpr int kSceneAnalysisHeight = 36;
constexpr int kSceneAnalysisMaxSamples = 180;
constexpr double kSceneAnalysisMinInterval = 0.35;
constexpr double kSceneCutThreshold = 0.2;
constexpr double kSceneCutMinGap = 1.25;

struct SceneCutSuggestion {
    double clipTimeSeconds{0.0};
    double score{0.0};
};

struct SceneCutAnalysis {
    std::vector<SceneCutSuggestion> suggestions;
    QString error;
};

SceneCutAnalysis detectSceneCuts(
    const std::filesystem::path& path,
    double sourceStartSeconds,
    double clipDurationSeconds,
    double playbackRate,
    double sourceDurationSeconds,
    const std::function<void(double)>& onProgress) {
    if (!std::isfinite(sourceStartSeconds) || !std::isfinite(clipDurationSeconds) ||
        !std::isfinite(playbackRate) || sourceStartSeconds < 0.0 ||
        clipDurationSeconds <= 0.0 || playbackRate <= 0.0) {
        return {{}, QStringLiteral("Clip không có thời gian hợp lệ để dò chuyển cảnh.")};
    }

    media::NativeVideoDecoder decoder;
    decoder.setPreviewSize(kSceneAnalysisWidth, kSceneAnalysisHeight);
    const double sampleInterval = std::max(
        kSceneAnalysisMinInterval,
        clipDurationSeconds / kSceneAnalysisMaxSamples);
    const int sampleCount = std::min(
        kSceneAnalysisMaxSamples,
        std::max(2, static_cast<int>(std::ceil(clipDurationSeconds / sampleInterval))));

    struct ScoredSample {
        double clipTimeSeconds;
        double score;
    };
    std::vector<ScoredSample> scores;
    std::vector<uint8_t> previous;
    for (int sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex) {
        const double clipTimeSeconds = std::clamp(
            sampleIndex * sampleInterval,
            0.0,
            std::max(0.0, clipDurationSeconds - 0.02));
        const double sourceTime = sourceStartSeconds + clipTimeSeconds * playbackRate;
        if (std::isfinite(sourceDurationSeconds) && sourceDurationSeconds > 0.0 &&
            sourceTime >= sourceDurationSeconds) {
            break;
        }

        int width = 0;
        int height = 0;
        std::vector<uint8_t> rgba;
        if (!decoder.getFrame(path, sourceTime, width, height, rgba) ||
            width <= 0 || height <= 0 || rgba.size() != static_cast<size_t>(width) * height * 4) {
            return {{}, QString("Không thể giải mã frame tại %1 giây.").arg(sourceTime, 0, 'f', 2)};
        }

        if (!previous.empty()) {
            double difference = 0.0;
            size_t pixelCount = 0;
            for (size_t index = 0; index + 3 < rgba.size(); index += 4) {
                const double previousLuma = previous[index] * 0.2126 +
                    previous[index + 1] * 0.7152 + previous[index + 2] * 0.0722;
                const double currentLuma = rgba[index] * 0.2126 +
                    rgba[index + 1] * 0.7152 + rgba[index + 2] * 0.0722;
                const double colorDifference = (
                    std::abs(static_cast<int>(rgba[index]) - static_cast<int>(previous[index])) +
                    std::abs(static_cast<int>(rgba[index + 1]) - static_cast<int>(previous[index + 1])) +
                    std::abs(static_cast<int>(rgba[index + 2]) - static_cast<int>(previous[index + 2]))) / (3.0 * 255.0);
                difference += 0.6 * std::abs(currentLuma - previousLuma) / 255.0 + 0.4 * colorDifference;
                ++pixelCount;
            }
            scores.push_back({clipTimeSeconds, pixelCount ? difference / pixelCount : 0.0});
        }
        previous = std::move(rgba);
        if (onProgress) onProgress(static_cast<double>(sampleIndex + 1) / (sampleCount + 1));
    }

    SceneCutAnalysis result;
    for (size_t index = 0; index < scores.size(); ++index) {
        const double previousScore = index > 0 ? scores[index - 1].score : 0.0;
        const double nextScore = index + 1 < scores.size() ? scores[index + 1].score : 0.0;
        const auto& current = scores[index];
        const bool isPeak = current.score >= previousScore && current.score > nextScore;
        const auto last = result.suggestions.empty() ? nullptr : &result.suggestions.back();
        if (current.score >= kSceneCutThreshold && isPeak &&
            (!last || current.clipTimeSeconds - last->clipTimeSeconds >= kSceneCutMinGap)) {
            result.suggestions.push_back({current.clipTimeSeconds, current.score});
        }
    }
    return result;
}

} // namespace

TimelineToolbar::TimelineToolbar(editor::EditorEngine& engine, media::MediaLibrary& mediaLibrary, QWidget* parent)
    : QWidget(parent)
    , engine_(engine)
    , mediaLibrary_(mediaLibrary)
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

    sceneCutsBtn_ = makeIconBtn(UiIcon::Video, "Dò điểm chuyển cảnh trên video đã chọn");
    connect(sceneCutsBtn_, &QPushButton::clicked, this, [this]() {
        auto* timeline = engine_.activeTimeline();
        const auto& selected = engine_.selectedClips();
        if (!timeline || selected.size() != 1) return;
        const auto* clip = timeline->findClip(selected.front());
        if (!clip || clip->type() != editor::ClipType::Video) return;
        const auto asset = mediaLibrary_.findAsset(clip->mediaId());
        if (!asset || !asset->hasVideo()) {
            QMessageBox::information(this, "Dò chuyển cảnh", "Không tìm thấy video để phân tích.");
            return;
        }

        const auto clipId = clip->id();
        const auto projectId = engine_.project().metadata().id;
        const auto sceneId = engine_.project().currentSceneId();
        const auto startTime = clip->startTime();
        const auto trimStart = clip->trimStart();
        const auto clipDuration = clip->duration();
        const double playbackRate = std::max(0.01, clip->getParam<double>("speed", 1.0));
        const auto path = asset->filePath();
        const double sourceDuration = asset->duration().toSeconds();

        isDetectingSceneCuts_ = true;
        sceneCutsBtn_->setEnabled(false);
        sceneCutsBtn_->setToolTip("Đang phân tích frame video…");
        QPointer<TimelineToolbar> target(this);
        QThreadPool::globalInstance()->start(QRunnable::create(
            [target, path, sourceStart = trimStart.toSeconds(),
             duration = clipDuration.toSeconds(), playbackRate, sourceDuration,
             clipId, projectId, sceneId, startTime, trimStart, clipDuration]() {
                int lastProgress = -1;
                auto result = detectSceneCuts(
                    path, sourceStart, duration, playbackRate, sourceDuration,
                    [target, &lastProgress](double progress) {
                        const int percent = static_cast<int>(std::round(progress * 100.0));
                        if (percent == lastProgress || percent % 5 != 0) return;
                        lastProgress = percent;
                        if (!target) return;
                        QMetaObject::invokeMethod(target.data(), [target, percent] {
                            if (target && target->sceneCutsBtn_) {
                                target->sceneCutsBtn_->setToolTip(
                                    QString("Đang phân tích frame video… %1%").arg(percent));
                            }
                        }, Qt::QueuedConnection);
                    });

                if (!target) return;
                QMetaObject::invokeMethod(target.data(),
                    [target, result = std::move(result), clipId, projectId, sceneId,
                     startTime, trimStart, clipDuration, playbackRate]() mutable {
                        if (!target) return;
                        target->isDetectingSceneCuts_ = false;
                        target->refresh();
                        if (!result.error.isEmpty()) {
                            QMessageBox::warning(target.data(), "Dò chuyển cảnh", result.error);
                            return;
                        }

                        auto* currentTimeline = target->engine_.activeTimeline();
                        const auto* currentClip = currentTimeline ? currentTimeline->findClip(clipId) : nullptr;
                        if (target->engine_.project().metadata().id != projectId ||
                            target->engine_.project().currentSceneId() != sceneId || !currentClip ||
                            currentClip->startTime() != startTime || currentClip->trimStart() != trimStart ||
                            currentClip->duration() != clipDuration ||
                            std::max(0.01, currentClip->getParam<double>("speed", 1.0)) != playbackRate) {
                            QMessageBox::information(target.data(), "Dò chuyển cảnh",
                                "Dự án, cảnh hoặc clip đã thay đổi trong lúc phân tích. Hãy thử lại.");
                            return;
                        }

                        auto batch = std::make_unique<editor::BatchCommand>("Detect Scene Cuts");
                        size_t added = 0;
                        for (const auto& suggestion : result.suggestions) {
                            const auto time = startTime + core::TimelineTime::fromSeconds(suggestion.clipTimeSeconds);
                            if (currentTimeline->hasBookmarkAt(time, core::TimelineTime::fromSeconds(0.02))) continue;
                            batch->addCommand(std::make_unique<editor::ToggleBookmarkCommand>(
                                *currentTimeline, time, "Scene cut", "#38bdf8"));
                            ++added;
                        }
                        if (added > 0 && target->engine_.history().execute(std::move(batch))) {
                            target->engine_.project().setDirty(true);
                            target->engine_.notifyTimelineChanged();
                        } else {
                            added = 0;
                        }
                        QMessageBox::information(target.data(), "Dò chuyển cảnh", added > 0
                            ? QString("Đã thêm %1 dấu mốc chuyển cảnh vào timeline.").arg(added)
                            : QString("Không tìm thấy điểm chuyển cảnh rõ mới trong clip này."));
                    }, Qt::QueuedConnection);
            }));
    });

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
    layout->addWidget(sceneCutsBtn_);
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
    const auto clipAsset=clip&&clip->type()==editor::ClipType::Video
        ?mediaLibrary_.findAsset(clip->mediaId()):nullptr;
    sceneCutsBtn_->setEnabled(clipAsset && clipAsset->hasVideo() && !isDetectingSceneCuts_);
    if (!isDetectingSceneCuts_) sceneCutsBtn_->setToolTip("Dò điểm chuyển cảnh trên video đã chọn");
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
    if (sceneCutsBtn_) sceneCutsBtn_->setIcon(UiIcons::get(UiIcon::Video, pal.textPrimary, 16));
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

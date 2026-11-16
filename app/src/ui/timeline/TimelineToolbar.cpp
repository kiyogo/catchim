#include "TimelineToolbar.h"

#if defined(HAVE_QT6)
#include "ui/icons/UiIcons.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/ScenesDrawer.h"
#include "media/decoder/NativeVideoDecoder.h"
#include "editor/animation/AnimationValueResolvers.h"
#include "editor/animation/GraphEditorEasingPresets.h"
#include "editor/animation/GraphEditorSessionEngine.h"
#include "editor/history/commands/KeyframeCommands.h"
#include "editor/history/commands/BatchCommand.h"
#include "editor/history/commands/ProjectCommands.h"
#include <QHBoxLayout>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QMenu>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QSettings>
#include <QTabWidget>
#include <QUuid>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>
#if defined(__EMSCRIPTEN__)
#include <emscripten/val.h>
#endif

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

using NormalizedCurve = editor::NormalizedCubicBezier;

struct EditableCurveSegment {
    QString label;
    std::string propertyPath;
    core::TimelineTime keyframeTime{0};
    NormalizedCurve original;
    NormalizedCurve curve;
    double referenceSpanValue{1.0};
};

bool curvesEqual(const NormalizedCurve& a, const NormalizedCurve& b) {
    constexpr double epsilon = 1e-6;
    return std::abs(a.x1 - b.x1) < epsilon && std::abs(a.y1 - b.y1) < epsilon &&
        std::abs(a.x2 - b.x2) < epsilon && std::abs(a.y2 - b.y2) < epsilon;
}

class BezierCurveWidget final : public QWidget {
public:
    explicit BezierCurveWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(220, 136);
        setMouseTracking(true);
        setCursor(Qt::CrossCursor);
    }

    void setCurve(const NormalizedCurve& curve) {
        curve_ = curve;
        update();
    }

    const NormalizedCurve& curve() const noexcept { return curve_; }
    std::function<void(const NormalizedCurve&)> onCurveChanged;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF plot(14.0, 10.0, std::max(1, width() - 28), std::max(1, height() - 20));
        painter.fillRect(rect(), palette().window());
        painter.fillRect(plot, palette().alternateBase());
        painter.setPen(palette().mid().color());
        painter.drawRect(plot);

        const auto mapPoint = [&plot](double x, double y) {
            return QPointF(plot.left() + std::clamp(x, 0.0, 1.0) * plot.width(),
                           plot.top() + (1.0 - y) * plot.height());
        };
        const QPointF p0 = mapPoint(0.0, 0.0);
        const QPointF p3 = mapPoint(1.0, 1.0);
        const QPointF c1 = mapPoint(curve_.x1, std::clamp(curve_.y1, 0.0, 1.0));
        const QPointF c2 = mapPoint(curve_.x2, std::clamp(curve_.y2, 0.0, 1.0));

        QPen guide(palette().mid().color(), 1.0, Qt::DashLine);
        painter.setPen(guide);
        painter.drawLine(p0, p3);

        QPen handles(QColor("#38bdf8"), 1.0);
        handles.setColor(QColor(56, 189, 248, 130));
        painter.setPen(handles);
        painter.drawLine(p0, c1);
        painter.drawLine(p3, c2);

        QPainterPath path;
        path.moveTo(p0);
        path.cubicTo(mapPoint(curve_.x1, curve_.y1), mapPoint(curve_.x2, curve_.y2), p3);
        painter.setPen(QPen(QColor("#0ea5e9"), 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawPath(path);

        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().text().color());
        painter.drawEllipse(p0, 2.0, 2.0);
        painter.drawEllipse(p3, 2.0, 2.0);
        painter.setBrush(QColor("#0ea5e9"));
        painter.setPen(QPen(Qt::white, 1.0));
        painter.drawEllipse(c1, 4.0, 4.0);
        painter.drawEllipse(c2, 4.0, 4.0);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) return;
        const auto [c1, c2] = handlePoints();
        const QPointF position = event->position();
        const double d1 = QLineF(position, c1).length();
        const double d2 = QLineF(position, c2).length();
        if (d1 <= 12.0 && d1 <= d2) activeHandle_ = 1;
        else if (d2 <= 12.0) activeHandle_ = 2;
        else return;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (activeHandle_ == 0) {
            const auto [c1, c2] = handlePoints();
            const QPointF position = event->position();
            setCursor(QLineF(position, c1).length() < 12.0 || QLineF(position, c2).length() < 12.0
                ? Qt::OpenHandCursor : Qt::CrossCursor);
            return;
        }
        const QRectF plot(14.0, 10.0, std::max(1, width() - 28), std::max(1, height() - 20));
        const QPointF position = event->position();
        const double x = std::clamp((position.x() - plot.left()) / plot.width(), 0.0, 1.0);
        double y = std::clamp(1.0 - (position.y() - plot.top()) / plot.height(), -0.5, 1.5);
        if (!(event->modifiers() & Qt::ShiftModifier)) {
            if (std::abs(y) < 0.06) y = 0.0;
            else if (std::abs(y - 1.0) < 0.06) y = 1.0;
        }
        if (activeHandle_ == 1) { curve_.x1 = x; curve_.y1 = y; }
        else { curve_.x2 = x; curve_.y2 = y; }
        update();
        if (onCurveChanged) onCurveChanged(curve_);
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && activeHandle_ != 0) {
            activeHandle_ = 0;
            setCursor(Qt::OpenHandCursor);
            event->accept();
        }
    }

private:
    std::pair<QPointF, QPointF> handlePoints() const {
        const QRectF plot(14.0, 10.0, std::max(1, width() - 28), std::max(1, height() - 20));
        const auto mapPoint = [&plot](double x, double y) {
            return QPointF(plot.left() + std::clamp(x, 0.0, 1.0) * plot.width(),
                           plot.top() + (1.0 - std::clamp(y, 0.0, 1.0)) * plot.height());
        };
        return {mapPoint(curve_.x1, curve_.y1), mapPoint(curve_.x2, curve_.y2)};
    }

    NormalizedCurve curve_;
    int activeHandle_{0};
};

class CurveEditorDialog final : public QDialog {
public:
    explicit CurveEditorDialog(std::vector<EditableCurveSegment> segments, QWidget* parent = nullptr)
        : QDialog(parent), segments_(std::move(segments)) {
        loadCustomPresets();
        setWindowTitle(QStringLiteral("Chỉnh đường cong easing"));
        setModal(true);
        setMinimumWidth(310);
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(14, 14, 14, 12);
        root->setSpacing(10);

        auto* selectorLabel = new QLabel(QStringLiteral("Thuộc tính và đoạn keyframe"), this);
        root->addWidget(selectorLabel);
        segmentPicker_ = new QComboBox(this);
        for (const auto& segment : segments_) segmentPicker_->addItem(segment.label);
        root->addWidget(segmentPicker_);

        graph_ = new BezierCurveWidget(this);
        root->addWidget(graph_);
        graph_->onCurveChanged = [this](const NormalizedCurve& curve) {
            if (activeIndex_ >= 0 && activeIndex_ < static_cast<int>(segments_.size())) {
                segments_[activeIndex_].curve = curve;
            }
        };

        auto* presetTabs = new QTabWidget(this);
        auto* builtinPage = new QWidget(presetTabs);
        auto* builtinGrid = new QGridLayout(builtinPage);
        builtinGrid->setContentsMargins(4, 6, 4, 4);
        builtinGrid->setSpacing(4);
        int builtinIndex = 0;
        for (const auto& preset : presets_.allPresets()) {
            if (preset.isCustom) continue;
            const NormalizedCurve curve{
                preset.curve.x1, preset.curve.y1, preset.curve.x2, preset.curve.y2
            };
            auto* button = new QPushButton(QString::fromStdString(preset.label), builtinPage);
            button->setFixedHeight(28);
            builtinGrid->addWidget(button, builtinIndex / 3, builtinIndex % 3);
            ++builtinIndex;
            connect(button, &QPushButton::clicked, this, [this, curve] { setActiveCurve(curve); });
        }
        presetTabs->addTab(builtinPage, QStringLiteral("Mẫu có sẵn"));

        auto* savedPage = new QWidget(presetTabs);
        auto* savedLayout = new QVBoxLayout(savedPage);
        savedLayout->setContentsMargins(4, 6, 4, 4);
        savedLayout->setSpacing(6);
        auto* savePresetButton = new QPushButton(QStringLiteral("+ Lưu mẫu hiện tại"), savedPage);
        connect(savePresetButton, &QPushButton::clicked, this, [this] {
            if (activeIndex_ < 0 || activeIndex_ >= static_cast<int>(segments_.size())) return;
            const auto curve = graph_->curve();
            const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
            const auto label = std::string("Custom ") + std::to_string(presets_.customPresets().size() + 1);
            presets_.addCustomPreset(id, label, {curve.x1, curve.y1, curve.x2, curve.y2});
            persistCustomPresets();
            rebuildSavedPresets();
        });
        savedLayout->addWidget(savePresetButton);
        auto* savedScroll = new QScrollArea(savedPage);
        savedScroll->setWidgetResizable(true);
        savedScroll->setFrameShape(QFrame::NoFrame);
        savedScroll->setMaximumHeight(150);
        auto* savedContainer = new QWidget(savedScroll);
        savedGrid_ = new QGridLayout(savedContainer);
        savedGrid_->setContentsMargins(0, 0, 0, 0);
        savedGrid_->setSpacing(4);
        savedScroll->setWidget(savedContainer);
        savedLayout->addWidget(savedScroll);
        presetTabs->addTab(savedPage, QStringLiteral("Đã lưu"));
        root->addWidget(presetTabs);
        rebuildSavedPresets();


        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
        buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Áp dụng"));
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        root->addWidget(buttons);

        connect(segmentPicker_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            activeIndex_ = index;
            if (index >= 0 && index < static_cast<int>(segments_.size())) graph_->setCurve(segments_[index].curve);
        });
        if (!segments_.empty()) graph_->setCurve(segments_.front().curve);
    }

    const std::vector<EditableCurveSegment>& segments() const noexcept { return segments_; }

private:
    void setActiveCurve(const NormalizedCurve& curve) {
        graph_->setCurve(curve);
        if (activeIndex_ >= 0 && activeIndex_ < static_cast<int>(segments_.size())) {
            segments_[activeIndex_].curve = curve;
        }
    }

    void loadCustomPresets() {
#if defined(__EMSCRIPTEN__)
        try {
            const auto stored = emscripten::val::global("localStorage").call<emscripten::val>(
                "getItem", std::string("graph-editor-presets"));
            if (stored.isNull() || stored.isUndefined()) return;
            const auto document = QJsonDocument::fromJson(
                QString::fromStdString(stored.as<std::string>()).toUtf8());
            if (!document.isArray()) return;
            for (const auto& entry : document.array()) {
                const auto preset = entry.toObject();
                const auto id = preset.value(QStringLiteral("id")).toString();
                const auto label = preset.value(QStringLiteral("label")).toString();
                const auto values = preset.value(QStringLiteral("value")).toArray();
                if (id.isEmpty() || label.isEmpty() || values.size() != 4) continue;
                double curve[4]{};
                bool valid = true;
                for (int i = 0; i < 4; ++i) {
                    valid = valid && values[i].isDouble() && std::isfinite(values[i].toDouble());
                    curve[i] = values[i].toDouble();
                }
                if (valid) {
                    presets_.addCustomPreset(id.toStdString(), label.toStdString(),
                        {std::clamp(curve[0], 0.0, 1.0), std::clamp(curve[1], -0.5, 1.5),
                         std::clamp(curve[2], 0.0, 1.0), std::clamp(curve[3], -0.5, 1.5)});
                }
            }
        } catch (...) {
            // A blocked or malformed localStorage entry should not stop editing.
        }
#else
        const auto storedPresets = QSettings().value(QStringLiteral("graph-editor-presets")).toList();
        for (const auto& entry : storedPresets) {
            const auto preset = entry.toMap();
            const auto id = preset.value(QStringLiteral("id")).toString();
            const auto label = preset.value(QStringLiteral("label")).toString();
            const auto values = preset.value(QStringLiteral("value")).toList();
            if (id.isEmpty() || label.isEmpty() || values.size() != 4) continue;
            bool valid = true;
            double curve[4]{};
            for (int i = 0; i < 4; ++i) {
                bool numberOk = false;
                curve[i] = values[i].toDouble(&numberOk);
                valid = valid && numberOk && std::isfinite(curve[i]);
            }
            if (valid) {
                presets_.addCustomPreset(id.toStdString(), label.toStdString(),
                    {std::clamp(curve[0], 0.0, 1.0), std::clamp(curve[1], -0.5, 1.5),
                     std::clamp(curve[2], 0.0, 1.0), std::clamp(curve[3], -0.5, 1.5)});
            }
        }
#endif
    }

    void persistCustomPresets() const {
#if defined(__EMSCRIPTEN__)
        try {
            QJsonArray storedPresets;
            for (const auto& preset : presets_.customPresets()) {
                storedPresets.push_back(QJsonObject{
                    {QStringLiteral("id"), QString::fromStdString(preset.id)},
                    {QStringLiteral("label"), QString::fromStdString(preset.label)},
                    {QStringLiteral("value"), QJsonArray{
                        preset.curve.x1, preset.curve.y1, preset.curve.x2, preset.curve.y2
                    }},
                    {QStringLiteral("isCustom"), true}
                });
            }
            const auto json = QString::fromUtf8(QJsonDocument(storedPresets).toJson(QJsonDocument::Compact));
            emscripten::val::global("localStorage").call<void>(
                "setItem", std::string("graph-editor-presets"), json.toStdString());
        } catch (...) {
            // Storage can be unavailable in private browsing; keep the current session usable.
        }
#else
        QVariantList storedPresets;
        for (const auto& preset : presets_.customPresets()) {
            storedPresets.push_back(QVariantMap{
                {QStringLiteral("id"), QString::fromStdString(preset.id)},
                {QStringLiteral("label"), QString::fromStdString(preset.label)},
                {QStringLiteral("value"), QVariantList{
                    preset.curve.x1, preset.curve.y1, preset.curve.x2, preset.curve.y2
                }},
                {QStringLiteral("isCustom"), true}
            });
        }
        QSettings settings;
        settings.setValue(QStringLiteral("graph-editor-presets"), storedPresets);
        settings.sync();
#endif
    }

    void rebuildSavedPresets() {
        while (auto* item = savedGrid_->takeAt(0)) {
            if (auto* widget = item->widget()) widget->deleteLater();
            delete item;
        }
        const auto& saved = presets_.customPresets();
        if (saved.empty()) {
            auto* empty = new QLabel(QStringLiteral("Chưa có mẫu đã lưu"), savedGrid_->parentWidget());
            empty->setAlignment(Qt::AlignCenter);
            empty->setMinimumHeight(52);
            savedGrid_->addWidget(empty, 0, 0, 1, 2);
            return;
        }
        int row = 0;
        for (const auto& preset : saved) {
            auto* apply = new QPushButton(QString::fromStdString(preset.label), savedGrid_->parentWidget());
            apply->setMinimumHeight(28);
            auto* remove = new QPushButton(QStringLiteral("×"), savedGrid_->parentWidget());
            remove->setFixedSize(30, 28);
            remove->setToolTip(QStringLiteral("Xóa mẫu đã lưu"));
            const auto id = preset.id;
            const NormalizedCurve curve{
                preset.curve.x1, preset.curve.y1, preset.curve.x2, preset.curve.y2
            };
            connect(apply, &QPushButton::clicked, this, [this, curve] { setActiveCurve(curve); });
            connect(remove, &QPushButton::clicked, this, [this, id] {
                presets_.removeCustomPreset(id);
                persistCustomPresets();
                rebuildSavedPresets();
            });
            savedGrid_->addWidget(apply, row, 0);
            savedGrid_->addWidget(remove, row, 1);
            ++row;
        }
    }

    std::vector<EditableCurveSegment> segments_;
    editor::GraphEditorEasingPresets presets_;
    QComboBox* segmentPicker_{nullptr};
    BezierCurveWidget* graph_{nullptr};
    QGridLayout* savedGrid_{nullptr};
    int activeIndex_{0};
};

std::vector<EditableCurveSegment> collectEditableCurves(const editor::Clip& clip) {
    std::vector<EditableCurveSegment> segments;
    for (const auto& [path, channel] : clip.animationChannels()) {
        const auto& keys = channel.keyframes();
        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            const auto& key = keys[i];
            if (key.interpolation == editor::KeyframeInterpolation::Hold) continue;
            const NormalizedCurve curve = key.interpolation == editor::KeyframeInterpolation::Linear
                ? NormalizedCurve{0.0, 0.0, 1.0, 1.0}
                : NormalizedCurve{key.bezierX1, key.bezierY1, key.bezierX2, key.bezierY2};
            const auto start = key.time.toSeconds();
            const auto end = keys[i + 1].time.toSeconds();
            segments.push_back({
                QStringLiteral("%1 · %2–%3 s")
                    .arg(QString::fromStdString(path))
                    .arg(start, 0, 'f', 2)
                    .arg(end, 0, 'f', 2),
                path,
                key.time,
                curve,
                curve,
                editor::GraphEditorSessionEngine::getReferenceSpanValue(channel, i)
            });
        }
    }
    return segments;
}

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

    curveBtn_ = makeIconBtn(UiIcon::Chart, "Chỉnh easing cho keyframe");
    connect(curveBtn_, &QPushButton::clicked, this, [this]() {
        auto* timeline = engine_.activeTimeline();
        const auto& selected = engine_.selectedClips();
        if (!timeline || selected.size() != 1) return;
        const auto clipId = selected.front();
        const auto* clip = timeline->findClip(clipId);
        const auto* track = timeline->findTrackContainingClip(clipId);
        if (!clip || !track) return;
        auto segments = collectEditableCurves(*clip);
        if (segments.empty()) return;

        const auto trackId = track->id();
        const auto projectId = engine_.project().metadata().id;
        const auto sceneId = engine_.project().currentSceneId();
        auto applyCurves = [this, clipId, trackId, projectId, sceneId](const std::vector<EditableCurveSegment>& edited) {
            auto* currentTimeline = engine_.activeTimeline();
            if (!currentTimeline || engine_.project().metadata().id != projectId ||
                engine_.project().currentSceneId() != sceneId) {
                return;
            }
            auto* currentClip = currentTimeline->findClip(clipId);
            const auto* currentTrack = currentTimeline->findTrackContainingClip(clipId);
            if (!currentClip || !currentTrack || currentTrack->id() != trackId) return;

            auto batch = std::make_unique<editor::BatchCommand>("Edit easing curves");
            for (const auto& segment : edited) {
                if (curvesEqual(segment.original, segment.curve)) continue;
                const auto* channel = currentClip->findAnimationChannel(segment.propertyPath);
                if (!channel) continue;
                const auto& keys = channel->keyframes();
                const auto key = std::find_if(keys.begin(), keys.end(), [&](const editor::Keyframe& item) {
                    return item.time == segment.keyframeTime;
                });
                if (key == keys.end() || std::next(key) == keys.end()) continue;
                const auto handles = editor::AnimationValueResolvers::getCurveHandlesForNormalizedCubicBezier(
                    *key, *std::next(key), segment.curve, segment.referenceSpanValue);
                if (!handles) continue;
                batch->addCommand(std::make_unique<editor::UpdateKeyframeCurveCommand>(
                    *currentTimeline, trackId, clipId, segment.propertyPath, segment.keyframeTime,
                    editor::KeyframeInterpolation::Bezier, handles->leftHandle, handles->rightHandle,
                    segment.curve.x1, segment.curve.y1, segment.curve.x2, segment.curve.y2));
            }

            if (!batch->empty() && engine_.history().execute(std::move(batch))) {
                engine_.project().setDirty(true);
                engine_.notifyTimelineChanged();
                refresh();
            }
        };

        auto* dialog = new CurveEditorDialog(std::move(segments), window());
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &QDialog::accepted, this, [dialog, applyCurves] {
            applyCurves(dialog->segments());
        });
        dialog->open();
    });

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
    layout->addWidget(curveBtn_);

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
    curveBtn_->setEnabled(clip && !collectEditableCurves(*clip).empty());
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
    if (curveBtn_) curveBtn_->setIcon(UiIcons::get(UiIcon::Chart, pal.textPrimary, 16));
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

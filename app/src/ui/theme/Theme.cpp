#include "Theme.h"

#if defined(HAVE_QT6)
#include <QPalette>
#include <QEvent>
#include <QMenu>
#include <QComboBox>
#include <QListView>
#include <QWidget>
#include <QFile>
#include <QCoreApplication>
#include <QProxyStyle>
#include <QHelpEvent>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QFontDatabase>
#include <QPointer>
#include <QScrollBar>
#include <QTimer>
#include <QStyle>
#include <QAbstractScrollArea>
#include <QWheelEvent>

// Explicitly retain the resource object when catchim_ui is linked statically.
static void initializeUiFonts() {
    Q_INIT_RESOURCE(fonts);
    Q_INIT_RESOURCE(web_assets);
    QFontDatabase::addApplicationFont(":/fonts/Inter.ttf");
}

namespace catchim::ui {

namespace {

class GlobalTooltipWidget : public QWidget {
public:
    explicit GlobalTooltipWidget()
        : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    {
        setObjectName("globalTooltipBubble");
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    void showTip(const QString& text, QWidget* target) {
        text_ = text;
        target_ = target;

        QFont f = font();
        f.setFamily("Inter");
        f.setPixelSize(14);
        f.setWeight(QFont::Medium);
        setFont(f);

        const QFontMetrics fm(f);
        const int cardW = fm.horizontalAdvance(text_) + 20;
        const int cardH = 34;
        const int shadowPad = 6;
        setFixedSize(shadowPad + cardW + shadowPad + 2,
                     shadowPad + cardH + 6 + shadowPad + 2);

        // Keep the tooltip above and centered on the hovered control.
        const QPoint anchor = target_->mapToGlobal(QPoint(target_->width() / 2, 0));
        move(anchor.x() - width() / 2, anchor.y() - height() + 4);
        show();
        raise();
        update();
    }

    void hideTip() {
        target_ = nullptr;
        hide();
    }

    bool belongsTo(QObject* object) const { return target_ == object; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const bool isDark = Theme::instance().mode() == ThemeMode::Dark;
        const QColor bg = isDark ? QColor("#413F3E") : QColor("#FFFFFF");
        const QColor border = isDark ? QColor(255, 255, 255, 25) : QColor(0, 0, 0, 20);
        const QColor textColor = isDark ? QColor("#F4F4F5") : QColor("#09090B");

        const qreal shadowPad = 6.0;
        const qreal cardX = shadowPad;
        const qreal cardY = shadowPad;
        const qreal cardW = width() - cardX - shadowPad;
        const qreal cardH = height() - shadowPad * 2.0 - 6.0;
        const qreal radius = 4.0;
        const qreal centerX = cardX + cardW / 2.0;

        QPainterPath path;
        path.moveTo(cardX, cardY + radius);
        path.lineTo(cardX, cardY + cardH - radius);
        path.arcTo(cardX, cardY + cardH - 2.0 * radius, 2.0 * radius, 2.0 * radius, 180, 90);
        path.lineTo(centerX - 5.0, cardY + cardH);
        path.lineTo(centerX, cardY + cardH + 6.0);
        path.lineTo(centerX + 5.0, cardY + cardH);
        path.lineTo(cardX + cardW - radius, cardY + cardH);
        path.arcTo(cardX + cardW - 2.0 * radius, cardY + cardH - 2.0 * radius,
                   2.0 * radius, 2.0 * radius, 270, 90);
        path.lineTo(cardX + cardW, cardY + radius);
        path.arcTo(cardX + cardW - 2.0 * radius, cardY, 2.0 * radius, 2.0 * radius, 0, 90);
        path.lineTo(cardX + radius, cardY);
        path.arcTo(cardX, cardY, 2.0 * radius, 2.0 * radius, 90, 90);
        path.closeSubpath();

        for (int i = 4; i >= 1; --i) {
            p.fillPath(path.translated(0.0, i * 0.75), QColor(0, 0, 0, (5 - i) * 6));
        }
        p.fillPath(path, bg);
        p.setPen(QPen(border, 1.0));
        p.drawPath(path);
        p.setPen(textColor);
        p.setFont(font());
        p.drawText(QRectF(cardX, cardY, cardW, cardH), Qt::AlignCenter, text_);
    }

private:
    QString text_;
    QPointer<QWidget> target_;
};

class FastToolTipStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;
    int styleHint(StyleHint hint, const QStyleOption* option = nullptr, const QWidget* widget = nullptr, QStyleHintReturn* returnData = nullptr) const override {
        if (hint == SH_ToolTip_WakeUpDelay) {
            return 80; // Instantaneous tooltip display (~80ms) matching web Radix delayDuration
        }
        if (hint == SH_ToolTip_FallAsleepDelay) {
            return 2500;
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

class GlobalPopupFilter : public QObject {
public:
    explicit GlobalPopupFilter(QObject* parent = nullptr) : QObject(parent) {}

    void registerScrollBar(QScrollBar* bar) {
        if (!bar || bar->property("_scrollbarAutoHideInstalled").toBool()) return;
        bar->setProperty("_scrollbarAutoHideInstalled", true);
        bar->setProperty("scrollbarIdle", false);
        auto* timer = new QTimer(bar);
        timer->setObjectName("scrollbarAutoHideTimer");
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, bar, [bar] { setScrollbarIdle(bar, true); });

        const auto activity = [this, bar, timer] { revealScrollBar(bar, timer); };
        QObject::connect(bar, &QScrollBar::sliderPressed, bar, activity);
        QObject::connect(bar, &QScrollBar::sliderMoved, bar, [activity](int) { activity(); });
        QObject::connect(bar, &QScrollBar::sliderReleased, bar, activity);
        QObject::connect(bar, &QScrollBar::actionTriggered, bar, [activity](int) { activity(); });
        setScrollbarIdle(bar, true);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (auto* bar = qobject_cast<QScrollBar*>(watched)) {
            registerScrollBar(bar);
            switch (event->type()) {
            case QEvent::Enter:
            case QEvent::HoverEnter:
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
            case QEvent::Wheel:
                revealScrollBar(bar, bar->findChild<QTimer*>("scrollbarAutoHideTimer"));
                break;
            default:
                break;
            }
        }
        if (event->type() == QEvent::Wheel) {
            auto* wheel = static_cast<QWheelEvent*>(event);
            for (auto* widget = qobject_cast<QWidget*>(watched); widget; widget = widget->parentWidget()) {
                if (auto* area = qobject_cast<QAbstractScrollArea*>(widget)) {
                    QScrollBar* bar = wheel->angleDelta().x() != 0 && wheel->angleDelta().y() == 0
                        ? area->horizontalScrollBar() : area->verticalScrollBar();
                    if (bar && bar->maximum() > bar->minimum()) {
                        registerScrollBar(bar);
                        revealScrollBar(bar, bar->findChild<QTimer*>("scrollbarAutoHideTimer"));
                    }
                    break;
                }
            }
        }
        if (event->type() == QEvent::ToolTip) {
            auto* widget = qobject_cast<QWidget*>(watched);
            if (widget && !widget->toolTip().isEmpty()) {
                if (!tooltip_) tooltip_ = new GlobalTooltipWidget();
                tooltip_->showTip(widget->toolTip(), widget);
                event->accept();
                return true;
            }
        }
        if (tooltip_ && tooltip_->belongsTo(watched) &&
            (event->type() == QEvent::Leave || event->type() == QEvent::Hide ||
             event->type() == QEvent::MouseButtonPress || event->type() == QEvent::Destroy)) {
            tooltip_->hideTip();
        }
        if (event->type() == QEvent::ApplicationDeactivate && tooltip_) tooltip_->hideTip();
        if (event->type() == QEvent::Polish) {
            if (auto* bar = qobject_cast<QScrollBar*>(watched)) {
                registerScrollBar(bar);
            } else if (auto* menu = qobject_cast<QMenu*>(watched)) {
                menu->setAttribute(Qt::WA_TranslucentBackground, true);
                menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
            } else if (auto* combo = qobject_cast<QComboBox*>(watched)) {
                if (!combo->property("_styled_view").toBool()) {
                    combo->setProperty("_styled_view", true);
                    auto* view = new QListView(combo);
                    combo->setView(view);
                }
            } else if (auto* w = qobject_cast<QWidget*>(watched)) {
                // WindowType is an enum, not a bitset: Dialog (3) overlaps Popup (9).
                // Testing flags made ordinary dialogs transparent (black on Windows).
                if (w->isWindow() && w->windowType() == Qt::Popup) {
                    w->setAttribute(Qt::WA_TranslucentBackground, true);
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    static void setScrollbarIdle(QScrollBar* bar, bool idle) {
        if (!bar || bar->property("scrollbarIdle").toBool() == idle) return;
        bar->setProperty("scrollbarIdle", idle);
        if (auto* style = bar->style()) {
            style->unpolish(bar);
            style->polish(bar);
        }
        bar->update();
    }

    static void revealScrollBar(QScrollBar* bar, QTimer* timer) {
        if (!bar || !timer) return;
        setScrollbarIdle(bar, false);
        timer->start(3000);
    }

    QPointer<GlobalTooltipWidget> tooltip_;
};

} // namespace

Theme& Theme::instance() {
    static Theme s_instance;
    return s_instance;
}

Theme::Theme()
    : palette_(getDarkPalette())
{
}

void Theme::setTheme(ThemeMode mode) {
    mode_ = mode;
    palette_ = (mode == ThemeMode::Dark) ? getDarkPalette() : getLightPalette();
    if (qApp) {
        static const bool fontsLoaded = [] { initializeUiFonts(); return true; }();
        Q_UNUSED(fontsLoaded);
        QFont uiFont("Inter");
        uiFont.setPixelSize(13);
        qApp->setFont(uiFont);
        static bool s_tooltipStyleInstalled = false;
        if (!s_tooltipStyleInstalled) {
            qApp->setStyle(new FastToolTipStyle(qApp->style()));
            s_tooltipStyleInstalled = true;
        }

        static GlobalPopupFilter* s_popupFilter = nullptr;
        if (!s_popupFilter) {
            s_popupFilter = new GlobalPopupFilter(qApp);
            qApp->installEventFilter(s_popupFilter);
            for (QWidget* widget : QApplication::allWidgets())
                if (auto* bar = qobject_cast<QScrollBar*>(widget)) s_popupFilter->registerScrollBar(bar);
        }

        QPalette pal;
        pal.setColor(QPalette::Window, palette_.background);
        pal.setColor(QPalette::WindowText, palette_.textPrimary);
        pal.setColor(QPalette::Base, palette_.panelBackground);
        pal.setColor(QPalette::AlternateBase, palette_.secondary);
        pal.setColor(QPalette::ToolTipBase, palette_.popoverBackground);
        pal.setColor(QPalette::ToolTipText, palette_.popoverForeground);
        pal.setColor(QPalette::Text, palette_.textPrimary);
        pal.setColor(QPalette::Button, palette_.secondary);
        pal.setColor(QPalette::ButtonText, palette_.textPrimary);
        pal.setColor(QPalette::BrightText, palette_.destructive);
        pal.setColor(QPalette::Link, palette_.primaryAccent);
        pal.setColor(QPalette::Highlight, palette_.primaryAccent);
        pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        qApp->setPalette(pal);
        qApp->setStyleSheet(buildGlobalStyleSheet());
    }
    for (const auto& [k, cb] : listeners_) {
        if (cb) cb(mode_);
    }
}

void Theme::addListener(const std::string& key, ThemeListener listener) {
    listeners_[key] = std::move(listener);
}

void Theme::removeListener(const std::string& key) {
    listeners_.erase(key);
}

QFont Theme::fontSans(int pointSize, int weight) const {
    QFont f("Inter", pointSize, weight);
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}

QFont Theme::fontMono(int pointSize) const {
    QFont f("Cascadia Code", pointSize);
    f.setStyleHint(QFont::Monospace);
    return f;
}

QString Theme::buildGlobalStyleSheet() const {
    bool isDark = (mode_ == ThemeMode::Dark);
    const QString chevronIcon = isDark ? ":/icons/chevron-down-dark.svg" : ":/icons/chevron-down.svg";
    const QString chevronUpIcon = isDark ? ":/icons/chevron-up-dark.svg" : ":/icons/chevron-up.svg";

    QString qss = (QString(R"(
        /* Global Window & Central Widget */
        QMainWindow, QWidget#centralWidget {
            background-color: %1;
            color: %2;
            font-family: 'Inter';
            font-size: 13px;
        }

        QWidget {
            font-family: 'Inter';
            font-size: 13px;
            color: %2;
            outline: none;
        }

        QLabel {
            background-color: transparent;
            color: %2;
        }

        QDialog { background-color: %1; color: %2; }
        QWidget[class="webSectionRow"] { border: none; border-bottom: 1px solid %4; }
        QLabel[class="sectionCaption"] { color: %5; font-weight: 500; }
        QPushButton[class="underlineTab"] {
            background: transparent; color: %5; border: 1px solid transparent;
            border-radius: 10px; padding: 0px 10px; font-weight: 500;
        }
        QPushButton[class="browseTab"] { background:transparent; color:%5; border:none; border-bottom:2px solid transparent; border-radius:0; padding:0 12px; }
        QPushButton[class="browseTab"]:checked { color:%8; border-bottom-color:%8; background:transparent; }
        QPushButton[class="underlineTab"]:checked { color: %8; border-color: rgba(23,170,243,30); background: rgba(23,170,243,18); }
        QPushButton[class="sectionToggle"] { background: transparent; color: %2; text-align: left; border: none; padding: 0 6px; }
        QPushButton[class="sectionToggle"]:checked { background: transparent; color: %2; border:none; }
        QPushButton[class="ratioPreset"] { background: transparent; color: %5; text-align:left; border:1px solid transparent; padding:0 8px; }
        QPushButton[class="ratioPreset"]:hover { background:%7; }
        QPushButton[class="ratioPreset"]:checked { background:rgba(23,170,243,18); color:%8; border-color:rgba(23,170,243,30); border-radius:10px; }
        QComboBox[class="inlineSelect"] { background:transparent; border:1px solid transparent; padding:0px 22px 0px 4px; min-height:26px; }

        QLabel[class="SecondaryLabel"] {
            color: %5;
            font-size: 11px;
        }

        QLabel[class="SectionTitle"] {
            font-weight: 600;
            font-size: 14px;
            color: %2;
        }

        /* Panels & Frames */
        QWidget#editorBody {
            background-color: %1;
        }

        .Panel, QWidget[class="Panel"], QFrame#panelFrame, QWidget#assetsPanel, QWidget#previewPanel, QWidget#propertiesPanel, QWidget#timelinePanel {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 6px;
        }

        /* Editor Header */
        #editorHeader {
            background-color: %1;
            border-bottom: 1px solid transparent;
        }
        #editorHeader QPushButton {
            background-color: transparent;
            color: %2;
            border: none;
            border-radius: 4px;
            padding: 0px;
        }
        #editorHeader QPushButton:hover {
            background-color: %7;
        }
        #editorHeader QLineEdit {
            background: transparent;
            color: %2;
            font-weight: 500;
            font-size: 13px;
            padding: 2px 8px;
            border-radius: 4px;
            border: 1px solid transparent;
        }
        #editorHeader QLineEdit:hover {
            background: %7;
            border: 1px solid transparent;
        }
        #editorHeader QLineEdit:focus {
            background: transparent;
            border: 1px solid %8;
        }

        /* Left TabList in AssetsPanel */
        #tabList {
            background-color: %3;
            border: none;
            border-right: 1px solid %4;
            padding: 6px 0px;
            outline: none;
        }
        #tabList::item {
            height: 32px;
            width: 32px;
            border-radius: 6px;
            margin: 2px 6px;
            border: none;
            outline: none;
        }
        #tabList::item:hover {
            background-color: %7;
            border: none;
            outline: none;
        }
        #tabList::item:selected {
            background-color: rgba(56, 189, 248, 0.15);
            border: none;
            outline: none;
        }
        #tabList::item:focus {
            border: none;
            outline: none;
        }

        /* Toolbars */
        #previewToolbar {
            background-color: %3;
            border: none;
        }
        #timelineToolbar {
            background-color: %3;
            border-bottom: 1px solid %4;
        }
        #previewToolbar QPushButton, #timelineToolbar QPushButton {
            background-color: transparent;
            color: %2;
            border: 1px solid transparent;
            border-radius: 5px;
            padding: 0px;
        }
        #previewToolbar QPushButton:hover, #timelineToolbar QPushButton:hover {
            background-color: %7;
            border-color: transparent;
        }
        #previewToolbar QPushButton:checked, #timelineToolbar QPushButton:checked {
            background-color: rgba(56, 189, 248, 0.18);
            color: %8;
            border-color: rgba(56, 189, 248, 0.35);
        }

        #previewPlayBtn {
            background-color: transparent;
            border: none;
            border-radius: 5px;
        }
        #previewPlayBtn:hover {
            background-color: %7;
        }

        #sceneSplitWidget {
            background-color: %7;
            border: 1px solid %4;
            border-radius: 13px;
        }
        #sceneSplitWidget:hover {
            border-color: %7;
        }
        #sceneLabelBtn {
            color: %2;
            font-weight: 500;
            font-size: 12px;
            padding: 3px 8px;
            background: transparent;
            border: none;
        }
        #sceneLayersBtn {
            background: transparent;
            border: none;
            border-left: 1px solid %4;
            padding: 2px 6px;
            border-top-right-radius: 5px;
            border-bottom-right-radius: 5px;
        }
        #sceneLayersBtn:hover, #sceneLabelBtn:hover {
            background-color: %7;
        }

        #timecodeLabel {
            font-family: 'Consolas';
            color: %5;
            font-size: 11px;
            font-weight: 500;
            padding: 0px;
            background-color: transparent;
            border: none;
        }

    )") + QString(R"(
        /* Card Widgets */
        QWidget[class="cardWidget"] {
            background-color: %6;
            border: 1px solid %4;
            border-radius: 6px;
        }

        /* Import Button / Drop Zone */
        #mediaDropCard {
            background-color: %6;
            border: none;
            border-radius: 8px;
        }
        #mediaDropCard:hover {
            background-color: %7;
        }

        #importBtn, #importDropZone {
            background-color: %6;
            color: %2;
            border: 2px dashed %4;
            border-radius: 8px;
            font-weight: 600;
            font-size: 12px;
            padding: 8px 16px;
        }
        #importBtn:hover, #importDropZone:hover {
            background-color: %7;
            border-color: %8;
            color: %8;
        }

        #previewZoomCombo {
            min-height: 22px;
            max-height: 22px;
            background-color: %7;
            color: %10;
            border: 1px solid %11;
            border-radius: 6px;
            padding: 2px 4px 2px 10px;
            font-size: 12px;
        }
        #previewZoomCombo:hover {
            border-color: %5;
            background-color: %12;
        }
        #previewZoomCombo::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 18px;
            border: none;
            background: transparent;
        }
        #previewZoomCombo::down-arrow {
            image: url("%14");
            width: 11px;
            height: 11px;
            margin-right: 4px;
        }

        /* Media List Widget */
        #mediaListWidget {
            background: transparent;
            border: none;
            border-radius: 0;
            padding: 4px;
        }
        #mediaListWidget::item {
            height: 72px;
            padding: 4px;
            border: 1px solid transparent;
            border-radius: 6px;
            color: %2;
        }
        #mediaListWidget::item:hover {
            background-color: %7;
        }
        #mediaListWidget::item:selected {
            background-color: %7;
            border-color: %8;
            color: %2;
        }

        /* QSplitter */
        QSplitter {
            background-color: %1;
        }
        QSplitter::handle {
            background-color: transparent;
        }
        QSplitter::handle:hover {
            background-color: transparent;
        }

        /* Scroll Areas */
        QScrollArea {
            background-color: transparent;
            border: none;
        }
        QScrollArea > QWidget > QWidget {
            background-color: transparent;
        }
        #timelineScrollArea {
            border: none;
            background-color: %1;
        }

        /* Buttons */
        QPushButton {
            background-color: %6;
            color: %2;
            border: 1px solid %4;
            border-radius: 6px;
            padding: 6px 12px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: %7;
            border-color: %8;
        }
        QPushButton:pressed {
            background-color: %6;
        }
        QPushButton:checked {
            background-color: %8;
            color: #ffffff;
            border-color: %8;
        }

        /* LineEdits, TextEdits, SpinBoxes */
        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox {
            background-color: %6;
            color: %2;
            border: 1px solid %4;
            border-radius: 6px;
            padding: 4px 8px;
            selection-background-color: %8;
            selection-color: #ffffff;
        }
        QLineEdit:hover, QTextEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover {
            border-color: %5;
        }
        QLineEdit:focus, QTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: %8;
            background-color: %6;
        }

        /* SpinBox up/down buttons */
        QSpinBox::up-button, QDoubleSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::down-button {
            background: transparent;
            border: none;
            width: 16px;
        }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
            image: url("%15");
            width: 9px;
            height: 9px;
        }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
            image: url("%14");
            width: 9px;
            height: 9px;
        }

        /* ComboBox */
        QComboBox {
            background-color: %9;
            color: %10;
            border: 1px solid %11;
            border-radius: 6px;
            padding: 3px 24px 3px 10px;
            min-height: 26px;
            font-size: 13px;
        }
        QComboBox:hover {
            border-color: %5;
            background-color: %12;
        }
        QComboBox:focus {
            border-color: %8;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 22px;
            border-left-width: 0px;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
            background: transparent;
        }
        QComboBox::down-arrow {
            image: url("%14");
            width: 12px;
            height: 12px;
            margin-right: 6px;
        }
        QComboBox QAbstractItemView {
            background-color: %9;
            color: %10;
            border: 1px solid %11;
            border-radius: 6px;
            padding: 4px;
            outline: none;
            selection-background-color: %12;
            selection-color: %10;
        }
        QComboBox QAbstractItemView::item {
            min-height: 24px;
            padding: 4px 8px;
            border-radius: 4px;
            margin: 1px 2px;
            color: %10;
        }
        QComboBox QAbstractItemView::item:hover,
        QComboBox QAbstractItemView::item:selected {
            background-color: %12;
            color: %10;
        }

        /* GroupBox */
        QGroupBox[class="inspectorSection"] {
            border:none; border-bottom:1px solid %4; border-radius:0;
            margin-top:0px; padding:32px 0px 0px; font-size:13px; font-weight:500;
        }
        QGroupBox[class="inspectorSection"]::title {
            subcontrol-origin:padding; subcontrol-position:top left;
            left:14px; top:12px; padding:0; background:transparent; color:%2;
        }
        QGroupBox {
            background-color: %3;
            color: %2;
            font-weight: 600;
            border: 1px solid %4;
            border-radius: 8px;
            margin-top: 18px;
            padding-top: 14px;
            padding-left: 10px;
            padding-right: 10px;
            padding-bottom: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 6px;
            background-color: %3;
            color: %2;
        }

        /* Tab Widgets & TabBar */
        QTabWidget::pane {
            border: 1px solid %4;
            background-color: %3;
            border-radius: 8px;
        }
        QTabBar::tab {
            background-color: %6;
            color: %5;
            padding: 8px 16px;
            margin-right: 2px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
        }
        QTabBar::tab:hover {
            background-color: %7;
            color: %2;
        }
        QTabBar::tab:selected {
            background-color: %3;
            color: %8;
            border-bottom: 2px solid %8;
            font-weight: 600;
        }

        /* List Widgets */
        QListWidget {
            background-color: transparent;
            border: none;
            outline: none;
        }
        QListWidget::item {
            border-radius: 6px;
            padding: 6px 10px;
            color: %2;
        }
        QListWidget::item:hover {
            background-color: %7;
        }
        QListWidget::item:selected {
            background-color: %8;
            color: #ffffff;
        }

        /* Sliders */
        QSlider::groove:horizontal {
            height: 4px;
            background: %4;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: %8;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #ffffff;
            border: 2px solid #38bdf8;
            width: 12px;
            height: 12px;
            margin-top: -4px;
            margin-bottom: -4px;
            border-radius: 6px;
        }
        QSlider::handle:horizontal:hover {
            border-color: %8;
            background: #ffffff;
        }

        /* Scrollbars */
        QScrollBar:vertical {
            background: transparent;
            border: none;
            width: 8px;
            margin: 2px 0px;
        }
        QScrollBar::handle:vertical {
            background: %4;
            min-height: 24px;
            border-radius: 1px;
            margin: 0px 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: %5;
        }
        QScrollBar::horizontal {
            background: transparent;
            border: none;
            height: 8px;
            margin: 0px 2px;
        }
        QScrollBar::handle:horizontal {
            background: %4;
            min-width: 24px;
            border-radius: 1px;
            margin: 3px 0px;
        }
        QScrollBar::handle:horizontal:hover {
            background: %5;
        }
        QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page {
            background: none;
            border: none;
            width: 0px;
            height: 0px;
        }
        QScrollBar::groove:vertical, QScrollBar::groove:horizontal {
            background: transparent;
            border: none;
        }
        /* Compact dropdown scrollbar: 2px thumb with a 2px inset on every side. */
        QScrollBar#webSelectScrollBar:vertical {
            background: transparent;
            border: none;
            width: 8px;
            margin: 2px 0px;
        }
        QScrollBar#webSelectScrollBar::handle:vertical {
            background: %4;
            border: none;
            min-height: 18px;
            border-radius: 1px;
            margin: 0px 3px;
        }
        QScrollBar#webSelectScrollBar::handle:vertical:hover {
            background: %5;
        }
        QScrollBar[scrollbarIdle="true"]::handle:vertical,
        QScrollBar[scrollbarIdle="true"]::handle:horizontal,
        QScrollBar#webSelectScrollBar[scrollbarIdle="true"]::handle:vertical {
            background: transparent;
            border-color: transparent;
        }
        QAbstractScrollArea::corner {
            background: transparent;
            border: none;
        }

        /* Tooltips */
        QToolTip {
            background-color: %9;
            color: %10;
            border: 1px solid %11;
            border-radius: 6px;
            padding: 5px 10px;
            font-family: 'Inter', -apple-system, sans-serif;
            font-size: 12px;
            font-weight: 500;
        }

        /* Menu */
        QMenu {
            background-color: %9;
            color: %10;
            border: 1px solid %11;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            background-color: transparent;
            color: %10;
            padding: 6px 14px;
            min-height: 22px;
            border-radius: 4px;
            margin: 1px 2px;
            font-size: 13px;
        }
        QMenu::item:selected {
            background-color: %12;
            color: %10;
        }
        QMenu::item:disabled {
            color: %5;
            background-color: transparent;
        }
        QMenu::separator {
            height: 1px;
            background-color: %13;
            margin: 4px 6px;
        }
        QMenu::icon {
            padding-left: 4px;
            margin-right: 6px;
        }
    )"))
    .arg(palette_.background.name())          // %1
    .arg(palette_.textPrimary.name())         // %2
    .arg(palette_.panelBackground.name())     // %3
    .arg(palette_.panelBorder.name())         // %4
    .arg(palette_.textSecondary.name())       // %5
    .arg(palette_.secondary.name())           // %6
    .arg(palette_.hover.name())               // %7
    .arg(palette_.primaryAccent.name())       // %8
    .arg(palette_.popoverBackground.name())   // %9
    .arg(palette_.popoverForeground.name())   // %10
    .arg(palette_.popoverBorder.name())       // %11
    .arg(palette_.popoverHover.name())        // %12
    .arg(palette_.popoverSeparator.name())    // %13
    .arg(chevronIcon)                         // %14
    .arg(chevronUpIcon);                      // %15

    return qss;
}

} // namespace catchim::ui
#endif

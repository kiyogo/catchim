#pragma once
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include "ui/theme/Theme.h"
#include "EditorLanguage.h"

namespace catchim::ui {
class SidebarTooltipWidget : public QWidget {
public:
    explicit SidebarTooltipWidget(QWidget* parent = nullptr)
        : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    // `onLeft` places the bubble to the left of its anchor and flips the
    // pointer to the right. The default preserves the left sidebar behavior.
    void showTip(const QString& text, const QPoint& targetCenter, bool onLeft = false) {
        onLeft_ = onLeft;
        text_ = translatedEditorText(text);
        QFont f = font();
        f.setFamily("Inter");
        // Match the sidebar tooltip from the reference UI: text-sm (14px)
        // with a medium weight and a compact, single-line bubble.
        f.setPixelSize(14);
        f.setWeight(QFont::Medium);
        setFont(f);

        QFontMetrics fm(f);
        int textW = fm.horizontalAdvance(text_);

        // Exact web dimensions matching web/src/components/ui/tooltip.tsx:
        // p-2.5 + leading-none + text-sm = 34px tall.
        int cardW = textW + 20;
        int cardH = 34;

        arrowW_ = 6.0;
        shadowPad_ = 6.0;

        int totalW = static_cast<int>(shadowPad_ + arrowW_ + cardW + shadowPad_ + 2);
        int totalH = static_cast<int>(shadowPad_ + cardH + shadowPad_ + 2);
        setFixedSize(totalW, totalH);

        // Position from the anchor so the pointer meets the button edge;
        // keep the full bubble visible instead of letting it sit under the
        // neighboring preview/inspector divider.
        int x = onLeft
            ? targetCenter.x() - totalW + static_cast<int>(shadowPad_) + 1
            : targetCenter.x() + 1 - static_cast<int>(shadowPad_);
        int y = targetCenter.y() - totalH / 2;
        move(x, y);
        show();
        raise();
        update();
    }

    void hideTip() {
        hide();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        bool isDark = Theme::instance().mode() == ThemeMode::Dark;
        QColor bg = isDark ? QColor("#413F3E") : QColor("#FFFFFF");
        QColor border = isDark ? QColor(255, 255, 255, 25) : QColor(0, 0, 0, 20);
        QColor textCol = isDark ? QColor("#F4F4F5") : QColor("#09090B");

        qreal cardX = shadowPad_ + (onLeft_ ? 0.0 : arrowW_);
        qreal cardY = shadowPad_;
        qreal cardW = width() - cardX - shadowPad_ - (onLeft_ ? arrowW_ : 0.0);
        qreal cardH = height() - shadowPad_ * 2.0;
        qreal r = 4.0;
        qreal cy = cardY + cardH / 2.0;
        qreal arrowH = 5.0; // 10px total height, matching web SVG viewBox 6x10

        QPainterPath path;
        if (onLeft_) {
            // Mirror the reference tooltip: rounded card on the left, arrow
            // on its right edge pointing at the hovered control.
            path.moveTo(cardX + r, cardY);
            path.lineTo(cardX + cardW - r, cardY);
            path.arcTo(cardX + cardW - 2.0 * r, cardY, 2.0 * r, 2.0 * r, 90, -90);
            path.lineTo(cardX + cardW, cy - arrowH);
            path.lineTo(cardX + cardW + arrowW_, cy);
            path.lineTo(cardX + cardW, cy + arrowH);
            path.lineTo(cardX + cardW, cardY + cardH - r);
            path.arcTo(cardX + cardW - 2.0 * r, cardY + cardH - 2.0 * r, 2.0 * r, 2.0 * r, 0, -90);
            path.lineTo(cardX + r, cardY + cardH);
            path.arcTo(cardX, cardY + cardH - 2.0 * r, 2.0 * r, 2.0 * r, 270, -90);
            path.lineTo(cardX, cardY + r);
            path.arcTo(cardX, cardY, 2.0 * r, 2.0 * r, 180, -90);
        } else {
            path.moveTo(cardX, cardY + r);
            path.lineTo(cardX, cy - arrowH);
            path.lineTo(shadowPad_, cy); // Arrow tip pointing left
            path.lineTo(cardX, cy + arrowH);
        path.lineTo(cardX, cardY + cardH - r);
        path.arcTo(cardX, cardY + cardH - 2.0 * r, 2.0 * r, 2.0 * r, 180, 90);
        path.lineTo(cardX + cardW - r, cardY + cardH);
        path.arcTo(cardX + cardW - 2.0 * r, cardY + cardH - 2.0 * r, 2.0 * r, 2.0 * r, 270, 90);
        path.lineTo(cardX + cardW, cardY + r);
        path.arcTo(cardX + cardW - 2.0 * r, cardY, 2.0 * r, 2.0 * r, 0, 90);
        path.lineTo(cardX + r, cardY);
        path.arcTo(cardX, cardY, 2.0 * r, 2.0 * r, 90, 90);
        }
        path.closeSubpath();

        // Soft drop shadow (ambient + key shadow matching Radix shadow-md)
        for (int i = 4; i >= 1; --i) {
            QColor shadowCol(0, 0, 0, (5 - i) * 6);
            p.fillPath(path.translated(0.0, i * 0.75), shadowCol);
        }

        // Fill bubble background
        p.fillPath(path, bg);

        // Draw outline border
        p.setPen(QPen(border, 1.0));
        p.drawPath(path);

        // Text centered in card
        p.setPen(textCol);
        p.setFont(font());
        QRectF textRect(cardX, cardY, cardW, cardH);
        p.drawText(textRect, Qt::AlignCenter, text_);
    }

private:
    QString text_;
    qreal arrowW_{6.0};
    qreal shadowPad_{6.0};
    bool onLeft_{false};
};
}


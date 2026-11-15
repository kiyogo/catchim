#pragma once

#if defined(HAVE_QT6)
#include <QAbstractScrollArea>
#include <QEvent>
#include <QLayout>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTimer>

namespace catchim::ui {

inline QMargins balancedViewportMargins(
    const QMargins& baseMargins,
    const QAbstractScrollArea* area) {
    if (!area || !area->viewport()) return baseMargins;

    const QRect areaRect = area->contentsRect();
    const QRect viewportRect = area->viewport()->geometry();
    const int leftGutter = qMax(0, viewportRect.left() - areaRect.left());
    const int rightGutter = qMax(0, areaRect.right() - viewportRect.right());
    const int equalOuterMargin = qMax(
        baseMargins.left() + leftGutter,
        baseMargins.right() + rightGutter);

    return QMargins(
        qMax(0, equalOuterMargin - leftGutter),
        baseMargins.top(),
        qMax(0, equalOuterMargin - rightGutter),
        baseMargins.bottom());
}

class BalancedScrollMarginsFilter final : public QObject {
public:
    BalancedScrollMarginsFilter(
        QAbstractScrollArea* area,
        QLayout* layout,
        const QMargins& baseMargins)
        : QObject(area), area_(area), layout_(layout), baseMargins_(baseMargins) {
        if (!area_) return;
        setObjectName(QStringLiteral("balancedScrollMarginsFilter"));

        if (auto* scroll = qobject_cast<QScrollArea*>(area_)) {
            if (auto* content = scroll->widget()) {
                auto contentPolicy = content->sizePolicy();
                contentPolicy.setHorizontalPolicy(QSizePolicy::Ignored);
                content->setSizePolicy(contentPolicy);
                content->setMinimumWidth(0);
            }
        }

        area_->installEventFilter(this);
        area_->viewport()->installEventFilter(this);
        area_->verticalScrollBar()->installEventFilter(this);
        QObject::connect(
            area_->verticalScrollBar(),
            &QScrollBar::rangeChanged,
            this,
            [this](int, int) { scheduleUpdate(); });
        scheduleUpdate();
    }

    void setLayout(QLayout* layout, const QMargins& baseMargins) {
        // Keep the original margins when the same layout is registered again.
        // Dynamic views can replace their scroll contents; in that case the
        // new layout becomes the sole target and starts from its own margins.
        if (layout_ != layout) {
            layout_ = layout;
            baseMargins_ = baseMargins;
        }
        scheduleUpdate();
    }

protected:
    bool eventFilter(QObject*, QEvent* event) override {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::ShowToParent:
        case QEvent::HideToParent:
        case QEvent::LayoutRequest:
        case QEvent::StyleChange:
        case QEvent::ContentsRectChange:
            scheduleUpdate();
            break;
        default:
            break;
        }
        return false;
    }

private:
    void scheduleUpdate() {
        if (updatePending_ || !area_) return;
        updatePending_ = true;
        QTimer::singleShot(0, this, [this] {
            updatePending_ = false;
            if (!area_ || !layout_) return;
            const QMargins margins = balancedViewportMargins(baseMargins_, area_);
            if (layout_->contentsMargins() != margins) {
                layout_->setContentsMargins(margins);
            }
        });
    }

    QPointer<QAbstractScrollArea> area_;
    QPointer<QLayout> layout_;
    QMargins baseMargins_;
    bool updatePending_{false};
};

inline void balanceViewportLayoutMargins(
    QAbstractScrollArea* area,
    QLayout* layout,
    const QMargins& baseMargins) {
    if (!area || !layout) return;
    if (auto* filter = area->findChild<QObject*>(QStringLiteral("balancedScrollMarginsFilter"))) {
        auto* marginsFilter = static_cast<BalancedScrollMarginsFilter*>(filter);
        marginsFilter->setLayout(layout, baseMargins);
        return;
    }
    new BalancedScrollMarginsFilter(area, layout, baseMargins);
}

inline void balanceScrollContentMargins(QScrollArea* scroll) {
    if (!scroll || !scroll->widget() || !scroll->widget()->layout()) return;
    auto* contentLayout = scroll->widget()->layout();
    balanceViewportLayoutMargins(scroll, contentLayout, contentLayout->contentsMargins());
}

} // namespace catchim::ui
#endif

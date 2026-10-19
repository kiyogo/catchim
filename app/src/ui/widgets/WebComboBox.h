#pragma once

#include <QComboBox>
#include <QListView>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QVBoxLayout>
#include <QStyledItemDelegate>
#include "ui/theme/Theme.h"

namespace catchim::ui {

// Native counterpart of ui/select.tsx: popper placement, rounded surface,
// leading selection check, 28px rows, scrolling and keyboard navigation.
class SelectPopup : public QWidget {
public:
    explicit SelectPopup(QWidget* parent) : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint) {
        setObjectName("webSelectPopup");
        setAttribute(Qt::WA_TranslucentBackground);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        const auto& p = Theme::instance().palette();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF card(6, 4, width()-12, height()-12);
        for (int i=5; i>0; --i) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0,0,0,5));
            painter.drawRoundedRect(card.adjusted(-i/2., i/2., i/2., i), 11, 11);
        }
        painter.setPen(QPen(p.popoverBorder, 1));
        painter.setBrush(p.popoverBackground);
        painter.drawRoundedRect(card, 10, 10);
    }
};

class SelectItemDelegate : public QStyledItemDelegate {
public:
    explicit SelectItemDelegate(QComboBox* owner) : QStyledItemDelegate(owner), owner_(owner) {}
    // QListView::spacing adds padding on BOTH sides of every row. Keep it
    // zero and give each row one explicit 2px gap, with none after the last.
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const override {
        const bool last = index.row()+1 == index.model()->rowCount(index.parent());
        return QSize(1,28+(last ? 0 : 2));
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& opt, const QModelIndex& index) const override {
        const auto& p = Theme::instance().palette();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        QRect itemRect = opt.rect;
        itemRect.setHeight(std::min(28,itemRect.height()));
        QRectF r = itemRect;
        if (opt.state & (QStyle::State_Selected | QStyle::State_MouseOver)) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(p.popoverHover);
            painter->drawRoundedRect(r,5,5);
        }
        painter->setPen(p.popoverForeground);
        QFont font("Inter"); font.setPixelSize(13); painter->setFont(font);
        painter->drawText(itemRect.adjusted(24,0,-8,0), Qt::AlignVCenter | Qt::AlignLeft,
            painter->fontMetrics().elidedText(index.data().toString(), Qt::ElideRight, itemRect.width()-32));
        if (index.row() == owner_->currentIndex()) {
            QPainterPath check; const QPointF c(itemRect.left()+12,itemRect.center().y());
            check.moveTo(c+QPointF(-4,0)); check.lineTo(c+QPointF(-1,3)); check.lineTo(c+QPointF(5,-3));
            painter->setPen(QPen(p.popoverForeground,1.4,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            painter->drawPath(check);
        }
        painter->restore();
    }
private:
    QComboBox* owner_;
};

class WebComboBox : public QComboBox {
public:
    explicit WebComboBox(QWidget* parent=nullptr) : QComboBox(parent) {
        setProperty("_styled_view",true);
        setStyleSheet("QComboBox{background:transparent;border:none;padding:0;min-height:0;min-width:0;}");
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        setMinimumContentsLength(1);
        popup_ = new SelectPopup(this);
        auto* layout = new QVBoxLayout(popup_);
        // Shadow margins (6,4,6,8), a 1px outline, then 3px of clear padding.
        // Measure the padding from inside the outline, not its centerline.
        layout->setContentsMargins(10,8,10,12);
        list_ = new QListView(popup_);
        list_->setObjectName("webSelectList");
        list_->verticalScrollBar()->setObjectName("webSelectScrollBar");
        list_->setItemDelegate(new SelectItemDelegate(this));
        list_->setMouseTracking(true);
        list_->setUniformItemSizes(false);list_->setSpacing(0);list_->setMinimumSize(0,0);
        list_->setWordWrap(false);
        // Per-item scrolling can leave the final row at the top of the
        // viewport when the scrollbar reaches its maximum, exposing a blank
        // strip below it. Pixel scrolling aligns the last row with the bottom.
        list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        list_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);
        list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_->setStyleSheet("QListView#webSelectList {background:transparent;border:none;padding:0;outline:none;} QListView#webSelectList::item {padding:0;margin:0;border:none;}");
        list_->installEventFilter(this);
        layout->addWidget(list_);
        connect(list_, &QListView::clicked, this, [this](const QModelIndex& index){ choose(index.row()); });
    }
    void showPopup() override {
        if (!count() || !isEnabled()) return;
        list_->setModel(model());
        list_->setModelColumn(modelColumn());
        list_->setRootIndex(rootModelIndex());
        list_->setCurrentIndex(model()->index(currentIndex(),modelColumn(),rootModelIndex()));
        int contentWidth = std::max(width(),128);
        for (int i=0;i<count();++i) contentWidth=std::max(contentWidth,fontMetrics().horizontalAdvance(itemText(i))+40);
        QRect available = screen()->availableGeometry();
        const QRect appBounds(window()->mapToGlobal(QPoint(0,0)),window()->size());
        available=available.intersected(appBounds);
        const QPoint anchorTop=mapToGlobal(QPoint(0,0));
        const QPoint anchorBottom=mapToGlobal(QPoint(0,height()));
        // Expand downward to the app's lower edge before enabling scrolling.
        const int screenTop=available.top()+2;
        const int screenBottomExclusive=available.top()+available.height()-2;
        const int aboveSpace=std::max(0,anchorTop.y()-screenTop);
        const int belowSpace=std::max(0,screenBottomExclusive-anchorBottom.y());
        const int rowCount = list_->model()->rowCount(list_->rootIndex());
        list_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_->verticalScrollBar()->setValue(0);
        list_->doItemsLayout();
        // Measure the view's actual laid-out extent, including its row gaps.
        const QRect firstRow=list_->visualRect(model()->index(0,modelColumn(),rootModelIndex()));
        const QRect lastRow=list_->visualRect(model()->index(rowCount-1,modelColumn(),rootModelIndex()));
        const int contentHeight=lastRow.bottom()+1-firstRow.top();
        const QMargins popupMargins=popup_->layout()->contentsMargins();
        const int verticalPopupMargins=popupMargins.top()+popupMargins.bottom();
        const int desiredHeight=contentHeight+verticalPopupMargins;
        // Only flip above when not even one option fits below the control.
        const bool openAbove=belowSpace<verticalPopupMargins+28 && aboveSpace>belowSpace;
        const int availablePopupSpace=openAbove ? aboveSpace : belowSpace;
        const bool fitsWithoutScrolling=desiredHeight<=availablePopupSpace;
        list_->setVerticalScrollBarPolicy(fitsWithoutScrolling
            ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
        int popupHeight=std::max(1+verticalPopupMargins,
                                 std::min(desiredHeight,availablePopupSpace));
        popup_->resize(std::min(contentWidth+popupMargins.left()+popupMargins.right(),available.width()),popupHeight);
        int y=anchorBottom.y();
        if (openAbove) y=anchorTop.y()-popupHeight;
        int x=qBound(available.left(),anchorBottom.x()-6, std::max(available.left(),available.right()-popup_->width()+1));
        popup_->move(x,y);
        popup_->layout()->activate();
        list_->doItemsLayout();
        popup_->show(); list_->setFocus();
        if (fitsWithoutScrolling) list_->verticalScrollBar()->setValue(0);
        else list_->scrollTo(list_->currentIndex());
    }
    void hidePopup() override { popup_->hide(); QComboBox::hidePopup(); }
    QWidget* popupWidget() const { return popup_; }
    QSize sizeHint() const override {
        const int h=maximumHeight()<1000?maximumHeight():32;
        const int w=maximumWidth()<10000?maximumWidth():std::max(40,QComboBox::sizeHint().width());
        return {w,h};
    }
    QSize minimumSizeHint() const override {return {maximumWidth()<10000?maximumWidth():40,maximumHeight()<1000?maximumHeight():28};}
protected:
    void paintEvent(QPaintEvent*) override {
        const auto& colors=Theme::instance().palette();
        const bool inlineSelect=property("class").toString()=="inlineSelect";
        const bool exportSelect=objectName().startsWith("export");
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        if(!isEnabled())p.setOpacity(0.5);
        if(!inlineSelect) {
            QColor border=hasFocus()?colors.primaryAccent:colors.border;
            p.setPen(QPen(border,1));
            p.setBrush(exportSelect?(Theme::instance().mode()==ThemeMode::Light?QColor("#f5f5f5"):colors.hover):colors.hover);
            p.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),10,10);
        }
        QFont textFont("Inter");textFont.setPixelSize(13);p.setFont(textFont);p.setPen(colors.textPrimary);
        const int left=inlineSelect?4:10;
        const QString label=p.fontMetrics().elidedText(currentText(),Qt::ElideRight,std::max(0,width()-left-24));
        p.drawText(rect().adjusted(left,0,-24,0),Qt::AlignLeft|Qt::AlignVCenter,label);
        p.setPen(QPen(colors.textSecondary,1.1,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        const QPointF center(width()-14,height()/2.0);
        QPainterPath chevron;chevron.moveTo(center+QPointF(-3,-1.5));chevron.lineTo(center+QPointF(0,1.5));chevron.lineTo(center+QPointF(3,-1.5));p.drawPath(chevron);
    }
    bool eventFilter(QObject* watched,QEvent* event) override {
        if (watched==list_ && event->type()==QEvent::KeyPress) {
            const auto key=static_cast<QKeyEvent*>(event)->key();
            if (key==Qt::Key_Return || key==Qt::Key_Enter || key==Qt::Key_Space) { choose(list_->currentIndex().row()); return true; }
            if (key==Qt::Key_Escape) { hidePopup(); setFocus(); return true; }
        }
        return QComboBox::eventFilter(watched,event);
    }
private:
    void choose(int row) {
        if (row<0 || row>=count() || !(model()->flags(model()->index(row,modelColumn(),rootModelIndex())) & Qt::ItemIsEnabled)) return;
        setCurrentIndex(row); hidePopup(); setFocus(); emit activated(row); emit textActivated(currentText());
    }
    SelectPopup* popup_;
    QListView* list_;
};
}

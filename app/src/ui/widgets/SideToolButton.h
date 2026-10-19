#pragma once
#include <QPushButton>
#include <QEnterEvent>
#include "SidebarTooltipWidget.h"
namespace catchim::ui {
class SideToolButton : public QPushButton {
public:
    SideToolButton(const QString& hint,QWidget* parent):QPushButton(parent),hint_(hint){setMouseTracking(true);setCursor(Qt::PointingHandCursor);setAccessibleName(hint);}
    ~SideToolButton() override {delete bubble_;}
protected:
    void enterEvent(QEnterEvent* event) override {
        QPushButton::enterEvent(event);update();
        if(!bubble_){bubble_=new SidebarTooltipWidget(this);bubble_->setObjectName("editorSideHint");}
        auto* rail=parentWidget();
        const auto anchor=rail?rail->mapToGlobal(QPoint(0,mapTo(rail,QPoint(0,height()/2)).y()))
                               :mapToGlobal(QPoint(0,height()/2));
        bubble_->showTip(hint_,anchor,true);
    }
    void leaveEvent(QEvent* event) override {if(bubble_)bubble_->hide();QPushButton::leaveEvent(event);update();}
    void hideEvent(QHideEvent* event) override {if(bubble_)bubble_->hide();QPushButton::hideEvent(event);}
    void mousePressEvent(QMouseEvent* event) override {if(bubble_)bubble_->hideTip();QPushButton::mousePressEvent(event);}
private:
    QString hint_;SidebarTooltipWidget* bubble_{nullptr};
};
}

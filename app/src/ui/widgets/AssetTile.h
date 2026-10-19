#pragma once
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include "ui/theme/Theme.h"

namespace catchim::ui {
class AssetTile : public QPushButton {
public:
    AssetTile(const QString& name,const QPixmap& image,int size=80,bool label=false,QWidget* parent=nullptr)
        :QPushButton(parent),name_(name),image_(image),tileSize_(size),label_(label){
        setFixedSize(size,size+(label?20:0));setToolTip(name);setMouseTracking(true);setCursor(Qt::PointingHandCursor);
    }
    void setDragPayload(const QByteArray& payload){payload_=payload;}
protected:
    void paintEvent(QPaintEvent*) override {
        const auto& palette=Theme::instance().palette();
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF r(0,0,tileSize_,tileSize_);QPainterPath clip;clip.addRoundedRect(r,6,6);
        p.fillPath(clip,palette.hover);p.save();p.setClipPath(clip);
        if(!image_.isNull())p.drawPixmap(r,image_,image_.rect());
        else {p.setPen(palette.textPrimary);QFont f("Inter");f.setPixelSize(12);p.setFont(f);p.drawText(r,Qt::AlignCenter,name_);}
        p.restore();
        if(underMouse()) {
            const QRectF plus(tileSize_-28,tileSize_-28,20,20);
            p.setPen(Qt::NoPen);p.setBrush(palette.background);p.drawRoundedRect(plus,5,5);
            p.setPen(QPen(palette.textPrimary,1.3));auto c=plus.center();p.drawLine(c+QPointF(-4,0),c+QPointF(4,0));p.drawLine(c+QPointF(0,-4),c+QPointF(0,4));
        }
        if(label_){p.setPen(palette.textSecondary);QFont f("Inter");f.setPixelSize(11);p.setFont(f);p.drawText(QRect(0,tileSize_+4,width(),16),Qt::AlignLeft|Qt::AlignTop,name_);}
    }
    void mousePressEvent(QMouseEvent* event) override {start_=event->pos();QPushButton::mousePressEvent(event);}
    void mouseMoveEvent(QMouseEvent* event) override {
        if(!payload_.isEmpty() && (event->buttons()&Qt::LeftButton) && (event->pos()-start_).manhattanLength()>=QApplication::startDragDistance()) {
            auto* drag=new QDrag(this);auto* mime=new QMimeData();mime->setData("application/x-catchim-preset",payload_);drag->setMimeData(mime);drag->setPixmap(grab().scaled(80,80,Qt::KeepAspectRatio,Qt::SmoothTransformation));
            setDown(false);drag->exec(Qt::CopyAction);return;
        }
        QPushButton::mouseMoveEvent(event);
    }
private:
    QString name_;QPixmap image_;int tileSize_;bool label_;QPoint start_;QByteArray payload_;
};
}

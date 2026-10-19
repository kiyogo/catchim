#pragma once
#include "AppDialog.h"
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QSlider>
#include <QMouseEvent>
#include <QPainterPath>
#include <QSignalBlocker>
#include <functional>
#include <algorithm>
#include <array>
namespace catchim::ui {
class EditorColorPlane : public QWidget {
public:
    EditorColorPlane(QWidget* parent):QWidget(parent){setObjectName("editorColorPlane");setMinimumHeight(180);setCursor(Qt::CrossCursor);}
    QColor color{Qt::white};int hue{0};std::function<void(QColor)> changed;
protected:
    QRectF area() const {return QRectF(rect()).adjusted(6,6,-6,-6);}
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const auto r=area();
        QPainterPath path;path.addRoundedRect(r,8,8);p.setClipPath(path);
        p.fillRect(r,QColor::fromHsv(hue,255,255));
        QLinearGradient saturation(r.topLeft(),r.topRight());saturation.setColorAt(0,Qt::white);saturation.setColorAt(1,QColor(255,255,255,0));p.fillRect(r,saturation);
        QLinearGradient value(r.topLeft(),r.bottomLeft());value.setColorAt(0,Qt::transparent);value.setColorAt(1,Qt::black);p.fillRect(r,value);p.setClipping(false);
        const QPointF point(r.left()+color.hsvSaturationF()*r.width(),r.top()+(1-color.valueF())*r.height());
        p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor(0,0,0,150),3));p.drawEllipse(point,5,5);p.setPen(QPen(Qt::white,1.5));p.drawEllipse(point,5,5);
    }
    void choose(QPointF point){const auto r=area();const auto chosen=QColor::fromHsvF(hue/359.,std::clamp((point.x()-r.left())/r.width(),0.,1.),std::clamp(1-(point.y()-r.top())/r.height(),0.,1.));if(changed)changed(chosen);}
    void mousePressEvent(QMouseEvent* event) override {if(event->button()==Qt::LeftButton)choose(event->position());}
    void mouseMoveEvent(QMouseEvent* event) override {if(event->buttons()&Qt::LeftButton)choose(event->position());}
};
inline QColor chooseEditorColor(const QColor& initial,QWidget* parent,const QString& title="Chọn màu") {
    AppDialog dialog(title,parent);dialog.setFixedWidth(408);
    auto* plane=new EditorColorPlane(&dialog);dialog.bodyLayout()->addWidget(plane);
    auto* hue=new QSlider(Qt::Horizontal,&dialog);hue->setRange(0,359);hue->setFixedHeight(22);
    hue->setStyleSheet("QSlider::groove:horizontal{height:10px;border-radius:5px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 red,stop:0.17 yellow,stop:0.33 lime,stop:0.5 cyan,stop:0.67 blue,stop:0.83 magenta,stop:1 red);} QSlider::handle:horizontal{background:white;border:1px solid #aaa;width:12px;margin:-3px 0;border-radius:6px;}");
    dialog.bodyLayout()->addWidget(hue);
    auto* row=new QHBoxLayout;auto* hex=new QLineEdit;hex->setObjectName("editorColorHex");hex->setMaxLength(7);hex->setFixedHeight(32);hex->setPlaceholderText("#FFFFFF");
    auto* swatch=new QLabel;swatch->setFixedSize(32,32);row->addWidget(swatch);row->addWidget(hex,1);dialog.bodyLayout()->addLayout(row);
    auto* rgb=new QHBoxLayout;std::array<QSpinBox*,3> channel{};
    for(int i=0;i<3;++i){auto* column=new QVBoxLayout;column->setSpacing(6);column->addWidget(new QLabel(i==0?"R":i==1?"G":"B"));auto* spin=new QSpinBox;spin->setRange(0,255);spin->setButtonSymbols(QAbstractSpinBox::NoButtons);spin->setFixedHeight(32);column->addWidget(spin);channel[i]=spin;rgb->addLayout(column);}
    dialog.bodyLayout()->addLayout(rgb);
    auto* palette=new QHBoxLayout;palette->setSpacing(6);
    QColor selected=initial.isValid()?initial:QColor(Qt::white);
    QDialogButtonBox* actions=nullptr;
    auto update=[&](QColor color){selected=color;plane->color=color;if(color.hsvHue()>=0)plane->hue=color.hsvHue();plane->update();
        {QSignalBlocker guard(hue);hue->setValue(plane->hue);}{QSignalBlocker guard(hex);hex->setText(color.name().toUpper());}
        for(int i=0;i<3;++i){QSignalBlocker guard(channel[i]);channel[i]->setValue(i==0?color.red():i==1?color.green():color.blue());}
        swatch->setStyleSheet("border:1px solid #ccc;border-radius:7px;background:"+color.name()+";");if(actions)actions->button(QDialogButtonBox::Ok)->setEnabled(true);};
    for(const char* value:{"#ffffff","#000000","#ef4444","#f59e0b","#22c55e","#0ea5e9","#8b5cf6","#ec4899"}){auto* button=new QPushButton;button->setFixedSize(30,30);const QColor color(value);button->setStyleSheet("border:1px solid #ccc;border-radius:7px;padding:0;background:"+color.name()+";");QObject::connect(button,&QPushButton::clicked,&dialog,[&,color]{update(color);});palette->addWidget(button);}
    palette->addStretch();dialog.bodyLayout()->addLayout(palette);
    actions=new QDialogButtonBox(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);actions->button(QDialogButtonBox::Cancel)->setText("Hủy");actions->button(QDialogButtonBox::Ok)->setText("Áp dụng");dialog.bodyLayout()->addWidget(actions);
    plane->changed=update;
    QObject::connect(hue,&QSlider::valueChanged,&dialog,[&](int h){plane->hue=h;update(QColor::fromHsv(h,selected.hsvSaturation(),selected.value()));});
    for(auto* spin:channel)QObject::connect(spin,&QSpinBox::valueChanged,&dialog,[&](int){update(QColor(channel[0]->value(),channel[1]->value(),channel[2]->value()));});
    QObject::connect(hex,&QLineEdit::textEdited,&dialog,[&](const QString& text){QColor color(text.startsWith('#')?text:"#"+text);actions->button(QDialogButtonBox::Ok)->setEnabled(color.isValid());if(color.isValid())update(color);});
    QObject::connect(actions,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(actions,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);update(selected);
    return dialog.exec()==QDialog::Accepted?selected:QColor();
}
}

#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QShowEvent>
#include <QHideEvent>
#include <QGraphicsDropShadowEffect>
#include "ui/theme/Theme.h"
#include "EditorLanguage.h"

namespace catchim::ui {
// Native card shared by editor tools: one title, rounded edge and modal shade.
class AppDialog : public QDialog {
public:
    explicit AppDialog(const QString& title,QWidget* parent=nullptr):QDialog(parent) {
        setWindowTitle(title);setWindowFlags(Qt::Dialog|Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);setModal(true);setObjectName("appDialog");
        const auto& p=Theme::instance().palette();
        setStyleSheet(QString("#appDialog{background:transparent;} #appDialogCard{background:%1;border:1px solid %2;border-radius:12px;}").arg(p.background.name(),p.border.name()));
        auto* outer=new QVBoxLayout(this);outer->setContentsMargins(12,12,12,12);
        auto* card=new QWidget;card->setObjectName("appDialogCard");card->setAttribute(Qt::WA_StyledBackground);outer->addWidget(card);
        auto* shadow=new QGraphicsDropShadowEffect(card);shadow->setBlurRadius(18);shadow->setOffset(0,4);shadow->setColor(QColor(0,0,0,45));card->setGraphicsEffect(shadow);
        auto* column=new QVBoxLayout(card);column->setContentsMargins(1,1,1,1);column->setSpacing(0);
        auto* head=new QWidget;auto* row=new QHBoxLayout(head);row->setContentsMargins(16,8,12,8);
        auto* label=new QLabel(title);label->setStyleSheet("border:none;background:transparent;font-size:14px;font-weight:500;");row->addWidget(label);row->addStretch();
        auto* close=new QPushButton(QString::fromUtf8("×"));close->setFixedSize(28,28);
        close->setStyleSheet(QString("QPushButton{border:none;background:transparent;padding:0;font-size:22px;color:%1;} QPushButton:hover{background:%2;border-radius:6px;}").arg(p.textSecondary.name(),p.hover.name()));
        row->addWidget(close);connect(close,&QPushButton::clicked,this,&QDialog::reject);column->addWidget(head);
        auto* line=new QWidget;line->setFixedHeight(1);line->setStyleSheet("background:"+p.border.name()+";");column->addWidget(line);
        auto* body=new QWidget;body_=new QVBoxLayout(body);body_->setContentsMargins(16,16,16,16);body_->setSpacing(12);column->addWidget(body,1);
    }
    ~AppDialog() override {delete shade_;}
    QVBoxLayout* bodyLayout() const {return body_;}
protected:
    void showEvent(QShowEvent* event) override {
        applyEditorLanguage(this);
        if(parentWidget()) {
            auto* host=parentWidget()->window();
            if(!shade_){shade_=new QWidget(host);shade_->setObjectName("appDialogShade");shade_->setAttribute(Qt::WA_StyledBackground);shade_->setStyleSheet("background:rgba(0,0,0,140);");}
            shade_->setGeometry(host->rect());shade_->show();shade_->raise();
            move(host->mapToGlobal(QPoint((host->width()-width())/2,(host->height()-height())/2)));
        }
        QDialog::showEvent(event);
    }
    void hideEvent(QHideEvent* event) override {if(shade_)shade_->hide();QDialog::hideEvent(event);}
private:
    QVBoxLayout* body_;QWidget* shade_{nullptr};
};
}

#pragma once
#if defined(HAVE_QT6)
#include <QDialog>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QSettings>
#include <QShowEvent>
#include <QHideEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "ui/theme/Theme.h"

namespace catchim::ui {
class FeedbackDialog : public QDialog {
public:
    explicit FeedbackDialog(QWidget* host):QDialog(host),network_(this) {
        const auto& pal=Theme::instance().palette();
        setObjectName("feedbackDialog");setWindowTitle("Gửi phản hồi");setWindowFlags(Qt::Dialog|Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);setModal(true);setFixedWidth(408);
        setStyleSheet(QString("#feedbackDialog{background:transparent;} #feedbackCard{background:%1;border:1px solid %2;border-radius:12px;}").arg(pal.background.name(),pal.border.name()));
        auto* outer=new QVBoxLayout(this);outer->setContentsMargins(12,12,12,12);
        auto* card=new QWidget;card->setObjectName("feedbackCard");card->setAttribute(Qt::WA_StyledBackground);outer->addWidget(card);
        auto* shadow=new QGraphicsDropShadowEffect(card);shadow->setBlurRadius(20);shadow->setOffset(0,4);shadow->setColor(QColor(0,0,0,40));card->setGraphicsEffect(shadow);
        auto* layout=new QVBoxLayout(card);layout->setContentsMargins(1,1,1,1);layout->setSpacing(0);
        auto* header=new QWidget;header->setFixedHeight(43);auto* head=new QHBoxLayout(header);head->setContentsMargins(12,0,16,0);
        head->addWidget(new QLabel("Gửi phản hồi"));head->addStretch();
        auto* close=new QPushButton("×");close->setObjectName("feedbackClose");close->setFixedSize(24,24);close->setStyleSheet("border:none;background:transparent;font-size:20px;color:#999;padding:0;");head->addWidget(close);
        connect(close,&QPushButton::clicked,this,&QDialog::reject);layout->addWidget(header);
        auto separator=[&]{auto* line=new QWidget;line->setFixedHeight(1);line->setStyleSheet(QString("background:%1;").arg(pal.border.name()));layout->addWidget(line);};separator();
        message_=new QTextEdit;message_->setObjectName("feedbackMessage");message_->setFixedHeight(112);message_->setPlaceholderText("Góp ý, lỗi, ý tưởng...");
        message_->setStyleSheet(QString("QTextEdit{background:%1;border:none;padding:8px;font-size:14px;border-radius:0;}").arg(pal.background.name()));
        message_->setPlainText(settings_.value("feedback/draft").toString());layout->addWidget(message_);
        status_=new QLabel;status_->setWordWrap(true);status_->setStyleSheet("color:#b45309;font-size:12px;padding:4px 12px;");status_->hide();layout->addWidget(status_);
        separator();auto* footer=new QWidget;footer->setFixedHeight(44);auto* buttons=new QHBoxLayout(footer);buttons->setContentsMargins(12,8,12,8);buttons->setSpacing(8);
        auto* history=new QPushButton;history->setObjectName("feedbackHistory");history->setFixedHeight(28);buttons->addWidget(history);buttons->addStretch();
        const auto entries=QJsonDocument::fromJson(settings_.value("feedback/history").toByteArray()).array();
        history->setText(QString::number(entries.size()));history->setVisible(!entries.empty());
        connect(history,&QPushButton::clicked,this,[this,entries] {
            QString text;for(const auto& value:entries){const auto e=value.toObject();text+=e.value("createdAt").toString()+"\n"+e.value("message").toString()+"\n\n";}
            message_->setReadOnly(true);message_->setPlainText(text);send_->hide();cancel_->show();cancel_->setText("Đóng");
        });
        cancel_=new QPushButton("Hủy");cancel_->setFixedSize(46,28);connect(cancel_,&QPushButton::clicked,this,&QDialog::reject);buttons->addWidget(cancel_);
        cancel_->setStyleSheet(QString("background:transparent;border:1px solid %1;border-radius:5px;padding:0;").arg(pal.border.name()));
        send_=new QPushButton("Gửi");send_->setObjectName("feedbackSend");send_->setFixedSize(41,28);buttons->addWidget(send_);
        send_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;border-radius:5px;font-weight:600;padding:0;} QPushButton:disabled{background:#999;color:white;}").arg(pal.textPrimary.name(),pal.background.name()));
        layout->addWidget(footer);
        auto draftChanged=[this] {
            if(message_->isReadOnly())return;
            const bool empty=message_->toPlainText().trimmed().isEmpty();cancel_->setVisible(empty);send_->setEnabled(!empty&&message_->toPlainText().size()<=5000);
            settings_.setValue("feedback/draft",message_->toPlainText());
        };
        connect(message_,&QTextEdit::textChanged,this,draftChanged);draftChanged();
        connect(send_,&QPushButton::clicked,this,[this] {
            const QUrl endpoint(qEnvironmentVariable("CATCHIM_FEEDBACK_URL",settings_.value("services/feedbackUrl").toString()));
            if(!endpoint.isValid()||endpoint.host().isEmpty()||(endpoint.scheme()!="https"&&!(endpoint.scheme()=="http"&&(endpoint.host()=="localhost"||endpoint.host()=="127.0.0.1")))) {
                status_->setText("Chưa cấu hình dịch vụ gửi phản hồi. Nội dung đã được giữ trong bản nháp.");status_->show();adjustSize();return;
            }
            QNetworkRequest request(endpoint);request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");request.setTransferTimeout(15000);
            send_->setEnabled(false);message_->setReadOnly(true);
            auto* reply=network_.post(request,QJsonDocument(QJsonObject{{"message",message_->toPlainText()}}).toJson(QJsonDocument::Compact));
            connect(reply,&QNetworkReply::finished,this,[this,reply]{
                const auto document=QJsonDocument::fromJson(reply->readAll()).object();
                const auto entry=document.value("entry").toObject();
                const bool success=reply->error()==QNetworkReply::NoError&&!entry.value("id").toString().isEmpty()&&entry.value("message").isString()&&entry.value("createdAt").isString();
                if(success) {
                    auto entries=QJsonDocument::fromJson(settings_.value("feedback/history").toByteArray()).array();entries.prepend(entry);while(entries.size()>20)entries.removeLast();
                    settings_.setValue("feedback/history",QJsonDocument(entries).toJson(QJsonDocument::Compact));settings_.remove("feedback/draft");accept();
                } else {
                    status_->setText(document.value("error").toString("Không gửi được phản hồi: "+reply->errorString()));status_->show();
                    message_->setReadOnly(false);send_->setEnabled(true);adjustSize();
                }
                reply->deleteLater();
            });
        });
        adjustSize();
    }
    ~FeedbackDialog() override {delete backdrop_;}
protected:
    void showEvent(QShowEvent* event) override {
        auto* host=parentWidget()->window();
        if(!backdrop_) {
            const auto capture=host->grab();backdrop_=new QWidget(host);backdrop_->setGeometry(host->rect());
            auto* image=new QLabel(backdrop_);image->setGeometry(backdrop_->rect());image->setPixmap(capture);auto* blur=new QGraphicsBlurEffect(image);blur->setBlurRadius(6);image->setGraphicsEffect(blur);
            auto* tint=new QWidget(backdrop_);tint->setGeometry(backdrop_->rect());tint->setStyleSheet("background:rgba(0,0,0,166);");tint->setAttribute(Qt::WA_StyledBackground);
        }
        backdrop_->show();backdrop_->raise();move(host->mapToGlobal(QPoint((host->width()-width())/2,(host->height()-height())/2)));QDialog::showEvent(event);message_->setFocus();
    }
    void hideEvent(QHideEvent* e) override {if(backdrop_)backdrop_->hide();QDialog::hideEvent(e);}
private:
    QSettings settings_;QNetworkAccessManager network_;QWidget* backdrop_{nullptr};
    QTextEdit* message_;QPushButton* send_;QPushButton* cancel_;QLabel* status_;
};
}
#endif

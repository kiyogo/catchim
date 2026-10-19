#pragma once
#if defined(HAVE_QT6)
#include <QDialog>
#include <QSet>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QMouseEvent>
#include <QStyledItemDelegate>
#include "editor/EditorEngine.h"
#include "editor/history/commands/SceneCommands.h"
#include "ui/theme/Theme.h"
#include "ui/icons/UiIcons.h"

namespace catchim::ui {
class SceneRowDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        painter->save();painter->setRenderHint(QPainter::Antialiasing);
        const auto& pal=Theme::instance().palette();const bool checked=index.data(Qt::UserRole+1).toBool();
        const QRectF card=QRectF(option.rect).adjusted(.5,.5,-.5,-8.5);
        painter->setPen(QPen(checked?pal.primaryAccent:pal.border,1));
        painter->setBrush(option.state&QStyle::State_MouseOver?pal.hover:pal.background);painter->drawRoundedRect(card,10,10);
        painter->setPen(checked?pal.primaryAccent:pal.textPrimary);painter->drawText(card.adjusted(16,0,-36,0),Qt::AlignLeft|Qt::AlignVCenter,index.data().toString());
        if(checked)painter->drawPixmap(QPoint(qRound(card.right()-30),qRound(card.center().y()-8)),UiIcons::getPixmap(UiIcon::Check,pal.primaryAccent,16));
        painter->restore();
    }
};
class ScenesDrawer : public QDialog {
public:
    ScenesDrawer(editor::EditorEngine& engine,QWidget* host):QDialog(host),engine_(engine) {
        setObjectName("scenesDrawer");setWindowFlags(Qt::Dialog|Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);setModal(true);
        setStyleSheet("#scenesDrawer{background:transparent;}");
        const auto& pal=Theme::instance().palette();
        auto* root=new QHBoxLayout(this);root->setContentsMargins(0,0,0,0);root->setSpacing(0);root->addStretch();
        panel_=new QWidget(this);panel_->setObjectName("scenesDrawerPanel");panel_->setFixedWidth(std::min(384,host->width()));
        panel_->setAttribute(Qt::WA_StyledBackground);
        panel_->setStyleSheet(QString("#scenesDrawerPanel{background:%1;border-left:1px solid %2;}").arg(pal.background.name(),pal.border.name()));
        root->addWidget(panel_);auto* layout=new QVBoxLayout(panel_);layout->setContentsMargins(24,24,24,24);layout->setSpacing(8);
        title_=new QLabel("Cảnh");title_->setStyleSheet("font-size:18px;font-weight:600;");title_->setFixedHeight(24);layout->addWidget(title_);
        description_=new QLabel("Chuyển đổi giữa các cảnh trong dự án");description_->setWordWrap(true);
        description_->setStyleSheet(QString("color:%1;font-size:14px;").arg(pal.textSecondary.name()));layout->addWidget(description_);
        layout->addSpacing(8);
        auto* actions=new QHBoxLayout;select_=new QPushButton("Chọn");select_->setObjectName("scenesSelect");select_->setFixedSize(78,28);select_->setIcon(UiIcons::get(UiIcon::List,pal.textPrimary,16));
        actions->addWidget(select_);remove_=new QPushButton("Xóa");remove_->setObjectName("scenesDelete");remove_->setFixedHeight(28);remove_->hide();actions->addWidget(remove_);actions->addStretch();layout->addLayout(actions);
        select_->setStyleSheet(QString("background:transparent;border:1px solid %1;border-radius:6px;padding:0;").arg(pal.border.name()));
        layout->addSpacing(8);list_=new QListWidget;list_->setObjectName("scenesList");list_->setSelectionMode(QAbstractItemView::NoSelection);
        list_->setSpacing(0);list_->setStyleSheet("QListWidget{border:none;background:transparent;padding:0;}");list_->setItemDelegate(new SceneRowDelegate(list_));
        layout->addWidget(list_,1);
        close_=new QPushButton("×",panel_);close_->setObjectName("scenesClose");close_->setFixedSize(24,24);close_->setStyleSheet("background:transparent;border:none;font-size:22px;padding:0;");close_->setToolTip("Đóng");
        connect(close_,&QPushButton::clicked,this,&QDialog::reject);
        connect(select_,&QPushButton::clicked,this,[this]{selectMode_=!selectMode_;selected_.clear();refresh();});
        connect(list_,&QListWidget::itemClicked,this,[this](QListWidgetItem* item){
            const auto id=item->data(Qt::UserRole).toString();
            if(selectMode_){if(selected_.contains(id))selected_.remove(id);else selected_.insert(id);refresh();return;}
            if(engine_.project().switchScene(core::SceneId(id.toStdString()))) {
                engine_.playback().pause();engine_.playback().seek(core::TimelineTime(0));engine_.deselectAll();
                engine_.project().setDirty(true);engine_.notifyProjectChanged();engine_.notifyTimelineChanged();refresh();
            }
        });
        connect(remove_,&QPushButton::clicked,this,[this]{
            if(QMessageBox::question(this,"Xóa cảnh","Xóa các cảnh đã chọn? Có thể hoàn tác bằng Ctrl+Z.")!=QMessageBox::Yes)return;
            engine_.playback().pause();engine_.deselectAll();
            for(const auto& id:selected_)engine_.history().execute(std::make_unique<editor::DeleteSceneCommand>(engine_.project(),core::SceneId(id.toStdString())));
            selected_.clear();selectMode_=false;engine_.notifyProjectChanged();refresh();
        });
        // Native management extensions stay in a context menu, leaving the web drawer layout unchanged.
        list_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(list_,&QListWidget::customContextMenuRequested,this,[this](QPoint point){
            QMenu menu(this);auto* create=menu.addAction("Tạo cảnh");auto* item=list_->itemAt(point);
            auto* rename=menu.addAction("Đổi tên");auto* duplicate=menu.addAction("Nhân đôi");rename->setEnabled(item);duplicate->setEnabled(item);
            const auto sceneId=item?core::SceneId(item->data(Qt::UserRole).toString().toStdString()):core::SceneId::empty();
            auto* chosen=menu.exec(list_->mapToGlobal(point));if(!chosen)return;
            bool ok=false;QString name;
            if(chosen==create||chosen==rename){name=QInputDialog::getText(this,chosen==create?"Tạo cảnh":"Đổi tên","Tên cảnh",QLineEdit::Normal,item?item->text():QString(),&ok).trimmed();if(!ok||name.isEmpty())return;}
            engine_.playback().pause();engine_.deselectAll();
            if(chosen==create)engine_.history().execute(std::make_unique<editor::CreateSceneCommand>(engine_.project(),name.toStdString()));
            else if(chosen==rename)engine_.history().execute(std::make_unique<editor::RenameSceneCommand>(engine_.project(),sceneId,name.toStdString()));
            else if(chosen==duplicate)engine_.history().execute(std::make_unique<editor::DuplicateSceneCommand>(engine_.project(),sceneId));
            engine_.notifyProjectChanged();refresh();
        });
        refresh();
        setGeometry(QRect(host->mapToGlobal(QPoint()),host->size()));
    }
protected:
    void paintEvent(QPaintEvent*) override {QPainter p(this);p.fillRect(rect(),QColor(0,0,0,128));}
    void resizeEvent(QResizeEvent* e) override {QDialog::resizeEvent(e);close_->move(panel_->width()-40,16);}
    void mousePressEvent(QMouseEvent* e) override {if(!panel_->geometry().contains(e->pos()))reject();else QDialog::mousePressEvent(e);}
private:
    void refresh() {
        const auto& pal=Theme::instance().palette();list_->clear();bool mainSelected=false;
        for(const auto& scene:engine_.project().scenes()){
            const auto id=QString::fromStdString(scene.id().str());
            auto* item=new QListWidgetItem(QString::fromStdString(scene.name()),list_);item->setData(Qt::UserRole,id);item->setSizeHint(QSize(0,44));
            const bool checked=selectMode_?selected_.contains(id):scene.id()==engine_.project().currentSceneId();
            item->setData(Qt::UserRole+1,checked);
            if(checked){item->setIcon(UiIcons::get(UiIcon::Check,pal.primaryAccent,16));item->setForeground(pal.primaryAccent);}
            if(scene.isMain()&&selected_.contains(id))mainSelected=true;
        }
        select_->setText(selectMode_?"Hủy":"Chọn");remove_->setVisible(selectMode_);remove_->setText(QString("Xóa (%1)").arg(selected_.size()));remove_->setEnabled(!selected_.empty()&&!mainSelected);
        title_->setText(selectMode_?QString("Chọn cảnh (%1)").arg(selected_.size()):"Cảnh");
        description_->setText(selectMode_?"Chọn các cảnh cần xóa":"Chuyển đổi giữa các cảnh trong dự án");
    }
    editor::EditorEngine& engine_;
    QWidget* panel_;QLabel* title_;QLabel* description_;QPushButton* close_;QPushButton* select_;QPushButton* remove_;QListWidget* list_;
    bool selectMode_{false};QSet<QString> selected_;
};
}
#endif

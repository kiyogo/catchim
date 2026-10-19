#pragma once
#include <QLayout>
#include <QWidget>
#include <QVector>

namespace catchim::ui {
// Wrapping layout for the web's flex-wrap swatches and sticker grids.
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent,int gap=8):QLayout(parent),gap_(gap){setContentsMargins(0,0,0,0);}
    ~FlowLayout() override {while(auto* item=takeAt(0))delete item;}
    void addItem(QLayoutItem* item) override {items_.append(item);}
    int count() const override {return items_.size();}
    QLayoutItem* itemAt(int i) const override {return items_.value(i,nullptr);}
    QLayoutItem* takeAt(int i) override {return i>=0&&i<items_.size()?items_.takeAt(i):nullptr;}
    Qt::Orientations expandingDirections() const override {return {};}
    bool hasHeightForWidth() const override {return true;}
    int heightForWidth(int width) const override {return arrange(QRect(0,0,width,0),false);}
    QSize sizeHint() const override {return minimumSize();}
    QSize minimumSize() const override {QSize s;for(auto* item:items_)if(!item->isEmpty())s=s.expandedTo(item->minimumSize());return s;}
    void setGeometry(const QRect& rect) override {QLayout::setGeometry(rect);arrange(rect,true);}
private:
    int arrange(const QRect& rect,bool apply) const {
        int x=rect.x(),y=rect.y(),rowHeight=0;
        for(auto* item:items_) {
            if(item->isEmpty())continue;
            const QSize size=item->sizeHint();
            if(x>rect.x() && x+size.width()>rect.right()+1){x=rect.x();y+=rowHeight+gap_;rowHeight=0;}
            if(apply)item->setGeometry(QRect(QPoint(x,y),size));
            x+=size.width()+gap_;rowHeight=std::max(rowHeight,size.height());
        }
        return y+rowHeight-rect.y();
    }
    QVector<QLayoutItem*> items_;
    int gap_;
};
}

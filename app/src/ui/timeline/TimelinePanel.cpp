#include "TimelinePanel.h"
#include "ui/theme/Theme.h"
#if defined(HAVE_QT6)
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QTimer>
#include <cmath>

namespace catchim::ui {
TimelinePanel::TimelinePanel(editor::EditorEngine& engine,media::MediaLibrary& library,QWidget* parent)
    :QWidget(parent),engine_(engine),mediaLibrary_(library) {
    setAttribute(Qt::WA_StyledBackground,true);setupUi();refresh();
}
void TimelinePanel::setupUi() {
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(1,1,1,1);layout->setSpacing(0);
    toolbar_=new TimelineToolbar(engine_,this);
    connect(toolbar_,&TimelineToolbar::zoomChanged,this,&TimelinePanel::onZoomChanged);layout->addWidget(toolbar_);
    rulerWidget_=new TimelineRulerWidget(engine_,this);connect(rulerWidget_,&TimelineRulerWidget::seekRequested,this,&TimelinePanel::onSeekRequested);layout->addWidget(rulerWidget_);
    auto* row=new QHBoxLayout;row->setContentsMargins(0,0,0,0);row->setSpacing(0);
    scrollArea_=new QScrollArea(this);scrollArea_->setObjectName("timelineScrollArea");
    scrollArea_->setWidgetResizable(true);scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tracksWidget_=new TimelineTracksWidget(engine_,mediaLibrary_);
    tracksWidget_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);
    connect(tracksWidget_,&TimelineTracksWidget::mediaRevealRequested,this,&TimelinePanel::mediaRevealRequested);
    connect(tracksWidget_,&TimelineTracksWidget::seekRequested,this,&TimelinePanel::onSeekRequested);
    connect(tracksWidget_,&TimelineTracksWidget::clipSelected,this,&TimelinePanel::clipSelected);
    scrollArea_->setWidget(tracksWidget_);row->addWidget(scrollArea_,1);
    verticalBar_=new QScrollBar(Qt::Vertical,this);verticalBar_->setObjectName("timelineVerticalScroll");row->addWidget(verticalBar_);layout->addLayout(row,1);
    horizontalBar_=new QScrollBar(Qt::Horizontal,this);horizontalBar_->setObjectName("timelineHorizontalScroll");layout->addWidget(horizontalBar_);
    auto scroll=[this]{
        rulerWidget_->setScrollOffset(horizontalBar_->value());
        tracksWidget_->setScrollOffset(horizontalBar_->value(),verticalBar_->value());
    };
    connect(horizontalBar_,&QScrollBar::valueChanged,this,scroll);connect(verticalBar_,&QScrollBar::valueChanged,this,scroll);
    for(auto* surface:{static_cast<QWidget*>(tracksWidget_),scrollArea_->viewport(),static_cast<QWidget*>(rulerWidget_)})surface->installEventFilter(this);
}
void TimelinePanel::updateScrollRanges() {
    const int visible=std::max(1,scrollArea_->viewport()->width()-Metrics::trackLabelsWidth);
    const double duration=engine_.project().totalDuration().toSeconds();
    // Stop the scrollable area at the actual project duration.
    // Include the endpoint pixel so the final clip border stays inside the
    // viewport at maximum zoom, without adding an empty time range.
    const int maximum=duration>0?std::max(0,qCeil(duration*50*zoomFactor_)+1-visible):0;
    horizontalBar_->setPageStep(visible);horizontalBar_->setRange(0,maximum);horizontalBar_->setVisible(maximum>0);
    const int height=scrollArea_->viewport()->height();
    const int verticalMaximum=std::max(0,tracksWidget_->totalTracksHeight()-height);
    verticalBar_->setPageStep(height);verticalBar_->setRange(0,verticalMaximum);verticalBar_->setVisible(verticalMaximum>0);
    rulerWidget_->setScrollOffset(horizontalBar_->value());tracksWidget_->setScrollOffset(horizontalBar_->value(),verticalBar_->value());
}
void TimelinePanel::refresh() {
    lastPlayheadX_=-1;
    const double duration=engine_.project().totalDuration().toSeconds();
    if(duration>0&&(previousDuration_<=0||viewSceneId_!=engine_.project().currentSceneId())) {
        zoomFactor_=std::clamp((std::max(100,scrollArea_->viewport()->width()-Metrics::trackLabelsWidth-64))/(duration*50),.02,100.);
        toolbar_->setZoomFactor(zoomFactor_);rulerWidget_->setZoomFactor(zoomFactor_);tracksWidget_->setZoomFactor(zoomFactor_);
        horizontalBar_->setValue(0);
    }
    previousDuration_=duration;viewSceneId_=engine_.project().currentSceneId();
    updateScrollRanges();toolbar_->refresh();rulerWidget_->update();tracksWidget_->update();
}
void TimelinePanel::revealPlayhead() {
        const int x=tracksWidget_->viewportXAtTime(engine_.playback().currentTime());
        const int right=scrollArea_->viewport()->width()-24;
        if(x>right)horizontalBar_->setValue(horizontalBar_->value()+x-right);
        else if(x<Metrics::trackLabelsWidth)
            horizontalBar_->setValue(std::max(0,static_cast<int>(std::floor(engine_.playback().currentTime().toSeconds()*50*zoomFactor_))-24));
}
void TimelinePanel::updatePlaybackPosition() {
    if(engine_.playback().isPlaying())revealPlayhead();
    const int x=tracksWidget_->viewportXAtTime(engine_.playback().currentTime());
    if(x==lastPlayheadX_)return;
    if(lastPlayheadX_<0){rulerWidget_->update();tracksWidget_->update();}
    else {
        for(const int position:{lastPlayheadX_,x}) {
            rulerWidget_->update(QRect(position-10,0,21,rulerWidget_->height()));
            tracksWidget_->update(QRect(position-10,0,21,tracksWidget_->height()));
        }
    }
    lastPlayheadX_=x;
}
void TimelinePanel::onZoomChanged(double factor) {
    const int visible=std::max(1,scrollArea_->viewport()->width()-Metrics::trackLabelsWidth);
    double anchorX=engine_.playback().currentTime().toSeconds()*50*zoomFactor_-horizontalBar_->value();
    if(anchorX<0||anchorX>visible)anchorX=visible*.5;
    const double anchorTime=(horizontalBar_->value()+anchorX)/(50*zoomFactor_);
    zoomFactor_=std::clamp(factor,.02,100.);
    rulerWidget_->setZoomFactor(zoomFactor_);tracksWidget_->setZoomFactor(zoomFactor_);toolbar_->setZoomFactor(zoomFactor_);
    updateScrollRanges();horizontalBar_->setValue(qRound(anchorTime*50*zoomFactor_-anchorX));
}
void TimelinePanel::onSeekRequested(core::TimelineTime time) {engine_.seek(time);revealPlayhead();updatePlaybackPosition();}
void TimelinePanel::resizeEvent(QResizeEvent* e) {QWidget::resizeEvent(e);QTimer::singleShot(0,this,[this]{updateScrollRanges();});}
bool TimelinePanel::eventFilter(QObject* object,QEvent* event) {
    if(object==scrollArea_->viewport()&&event->type()==QEvent::Resize)
        QTimer::singleShot(0,this,[this]{updateScrollRanges();});
    if(event->type()==QEvent::Wheel) {
        auto* wheel=static_cast<QWheelEvent*>(event);const int delta=wheel->angleDelta().y();
        const int horizontal=wheel->pixelDelta().isNull()?wheel->angleDelta().x():wheel->pixelDelta().x();
        const int vertical=wheel->pixelDelta().isNull()?delta:wheel->pixelDelta().y();
        if(wheel->modifiers()&Qt::ControlModifier)onZoomChanged(zoomFactor_*std::pow(1.2,delta/120.));
        else if(horizontal||wheel->modifiers()&Qt::ShiftModifier||!verticalBar_->isVisible())horizontalBar_->setValue(horizontalBar_->value()-(horizontal?horizontal:vertical));
        else verticalBar_->setValue(verticalBar_->value()-vertical);
        wheel->accept();return true;
    }
    return QWidget::eventFilter(object,event);
}
}
#endif

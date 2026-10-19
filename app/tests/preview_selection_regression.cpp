#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineF>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "ui/preview/PreviewWidget.h"
#include "ui/theme/Theme.h"
#include "media/probe/MediaProbe.h"

using namespace catchim;

int main(int argc,char** argv) {
    QApplication app(argc,argv); app.setStyle("Fusion"); ui::Theme::instance().setTheme(ui::ThemeMode::Light);
    const QString output=qEnvironmentVariable("CATCHIM_SELECTION_OUTPUT",
        "E:/DATA/source/catchim/app/qa/preview-selection/dpi-"+QString::number(app.devicePixelRatio()));
    QDir().mkpath(output); QJsonArray checks; bool passed=true;
    auto check=[&](const QString& name,bool ok){checks.append(QJsonObject{{"name",name},{"passed",ok}});passed&=ok;std::cout<<(ok?"PASS ":"FAIL ")<<name.toStdString()<<std::endl;};
    auto wait=[&](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
    editor::EditorEngine engine; media::MediaLibrary library; render::RenderEngine renderer;
    engine.project().settings().canvasSize={900,600}; engine.project().settings().background.color="#000000";
    QTemporaryDir temporary; QImage fixture(600,320,QImage::Format_RGBA8888);fixture.fill(Qt::white);
    const QString fixturePath=temporary.filePath("visible-bounds.png");check("fixture-written",fixture.save(fixturePath));
    auto asset=std::make_shared<media::MediaAsset>(core::MediaId::generate(),fixturePath.toStdWString(),media::MediaType::Image);
    asset->setDimensions(600,320);library.addAsset(asset);
    editor::Clip clip(core::ClipId::generate(),editor::ClipType::Image,"Bounds fixture",core::TimelineTime(0),core::TimelineTime::fromSeconds(10));
    const auto id=clip.id();clip.setMediaId(asset->id());
    engine.addClip(engine.activeTimeline()->mainTrack().id(),clip);engine.selectClip(id);
    auto current=[&]{return engine.activeTimeline()->findClip(id);};
    ui::PreviewWidget preview(engine,renderer,library);preview.resize(752,509);preview.show();app.processEvents();
    auto render=[&]{const auto before=preview.presentedFrames();preview.markFrameDirty();preview.updateFrameIfNeeded();QElapsedTimer timer;timer.start();
        while(preview.presentedFrames()==before&&timer.elapsed()<10000)wait(10);
        app.processEvents();return preview.presentedFrames()>before;};
    auto viewport=[&]{const double zoom=preview.zoomFactor()<0?std::max(.05,std::min(double(preview.width()-16)/900,double(preview.height()-16)/600)):preview.zoomFactor();
        const int w=qRound(900*zoom),h=qRound(600*zoom);return QRect((preview.width()-w)/2,8+(preview.height()-16-h)/2,w,h);};
    auto mouse=[&](QEvent::Type type,QPointF point,Qt::MouseButton button,Qt::MouseButtons buttons){
        QMouseEvent event(type,point,QPointF(preview.mapToGlobal(point.toPoint())),button,buttons,Qt::NoModifier);QApplication::sendEvent(&preview,&event);};
    auto drag=[&](QPointF from,QPointF to){mouse(QEvent::MouseButtonPress,from,Qt::LeftButton,Qt::LeftButton);
        mouse(QEvent::MouseMove,to,Qt::NoButton,Qt::LeftButton);mouse(QEvent::MouseButtonRelease,to,Qt::LeftButton,Qt::NoButton);};
    auto midpoint=[](QPointF a,QPointF b){return (a+b)/2;};
    auto handlePoints=[&]{const auto p=preview.selectionOutline();return std::array<QPointF,8>{p[0],midpoint(p[0],p[1]),p[1],midpoint(p[0],p[3]),midpoint(p[1],p[2]),p[3],midpoint(p[3],p[2]),p[2]};};
    // Compare the overlay against independently decoded/composited pixels, not
    // merely another copy of the widget's geometry formula.
    auto pixelAgreement=[&](const QString& name){
        check(name+"-rendered",render());const auto polygon=preview.selectionOutline();const auto image=preview.presentedImage();const auto v=viewport();
        int mismatches=0,samples=0;
        for(int y=4;y<image.height()-4;y+=3)for(int x=4;x<image.width()-4;x+=3){
            const QPointF point(v.x()+(x+.5)*v.width()/image.width(),v.y()+(y+.5)*v.height()/image.height());
            bool nearEdge=false;
            for(int i=0;i<4;++i){const QPointF a=polygon[i],edge=polygon[(i+1)%4]-a,delta=point-a;
                const double t=std::clamp(QPointF::dotProduct(delta,edge)/QPointF::dotProduct(edge,edge),0.,1.);
                if(QLineF(point,a+edge*t).length()<2)nearEdge=true;}
            if(nearEdge)continue;
            const auto color=image.pixelColor(x,y);const bool visible=color.red()>180&&color.green()>180&&color.blue()>180;
            mismatches+=visible!=polygon.containsPoint(point,Qt::OddEvenFill);++samples;
        }
        check(name+"-selection-matches-actual-pixels",samples>1000&&mismatches==0);
        preview.grab().save(output+"/"+name+".png");
    };
    pixelAgreement("fit-letterboxed");
    auto p=preview.selectionOutline();const auto v=viewport();
    check("uses-source-not-80-percent-of-canvas",std::abs(p.boundingRect().width()-600.*v.width()/900)<.01&&std::abs(p.boundingRect().height()-320.*v.height()/600)<.01);
    const auto composed=preview.grab();const auto screenshot=composed.toImage();bool blueEdges=true;
    for(int i=0;i<4;++i){const auto edge=p[i]*.75+p[(i+1)%4]*.25;bool blue=false;
        const QPoint physical=(edge*composed.devicePixelRatio()).toPoint();
        for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){const auto color=screenshot.pixelColor(physical+QPoint(dx,dy));blue|=color.blue()>160&&color.green()>80&&color.red()<120;}
        blueEdges&=blue;}
    check("blue-border-painted-on-real-four-edges",blueEdges);
    current()->setParam("transform.scaleX",.13);current()->setParam("transform.scaleY",.19);pixelAgreement("small-nonuniform-no-minimum-box");
    current()->setParam("transform.scaleX",.7);current()->setParam("transform.scaleY",.5);
    current()->setParam("transform.positionX",73.);current()->setParam("transform.positionY",-41.);
    current()->setParam("transform.rotate",31.);pixelAgreement("translated-rotated");
    current()->setParam("transform.scaleX",-.7);pixelAgreement("negative-scale");current()->setParam("transform.scaleX",.7);
    preview.setZoomFactor(.25);pixelAgreement("zoom-25");preview.setZoomFactor(1.);pixelAgreement("zoom-100");preview.fitToScreen();render();
    // Test every corner and side on a rotated, non-uniformly scaled image.
    for(int index=0;index<8;++index){
        const auto before=current()->params();const auto history=engine.history().undoIndex();
        p=preview.selectionOutline();const QPointF center=(p[0]+p[2])/2,from=handlePoints()[index];
        drag(from,center+(from-center)*1.12);
        const double sx=current()->getParam<double>("transform.scaleX",1),sy=current()->getParam<double>("transform.scaleY",1);
        const bool vertical=index==1||index==6,horizontal=index==3||index==4;
        check(QString("handle-%1-correct-axis-and-direction").arg(index),
            (vertical?std::abs(sx-.7)<1e-9:sx>.74)&&(horizontal?std::abs(sy-.5)<1e-9:sy>.53));
        check(QString("handle-%1-single-undo-record").arg(index),engine.history().undoIndex()==history+1);
        check(QString("handle-%1-undo-exact").arg(index),engine.undo()&&current()->params()==before);
        check(QString("handle-%1-redo").arg(index),engine.redo()&&current()->getParam<double>("transform.scaleX",1)==sx&&current()->getParam<double>("transform.scaleY",1)==sy);
        engine.undo();render();
    }
    // The real edge band outside the old inset box must also be draggable.
    p=preview.selectionOutline();const QPointF nearEdge=p[0]*.89+p[1]*.05+p[3]*.06;
    drag(nearEdge,nearEdge+QPointF(18,12));
    check("drag-in-previously-dead-outer-band",current()->getParam<double>("transform.positionX",0)>90&&current()->getParam<double>("transform.positionY",0)>-30);engine.undo();render();
    p=preview.selectionOutline();const QPointF center=(p[0]+p[2])/2,top=midpoint(p[0],p[1]);
    const QPointF outward=top-center;const QPointF rotate=top+outward*(20/std::hypot(outward.x(),outward.y()));
    QTransform rotation;rotation.translate(center.x(),center.y());rotation.rotate(20);rotation.translate(-center.x(),-center.y());
    drag(rotate,rotation.map(rotate));check("rotation-handle-follows-pointer-angle",std::abs(current()->getParam<double>("transform.rotate",0)-51)<1.5);engine.undo();render();
    const auto outsideBefore=current()->params();drag(p.boundingRect().topLeft()-QPointF(12,12),p.boundingRect().topLeft()-QPointF(30,30));
    check("outside-rotated-polygon-not-draggable",current()->params()==outsideBefore);
    const auto unpanned=preview.selectionOutline();mouse(QEvent::MouseButtonPress,QPointF(350,250),Qt::MiddleButton,Qt::MiddleButton);
    mouse(QEvent::MouseMove,QPointF(373,237),Qt::NoButton,Qt::MiddleButton);mouse(QEvent::MouseButtonRelease,QPointF(373,237),Qt::MiddleButton,Qt::NoButton);
    check("pan-moves-selection-with-frame",QLineF(preview.selectionOutline()[0],unpanned[0]+QPointF(23,-13)).length()<.01);
    preview.grab().save(output+"/panned.png");
    current()->setHidden(true);check("hidden-clip-has-no-selection",preview.selectionOutline().isEmpty());current()->setHidden(false);
    auto* track=engine.activeTimeline()->findTrackContainingClip(id);track->setHidden(true);check("hidden-track-has-no-selection",preview.selectionOutline().isEmpty());track->setHidden(false);
    engine.deselectAll();check("deselected-has-no-selection",preview.selectionOutline().isEmpty());
    {
        // Exact user source: HEVC visible aperture is 2030x1080 (not its
        // 2048x1088 coded allocation). Recreate a letterboxed preview.
        const QString source=qEnvironmentVariable("CATCHIM_TEST_MEDIA","E:/1004(1).mp4");auto probe=media::MediaProbe::probe(source.toStdWString());
        check("user-video-probed",probe.ok());
        if(probe.ok()){
            editor::EditorEngine videoEngine;media::MediaLibrary videoLibrary;render::RenderEngine videoRenderer;
            const auto video=probe.unwrap();videoLibrary.addAsset(video);videoEngine.project().settings().canvasSize={2600,1440};
            editor::Clip mediaClip(core::ClipId::generate(),editor::ClipType::Video,"1004(1).mp4",core::TimelineTime(0),video->duration());
            mediaClip.setMediaId(video->id());videoEngine.addClip(videoEngine.activeTimeline()->mainTrack().id(),mediaClip);videoEngine.selectClip(mediaClip.id());
            ui::PreviewWidget videoPreview(videoEngine,videoRenderer,videoLibrary);videoPreview.resize(752,419);videoPreview.show();app.processEvents();videoPreview.updateFrameIfNeeded();
            QElapsedTimer timer;timer.start();while(!videoPreview.presentedFrames()&&timer.elapsed()<15000)wait(10);
            check("actual-hevc-rendered",videoPreview.presentedFrames()>0&&!videoPreview.presentedImage().isNull());
            const auto bounds=videoPreview.selectionOutline().boundingRect();
            check("actual-hevc-visible-aperture",video->width()==2030&&video->height()==1080&&std::abs(bounds.width()-qRound(2600.*std::min(736./2600,403./1440))*2030./2600)<.01);
            videoPreview.grab().save(output+"/user-video-selection.png");
            const auto frame=videoPreview.presentedImage();int left=frame.width(),topPixel=frame.height(),right=-1,bottom=-1;
            for(int y=0;y<frame.height();++y)for(int x=0;x<frame.width();++x){const auto color=frame.pixelColor(x,y);if(color.red()>25||color.green()>25||color.blue()>25){left=std::min(left,x);topPixel=std::min(topPixel,y);right=std::max(right,x);bottom=std::max(bottom,y);}}
            const double fit=std::min(736./2600,403./1440);const int dw=qRound(2600*fit),dh=qRound(1440*fit);
            const QRectF pixels((752-dw)/2.+left*double(dw)/frame.width(),8+(419-16-dh)/2.+topPixel*double(dh)/frame.height(),(right-left+1)*double(dw)/frame.width(),(bottom-topPixel+1)*double(dh)/frame.height());
            check("actual-hevc-box-agrees-with-visible-video-edges",right>=left&&std::abs(pixels.left()-bounds.left())<3&&std::abs(pixels.top()-bounds.top())<3&&std::abs(pixels.right()-bounds.right())<3&&std::abs(pixels.bottom()-bounds.bottom())<3);
            videoEngine.project().settings().canvasSize={2030,1080};videoPreview.resize(752,300);videoPreview.markFrameDirty();videoPreview.updateFrameIfNeeded();
            const auto full=videoPreview.selectionOutline().boundingRect();
            check("fit-keeps-bottom-handles-inside-viewport",full.bottom()+5<videoPreview.height());
        }
    }
    QFile report(output+"/checks.json");if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(QJsonObject{{"passed",passed},{"dpi",app.devicePixelRatio()},{"checks",checks}}).toJson());
    return passed?0:1;
}

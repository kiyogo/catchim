#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QProcess>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <iostream>
#include "ui/MainWindow.h"
#include "ui/panels/properties/PropertiesPanel.h"
#include "ui/panels/assets/AssetsPanel.h"
#include "ui/widgets/ScenesDrawer.h"
#include "ui/widgets/PresetInsertion.h"
#include "ui/theme/Theme.h"
#include "ui/export/ExportWorker.h"
#include "media/probe/MediaProbe.h"
#include "media/decoder/NativeAudioDecoder.h"
#include "media/decoder/NativeVideoDecoder.h"
#include "ui/widgets/FeedbackDialog.h"
#include "audio/AudioPlaybackEngine.h"
#include "editor/history/commands/SceneCommands.h"
#include "editor/clipboard/ClipboardManager.h"

using namespace catchim;
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");ui::Theme::instance().setTheme(ui::ThemeMode::Light);
    const QString output="E:/DATA/source/catchim/app/qa/native-workflow/dpi-"+QString::number(app.devicePixelRatio());
    QDir().mkpath(output);QJsonArray checks;bool passed=true;
    auto check=[&](const char* name,bool result){checks.append(QJsonObject{{"name",name},{"passed",result}});passed=passed&&result;std::cout<<(result?"PASS ":"FAIL ")<<name<<std::endl;};
    auto wait=[&](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
    {
        editor::EditorEngine engine;auto mainId=engine.project().currentSceneId();auto* original=engine.activeTimeline();
        const auto first=core::ClipId::generate();
        engine.addClip(original->mainTrack().id(),editor::Clip(first,editor::ClipType::Video,"Original",core::TimelineTime(0),core::TimelineTime::fromSeconds(2)));
        engine.history().execute(std::make_unique<editor::CreateSceneCommand>(engine.project(),"Second"));
        const auto secondId=engine.project().currentSceneId();auto* second=engine.activeTimeline();const auto clip=core::ClipId::generate();
        engine.addClip(second->mainTrack().id(),editor::Clip(clip,editor::ClipType::Video,"Second clip",core::TimelineTime(0),core::TimelineTime::fromSeconds(1)));
        engine.history().execute(std::make_unique<editor::DeleteSceneCommand>(engine.project(),secondId));
        check("undo-delete-scene",engine.undo()&&&engine.project().findScene(secondId)->timeline()==second);
        check("undo-edit-in-restored-scene",engine.undo()&&second->findClip(clip)==nullptr);
        check("undo-create-preserves-main-timeline",engine.undo()&&engine.activeTimeline()==original);
        check("undo-original-after-scene-operations",engine.undo()&&original->findClip(first)==nullptr);
        check("redo-original",engine.redo());check("redo-create",engine.redo());
        check("redo-edit-in-created-scene",engine.redo()&&engine.activeTimeline()==second&&second->findClip(clip));
        check("redo-delete-scene",engine.redo());check("restore-again",engine.undo());
        editor::Project copy=engine.project();copy.findScene(secondId)->timeline().findClip(clip)->setName("Independent");
        check("export-project-copy-independent",second->findClip(clip)->name()=="Second clip");
        engine.history().execute(std::make_unique<editor::DuplicateSceneCommand>(engine.project(),secondId));
        engine.activeTimeline()->findClip(clip)->setName("Duplicated");
        check("duplicate-scene-timeline-independent",engine.project().findScene(secondId)->timeline().findClip(clip)->name()=="Second clip");
        check("main-scene-protected",!editor::DeleteSceneCommand(engine.project(),mainId).execute());
    }
    editor::EditorEngine engine;media::MediaLibrary library;render::RenderEngine renderer(1920,1080);
    ui::MainWindow window(engine,library,renderer);window.resize(1912,938);window.show();app.processEvents();
    ui::insertUiPreset(engine,library,QJsonObject{{"type","text"},{"name","Văn bản mặc định"}},core::TimelineTime(0));
    core::ClipId textId;
    for(auto* track:engine.activeTimeline()->allTracks())for(const auto& clip:track->clips())if(clip.type()==editor::ClipType::Text)textId=clip.id();
    engine.selectClip(textId);app.processEvents();
    check("undo-insert",engine.undo()&&!engine.activeTimeline()->findClip(textId));
    check("redo-insert-retains-content",engine.redo()&&engine.activeTimeline()->findClip(textId)&&engine.activeTimeline()->findClip(textId)->getParam<std::string>("text.content","")=="Văn bản mặc định");
    engine.selectClip(textId);
    auto* rail=window.findChild<QWidget*>("propertiesTabRail");check("inspector-rail-41px",rail&&rail->width()==41);
    auto* content=window.findChild<QTextEdit*>("property_text.content");content->setPlainText("Tiếng Việt chính xác");
    check("text-edit-committed",engine.activeTimeline()->findClip(textId)->getParam<std::string>("text.content","")=="Tiếng Việt chính xác");
    check("text-edit-undo",engine.undo()&&engine.activeTimeline()->findClip(textId)->getParam<std::string>("text.content","")=="Văn bản mặc định");
    check("inspector-updates-after-undo",content->toPlainText()=="Văn bản mặc định");engine.redo();
    auto* size=window.findChild<QDoubleSpinBox*>("property_text.fontSize");size->setValue(26);
    check("font-size-committed",engine.activeTimeline()->findClip(textId)->getParam<double>("text.fontSize",0)==26);
    window.grab().save(output+"/text-inspector.png");
    ui::Theme::instance().setTheme(ui::ThemeMode::Dark);app.processEvents();
    check("inspector-dark-theme-field",size->grab().toImage().pixelColor(size->width()-20,10).lightness()<80);
    window.grab().save(output+"/text-inspector-dark.png");
    ui::Theme::instance().setTheme(ui::ThemeMode::Light);app.processEvents();
    window.findChild<QPushButton*>("propertyTab_transform")->click();
    check("text-transform-tab",window.findChild<QWidget*>("propertyPage_transform")->isVisible()&&!window.findChild<QWidget*>("propertyPage_text")->isVisible());
    window.findChild<QPushButton*>("propertyTab_text")->click();
    for(const QSize viewport:{QSize(1280,720),QSize(1912,938)}) {
        window.resize(viewport);app.processEvents();auto* panel=window.findChild<ui::PropertiesPanel*>();
        check("inspector-no-horizontal-overflow",panel->findChild<QScrollArea*>()->horizontalScrollBarPolicy()==Qt::ScrollBarAlwaysOff);
    }
    {
        ui::ScenesDrawer drawer(engine,&window);drawer.show();app.processEvents();
        check("scenes-drawer-native-384px",drawer.findChild<QWidget*>("scenesDrawerPanel")->width()==384);
        auto composed=window.grab();QPainter painter(&composed);drawer.render(&painter,QPoint());painter.end();composed.save(output+"/scenes-drawer.png");
        drawer.findChild<QPushButton*>("scenesSelect")->click();
        check("scene-select-mode",drawer.findChild<QPushButton*>("scenesDelete")->isVisible());
        drawer.reject();
    }
    {
        ui::FeedbackDialog dialog(&window);dialog.show();app.processEvents();
        auto composed=window.grab();QPainter painter(&composed);dialog.render(&painter,window.mapFromGlobal(dialog.pos()));painter.end();composed.save(output+"/feedback-dialog.png");
        check("feedback-card-384px",dialog.findChild<QWidget*>("feedbackCard")->width()==384);
        dialog.reject();
    }
    {
        // Exercise the real native HTTP client against an in-memory loopback
        // server. No feedback is sent to the user's backend or an external site.
        QTcpServer server;check("feedback-test-server",server.listen(QHostAddress::LocalHost,0));
        const QByteArray previous=qgetenv("CATCHIM_FEEDBACK_URL");
        qputenv("CATCHIM_FEEDBACK_URL",QString("http://127.0.0.1:%1/api/feedback").arg(server.serverPort()).toUtf8());
        bool received=false;
        QObject::connect(&server,&QTcpServer::newConnection,&server,[&]{
            auto* socket=server.nextPendingConnection();
            QObject::connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{
                auto data=socket->property("request").toByteArray()+socket->readAll();socket->setProperty("request",data);
                const int split=data.indexOf("\r\n\r\n");if(split<0)return;
                int length=0;for(const auto& line:data.left(split).split('\n'))if(line.toLower().startsWith("content-length:"))length=line.mid(15).trimmed().toInt();
                if(data.size()<split+4+length)return;
                const auto body=QJsonDocument::fromJson(data.mid(split+4,length)).object();
                received=data.startsWith("POST /api/feedback ")&&body.value("message").toString()=="Kiểm tra phản hồi native";
                const auto response=QJsonDocument(QJsonObject{{"entry",QJsonObject{{"id","local-regression"},{"message",body.value("message")},{"createdAt","2026-10-18T06:00:00.000Z"}}}}).toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 201 Created\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(response.size())+"\r\nConnection: close\r\n\r\n"+response);socket->disconnectFromHost();
            });
            QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        });
        ui::FeedbackDialog dialog(&window);
        dialog.findChild<QTextEdit*>("feedbackMessage")->setPlainText("Kiểm tra phản hồi native");
        QTimer::singleShot(0,&dialog,[&]{dialog.findChild<QPushButton*>("feedbackSend")->click();});
        QTimer timeout;timeout.setSingleShot(true);QObject::connect(&timeout,&QTimer::timeout,&dialog,&QDialog::reject);timeout.start(16000);
        check("feedback-native-post-and-ack",dialog.exec()==QDialog::Accepted&&received);
        if(previous.isNull())qunsetenv("CATCHIM_FEEDBACK_URL");else qputenv("CATCHIM_FEEDBACK_URL",previous);
    }
    engine.newProject("Media workflow");
    {
        const auto before=engine.activeTimeline()->allTracks().size();
        editor::Clip extra(core::ClipId::generate(),editor::ClipType::Text,"New layer",core::TimelineTime(0),core::TimelineTime::fromSeconds(1));
        const auto id=extra.id();engine.insertElement(extra,core::TimelineTime(0),std::nullopt,true);
        const auto createdTrackId=engine.activeTimeline()->findTrackContainingClip(id)->id();
        check("new-layer-adds-track",engine.activeTimeline()->allTracks().size()==before+1);
        check("new-layer-undo-removes-track",engine.undo()&&engine.activeTimeline()->allTracks().size()==before);
        check("new-layer-redo-retains-clip",engine.redo()&&engine.activeTimeline()->findClip(id)&&engine.activeTimeline()->findClip(id)->name()=="New layer");
        check("new-layer-redo-stable-track-id",engine.activeTimeline()->findTrackContainingClip(id)->id()==createdTrackId);
        engine.newProject("Media workflow");
    }
    engine.project().settings().canvasSize={320,180};
    engine.project().settings().fps={10,1};
    const QString ffmpeg=QStandardPaths::findExecutable("ffmpeg"),ffprobe=QStandardPaths::findExecutable("ffprobe");
    check("ffmpeg-available",!ffmpeg.isEmpty()&&!ffprobe.isEmpty());
    const QString fixture=output+"/fixture.mp4";
    QProcess generate;generate.start(ffmpeg,{"-y","-f","lavfi","-i","testsrc2=size=320x180:rate=10:duration=3","-f","lavfi","-i","sine=frequency=330:sample_rate=48000:duration=3","-c:v","libx264","-pix_fmt","yuv420p","-c:a","aac","-shortest",fixture});
    check("fixture-generated",generate.waitForFinished(20000)&&generate.exitCode()==0);
    auto probe=media::MediaProbe::probe(fixture.toStdWString());check("real-media-probe",probe.ok());
    if(probe.ok()) {
        auto asset=probe.unwrap();library.addAsset(asset);const auto id=core::ClipId::generate();
        for(double sampleTime:{1.0,.9}) {
            const auto reference=output+QString("/decode-reference-%1.png").arg(sampleTime);
            QProcess decode;decode.start(ffmpeg,{"-y","-ss",QString::number(sampleTime),"-i",fixture,"-frames:v","1",reference});decode.waitForFinished(5000);
            int w=0,h=0;std::vector<uint8_t> rgba;
            const bool decoded=media::NativeVideoDecoder::instance().getFrame(fixture.toStdWString(),sampleTime,w,h,rgba,"regression");
            double error=1e9;QImage expected(reference);
            if(decoded&&expected.width()==w&&expected.height()==h) {
                expected=expected.convertToFormat(QImage::Format_RGBA8888);error=0;
                for(int y=0;y<h;++y)for(int x=0;x<w;++x)for(int c=0;c<3;++c)error+=std::abs(int(rgba[(y*w+x)*4+c])-int(expected.constScanLine(y)[x*4+c]));
                error/=w*h*3.;
                QImage(rgba.data(),w,h,w*4,QImage::Format_RGBA8888).save(output+QString("/native-decode-%1.png").arg(sampleTime));
            }
            std::cout<<"Frame RGB mean error: "<<error<<std::endl;
            check(sampleTime==1.0?"native-decoder-matches-reference":"native-decoder-small-backward-seek",error<8.);
        }
        editor::Clip clip(id,editor::ClipType::Video,"fixture.mp4",core::TimelineTime::fromSeconds(.5),core::TimelineTime::fromSeconds(2));
        clip.setMediaId(asset->id());clip.setSourceDuration(asset->duration());clip.setTrimStart(core::TimelineTime::fromSeconds(.2));
        engine.addClip(engine.activeTimeline()->mainTrack().id(),std::move(clip));engine.selectClip(id);
        engine.playback().seek(core::TimelineTime::fromSeconds(1));engine.notifyProjectChanged();
        window.findChild<QPushButton*>("propertyTab_audio")->click();
        auto* volume=window.findChild<QDoubleSpinBox*>("property_volume");volume->setValue(1.);
        auto* current=engine.activeTimeline()->findClip(id);
        check("one-decibel-not-linear-unity",std::abs(audio::AudioPlaybackEngine::resolveEffectiveAudioGain(*current,false,core::TimelineTime(0))-std::pow(10.,1./20))<.0001);
        auto* mute=window.findChild<QCheckBox*>("property_muted");mute->click();
        check("mute-affects-audio",engine.activeTimeline()->findClip(id)->isMuted());
        check("undo-mute",engine.undo()&&!engine.activeTimeline()->findClip(id)->isMuted()&&!mute->isChecked());
        wait(4000);window.grab().save(output+"/video-audio-inspector.png");
        auto* tracks=window.findChild<ui::TimelineTracksWidget*>();tracks->grab().save(output+"/video-timeline.png");
        engine.setSnappingEnabled(false);
        const auto beforeDrag=engine.history().undoIndex();const auto originalStart=engine.activeTimeline()->findClip(id)->startTime();
        auto pointer=[&](QEvent::Type type,QPoint point,Qt::MouseButton button,Qt::MouseButtons buttons) {
            QMouseEvent event(type,QPointF(point),QPointF(tracks->mapToGlobal(point)),button,buttons,Qt::NoModifier);QApplication::sendEvent(tracks,&event);
        };
        const int dragX=tracks->viewportXAtTime(originalStart+core::TimelineTime::fromSeconds(.5));
        pointer(QEvent::MouseButtonPress,{dragX,30},Qt::LeftButton,Qt::LeftButton);
        pointer(QEvent::MouseMove,{dragX+40,30},Qt::NoButton,Qt::LeftButton);
        pointer(QEvent::MouseMove,{dragX+80,30},Qt::NoButton,Qt::LeftButton);
        pointer(QEvent::MouseButtonRelease,{dragX+80,30},Qt::LeftButton,Qt::NoButton);
        check("drag-is-single-undo-step",engine.history().undoIndex()==beforeDrag+1&&engine.activeTimeline()->findClip(id)->startTime()>originalStart);
        check("drag-undo-restores-position",engine.undo()&&engine.activeTimeline()->findClip(id)->startTime()==originalStart);
        auto context=[&](const QString& prefix) {
            QTimer::singleShot(0,&window,[&]{
                auto* menu=tracks->findChild<QMenu*>("timelineContextMenu");if(!menu)return;
                menu->grab().save(output+"/clip-context-menu.png");
                for(auto* action:menu->actions())if(action->text().startsWith(prefix)){
                    const auto point=menu->actionGeometry(action).center();const auto global=menu->mapToGlobal(point);
                    QMouseEvent press(QEvent::MouseButtonPress,QPointF(point),QPointF(global),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(menu,&press);
                    QMouseEvent release(QEvent::MouseButtonRelease,QPointF(point),QPointF(global),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(menu,&release);return;
                }
                menu->close();
            });
            const QPoint point(tracks->viewportXAtTime(engine.activeTimeline()->findClip(id)->startTime()+core::TimelineTime::fromSeconds(.5)),30);QContextMenuEvent event(QContextMenuEvent::Mouse,point,tracks->mapToGlobal(point));QApplication::sendEvent(tracks,&event);
        };
        context("Sao chép");check("context-copy-real-clipboard",editor::ClipboardManager::instance().hasData());
        context("Tắt tiếng");check("context-mute",engine.activeTimeline()->findClip(id)->isMuted());engine.undo();
        context("Ẩn");check("context-hide",engine.activeTimeline()->findClip(id)->isHidden());
        const auto& hiddenFrame=renderer.renderFrame(engine.project(),library,core::TimelineTime::fromSeconds(1));
        bool black=true;for(size_t i=0;i<hiddenFrame.rgbaPixels.size();i+=4)if(hiddenFrame.rgbaPixels[i]||hiddenFrame.rgbaPixels[i+1]||hiddenFrame.rgbaPixels[i+2]){black=false;break;}
        check("hidden-clip-not-rendered",black);engine.undo();
        auto exportAndCheck=[&](bool muted,const QString& name) {
            engine.activeTimeline()->findClip(id)->setMuted(muted);
            const QString path=output+"/"+name+".mp4";
            const int exportW=muted?320:640,exportH=muted?180:360;
            QStringList args={"-y","-f","rawvideo","-vcodec","rawvideo","-s",QString("%1x%2").arg(exportW).arg(exportH),"-pix_fmt","rgba","-r","10","-i","-","-c:v","libx264","-pix_fmt","yuv420p",path};
            ui::ExportWorker worker(engine.project(),library,exportW,exportH,10,30,ffmpeg,args,path,nullptr,true);
            bool success=false;QEventLoop loop;
            QObject::connect(&worker,&ui::ExportWorker::exportFinished,&loop,[&](bool ok,const QString&){success=ok;loop.quit();});
            QTimer timeout;timeout.setSingleShot(true);QObject::connect(&timeout,&QTimer::timeout,&loop,[&]{worker.cancel();loop.quit();});timeout.start(30000);
            worker.start();loop.exec();worker.wait();check(muted?"muted-export-file":"mixed-export-file",success&&QFileInfo(path).size()>1000);
            auto decoded=media::NativeAudioDecoder::instance().getAudioBuffer(path.toStdWString(),48000,2);
            auto mean=[&](double start,double end){double sum=0;int count=0;if(decoded)for(size_t i=static_cast<size_t>(start*96000);i<std::min(decoded->size(),static_cast<size_t>(end*96000));++i){sum+=std::abs((*decoded)[i]);++count;}return count?sum/count:0.;};
            check(muted?"muted-export-silent":"timeline-export-silence-before-clip",decoded&&mean(.05,.25)<.0001);
            check(muted?"muted-stays-silent-mid-clip":"timeline-export-audible-at-clip",muted?mean(.8,1.2)<.0001:mean(.8,1.2)>.01);
            check("timeline-export-silence-after-clip",mean(2.7,2.9)<.0001);
            QProcess inspect;inspect.start(ffprobe,{"-v","error","-show_entries","format=duration:stream=width,height,codec_type","-of","json",path});inspect.waitForFinished(5000);
            const auto metadata=inspect.readAllStandardOutput();
            QFile report(output+"/"+name+"-ffprobe.json");if(report.open(QIODevice::WriteOnly))report.write(metadata);
            check("export-duration-preserved",std::abs(QJsonDocument::fromJson(metadata).object().value("format").toObject().value("duration").toString().toDouble()-3.)<.05);
            int width=0,height=0;std::vector<uint8_t> pixels;
            const bool frameDecoded=media::NativeVideoDecoder::instance().getFrame(path.toStdWString(),1,width,height,pixels,"export-regression");
            check("export-resolution-correct",frameDecoded&&width==exportW&&height==exportH);
            const size_t edge=(static_cast<size_t>(height/2)*width+10)*4;
            check("export-media-fills-scaled-canvas",frameDecoded&&pixels.size()>edge+2&&pixels[edge]>100&&pixels[edge+1]<60&&pixels[edge+2]<60);
        };
        exportAndCheck(false,"mixed-export");exportAndCheck(true,"muted-export");
        {
            const auto path=output+"/cancelled-export.mp4";
            QStringList args={"-y","-f","rawvideo","-s","320x180","-pix_fmt","rgba","-r","10","-i","-","-an","-c:v","libx264",path};
            ui::ExportWorker worker(engine.project(),library,320,180,10,600,ffmpeg,args,path);
            QEventLoop loop;bool success=true;
            QObject::connect(&worker,&QThread::started,&loop,[&]{worker.cancel();});
            QObject::connect(&worker,&ui::ExportWorker::exportFinished,&loop,[&](bool ok,const QString&){success=ok;loop.quit();});
            worker.start();loop.exec();worker.wait();check("cancelled-export-never-reports-success",!success);
        }
        engine.activeTimeline()->findClip(id)->setMuted(false);
        media::MediaLibrary missing;audio::AudioPlaybackEngine mixer;const auto silence=mixer.renderAudioSlice(*engine.activeTimeline(),core::TimelineTime::fromSeconds(.8),core::TimelineTime::fromSeconds(.2),48000,&missing);
        check("missing-media-never-beeps",std::all_of(silence.samples().begin(),silence.samples().end(),[](float value){return value==0;}));
    }
    QFile report(output+"/checks.json");if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(QJsonObject{{"passed",passed},{"checks",checks}}).toJson());
    return passed?0:1;
}

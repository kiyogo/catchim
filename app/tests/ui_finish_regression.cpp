#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QScrollBar>
#include <QSpinBox>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QColorDialog>
#include <QFileDialog>
#include <QProgressBar>
#include <iostream>
#include "ui/MainWindow.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PresetInsertion.h"
#include "ui/widgets/WebComboBox.h"
#include "ui/widgets/ColorPickerDialog.h"
#include "ui/widgets/EditorLanguage.h"
#include "ui/shell/ShortcutsDialog.h"
#include "ui/export/ExportDialog.h"
#include "ui/export/ExportWorker.h"
#include "media/probe/MediaProbe.h"
#include "media/decoder/NativeAudioDecoder.h"
using namespace catchim;
int main(int argc,char** argv){
    QApplication app(argc,argv);app.setStyle("Fusion");ui::Theme::instance().setTheme(ui::ThemeMode::Light);
    const auto previousLanguage=QSettings().value("ui/language","vi").toString();ui::setEditorLanguage("vi");
    const QString output="E:/DATA/source/catchim/app/qa/ui-finish/dpi-"+QString::number(app.devicePixelRatio());QDir().mkpath(output);
    bool passed=true;QJsonArray checks;QJsonObject metrics;
    auto check=[&](const QString& name,bool ok){passed&=ok;checks.append(QJsonObject{{"name",name},{"passed",ok}});std::cout<<(ok?"PASS ":"FAIL ")<<name.toStdString()<<std::endl;};
    auto wait=[&](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
    auto until=[&](auto condition,int ms){QElapsedTimer time;time.start();while(!condition()&&time.elapsed()<ms)wait(10);return condition();};
    editor::EditorEngine engine;media::MediaLibrary library;render::RenderEngine renderer;
    {
        QWidget controls;controls.setFixedSize(180,128);auto* layout=new QHBoxLayout(&controls);
        auto* spin=new QSpinBox;spin->setRange(0,255);spin->setValue(255);spin->setFixedSize(100,32);
        auto* bar=new QScrollBar(Qt::Vertical);bar->setRange(0,100);bar->setValue(40);bar->setFixedHeight(104);
        layout->addWidget(spin);layout->addWidget(bar);controls.show();app.processEvents();
        const auto style=app.styleSheet();
        check("spin-arrows-use-svg",QFile::exists(":/icons/chevron-up.svg")&&QFile::exists(":/icons/chevron-down.svg")&&style.contains("image: url(\":/icons/chevron-up.svg\")")&&!style.contains("border-bottom: 5px solid"));
        check("scrollbar-native-border-removed",style.contains("QScrollBar::groove:vertical, QScrollBar::groove:horizontal")&&style.contains("QAbstractScrollArea::corner"));
        controls.grab().save(output+"/spin-scroll-controls.png");
    }
    ui::MainWindow window(engine,library,renderer);window.resize(1280,720);window.show();app.processEvents();
    ui::insertUiPreset(engine,library,QJsonObject{{"type","text"},{"name","Văn bản mặc định"}},core::TimelineTime(0));
    for(auto* track:engine.activeTimeline()->allTracks())for(const auto& clip:track->clips())if(clip.type()==editor::ClipType::Text)engine.selectClip(clip.id());
    auto* inspector=window.findChild<ui::PropertiesPanel*>();
    auto* scroll=inspector->findChild<QScrollArea*>();
    check("preview-has-no-obstructing-tooltip",window.findChild<QPushButton*>("previewPlayBtn")->toolTip().isEmpty());
    auto* blendButton=window.findChild<QPushButton*>("propertyTab_blending");
    blendButton->setAttribute(Qt::WA_UnderMouse,true);
    QEnterEvent enter(QPointF(10,10),QPointF(10,10),QPointF(blendButton->mapToGlobal(QPoint(10,10))));QApplication::sendEvent(blendButton,&enter);app.processEvents();
    QWidget* hint=nullptr;for(auto* widget:QApplication::topLevelWidgets())if(widget->objectName()=="editorSideHint"&&widget->isVisible())hint=widget;
    // Tooltip is immediate, and its bubble sits to the left of the inspector rail.
    check("inspector-hint-opens-immediately-on-left",hint&&hint->geometry().center().x()<blendButton->mapToGlobal(QPoint(0,0)).x());
    if(hint)hint->grab().save(output+"/inspector-hint.png");
    bool labelsClean=true;
    for(auto* button:window.findChild<QWidget*>("timelineToolbar")->findChildren<QPushButton*>())
        labelsClean&=!button->toolTip().contains('(')&&!button->toolTip().contains(')');
    check("timeline-tooltip-labels-have-no-parenthetical-hints",labelsClean);
    QEvent leave(QEvent::Leave);QApplication::sendEvent(blendButton,&leave);blendButton->setAttribute(Qt::WA_UnderMouse,false);
    {
        engine.seek(core::TimelineTime(0));
        ui::TimelineRulerWidget ruler(engine);ruler.setZoomFactor(1);ruler.resize(480,ruler.height());ruler.show();app.processEvents();
        const auto rulerPixmap=ruler.grab();const auto rulerImage=rulerPixmap.toImage();const auto ratio=rulerPixmap.devicePixelRatio();
        const auto accent=ui::Theme::instance().palette().primaryAccent;
        check("playhead-full-left-half-visible-at-zero",rulerImage.pixelColor(qRound(108*ratio),qRound(10*ratio))==accent);
        const auto tracksPixmap=window.findChild<ui::TimelineTracksWidget*>()->grab();const auto tracksImage=tracksPixmap.toImage();
        auto lineWidth=[&](const QImage& image,double dpr,int y){int count=0;const int x=qRound(112*dpr);for(int dx=-4;dx<=4;++dx)count+=image.pixelColor(x+dx,qRound(y*dpr))==accent;return count;};
        check("playhead-ruler-and-track-line-width-match",lineWidth(rulerImage,ratio,25)>0&&lineWidth(rulerImage,ratio,25)==lineWidth(tracksImage,tracksPixmap.devicePixelRatio(),tracksPixmap.height()/tracksPixmap.devicePixelRatio()-8));
        rulerPixmap.save(output+"/playhead-zero-full-head.png");
        core::TimelineTime dragged;
        QObject::connect(&ruler,&ui::TimelineRulerWidget::seekRequested,&ruler,[&](core::TimelineTime time){dragged=time;});
        const QPoint start(108,10),end(138,10);
        QMouseEvent press(QEvent::MouseButtonPress,QPointF(start),QPointF(ruler.mapToGlobal(start)),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&ruler,&press);
        QMouseEvent move(QEvent::MouseMove,QPointF(end),QPointF(ruler.mapToGlobal(end)),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&ruler,&move);
        QMouseEvent release(QEvent::MouseButtonRelease,QPointF(end),QPointF(ruler.mapToGlobal(end)),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(&ruler,&release);
        check("playhead-left-half-drags-without-jump",std::abs(dragged.toSeconds()-.6)<.001);
    }
    for(const auto size:{QSize(1280,720),QSize(1912,938)}){
        window.resize(size);app.processEvents();
        check(QString("inspector-no-overflow-%1").arg(size.width()),scroll->widget()->width()<=scroll->viewport()->width());
        for(const QString name:{"property_text.fontSize","property_text.fontWeight","property_text.fontFamily","property_text.content"}){
            auto* field=window.findChild<QWidget*>(name);const auto pos=field->mapTo(scroll->viewport(),QPoint());
            check(QString("field-not-covered-%1-%2").arg(size.width()).arg(name),pos.x()>=0&&pos.x()+field->width()<=scroll->viewport()->width());
        }
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());window.grab().save(output+QString("/inspector-%1-bottom.png").arg(size.width()));
        scroll->verticalScrollBar()->setValue(0);
    }
    auto* tabs=window.findChild<QListWidget*>("tabList");tabs->setCurrentRow(8);app.processEvents();
    auto* language=dynamic_cast<ui::WebComboBox*>(window.findChild<QComboBox*>("settingsLanguage"));
    check("english-option-present",language&&language->findData("en")>=0);
    language->showPopup();app.processEvents();auto* popup=language->popupWidget();
    auto* list=popup->findChild<QListView*>();
    check("language-popup-compact",popup->height()<=76&&list->visualRect(list->model()->index(1,0)).bottom()<list->viewport()->height()+1);
    check("dropdown-hides-scrollbar-when-full-content-fits",list->verticalScrollBarPolicy()==Qt::ScrollBarAlwaysOff);
    popup->grab().save(output+"/language-popup.png");language->hidePopup();
    language->setCurrentIndex(language->findData("en"));app.processEvents();
    bool englishCaption=false;for(auto* label:window.findChildren<QLabel*>())englishCaption|=label->text()=="Language";
    check("english-switch-translates-ui",ui::editorEnglish()&&englishCaption);
    window.grab().save(output+"/settings-english.png");language->setCurrentIndex(language->findData("vi"));app.processEvents();
    {ui::ShortcutsDialog dialog(&window);dialog.show();app.processEvents();check("shortcuts-single-native-card",dialog.windowFlags().testFlag(Qt::FramelessWindowHint)&&dialog.findChild<QWidget*>("appDialogCard"));dialog.grab().save(output+"/shortcuts.png");dialog.reject();}
    QTimer::singleShot(0,&window,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());check("color-dialog-themed-card",dialog&&dialog->findChild<QWidget*>("appDialogCard")&&dialog->findChild<QWidget*>("editorColorPlane"));if(dialog){dialog->grab().save(output+"/color-picker.png");dialog->reject();}});
    ui::chooseEditorColor(Qt::white,&window);
    QTimer::singleShot(0,&window,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(dialog){auto* hex=dialog->findChild<QLineEdit*>("editorColorHex");QMetaObject::invokeMethod(hex,"textEdited",Q_ARG(QString,QString("#123456")));dialog->accept();}});
    check("color-picker-applies-entered-hex",ui::chooseEditorColor(Qt::white,&window)==QColor("#123456"));
    QTimer::singleShot(0,&window,[&]{auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());check("canvas-size-themed-card",dialog&&dialog->findChild<QWidget*>("appDialogCard"));if(dialog){dialog->grab().save(output+"/canvas-size.png");dialog->reject();}});
    window.findChild<QPushButton*>("customCanvasButton")->click();
    {ui::ExportDialog dialog(engine,library,renderer,&window);dialog.show();app.processEvents();
        check("export-themed-card",dialog.findChild<QWidget*>("exportCard")&&dialog.windowFlags().testFlag(Qt::FramelessWindowHint));
        for(const QString name:{"exportResolution","exportFormat","exportQuality","exportAudio"}){
            auto* combo=dynamic_cast<ui::WebComboBox*>(dialog.findChild<QComboBox*>(name));check(name+"-complete-control",combo&&combo->height()==32&&combo->rect().contains(QPoint(combo->width()-2,combo->height()-2)));
            combo->showPopup();app.processEvents();combo->popupWidget()->grab().save(output+"/"+name+".png");combo->hidePopup();
        }
        dialog.grab().save(output+"/export-dialog.png");dialog.reject();
    }
    engine.newProject("Filmstrip");auto probe=media::MediaProbe::probe(L"E:/1004(1).mp4");check("source-probed",probe.ok());
    if(probe.ok()){
        const auto asset=probe.unwrap();library.addAsset(asset);engine.project().settings().canvasSize={asset->width(),asset->height()};
        editor::Clip clip(core::ClipId::generate(),editor::ClipType::Video,"1004(1).mp4",core::TimelineTime(0),core::TimelineTime::fromSeconds(8));
        clip.setMediaId(asset->id());clip.setSourceDuration(asset->duration());clip.setTrimStart(core::TimelineTime::fromSeconds(5));const auto id=clip.id();
        engine.addClip(engine.activeTimeline()->mainTrack().id(),clip);engine.selectClip(id);tabs->setCurrentRow(0);engine.notifyProjectChanged();
        auto* tracks=window.findChild<ui::TimelineTracksWidget*>();
        QElapsedTimer cold;cold.start();
        const auto coverFilled=[&]{const auto pixmap=tracks->grab();const auto image=pixmap.toImage();const double ratio=pixmap.devicePixelRatio();
            const int begin=tracks->viewportXAtTime(core::TimelineTime(0)),end=tracks->viewportXAtTime(core::TimelineTime::fromSeconds(8));
            const auto neutral=ui::Theme::instance().palette().secondary;int filled=0;
            for(int i=1;i<=8;++i){const int x=begin+(end-begin)*i/9;int distinct=0;
                for(int y=20;y<65;y+=5){const auto color=image.pixelColor(qRound(x*ratio),qRound(y*ratio));distinct+=(std::abs(color.red()-neutral.red())+std::abs(color.green()-neutral.green())+std::abs(color.blue()-neutral.blue()))>60;}
                filled+=distinct>=3;}
            return filled==8;};
        check("trimmed-video-entire-filmstrip-fills",until(coverFilled,8000));metrics["filmstripColdMs"]=static_cast<double>(cold.elapsed());
        tracks->grab().save(output+"/trimmed-filmstrip.png");
        // Compare actual export paths for the same source, settings and frame
        // range. The rendered path remains required for overlays/effects.
        const QString ffmpeg=QStandardPaths::findExecutable("ffmpeg");check("ffmpeg-present",!ffmpeg.isEmpty());
        if(!ffmpeg.isEmpty()&&!app.arguments().contains("--skip-export")){
            auto exportVideo=[&](bool direct,bool audio){
                const QString name=direct?(audio?"direct-audio":"direct-muted"):"native-compositor";
                const QString path=output+"/"+name+".mp4";const int w=1920,h=1022,frames=120;
                QStringList args={"-y","-filter_threads","2","-f","rawvideo","-s",QString("%1x%2").arg(w).arg(h),"-pix_fmt","rgba","-r","30","-i","-","-c:v","h264_amf","-quality","speed","-b:v","8M","-pix_fmt","yuv420p",path};
                if(!direct)qputenv("CATCHIM_DISABLE_DIRECT_EXPORT","1");else qunsetenv("CATCHIM_DISABLE_DIRECT_EXPORT");
                ui::ExportWorker worker(engine.project(),library,w,h,30,frames,ffmpeg,args,path,nullptr,audio);
                bool success=false,usedDirect=false;QString log;QEventLoop loop;QElapsedTimer timer;timer.start();
                QObject::connect(&worker,&ui::ExportWorker::logOutputReceived,&loop,[&](const QString& text){log+=text;usedDirect|=text.contains("direct source");});
                QObject::connect(&worker,&ui::ExportWorker::exportFinished,&loop,[&](bool ok,const QString& text){success=ok;if(!ok)std::cout<<text.toStdString()<<std::endl;loop.quit();});
                QTimer timeout;timeout.setSingleShot(true);QObject::connect(&timeout,&QTimer::timeout,&loop,[&]{worker.cancel();loop.quit();});timeout.start(60000);
                worker.start();loop.exec();worker.wait();metrics[name+"Ms"]=static_cast<double>(timer.elapsed());
                check(name+"-succeeds",success&&QFileInfo(path).size()>1000);check(name+"-correct-pipeline",usedDirect==direct);
                QFile logs(output+"/"+name+".log");if(logs.open(QIODevice::WriteOnly))logs.write(log.toUtf8());
                const auto metadata=media::MediaProbe::probe(path.toStdWString());check(name+"-output-geometry",metadata.ok());
                if(metadata.ok()){auto copy=metadata;auto out=copy.unwrap();check(name+"-dimensions-and-duration",out->width()==w&&out->height()==h&&std::abs(out->duration().toSeconds()-4.)<.12);check(name+"-audio-setting",out->hasAudio()==audio);}
                return path;
            };
            const auto original=engine.activeTimeline()->findClip(id)->duration();engine.activeTimeline()->findClip(id)->setDuration(core::TimelineTime::fromSeconds(4));
            exportVideo(false,true);exportVideo(true,true);exportVideo(true,false);qunsetenv("CATCHIM_DISABLE_DIRECT_EXPORT");
            // Exercise the actual export form and button, including its save
            // dialog and option-to-encoder mapping, for both supported formats.
            engine.activeTimeline()->findClip(id)->setDuration(core::TimelineTime::fromSeconds(2));
            for(const QString format:{"mp4","webm"}){
                ui::ExportDialog dialog(engine,library,renderer,&window);dialog.show();app.processEvents();
                const bool withAudio=format=="webm";const QString resolution=withAudio?"720p":"480p";
                for(const auto& choice:std::vector<std::pair<QString,QString>>{{"exportResolution",resolution},{"exportFormat",format},{"exportQuality",withAudio?"medium":"low"},{"exportAudio",withAudio?"include":"mute"}}){
                    auto* combo=dialog.findChild<QComboBox*>(choice.first);combo->setCurrentIndex(combo->findData(choice.second));
                }
                const QString path=output+"/form-"+format+"."+format;
                QTimer::singleShot(0,&dialog,[&]{auto* save=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());check(format+"-save-dialog-opens",save!=nullptr);if(save){save->selectFile(path);QMetaObject::invokeMethod(save,"accept");}});
                dialog.findChild<QPushButton*>("startExportButton")->click();
                app.processEvents();dialog.grab().save(output+"/export-progress-"+format+".png");
                check(format+"-form-export-finishes",until([&]{return dialog.findChild<QPushButton*>("startExportButton")->isEnabled();},60000)&&dialog.findChild<QProgressBar*>()->value()==100);
                QProcess inspect;inspect.start(QStandardPaths::findExecutable("ffprobe"),{"-v","error","-show_entries","stream=codec_name,codec_type,width,height:format=duration","-of","json",path});inspect.waitForFinished(5000);
                const auto document=QJsonDocument::fromJson(inspect.readAllStandardOutput()).object();bool audioFound=false,videoFound=false;
                const int height=withAudio?720:480,width=qRound((height*2030./1080)/2)*2;
                for(const auto& value:document.value("streams").toArray()){const auto stream=value.toObject();audioFound|=stream.value("codec_type").toString()=="audio";videoFound|=stream.value("codec_type").toString()=="video"&&stream.value("width").toInt()==width&&stream.value("height").toInt()==height&&stream.value("codec_name").toString()==(withAudio?"vp9":"h264");}
                check(format+"-form-resolution-format-audio-correct",videoFound&&audioFound==withAudio&&std::abs(document.value("format").toObject().value("duration").toString().toDouble()-2.)<.15);
                dialog.reject();
            }
            engine.activeTimeline()->findClip(id)->setDuration(original);
        }
    }
    ui::setEditorLanguage(previousLanguage);
    QFile file(output+"/checks.json");if(file.open(QIODevice::WriteOnly)){metrics["passed"]=passed;metrics["checks"]=checks;file.write(QJsonDocument(metrics).toJson());}
    std::cout<<QJsonDocument(metrics).toJson().toStdString();return passed?0:1;
}

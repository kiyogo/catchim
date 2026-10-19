#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QSlider>
#include <QScrollBar>
#include <QMouseEvent>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QHelpEvent>
#include <QToolTip>
#include <QSplitterHandle>
#include <iostream>
#include <cmath>
#include <algorithm>
#include "ui/MainWindow.h"
#include "ui/theme/Theme.h"
#include "media/probe/MediaProbe.h"
#include "media/decoder/NativeVideoDecoder.h"
#include "media/decoder/NativeAudioDecoder.h"
#include "media/probe/Mp4AudioTiming.h"
#include "audio/PlaybackAudioSession.h"
using namespace catchim;
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");ui::Theme::instance().setTheme(ui::ThemeMode::Light);
    const QString source=qEnvironmentVariable("CATCHIM_TEST_MEDIA","E:/1004(1).mp4");
    const QString output=qEnvironmentVariable("CATCHIM_TEST_OUTPUT","E:/DATA/source/catchim/app/qa/gpu-playback-1004");QDir().mkpath(output);
    QJsonArray checks;QJsonObject metrics;bool passed=true;
    auto check=[&](const char* name,bool ok){checks.append(QJsonObject{{"name",name},{"passed",ok}});passed&=ok;std::cout<<(ok?"PASS ":"FAIL ")<<name<<std::endl;};
    auto wait=[&](int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();};
    auto until=[&](auto predicate,int timeout){QElapsedTimer elapsed;elapsed.start();while(!predicate()&&elapsed.elapsed()<timeout)wait(10);return predicate();};
    auto probe=media::MediaProbe::probe(source.toStdWString());check("exact-user-file-probed",probe.ok());if(!probe.ok())return 1;
    auto asset=probe.unwrap();metrics["source"]=source;metrics["duration"]=asset->duration().toSeconds();
    const auto timing=media::readMp4AudioTiming(source.toStdWString());metrics["aacPrimingSeconds"]=timing.mediaStartSeconds;
    editor::EditorEngine engine;media::MediaLibrary library;render::RenderEngine renderer;
    engine.project().settings().canvasSize={asset->width(),asset->height()};engine.project().settings().fps=asset->fps();
    ui::MainWindow window(engine,library,renderer);window.setPlaybackOutputVolume(0);window.resize(1912,938);window.show();wait(100);
    library.addAsset(asset);editor::Clip clip(core::ClipId::generate(),editor::ClipType::Video,asset->fileName(),core::TimelineTime(0),asset->duration());
    clip.setMediaId(asset->id());clip.setSourceDuration(asset->duration());clip.setParam("hasAudio",asset->hasAudio());const auto clipId=clip.id();
    engine.addClip(engine.activeTimeline()->mainTrack().id(),std::move(clip));engine.notifyProjectChanged();
    auto* timeline=window.findChild<ui::TimelinePanel*>();auto* tracks=window.findChild<ui::TimelineTracksWidget*>();
    auto* preview=window.findChild<ui::PreviewWidget*>();auto* slider=window.findChild<QSlider*>("timelineZoomSlider");
    auto* bar=window.findChild<QScrollBar*>("timelineHorizontalScroll");auto* ruler=window.findChild<ui::TimelineRulerWidget*>();
    auto* assets=window.findChild<QListWidget*>("mediaListWidget");
    metrics["mediaListFrameWidth"]=assets?assets->frameWidth():-1;
    // QSS padding contributes to QFrame::frameWidth even with border:none.
    // Compare the composed edge against its adjacent empty surface instead.
    int borderMismatches=0;
    if(assets) {
        const auto composed=assets->parentWidget()->grab();const auto image=composed.toImage();const auto ratio=composed.devicePixelRatio();
        auto pixel=[&](QPoint point){point=assets->mapTo(assets->parentWidget(),point);return image.pixelColor(qRound(point.x()*ratio),qRound(point.y()*ratio));};
        for(int y:{100,120,180}) {
            borderMismatches+=pixel(QPoint(0,y))!=pixel(QPoint(12,y));
            borderMismatches+=pixel(QPoint(assets->width()-1,y))!=pixel(QPoint(assets->width()-12,y));
        }
        borderMismatches+=pixel(QPoint(assets->width()/2,0))!=pixel(QPoint(assets->width()/2,12));
        borderMismatches+=pixel(QPoint(assets->width()/2,assets->height()-1))!=pixel(QPoint(assets->width()/2,assets->height()-12));
    }
    metrics["mediaInnerBorderPixelMismatches"]=borderMismatches;
    check("assets-have-no-inner-frame",assets&&borderMismatches==0);
    check("media-tile-has-single-line-name-and-no-tooltip",assets&&assets->count()==1&&assets->item(0)->text()==QString::fromStdString(asset->fileName())&&assets->item(0)->toolTip().isEmpty());
    const QPoint hover(tracks->viewportXAtTime(core::TimelineTime::fromSeconds(.1)),30);
    QHelpEvent tip(QEvent::ToolTip,hover,tracks->mapToGlobal(hover));QApplication::sendEvent(tracks,&tip);
    check("timeline-clip-does-not-show-name-popup",!QToolTip::isVisible());
    auto* splitter=window.findChild<QSplitter*>("verticalSplitter");auto* handle=splitter->handle(1);
    QEnterEvent enter(QPointF(1,1),QPointF(1,1),QPointF(handle->mapToGlobal(QPoint(1,1))));QApplication::sendEvent(handle,&enter);
    check("splitter-hover-is-not-full-width-blue",handle->grab().toImage().pixelColor(10,handle->height()/2)!=ui::Theme::instance().palette().primaryAccent);
    window.grab().save(output+"/media-panel-fixed.png");
    check("fit-shows-complete-59-second-clip",tracks->viewportXAtTime(asset->duration())<=tracks->width()+2&&timeline->zoomFactor()<1.);
    slider->setValue(0);wait(50);check("zoom-out-is-one-pixel-per-second",std::abs(timeline->zoomFactor()-.02)<.0001&&bar->maximum()==0);
    slider->setValue(100);wait(50);check("zoom-in-has-real-scroll-range",timeline->zoomFactor()==100&&bar->maximum()>200000);
    bar->setValue(bar->maximum());wait(50);
    check("scroll-reaches-clip-end",tracks->viewportXAtTime(asset->duration())<tracks->width()&&tracks->viewportXAtTime(asset->duration())>112);
    const QPoint point(500,12);const auto target=tracks->timeAtViewportX(point.x());
    QMouseEvent press(QEvent::MouseButtonPress,QPointF(point),QPointF(ruler->mapToGlobal(point)),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(ruler,&press);
    QMouseEvent release(QEvent::MouseButtonRelease,QPointF(point),QPointF(ruler->mapToGlobal(point)),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(ruler,&release);
    check("scrolled-ruler-hit-test-matches-track-time",std::abs(engine.playback().currentTime().toSeconds()-target.toSeconds())<.001);
    // Fit back out, then begin cold playback while monitoring actual GUI timers.
    slider->setValue(42);bar->setValue(0);engine.seek(core::TimelineTime(0));
    const int heartbeatMs=std::max(5,qEnvironmentVariableIntValue("CATCHIM_TEST_HEARTBEAT_MS"));
    std::vector<double> heartbeat;QElapsedTimer timer;timer.start();qint64 previous=0;QTimer pulse;
    pulse.setTimerType(heartbeatMs<20?Qt::PreciseTimer:Qt::CoarseTimer);pulse.setInterval(heartbeatMs);
    metrics["heartbeatIntervalMs"]=heartbeatMs;
    QObject::connect(&pulse,&QTimer::timeout,&window,[&]{const auto now=timer.nsecsElapsed();if(previous)heartbeat.push_back((now-previous)/1e6);previous=now;});pulse.start();
    const auto framesBefore=preview->presentedFrames();engine.play();
    check("audio-device-starts-with-cold-source",until([&]{return window.playbackAudioStatistics().active;},10000));
    metrics["coldAudioReadyMs"]=static_cast<double>(timer.elapsed());
    wait(1000);const auto startFrames=preview->presentedFrames(),startPainted=preview->paintedFrames();const auto startTime=engine.playback().currentTime().toSeconds();
    const auto startUnderruns=window.playbackAudioStatistics().underruns;
    QElapsedTimer playTimer;playTimer.start();double maxAge=0,maxAudioDrift=0;int ageSamples=0;
    const int measurementMs=std::max(8000,qEnvironmentVariableIntValue("CATCHIM_TEST_PLAYBACK_MS"));
    while(playTimer.elapsed()<measurementMs) {
        wait(25);const double now=engine.playback().currentTime().toSeconds();
        if(preview->presentedSeconds()>=0){maxAge=std::max(maxAge,now-preview->presentedSeconds());++ageSamples;}
        maxAudioDrift=std::max(maxAudioDrift,std::abs(now-window.playbackAudioStatistics().positionSeconds));
        if(playTimer.elapsed()>3000&&playTimer.elapsed()<3050)slider->setValue(50);
    }
    const double fps=(preview->presentedFrames()-startFrames)*1000./playTimer.elapsed();
    const double paintFps=(preview->paintedFrames()-startPainted)*1000./playTimer.elapsed();
    const auto gpu=preview->gpuStatistics();
    metrics["gpuAdapter"]=QString::fromStdString(gpu.adapter);metrics["gpuSurfaceFrames"]=static_cast<double>(gpu.gpuSurfaceFrames);
    metrics["gpuProcessedFrames"]=static_cast<double>(gpu.gpuProcessedFrames);metrics["cpuFallbackFrames"]=static_cast<double>(gpu.cpuFallbackFrames);
    metrics["gpuLastError"]=QString::number(gpu.lastGpuError,16);metrics["gpuReadbackBytes"]=static_cast<double>(gpu.readbackBytes);
    metrics["paintedFps"]=paintFps;
    check("gpu-actually-processes-preview-frames",gpu.gpuProcessedFrames>100&&gpu.cpuFallbackFrames==0);
    check("preview-actually-paints-at-least-24-fps",paintFps>=24);
    metrics["measuredPlaybackSeconds"]=playTimer.elapsed()/1000.;metrics["presentedFps"]=fps;metrics["maxFrameAgeSec"]=maxAge;metrics["maxAudioClockDriftSec"]=maxAudioDrift;
    metrics["audioBackend"]=static_cast<int>(window.playbackAudioStatistics().backend);
    metrics["steadyUnderruns"]=static_cast<double>(window.playbackAudioStatistics().underruns-startUnderruns);
    check("video-presents-at-least-24-fps",fps>=24&&preview->presentedFrames()>framesBefore);
    check("video-stays-within-200ms-of-audio",ageSamples>100&&maxAge<.2);
    check("playhead-tracks-hardware-audio",maxAudioDrift<.08&&engine.playback().currentTime().toSeconds()-startTime>7.5);
    check("steady-audio-does-not-underrun",window.playbackAudioStatistics().underruns==startUnderruns);
    pulse.stop();std::sort(heartbeat.begin(),heartbeat.end());
    const double p95=heartbeat.empty()?1e9:heartbeat[heartbeat.size()*95/100],maximum=heartbeat.empty()?1e9:heartbeat.back();
    metrics["guiTimerP95Ms"]=p95;metrics["guiTimerMaxMs"]=maximum;
    check("gui-responsive-during-real-hevc-playback",p95<std::max(30,heartbeatMs*3)&&maximum<250);
    engine.pause();engine.seek(core::TimelineTime::fromSeconds(18));
    check("paused-seek-actually-presents-new-frame",until([&]{return std::abs(preview->presentedSeconds()-18)<.02;},5000));
    metrics["pausedSeekPlayheadX"]=tracks->viewportXAtTime(engine.playback().currentTime());
    check("paused-seek-reveals-playhead",tracks->viewportXAtTime(engine.playback().currentTime())>=112&&tracks->viewportXAtTime(engine.playback().currentTime())<tracks->width());
    preview->presentedImage().save(output+"/preview-18.png");window.grab().save(output+"/native-playing-ui.png");
    for(double time:{4.,45.,2.,55.,17.9})engine.seek(core::TimelineTime::fromSeconds(time));
    check("latest-seek-wins-over-obsolete-decodes",until([&]{return std::abs(preview->presentedSeconds()-17.9)<.02;},8000));
    wait(300);check("old-frame-cannot-overwrite-paused-seek",std::abs(preview->presentedSeconds()-17.9)<.02);
    const auto submits=window.playbackAudioStatistics().submittedFrames;
    engine.seek(asset->duration()-core::TimelineTime::fromSeconds(1));engine.play();
    check("playback-finishes-and-pauses-at-end",until([&]{return !engine.playback().isPlaying();},5000));
    check("last-second-no-audio-frames-lost",window.playbackAudioStatistics().submittedFrames-submits==48000);
    check("end-position-preserved",std::abs(engine.playback().currentTime().toSeconds()-asset->duration().toSeconds())<.001);
    // Explicit backpressure test against the real Windows endpoint (silenced).
    {
        audio::WindowsAudioPlayer sink;check("real-windows-sink-initializes",sink.init(48000,2));sink.setVolume(0);
        std::vector<float> samples(48000*2,0);const auto accepted=sink.writeSamples(samples.data(),samples.size());
        check("sink-reports-partial-acceptance",accepted>0&&accepted<48000);sink.shutdown();
    }
    // Compare the actual AAC resampling with independent FFmpeg output.
    const auto pcm=media::NativeAudioDecoder::instance().getAudioBuffer(source.toStdWString(),48000,2);
    check("real-aac-decoded-at-48k-stereo",pcm&&pcm->size()>48000*2*59);
    if(pcm) {
        const QString reference=output+"/reference-48000-stereo.f32";QProcess process;
        process.start(QStandardPaths::findExecutable("ffmpeg"),{"-v","error","-y","-i",source,"-vn","-ar","48000","-ac","2","-f","f32le",reference});
        check("independent-audio-reference",process.waitForFinished(20000)&&process.exitCode()==0);
        QFile file(reference);file.open(QIODevice::ReadOnly);const auto bytes=file.readAll();const auto* expected=reinterpret_cast<const float*>(bytes.constData());
        const size_t count=bytes.size()/sizeof(float);double best=0;int bestLag=0;
        for(int lag=-2048;lag<=2048;++lag) {
            double xy=0,xx=0,yy=0;
            for(int i=4800;i<48000*4;i+=16){const double a=(*pcm)[2*i],b=expected[2*(i+lag)];xy+=a*b;xx+=a*a;yy+=b*b;}
            const double correlation=xy/std::sqrt(std::max(1e-20,xx*yy));if(correlation>best){best=correlation;bestLag=lag;}
        }
        metrics["audioReferenceCorrelation"]=best;metrics["audioReferenceLagFrames"]=bestLag;
        metrics["audioDurationDifferenceMs"]=std::abs(static_cast<double>(count)-pcm->size())/96.;
        check("aac-waveform-matches-reference",best>.98);
        check("aac-priming-does-not-delay-timeline",std::abs(bestLag)<=2);
        check("resampled-audio-duration-correct",std::abs(static_cast<double>(count)-pcm->size())<4800*2);
        auto mono=media::NativeAudioDecoder::instance().getAudioBuffer(source.toStdWString(),44100,1);
        check("audio-cache-respects-rate-and-channels",mono&&std::abs(mono->size()/44100.-pcm->size()/96000.)<.05&&mono->size()!=pcm->size());
    }
    // GPU readback must retain source size metadata, color/crop and transforms.
    {
        media::NativeVideoDecoder gpuDecoder,cpuDecoder;gpuDecoder.setPreviewSize(1015,540);
        auto meanError=[](QImage a,QImage b) {
            if(a.size()!=b.size()||a.isNull()||b.isNull())return 1e9;
            a=a.convertToFormat(QImage::Format_RGBA8888);b=b.convertToFormat(QImage::Format_RGBA8888);double error=0;
            for(int y=0;y<a.height();++y)for(int x=0;x<a.width();++x)for(int c=0;c<3;++c)error+=std::abs(int(a.constScanLine(y)[x*4+c])-int(b.constScanLine(y)[x*4+c]));
            return error/(a.width()*a.height()*3.);
        };
        for(double time:{18.,17.9,55.}) {
            int w=0,h=0,ow=0,oh=0,cw=0,ch=0;std::vector<uint8_t> pixels,full;
            const bool ok=gpuDecoder.getFrame(source.toStdWString(),time,w,h,pixels,"gpu-reference",&ow,&oh);
            const bool cpuOk=cpuDecoder.getFrame(source.toStdWString(),time,cw,ch,full,"cpu-reference");
            check("gpu-preserves-original-size-and-bounds-readback",ok&&ow==2030&&oh==1080&&w==1015&&h==540);
            double error=1e9;
            if(ok&&cpuOk) {
                QImage gpuImage(pixels.data(),w,h,w*4,QImage::Format_RGBA8888);
                QImage cpuImage(full.data(),cw,ch,cw*4,QImage::Format_RGBA8888);
                error=meanError(gpuImage,cpuImage.scaled(w,h,Qt::IgnoreAspectRatio,Qt::SmoothTransformation));
                gpuImage.save(output+QString("/gpu-readback-%1.png").arg(time));
            }
            metrics[QString("gpuRgbMeanError_%1").arg(time)]=error;check("gpu-color-and-crop-match-cpu-reference",error<6.);
        }
        editor::Project project=engine.project();auto* edited=project.activeTimeline()->findClip(clipId);
        edited->setParam("transform.scaleX",.65);edited->setParam("transform.scaleY",.65);edited->setParam("transform.positionX",160.);
        edited->setParam("transform.positionY",-90.);edited->setParam("transform.rotate",12.);
        render::RenderEngine gpuRenderer(1015,540),cpuRenderer(1015,540);
        gpuRenderer.setVideoDecoder(&gpuDecoder);cpuRenderer.setVideoDecoder(&cpuDecoder);
        const auto& gpuFrame=gpuRenderer.renderFrame(project,library,core::TimelineTime::fromSeconds(18));
        const auto& cpuFrame=cpuRenderer.renderFrame(project,library,core::TimelineTime::fromSeconds(18));
        const double transformedError=meanError(QImage(gpuFrame.rgbaPixels.data(),1015,540,1015*4,QImage::Format_RGBA8888),QImage(cpuFrame.rgbaPixels.data(),1015,540,1015*4,QImage::Format_RGBA8888));
        metrics["gpuTransformMeanError"]=transformedError;check("gpu-preview-keeps-transform-geometry",transformedError<6.);
        const auto stats=gpuDecoder.statistics();check("reference-checks-used-real-gpu-not-fallback",stats.gpuProcessedFrames>=4&&stats.cpuFallbackFrames==0);
    }
    // Full-resolution decoder comparison, including non-mod16 HEVC dimensions.
    {
        media::NativeVideoDecoder decoder;
        for(double time:{18.,17.9,55.}) {
            const QString reference=output+QString("/reference-%1.png").arg(time);QProcess process;
            process.start(QStandardPaths::findExecutable("ffmpeg"),{"-v","error","-y","-ss",QString::number(time),"-i",source,"-frames:v","1",reference});process.waitForFinished(20000);
            int w=0,h=0;std::vector<uint8_t> pixels;const bool decoded=decoder.getFrame(source.toStdWString(),time,w,h,pixels,"reference-test");
            QImage expected(reference);expected=expected.convertToFormat(QImage::Format_RGBA8888);double error=1e9;
            metrics[QString("decodedWidth_%1").arg(time)]=w;metrics[QString("decodedHeight_%1").arg(time)]=h;
            if(decoded)QImage(pixels.data(),w,h,w*4,QImage::Format_RGBA8888).save(output+QString("/decoded-%1.png").arg(time));
            if(decoded&&w==expected.width()&&h==expected.height()) {
                QImage(pixels.data(),w,h,w*4,QImage::Format_RGBA8888).save(output+QString("/decoded-%1.png").arg(time));error=0;
                for(int y=0;y<h;++y)for(int x=0;x<w;++x)for(int c=0;c<3;++c)error+=std::abs(int(pixels[(y*w+x)*4+c])-int(expected.constScanLine(y)[x*4+c]));error/=w*h*3.;
            }
            metrics[QString("frameRgbMeanError_%1").arg(time)]=error;
            check("real-hevc-frame-matches-independent-reference",error<8.);
        }
    }
    metrics["checks"]=checks;metrics["passed"]=passed;
    QFile report(output+"/results.json");report.open(QIODevice::WriteOnly);report.write(QJsonDocument(metrics).toJson());
    std::cout<<QJsonDocument(metrics).toJson().toStdString()<<std::endl;return passed?0:1;
}

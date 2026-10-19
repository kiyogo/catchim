#include "ExportWorker.h"
#include <cmath>
#include "audio/AudioPlaybackEngine.h"
#include "media/decoder/NativeVideoDecoder.h"
#include <QTemporaryDir>
#include <QFile>
#include <QDataStream>
#include <QElapsedTimer>
#include <QRegularExpression>
#if !defined(__EMSCRIPTEN__)
#include <QProcess>
#endif
#ifdef _WIN32
#include <objbase.h>
#endif

#if defined(HAVE_QT6)
namespace catchim::ui {

ExportWorker::ExportWorker(
    const editor::Project& project,
    const media::MediaLibrary& mediaLibrary,
    int width,
    int height,
    double fps,
    int totalFrames,
    const QString& ffmpegPath,
    const QStringList& ffmpegArgs,
    const QString& savePath,
    QObject* parent,
    bool includeAudio
)
    : QThread(parent)
    , project_(project)
    , width_(width)
    , height_(height)
    , fps_(fps)
    , totalFrames_(totalFrames)
    , ffmpegPath_(ffmpegPath)
    , ffmpegArgs_(ffmpegArgs)
    , savePath_(savePath)
    , includeAudio_(includeAudio)
{
    for(const auto& asset:mediaLibrary.assets())mediaLibrary_.addAsset(std::make_shared<media::MediaAsset>(*asset));
}

ExportWorker::~ExportWorker() {
    cancel();
    wait(); // Never destroy a live QThread while a decoder is finishing a read.
}

void ExportWorker::cancel() {
    cancelRequested_.store(true);
}

#if !defined(__EMSCRIPTEN__)
bool ExportWorker::tryDirectExport() {
    if(qEnvironmentVariableIntValue("CATCHIM_DISABLE_DIRECT_EXPORT"))return false;
    const auto* timeline=project_.activeTimeline();if(!timeline)return false;
    const editor::Clip* clip=nullptr;const editor::Track* owner=nullptr;
    for(const auto* track:timeline->allTracks())for(const auto& candidate:track->clips()){
        // Hidden video may still contribute audio in the native mixer.
        if(track->isHidden()||candidate.isHidden())return false;
        if(clip)return false;clip=&candidate;owner=track;
    }
    if(!clip||clip->type()!=editor::ClipType::Video||clip->startTime().ticks()!=0||
        !clip->effects().empty()||!clip->masks().empty()||!clip->animationChannels().empty())return false;
    for(const char* key:{"transform.positionX","transform.positionY","transform.rotate","adjustment.brightness","adjustment.contrast","adjustment.saturation","adjustment.temperature","adjustment.tint","audio.fadeIn","audio.fadeOut"})
        if(std::abs(clip->getParam<double>(key,0))>1e-6)return false;
    for(const char* key:{"transform.scaleX","transform.scaleY","opacity","speed"})if(std::abs(clip->getParam<double>(key,1)-1)>1e-6)return false;
    if(clip->getParam<bool>("reversed",false)||!clip->getParam<std::string>("effect.name","").empty()||
       !clip->getParam<std::string>("transition.type","").empty())return false;
    if(clip->getParam<std::string>("transform.blendModeName","normal")!="normal")return false;
    if(clip->hasParam("transform.blendMode")&&clip->params()["transform.blendMode"]!=0&&clip->params()["transform.blendMode"]!="normal")return false;
    const auto asset=mediaLibrary_.findAsset(clip->mediaId());const auto canvas=project_.settings().canvasSize;
    if(!asset||asset->width()!=canvas.width||asset->height()!=canvas.height||!QFile::exists(QString::fromStdWString(asset->filePath().wstring())))return false;
    const int input=ffmpegArgs_.indexOf("-i");if(input<0||input+2>=ffmpegArgs_.size())return false;
    QStringList arguments={"-y","-filter_threads","2","-ss",QString::number(clip->trimStart().toSeconds(),'f',6),"-i",QString::fromStdWString(asset->filePath().wstring())};
    arguments<<"-map"<<"0:v:0"<<"-vf"<<QString("scale=%1:%2:flags=bilinear,setsar=1,fps=%3").arg(width_).arg(height_).arg(fps_,0,'f',6);
    if(includeAudio_&&asset->hasAudio()){
        const auto gain=audio::AudioPlaybackEngine::resolveEffectiveAudioGain(*clip,owner->isMuted(),core::TimelineTime(0));
        arguments<<"-map"<<"0:a:0?"<<"-af"<<QString("volume=%1,apad").arg(gain,0,'f',8)<<"-c:a"<<(savePath_.endsWith(".webm",Qt::CaseInsensitive)?"libopus":"aac")<<"-b:a"<<"192k";
    } else arguments<<"-an";
    auto encoder=ffmpegArgs_.mid(input+2);if(!encoder.isEmpty())encoder.removeLast();
    encoder.removeAll("-an");
    arguments<<encoder<<"-frames:v"<<QString::number(totalFrames_)<<"-t"<<QString::number(totalFrames_/fps_,'f',6)<<"-progress"<<"pipe:1"<<"-nostats"<<savePath_;
    QProcess process;process.start(ffmpegPath_,arguments);
    emit logOutputReceived("Export pipeline: direct source decode/filter/encode (no RGBA GUI pipe)");
    if(!process.waitForStarted(5000)){emit exportFinished(false,"Không khởi động được bộ mã hóa.");return true;}
    QElapsedTimer elapsed;elapsed.start();QByteArray pending;
    while(process.state()!=QProcess::NotRunning){
        if(cancelRequested_.load()){process.kill();process.waitForFinished(3000);emit exportFinished(false,"Đã hủy xuất.");return true;}
        process.waitForReadyRead(50);pending+=process.readAllStandardOutput();
        int newline;while((newline=pending.indexOf('\n'))>=0){const auto line=pending.left(newline).trimmed();pending.remove(0,newline+1);
            if(line.startsWith("frame=")){const int frames=line.mid(6).toInt();emit progressUpdated(frames,totalFrames_,frames*1000./std::max<qint64>(1,elapsed.elapsed()),std::min(99,frames*100/std::max(1,totalFrames_)));}}
        const auto log=process.readAllStandardError();if(!log.isEmpty())emit logOutputReceived(QString::fromUtf8(log));
    }
    const auto log=process.readAllStandardError();if(!log.isEmpty())emit logOutputReceived(QString::fromUtf8(log));
    const bool success=process.exitStatus()==QProcess::NormalExit&&process.exitCode()==0;
    if(success)emit progressUpdated(totalFrames_,totalFrames_,totalFrames_*1000./std::max<qint64>(1,elapsed.elapsed()),100);
    emit exportFinished(success,success?savePath_:"Không thể xuất video. Xem nhật ký bộ mã hóa.");return true;
}
#endif

void ExportWorker::run() {
#if defined(__EMSCRIPTEN__)
    Q_UNUSED(project_);
    Q_UNUSED(mediaLibrary_);
    Q_UNUSED(width_);
    Q_UNUSED(height_);
    Q_UNUSED(fps_);
    Q_UNUSED(totalFrames_);
    Q_UNUSED(ffmpegPath_);
    Q_UNUSED(ffmpegArgs_);
    Q_UNUSED(savePath_);
    Q_UNUSED(includeAudio_);
    emit exportFinished(false, "Bộ xuất video WebCodecs/WebGPU chưa được tích hợp.");
#else
    if(cancelRequested_.load()){emit exportFinished(false,"Đã hủy xuất.");return;}
    if(tryDirectExport())return;
#ifdef _WIN32
    struct ComScope {
        HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        ~ComScope(){if(SUCCEEDED(result))CoUninitialize();}
    } comScope;
#endif
    render::RenderEngine renderEngine;
    media::NativeVideoDecoder decoder;decoder.setPreviewSize(width_,height_);
    renderEngine.setVideoDecoder(&decoder);
    renderEngine.setCanvasSize(width_, height_);

    QTemporaryDir temp;
    QStringList arguments=ffmpegArgs_;
    if(includeAudio_) {
        if(!temp.isValid()){emit exportFinished(false,"Không thể tạo thư mục âm thanh tạm.");return;}
        const QString wavPath=temp.filePath("timeline-mix.wav");
        QFile wav(wavPath);
        const qint64 sampleFrames=qRound64(static_cast<double>(totalFrames_)*48000/fps_);
        const qint64 byteCount=sampleFrames*2*static_cast<qint64>(sizeof(float));
        if(byteCount>0xffffffffLL-36||!wav.open(QIODevice::WriteOnly)){emit exportFinished(false,"Không thể tạo file mix âm thanh (giới hạn WAV 4 GB).");return;}
        QDataStream header(&wav);header.setByteOrder(QDataStream::LittleEndian);
        header.writeRawData("RIFF",4);header<<quint32(36+byteCount);header.writeRawData("WAVEfmt ",8);
        header<<quint32(16)<<quint16(3)<<quint16(2)<<quint32(48000)<<quint32(48000*8)<<quint16(8)<<quint16(32);
        header.writeRawData("data",4);header<<quint32(byteCount);
        audio::AudioPlaybackEngine mixer;
        const auto* timeline=project_.activeTimeline();
        if(!timeline){emit exportFinished(false,"Không có timeline để xuất.");return;}
        for(qint64 offset=0;offset<sampleFrames;offset+=48000) {
            if(cancelRequested_.load()){emit exportFinished(false,"Đã hủy xuất.");return;}
            const auto frames=std::min<qint64>(48000,sampleFrames-offset);
            const auto pcm=mixer.renderAudioSlice(*timeline,core::TimelineTime::fromSeconds(static_cast<double>(offset)/48000),core::TimelineTime::fromSeconds(static_cast<double>(frames)/48000),48000,&mediaLibrary_);
            const qint64 bytes=frames*8;
            if(static_cast<qint64>(pcm.samples().size()*sizeof(float))!=bytes||wav.write(reinterpret_cast<const char*>(pcm.samples().data()),bytes)!=bytes){emit exportFinished(false,"Không thể ghi file mix âm thanh.");return;}
        }
        wav.close();
        const int input=arguments.indexOf("-i");
        if(input<0||input+1>=arguments.size()){emit exportFinished(false,"Thiếu đầu vào video.");return;}
        const QStringList audioArgs={"-i",wavPath,"-map","0:v:0","-map","1:a:0","-c:a",savePath_.endsWith(".webm",Qt::CaseInsensitive)?"libopus":"aac","-b:a","192k"};
        for(int i=0;i<audioArgs.size();++i)arguments.insert(input+2+i,audioArgs[i]);
    }
    QProcess ffmpegProcess;
    ffmpegProcess.start(ffmpegPath_, arguments);
    if (!ffmpegProcess.waitForStarted(5000)) {
        emit exportFinished(false, "Failed to start FFmpeg process.");
        return;
    }

    auto startTime = std::chrono::steady_clock::now();
    int lastReportedFrame = 0;
    auto lastReportTime = startTime;

    for (int f = 0; f < totalFrames_; ++f) {
        if (cancelRequested_.load()) {
            ffmpegProcess.kill();
            ffmpegProcess.waitForFinished(3000);
            emit exportFinished(false, "Export cancelled.");
            return;
        }

        double timeSec = static_cast<double>(f) / fps_;
        const auto& output = renderEngine.renderFrame(
            project_,
            mediaLibrary_,
            core::TimelineTime::fromSeconds(timeSec)
        );

        if (output.rgbaPixels.size() == static_cast<size_t>(width_ * height_ * 4)) {
            const char* dataPtr = reinterpret_cast<const char*>(output.rgbaPixels.data());
            qint64 toWrite = static_cast<qint64>(output.rgbaPixels.size());
            qint64 written = 0;
            while (written < toWrite && ffmpegProcess.state() == QProcess::Running && !cancelRequested_.load()) {
                qint64 w = ffmpegProcess.write(dataPtr + written, toWrite - written);
                if (w <= 0) {
                    if (!ffmpegProcess.waitForBytesWritten(50)) break;
                } else {
                    written += w;
                }
            }
            if(written!=toWrite) {
                ffmpegProcess.kill();ffmpegProcess.waitForFinished(3000);
                emit exportFinished(false,"Không ghi đủ frame vào bộ mã hóa.");return;
            }
        } else {
            ffmpegProcess.kill();ffmpegProcess.waitForFinished(3000);
            emit exportFinished(false,"Kích thước frame không hợp lệ.");return;
        }

        // Throttle memory if pending write buffer is over 16 frames
        const auto drainStart=std::chrono::steady_clock::now();
        while(ffmpegProcess.bytesToWrite()>static_cast<qint64>(width_)*height_*4*4) {
            ffmpegProcess.waitForBytesWritten(50);
            if(cancelRequested_.load()||ffmpegProcess.state()!=QProcess::Running||std::chrono::steady_clock::now()-drainStart>std::chrono::seconds(15)) {
                ffmpegProcess.kill();ffmpegProcess.waitForFinished(3000);
                emit exportFinished(false,cancelRequested_.load()?"Đã hủy xuất.":"Bộ mã hóa không nhận dữ liệu.");return;
            }
        }

        // Read standard error output from FFmpeg
        QByteArray errData = ffmpegProcess.readAllStandardError();
        if (!errData.isEmpty()) {
            emit logOutputReceived(QString::fromUtf8(errData).trimmed());
        }

        // Progress & FPS reporting
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsedSinceReport = now - lastReportTime;
        if (elapsedSinceReport.count() >= 0.2 || f == totalFrames_ - 1) {
            double currentFps = (f + 1 - lastReportedFrame) / elapsedSinceReport.count();
            int pct = static_cast<int>(std::round((static_cast<double>(f + 1) / totalFrames_) * 100.0));
            emit progressUpdated(f + 1, totalFrames_, currentFps, pct);
            lastReportedFrame = f + 1;
            lastReportTime = now;
        }
    }

    ffmpegProcess.closeWriteChannel();
    if(!ffmpegProcess.waitForFinished(30000)){
        ffmpegProcess.kill();ffmpegProcess.waitForFinished(3000);
        emit exportFinished(false,"Bộ mã hóa không hoàn tất trong thời gian cho phép.");return;
    }

    QByteArray trailingErr = ffmpegProcess.readAllStandardError();
    if (!trailingErr.isEmpty()) {
        emit logOutputReceived(QString::fromUtf8(trailingErr).trimmed());
    }

    if (cancelRequested_.load()) {
        emit exportFinished(false, "Export cancelled.");
    } else if (ffmpegProcess.exitStatus() == QProcess::NormalExit && ffmpegProcess.exitCode() == 0) {
        emit exportFinished(true, savePath_);
    } else {
        emit exportFinished(false, QString("FFmpeg exited with error code: %1").arg(ffmpegProcess.exitCode()));
    }
#endif
}

} // namespace catchim::ui
#endif

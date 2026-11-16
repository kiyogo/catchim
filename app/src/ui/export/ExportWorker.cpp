#include "ExportWorker.h"
#include <algorithm>
#include <cmath>
#include "audio/AudioPlaybackEngine.h"
#include "media/decoder/NativeVideoDecoder.h"
#include <QTemporaryDir>
#include <QFile>
#include <QDataStream>
#include <QElapsedTimer>
#include <QRegularExpression>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/threading.h>
#endif
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
    bool includeAudio,
    qint64 videoBitrate
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
    , videoBitrate_(videoBitrate)
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
    if (!clip->getParam<std::string>("audio.processingPresetId", "").empty()) return false;
    for(const char* key:{"transform.positionX","transform.positionY","transform.rotate","adjustment.brightness","adjustment.contrast","adjustment.saturation","adjustment.temperature","adjustment.tint","audio.fadeIn","audio.fadeOut"})
        if(std::abs(clip->getParam<double>(key,0))>1e-6)return false;
    for(const char* key:{"transform.scaleX","transform.scaleY","opacity","speed"})if(std::abs(clip->getParam<double>(key,1)-1)>1e-6)return false;
    if(clip->getParam<bool>("reversed",false)||!clip->getParam<std::string>("effect.name","").empty()||
       !clip->getParam<std::string>("transition.type","").empty())return false;
    if(clip->getParam<std::string>("transform.blendModeName","normal")!="normal")return false;
    if(clip->hasParam("transform.blendMode")&&clip->params()["transform.blendMode"]!=0&&clip->params()["transform.blendMode"]!="normal")return false;
    const auto asset=mediaLibrary_.findAsset(clip->mediaId());const auto canvas=project_.settings().canvasSize;
    if(!asset||asset->width()!=canvas.width||asset->height()!=canvas.height||!QFile::exists(QString::fromStdWString(asset->filePath().wstring())))return false;
    if(width_<=0||height_<=0||canvas.width<=0||canvas.height<=0||
        std::abs(static_cast<double>(width_)/height_-static_cast<double>(canvas.width)/canvas.height)>.001)return false;
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
    Q_UNUSED(ffmpegPath_);
    Q_UNUSED(ffmpegArgs_);
    if (cancelRequested_.load()) { emit exportFinished(false, "Đã hủy xuất."); return; }
    render::RenderEngine renderer;
    media::NativeVideoDecoder decoder;
    decoder.setPreviewSize(width_, height_);
    renderer.setVideoDecoder(&decoder);
    renderer.setCanvasSize(width_, height_);
    renderer.setFitSourceToOutput(true);
    // WebGPU's current export shader supports normal alpha compositing. Keep
    // advanced blend modes on the existing CPU renderer until their matching
    // GPU shader is selected, rather than silently changing export appearance.
    bool gpuLayerExportCompatible = true;
    if (project_.settings().background.type == editor::ProjectBackground::Type::Blur)
        gpuLayerExportCompatible = false;
    if (project_.settings().background.color.find("gradient(") != std::string::npos)
        gpuLayerExportCompatible = false;
    const auto* exportTimeline = project_.activeTimeline();
    if (!exportTimeline) gpuLayerExportCompatible = false;
    else for (const auto* track : exportTimeline->allTracks()) {
        if (!track || track->isHidden()) continue;
        for (const auto& clip : track->clips()) {
            if (clip.isHidden()) continue;
            if (clip.getParam<bool>("isAdjustmentLayer", false)) { gpuLayerExportCompatible = false; break; }
            if (clip.getParam<std::string>("transform.blendModeName", "normal") != "normal") { gpuLayerExportCompatible = false; break; }
            if (clip.hasParam("transform.blendMode")) {
                try { if (clip.getParam<int>("transform.blendMode", 0) != 0) { gpuLayerExportCompatible = false; break; } }
                catch (...) { if (clip.getParam<std::string>("transform.blendMode", "normal") != "normal") { gpuLayerExportCompatible = false; break; } }
            }
        }
        if (!gpuLayerExportCompatible) break;
    }
    std::atomic<uint32_t> bridgeResult{0};
    std::atomic<uint32_t> gpuExportStatus{0};
    QByteArray bridgeError(1024, '\0');
    auto waitBridge = [&](const char* failure) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
        while (bridgeResult.load(std::memory_order_acquire) == 0 && std::chrono::steady_clock::now() < deadline)
            emscripten_futex_wait(reinterpret_cast<void*>(&bridgeResult), 0, 1000);
        const auto result = bridgeResult.load(std::memory_order_acquire);
        QString failureMessage = QString::fromUtf8(failure);
        if (result != 1) {
            const QString detail = QString::fromUtf8(bridgeError.constData()).trimmed();
            emit logOutputReceived(detail.isEmpty()
                ? QStringLiteral("WebCodecs: thao tác hết giờ hoặc lỗi. Kiểm tra codec/trình duyệt và bộ nhớ.")
                : QStringLiteral("WebCodecs: ") + detail);
            if (!detail.isEmpty()) failureMessage = detail;
        }
        bridgeError.fill('\0');
        bridgeResult.store(0, std::memory_order_release);
        if (result != 1) emit exportFinished(false, failureMessage);
        return result == 1;
    };
    const QString format = savePath_.endsWith(".webm", Qt::CaseInsensitive) ? "webm" : "mp4";
    // MAIN_THREAD_ASYNC_EM_ASM can run after this worker yields to the main
    // thread. Keep the UTF-8 storage alive until the bridge has acknowledged
    // initialization; passing format.toUtf8().constData() would dangle.
    const QByteArray formatUtf8 = format.toUtf8();
    const QByteArray pathUtf8 = savePath_.toUtf8();
    MAIN_THREAD_ASYNC_EM_ASM({
        Module.catchimExportBegin($0, $1, $2, $3, UTF8ToString($4), UTF8ToString($5), !!$6, $7, $8, !!$9, $10, $11);
    }, width_, height_, fps_, totalFrames_, formatUtf8.constData(), pathUtf8.constData(), includeAudio_, &bridgeResult, &gpuExportStatus, gpuLayerExportCompatible, bridgeError.data(), static_cast<int>(videoBitrate_));
    if (!waitBridge("Không khởi động được bộ xuất WebCodecs.")) return;
    const bool gpuExport = gpuExportStatus.load(std::memory_order_acquire) == 1;
    renderer.setGpuLayerMode(gpuExport);
    decoder.setBrowserGpuDirectMode(gpuExport);
    decoder.setPlaybackMode(gpuExport);
    decoder.setBrowserGpuPlaybackRate(gpuExport ? 0.1 : 1.0);
    if (gpuExport) emit logOutputReceived("WebGPU: đang ghép các layer xuất video trực tiếp trên GPU.");
    else if (!gpuLayerExportCompatible) emit logOutputReceived("WebGPU: nền mờ/gradient hoặc blend/adjustment layer cần bộ ghép CPU để giữ nguyên hình ảnh.");
    else emit logOutputReceived("WebGPU: không khởi tạo được compositor phần cứng; dùng bộ ghép CPU dự phòng.");
    const auto started = std::chrono::steady_clock::now();
    auto previousGpuFrameFinished = started;
    int lastReported = 0;
    auto lastReport = started;
    int audioRenderedFrames = 0;
    audio::AudioPlaybackEngine audioMixer;
    for (int frame = 0; frame < totalFrames_; ++frame) {
        if (cancelRequested_.load()) {
            MAIN_THREAD_ASYNC_EM_ASM({ Module.catchimExportCancel(); });
            emit exportFinished(false, "Đã hủy xuất."); return;
        }
        const auto& rendered = renderer.renderFrame(project_, mediaLibrary_, core::TimelineTime::fromSeconds(static_cast<double>(frame) / fps_));
        if (gpuExport) {
            struct BrowserGpuLayer {
                uint32_t pixels, videoKey;
                int32_t width, height;
                float x, y, scaleX, scaleY, rotation, opacity;
                int32_t blend;
            };
            static_assert(sizeof(BrowserGpuLayer) == 44, "Update the WebGPU export layer descriptor layout.");
            const auto layers = renderer.takeLastLayers();
            std::vector<BrowserGpuLayer> descriptors;
            descriptors.reserve(layers.size());
            for (const auto& layer : layers) {
                descriptors.push_back({
                    layer.rgbaPixels.empty() ? 0u : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(layer.rgbaPixels.data())),
                    layer.browserVideoKey.empty() ? 0u : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(layer.browserVideoKey.c_str())),
                    layer.sourceWidth, layer.sourceHeight,
                    static_cast<float>(layer.transform.positionX), static_cast<float>(layer.transform.positionY),
                    static_cast<float>(layer.transform.scaleX), static_cast<float>(layer.transform.scaleY),
                    static_cast<float>(layer.transform.rotate), static_cast<float>(layer.transform.opacity),
                    layer.transform.blendMode == "normal" ? 0 : 1
                });
            }
            const auto& background = project_.settings().background;
            float red=0, green=0, blue=0;
            if (background.type == editor::ProjectBackground::Type::Color && background.color.size() == 7 && background.color[0] == '#') {
                try { red=std::stoi(background.color.substr(1,2),nullptr,16)/255.f; green=std::stoi(background.color.substr(3,2),nullptr,16)/255.f; blue=std::stoi(background.color.substr(5,2),nullptr,16)/255.f; } catch (...) {}
            }
            MAIN_THREAD_ASYNC_EM_ASM({ Module.catchimExportGpuFrame($0, $1, $2, $3, $4, $5, $6, $7); },
                descriptors.empty() ? nullptr : descriptors.data(), static_cast<int>(descriptors.size()), frame, red, green, blue, &bridgeResult, bridgeError.data());
            if (!waitBridge("WebCodecs không nhận được frame video.")) {
                MAIN_THREAD_ASYNC_EM_ASM({ Module.catchimExportCancel(); });
                return;
            }
        } else {
            if (rendered.rgbaPixels.size() != static_cast<size_t>(width_) * height_ * 4) {
                MAIN_THREAD_ASYNC_EM_ASM({ Module.catchimExportCancel(); });
                emit exportFinished(false, "Frame dựng không đúng kích thước."); return;
            }
            MAIN_THREAD_ASYNC_EM_ASM({ Module.catchimExportFrame($0, $1, $2, $3, $4); }, rendered.rgbaPixels.data(), rendered.rgbaPixels.size(), frame, &bridgeResult, bridgeError.data());
        }
        if (!gpuExport && !waitBridge("WebCodecs không nhận được frame video.")) {
            MAIN_THREAD_ASYNC_EM_ASM({ Module.catchimExportCancel(); }); return;
        }
        const int audioChunk = std::max(1, static_cast<int>(std::round(fps_)));
        if (includeAudio_ && (frame + 1 >= audioRenderedFrames + audioChunk || frame == totalFrames_ - 1)) {
            const int chunkFrames = std::min(totalFrames_ - audioRenderedFrames, audioChunk);
            const auto* timeline = project_.activeTimeline();
            if (!timeline) { emit exportFinished(false, "Không có timeline để xuất âm thanh."); return; }
            const auto pcm = audioMixer.renderAudioSlice(*timeline, core::TimelineTime::fromSeconds(audioRenderedFrames / fps_), core::TimelineTime::fromSeconds(chunkFrames / fps_), 48000, &mediaLibrary_);
            if (!pcm.samples().empty()) {
                MAIN_THREAD_ASYNC_EM_ASM({ Module.catchimExportAudio($0, $1, $2, $3); }, pcm.samples().data(), pcm.samples().size(), &bridgeResult, bridgeError.data());
                if (!waitBridge("Không thể mã hóa âm thanh.")) { MAIN_THREAD_ASYNC_EM_ASM({ Module.catchimExportCancel(); }); return; }
            }
            audioRenderedFrames += chunkFrames;
        }
        const auto now = std::chrono::steady_clock::now();
        if (gpuExport) {
            const double frameSeconds = std::chrono::duration<double>(now - previousGpuFrameFinished).count();
            if (frameSeconds > .001) decoder.setBrowserGpuPlaybackRate(std::clamp(1.0 / frameSeconds / fps_, .0625, 16.0));
            previousGpuFrameFinished = now;
        }
        if (now - lastReport >= std::chrono::milliseconds(200) || frame == totalFrames_ - 1) {
            const double elapsed = std::chrono::duration<double>(now - lastReport).count();
            emit progressUpdated(frame + 1, totalFrames_, (frame + 1 - lastReported) / std::max(.001, elapsed), (frame + 1) * 100 / totalFrames_);
            lastReported = frame + 1; lastReport = now;
        }
    }
    MAIN_THREAD_ASYNC_EM_ASM({ Module.catchimExportFinish($0, $1); }, &bridgeResult, bridgeError.data());
    if (!waitBridge("Không hoàn tất hoặc tải được file video.")) return;
    emit progressUpdated(totalFrames_, totalFrames_, totalFrames_ / std::max(.001, std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count()), 100);
    emit exportFinished(true, "Video đã sẵn sàng để tải xuống.");
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
    renderEngine.setFitSourceToOutput(true);

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

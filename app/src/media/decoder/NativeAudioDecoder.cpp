#include "NativeAudioDecoder.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include "media/probe/Mp4AudioTiming.h"
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
#include <atomic>
#include <emscripten/em_asm.h>
#include <emscripten/threading.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#endif

namespace catchim::media {

NativeAudioDecoder& NativeAudioDecoder::instance() {
    static NativeAudioDecoder s_instance;
    return s_instance;
}

NativeAudioDecoder::NativeAudioDecoder() {
#if defined(_WIN32)
    MFStartup(MF_VERSION);
#endif
}

NativeAudioDecoder::~NativeAudioDecoder() {
    clearCache();
#if defined(_WIN32)
    MFShutdown();
#endif
}

void NativeAudioDecoder::clearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
    failedCache_.clear();
}

std::shared_ptr<const std::vector<float>> NativeAudioDecoder::getAudioBuffer(
    const std::filesystem::path& path,
    int targetSampleRate,
    int targetChannels
) {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string pathKey = path.string()+"@"+std::to_string(targetSampleRate)+"/"+std::to_string(targetChannels);
    if (failedCache_.contains(pathKey)) return nullptr;
    auto it = cache_.find(pathKey);
    if (it != cache_.end()) {
        return it->second;
    }

#if defined(_WIN32)
    IMFSourceReader* pReader = nullptr;
    IMFAttributes* pAttributes = nullptr;
    MFCreateAttributes(&pAttributes, 1);
    if (pAttributes) {
        pAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);
    }

    std::wstring wpath = path.wstring();
    HRESULT hr = MFCreateSourceReaderFromURL(wpath.c_str(), pAttributes, &pReader);
    if (pAttributes) pAttributes->Release();

    if (FAILED(hr) || !pReader) {
        return nullptr;
    }

    // Deselect all streams, select only first audio stream
    pReader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    hr = pReader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), TRUE);
    if (FAILED(hr)) {
        pReader->Release();
        return nullptr;
    }

    bool isAac=false;IMFMediaType* nativeType=nullptr;
    if(SUCCEEDED(pReader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),0,&nativeType))) {
        GUID subtype{};nativeType->GetGUID(MF_MT_SUBTYPE,&subtype);isAac=subtype==MFAudioFormat_AAC;nativeType->Release();
    }
    const auto timing=isAac?readMp4AudioTiming(path):Mp4AudioTiming{};
    // Configure requested PCM format: 48kHz, float, stereo
    IMFMediaType* pPartialType = nullptr;
    MFCreateMediaType(&pPartialType);
    if (pPartialType) {
        pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        pPartialType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, static_cast<UINT32>(targetChannels));
        pPartialType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, static_cast<UINT32>(targetSampleRate));
        pPartialType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
        pPartialType->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, static_cast<UINT32>(targetChannels * 4));
        pPartialType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, static_cast<UINT32>(targetSampleRate * targetChannels * 4));

        hr = pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pPartialType);
        pPartialType->Release();
    }

    if (FAILED(hr)) {
        pReader->Release();
        return nullptr;
    }

    auto pcmBuf = std::make_shared<std::vector<float>>();
    pcmBuf->reserve(targetSampleRate * targetChannels * 60); // 1 minute pre-reserve
    bool firstSample=true;
    bool negativeTimestampTrimmed=false;

    while (true) {
        DWORD streamIndex = 0, flags = 0;
        LONGLONG timestamp = 0;
        IMFSample* pSample = nullptr;

        hr = pReader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
            0,
            &streamIndex,
            &flags,
            &timestamp,
            &pSample
        );

        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            if (pSample) pSample->Release();
            break;
        }

        if (flags & MF_SOURCE_READERF_STREAMTICK) {
            if (pSample) pSample->Release();
            continue;
        }

        if (pSample) {
            IMFMediaBuffer* pBuffer = nullptr;
            hr = pSample->ConvertToContiguousBuffer(&pBuffer);
            if (SUCCEEDED(hr) && pBuffer) {
                BYTE* pData = nullptr;
                DWORD currentLength = 0;
                hr = pBuffer->Lock(&pData, nullptr, &currentLength);
                if (SUCCEEDED(hr) && pData && currentLength >= sizeof(float)) {
                    size_t floatCount = currentLength / sizeof(float);
                    const float* floatData = reinterpret_cast<const float*>(pData);
                    if(firstSample) {
                        // AAC priming can be timestamped before timeline zero.
                        // Appending it as time zero delays speech by one packet.
                        const int64_t firstFrame=std::llround(static_cast<double>(timestamp)*targetSampleRate/10000000.);
                        if(firstFrame<0) {
                            const size_t skip=std::min(floatCount,static_cast<size_t>(-firstFrame)*targetChannels);
                            floatData+=skip;floatCount-=skip;
                            negativeTimestampTrimmed=true;
                        } else if(firstFrame>0)pcmBuf->resize(static_cast<size_t>(firstFrame)*targetChannels,0.f);
                        firstSample=false;
                    }
                    pcmBuf->insert(pcmBuf->end(), floatData, floatData + floatCount);
                    pBuffer->Unlock();
                }
                pBuffer->Release();
            }
            pSample->Release();
        }
    }

    pReader->Release();

    if(!negativeTimestampTrimmed&&timing.mediaStartSeconds>0&&timing.presentationSeconds>0) {
        const size_t delay=static_cast<size_t>(std::llround(timing.mediaStartSeconds*targetSampleRate))*targetChannels;
        const double rawSeconds=static_cast<double>(pcmBuf->size())/(targetSampleRate*targetChannels);
        // Some MF AAC readers rebase the initial packet to zero but retain
        // the priming samples. Do not trim readers that already removed them.
        if(rawSeconds>timing.presentationSeconds+timing.mediaStartSeconds*.5&&delay<pcmBuf->size())
            pcmBuf->erase(pcmBuf->begin(),pcmBuf->begin()+delay);
    }

    if (!pcmBuf->empty()) {
        cache_[pathKey] = pcmBuf;
        return pcmBuf;
    }
#endif

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
    if (targetSampleRate > 0 && targetChannels > 0) {
        const auto utf8 = path.u8string();
        const std::string pathUtf8(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        std::atomic<uint32_t> completed{0}, resultPointer{0}, decodedFrames{0}, succeeded{0};
        const auto doneAddress = reinterpret_cast<uintptr_t>(&completed);
        const auto pointerAddress = reinterpret_cast<uintptr_t>(&resultPointer);
        const auto framesAddress = reinterpret_cast<uintptr_t>(&decodedFrames);
        const auto successAddress = reinterpret_cast<uintptr_t>(&succeeded);
        MAIN_THREAD_ASYNC_EM_ASM({
            Module.catchimDecodeAudio(
                UTF8ToString($0), $1, $2, $3, $4, $5, $6
            );
        }, pathUtf8.c_str(), targetSampleRate, targetChannels,
           pointerAddress, framesAddress, doneAddress, successAddress);

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
        while (completed.load(std::memory_order_acquire) == 0 && std::chrono::steady_clock::now() < deadline)
            emscripten_futex_wait(reinterpret_cast<void*>(&completed), 0, 1000);

        const uint32_t ptr = resultPointer.load(std::memory_order_acquire);
        const uint32_t frames = decodedFrames.load(std::memory_order_acquire);
        if (completed.load(std::memory_order_acquire) != 0 && succeeded.load(std::memory_order_acquire) && ptr && frames) {
            const auto* samples = reinterpret_cast<const float*>(static_cast<uintptr_t>(ptr));
            auto pcm = std::make_shared<std::vector<float>>(samples, samples + static_cast<size_t>(frames) * targetChannels);
            std::free(const_cast<float*>(samples));
            cache_[pathKey] = pcm;
            return pcm;
        }
        if (ptr) std::free(reinterpret_cast<void*>(static_cast<uintptr_t>(ptr)));
    }
#endif

    failedCache_.insert(pathKey);
    return nullptr;
}

bool NativeAudioDecoder::getAudioSamples(
    const std::filesystem::path& path,
    std::vector<float>& outPcmInterleaved,
    int targetSampleRate,
    int targetChannels
) {
    auto buf = getAudioBuffer(path, targetSampleRate, targetChannels);
    if (buf && !buf->empty()) {
        outPcmInterleaved = *buf;
        return true;
    }
    return false;
}

} // namespace catchim::media


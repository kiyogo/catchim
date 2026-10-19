#include "WindowsAudioPlayer.h"
#include <algorithm>
#include <vector>
#include <cstring>
#include <cmath>
#include <cstdlib>
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
#include <emscripten/em_asm.h>
#include <emscripten/threading.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ole32.lib")

static const GUID KSDATAFORMAT_IEEE_FLOAT_GUID =
    { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID KSDATAFORMAT_PCM_GUID =
    { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
#endif

namespace catchim::audio {

struct BufferSlot {
#if defined(_WIN32)
    WAVEHDR hdr{};
#endif
    std::vector<int16_t> pcm;
    bool inFlight{false};
};

struct WindowsAudioPlayer::Impl {
    mutable std::mutex mutex;
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
    std::atomic<uint32_t> queuedFrames{0};
    std::atomic<uint32_t> generation{0};
#endif

#if defined(_WIN32)
    // WASAPI Low-Latency Engine State
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> pEnumerator;
    Microsoft::WRL::ComPtr<IMMDevice> pDevice;
    Microsoft::WRL::ComPtr<IAudioClient> pAudioClient;
    Microsoft::WRL::ComPtr<IAudioRenderClient> pRenderClient;
    UINT32 bufferFrameCount{0};
    bool wasapiIsFloat{true};
    bool coInitialized{false};

    // WaveOut Fallback Engine State
    HWAVEOUT hWaveOut{nullptr};
    std::vector<BufferSlot> slots;
    size_t nextSlotIndex{0};

    bool initWasapi(int sampleRate, int channels) {
        HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        coInitialized = (hrCo == S_OK || hrCo == S_FALSE);

        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            reinterpret_cast<void**>(pEnumerator.GetAddressOf())
        );
        if (FAILED(hr) || !pEnumerator) {
            cleanupWasapi();
            return false;
        }

        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, pDevice.GetAddressOf());
        if (FAILED(hr) || !pDevice) {
            cleanupWasapi();
            return false;
        }

        hr = pDevice->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void**>(pAudioClient.GetAddressOf())
        );
        if (FAILED(hr) || !pAudioClient) {
            cleanupWasapi();
            return false;
        }

        // Prepare IEEE Float extensible format
        WAVEFORMATEXTENSIBLE wfe{};
        wfe.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfe.Format.nChannels = static_cast<WORD>(channels);
        wfe.Format.nSamplesPerSec = static_cast<DWORD>(sampleRate);
        wfe.Format.wBitsPerSample = 32;
        wfe.Format.nBlockAlign = wfe.Format.nChannels * (wfe.Format.wBitsPerSample / 8);
        wfe.Format.nAvgBytesPerSec = wfe.Format.nSamplesPerSec * wfe.Format.nBlockAlign;
        wfe.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfe.Samples.wValidBitsPerSample = 32;
        wfe.dwChannelMask = (channels == 1) ? 0x4 : 0x3; // SPEAKER_FRONT_CENTER or STEREO
        wfe.SubFormat = KSDATAFORMAT_IEEE_FLOAT_GUID;

        // Always accept the exact 48 kHz float/stereo stream supplied by the
        // mixer. Windows converts to the endpoint mix format; choosing an
        // arbitrary closest format without converting our samples corrupts audio.
        wasapiIsFloat=true;
        hr=pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            1000000,0,reinterpret_cast<WAVEFORMATEX*>(&wfe),nullptr);

        if (FAILED(hr)) {
            cleanupWasapi();
            return false;
        }

        hr = pAudioClient->GetBufferSize(&bufferFrameCount);
        if (FAILED(hr) || bufferFrameCount == 0) {
            cleanupWasapi();
            return false;
        }

        hr = pAudioClient->GetService(
            __uuidof(IAudioRenderClient),
            reinterpret_cast<void**>(pRenderClient.GetAddressOf())
        );
        if (FAILED(hr) || !pRenderClient) {
            cleanupWasapi();
            return false;
        }

        hr = pAudioClient->Start();
        if (FAILED(hr)) {
            cleanupWasapi();
            return false;
        }

        return true;
    }

    void cleanupWasapi() {
        if (pAudioClient) {
            pAudioClient->Stop();
            pAudioClient.Reset();
        }
        pRenderClient.Reset();
        pDevice.Reset();
        pEnumerator.Reset();
        bufferFrameCount = 0;
        if (coInitialized) {
            CoUninitialize();
            coInitialized = false;
        }
    }

    bool initWaveOut(int sampleRate, int channels) {
        WAVEFORMATEX wfx{};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = static_cast<WORD>(channels);
        wfx.nSamplesPerSec = static_cast<DWORD>(sampleRate);
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize = 0;

        MMRESULT res = waveOutOpen(
            &hWaveOut,
            WAVE_MAPPER,
            &wfx,
            0,
            0,
            CALLBACK_NULL
        );

        if (res != MMSYSERR_NOERROR || !hWaveOut) {
            return false;
        }

        const size_t numSlots = 8;
        slots.resize(numSlots);
        for (auto& slot : slots) {
            slot.inFlight = false;
            std::memset(&slot.hdr, 0, sizeof(slot.hdr));
        }
        nextSlotIndex = 0;
        return true;
    }

    void cleanupWaveOut() {
        if (hWaveOut) {
            waveOutReset(hWaveOut);
            for (auto& slot : slots) {
                if (slot.inFlight && (slot.hdr.dwFlags & WHDR_PREPARED)) {
                    waveOutUnprepareHeader(hWaveOut, &slot.hdr, sizeof(WAVEHDR));
                }
                slot.inFlight = false;
            }
            waveOutClose(hWaveOut);
            hWaveOut = nullptr;
        }
        slots.clear();
    }
#endif
};

WindowsAudioPlayer::WindowsAudioPlayer()
    : impl_(std::make_unique<Impl>())
{
}

WindowsAudioPlayer::~WindowsAudioPlayer() {
    shutdown();
}

bool WindowsAudioPlayer::init(int sampleRate, int channels) {
    if (initialized_) return true;

    sampleRate_ = sampleRate;
    channels_ = channels;

#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(impl_->mutex);

    // 1. Try low-latency WASAPI first
    if (impl_->initWasapi(sampleRate_, channels_)) {
        backend_ = AudioBackend::Wasapi;
        initialized_ = true;
        return true;
    }

    // 2. Seamless fallback to waveOut
    if (impl_->initWaveOut(sampleRate_, channels_)) {
        backend_ = AudioBackend::WaveOut;
        initialized_ = true;
        return true;
    }

    backend_ = AudioBackend::None;
    initialized_ = false;
    return false;
#else
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
    backend_ = AudioBackend::WebAudio;
    initialized_ = sampleRate_ > 0 && channels_ > 0;
    return initialized_;
#else
    return false;
#endif
#endif
}

void WindowsAudioPlayer::shutdown() {
    if (!initialized_) return;
    reset();

#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (backend_ == AudioBackend::Wasapi) {
        impl_->cleanupWasapi();
    } else if (backend_ == AudioBackend::WaveOut) {
        impl_->cleanupWaveOut();
    }
#endif
    backend_ = AudioBackend::None;
    initialized_ = false;
}

void WindowsAudioPlayer::reset() {
    if (!initialized_) return;

#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (backend_ == AudioBackend::Wasapi) {
        if (impl_->pAudioClient) {
            impl_->pAudioClient->Stop();
            impl_->pAudioClient->Reset();
            impl_->pAudioClient->Start();
        }
    } else if (backend_ == AudioBackend::WaveOut) {
        if (!impl_->hWaveOut) return;
        waveOutReset(impl_->hWaveOut);
        for (auto& slot : impl_->slots) {
            if (slot.inFlight) {
                if (slot.hdr.dwFlags & WHDR_PREPARED) {
                    waveOutUnprepareHeader(impl_->hWaveOut, &slot.hdr, sizeof(WAVEHDR));
                }
                slot.inFlight = false;
            }
        }
    }
#endif
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
    if (backend_ == AudioBackend::WebAudio) {
        const uint32_t generation = impl_->generation.fetch_add(1, std::memory_order_acq_rel) + 1;
        impl_->queuedFrames.store(0, std::memory_order_release);
        MAIN_THREAD_ASYNC_EM_ASM({
            if (Module.catchimResetAudio) Module.catchimResetAudio($0, $1, $2);
        }, reinterpret_cast<uintptr_t>(&impl_->queuedFrames),
           reinterpret_cast<uintptr_t>(&impl_->generation), generation);
    }
#endif
}

int WindowsAudioPlayer::queuedMilliseconds() const {
    return sampleRate_>0?static_cast<int>(queuedFrames()*1000/sampleRate_):0;
}

size_t WindowsAudioPlayer::queuedFrames() const {
    if (!initialized_) return 0;

#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (backend_ == AudioBackend::Wasapi) {
        if (!impl_->pAudioClient || sampleRate_ <= 0) return 0;
        UINT32 paddingFrames = 0;
        HRESULT hr = impl_->pAudioClient->GetCurrentPadding(&paddingFrames);
        if (FAILED(hr)) return 0;
        return paddingFrames;
    } else if (backend_ == AudioBackend::WaveOut) {
        size_t inFlightBytes = 0;
        for (const auto& slot : impl_->slots) {
            if (slot.inFlight && !(slot.hdr.dwFlags & WHDR_DONE)) {
                inFlightBytes += slot.pcm.size() * sizeof(int16_t);
            }
        }
        return channels_>0?inFlightBytes/(channels_*sizeof(int16_t)):0;
    }
#endif
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
    if (backend_ == AudioBackend::WebAudio)
        return impl_->queuedFrames.load(std::memory_order_acquire);
#endif
    return 0;
}

size_t WindowsAudioPlayer::writeSamples(const float* floatSamples, size_t sampleCount) {
    if (!initialized_ || !floatSamples || sampleCount == 0) return 0;

#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(impl_->mutex);

    if (backend_ == AudioBackend::Wasapi) {
        if (!impl_->pAudioClient || !impl_->pRenderClient || channels_ <= 0) return 0;

        UINT32 paddingFrames = 0;
        HRESULT hr = impl_->pAudioClient->GetCurrentPadding(&paddingFrames);
        if (FAILED(hr)) return 0;

        if (paddingFrames >= impl_->bufferFrameCount) {
            return 0; // Hardware buffer is full
        }

        UINT32 availableFrames = impl_->bufferFrameCount - paddingFrames;
        UINT32 framesRequested = static_cast<UINT32>(sampleCount / channels_);
        UINT32 framesToWrite = std::min(framesRequested, availableFrames);
        if (framesToWrite == 0) return 0;

        BYTE* pBuffer = nullptr;
        hr = impl_->pRenderClient->GetBuffer(framesToWrite, &pBuffer);
        if (FAILED(hr) || !pBuffer) return 0;

        float vol = volume_.load();
        size_t samplesToWrite = framesToWrite * channels_;

        if (impl_->wasapiIsFloat) {
            float* dst = reinterpret_cast<float*>(pBuffer);
            for (size_t i = 0; i < samplesToWrite; ++i) {
                dst[i] = std::clamp(floatSamples[i] * vol, -1.0f, 1.0f);
            }
        } else {
            int16_t* dst = reinterpret_cast<int16_t*>(pBuffer);
            for (size_t i = 0; i < samplesToWrite; ++i) {
                float val = std::clamp(floatSamples[i] * vol, -1.0f, 1.0f);
                dst[i] = static_cast<int16_t>(val * 32767.0f);
            }
        }

        if(FAILED(impl_->pRenderClient->ReleaseBuffer(framesToWrite,0)))return 0;
        return framesToWrite;
    }

    if (backend_ == AudioBackend::WaveOut) {
        if (!impl_->hWaveOut) return 0;

        // Cleanup any finished slots
        for (auto& slot : impl_->slots) {
            if (slot.inFlight && (slot.hdr.dwFlags & WHDR_DONE)) {
                waveOutUnprepareHeader(impl_->hWaveOut, &slot.hdr, sizeof(WAVEHDR));
                slot.inFlight = false;
            }
        }

        // Find an available slot
        BufferSlot* targetSlot = nullptr;
        for (size_t i = 0; i < impl_->slots.size(); ++i) {
            size_t idx = (impl_->nextSlotIndex + i) % impl_->slots.size();
            if (!impl_->slots[idx].inFlight) {
                targetSlot = &impl_->slots[idx];
                impl_->nextSlotIndex = (idx + 1) % impl_->slots.size();
                break;
            }
        }

        if (!targetSlot) {
            return 0;
        }

        // Convert float to int16 with volume scaling
        float vol = volume_.load();
        targetSlot->pcm.resize(sampleCount);
        for (size_t i = 0; i < sampleCount; ++i) {
            float val = floatSamples[i] * vol;
            val = std::clamp(val, -1.0f, 1.0f);
            targetSlot->pcm[i] = static_cast<int16_t>(val * 32767.0f);
        }

        std::memset(&targetSlot->hdr, 0, sizeof(WAVEHDR));
        targetSlot->hdr.lpData = reinterpret_cast<LPSTR>(targetSlot->pcm.data());
        targetSlot->hdr.dwBufferLength = static_cast<DWORD>(targetSlot->pcm.size() * sizeof(int16_t));

        MMRESULT prepRes = waveOutPrepareHeader(impl_->hWaveOut, &targetSlot->hdr, sizeof(WAVEHDR));
        if (prepRes == MMSYSERR_NOERROR) {
            MMRESULT writeRes = waveOutWrite(impl_->hWaveOut, &targetSlot->hdr, sizeof(WAVEHDR));
            if (writeRes == MMSYSERR_NOERROR) {
                targetSlot->inFlight = true;
                return sampleCount/static_cast<size_t>(channels_);
            } else {
                waveOutUnprepareHeader(impl_->hWaveOut, &targetSlot->hdr, sizeof(WAVEHDR));
            }
        }
    }
#endif
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
    if (backend_ == AudioBackend::WebAudio && channels_ > 0) {
        const size_t frames = sampleCount / static_cast<size_t>(channels_);
        const uint32_t generation = impl_->generation.load(std::memory_order_acquire);
        // Own the samples until the browser has copied them. Waiting for each
        // 20 ms packet starves output whenever Qt has a longer paint pending.
        auto* packet = static_cast<float*>(std::malloc(sampleCount * sizeof(float)));
        if (!packet) return 0;
        std::memcpy(packet, floatSamples, sampleCount * sizeof(float));
        impl_->queuedFrames.fetch_add(static_cast<uint32_t>(frames), std::memory_order_acq_rel);
        MAIN_THREAD_ASYNC_EM_ASM({
            try {
                if (Module.catchimScheduleAudio)
                    Module.catchimScheduleAudio($0, $1, $2, $3, $4, $5, $6, $7, 0, true);
                else if (Atomics.load(HEAPU32, $6 >> 2) === $7)
                    Atomics.sub(HEAPU32, $5 >> 2, $1);
            } finally {
                _free($0);
            }
        }, packet, frames, channels_, sampleRate_, volume_.load(std::memory_order_relaxed),
           reinterpret_cast<uintptr_t>(&impl_->queuedFrames),
           reinterpret_cast<uintptr_t>(&impl_->generation), generation);
        return frames;
    }
#endif
    return 0;
}

} // namespace catchim::audio

#include "TranscriptionService.h"
#include "TranscriptionCatalog.h"
#include "TranscriptionEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <nlohmann/json.hpp>

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
#include <emscripten/em_asm.h>
#include <emscripten/threading.h>
#endif

namespace catchim::subtitles {

namespace {

constexpr int kSampleRate = 16000;
constexpr double kChunkLengthSeconds = 30.0;
constexpr double kChunkStrideSeconds = 5.0;
constexpr double kChunkStepSeconds = kChunkLengthSeconds - kChunkStrideSeconds;

std::string resolveModelId(const std::string& modelId) {
    if (modelId == "whisper-tiny") return "onnx-community/whisper-tiny";
    if (modelId == "whisper-small") return "onnx-community/whisper-small";
    if (modelId == "whisper-medium") return "onnx-community/whisper-medium";
    if (modelId == "whisper-large-v3-turbo") return "onnx-community/whisper-large-v3-turbo";
    return {};
}

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
bool transcribeBrowserChunk(
    const std::vector<float>& samples,
    const std::string& language,
    const std::string& modelId,
    std::string& payload,
    const TranscriptionService::ProgressCallback& onProgress,
    std::string& error) {
    if (samples.empty()) {
        error = "Không có mẫu âm thanh để chép lời.";
        return false;
    }

    std::atomic<uint32_t> completed{0};
    std::atomic<uint32_t> resultPointer{0};
    std::atomic<uint32_t> resultLength{0};
    std::atomic<uint32_t> resultStatus{0};
    std::atomic<uint32_t> progressWord{0x01000000};

    const auto doneAddress = reinterpret_cast<uintptr_t>(&completed);
    const auto resultPointerAddress = reinterpret_cast<uintptr_t>(&resultPointer);
    const auto resultLengthAddress = reinterpret_cast<uintptr_t>(&resultLength);
    const auto resultStatusAddress = reinterpret_cast<uintptr_t>(&resultStatus);
    const auto progressAddress = reinterpret_cast<uintptr_t>(&progressWord);

    if (onProgress) {
        onProgress({TranscriptionStatus::LoadingModel, 0.0, "Đang tải mô hình Whisper…"});
    }

    MAIN_THREAD_ASYNC_EM_ASM({
        try {
            if (typeof Module.catchimTranscribeChunk !== "function") {
                throw new Error("Worker chép lời chưa được tải.");
            }
            Module.catchimTranscribeChunk($0, $1, $2, $3, $4, $5, $6, $7, $8);
        } catch (error) {
            const message = JSON.stringify({
                error: error instanceof Error ? error.message : "Không thể khởi chạy worker chép lời.",
            });
            const bytes = new TextEncoder().encode(message);
            const pointer = _malloc(bytes.length + 1);
            if (pointer) {
                HEAPU8.set(bytes, pointer);
                HEAPU8[pointer + bytes.length] = 0;
                Atomics.store(HEAPU32, $5 >> 2, pointer);
                Atomics.store(HEAPU32, $6 >> 2, bytes.length);
            }
            Atomics.store(HEAPU32, $7 >> 2, 2);
            Atomics.store(HEAPU32, $4 >> 2, 1);
            Atomics.notify(HEAP32, $4 >> 2);
        }
    }, samples.data(), samples.size(), language.c_str(), modelId.c_str(),
       doneAddress, resultPointerAddress, resultLengthAddress,
       resultStatusAddress, progressAddress);

    uint32_t lastProgress = std::numeric_limits<uint32_t>::max();
    while (completed.load(std::memory_order_acquire) == 0) {
        emscripten_futex_wait(reinterpret_cast<void*>(&completed), 0, 100);
        const uint32_t currentProgress = progressWord.load(std::memory_order_acquire);
        if (currentProgress == lastProgress) continue;
        lastProgress = currentProgress;

        const uint32_t phase = currentProgress >> 24;
        const double fraction = static_cast<double>(currentProgress & 0x00ffffffu) / 100.0;
        if (onProgress && phase == 1) {
            onProgress({TranscriptionStatus::LoadingModel, fraction,
                        "Đang tải mô hình Whisper…"});
        } else if (onProgress && phase == 2) {
            onProgress({TranscriptionStatus::Transcribing, fraction,
                        "Đang nhận dạng giọng nói…"});
        }
    }

    const uint32_t pointer = resultPointer.load(std::memory_order_acquire);
    const uint32_t length = resultLength.load(std::memory_order_acquire);
    const uint32_t status = resultStatus.load(std::memory_order_acquire);
    if (pointer && length > 0) {
        payload.assign(reinterpret_cast<const char*>(static_cast<uintptr_t>(pointer)), length);
        std::free(reinterpret_cast<void*>(static_cast<uintptr_t>(pointer)));
    }

    if (status != 1) {
        try {
            const auto response = nlohmann::json::parse(payload);
            error = response.value("error", "Mô hình Whisper không thể chép lời âm thanh.");
        } catch (...) {
            error = "Mô hình Whisper không thể chép lời âm thanh.";
        }
        return false;
    }
    return true;
}
#endif

} // namespace

TranscriptionService::TranscriptionService() = default;

void TranscriptionService::setModel(std::string modelId) {
    currentModelId_ = std::move(modelId);
}

void TranscriptionService::cancel() noexcept {
    isCancelled_.store(true, std::memory_order_release);
}

void TranscriptionService::reset() noexcept {
    isCancelled_.store(false, std::memory_order_release);
}

TranscriptionResult TranscriptionService::transcribe(
    const std::vector<float>& audioData,
    const std::string& language,
    const std::string& modelId,
    ProgressCallback onProgress) {
    const double durationSeconds = static_cast<double>(audioData.size()) / kSampleRate;
    const AudioChunkProvider provider = [&audioData](double start, double duration, std::vector<float>& output) {
        const auto first = static_cast<size_t>(std::max(0.0, std::floor(start * kSampleRate)));
        const auto last = static_cast<size_t>(std::min(
            static_cast<double>(audioData.size()),
            std::ceil((start + duration) * kSampleRate)));
        if (first >= last || first >= audioData.size()) {
            output.clear();
            return true;
        }
        output.assign(audioData.begin() + first, audioData.begin() + last);
        return true;
    };
    return transcribeStream(durationSeconds, provider, language, modelId, std::move(onProgress));
}

TranscriptionResult TranscriptionService::transcribeStream(
    double totalDurationSeconds,
    const AudioChunkProvider& audioChunkProvider,
    const std::string& language,
    const std::string& modelId,
    ProgressCallback onProgress) {
    reset();
    if (!modelId.empty()) setModel(modelId);

    TranscriptionResult result;
    result.language = language;

    const std::string resolvedModelId = resolveModelId(currentModelId_);
    if (resolvedModelId.empty()) {
        result.error = "Mô hình chép lời được chọn không hợp lệ.";
    } else if (!audioChunkProvider) {
        result.error = "Không có nguồn âm thanh để chép lời.";
    } else if (!std::isfinite(totalDurationSeconds) || totalDurationSeconds <= 0.0) {
        result.error = "Dòng thời gian chưa có âm thanh để chép lời.";
    }

    if (!result.error.empty()) {
        if (onProgress) onProgress({TranscriptionStatus::Error, 0.0, result.error});
        return result;
    }

    const auto chunkCount = static_cast<size_t>(std::max(
        1.0,
        1.0 + std::ceil(std::max(0.0, totalDurationSeconds - kChunkLengthSeconds) /
                        kChunkStepSeconds)));

    size_t chunkIndex = 0;
    for (double start = 0.0; start < totalDurationSeconds; start += kChunkStepSeconds, ++chunkIndex) {
        if (isCancelled()) {
            result.error = "Đã hủy chép lời.";
            if (onProgress) onProgress({TranscriptionStatus::Cancelled, 0.0, result.error});
            return result;
        }

        const double chunkDuration = std::min(kChunkLengthSeconds, totalDurationSeconds - start);
        const bool isFinalChunk = start + chunkDuration >= totalDurationSeconds - 1e-6;
        std::vector<float> monoSamples;

        if (onProgress) {
            onProgress({TranscriptionStatus::PreparingAudio,
                        static_cast<double>(chunkIndex) / chunkCount,
                        "Đang trộn âm thanh trên dòng thời gian…"});
        }

        try {
            if (!audioChunkProvider(start, chunkDuration, monoSamples)) {
                result.error = "Không thể đọc âm thanh trên dòng thời gian.";
                if (onProgress) onProgress({TranscriptionStatus::Error, 0.0, result.error});
                return result;
            }
        } catch (const std::exception& error) {
            result.error = error.what();
            if (onProgress) onProgress({TranscriptionStatus::Error, 0.0, result.error});
            return result;
        }

        if (monoSamples.empty()) continue;

        double sumSquares = 0.0;
        for (const float sample : monoSamples) {
            const double value = std::isfinite(sample) ? sample : 0.0;
            sumSquares += value * value;
        }
        const double rms = std::sqrt(sumSquares / monoSamples.size());
        if (rms < 1e-6) continue;

        std::string payload;
        std::string bridgeError;
        const ProgressCallback chunkProgress = [&, chunkIndex](const TranscriptionProgress& progress) {
            if (!onProgress) return;
            const double chunkFraction = std::clamp(progress.progress, 0.0, 1.0);
            const double overall = std::clamp(
                (static_cast<double>(chunkIndex) + chunkFraction) / chunkCount,
                0.0, 0.99);
            onProgress({progress.status, overall, progress.message});
        };

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
        if (!transcribeBrowserChunk(monoSamples, language, resolvedModelId,
                                    payload, chunkProgress, bridgeError)) {
            result.error = std::move(bridgeError);
            if (onProgress) onProgress({TranscriptionStatus::Error, 0.0, result.error});
            return result;
        }
#else
        (void)monoSamples;
        (void)chunkProgress;
        result.error = "Tính năng chép lời tự động cần bản Catchim chạy trên WebAssembly.";
        if (onProgress) onProgress({TranscriptionStatus::Error, 0.0, result.error});
        return result;
#endif

        if (isCancelled()) {
            result.error = "Đã hủy chép lời.";
            if (onProgress) onProgress({TranscriptionStatus::Cancelled, 0.0, result.error});
            return result;
        }

        try {
            const auto response = nlohmann::json::parse(payload);
            if (response.contains("error")) {
                result.error = response.value("error", "Mô hình Whisper không thể chép lời âm thanh.");
                if (onProgress) onProgress({TranscriptionStatus::Error, 0.0, result.error});
                return result;
            }

            auto chunkSegments = TranscriptionEngine::parseWhisperJson(payload);
            const double keepBefore = isFinalChunk
                ? chunkDuration
                : std::max(0.0, chunkDuration - kChunkStrideSeconds);
            for (auto& segment : chunkSegments) {
                segment.start = std::clamp(segment.start, 0.0, chunkDuration);
                segment.end = std::clamp(segment.end, 0.0, chunkDuration);
                if (segment.text.empty() || segment.end <= segment.start) continue;
                if (!isFinalChunk && (segment.start + segment.end) * 0.5 >= keepBefore) continue;

                segment.start += start;
                segment.end += start;
                if (!result.text.empty()) result.text.push_back(' ');
                result.text += segment.text;
                result.segments.push_back(std::move(segment));
            }
        } catch (const std::exception& error) {
            result.error = std::string("Kết quả chép lời không hợp lệ: ") + error.what();
            if (onProgress) onProgress({TranscriptionStatus::Error, 0.0, result.error});
            return result;
        }

        if (onProgress) {
            onProgress({TranscriptionStatus::Transcribing,
                        static_cast<double>(chunkIndex + 1) / chunkCount,
                        "Đã nhận dạng xong một đoạn âm thanh."});
        }

        if (isFinalChunk) break;
    }

    if (onProgress) {
        onProgress({TranscriptionStatus::Complete, 1.0,
                    result.segments.empty() ? "Không tìm thấy giọng nói." : "Đã chép lời xong."});
    }
    return result;
}

} // namespace catchim::subtitles

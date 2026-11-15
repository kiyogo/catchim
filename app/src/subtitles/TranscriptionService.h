#pragma once

#include "TranscriptionEngine.h"
#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace catchim::subtitles {

enum class TranscriptionStatus {
    Idle,
    PreparingAudio,
    LoadingModel,
    Transcribing,
    Complete,
    Error,
    Cancelled
};

struct TranscriptionProgress {
    TranscriptionStatus status{TranscriptionStatus::Idle};
    double progress{0.0};
    std::string message;
};

struct TranscriptionResult {
    std::string text;
    std::vector<TranscriptionSegment> segments;
    std::string language;
    std::string error;
};

class TranscriptionService {
public:
    using ProgressCallback = std::function<void(const TranscriptionProgress&)>;
    using AudioChunkProvider = std::function<bool(
        double startSeconds,
        double durationSeconds,
        std::vector<float>& monoSamples)>;

    TranscriptionService();
    ~TranscriptionService() = default;

    void setModel(std::string modelId);
    const std::string& currentModel() const noexcept { return currentModelId_; }

    TranscriptionResult transcribe(
        const std::vector<float>& audioData,
        const std::string& language = "auto",
        const std::string& modelId = "whisper-small",
        ProgressCallback onProgress = nullptr);

    // Pulls 30-second mono 16 kHz chunks as needed so long timelines do not
    // need to be materialized as one large PCM buffer before Whisper runs.
    TranscriptionResult transcribeStream(
        double totalDurationSeconds,
        const AudioChunkProvider& audioChunkProvider,
        const std::string& language = "auto",
        const std::string& modelId = "whisper-small",
        ProgressCallback onProgress = nullptr);

    void cancel() noexcept;
    bool isCancelled() const noexcept { return isCancelled_.load(); }
    void reset() noexcept;

private:
    std::string currentModelId_{"whisper-small"};
    std::atomic<bool> isCancelled_{false};
};

} // namespace catchim::subtitles

#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <atomic>
#include <mutex>

namespace catchim::audio {

enum class AudioBackend {
    None,
    Wasapi,
    WaveOut,
    WebAudio
};

class WindowsAudioPlayer {
public:
    WindowsAudioPlayer();
    ~WindowsAudioPlayer();

    bool init(int sampleRate = 48000, int channels = 2);
    void shutdown();

    // Write interleaved float PCM samples (-1.0f to 1.0f)
    // Returns accepted FRAMES (not samples). Callers must retain the remainder.
    size_t writeSamples(const float* floatSamples, size_t sampleCount);

    // Immediately stop playing buffers (called on pause or seek)
    void reset();

    void setVolume(float volume) { volume_ = volume; }
    float getVolume() const { return volume_; }

    bool isInitialized() const { return initialized_; }
    AudioBackend backend() const { return backend_; }
    bool isWasapi() const { return backend_ == AudioBackend::Wasapi; }

    // Milliseconds of audio currently queued in the hardware buffer pool
    int queuedMilliseconds() const;
    size_t queuedFrames() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<float> volume_{1.0f};
    std::atomic<bool> initialized_{false};
    std::atomic<AudioBackend> backend_{AudioBackend::None};
    int sampleRate_{48000};
    int channels_{2};
};

} // namespace catchim::audio

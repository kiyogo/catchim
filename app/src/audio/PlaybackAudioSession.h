#pragma once
#include "media/PlaybackSnapshot.h"
#include "audio/WindowsAudioPlayer.h"
#include <thread>
#include <condition_variable>
#include <optional>
namespace catchim::audio {
class PlaybackAudioSession {
public:
    struct Statistics {
        bool deviceAvailable{false},preparing{false},active{false};
        AudioBackend backend{AudioBackend::None};
        uint64_t submittedFrames{0},partialWrites{0},underruns{0},resets{0};
        double positionSeconds{0},queuedSeconds{0};
    };
    PlaybackAudioSession();
    ~PlaybackAudioSession();
    void setState(std::shared_ptr<const media::PlaybackSnapshot> snapshot,core::TimelineTime position,bool playing);
    void setVolume(float volume){volume_=volume;}
    Statistics statistics() const;
    std::optional<core::TimelineTime> audiblePosition() const;
    bool preparing() const {return preparing_;}
    core::TimelineTime requestedPosition() const;
private:
    void run();
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::thread thread_;
    bool stopping_{false},playing_{false};
    uint64_t serial_{0};
    core::TimelineTime requestedTime_{0};
    std::shared_ptr<const media::PlaybackSnapshot> snapshot_;
    std::atomic<float> volume_{1};
    std::atomic<bool> available_{false},preparing_{false},active_{false};
    std::atomic<AudioBackend> backend_{AudioBackend::None};
    std::atomic<uint64_t> publishedSerial_{0},submitted_{0},partialWrites_{0},underruns_{0},resets_{0};
    std::atomic<double> position_{0},queued_{0};
};
}

#pragma once

#include "core/time/TimelineTime.h"
#include "editor/timeline/Clip.h"
#include "editor/timeline/Timeline.h"
#include "audio/AudioBuffer.h"
#include "audio/AudioMastering.h"
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>

namespace catchim::media {
class MediaLibrary;
}

namespace catchim::audio {

class AudioPlaybackEngine {
public:
    static constexpr double VOLUME_DB_MIN = -60.0;
    static constexpr double VOLUME_DB_MAX = 12.0;

    static double clampDb(double db);
    static double dBToLinear(double db);
    static double linearToDb(double linear);

    static double resolveEffectiveAudioGain(
        const editor::Clip& clip,
        bool trackMuted,
        core::TimelineTime localTime
    );

    static std::vector<float> buildWaveformGainSamples(
        const editor::Clip& clip,
        size_t count
    );

    AudioPlaybackEngine();

    void setMasterVolume(float volume) { masterVolume_ = std::clamp(volume, 0.0f, 2.0f); }
    float getMasterVolume() const { return masterVolume_; }

    void setMasterLimiterEnabled(bool enabled) { limiterEnabled_ = enabled; }
    bool isMasterLimiterEnabled() const { return limiterEnabled_; }

    AudioBuffer renderAudioSlice(
        const editor::Timeline& timeline,
        core::TimelineTime startTime,
        core::TimelineTime duration,
        int sampleRate = 48000,
        const media::MediaLibrary* mediaLibrary = nullptr
    );

private:
    struct PreparedPitchBuffer {
        std::string sourcePath;
        int64_t trimStartTicks{0};
        int64_t durationTicks{0};
        double rate{1.0};
        int sampleRate{0};
        size_t byteSize{0};
        uint64_t lastUsed{0};
        std::shared_ptr<const AudioBuffer> buffer;
    };

    std::shared_ptr<const AudioBuffer> getPitchPreservedBuffer(
        const editor::Clip& clip,
        const std::filesystem::path& sourcePath,
        double rate,
        int sampleRate,
        const std::unordered_set<std::string>& activePitchClipIds
    );

    float masterVolume_{1.0f};
    bool limiterEnabled_{true};
    std::mutex pitchBufferMutex_;
    std::unordered_map<std::string, PreparedPitchBuffer> pitchBuffers_;
    size_t pitchBufferBytes_{0};
    uint64_t pitchBufferClock_{0};
};

} // namespace catchim::audio

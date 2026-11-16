#pragma once

#include "audio/AudioBuffer.h"
#include <cstdint>
#include <vector>

namespace catchim::audio {

class AudioRetimeEngine {
public:
    static constexpr double DEFAULT_RATE = 1.0;
    static constexpr double MIN_RATE = 0.01;
    static constexpr double MAX_RATE = 5.0;
    static constexpr double MIN_PITCH_PRESERVE_RATE = MIN_RATE;
    static constexpr double MAX_PITCH_PRESERVE_RATE = MAX_RATE;
    static constexpr double RATE_EPSILON = 1e-6;

    static double clampRate(double rate) noexcept;
    static bool canMaintainPitch(double rate) noexcept;
    static bool shouldMaintainPitch(double rate, bool maintainPitch) noexcept;

    // Resamples audio buffer with linear interpolation (pitch varies with speed)
    static AudioBuffer renderResampledBuffer(
        const AudioBuffer& sourceBuffer,
        double trimStartSeconds,
        double clipDurationSeconds,
        double rate,
        int32_t targetSampleRate = 44100
    );

    // High quality Synchronized Overlap-Add (WSOLA) pitch-preserved time stretch
    static AudioBuffer renderPitchPreservedBuffer(
        const AudioBuffer& sourceBuffer,
        double trimStartSeconds,
        double clipDurationSeconds,
        double rate,
        int32_t targetSampleRate = 44100
    );

    // Main entry point matching web renderRetimedBuffer
    static AudioBuffer renderRetimedBuffer(
        const AudioBuffer& sourceBuffer,
        double trimStartSeconds,
        double clipDurationSeconds,
        double rate,
        bool maintainPitch = false,
        int32_t targetSampleRate = 44100
    );
};

} // namespace catchim::audio

#include "audio/AudioRetimeEngine.h"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace catchim::audio {

double AudioRetimeEngine::clampRate(double rate) noexcept {
    if (!std::isfinite(rate) || rate <= 0.0) return DEFAULT_RATE;
    return std::clamp(rate, MIN_RATE, MAX_RATE);
}

bool AudioRetimeEngine::canMaintainPitch(double rate) noexcept {
    return rate >= MIN_PITCH_PRESERVE_RATE && rate <= MAX_PITCH_PRESERVE_RATE;
}

bool AudioRetimeEngine::shouldMaintainPitch(double rate, bool maintainPitch) noexcept {
    return maintainPitch && canMaintainPitch(rate);
}

AudioBuffer AudioRetimeEngine::renderResampledBuffer(
    const AudioBuffer& sourceBuffer,
    double trimStartSeconds,
    double clipDurationSeconds,
    double rate,
    int32_t targetSampleRate
) {
    int32_t channels = sourceBuffer.channels() <= 0 ? 1 : sourceBuffer.channels();
    size_t outputFrames = std::max<size_t>(1, static_cast<size_t>(std::ceil(clipDurationSeconds * targetSampleRate)));

    AudioBuffer output(channels, targetSampleRate);
    output.resize(outputFrames);

    if (sourceBuffer.frameCount() == 0) {
        output.clear();
        return output;
    }

    double srcSampleRate = static_cast<double>(sourceBuffer.sampleRate());
    const auto& srcSamples = sourceBuffer.samples();
    size_t srcTotalFrames = sourceBuffer.frameCount();

    for (size_t i = 0; i < outputFrames; ++i) {
        double clipTime = static_cast<double>(i) / static_cast<double>(targetSampleRate);
        double sourceTime = trimStartSeconds + clipTime * rate;
        double samplePos = sourceTime * srcSampleRate;

        if (samplePos <= 0.0) {
            for (int32_t ch = 0; ch < channels; ++ch) {
                output.samples()[i * channels + ch] = srcSamples[ch];
            }
        } else {
            size_t lower = static_cast<size_t>(std::floor(samplePos));
            if (lower >= srcTotalFrames) {
                for (int32_t ch = 0; ch < channels; ++ch) {
                    output.samples()[i * channels + ch] = 0.0f;
                }
            } else {
                size_t upper = std::min(srcTotalFrames - 1, lower + 1);
                float fraction = static_cast<float>(samplePos - static_cast<double>(lower));
                for (int32_t ch = 0; ch < channels; ++ch) {
                    float s0 = srcSamples[lower * channels + ch];
                    float s1 = srcSamples[upper * channels + ch];
                    output.samples()[i * channels + ch] = s0 * (1.0f - fraction) + s1 * fraction;
                }
            }
        }
    }

    return output;
}

AudioBuffer AudioRetimeEngine::renderPitchPreservedBuffer(
    const AudioBuffer& sourceBuffer,
    double trimStartSeconds,
    double clipDurationSeconds,
    double rate,
    int32_t targetSampleRate
) {
    int32_t channels = sourceBuffer.channels() <= 0 ? 1 : sourceBuffer.channels();
    size_t outputFrames = std::max<size_t>(1, static_cast<size_t>(std::ceil(clipDurationSeconds * targetSampleRate)));

    AudioBuffer output(channels, targetSampleRate);
    output.resize(outputFrames);

    if (sourceBuffer.frameCount() == 0) {
        output.clear();
        return output;
    }

    // Step 1: Extract the input slice at target sample rate
    double sourceDuration = clipDurationSeconds * rate;
    size_t numSourceFrames = std::max<size_t>(1, static_cast<size_t>(std::ceil(sourceDuration * targetSampleRate)));

    AudioBuffer resampledSlice(channels, targetSampleRate);
    resampledSlice.resize(numSourceFrames);

    double srcSampleRate = static_cast<double>(sourceBuffer.sampleRate());
    const auto& srcSamples = sourceBuffer.samples();
    size_t srcTotalFrames = sourceBuffer.frameCount();

    for (size_t i = 0; i < numSourceFrames; ++i) {
        double t = trimStartSeconds + (static_cast<double>(i) / static_cast<double>(targetSampleRate));
        double samplePos = t * srcSampleRate;
        if (samplePos <= 0.0) {
            for (int32_t ch = 0; ch < channels; ++ch) {
                resampledSlice.samples()[i * channels + ch] = srcSamples[ch];
            }
        } else {
            size_t lower = static_cast<size_t>(std::floor(samplePos));
            if (lower >= srcTotalFrames) {
                for (int32_t ch = 0; ch < channels; ++ch) {
                    resampledSlice.samples()[i * channels + ch] = 0.0f;
                }
            } else {
                size_t upper = std::min(srcTotalFrames - 1, lower + 1);
                float fraction = static_cast<float>(samplePos - static_cast<double>(lower));
                for (int32_t ch = 0; ch < channels; ++ch) {
                    float s0 = srcSamples[lower * channels + ch];
                    float s1 = srcSamples[upper * channels + ch];
                    resampledSlice.samples()[i * channels + ch] = s0 * (1.0f - fraction) + s1 * fraction;
                }
            }
        }
    }

    // Step 2: WSOLA (Synchronized Overlap-Add)
    constexpr size_t WINDOW_SIZE = 1024;
    constexpr size_t SYNTHESIS_HOP = 512;
    const int64_t searchRange = 256;

    // Precompute Hann window
    std::vector<float> window(WINDOW_SIZE);
    for (size_t n = 0; n < WINDOW_SIZE; ++n) {
        window[n] = 0.5f * (1.0f - std::cos(2.0f * std::numbers::pi_v<float> * static_cast<float>(n) / static_cast<float>(WINDOW_SIZE - 1)));
    }

    std::vector<float> synthBuffer(outputFrames * channels, 0.0f);
    std::vector<float> synthWeights(outputFrames, 0.0f);

    const auto& inSamples = resampledSlice.samples();
    size_t inFrames = resampledSlice.frameCount();

    for (size_t ps = 0; ps < outputFrames; ps += SYNTHESIS_HOP) {
        int64_t nominalAnalysisPos = static_cast<int64_t>(std::round(static_cast<double>(ps) * rate));

        // Find best delta via cross-correlation with previous overlap region
        int64_t bestDelta = 0;
        if (ps > 0 && ps < outputFrames) {
            double bestCorr = -1e18;

            int64_t deltaMin = -searchRange;
            int64_t deltaMax = searchRange;

            for (int64_t delta = deltaMin; delta <= deltaMax; ++delta) {
                int64_t candPos = nominalAnalysisPos + delta;
                if (candPos < 0 || static_cast<size_t>(candPos + WINDOW_SIZE) > inFrames) {
                    continue;
                }

                double corr = 0.0;
                double normIn = 0.0;
                double normSynth = 0.0;

                size_t overlapLen = std::min<size_t>(WINDOW_SIZE - SYNTHESIS_HOP, outputFrames - ps);
                for (size_t k = 0; k < overlapLen; ++k) {
                    float inVal = inSamples[(static_cast<size_t>(candPos) + k) * channels];
                    float synVal = synthBuffer[(ps + k) * channels];
                    corr += static_cast<double>(inVal * synVal);
                    normIn += static_cast<double>(inVal * inVal);
                    normSynth += static_cast<double>(synVal * synVal);
                }

                double denom = std::sqrt(normIn * normSynth);
                if (denom > 1e-6) {
                    corr /= denom;
                }

                if (corr > bestCorr) {
                    bestCorr = corr;
                    bestDelta = delta;
                }
            }
        }

        int64_t actualAnalysisPos = nominalAnalysisPos + bestDelta;
        if (actualAnalysisPos < 0) actualAnalysisPos = 0;
        if (inFrames > WINDOW_SIZE && static_cast<size_t>(actualAnalysisPos + WINDOW_SIZE) > inFrames) {
            actualAnalysisPos = static_cast<int64_t>(inFrames - WINDOW_SIZE);
        }

        for (size_t n = 0; n < WINDOW_SIZE; ++n) {
            size_t outIdx = ps + n;
            if (outIdx >= outputFrames) break;

            size_t inIdx = static_cast<size_t>(actualAnalysisPos) + n;
            float w = window[n];
            synthWeights[outIdx] += w;

            for (int32_t ch = 0; ch < channels; ++ch) {
                float sampleVal = (inIdx < inFrames) ? inSamples[inIdx * channels + ch] : 0.0f;
                synthBuffer[outIdx * channels + ch] += w * sampleVal;
            }
        }
    }

    // Step 3: Normalize synthesis by weights
    for (size_t i = 0; i < outputFrames; ++i) {
        float weight = synthWeights[i];
        if (weight > 1e-5f) {
            float inv = 1.0f / weight;
            for (int32_t ch = 0; ch < channels; ++ch) {
                output.samples()[i * channels + ch] = std::clamp(synthBuffer[i * channels + ch] * inv, -1.0f, 1.0f);
            }
        } else {
            for (int32_t ch = 0; ch < channels; ++ch) {
                output.samples()[i * channels + ch] = 0.0f;
            }
        }
    }

    return output;
}

AudioBuffer AudioRetimeEngine::renderRetimedBuffer(
    const AudioBuffer& sourceBuffer,
    double trimStartSeconds,
    double clipDurationSeconds,
    double rate,
    bool maintainPitch,
    int32_t targetSampleRate
) {
    double clampedRate = clampRate(rate);
    bool usePitchPreserve = shouldMaintainPitch(clampedRate, maintainPitch) &&
                            std::abs(clampedRate - 1.0) > RATE_EPSILON;

    if (usePitchPreserve) {
        return renderPitchPreservedBuffer(
            sourceBuffer,
            trimStartSeconds,
            clipDurationSeconds,
            clampedRate,
            targetSampleRate
        );
    }

    return renderResampledBuffer(
        sourceBuffer,
        trimStartSeconds,
        clipDurationSeconds,
        clampedRate,
        targetSampleRate
    );
}

} // namespace catchim::audio

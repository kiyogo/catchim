#include "render/effects/ClipEffectRenderer.h"
#include "editor/animation/EffectParamAnimationEngine.h"
#include "render/Compositor.h"
#include "render/effects/EffectDefinitionRegistry.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace catchim::render {
namespace {

double readEffectNumber(const nlohmann::json& params, const char* key, double fallback) {
    if (!params.is_object()) return fallback;
    const auto it = params.find(key);
    if (it == params.end() || !it->is_number()) return fallback;
    const double value = it->get<double>();
    return std::isfinite(value) ? value : fallback;
}

std::string readEffectString(
    const nlohmann::json& params,
    const char* key,
    const std::string& fallback
) {
    if (!params.is_object()) return fallback;
    const auto it = params.find(key);
    return it != params.end() && it->is_string() ? it->get<std::string>() : fallback;
}

std::array<uint8_t, 3> parseHexColor(const std::string& color) {
    std::string hex = color;
    if (!hex.empty() && hex.front() == '#') hex.erase(hex.begin());
    if (hex.size() == 3) {
        const std::string shortHex = hex;
        hex = {shortHex[0], shortHex[0], shortHex[1], shortHex[1], shortHex[2], shortHex[2]};
    }

    const auto hexValue = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    std::array<uint8_t, 3> rgb{0, 255, 0};
    if (hex.size() != 6) return rgb;
    for (size_t i = 0; i < rgb.size(); ++i) {
        const int high = hexValue(hex[i * 2]);
        const int low = hexValue(hex[i * 2 + 1]);
        if (high < 0 || low < 0) return {0, 255, 0};
        rgb[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return rgb;
}

void applyEffect(
    std::vector<uint8_t>& pixels,
    int width,
    int height,
    const editor::Clip& clip,
    const editor::EffectInstance& effect,
    core::TimelineTime localTime
) {
    if (!effect.enabled) return;
    const auto params = editor::EffectParamAnimationEngine::resolveEffectParamsAtTime(
        clip, effect.id, localTime);

    if (effect.type == "blur" || effect.type == "gaussian_blur") {
        const double intensity = std::clamp(readEffectNumber(params, "intensity", 15.0), 0.0, 100.0);
        if (intensity <= 0.001) return;
        const double sigmaX = EffectDefinitionRegistry::intensityToSigma(intensity, width, 1920.0);
        const double sigmaY = EffectDefinitionRegistry::intensityToSigma(intensity, height, 1080.0);
        const int radius = std::clamp(static_cast<int>(std::ceil(std::max(sigmaX, sigmaY))), 1, 50);
        Compositor::applyGaussianBlur(pixels.data(), width, height, radius);
        return;
    }

    if (effect.type == "vignette") {
        const double amount = std::clamp(
            readEffectNumber(params, "amount", readEffectNumber(params, "intensity", 0.45)), 0.0, 1.0);
        const double softness = std::clamp(readEffectNumber(params, "softness", 0.55), 0.01, 1.0);
        const double roundness = std::clamp(readEffectNumber(params, "roundness", 1.0), 0.0, 1.0);
        Compositor::applyVignette(pixels.data(), width, height, amount, softness, roundness);
        return;
    }

    if (effect.type == "sharpen") {
        const double amount = std::clamp(readEffectNumber(params, "amount", 0.35), 0.0, 2.0);
        Compositor::applySharpen(pixels.data(), width, height, amount);
        return;
    }

    if (effect.type == "color-grading" || effect.type == "color_grading") {
        Compositor::applyColorGrading(pixels.data(), width, height, ColorGradingSettings{
            readEffectNumber(params, "brightness", 0.0),
            readEffectNumber(params, "contrast", 0.0),
            readEffectNumber(params, "saturation", 0.0),
            readEffectNumber(params, "exposure", 0.0),
            readEffectNumber(params, "temperature", 0.0),
            readEffectNumber(params, "tint", 0.0),
            readEffectNumber(params, "hue", 0.0),
            readEffectNumber(params, "gamma", 1.0)});
        return;
    }

    if (effect.type == "chroma-key" || effect.type == "chroma_key") {
        const auto keyColor = parseHexColor(readEffectString(params, "keyColor", "#00FF00"));
        Compositor::applyChromaKey(
            pixels.data(), width, height, keyColor[0], keyColor[1], keyColor[2],
            std::clamp(readEffectNumber(params, "similarity", 0.4), 0.01, 1.0),
            std::clamp(readEffectNumber(params, "smoothness", 0.1), 0.001, 1.0),
            std::clamp(readEffectNumber(params, "spill", 0.1), 0.0, 1.0));
        return;
    }

    if (effect.type == "grayscale" || effect.type == "monochrome") {
        const double amount = std::clamp(readEffectNumber(params, "intensity", 1.0), 0.0, 1.0);
        const size_t pixelCount = static_cast<size_t>(width) * height;
        for (size_t i = 0; i < pixelCount; ++i) {
            auto* pixel = pixels.data() + i * 4;
            const double gray = 0.299 * pixel[0] + 0.587 * pixel[1] + 0.114 * pixel[2];
            for (int channel = 0; channel < 3; ++channel) {
                pixel[channel] = static_cast<uint8_t>(std::clamp(
                    pixel[channel] + (gray - pixel[channel]) * amount, 0.0, 255.0));
            }
        }
        return;
    }

    if (effect.type == "invert" || effect.type == "invert-colors" || effect.type == "invert_colors") {
        const double amount = std::clamp(readEffectNumber(params, "intensity", 1.0), 0.0, 1.0);
        const size_t pixelCount = static_cast<size_t>(width) * height;
        for (size_t i = 0; i < pixelCount; ++i) {
            auto* pixel = pixels.data() + i * 4;
            for (int channel = 0; channel < 3; ++channel) {
                pixel[channel] = static_cast<uint8_t>(std::lround(
                    pixel[channel] + (255 - 2 * pixel[channel]) * amount));
            }
        }
    }
}

} // namespace

void ClipEffectRenderer::apply(
    std::vector<uint8_t>& rgbaPixels,
    int width,
    int height,
    const editor::Clip& clip,
    core::TimelineTime timelineTime
) {
    if (rgbaPixels.empty() || width <= 0 || height <= 0 ||
        rgbaPixels.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
        return;
    }

    const auto localTime = timelineTime - clip.startTime();
    for (const auto& effect : clip.effects()) {
        applyEffect(rgbaPixels, width, height, clip, effect, localTime);
    }
}

} // namespace catchim::render

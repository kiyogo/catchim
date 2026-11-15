#pragma once

#include "Transform.h"
#include <vector>
#include <cstdint>
#include <string>
#include <memory>

namespace catchim::render {

struct RenderLayer {
    std::string id;
    int32_t zIndex{0};
    Transform transform;
    int32_t sourceWidth{0};
    int32_t sourceHeight{0};
    std::vector<uint8_t> rgbaPixels; // Source RGBA buffer
};

struct CompositorOutput {
    int32_t width{1920};
    int32_t height{1080};
    std::vector<uint8_t> rgbaPixels;
};

class Compositor {
public:
    Compositor(int32_t width = 1920, int32_t height = 1080);

    void setCanvasSize(int32_t width, int32_t height);
    int32_t width() const noexcept { return width_; }
    int32_t height() const noexcept { return height_; }

    void clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255);

    void compositeLayer(const RenderLayer& layer);

    const CompositorOutput& getOutput() const noexcept { return output_; }

    // Builtin pixel processing filters
    static void applyColorGrading(
        uint8_t* pixels,
        int32_t w,
        int32_t h,
        double brightness,
        double contrast,
        double saturation
    );

    static void applyVignette(
        uint8_t* pixels,
        int32_t w,
        int32_t h,
        double amount,
        double softness,
        double roundness = 1.0
    );

    static void applyGaussianBlur(
        uint8_t* pixels,
        int32_t w,
        int32_t h,
        double radius
    );

    static void applyChromaKey(
        uint8_t* pixels,
        int32_t w,
        int32_t h,
        uint8_t keyR,
        uint8_t keyG,
        uint8_t keyB,
        double threshold = 0.4,
        double smoothness = 0.1
    );

    static void applyGrayscale(
        uint8_t* pixels,
        int32_t w,
        int32_t h
    );

    static void applyExtendedColorGrading(
        uint8_t* pixels,
        int32_t w,
        int32_t h,
        double brightness,
        double contrast,
        double saturation,
        double exposure = 0.0,
        double temperature = 0.0,
        double tint = 0.0,
        double gamma = 1.0
    );

    static void applyInvert(
        uint8_t* pixels,
        int32_t w,
        int32_t h
    );

    static void applySepia(
        uint8_t* pixels,
        int32_t w,
        int32_t h,
        double amount = 1.0
    );

private:
    int32_t width_{1920};
    int32_t height_{1080};
    CompositorOutput output_;
};

} // namespace catchim::render

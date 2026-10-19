#include "Compositor.h"
#include <algorithm>
#include <cmath>

#if defined(HAVE_QT6)
#include <QImage>
#include <QPainter>
#endif

namespace catchim::render {

Compositor::Compositor(int32_t width, int32_t height)
    : width_(width)
    , height_(height)
{
    output_.width = width_;
    output_.height = height_;
    output_.rgbaPixels.resize(width_ * height_ * 4, 0);
    clear(0, 0, 0, 255);
}

void Compositor::setCanvasSize(int32_t width, int32_t height) {
    if(width==width_&&height==height_)return;
    width_ = width;
    height_ = height;
    output_.width = width_;
    output_.height = height_;
    output_.rgbaPixels.resize(width_ * height_ * 4);
    clear(0, 0, 0, 255);
}

void Compositor::clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint8_t* dst = output_.rgbaPixels.data();
    size_t totalPixels = width_ * height_;
    for (size_t i = 0; i < totalPixels; ++i) {
        dst[i * 4 + 0] = r;
        dst[i * 4 + 1] = g;
        dst[i * 4 + 2] = b;
        dst[i * 4 + 3] = a;
    }
}

void Compositor::compositeLayer(const RenderLayer& layer) {
    if (layer.rgbaPixels.empty() || layer.sourceWidth <= 0 || layer.sourceHeight <= 0) {
        return;
    }

    double opacity = std::clamp(layer.transform.opacity, 0.0, 1.0);
    if (opacity <= 0.0) return;

    int32_t srcW = layer.sourceWidth;
    int32_t srcH = layer.sourceHeight;
    const uint8_t* src = layer.rgbaPixels.data();
    uint8_t* dst = output_.rgbaPixels.data();

    // Fast-path: 1:1 match canvas dimensions and default transform
    if (srcW == width_ && srcH == height_ && layer.transform.isDefault()) {
        for (size_t i = 0; i < static_cast<size_t>(srcW * srcH); ++i) {
            uint8_t sr = src[i * 4 + 0];
            uint8_t sg = src[i * 4 + 1];
            uint8_t sb = src[i * 4 + 2];
            uint8_t sa = static_cast<uint8_t>(src[i * 4 + 3] * opacity);

            if (sa == 255) {
                dst[i * 4 + 0] = sr;
                dst[i * 4 + 1] = sg;
                dst[i * 4 + 2] = sb;
                dst[i * 4 + 3] = 255;
            } else if (sa > 0) {
                float alpha = sa / 255.0f;
                float invAlpha = 1.0f - alpha;
                dst[i * 4 + 0] = static_cast<uint8_t>(sr * alpha + dst[i * 4 + 0] * invAlpha);
                dst[i * 4 + 1] = static_cast<uint8_t>(sg * alpha + dst[i * 4 + 1] * invAlpha);
                dst[i * 4 + 2] = static_cast<uint8_t>(sb * alpha + dst[i * 4 + 2] * invAlpha);
                dst[i * 4 + 3] = 255;
            }
        }
        return;
    }

#if defined(HAVE_QT6)
    // Hardware-accelerated transformation pipeline using QPainter
    QImage dstImg(output_.rgbaPixels.data(), width_, height_, width_ * 4, QImage::Format_RGBA8888);
    QImage srcImg(const_cast<uint8_t*>(src), srcW, srcH, srcW * 4, QImage::Format_RGBA8888);

    QPainter painter(&dstImg);
    if (painter.isActive()) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        // Blend modes
        const std::string& bm = layer.transform.blendMode;
        if (bm == "multiply") {
            painter.setCompositionMode(QPainter::CompositionMode_Multiply);
        } else if (bm == "screen") {
            painter.setCompositionMode(QPainter::CompositionMode_Screen);
        } else if (bm == "overlay") {
            painter.setCompositionMode(QPainter::CompositionMode_Overlay);
        } else if (bm == "darken") {
            painter.setCompositionMode(QPainter::CompositionMode_Darken);
        } else if (bm == "lighten") {
            painter.setCompositionMode(QPainter::CompositionMode_Lighten);
        } else if (bm == "color_dodge") {
            painter.setCompositionMode(QPainter::CompositionMode_ColorDodge);
        } else {
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        }

        painter.setOpacity(opacity);

        // Center on canvas + position offset
        double cx = (width_ / 2.0) + layer.transform.positionX;
        double cy = (height_ / 2.0) + layer.transform.positionY;
        painter.translate(cx, cy);

        // Rotation around center
        if (std::abs(layer.transform.rotate) > 0.001) {
            painter.rotate(layer.transform.rotate);
        }

        // Scale & Flip
        double sx = layer.transform.scaleX * (layer.transform.flipX ? -1.0 : 1.0);
        double sy = layer.transform.scaleY * (layer.transform.flipY ? -1.0 : 1.0);
        painter.scale(sx, sy);

        // Draw image centered at origin
        painter.drawImage(QRectF(-srcW / 2.0, -srcH / 2.0, srcW, srcH), srcImg);
        painter.end();
        return;
    }
#endif

    // Fallback software blitter (for non-Qt environment)
    double scaleX = layer.transform.scaleX;
    double scaleY = layer.transform.scaleY;
    int32_t targetW = static_cast<int32_t>(srcW * scaleX);
    int32_t targetH = static_cast<int32_t>(srcH * scaleY);
    if (targetW <= 0 || targetH <= 0) return;

    int32_t startX = static_cast<int32_t>((width_ - targetW) / 2 + layer.transform.positionX);
    int32_t startY = static_cast<int32_t>((height_ - targetH) / 2 + layer.transform.positionY);

    for (int32_t dy = 0; dy < targetH; ++dy) {
        int32_t cy = startY + dy;
        if (cy < 0 || cy >= height_) continue;

        int32_t sy = (dy * srcH) / targetH;
        if (layer.transform.flipY) sy = srcH - 1 - sy;

        for (int32_t dx = 0; dx < targetW; ++dx) {
            int32_t cx = startX + dx;
            if (cx < 0 || cx >= width_) continue;

            int32_t sx = (dx * srcW) / targetW;
            if (layer.transform.flipX) sx = srcW - 1 - sx;

            size_t srcIdx = (sy * srcW + sx) * 4;
            size_t dstIdx = (cy * width_ + cx) * 4;

            uint8_t sr = src[srcIdx + 0];
            uint8_t sg = src[srcIdx + 1];
            uint8_t sb = src[srcIdx + 2];
            uint8_t sa = static_cast<uint8_t>(src[srcIdx + 3] * opacity);

            if (sa == 255) {
                dst[dstIdx + 0] = sr;
                dst[dstIdx + 1] = sg;
                dst[dstIdx + 2] = sb;
                dst[dstIdx + 3] = 255;
            } else if (sa > 0) {
                float alpha = sa / 255.0f;
                float invAlpha = 1.0f - alpha;
                dst[dstIdx + 0] = static_cast<uint8_t>(sr * alpha + dst[dstIdx + 0] * invAlpha);
                dst[dstIdx + 1] = static_cast<uint8_t>(sg * alpha + dst[dstIdx + 1] * invAlpha);
                dst[dstIdx + 2] = static_cast<uint8_t>(sb * alpha + dst[dstIdx + 2] * invAlpha);
                dst[dstIdx + 3] = 255;
            }
        }
    }
}

void Compositor::applyColorGrading(
    uint8_t* pixels,
    int32_t w,
    int32_t h,
    double brightness,
    double contrast,
    double saturation
) {
    if (!pixels || w <= 0 || h <= 0) return;
    size_t total = w * h;

    float bright = static_cast<float>(brightness);
    float cont = static_cast<float>(contrast + 1.0);
    float sat = static_cast<float>(saturation + 1.0);

    for (size_t i = 0; i < total; ++i) {
        float r = pixels[i * 4 + 0] / 255.0f;
        float g = pixels[i * 4 + 1] / 255.0f;
        float b = pixels[i * 4 + 2] / 255.0f;

        // Brightness & Contrast
        r = (r - 0.5f) * cont + 0.5f + bright;
        g = (g - 0.5f) * cont + 0.5f + bright;
        b = (b - 0.5f) * cont + 0.5f + bright;

        // Saturation
        float gray = 0.299f * r + 0.587f * g + 0.114f * b;
        r = gray + (r - gray) * sat;
        g = gray + (g - gray) * sat;
        b = gray + (b - gray) * sat;

        pixels[i * 4 + 0] = static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f));
        pixels[i * 4 + 1] = static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f));
        pixels[i * 4 + 2] = static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f));
    }
}

inline float smoothstep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void Compositor::applyVignette(
    uint8_t* pixels,
    int32_t w,
    int32_t h,
    double amount,
    double softness
) {
    if (!pixels || w <= 0 || h <= 0 || amount <= 0.001) return;

    float cx = w / 2.0f;
    float cy = h / 2.0f;
    float maxDist = std::sqrt(cx * cx + cy * cy);
    float amt = static_cast<float>(amount);
    float soft = std::max(0.01f, static_cast<float>(softness));

    for (int32_t y = 0; y < h; ++y) {
        float dy = y - cy;
        for (int32_t x = 0; x < w; ++x) {
            float dx = x - cx;
            float dist = std::sqrt(dx * dx + dy * dy) / maxDist;
            float factor = std::clamp(1.0f - smoothstep(1.0f - soft, 1.0f, dist) * amt, 0.0f, 1.0f);

            size_t idx = (y * w + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>(pixels[idx + 0] * factor);
            pixels[idx + 1] = static_cast<uint8_t>(pixels[idx + 1] * factor);
            pixels[idx + 2] = static_cast<uint8_t>(pixels[idx + 2] * factor);
        }
    }
}

void Compositor::applyGaussianBlur(uint8_t* pixels, int32_t w, int32_t h, double radius) {
    if (!pixels || w <= 0 || h <= 0 || radius <= 0.5) return;
    int r = static_cast<int>(std::round(radius));
    r = std::clamp(r, 1, 50);

    std::vector<uint8_t> temp(w * h * 4);

    // Horizontal pass
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int32_t rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            int count = 0;
            for (int kx = -r; kx <= r; ++kx) {
                int px = std::clamp(x + kx, 0, w - 1);
                int idx = (y * w + px) * 4;
                rSum += pixels[idx + 0];
                gSum += pixels[idx + 1];
                bSum += pixels[idx + 2];
                aSum += pixels[idx + 3];
                count++;
            }
            int outIdx = (y * w + x) * 4;
            temp[outIdx + 0] = static_cast<uint8_t>(rSum / count);
            temp[outIdx + 1] = static_cast<uint8_t>(gSum / count);
            temp[outIdx + 2] = static_cast<uint8_t>(bSum / count);
            temp[outIdx + 3] = static_cast<uint8_t>(aSum / count);
        }
    }

    // Vertical pass
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int32_t rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            int count = 0;
            for (int ky = -r; ky <= r; ++ky) {
                int py = std::clamp(y + ky, 0, h - 1);
                int idx = (py * w + x) * 4;
                rSum += temp[idx + 0];
                gSum += temp[idx + 1];
                bSum += temp[idx + 2];
                aSum += temp[idx + 3];
                count++;
            }
            int outIdx = (y * w + x) * 4;
            pixels[outIdx + 0] = static_cast<uint8_t>(rSum / count);
            pixels[outIdx + 1] = static_cast<uint8_t>(gSum / count);
            pixels[outIdx + 2] = static_cast<uint8_t>(bSum / count);
            pixels[outIdx + 3] = static_cast<uint8_t>(aSum / count);
        }
    }
}

void Compositor::applyChromaKey(
    uint8_t* pixels,
    int32_t w,
    int32_t h,
    uint8_t keyR,
    uint8_t keyG,
    uint8_t keyB,
    double threshold,
    double smoothness
) {
    if (!pixels || w <= 0 || h <= 0) return;
    size_t total = w * h;

    float kR = keyR / 255.0f;
    float kG = keyG / 255.0f;
    float kB = keyB / 255.0f;
    float thresh = static_cast<float>(threshold);
    float smooth = std::max(0.001f, static_cast<float>(smoothness));

    for (size_t i = 0; i < total; ++i) {
        float r = pixels[i * 4 + 0] / 255.0f;
        float g = pixels[i * 4 + 1] / 255.0f;
        float b = pixels[i * 4 + 2] / 255.0f;

        float dist = std::sqrt((r - kR) * (r - kR) + (g - kG) * (g - kG) + (b - kB) * (b - kB));
        float mask = std::clamp((dist - thresh) / smooth, 0.0f, 1.0f);

        pixels[i * 4 + 3] = static_cast<uint8_t>(pixels[i * 4 + 3] * mask);
    }
}

void Compositor::applyGrayscale(uint8_t* pixels, int32_t w, int32_t h) {
    if (!pixels || w <= 0 || h <= 0) return;
    size_t total = w * h;
    for (size_t i = 0; i < total; ++i) {
        uint8_t r = pixels[i * 4 + 0];
        uint8_t g = pixels[i * 4 + 1];
        uint8_t b = pixels[i * 4 + 2];
        uint8_t gray = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
        pixels[i * 4 + 0] = gray;
        pixels[i * 4 + 1] = gray;
        pixels[i * 4 + 2] = gray;
    }
}

void Compositor::applyExtendedColorGrading(
    uint8_t* pixels,
    int32_t w,
    int32_t h,
    double brightness,
    double contrast,
    double saturation,
    double exposure,
    double temperature,
    double tint,
    double gamma
) {
    if (!pixels || w <= 0 || h <= 0) return;
    size_t total = static_cast<size_t>(w) * h;

    float bright = static_cast<float>(brightness);
    float cont = static_cast<float>(contrast + 1.0);
    float sat = static_cast<float>(saturation + 1.0);
    float expMult = static_cast<float>(std::pow(2.0, exposure));
    float temp = static_cast<float>(temperature);
    float tnt = static_cast<float>(tint);
    float invGamma = (gamma > 0.001) ? static_cast<float>(1.0 / gamma) : 1.0f;

    for (size_t i = 0; i < total; ++i) {
        float r = (pixels[i * 4 + 0] / 255.0f) * expMult;
        float g = (pixels[i * 4 + 1] / 255.0f) * expMult;
        float b = (pixels[i * 4 + 2] / 255.0f) * expMult;

        // Temperature & Tint
        r += temp * 0.1f + tnt * 0.05f;
        g -= tnt * 0.1f;
        b -= temp * 0.1f - tnt * 0.05f;

        // Brightness & Contrast
        r = (r - 0.5f) * cont + 0.5f + bright;
        g = (g - 0.5f) * cont + 0.5f + bright;
        b = (b - 0.5f) * cont + 0.5f + bright;

        // Saturation
        float gray = 0.299f * r + 0.587f * g + 0.114f * b;
        r = gray + (r - gray) * sat;
        g = gray + (g - gray) * sat;
        b = gray + (g - gray) * sat;

        // Gamma correction
        if (invGamma != 1.0f) {
            r = std::pow(std::max(0.0f, r), invGamma);
            g = std::pow(std::max(0.0f, g), invGamma);
            b = std::pow(std::max(0.0f, b), invGamma);
        }

        pixels[i * 4 + 0] = static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f));
        pixels[i * 4 + 1] = static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f));
        pixels[i * 4 + 2] = static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f));
    }
}

void Compositor::applyInvert(uint8_t* pixels, int32_t w, int32_t h) {
    if (!pixels || w <= 0 || h <= 0) return;
    size_t total = static_cast<size_t>(w) * h;
    for (size_t i = 0; i < total; ++i) {
        pixels[i * 4 + 0] = 255 - pixels[i * 4 + 0];
        pixels[i * 4 + 1] = 255 - pixels[i * 4 + 1];
        pixels[i * 4 + 2] = 255 - pixels[i * 4 + 2];
    }
}

void Compositor::applySepia(uint8_t* pixels, int32_t w, int32_t h, double amount) {
    if (!pixels || w <= 0 || h <= 0) return;
    size_t total = static_cast<size_t>(w) * h;
    float amt = static_cast<float>(std::clamp(amount, 0.0, 1.0));
    float invAmt = 1.0f - amt;

    for (size_t i = 0; i < total; ++i) {
        float r = pixels[i * 4 + 0];
        float g = pixels[i * 4 + 1];
        float b = pixels[i * 4 + 2];

        float sepiaR = std::min(255.0f, 0.393f * r + 0.769f * g + 0.189f * b);
        float sepiaG = std::min(255.0f, 0.349f * r + 0.686f * g + 0.168f * b);
        float sepiaB = std::min(255.0f, 0.272f * r + 0.534f * g + 0.131f * b);

        pixels[i * 4 + 0] = static_cast<uint8_t>(r * invAmt + sepiaR * amt);
        pixels[i * 4 + 1] = static_cast<uint8_t>(g * invAmt + sepiaG * amt);
        pixels[i * 4 + 2] = static_cast<uint8_t>(b * invAmt + sepiaB * amt);
    }
}

} // namespace catchim::render

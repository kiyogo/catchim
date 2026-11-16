#include "ui/widgets/ClipMotionPresetPreview.h"

#if defined(HAVE_QT6)
#include "render/TransitionEngine.h"
#include "render/Transform.h"
#include <QHash>
#include <QImage>
#include <QPainter>
#include <algorithm>
#include <cstring>
#include <vector>

namespace catchim::ui {
namespace {
QImage motionPreviewSource() {
    constexpr int width = 180;
    constexpr int height = 102;
    QImage source(":/web/effects/preview.jpg");
    if (source.isNull()) {
        source = QImage(width, height, QImage::Format_RGBA8888);
        source.fill(QColor("#52a9bd"));
    }
    source = source.scaled(width, height, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    return source.copy((source.width() - width) / 2, (source.height() - height) / 2, width, height)
        .convertToFormat(QImage::Format_RGBA8888);
}
} // namespace

QPixmap clipMotionPresetPreview(const QString& slot, const QString& type, double loopPeriodSeconds) {
    static QHash<QString, QPixmap> cache;
    const QString cacheKey = slot + QLatin1Char(':') + type + QLatin1Char(':') +
        QString::number(loopPeriodSeconds, 'f', 3);
    if (cache.contains(cacheKey)) return cache.value(cacheKey);
    QImage source = motionPreviewSource();
    QPixmap pixmap(source.size());
    pixmap.fill(QColor("#d1d5db"));
    const QRectF bounds(pixmap.rect());
    constexpr double sampleProgress = .43;
    QImage previewImage = source;
    QImage outgoing = source.mirrored(true, false);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (slot == "between") {
        painter.drawImage(bounds, outgoing);
        const auto base = render::TransitionEngine::baseType(type.toStdString());
        const std::string rawType = type.toStdString();
        const double progress = render::TransitionEngine::progressForType(rawType, sampleProgress);
        if (base == "cross_dissolve") {
            painter.setOpacity(progress);
            painter.drawImage(bounds, previewImage);
        } else if (base == "fade_black" || base == "fade_white") {
            std::vector<uint8_t> pixels(static_cast<size_t>(source.width()) * source.height() * 4);
            std::memcpy(pixels.data(), previewImage.constBits(), pixels.size());
            // Use the same two-phase treatment as the real compositor. Show
            // the incoming part of the transition so the preview remains
            // legible while still matching the selected easing variant.
            const double fadeProgress = render::TransitionEngine::progressForType(rawType, 0.8);
            const double opacity = render::TransitionEngine::applyFadeThroughColorTransition(
                pixels, fadeProgress, base == "fade_white");
            const QImage faded(pixels.data(), source.width(), source.height(), source.width() * 4,
                               QImage::Format_RGBA8888);
            painter.setOpacity(opacity);
            painter.drawImage(bounds, faded);
            painter.setOpacity(1.0);
        } else if (base == "wipe_left" || base == "wipe_right" || base == "wipe_up" ||
                   base == "wipe_down" || base == "iris" || base == "diamond" ||
                   base == "center_split" || base == "blinds") {
            int mode = 0;
            if (base == "wipe_right") mode = 1;
            else if (base == "wipe_down") mode = 2;
            else if (base == "wipe_up") mode = 3;
            else if (base == "iris") mode = 4;
            else if (base == "diamond") mode = 5;
            else if (base == "center_split") mode = 6;
            else if (base == "blinds") mode = 7;
            std::vector<uint8_t> pixels(static_cast<size_t>(source.width()) * source.height() * 4);
            std::memcpy(pixels.data(), previewImage.constBits(), pixels.size());
            render::TransitionEngine::applyWipeMask(pixels, source.width(), source.height(), progress, mode);
            const QImage revealed(pixels.data(), source.width(), source.height(), source.width() * 4,
                                  QImage::Format_RGBA8888);
            painter.drawImage(bounds, revealed);
        } else {
            render::Transform transform;
            render::TransitionEngine::applyIncomingClipTransform(
                transform, rawType, sampleProgress, bounds.width(), 1.0);
            painter.setOpacity(transform.opacity);
            painter.translate(bounds.center().x() + transform.positionX,
                              bounds.center().y() + transform.positionY);
            painter.rotate(transform.rotate);
            painter.scale(transform.scaleX, transform.scaleY);
            painter.drawImage(QRectF(-bounds.width() * .5, -bounds.height() * .5,
                                     bounds.width(), bounds.height()), previewImage);
        }
    } else {
        render::Transform transform;
        double wipeProgress = -1.0;
        bool wipeFromRight = false;
        int wipeMode = 0;
        const std::string rawType = type.toStdString();
        if (slot == "in") {
            render::TransitionEngine::applyClipInMotion(
                transform, rawType, sampleProgress, bounds.width(), bounds.height(), 1.0, 1.0,
                wipeProgress, wipeFromRight, &wipeMode);
        } else if (slot == "out") {
            render::TransitionEngine::applyClipOutMotion(
                transform, rawType, sampleProgress, bounds.width(), bounds.height(), 1.0, 1.0,
                wipeProgress, wipeFromRight, &wipeMode);
        } else {
            render::TransitionEngine::applyClipLoopMotion(
                transform, rawType, .13, loopPeriodSeconds, 1.0, 1.0);
        }
        if (wipeProgress >= 0.0) {
            std::vector<uint8_t> pixels(static_cast<size_t>(source.width()) * source.height() * 4);
            std::memcpy(pixels.data(), previewImage.constBits(), pixels.size());
            render::TransitionEngine::applyWipeMask(
                pixels, source.width(), source.height(), wipeProgress, wipeMode);
            previewImage = QImage(pixels.data(), source.width(), source.height(), source.width() * 4,
                                  QImage::Format_RGBA8888).copy();
        }
        painter.setOpacity(std::clamp(transform.opacity, 0.0, 1.0));
        painter.translate(bounds.center().x() + transform.positionX,
                          bounds.center().y() + transform.positionY);
        painter.rotate(transform.rotate);
        painter.scale(transform.scaleX, transform.scaleY);
        painter.drawImage(QRectF(-bounds.width() * .5, -bounds.height() * .5,
                                 bounds.width(), bounds.height()), previewImage);
    }
    cache.insert(cacheKey, pixmap);
    return pixmap;
}

} // namespace catchim::ui
#endif

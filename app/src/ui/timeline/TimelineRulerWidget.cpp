#include "TimelineRulerWidget.h"

#if defined(HAVE_QT6)
#include "ui/theme/Theme.h"
#include <QPainter>
#include <QMouseEvent>
#include <cmath>

namespace catchim::ui {

TimelineRulerWidget::TimelineRulerWidget(editor::EditorEngine& engine, QWidget* parent)
    : QWidget(parent)
    , engine_(engine)
{
    setFixedHeight(Metrics::timelineRulerHeight + Metrics::timelineBookmarkHeight);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

core::TimelineTime TimelineRulerWidget::pixelToTime(int pixelX) const {
    int contentX = pixelX - Metrics::trackLabelsWidth + scrollOffset_;
    double pixelsPerSecond = 50.0 * zoomFactor_;
    double sec = static_cast<double>(contentX) / pixelsPerSecond;
    const auto time = core::TimelineTime::fromSeconds(std::max(0.0, sec));
    const auto duration = engine_.project().totalDuration();
    return duration.ticks() > 0 ? std::min(time, duration) : time;
}

void TimelineRulerWidget::paintEvent(QPaintEvent* /* event */) {
    QPainter painter(this);
    const auto& palette = Theme::instance().palette();
    painter.fillRect(rect(), palette.panelBackground);

    double pixelsPerSecond = 50.0 * zoomFactor_;
    int w = width();

    // Draw bottom border across whole ruler
    painter.setPen(palette.border);
    painter.drawLine(0, height() - 1, w, height() - 1);

    // Left Track Labels Corner (Metrics::trackLabelsWidth)
    QRect cornerRect(0, 0, Metrics::trackLabelsWidth, height() - 1);
    painter.fillRect(cornerRect, palette.panelBackground);
    painter.setPen(palette.border);
    painter.drawLine(Metrics::trackLabelsWidth - 1, 0, Metrics::trackLabelsWidth - 1, height() - 1);

    // Clip ruler content area to right of track labels
    painter.save();
    painter.setClipRect(Metrics::trackLabelsWidth, 0, w - Metrics::trackLabelsWidth, height());

    // Draw ruler tick marks
    QFont rulerFont("Inter");
    rulerFont.setPixelSize(10);
    painter.setFont(rulerFont);

    double fps = std::max(1.,engine_.project().settings().fps.toFps());
    double pixelsPerFrame = pixelsPerSecond / fps;

    // Minimum label spacing in pixels to prevent text collision (matching web MIN_LABEL_SPACING_PX = 120)
    double minLabelSpacingPx = 120.0;
    double labelIntervalSec = 1.0;

    const int frameIntervals[] = { 2, 3, 5, 10, 15 };
    bool foundFrame = false;
    for (int fi : frameIntervals) {
        if (pixelsPerFrame * fi >= minLabelSpacingPx) {
            labelIntervalSec = static_cast<double>(fi) / fps;
            foundFrame = true;
            break;
        }
    }
    if (!foundFrame) {
        const int secMultipliers[] = { 1, 2, 3, 5, 10, 15, 30, 60, 120, 300, 600 };
        for (int sm : secMultipliers) {
            if (pixelsPerSecond * sm >= minLabelSpacingPx) {
                labelIntervalSec = sm;
                break;
            }
        }
    }

    // Tick interval matching web getRulerConfig
    double rawTickIntervalSec = labelIntervalSec;
    const int tickFrameIntervals[] = { 1, 2, 3, 5, 10, 15 };
    for (int fi : tickFrameIntervals) {
        if (pixelsPerFrame * fi >= 18.0) {
            rawTickIntervalSec = static_cast<double>(fi) / fps;
            break;
        }
    }
    int labelFrames = static_cast<int>(std::round(labelIntervalSec * fps));
    int tickFrames = static_cast<int>(std::round(rawTickIntervalSec * fps));
    double tickIntervalSec = rawTickIntervalSec;
    if (tickFrames > 0 && labelFrames % tickFrames != 0) {
        for (int fi : tickFrameIntervals) {
            if (labelFrames % fi == 0 && pixelsPerFrame * fi >= 18.0) {
                tickIntervalSec = static_cast<double>(fi) / fps;
                break;
            }
        }
    }

    double startSec = std::max(0.0, static_cast<double>(scrollOffset_) / pixelsPerSecond);
    double endSec = static_cast<double>(scrollOffset_ + w - Metrics::trackLabelsWidth) / pixelsPerSecond;

    int startTick = static_cast<int>(std::floor(startSec / tickIntervalSec));
    int endTick = static_cast<int>(std::ceil(endSec / tickIntervalSec));

    for (int t = startTick; t <= endTick; ++t) {
        double sec = t * tickIntervalSec;
        int x = Metrics::trackLabelsWidth + static_cast<int>(sec * pixelsPerSecond) - scrollOffset_;
        if (x < Metrics::trackLabelsWidth || x >= w) continue;

        double mod = std::fmod(sec + 0.0001, labelIntervalSec);
        bool isLabel = (mod < 0.001 || mod > labelIntervalSec - 0.001);

        if (isLabel) {
            // Format label text matching web formatRulerLabel
            QString text;
            double remSec = std::fmod(sec + 0.0001, 1.0);
            bool isSecondBoundary = (remSec < 0.001 || remSec > 0.999);
            if (isSecondBoundary) {
                int totalSec = static_cast<int>(std::round(sec));
                int mm = totalSec / 60;
                int ss = totalSec % 60;
                text = (totalSec == 0) ? "0:00" : QString("%1:%2").arg(mm, 2, 10, QChar('0')).arg(ss, 2, 10, QChar('0'));
            } else {
                int frameInSec = static_cast<int>(std::round(remSec * fps));
                text = QString("%1f").arg(frameInSec);
            }

            // Web: text-muted-foreground/85 absolute top-1 select-none text-[10px] leading-none
            painter.setPen(QColor(113, 113, 122, 217));
            int textX = (t == 0) ? (x + 4) : x;
            painter.drawText(textX, 4, 80, 14, Qt::AlignLeft | Qt::AlignTop, text);
        } else {
            // Web: border-muted-foreground/25 absolute top-1.5 h-1.5 border-l
            painter.setPen(QPen(QColor(113, 113, 122, 64), 1.0));
            painter.drawLine(x, 6, x, 12);
        }
    }

    // Draw Bookmarks Row (height 16px at bottom)
    auto* tl = engine_.activeTimeline();
    if (tl) {
        int bmY = Metrics::timelineRulerHeight + 2;
        for (const auto& bm : tl->bookmarks()) {
            double sec = bm.time.toSeconds();
            int x = Metrics::trackLabelsWidth + static_cast<int>(sec * pixelsPerSecond) - scrollOffset_;
            if (x >= Metrics::trackLabelsWidth - 4 && x < w + 4) {
                // Draw small diamond bookmark marker
                painter.setBrush(palette.primaryAccent);
                painter.setPen(Qt::NoPen);
                QPolygon poly;
                poly << QPoint(x, bmY)
                     << QPoint(x + 4, bmY + 5)
                     << QPoint(x, bmY + 10)
                     << QPoint(x - 4, bmY + 5);
                painter.drawPolygon(poly);
            }
        }
    }

    // Tick marks and bookmarks stay inside the time lane. The playhead head
    // overlaps its boundary by 6px, so draw it after restoring the clip.
    painter.restore();

    core::TimelineTime cur = engine_.playback().currentTime();
    int playheadX = Metrics::trackLabelsWidth + static_cast<int>(cur.toSeconds() * pixelsPerSecond) - scrollOffset_;
    if (playheadX >= Metrics::trackLabelsWidth && playheadX < w) {
        // Match the 1px line in TimelineTracksWidget, including at time zero.
        painter.setPen(QPen(palette.primaryAccent, 1.0));
        painter.drawLine(playheadX, 4, playheadX, height());

        // The full head remains visible over the track-label corner at t=0.
        painter.setBrush(palette.primaryAccent);
        painter.setPen(QPen(palette.primaryAccent, 1.0));
        painter.drawEllipse(playheadX - 6, 4, 12, 12);
    }

}

void TimelineRulerWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const auto current=engine_.playback().currentTime();
        const int headX=Metrics::trackLabelsWidth+static_cast<int>(current.toSeconds()*50.0*zoomFactor_)-scrollOffset_;
        const bool onHead=headX>=Metrics::trackLabelsWidth&&QRect(headX-6,4,13,13).contains(event->pos());
        if (onHead || event->pos().x() >= Metrics::trackLabelsWidth) {
            isDragging_ = true;
            dragOffsetX_=onHead?event->pos().x()-headX:0;
            emit seekRequested(onHead?current:pixelToTime(event->pos().x()));
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void TimelineRulerWidget::mouseMoveEvent(QMouseEvent* event) {
    if (isDragging_) {
        int clampedX = std::max(Metrics::trackLabelsWidth, event->pos().x()-dragOffsetX_);
        core::TimelineTime time = pixelToTime(clampedX);
        emit seekRequested(time);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void TimelineRulerWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (isDragging_ && event->button() == Qt::LeftButton) {
        isDragging_ = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

} // namespace catchim::ui
#endif

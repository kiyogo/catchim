#pragma once

#if defined(HAVE_QT6)
#include <QPixmap>
#include <QString>

namespace catchim::ui {

// Renders a frame from the same motion algorithms used by the clip renderer.
QPixmap clipMotionPresetPreview(const QString& slot, const QString& type,
                                double loopPeriodSeconds = 0.5, double progress = 0.43);

} // namespace catchim::ui
#endif

#pragma once

#if defined(HAVE_QT6)
#include <QPixmap>
#include <QString>

namespace catchim::ui {

// Same cached, algorithm-rendered still used by the motion preset gallery.
QPixmap clipMotionPresetPreview(const QString& slot, const QString& type,
                                double loopPeriodSeconds = 0.5);

} // namespace catchim::ui
#endif

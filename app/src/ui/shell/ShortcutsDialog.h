#pragma once

#if defined(HAVE_QT6)
#include <QDialog>
#ifndef Q_MOC_RUN
#include "ui/widgets/AppDialog.h"
#endif
#include <QString>
#include <vector>

namespace catchim::ui {

class ShortcutsDialog : public AppDialog {
    Q_OBJECT
public:
    explicit ShortcutsDialog(QWidget* parent = nullptr);

private:
    void setupUi();
};

} // namespace catchim::ui
#endif

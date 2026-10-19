#pragma once

#if defined(HAVE_QT6)
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include "editor/EditorEngine.h"

namespace catchim::ui {

class EditorHeader : public QWidget {
    Q_OBJECT
public:
    explicit EditorHeader(editor::EditorEngine& engine, QWidget* parent = nullptr);

    void refresh();

signals:
    void exportClicked();
    void themeToggleClicked();
    void openProjectRequested();
    void saveProjectRequested();
    void projectsHubRequested();

private slots:
    void onNameEditingFinished();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();

    editor::EditorEngine& engine_;
    QLineEdit* nameEdit_{nullptr};
    QPushButton* logoButton_{nullptr};
    QMenu* logoMenu_{nullptr};
    QPushButton* undoBtn_{nullptr};
    QPushButton* redoBtn_{nullptr};
    QPushButton* feedbackButton_{nullptr};
    QPushButton* exportButton_{nullptr};
    QPushButton* themeButton_{nullptr};
};

} // namespace catchim::ui
#endif

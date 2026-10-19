#include "EditorHeader.h"

#if defined(HAVE_QT6)
#include "ui/icons/UiIcons.h"
#include "ui/theme/Theme.h"
#include "ShortcutsDialog.h"
#include "ui/widgets/FeedbackDialog.h"
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QApplication>
#include <QMouseEvent>

namespace catchim::ui {

EditorHeader::EditorHeader(editor::EditorEngine& engine, QWidget* parent)
    : QWidget(parent)
    , engine_(engine)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(Metrics::headerHeight);
    setupUi();
    refresh();
}

void EditorHeader::setupUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 12, 0);
    layout->setSpacing(4);

    // Left side: Logo & Project Name
    logoButton_ = new QPushButton(this);
    logoButton_->setObjectName("headerLogoBtn");
    logoButton_->setToolTip("Menu dự án");
    logoButton_->setFixedSize(32, 32);
    logoButton_->setIconSize(QSize(20, 20));
    logoButton_->setCursor(Qt::PointingHandCursor);
    logoButton_->setStyleSheet(R"(
        QPushButton#headerLogoBtn {
            background: transparent;
            border: none;
            border-radius: 4px;
        }
        QPushButton#headerLogoBtn:hover {
            background-color: #242424;
        }
        QPushButton#headerLogoBtn::menu-indicator {
            image: none;
            width: 0px;
        }
    )");

    logoMenu_ = new QMenu(logoButton_);
    logoMenu_->setAttribute(Qt::WA_TranslucentBackground, true);
    logoMenu_->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    logoButton_->setMenu(logoMenu_);

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setObjectName("headerNameEdit");
    nameEdit_->setToolTip("Bấm để đổi tên dự án");
    nameEdit_->setFixedHeight(30);
    nameEdit_->setCursor(Qt::PointingHandCursor);
    nameEdit_->setFocusPolicy(Qt::StrongFocus);
    connect(nameEdit_, &QLineEdit::editingFinished, this, &EditorHeader::onNameEditingFinished);
    qApp->installEventFilter(this);

    layout->addWidget(logoButton_);
    layout->addWidget(nameEdit_);

    // Undo / Redo Quick Buttons (hidden to match web editor-header.tsx)
    undoBtn_ = new QPushButton(this);
    undoBtn_->setVisible(false);
    connect(undoBtn_, &QPushButton::clicked, [this]() {
        engine_.undo();
    });

    redoBtn_ = new QPushButton(this);
    redoBtn_->setVisible(false);
    connect(redoBtn_, &QPushButton::clicked, [this]() {
        engine_.redo();
    });

    layout->addStretch();

    // Right side: Feedback, Export Button & Theme toggle
    feedbackButton_ = new QPushButton("Gửi phản hồi", this);
    feedbackButton_->setToolTip("Gửi phản hồi");
    feedbackButton_->setFixedSize(110, 32);
    feedbackButton_->setCursor(Qt::PointingHandCursor);
    connect(feedbackButton_,&QPushButton::clicked,this,[this]{FeedbackDialog dialog(window());dialog.exec();});
    exportButton_ = new QPushButton("Xuất", this);
    exportButton_->setToolTip("Xuất video");
    exportButton_->setIcon(UiIcons::get(UiIcon::Export, QColor("#ffffff"), 14));
    exportButton_->setIconSize(QSize(14, 14));
    exportButton_->setFixedSize(86, 34);
    exportButton_->setCursor(Qt::PointingHandCursor);
    exportButton_->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:1, y1:0, x2:0, y2:0, stop:0 #2567ec, stop:1 #37b6f7);
            color: #ffffff;
            font-weight: 500;
            font-size: 14px;
            padding: 0px 12px;
            border-radius: 11px;
            border: 2px solid #83d9f1;
        }
        QPushButton:hover {
            background: qlineargradient(x1:1, y1:0, x2:0, y2:0, stop:0 #1d4ed8, stop:1 #0ea5e9);
        }
        QPushButton:pressed {
            background: #1e40af;
        }
    )");
    connect(exportButton_, &QPushButton::clicked, this, &EditorHeader::exportClicked);

    themeButton_ = new QPushButton(this);
    themeButton_->setObjectName("headerThemeBtn");
    themeButton_->setToolTip("Đổi giao diện sáng/tối");
    themeButton_->setFixedSize(32, 32);
    themeButton_->setIconSize(QSize(18, 18));
    themeButton_->setCursor(Qt::PointingHandCursor);
    themeButton_->setStyleSheet(R"(
        QPushButton#headerThemeBtn {
            background: transparent;
            border: none;
            border-radius: 4px;
        }
        QPushButton#headerThemeBtn:hover {
            background-color: #242424;
        }
    )");
    connect(themeButton_, &QPushButton::clicked, this, &EditorHeader::themeToggleClicked);

    layout->addWidget(feedbackButton_);
    layout->addSpacing(4);
    layout->addWidget(exportButton_);
    layout->addSpacing(4);
    layout->addWidget(themeButton_);
}

void EditorHeader::refresh() {
    const auto& pal = Theme::instance().palette();
    bool isDark = (Theme::instance().mode() == ThemeMode::Dark);

    if (nameEdit_) {
        const auto name = QString::fromStdString(engine_.project().name());
        nameEdit_->setText(name.compare("New project", Qt::CaseInsensitive) == 0 ? (editorEnglish()?"New project":"Dự án mới") : name);
        nameEdit_->setFixedWidth(nameEdit_->fontMetrics().horizontalAdvance(nameEdit_->text()) + 24);
        nameEdit_->setStyleSheet(QString(R"(
            QLineEdit#headerNameEdit {
                background: transparent;
                border: 1px solid transparent;
                border-radius: 4px;
                color: %1;
                font-size: 14px;
                font-weight: 400;
                padding: 2px 8px;
            }
            QLineEdit#headerNameEdit:hover {
                background-color: %2;
                border: 1px solid transparent;
            }
            QLineEdit#headerNameEdit:focus {
                background: %3;
                border: 1px solid %4;
                color: %1;
            }
        )").arg(pal.textPrimary.name())
           .arg(pal.hover.name())
           .arg(pal.panelBackground.name())
           .arg(pal.primaryAccent.name()));
    }

    if (logoMenu_) {
        logoMenu_->clear();
        logoMenu_->setAttribute(Qt::WA_TranslucentBackground, true);
        logoMenu_->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        logoMenu_->addAction(UiIcons::get(UiIcon::Folder, pal.textPrimary, 16), "Thoát dự án", [this]() {
            emit projectsHubRequested();
        });
        logoMenu_->addAction(UiIcons::get(UiIcon::Keyboard, pal.textPrimary, 16), "Phím tắt", [this]() {
            ShortcutsDialog dlg(this);
            dlg.exec();
        });
        logoMenu_->addSeparator();
        logoMenu_->addAction("Discord",[]{QDesktopServices::openUrl(QUrl("https://discord.com/invite/Mu3acKZvCp"));});
    }

    if (exportButton_) {
        exportButton_->setToolTip("Xuất video");
    }
    if (feedbackButton_) {
        feedbackButton_->setFixedWidth(editorEnglish()?132:110);
        feedbackButton_->setStyleSheet(QString(
            "QPushButton { background: transparent; color: %1; border: 1px solid %2; border-radius: 10px; font-size: 13px; padding: 0 12px; }"
            "QPushButton:hover { background: %3; }").arg(pal.textPrimary.name(), pal.border.name(), pal.hover.name()));
    }
    if (themeButton_) {
        themeButton_->setIcon(UiIcons::get(UiIcon::Sun, pal.textSecondary, 18));
        themeButton_->setToolTip(isDark ? "Chuyển sang giao diện sáng" : "Chuyển sang giao diện tối");
        themeButton_->setStyleSheet(QString(R"(
            QPushButton#headerThemeBtn {
                background: transparent;
                border: none;
                border-radius: 4px;
            }
            QPushButton#headerThemeBtn:hover {
                background-color: %1;
            }
        )").arg(pal.hover.name()));
    }
    if (logoButton_) {
        logoButton_->setIcon(UiIcons::get(UiIcon::Logo, pal.textPrimary, 20));
        logoButton_->setToolTip("Menu dự án");
        logoButton_->setStyleSheet(QString(R"(
            QPushButton#headerLogoBtn {
                background: transparent;
                border: none;
                border-radius: 4px;
            }
            QPushButton#headerLogoBtn:hover {
                background-color: %1;
            }
            QPushButton#headerLogoBtn::menu-indicator {
                image: none;
                width: 0px;
            }
        )").arg(pal.hover.name()));
    }
}

void EditorHeader::onNameEditingFinished() {
    if (nameEdit_->text() == "Dự án mới" &&
        QString::fromStdString(engine_.project().name()).compare("New project", Qt::CaseInsensitive) == 0) return;
    std::string newName = nameEdit_->text().trimmed().toStdString();
    if (!newName.empty()) {
        engine_.renameProject(newName);
    } else {
        refresh();
    }
}

bool EditorHeader::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress && nameEdit_ && nameEdit_->hasFocus()) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        auto* widget = qobject_cast<QWidget*>(watched);
        if (mouse->button() == Qt::LeftButton && watched != nameEdit_ &&
            (!widget || !nameEdit_->isAncestorOf(widget))) {
            nameEdit_->clearFocus();
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace catchim::ui
#endif

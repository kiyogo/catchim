#include "ShortcutsDialog.h"

#if defined(HAVE_QT6)
#include "ui/icons/UiIcons.h"
#include "ui/theme/Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>

namespace catchim::ui {

ShortcutsDialog::ShortcutsDialog(QWidget* parent)
    : AppDialog("Phím tắt",parent)
{
    setModal(true);
    setFixedSize(604,560);
    setupUi();
}

void ShortcutsDialog::setupUi() {
    const auto& pal = Theme::instance().palette();
    auto* rootLayout = bodyLayout();
    rootLayout->setSpacing(12);


    // Scroll Area
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* container = new QWidget(scroll);
    auto* cLayout = new QVBoxLayout(container);
    cLayout->setContentsMargins(0, 0, 8, 0);
    cLayout->setSpacing(16);

    struct ShortcutDef {
        QString action;
        QString keys;
    };

    struct CategoryDef {
        QString name;
        std::vector<ShortcutDef> shortcuts;
    };

    std::vector<CategoryDef> categories = {
        {
            "Playback & Navigation",
            {
                {"Play / Pause video", "Space"},
                {"Jump to timeline start", "Home"},
                {"Jump to timeline end", "End"},
                {"Previous frame", "Left"},
                {"Next frame", "Right"},
                {"Jump backward 5s", "Shift + Left"},
                {"Jump forward 5s", "Shift + Right"}
            }
        },
        {
            "Timeline Editing",
            {
                {"Split element at playhead", "S"},
                {"Split left at playhead", "Q"},
                {"Split right at playhead", "W"},
                {"Duplicate element", "Ctrl + D"},
                {"Delete selected element", "Delete / Backspace"},
                {"Add marker", "M"},
                {"Toggle snapping", "N"},
                {"Toggle ripple editing", "R"}
            }
        },
        {
            "History & Project",
            {
                {"Open projects hub", "Ctrl + Shift + O"},
                {"Open project", "Ctrl + O"},
                {"Save project", "Ctrl + S"},
                {"Undo", "Ctrl + Z"},
                {"Redo", "Ctrl + Y"},
                {"Select all elements", "Ctrl + A"},
                {"Deselect all elements", "Escape"}
            }
        },
        {
            "Preview & Export",
            {
                {"Toggle safe zones", "T"},
                {"Toggle fullscreen", "F11"},
                {"Export video", "Ctrl + E"}
            }
        }
    };

    const QMap<QString,QString> vi={
        {"Playback & Navigation","PHÁT & ĐIỀU HƯỚNG"},{"Timeline Editing","CHỈNH SỬA DÒNG THỜI GIAN"},{"History & Project","LỊCH SỬ & DỰ ÁN"},{"Preview & Export","XEM TRƯỚC & XUẤT"},
        {"Play / Pause video","Phát / Tạm dừng video"},{"Jump to timeline start","Về đầu dòng thời gian"},{"Jump to timeline end","Đến cuối dòng thời gian"},{"Previous frame","Khung hình trước"},{"Next frame","Khung hình sau"},{"Jump backward 5s","Lùi 5 giây"},{"Jump forward 5s","Tiến 5 giây"},
        {"Split element at playhead","Cắt tại đầu phát"},{"Split left at playhead","Cắt phần bên trái"},{"Split right at playhead","Cắt phần bên phải"},{"Duplicate element","Nhân đôi phần tử"},{"Delete selected element","Xóa phần tử đã chọn"},{"Add marker","Thêm dấu mốc"},{"Toggle snapping","Bật / Tắt bắt dính"},{"Toggle ripple editing","Bật / Tắt chỉnh sửa ripple"},
        {"Open projects hub","Mở danh sách dự án"},{"Open project","Mở dự án"},{"Save project","Lưu dự án"},{"Undo","Hoàn tác"},{"Redo","Làm lại"},{"Select all elements","Chọn tất cả phần tử"},{"Deselect all elements","Bỏ chọn tất cả"},{"Toggle safe zones","Bật / Tắt vùng an toàn"},{"Toggle fullscreen","Bật / Tắt toàn màn hình"},{"Export video","Xuất video"}
    };
    const auto localized=[&](const QString& text){return editorEnglish()?text:vi.value(text,text);};
    for (const auto& cat : categories) {
        auto* catHeader = new QLabel(localized(cat.name), container);
        catHeader->setStyleSheet(QString("font-size: 11px; font-weight: 700; color: %1; text-transform: uppercase; letter-spacing: 0.5px; padding-top: 4px;").arg(pal.primaryAccent.name()));
        cLayout->addWidget(catHeader);

        auto* catBox = new QWidget(container);
        auto* boxLayout = new QVBoxLayout(catBox);
        boxLayout->setContentsMargins(10, 8, 10, 8);
        boxLayout->setSpacing(8);
        catBox->setObjectName("shortcutCategory");
        catBox->setStyleSheet(QString("#shortcutCategory{background-color: %1; border: 1px solid %2; border-radius: 6px;} #shortcutCategory QLabel{background:transparent;border:none;}")
            .arg(pal.panelBackground.name()).arg(pal.border.name()));

        for (const auto& item : cat.shortcuts) {
            auto* row = new QHBoxLayout();
            row->setContentsMargins(0, 0, 0, 0);

            auto* descLabel = new QLabel(localized(item.action), catBox);
            descLabel->setStyleSheet(QString("font-size: 12px; color: %1; font-weight: 500;").arg(pal.textPrimary.name()));
            row->addWidget(descLabel);

            row->addStretch();

            // Render key badge
            auto* kbdLabel = new QLabel(item.keys, catBox);
            kbdLabel->setStyleSheet(QString(R"(
                QLabel {
                    background-color: %1;
                    color: %2;
                    font-family: 'Cascadia Code', Consolas, monospace;
                    font-size: 11px;
                    font-weight: 600;
                    padding: 3px 8px;
                    border: 1px solid %3;
                    border-radius: 4px;
                }
            )").arg(pal.secondary.name()).arg(pal.textPrimary.name()).arg(pal.border.name()));
            row->addWidget(kbdLabel);

            boxLayout->addLayout(row);
        }

        cLayout->addWidget(catBox);
    }

    cLayout->addStretch();
    scroll->setWidget(container);
    rootLayout->addWidget(scroll, 1);

    // Footer button
    auto* footerLayout = new QHBoxLayout();
    footerLayout->addStretch();
    auto* doneBtn = new QPushButton("Đóng", this);
    doneBtn->setFixedSize(80, 32);
    doneBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #0284c7;
            color: #ffffff;
            font-weight: 600;
            border-radius: 6px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #0369a1;
        }
    )");
    connect(doneBtn, &QPushButton::clicked, this, &QDialog::accept);
    footerLayout->addWidget(doneBtn);
    rootLayout->addLayout(footerLayout);
}

} // namespace catchim::ui
#endif

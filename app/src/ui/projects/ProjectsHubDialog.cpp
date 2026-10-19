#include "ProjectsHubDialog.h"

#if defined(HAVE_QT6)
#include "ui/icons/UiIcons.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/WebComboBox.h"
#include "ui/theme/Metrics.h"
#include "storage/ProjectStorage.h"
#include "editor/project/ProjectSerializer.h"
#include "core/utils/UuidGenerator.h"
#include "core/project/ProjectOrganizationEngine.h"

#include <QMenu>
#include <QMouseEvent>
#include <QMessageBox>
#include <QInputDialog>
#include <QPainter>
#include <QPainterPath>
#include <QHeaderView>
#include <QDateTime>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace catchim::ui {

// Card widget representing a single project in Grid View
class ProjectCardWidget : public QWidget {
public:
    ProjectCardWidget(const ProjectItemRecord& record, bool isSelected, ProjectsHubDialog* hub, QWidget* parent = nullptr)
        : QWidget(parent)
        , record_(record)
        , isSelected_(isSelected)
        , hub_(hub)
    {
        setCursor(Qt::PointingHandCursor);
        setFixedWidth(220);
        setFixedHeight(210);
        setupUi();
    }

    void setSelected(bool sel) {
        isSelected_ = sel;
        checkbox_->setChecked(sel);
        update();
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            bool nextState = !isSelected_;
            checkbox_->setChecked(nextState);
            if (nextState) {
                hub_->selectedProjectIds_.insert(record_.id);
            } else {
                hub_->selectedProjectIds_.erase(record_.id);
            }
            hub_->updateToolbarState();
            isSelected_ = nextState;
            update();
        }
        QWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            hub_->openProject(record_.filePath);
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QRect r = rect().adjusted(1, 1, -1, -1);
        QPainterPath path;
        path.addRoundedRect(r, 6, 6);

        if (isSelected_) {
            p.fillPath(path, QColor("#1a1a1a"));
            p.setPen(QPen(QColor("#38bdf8"), 2));
        } else if (underMouse()) {
            p.fillPath(path, QColor("#242424"));
            p.setPen(QPen(QColor("#383838"), 1.5));
        } else {
            p.fillPath(path, QColor("#1a1a1a"));
            p.setPen(QPen(QColor("#2e2e2e"), 1));
        }
        p.drawPath(path);
    }

private:
    void setupUi() {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(8);

        // 1. Thumbnail container (16:9 aspect)
        auto* thumbBox = new QWidget(this);
        thumbBox->setFixedHeight(115);
        thumbBox->setStyleSheet(R"(
            background: #18181b;
            border-radius: 6px;
            border: 1px solid #27272a;
        )");

        auto* thumbLayout = new QVBoxLayout(thumbBox);
        thumbLayout->setContentsMargins(6, 6, 6, 6);

        // Top row inside thumbnail: Checkbox & More button
        auto* topThumbRow = new QHBoxLayout();
        topThumbRow->setContentsMargins(0, 0, 0, 0);

        checkbox_ = new QCheckBox(thumbBox);
        checkbox_->setChecked(isSelected_);
        checkbox_->setStyleSheet(R"(
            QCheckBox::indicator {
                width: 18px;
                height: 18px;
                border-radius: 4px;
                border: 1px solid #52525b;
                background: #18181b;
            }
            QCheckBox::indicator:checked {
                background: #2567ec;
                border-color: #38bdf8;
            }
        )");
        connect(checkbox_, &QCheckBox::toggled, [this](bool checked) {
            isSelected_ = checked;
            if (checked) {
                hub_->selectedProjectIds_.insert(record_.id);
            } else {
                hub_->selectedProjectIds_.erase(record_.id);
            }
            hub_->updateToolbarState();
            update();
        });

        auto* moreBtn = new QPushButton(thumbBox);
        moreBtn->setFixedSize(24, 24);
        moreBtn->setIcon(UiIcons::get(UiIcon::MoreHorizontal, QColor("#a1a1aa"), 16));
        moreBtn->setStyleSheet(R"(
            QPushButton {
                background: rgba(24, 24, 27, 0.7);
                border: 1px solid #3f3f46;
                border-radius: 4px;
            }
            QPushButton:hover {
                background: #27272a;
            }
        )");
        moreBtn->setCursor(Qt::PointingHandCursor);

        const auto& pal = Theme::instance().palette();
        auto* menu = new QMenu(moreBtn);
        menu->setAttribute(Qt::WA_TranslucentBackground, true);
        menu->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        menu->addAction(UiIcons::get(UiIcon::Folder, pal.textPrimary, 14), "Open", [this]() {
            hub_->openProject(record_.filePath);
        });
        menu->addAction(UiIcons::get(UiIcon::Edit, pal.textPrimary, 14), "Rename...", [this]() {
            hub_->renameProject(record_.filePath);
        });
        menu->addAction(UiIcons::get(UiIcon::Duplicate, pal.textPrimary, 14), "Duplicate", [this]() {
            hub_->duplicateProject(record_.filePath);
        });
        menu->addAction(UiIcons::get(UiIcon::Info, pal.textPrimary, 14), "Info...", [this]() {
            hub_->showProjectInfo(record_);
        });
        menu->addSeparator();
        menu->addAction(UiIcons::get(UiIcon::Delete, pal.destructive, 14), "Delete", [this]() {
            hub_->deleteProject(record_.filePath);
        });
        moreBtn->setMenu(menu);

        topThumbRow->addWidget(checkbox_);
        topThumbRow->addStretch();
        topThumbRow->addWidget(moreBtn);
        thumbLayout->addLayout(topThumbRow);

        // Center: Video icon placeholder or resolution
        auto* centerLabel = new QLabel(thumbBox);
        centerLabel->setAlignment(Qt::AlignCenter);
        centerLabel->setPixmap(UiIcons::getPixmap(UiIcon::Media, QColor("#52525b"), 28));
        thumbLayout->addWidget(centerLabel, 1);

        // Bottom row inside thumbnail: duration badge
        auto* bottomThumbRow = new QHBoxLayout();
        bottomThumbRow->setContentsMargins(0, 0, 0, 0);
        bottomThumbRow->addStretch();

        int durSec = static_cast<int>(record_.durationSeconds);
        int mins = durSec / 60;
        int secs = durSec % 60;
        QString durText = QString("%1:%2")
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));

        auto* durBadge = new QLabel(durText, thumbBox);
        durBadge->setStyleSheet(R"(
            background: rgba(0, 0, 0, 0.75);
            color: #ffffff;
            font-size: 11px;
            font-weight: 700;
            padding: 2px 6px;
            border-radius: 4px;
        )");
        bottomThumbRow->addWidget(durBadge);
        thumbLayout->addLayout(bottomThumbRow);

        layout->addWidget(thumbBox);

        // 2. Project Name
        auto* titleLabel = new QLabel(QString::fromStdString(record_.name), this);
        titleLabel->setStyleSheet("color: #f4f4f5; font-size: 13px; font-weight: 600;");
        titleLabel->setWordWrap(false);
        titleLabel->setToolTip(QString::fromStdString(record_.name));
        layout->addWidget(titleLabel);

        // 3. Date Modified / Created row
        auto* metaRow = new QHBoxLayout();
        metaRow->setContentsMargins(0, 0, 0, 0);
        metaRow->setSpacing(4);

        auto* clockIcon = new QLabel(this);
        clockIcon->setPixmap(UiIcons::getPixmap(UiIcon::Clock, QColor("#71717a"), 14));
        clockIcon->setFixedSize(14, 14);

        QString dateStr = QString::fromStdString(record_.updatedAt.empty() ? record_.createdAt : record_.updatedAt);
        if (dateStr.isEmpty()) {
            dateStr = "Today";
        } else if (dateStr.length() > 10) {
            dateStr = dateStr.left(10);
        }

        auto* dateLabel = new QLabel(dateStr, this);
        dateLabel->setStyleSheet("color: #71717a; font-size: 11px;");

        metaRow->addWidget(clockIcon);
        metaRow->addWidget(dateLabel);
        metaRow->addStretch();

        layout->addLayout(metaRow);
    }

    ProjectItemRecord record_;
    bool isSelected_{false};
    ProjectsHubDialog* hub_{nullptr};
    QCheckBox* checkbox_{nullptr};
};

// -----------------------------------------------------------------------------
// ProjectsHubDialog Implementation
// -----------------------------------------------------------------------------

ProjectsHubDialog::ProjectsHubDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Catchim - Projects");
    setWindowIcon(UiIcons::get(UiIcon::Folder, QColor("#38bdf8"), 24));
    resize(1000, 680);
    setMinimumSize(800, 500);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(R"(
        QDialog {
            background-color: #09090b;
            color: #f4f4f5;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
        }
        QScrollBar:vertical {
            background: #09090b;
            width: 8px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background: #27272a;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover {
            background: #3f3f46;
        }
    )");

    setupUi();
    loadProjectsFromDisk();
}

void ProjectsHubDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(16);

    // =========================================================================
    // 1. TOP HEADER (Breadcrumb, Search, Mode toggle, New Project)
    // =========================================================================
    auto* topHeader = new QHBoxLayout();
    topHeader->setContentsMargins(0, 0, 0, 0);
    topHeader->setSpacing(12);

    // Breadcrumb: Catchim / Projects
    auto* breadcrumbLayout = new QHBoxLayout();
    breadcrumbLayout->setSpacing(6);

    auto* brandLabel = new QLabel("Catchim", this);
    brandLabel->setStyleSheet("color: #808080; font-size: 15px; font-weight: 500;");
    auto* sepLabel = new QLabel("/", this);
    sepLabel->setStyleSheet("color: #383838; font-size: 15px; font-weight: 500;");
    auto* allProjTitle = new QLabel("Projects", this);
    allProjTitle->setStyleSheet("color: #dedede; font-size: 16px; font-weight: 700;");

    breadcrumbLayout->addWidget(brandLabel);
    breadcrumbLayout->addWidget(sepLabel);
    breadcrumbLayout->addWidget(allProjTitle);
    topHeader->addLayout(breadcrumbLayout);

    // View Mode Switcher: Grid vs List
    auto* viewModeBox = new QWidget(this);
    viewModeBox->setStyleSheet("background: #18181b; border: 1px solid #27272a; border-radius: 6px;");
    auto* vmLayout = new QHBoxLayout(viewModeBox);
    vmLayout->setContentsMargins(2, 2, 2, 2);
    vmLayout->setSpacing(2);

    gridViewBtn_ = new QPushButton(viewModeBox);
    gridViewBtn_->setFixedSize(30, 28);
    gridViewBtn_->setIcon(UiIcons::get(UiIcon::Grid, QColor("#38bdf8"), 16));
    gridViewBtn_->setStyleSheet("background: #27272a; border: none; border-radius: 4px;");
    gridViewBtn_->setToolTip("Grid View");
    connect(gridViewBtn_, &QPushButton::clicked, [this]() { setViewMode(ProjectViewMode::Grid); });

    listViewBtn_ = new QPushButton(viewModeBox);
    listViewBtn_->setFixedSize(30, 28);
    listViewBtn_->setIcon(UiIcons::get(UiIcon::List, QColor("#71717a"), 16));
    listViewBtn_->setStyleSheet("background: transparent; border: none; border-radius: 4px;");
    listViewBtn_->setToolTip("List View");
    connect(listViewBtn_, &QPushButton::clicked, [this]() { setViewMode(ProjectViewMode::List); });

    vmLayout->addWidget(gridViewBtn_);
    vmLayout->addWidget(listViewBtn_);
    topHeader->addWidget(viewModeBox);

    topHeader->addStretch();

    // Search bar
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setToolTip("Search projects by name");
    searchEdit_->setPlaceholderText("Search projects...");
    searchEdit_->setFixedWidth(260);
    searchEdit_->setFixedHeight(36);
    searchEdit_->setStyleSheet(R"(
        QLineEdit {
            background: #18181b;
            color: #f4f4f5;
            border: 1px solid #27272a;
            border-radius: 6px;
            padding: 0px 12px;
            font-size: 13px;
        }
        QLineEdit:focus {
            border: 1px solid #38bdf8;
        }
    )");
    connect(searchEdit_, &QLineEdit::textChanged, this, &ProjectsHubDialog::onSearchChanged);
    topHeader->addWidget(searchEdit_);

    // + New project Button
    newProjectBtn_ = new QPushButton("+ New project", this);
    newProjectBtn_->setToolTip("Create a new project");
    newProjectBtn_->setFixedHeight(36);
    newProjectBtn_->setCursor(Qt::PointingHandCursor);
    newProjectBtn_->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2567ec, stop:1 #38bdf8);
            color: #ffffff;
            font-size: 13px;
            font-weight: 700;
            padding: 0px 16px;
            border-radius: 6px;
            border: none;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1d4ed8, stop:1 #0284c7);
        }
        QPushButton:pressed {
            background: #1e40af;
        }
    )");
    connect(newProjectBtn_, &QPushButton::clicked, this, &ProjectsHubDialog::onCreateNewProject);
    topHeader->addWidget(newProjectBtn_);

    mainLayout->addLayout(topHeader);

    // =========================================================================
    // 2. TOOLBAR (Select all, Sort Dropdown, Sort Order, Batch actions)
    // =========================================================================
    auto* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(12);

    selectAllCheckbox_ = new QCheckBox("Select all", this);
    selectAllCheckbox_->setToolTip("Select or deselect all projects");
    selectAllCheckbox_->setStyleSheet(R"(
        QCheckBox {
            color: #a1a1aa;
            font-size: 13px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 4px;
            border: 1px solid #52525b;
            background: #18181b;
        }
        QCheckBox::indicator:checked {
            background: #2567ec;
            border-color: #38bdf8;
        }
    )");
    connect(selectAllCheckbox_, &QCheckBox::checkStateChanged, this, &ProjectsHubDialog::onSelectAllChanged);
    toolbar->addWidget(selectAllCheckbox_);

    // Separator line
    auto* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFrameShadow(QFrame::Sunken);
    sep1->setFixedHeight(18);
    sep1->setStyleSheet("color: #27272a;");
    toolbar->addWidget(sep1);

    // Sort ComboBox
    auto* sortLabel = new QLabel("Sort by:", this);
    sortLabel->setStyleSheet("color: #71717a; font-size: 13px;");
    toolbar->addWidget(sortLabel);

    sortCombo_ = new WebComboBox(this);
    sortCombo_->setToolTip("Sort projects by field");
    sortCombo_->setFixedHeight(32);
    sortCombo_->addItem("Last modified", static_cast<int>(ProjectSortField::Modified));
    sortCombo_->addItem("Creation date", static_cast<int>(ProjectSortField::Created));
    sortCombo_->addItem("Name", static_cast<int>(ProjectSortField::Name));
    sortCombo_->addItem("Duration", static_cast<int>(ProjectSortField::Duration));
    connect(sortCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProjectsHubDialog::onSortFieldChanged);
    toolbar->addWidget(sortCombo_);

    // Sort direction toggle button
    sortOrderBtn_ = new QPushButton(this);
    sortOrderBtn_->setFixedSize(32, 32);
    sortOrderBtn_->setIcon(UiIcons::get(UiIcon::ArrowDown, QColor("#a1a1aa"), 16));
    sortOrderBtn_->setStyleSheet(R"(
        QPushButton {
            background: #18181b;
            border: 1px solid #27272a;
            border-radius: 6px;
        }
        QPushButton:hover {
            background: #27272a;
            color: #38bdf8;
        }
    )");
    sortOrderBtn_->setToolTip("Toggle sort order");
    connect(sortOrderBtn_, &QPushButton::clicked, this, &ProjectsHubDialog::onToggleSortOrder);
    toolbar->addWidget(sortOrderBtn_);

    // Batch Actions Container (Duplicate, Delete)
    batchActionsContainer_ = new QWidget(this);
    auto* batchLayout = new QHBoxLayout(batchActionsContainer_);
    batchLayout->setContentsMargins(0, 0, 0, 0);
    batchLayout->setSpacing(8);

    selectionCountLabel_ = new QLabel("0 selected", batchActionsContainer_);
    selectionCountLabel_->setStyleSheet("color: #38bdf8; font-size: 12px; font-weight: 600;");
    batchLayout->addWidget(selectionCountLabel_);

    batchDuplicateBtn_ = new QPushButton("Duplicate", batchActionsContainer_);
    batchDuplicateBtn_->setToolTip("Duplicate selected projects");
    batchDuplicateBtn_->setFixedHeight(30);
    batchDuplicateBtn_->setIcon(UiIcons::get(UiIcon::Duplicate, QColor("#f4f4f5"), 14));
    batchDuplicateBtn_->setStyleSheet(R"(
        QPushButton {
            background: #27272a;
            color: #f4f4f5;
            border-radius: 4px;
            padding: 0px 10px;
            font-size: 12px;
            border: 1px solid #3f3f46;
        }
        QPushButton:hover {
            background: #3f3f46;
        }
    )");
    connect(batchDuplicateBtn_, &QPushButton::clicked, this, &ProjectsHubDialog::onDuplicateSelected);
    batchLayout->addWidget(batchDuplicateBtn_);

    batchDeleteBtn_ = new QPushButton("Delete", batchActionsContainer_);
    batchDeleteBtn_->setToolTip("Delete selected projects");
    batchDeleteBtn_->setFixedHeight(30);
    batchDeleteBtn_->setIcon(UiIcons::get(UiIcon::Delete, QColor("#ef4444"), 14));
    batchDeleteBtn_->setStyleSheet(R"(
        QPushButton {
            background: #451a1a;
            color: #ef4444;
            border-radius: 4px;
            padding: 0px 10px;
            font-size: 12px;
            border: 1px solid #7f1d1d;
        }
        QPushButton:hover {
            background: #7f1d1d;
            color: #ffffff;
        }
    )");
    connect(batchDeleteBtn_, &QPushButton::clicked, this, &ProjectsHubDialog::onDeleteSelected);
    batchLayout->addWidget(batchDeleteBtn_);

    batchActionsContainer_->setVisible(false);
    toolbar->addWidget(batchActionsContainer_);

    toolbar->addStretch();
    mainLayout->addLayout(toolbar);

    // =========================================================================
    // 3. MAIN CONTENT: STACKED WIDGET (Grid / List / Empty State)
    // =========================================================================
    viewStack_ = new QStackedWidget(this);

    // Page 0: Grid View (ScrollArea -> Flow/Grid)
    gridScrollArea_ = new QScrollArea(viewStack_);
    gridScrollArea_->setWidgetResizable(true);
    gridScrollArea_->setFrameShape(QFrame::NoFrame);
    gridScrollArea_->setStyleSheet("background: transparent;");

    gridContainer_ = new QWidget(gridScrollArea_);
    gridContainer_->setStyleSheet("background: transparent;");
    gridLayout_ = new QGridLayout(gridContainer_);
    gridLayout_->setContentsMargins(0, 8, 0, 8);
    gridLayout_->setSpacing(16);
    gridLayout_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    gridScrollArea_->setWidget(gridContainer_);

    viewStack_->addWidget(gridScrollArea_); // index 0

    // Page 1: List View (QTableWidget)
    listTable_ = new QTableWidget(viewStack_);
    listTable_->setColumnCount(6);
    listTable_->setHorizontalHeaderLabels({"Name", "Duration", "Resolution", "FPS", "Modified", "Actions"});
    listTable_->horizontalHeader()->setStretchLastSection(false);
    listTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    listTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    listTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    listTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    listTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    listTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    listTable_->verticalHeader()->setVisible(false);
    listTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    listTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    listTable_->setShowGrid(false);
    listTable_->setStyleSheet(R"(
        QTableWidget {
            background-color: #121215;
            color: #f4f4f5;
            border: 1px solid #27272a;
            border-radius: 8px;
            font-size: 13px;
        }
        QHeaderView::section {
            background-color: #18181b;
            color: #a1a1aa;
            font-size: 12px;
            font-weight: 600;
            padding: 8px 12px;
            border: none;
            border-bottom: 1px solid #27272a;
        }
        QTableWidget::item {
            padding: 8px 12px;
            border-bottom: 1px solid #1f1f23;
        }
        QTableWidget::item:selected {
            background-color: #1e293b;
            color: #38bdf8;
        }
    )");
    connect(listTable_, &QTableWidget::cellDoubleClicked, [this](int row, int /*col*/) {
        if (row >= 0 && row < static_cast<int>(displayedProjects_.size())) {
            openProject(displayedProjects_[row].filePath);
        }
    });

    viewStack_->addWidget(listTable_); // index 1

    // Page 2: Empty State
    emptyStateWidget_ = new QWidget(viewStack_);
    auto* emptyLayout = new QVBoxLayout(emptyStateWidget_);
    emptyLayout->setAlignment(Qt::AlignCenter);
    emptyLayout->setSpacing(14);

    auto* emptyIcon = new QLabel(emptyStateWidget_);
    emptyIcon->setPixmap(UiIcons::getPixmap(UiIcon::Video, QColor("#808080"), 48));
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon);

    auto* emptyTitle = new QLabel("No projects yet", emptyStateWidget_);
    emptyTitle->setStyleSheet("color: #dedede; font-size: 18px; font-weight: 700;");
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);

    auto* emptyDesc = new QLabel("Start creating your first project. Import media, edit, and export your videos. All privately.", emptyStateWidget_);
    emptyDesc->setStyleSheet("color: #808080; font-size: 13px;");
    emptyDesc->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyDesc);

    auto* emptyCreateBtn = new QPushButton("+ New project", emptyStateWidget_);
    emptyCreateBtn->setFixedHeight(38);
    emptyCreateBtn->setFixedWidth(160);
    emptyCreateBtn->setCursor(Qt::PointingHandCursor);
    emptyCreateBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2567ec, stop:1 #38bdf8);
            color: #ffffff;
            font-size: 13px;
            font-weight: 700;
            border-radius: 6px;
            border: none;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1d4ed8, stop:1 #0284c7);
        }
    )");
    connect(emptyCreateBtn, &QPushButton::clicked, this, &ProjectsHubDialog::onCreateNewProject);
    emptyLayout->addWidget(emptyCreateBtn, 0, Qt::AlignCenter);

    viewStack_->addWidget(emptyStateWidget_); // index 2

    mainLayout->addWidget(viewStack_, 1);
}

void ProjectsHubDialog::loadProjectsFromDisk() {
    allProjects_.clear();
    auto paths = storage::ProjectStorage::listProjects();

    for (const auto& path : paths) {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file.is_open()) continue;

        try {
            nlohmann::json j;
            file >> j;

            ProjectItemRecord rec;
            rec.filePath = path;
            std::error_code ec;
            rec.fileSizeBytes = std::filesystem::file_size(path, ec);

            if (j.contains("metadata")) {
                const auto& meta = j["metadata"];
                rec.id = meta.value("id", path.stem().string());
                rec.name = meta.value("name", path.stem().string());
                rec.thumbnail = meta.value("thumbnail", "");
                rec.createdAt = meta.value("createdAt", "");
                rec.updatedAt = meta.value("updatedAt", "");
            } else {
                rec.id = path.stem().string();
                rec.name = path.stem().string();
            }

            if (j.contains("settings")) {
                const auto& st = j["settings"];
                if (st.contains("canvasSize")) {
                    rec.width = st["canvasSize"].value("width", 1920);
                    rec.height = st["canvasSize"].value("height", 1080);
                }
                rec.fps = st.value("fps", 30.0);
            }

            // Calculate duration and clip count from tracks
            if (j.contains("scenes") && j["scenes"].is_array() && !j["scenes"].empty()) {
                const auto& scene = j["scenes"][0];
                if (scene.contains("tracks") && scene["tracks"].is_array()) {
                    rec.trackCount = scene["tracks"].size();
                    int64_t maxEndMicros = 0;
                    for (const auto& track : scene["tracks"]) {
                        if (track.contains("clips") && track["clips"].is_array()) {
                            rec.clipCount += track["clips"].size();
                            for (const auto& clip : track["clips"]) {
                                int64_t start = clip.value("startTime", 0LL);
                                int64_t dur = clip.value("duration", 0LL);
                                maxEndMicros = std::max(maxEndMicros, start + dur);
                            }
                        }
                    }
                    rec.durationSeconds = static_cast<double>(maxEndMicros) / 1000000.0;
                }
            }

            allProjects_.push_back(rec);
        } catch (...) {
            // Ignore malformed files
        }
    }

    refreshFilteredList();
}

void ProjectsHubDialog::refreshFilteredList() {
    displayedProjects_.clear();

    QString query = searchQuery_.trimmed().toLower();
    for (const auto& proj : allProjects_) {
        if (!query.isEmpty()) {
            QString name = QString::fromStdString(proj.name).toLower();
            if (!name.contains(query)) {
                continue;
            }
        }
        displayedProjects_.push_back(proj);
    }

    // Sort
    std::sort(displayedProjects_.begin(), displayedProjects_.end(), [this](const ProjectItemRecord& a, const ProjectItemRecord& b) {
        if (sortField_ == ProjectSortField::Modified) {
            if (a.updatedAt != b.updatedAt) {
                return sortAscending_ ? (a.updatedAt < b.updatedAt) : (a.updatedAt > b.updatedAt);
            }
        } else if (sortField_ == ProjectSortField::Created) {
            if (a.createdAt != b.createdAt) {
                return sortAscending_ ? (a.createdAt < b.createdAt) : (a.createdAt > b.createdAt);
            }
        } else if (sortField_ == ProjectSortField::Name) {
            int cmp = QString::compare(QString::fromStdString(a.name), QString::fromStdString(b.name), Qt::CaseInsensitive);
            if (cmp != 0) {
                return sortAscending_ ? (cmp < 0) : (cmp > 0);
            }
        } else if (sortField_ == ProjectSortField::Duration) {
            if (std::abs(a.durationSeconds - b.durationSeconds) > 0.001) {
                return sortAscending_ ? (a.durationSeconds < b.durationSeconds) : (a.durationSeconds > b.durationSeconds);
            }
        }
        // Strict deterministic tie-breaker
        return a.id < b.id;
    });

    if (displayedProjects_.empty()) {
        viewStack_->setCurrentIndex(2); // Empty state
    } else if (viewMode_ == ProjectViewMode::Grid) {
        viewStack_->setCurrentIndex(0);
        renderGridView();
    } else {
        viewStack_->setCurrentIndex(1);
        renderListView();
    }

    updateToolbarState();
}

void ProjectsHubDialog::renderGridView() {
    // Clear previous items in grid
    QLayoutItem* child;
    while ((child = gridLayout_->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    const int columns = 4;
    for (size_t i = 0; i < displayedProjects_.size(); ++i) {
        const auto& rec = displayedProjects_[i];
        bool isSel = selectedProjectIds_.find(rec.id) != selectedProjectIds_.end();
        auto* card = new ProjectCardWidget(rec, isSel, this, gridContainer_);
        int r = static_cast<int>(i / columns);
        int c = static_cast<int>(i % columns);
        gridLayout_->addWidget(card, r, c);
    }
}

void ProjectsHubDialog::renderListView() {
    listTable_->setRowCount(static_cast<int>(displayedProjects_.size()));

    for (int i = 0; i < static_cast<int>(displayedProjects_.size()); ++i) {
        const auto& rec = displayedProjects_[i];

        // 0. Name
        auto* nameItem = new QTableWidgetItem(UiIcons::get(UiIcon::Media, QColor("#38bdf8"), 16), QString::fromStdString(rec.name));
        nameItem->setFont(QFont(font().family(), 10, QFont::DemiBold));
        listTable_->setItem(i, 0, nameItem);

        // 1. Duration
        int durSec = static_cast<int>(rec.durationSeconds);
        QString durText = QString("%1:%2")
            .arg(durSec / 60, 2, 10, QChar('0'))
            .arg(durSec % 60, 2, 10, QChar('0'));
        auto* durItem = new QTableWidgetItem(durText);
        durItem->setTextAlignment(Qt::AlignCenter);
        listTable_->setItem(i, 1, durItem);

        // 2. Resolution
        auto* resItem = new QTableWidgetItem(QString("%1x%2").arg(rec.width).arg(rec.height));
        resItem->setTextAlignment(Qt::AlignCenter);
        listTable_->setItem(i, 2, resItem);

        // 3. FPS
        auto* fpsItem = new QTableWidgetItem(QString::number(rec.fps, 'f', 0) + " fps");
        fpsItem->setTextAlignment(Qt::AlignCenter);
        listTable_->setItem(i, 3, fpsItem);

        // 4. Updated date
        QString dateStr = QString::fromStdString(rec.updatedAt.empty() ? rec.createdAt : rec.updatedAt);
        if (dateStr.length() > 16) dateStr = dateStr.left(16);
        auto* dateItem = new QTableWidgetItem(dateStr);
        dateItem->setTextAlignment(Qt::AlignCenter);
        listTable_->setItem(i, 4, dateItem);

        // 5. Actions button
        auto* actionBtn = new QPushButton("Actions", listTable_);
        actionBtn->setFixedSize(70, 26);
        actionBtn->setStyleSheet(R"(
            QPushButton {
                background: #27272a;
                color: #f4f4f5;
                border-radius: 4px;
                border: 1px solid #3f3f46;
                font-size: 11px;
            }
            QPushButton:hover {
                background: #3f3f46;
                color: #38bdf8;
            }
        )");

        const auto& pal = Theme::instance().palette();
        auto* menu = new QMenu(actionBtn);
        menu->setAttribute(Qt::WA_TranslucentBackground, true);
        menu->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        menu->addAction(UiIcons::get(UiIcon::Folder, pal.textPrimary, 14), "Open", [this, path = rec.filePath]() { openProject(path); });
        menu->addAction(UiIcons::get(UiIcon::Edit, pal.textPrimary, 14), "Rename...", [this, path = rec.filePath]() { renameProject(path); });
        menu->addAction(UiIcons::get(UiIcon::Duplicate, pal.textPrimary, 14), "Duplicate", [this, path = rec.filePath]() { duplicateProject(path); });
        menu->addAction(UiIcons::get(UiIcon::Info, pal.textPrimary, 14), "Info...", [this, rec]() { showProjectInfo(rec); });
        menu->addSeparator();
        menu->addAction(UiIcons::get(UiIcon::Delete, pal.destructive, 14), "Delete", [this, path = rec.filePath]() { deleteProject(path); });
        actionBtn->setMenu(menu);

        listTable_->setCellWidget(i, 5, actionBtn);
        listTable_->setRowHeight(i, 42);
    }
}

void ProjectsHubDialog::updateToolbarState() {
    size_t selCount = selectedProjectIds_.size();
    if (selCount > 0) {
        batchActionsContainer_->setVisible(true);
        selectionCountLabel_->setText(QString("%1 selected").arg(selCount));
    } else {
        batchActionsContainer_->setVisible(false);
    }

    if (displayedProjects_.empty()) {
        selectAllCheckbox_->setCheckState(Qt::Unchecked);
    } else if (selCount == displayedProjects_.size()) {
        selectAllCheckbox_->setCheckState(Qt::Checked);
    } else if (selCount > 0) {
        selectAllCheckbox_->setCheckState(Qt::PartiallyChecked);
    } else {
        selectAllCheckbox_->setCheckState(Qt::Unchecked);
    }
}

void ProjectsHubDialog::onSearchChanged(const QString& query) {
    searchQuery_ = query;
    refreshFilteredList();
}

void ProjectsHubDialog::onSortFieldChanged(int index) {
    sortField_ = static_cast<ProjectSortField>(sortCombo_->itemData(index).toInt());
    refreshFilteredList();
}

void ProjectsHubDialog::onToggleSortOrder() {
    sortAscending_ = !sortAscending_;
    sortOrderBtn_->setIcon(UiIcons::get(sortAscending_ ? UiIcon::ArrowUp : UiIcon::ArrowDown, QColor("#a1a1aa"), 16));
    refreshFilteredList();
}

void ProjectsHubDialog::setViewMode(ProjectViewMode mode) {
    viewMode_ = mode;
    if (mode == ProjectViewMode::Grid) {
        gridViewBtn_->setStyleSheet("background: #27272a; border: none; border-radius: 4px;");
        gridViewBtn_->setIcon(UiIcons::get(UiIcon::Grid, QColor("#38bdf8"), 16));
        listViewBtn_->setStyleSheet("background: transparent; border: none; border-radius: 4px;");
        listViewBtn_->setIcon(UiIcons::get(UiIcon::List, QColor("#71717a"), 16));
    } else {
        gridViewBtn_->setStyleSheet("background: transparent; border: none; border-radius: 4px;");
        gridViewBtn_->setIcon(UiIcons::get(UiIcon::Grid, QColor("#71717a"), 16));
        listViewBtn_->setStyleSheet("background: #27272a; border: none; border-radius: 4px;");
        listViewBtn_->setIcon(UiIcons::get(UiIcon::List, QColor("#38bdf8"), 16));
    }
    refreshFilteredList();
}

void ProjectsHubDialog::onSelectAllChanged(Qt::CheckState state) {
    if (state == Qt::Checked) {
        for (const auto& p : displayedProjects_) {
            selectedProjectIds_.insert(p.id);
        }
    } else if (state == Qt::Unchecked) {
        selectedProjectIds_.clear();
    }
    updateToolbarState();
    if (viewMode_ == ProjectViewMode::Grid) {
        renderGridView();
    }
}

void ProjectsHubDialog::onCreateNewProject() {
    bool ok = false;
    QString name = QInputDialog::getText(
        this,
        "New Project",
        "Project name:",
        QLineEdit::Normal,
        "Untitled Project " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"),
        &ok
    );
    if (!ok || name.trimmed().isEmpty()) return;

    editor::Project newProj(name.trimmed().toStdString());
    auto dir = storage::ProjectStorage::getDefaultProjectsDirectory();
    std::string filename = core::UuidGenerator::generateUUID() + ".json";
    auto fullPath = dir / filename;

    auto res = storage::ProjectStorage::saveAtomic(newProj, fullPath);
    if (res.ok()) {
        openProject(fullPath);
    } else {
        QMessageBox::critical(this, "Error", "Failed to create project: " + QString::fromStdString(res.message()));
    }
}

void ProjectsHubDialog::openProject(const std::filesystem::path& path) {
    targetProjectPath_ = path;
    emit projectSelected(QString::fromStdString(path.string()));
    accept();
}

void ProjectsHubDialog::renameProject(const std::filesystem::path& path) {
    auto res = storage::ProjectStorage::load(path);
    if (!res.ok()) {
        QMessageBox::warning(this, "Error", "Failed to load project for rename");
        return;
    }

    auto proj = res.value();
    bool ok = false;
    QString newName = QInputDialog::getText(
        this,
        "Rename Project",
        "New name:",
        QLineEdit::Normal,
        QString::fromStdString(proj.name()),
        &ok
    );

    if (ok && !newName.trimmed().isEmpty()) {
        proj.setName(newName.trimmed().toStdString());
        proj.metadata().updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
        storage::ProjectStorage::saveAtomic(proj, path);
        loadProjectsFromDisk();
    }
}

void ProjectsHubDialog::duplicateProject(const std::filesystem::path& path) {
    auto res = storage::ProjectStorage::load(path);
    if (!res.ok()) return;

    auto proj = res.value();
    proj.metadata().id = core::ProjectId(core::UuidGenerator::generateUUID());
    std::vector<std::string> existingNames;
    existingNames.reserve(allProjects_.size());
    for (const auto& item : allProjects_) {
        existingNames.push_back(item.name);
    }
    proj.setName(core::ProjectOrganizationEngine::generateDuplicateName(proj.name(), existingNames));
    proj.metadata().createdAt = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
    proj.metadata().updatedAt = proj.metadata().createdAt;

    auto dir = storage::ProjectStorage::getDefaultProjectsDirectory();
    std::string filename = core::UuidGenerator::generateUUID() + ".json";
    auto fullPath = dir / filename;

    storage::ProjectStorage::saveAtomic(proj, fullPath);
    loadProjectsFromDisk();
}

void ProjectsHubDialog::deleteProject(const std::filesystem::path& path) {
    auto ans = QMessageBox::question(
        this,
        "Delete Project",
        "Are you sure you want to delete this project?\nThis action cannot be undone.",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (ans == QMessageBox::Yes) {
        storage::ProjectStorage::deleteProject(path);
        loadProjectsFromDisk();
    }
}

void ProjectsHubDialog::onDeleteSelected() {
    if (selectedProjectIds_.empty()) return;

    auto ans = QMessageBox::question(
        this,
        "Delete Projects",
        QString("Are you sure you want to delete %1 selected projects?\nThis action cannot be undone.").arg(selectedProjectIds_.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (ans == QMessageBox::Yes) {
        for (const auto& proj : allProjects_) {
            if (selectedProjectIds_.find(proj.id) != selectedProjectIds_.end()) {
                storage::ProjectStorage::deleteProject(proj.filePath);
            }
        }
        selectedProjectIds_.clear();
        loadProjectsFromDisk();
    }
}

void ProjectsHubDialog::onDuplicateSelected() {
    if (selectedProjectIds_.empty()) return;

    for (const auto& projRecord : allProjects_) {
        if (selectedProjectIds_.find(projRecord.id) != selectedProjectIds_.end()) {
            duplicateProject(projRecord.filePath);
        }
    }
    selectedProjectIds_.clear();
    loadProjectsFromDisk();
}

void ProjectsHubDialog::showProjectInfo(const ProjectItemRecord& r) {
    QDialog dlg(this);
    dlg.setWindowTitle("Project Info - " + QString::fromStdString(r.name));
    dlg.resize(460, 360);
    dlg.setStyleSheet("background-color: #121215; color: #f4f4f5;");

    auto* layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto addRow = [&](const QString& label, const QString& val) {
        auto* row = new QHBoxLayout();
        auto* l = new QLabel(label, &dlg);
        l->setStyleSheet("color: #71717a; font-weight: 500; font-size: 13px;");
        auto* v = new QLabel(val, &dlg);
        v->setStyleSheet("color: #f4f4f5; font-weight: 600; font-size: 13px;");
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(l);
        row->addStretch();
        row->addWidget(v);
        layout->addLayout(row);
    };

    addRow("Project Name:", QString::fromStdString(r.name));
    addRow("Project ID:", QString::fromStdString(r.id));
    addRow("Resolution:", QString("%1 x %2").arg(r.width).arg(r.height));
    addRow("Frame Rate:", QString::number(r.fps, 'f', 0) + " fps");

    int durSec = static_cast<int>(r.durationSeconds);
    addRow("Duration:", QString("%1:%2").arg(durSec / 60, 2, 10, QChar('0')).arg(durSec % 60, 2, 10, QChar('0')));
    addRow("Tracks / Clips:", QString("%1 tracks / %2 clips").arg(r.trackCount).arg(r.clipCount));
    addRow("File Size:", QString::number(r.fileSizeBytes / 1024.0, 'f', 1) + " KB");
    addRow("File Location:", QString::fromStdString(r.filePath.string()));

    layout->addStretch();
    auto* closeBtn = new QPushButton("Close", &dlg);
    closeBtn->setFixedHeight(34);
    closeBtn->setStyleSheet("background: #27272a; color: #ffffff; border-radius: 6px;");
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(closeBtn);

    dlg.exec();
}

} // namespace catchim::ui
#endif

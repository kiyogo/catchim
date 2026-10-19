#pragma once

#if defined(HAVE_QT6)
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QScrollArea>
#include <QTableWidget>
#include <QStackedWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <string>

#include "storage/ProjectStorage.h"

namespace catchim::ui {

enum class ProjectViewMode {
    Grid,
    List
};

enum class ProjectSortField {
    Modified,
    Created,
    Name,
    Duration
};

struct ProjectItemRecord {
    std::filesystem::path filePath;
    std::string id;
    std::string name;
    std::string thumbnail;
    double durationSeconds{0.0};
    std::string createdAt;
    std::string updatedAt;
    int width{1920};
    int height{1080};
    double fps{30.0};
    size_t trackCount{0};
    size_t clipCount{0};
    uintmax_t fileSizeBytes{0};
};

class ProjectCardWidget;

class ProjectsHubDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProjectsHubDialog(QWidget* parent = nullptr);
    ~ProjectsHubDialog() override = default;

    std::filesystem::path selectedProjectPath() const { return targetProjectPath_; }

signals:
    void projectSelected(const QString& filePath);

private slots:
    void onSearchChanged(const QString& query);
    void onSortFieldChanged(int index);
    void onToggleSortOrder();
    void setViewMode(ProjectViewMode mode);
    void onSelectAllChanged(Qt::CheckState state);
    void onCreateNewProject();
    void onDeleteSelected();
    void onDuplicateSelected();

private:
    void setupUi();
    void loadProjectsFromDisk();
    void refreshFilteredList();
    void renderGridView();
    void renderListView();
    void updateToolbarState();

    void openProject(const std::filesystem::path& path);
    void renameProject(const std::filesystem::path& path);
    void duplicateProject(const std::filesystem::path& path);
    void deleteProject(const std::filesystem::path& path);
    void showProjectInfo(const ProjectItemRecord& record);

    // Data
    std::vector<ProjectItemRecord> allProjects_;
    std::vector<ProjectItemRecord> displayedProjects_;
    std::unordered_set<std::string> selectedProjectIds_;
    std::filesystem::path targetProjectPath_;

    ProjectViewMode viewMode_{ProjectViewMode::Grid};
    ProjectSortField sortField_{ProjectSortField::Modified};
    bool sortAscending_{false};
    QString searchQuery_;

    // UI Widgets
    QLineEdit* searchEdit_{nullptr};
    QPushButton* newProjectBtn_{nullptr};
    QPushButton* gridViewBtn_{nullptr};
    QPushButton* listViewBtn_{nullptr};

    QCheckBox* selectAllCheckbox_{nullptr};
    QComboBox* sortCombo_{nullptr};
    QPushButton* sortOrderBtn_{nullptr};
    QWidget* batchActionsContainer_{nullptr};
    QLabel* selectionCountLabel_{nullptr};
    QPushButton* batchDuplicateBtn_{nullptr};
    QPushButton* batchDeleteBtn_{nullptr};

    QStackedWidget* viewStack_{nullptr};
    QScrollArea* gridScrollArea_{nullptr};
    QWidget* gridContainer_{nullptr};
    QGridLayout* gridLayout_{nullptr};

    QTableWidget* listTable_{nullptr};
    QWidget* emptyStateWidget_{nullptr};

    friend class ProjectCardWidget;
};

} // namespace catchim::ui
#endif

#include "ProjectManager.h"
#include <algorithm>

namespace catchim::editor {

void ProjectManager::setActive(std::unique_ptr<Project> project) {
    activeProject_ = std::move(project);
    notify();
}

void ProjectManager::closeActive() noexcept {
    activeProject_.reset();
    notify();
}

Project& ProjectManager::createNewProject(std::string name) {
    auto proj = std::make_unique<Project>(std::move(name));
    activeProject_ = std::move(proj);
    notify();
    return *activeProject_;
}

core::TimelineTime ProjectManager::getProjectDurationFromScenes(const std::vector<Scene>& scenes) {
    if (scenes.empty()) {
        return core::TimelineTime(0);
    }

    // Look for main scene first
    for (const auto& s : scenes) {
        if (s.isMain()) {
            return s.timeline().totalDuration();
        }
    }

    return scenes.front().timeline().totalDuration();
}

void ProjectManager::sortProjects(
    std::vector<ProjectMetadata>& list,
    ProjectSortKey key,
    SortDirection direction
) {
    auto comparator = [key, direction](const ProjectMetadata& a, const ProjectMetadata& b) -> bool {
        if (direction == SortDirection::Ascending) {
            switch (key) {
                case ProjectSortKey::UpdatedAt:
                    return a.updatedAt < b.updatedAt;
                case ProjectSortKey::CreatedAt:
                    return a.createdAt < b.createdAt;
                case ProjectSortKey::Name:
                    return a.name < b.name;
                case ProjectSortKey::Duration:
                    return a.duration < b.duration;
            }
        } else {
            switch (key) {
                case ProjectSortKey::UpdatedAt:
                    return a.updatedAt > b.updatedAt;
                case ProjectSortKey::CreatedAt:
                    return a.createdAt > b.createdAt;
                case ProjectSortKey::Name:
                    return a.name > b.name;
                case ProjectSortKey::Duration:
                    return a.duration > b.duration;
            }
        }
        return false;
    };

    std::sort(list.begin(), list.end(), comparator);
}

void ProjectManager::setSavedProjects(std::vector<ProjectMetadata> projects) {
    savedProjects_ = std::move(projects);
    notify();
}

void ProjectManager::setMigrationState(MigrationState state) noexcept {
    migrationState_ = std::move(state);
    notify();
}

bool ProjectManager::isDirty() const noexcept {
    return activeProject_ ? activeProject_->isDirty() : false;
}

void ProjectManager::setDirty(bool dirty) noexcept {
    if (activeProject_) {
        activeProject_->setDirty(dirty);
        notify();
    }
}

void ProjectManager::subscribe(std::function<void()> listener) {
    listeners_.push_back(std::move(listener));
}

void ProjectManager::notify() {
    for (const auto& l : listeners_) {
        if (l) l();
    }
}

} // namespace catchim::editor

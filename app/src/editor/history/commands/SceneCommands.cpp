#include "SceneCommands.h"
#include <algorithm>

namespace catchim::editor {
namespace {
std::vector<Scene> historyScenes(const std::vector<Scene>& scenes) {
    std::vector<Scene> result;result.reserve(scenes.size());
    for(const auto& scene:scenes)result.push_back(scene.shareTimelineForHistory());
    return result;
}
void restoreHistoryScenes(Project& project,const std::vector<Scene>& scenes) {
    project.scenes()=historyScenes(scenes);
}
}

namespace SceneUtils {

bool canDeleteScene(const Scene& scene, size_t totalScenesCount) noexcept {
    if (scene.isMain()) {
        return false; // Main scene cannot be deleted
    }
    if (totalScenesCount <= 1) {
        return false; // Cannot delete last remaining scene
    }
    return true;
}

core::SceneId getFallbackSceneAfterDelete(
    const std::vector<Scene>& remainingScenes,
    const core::SceneId& deletedSceneId,
    const core::SceneId& currentSceneId
) {
    if (remainingScenes.empty()) {
        return core::SceneId::empty();
    }

    // If current scene was not the deleted one, preserve it if still present
    if (currentSceneId != deletedSceneId) {
        for (const auto& s : remainingScenes) {
            if (s.id() == currentSceneId) {
                return currentSceneId;
            }
        }
    }

    // Otherwise fallback to main scene
    for (const auto& s : remainingScenes) {
        if (s.isMain()) {
            return s.id();
        }
    }

    // Default to the first available scene
    return remainingScenes.front().id();
}

} // namespace SceneUtils

// ============================================================================
// CreateSceneCommand
// ============================================================================

CreateSceneCommand::CreateSceneCommand(
    Project& project,
    std::string name,
    bool isMain
)
    : project_(project)
    , sceneName_(std::move(name))
    , isMain_(isMain)
{
}

bool CreateSceneCommand::execute() {
    savedScenes_ = historyScenes(project_.scenes());
    savedActiveSceneId_ = project_.currentSceneId();

    if (createdSceneId_.isEmpty()) {
        createdSceneId_ = core::SceneId::generate();
    }
    if(!createdScene_)createdScene_.emplace(createdSceneId_,sceneName_,isMain_);
    project_.scenes().push_back(createdScene_->shareTimelineForHistory());
    project_.setCurrentSceneId(createdSceneId_);
    project_.setDirty(true);
    executed_ = true;
    return true;
}

bool CreateSceneCommand::undo() {
    if (!executed_) {
        return false;
    }

    restoreHistoryScenes(project_,savedScenes_);
    project_.setCurrentSceneId(savedActiveSceneId_);
    project_.setDirty(true);
    return true;
}

// ============================================================================
// DeleteSceneCommand
// ============================================================================

DeleteSceneCommand::DeleteSceneCommand(
    Project& project,
    core::SceneId sceneId
)
    : project_(project)
    , sceneId_(std::move(sceneId))
{
}

bool DeleteSceneCommand::execute() {
    auto* scene = project_.findScene(sceneId_);
    if (!scene) {
        return false;
    }

    if (!SceneUtils::canDeleteScene(*scene, project_.scenes().size())) {
        return false;
    }

    savedScenes_ = historyScenes(project_.scenes());
    savedActiveSceneId_ = project_.currentSceneId();

    std::vector<Scene> remaining;
    remaining.reserve(project_.scenes().size() - 1);
    for (const auto& s : project_.scenes()) {
        if (s.id() != sceneId_) {
            remaining.push_back(s);
        }
    }

    core::SceneId fallbackId = SceneUtils::getFallbackSceneAfterDelete(
        remaining, sceneId_, project_.currentSceneId()
    );

    auto& scs = project_.scenes();
    scs.erase(
        std::remove_if(scs.begin(), scs.end(), [&](const Scene& s) {
            return s.id() == sceneId_;
        }),
        scs.end()
    );

    project_.setCurrentSceneId(fallbackId);
    project_.setDirty(true);
    executed_ = true;
    return true;
}

bool DeleteSceneCommand::undo() {
    if (!executed_) {
        return false;
    }

    restoreHistoryScenes(project_,savedScenes_);
    project_.setCurrentSceneId(savedActiveSceneId_);
    project_.setDirty(true);
    return true;
}

// ============================================================================
// RenameSceneCommand
// ============================================================================

RenameSceneCommand::RenameSceneCommand(
    Project& project,
    core::SceneId sceneId,
    std::string newName
)
    : project_(project)
    , sceneId_(std::move(sceneId))
    , newName_(std::move(newName))
{
}

bool RenameSceneCommand::execute() {
    auto* scene = project_.findScene(sceneId_);
    if (!scene) {
        return false;
    }

    previousName_ = scene->name();
    scene->setName(newName_);
    project_.setDirty(true);
    executed_ = true;
    return true;
}

bool RenameSceneCommand::undo() {
    if (!executed_) {
        return false;
    }

    auto* scene = project_.findScene(sceneId_);
    if (!scene) {
        return false;
    }

    scene->setName(previousName_);
    project_.setDirty(true);
    return true;
}

// ============================================================================
// DuplicateSceneCommand
// ============================================================================

DuplicateSceneCommand::DuplicateSceneCommand(
    Project& project,
    core::SceneId sceneId,
    std::optional<std::string> customName
)
    : project_(project)
    , sceneId_(std::move(sceneId))
    , customName_(std::move(customName))
{
}

bool DuplicateSceneCommand::execute() {
    const auto* src = project_.findScene(sceneId_);
    if (!src) {
        return false;
    }

    savedScenes_ = historyScenes(project_.scenes());
    savedActiveSceneId_ = project_.currentSceneId();

    if (duplicatedSceneId_.isEmpty()) {
        duplicatedSceneId_ = core::SceneId::generate();
    }
    std::string name = customName_.value_or(src->name() + " (Copy)");

    if(!duplicatedScene_) {
        duplicatedScene_.emplace(duplicatedSceneId_,std::move(name),false);
        duplicatedScene_->timeline()=src->timeline();
    }
    project_.scenes().push_back(duplicatedScene_->shareTimelineForHistory());
    project_.setCurrentSceneId(duplicatedSceneId_);
    project_.setDirty(true);
    executed_ = true;
    return true;
}

bool DuplicateSceneCommand::undo() {
    if (!executed_) {
        return false;
    }

    restoreHistoryScenes(project_,savedScenes_);
    project_.setCurrentSceneId(savedActiveSceneId_);
    project_.setDirty(true);
    return true;
}

} // namespace catchim::editor

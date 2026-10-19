#pragma once

#include "editor/history/Command.h"
#include "editor/project/Project.h"
#include "core/ids/Ids.h"
#include <string>
#include <vector>
#include <optional>

namespace catchim::editor {

namespace SceneUtils {
    /**
     * @brief Determines whether a scene can be safely deleted.
     * Mirrors web/src/timeline/scenes.ts canDeleteScene.
     */
    bool canDeleteScene(const Scene& scene, size_t totalScenesCount) noexcept;

    /**
     * @brief Resolves the fallback scene to activate after a scene is deleted.
     * Mirrors web/src/timeline/scenes.ts getFallbackSceneAfterDelete.
     */
    core::SceneId getFallbackSceneAfterDelete(
        const std::vector<Scene>& remainingScenes,
        const core::SceneId& deletedSceneId,
        const core::SceneId& currentSceneId
    );
}

/**
 * @brief Command to create a new scene and activate it.
 * Corresponds to web/src/commands/scene/create-scene.ts.
 */
class CreateSceneCommand : public EditorCommand {
public:
    CreateSceneCommand(
        Project& project,
        std::string name,
        bool isMain = false
    );

    bool execute() override;
    bool undo() override;
    [[nodiscard]] std::string name() const override { return "Create Scene"; }

    [[nodiscard]] const core::SceneId& createdSceneId() const noexcept { return createdSceneId_; }

private:
    Project& project_;
    std::string sceneName_;
    bool isMain_{false};
    core::SceneId createdSceneId_;
    std::optional<Scene> createdScene_;
    std::vector<Scene> savedScenes_;
    core::SceneId savedActiveSceneId_;
    bool executed_{false};
};

/**
 * @brief Command to delete an existing scene with safety checks and fallback activation.
 * Corresponds to web/src/commands/scene/delete-scene.ts.
 */
class DeleteSceneCommand : public EditorCommand {
public:
    DeleteSceneCommand(
        Project& project,
        core::SceneId sceneId
    );

    bool execute() override;
    bool undo() override;
    [[nodiscard]] std::string name() const override { return "Delete Scene"; }

    [[nodiscard]] const core::SceneId& deletedSceneId() const noexcept { return sceneId_; }

private:
    Project& project_;
    core::SceneId sceneId_;
    std::vector<Scene> savedScenes_;
    core::SceneId savedActiveSceneId_;
    bool executed_{false};
};

/**
 * @brief Command to rename an existing scene.
 * Corresponds to web/src/commands/scene/rename-scene.ts.
 */
class RenameSceneCommand : public EditorCommand {
public:
    RenameSceneCommand(
        Project& project,
        core::SceneId sceneId,
        std::string newName
    );

    bool execute() override;
    bool undo() override;
    [[nodiscard]] std::string name() const override { return "Rename Scene"; }

private:
    Project& project_;
    core::SceneId sceneId_;
    std::string newName_;
    std::string previousName_;
    bool executed_{false};
};

/**
 * @brief Command to duplicate a scene (including all tracks, clips, bookmarks) and activate it.
 * Mirrors web/src/timeline/scenes.ts duplicateScene workflow.
 */
class DuplicateSceneCommand : public EditorCommand {
public:
    DuplicateSceneCommand(
        Project& project,
        core::SceneId sceneId,
        std::optional<std::string> customName = std::nullopt
    );

    bool execute() override;
    bool undo() override;
    [[nodiscard]] std::string name() const override { return "Duplicate Scene"; }

    [[nodiscard]] const core::SceneId& duplicatedSceneId() const noexcept { return duplicatedSceneId_; }

private:
    Project& project_;
    core::SceneId sceneId_;
    std::optional<std::string> customName_;
    core::SceneId duplicatedSceneId_;
    std::optional<Scene> duplicatedScene_;
    std::vector<Scene> savedScenes_;
    core::SceneId savedActiveSceneId_;
    bool executed_{false};
};

} // namespace catchim::editor

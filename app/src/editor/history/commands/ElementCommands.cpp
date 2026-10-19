#include "ElementCommands.h"

namespace catchim::editor {

// ============================================================================
// DeleteElementsCommand
// ============================================================================

DeleteElementsCommand::DeleteElementsCommand(
    Timeline& timeline,
    std::vector<ElementLocation> elements
)
    : timeline_(timeline)
    , elements_(std::move(elements))
{
}

bool DeleteElementsCommand::execute() {
    if (elements_.empty()) {
        return false;
    }

    savedState_ = timeline_.createSnapshot();

    for (const auto& loc : elements_) {
        timeline_.removeClip(loc.clipId);
    }

    return true;
}

bool DeleteElementsCommand::undo() {
    if (!savedState_.has_value()) {
        return false;
    }

    timeline_.restoreSnapshot(*savedState_);
    savedState_.reset();
    return true;
}

// ============================================================================
// UpdateElementsCommand
// ============================================================================

UpdateElementsCommand::UpdateElementsCommand(
    Timeline& timeline,
    std::vector<ElementPatch> updates
)
    : timeline_(timeline)
    , updates_(std::move(updates))
{
}

bool UpdateElementsCommand::execute() {
    if (updates_.empty()) {
        return false;
    }

    savedState_ = timeline_.createSnapshot();

    for (const auto& entry : updates_) {
        Clip* clip = timeline_.findClip(entry.clipId);
        if (!clip) {
            continue;
        }

        if (entry.muted) clip->setMuted(*entry.muted);
        if (entry.hidden) clip->setHidden(*entry.hidden);
        if (entry.name) clip->setName(*entry.name);

        if (entry.patchParams.is_object()) {
            for (const auto& [key, value] : entry.patchParams.items()) {
                clip->params()[key] = value;
            }
        }
    }

    return true;
}

bool UpdateElementsCommand::undo() {
    if (!savedState_.has_value()) {
        return false;
    }

    timeline_.restoreSnapshot(*savedState_);
    savedState_.reset();
    return true;
}

} // namespace catchim::editor

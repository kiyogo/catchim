#pragma once

#include "render/PreviewSnap.h"
#include "render/Transform.h"
#include "editor/history/commands/ElementCommands.h"
#include "editor/timeline/ElementUtils.h"
#include <vector>
#include <string>
#include <optional>
#include <memory>
#include <nlohmann/json.hpp>

namespace catchim::render {

struct VisualElementBounds {
    core::TrackId trackId;
    core::ClipId clipId;
    std::string clipType{"video"}; // "video", "image", "text", "shape", etc.
    double cx{0.0};
    double cy{0.0};
    double width{100.0};
    double height{100.0};
    double rotation{0.0}; // in degrees
    Transform initialTransform;
    nlohmann::json initialParams = nlohmann::json::object();

    bool operator==(const VisualElementBounds& other) const {
        return trackId == other.trackId && clipId == other.clipId;
    }
};

struct TextEditingState {
    core::TrackId trackId;
    core::ClipId clipId;
    std::string textContent;
};

enum class PreviewGestureKind {
    Idle,
    Pending,
    Dragging
};

class PreviewInteractionEngine {
public:
    static constexpr double MIN_DRAG_DISTANCE = 0.5;

    PreviewInteractionEngine() = default;

    // Geometric hit-testing functions matching web/src/preview/hit-test.ts
    static bool pointInRotatedRect(
        double px, double py,
        double cx, double cy,
        double width, double height,
        double rotationDegrees
    ) noexcept;

    static std::vector<VisualElementBounds> getHitElements(
        Point2D canvasPos,
        const std::vector<VisualElementBounds>& elementsWithBounds
    );

    static const VisualElementBounds* resolvePreferredHit(
        const std::vector<VisualElementBounds>& hits,
        const std::vector<editor::ElementLocation>& preferredElements
    );

    // Gesture lifecycle matching web/src/preview/controllers/preview-interaction-controller.ts
    void onPointerDown(
        Point2D canvasPos,
        const std::vector<VisualElementBounds>& visibleElements,
        const std::vector<editor::ElementLocation>& currentSelection
    );

    void onPointerMove(
        Point2D canvasPos,
        bool isShiftHeld,
        Size2D canvasSize,
        double snapThresholdPixels = PreviewSnap::DEFAULT_SNAP_THRESHOLD
    );

    void onPointerUp(bool isCancelled = false);

    bool onDoubleClick(
        Point2D canvasPos,
        const std::vector<VisualElementBounds>& visibleElements
    );

    void cancelGesture();

    // Query state
    [[nodiscard]] PreviewGestureKind gestureKind() const noexcept { return gestureKind_; }
    [[nodiscard]] bool isDragging() const noexcept { return gestureKind_ == PreviewGestureKind::Dragging; }
    [[nodiscard]] bool isPending() const noexcept { return gestureKind_ == PreviewGestureKind::Pending; }

    [[nodiscard]] const std::vector<editor::ElementLocation>& selection() const noexcept { return selection_; }
    [[nodiscard]] const std::vector<editor::ElementPatch>& activePatches() const noexcept { return activePatches_; }
    [[nodiscard]] const std::vector<SnapLine>& activeSnapLines() const noexcept { return activeSnapLines_; }
    [[nodiscard]] const std::optional<TextEditingState>& textEditingState() const noexcept { return textEditingState_; }
    void clearTextEditing() noexcept { textEditingState_ = std::nullopt; }

private:
    PreviewGestureKind gestureKind_{PreviewGestureKind::Idle};
    Point2D dragOrigin_{0.0, 0.0};
    std::optional<VisualElementBounds> topmostHit_{std::nullopt};
    std::optional<VisualElementBounds> selectedHit_{std::nullopt};

    Size2D dragBoundsSize_{0.0, 0.0};
    double dragRotation_{0.0};
    std::vector<VisualElementBounds> draggingElements_;

    std::vector<editor::ElementLocation> selection_;
    std::vector<editor::ElementPatch> activePatches_;
    std::vector<SnapLine> activeSnapLines_;
    std::optional<TextEditingState> textEditingState_{std::nullopt};
};

} // namespace catchim::render

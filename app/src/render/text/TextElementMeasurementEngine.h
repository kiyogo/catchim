#pragma once

#include "core/time/TimelineTime.h"
#include <string>
#include <unordered_map>
#include <optional>

namespace catchim::editor {
class Clip;
}

namespace catchim::render {

struct TextBackground {
    bool enabled{false};
    std::string color{"#000000"};
    double cornerRadius{0.0};
    double paddingX{30.0};
    double paddingY{42.0};
    double offsetX{0.0};
    double offsetY{0.0};

    bool operator==(const TextBackground& other) const = default;
};

struct ResolvedTextBackground {
    bool enabled{false};
    std::string color{"#000000"};
    double cornerRadius{0.0};
    double paddingX{30.0};
    double paddingY{42.0};
    double offsetX{0.0};
    double offsetY{0.0};

    bool operator==(const ResolvedTextBackground& other) const = default;
};

struct TextVisualRect {
    double left{0.0};
    double top{0.0};
    double width{0.0};
    double height{0.0};

    bool operator==(const TextVisualRect& other) const = default;
};

struct MeasuredTextElement {
    double textWidth{0.0};
    double textHeight{0.0};
    ResolvedTextBackground resolvedBackground;
    TextVisualRect visualRect;

    bool operator==(const MeasuredTextElement& other) const = default;
};

class TextElementMeasurementEngine {
public:
    static constexpr double CORNER_RADIUS_MIN = 0.0;
    static constexpr double CORNER_RADIUS_MAX = 100.0;
    static constexpr double DEFAULT_PADDING_X = 30.0;
    static constexpr double DEFAULT_PADDING_Y = 42.0;
    static constexpr const char* DEFAULT_BACKGROUND_COLOR = "#000000";

    static TextBackground buildTextBackgroundFromParams(
        const std::unordered_map<std::string, std::string>& params
    );

    static ResolvedTextBackground resolveBackgroundAtTime(
        const TextBackground& base,
        const editor::Clip* clip,
        core::TimelineTime localTime
    );

    static TextVisualRect calculateVisualRect(
        const TextVisualRect& textBlockRect,
        const ResolvedTextBackground& bg
    );

    static MeasuredTextElement measureElement(
        const std::unordered_map<std::string, std::string>& params,
        const editor::Clip* clip,
        core::TimelineTime localTime,
        double textWidth,
        double textHeight
    );
};

} // namespace catchim::render

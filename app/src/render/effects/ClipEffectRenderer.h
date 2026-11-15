#pragma once

#include "core/time/TimelineTime.h"
#include "editor/timeline/Clip.h"
#include <cstdint>
#include <vector>

namespace catchim::render {

class ClipEffectRenderer {
public:
    static void apply(
        std::vector<uint8_t>& rgbaPixels,
        int width,
        int height,
        const editor::Clip& clip,
        core::TimelineTime timelineTime
    );
};

} // namespace catchim::render

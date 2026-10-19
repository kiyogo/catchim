#pragma once

#include "Compositor.h"
#include "editor/project/Project.h"
#include "media/MediaLibrary.h"
#include "core/time/TimelineTime.h"
#include <memory>

namespace catchim::media { class NativeVideoDecoder; }
namespace catchim::render {

class RenderEngine {
public:
    explicit RenderEngine(int32_t width = 1920, int32_t height = 1080);

    void setCanvasSize(int32_t width, int32_t height);
    // A preview/export worker owns its decoder; thumbnail seeking must not
    // serialize playback behind an unrelated shared decoder mutex.
    void setVideoDecoder(media::NativeVideoDecoder* decoder) { decoder_=decoder; }

    const CompositorOutput& renderFrame(
        const editor::Project& project,
        const media::MediaLibrary& mediaLibrary,
        core::TimelineTime time
    );

    const CompositorOutput& lastRenderedFrame() const noexcept {
        return compositor_.getOutput();
    }

private:
    Compositor compositor_;
    media::NativeVideoDecoder* decoder_{nullptr};
};

} // namespace catchim::render

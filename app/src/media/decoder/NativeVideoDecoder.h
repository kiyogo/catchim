#pragma once

#include "core/ids/Ids.h"
#include "core/time/TimelineTime.h"
#include <filesystem>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

namespace catchim::media {

class NativeVideoDecoder {
public:
    static NativeVideoDecoder& instance();
    static NativeVideoDecoder& thumbnailInstance();

    struct Statistics {
        std::string adapter;
        uint64_t gpuSurfaceFrames{0},gpuProcessedFrames{0},cpuFallbackFrames{0},readbackBytes{0};
        uint32_t lastGpuError{0};
    };
    // Preview-only limit; full-resolution export/validation uses the default.
    void setPreviewSize(int width,int height);
    void setPlaybackMode(bool playing) { playbackMode_ = playing; }
    bool playbackMode() const { return playbackMode_; }
    Statistics statistics() const;

    NativeVideoDecoder();
    ~NativeVideoDecoder();

    // Decode or retrieve frame from cache
    bool getFrame(
        const std::filesystem::path& path,
        double timestampSec,
        int& outWidth,
        int& outHeight,
        std::vector<uint8_t>& outRgba,
        const std::string& clipId = "",
        int* originalWidth = nullptr,
        int* originalHeight = nullptr
    );

    void clearCache();

private:
    struct SourceReaderContext;
    struct D3D11Context;
    std::unique_ptr<D3D11Context> d3d11Ctx_;
    void initD3D11();
    void cleanupD3D11();
    bool convertGpuFrame(SourceReaderContext& context,void* buffer);

    std::unordered_map<std::string, std::shared_ptr<SourceReaderContext>> readers_;
    mutable std::mutex mutex_;
    int previewWidth_{0},previewHeight_{0};
    bool playbackMode_{false};
    Statistics statistics_;
};

} // namespace catchim::media

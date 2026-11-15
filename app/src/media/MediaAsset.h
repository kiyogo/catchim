#pragma once

#include "core/ids/Ids.h"
#include "core/time/TimelineTime.h"
#include <string>
#include <filesystem>

namespace catchim::media {

enum class MediaType {
    Video,
    Audio,
    Image
};

class MediaAsset {
public:
    MediaAsset(
        core::MediaId id,
        std::filesystem::path filePath,
        MediaType type
    );

    const core::MediaId& id() const noexcept { return id_; }
    const std::filesystem::path& filePath() const noexcept { return filePath_; }
    void setFilePath(std::filesystem::path path) { filePath_ = std::move(path); }
    MediaType type() const noexcept { return type_; }

    const std::string& displayName() const noexcept { return displayName_; }
    void setDisplayName(std::string name) { displayName_ = std::move(name); }
    std::string fileName() const { return displayName_.empty() ? filePath_.filename().string() : displayName_; }
    uint64_t fileSize() const noexcept { return fileSize_; }
    void setFileSize(uint64_t size) noexcept { fileSize_ = size; }

    core::TimelineTime duration() const noexcept { return duration_; }
    void setDuration(core::TimelineTime dur) noexcept { duration_ = dur; }

    int32_t width() const noexcept { return width_; }
    int32_t height() const noexcept { return height_; }
    void setDimensions(int32_t w, int32_t h) noexcept { width_ = w; height_ = h; }

    core::FrameRate fps() const noexcept { return fps_; }
    void setFps(core::FrameRate rate) noexcept { fps_ = rate; }

    bool hasAudio() const noexcept { return hasAudio_; }
    void setHasAudio(bool has) noexcept { hasAudio_ = has; }

    bool hasVideo() const noexcept { return hasVideo_; }
    void setHasVideo(bool has) noexcept { hasVideo_ = has; }

    int32_t sampleRate() const noexcept { return sampleRate_; }
    void setSampleRate(int32_t sr) noexcept { sampleRate_ = sr; }

    int32_t channels() const noexcept { return channels_; }
    void setChannels(int32_t ch) noexcept { channels_ = ch; }

    const std::string& codec() const noexcept { return codec_; }
    void setCodec(std::string c) { codec_ = std::move(c); }

private:
    core::MediaId id_;
    std::filesystem::path filePath_;
    MediaType type_;
    std::string displayName_;
    uint64_t fileSize_{0};
    core::TimelineTime duration_{core::TimelineTime(0)};
    int32_t width_{0};
    int32_t height_{0};
    core::FrameRate fps_{30, 1};
    bool hasAudio_{false};
    bool hasVideo_{false};
    int32_t sampleRate_{44100};
    int32_t channels_{2};
    std::string codec_;
};

} // namespace catchim::media

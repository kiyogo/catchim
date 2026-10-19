#include "AudioSeparationEngine.h"

namespace catchim::editor {

bool AudioSeparationEngine::isSourceAudioEnabled(const Clip& clip) noexcept {
    if (clip.type() != ClipType::Video) {
        return false;
    }
    return clip.getParam<bool>("isSourceAudioEnabled", true);
}

bool AudioSeparationEngine::isSourceAudioSeparated(const Clip& clip) noexcept {
    if (clip.type() != ClipType::Video) {
        return false;
    }
    return !isSourceAudioEnabled(clip);
}

bool AudioSeparationEngine::canExtractSourceAudio(
    const Clip& clip,
    const media::MediaAsset* mediaAsset
) noexcept {
    if (clip.type() != ClipType::Video) {
        return false;
    }
    if (!isSourceAudioEnabled(clip)) {
        return false;
    }
    if (!mediaAsset) {
        // If mediaAsset is not provided, check clip's duration > 0
        return clip.getParam<bool>("hasAudio",true)&&clip.duration() > core::TimelineTime(0);
    }
    return mediaAsset->hasAudio() && clip.duration() > core::TimelineTime(0);
}

bool AudioSeparationEngine::canRecoverSourceAudio(const Clip& clip) noexcept {
    return clip.type() == ClipType::Video && isSourceAudioSeparated(clip);
}

bool AudioSeparationEngine::canToggleSourceAudio(
    const Clip& clip,
    const media::MediaAsset* mediaAsset
) noexcept {
    return canRecoverSourceAudio(clip) || canExtractSourceAudio(clip, mediaAsset);
}

bool AudioSeparationEngine::doesElementHaveEnabledAudio(
    const Clip& clip,
    const media::MediaAsset* mediaAsset
) noexcept {
    if (clip.type() == ClipType::Audio) {
        return true;
    }
    if (clip.type() != ClipType::Video) {
        return false;
    }
    if (!isSourceAudioEnabled(clip)) {
        return false;
    }
    if (mediaAsset) {
        return mediaAsset->hasAudio();
    }
    return true;
}

std::string_view AudioSeparationEngine::getSourceAudioActionLabel(const Clip& clip) noexcept {
    return isSourceAudioSeparated(clip) ? "Recover audio" : "Extract audio";
}

Clip AudioSeparationEngine::buildSeparatedAudioClip(const Clip& sourceClip) {
    core::ClipId audioId = core::ClipId::generate();
    std::string audioName = sourceClip.name() + " (Audio)";

    Clip audioClip(
        audioId,
        ClipType::Audio,
        std::move(audioName),
        sourceClip.startTime(),
        sourceClip.duration(),
        sourceClip.trimStart(),
        sourceClip.trimEnd()
    );

    audioClip.setMediaId(sourceClip.mediaId());
    audioClip.setSourceDuration(sourceClip.sourceDuration());

    double vol = sourceClip.getParam<double>("volume", 100.0);
    bool muted = sourceClip.getParam<bool>("muted", false);
    audioClip.setParam<double>("volume", vol);
    audioClip.setParam<bool>("muted", muted);
    audioClip.setMuted(sourceClip.isMuted()||muted);
    for(const char* key:{"audio.volumeUnit","speed","reversed","audio.fadeIn","audio.fadeOut"})
        if(sourceClip.hasParam(key))audioClip.params()[key]=sourceClip.params()[key];

    if (sourceClip.params().contains("retimeRate")) {
        audioClip.setParam<double>("retimeRate", sourceClip.getParam<double>("retimeRate", 1.0));
    }
    if (sourceClip.params().contains("maintainPitch")) {
        audioClip.setParam<bool>("maintainPitch", sourceClip.getParam<bool>("maintainPitch", true));
    }

    // Clone volume animation channel if present
    const auto* volChan = sourceClip.findAnimationChannel("volume");
    if (volChan) {
        audioClip.animationChannels()["volume"] = *volChan;
    }

    return audioClip;
}

} // namespace catchim::editor

#include "PlaybackController.h"
#include <algorithm>

namespace catchim::editor {

PlaybackController::PlaybackController() {}

void PlaybackController::play() {
    if (clock_.isPlaying()) return;

    // If at end, wrap to start
    if (currentTime() >= duration_ && duration_.ticks() > 0) {
        seek(core::TimelineTime(0));
    }

    clock_.start();
    if (onPlayStateChanged_) onPlayStateChanged_(true);
}

void PlaybackController::pause() {
    if (!clock_.isPlaying()) return;
    clock_.pause();
    if (onPlayStateChanged_) onPlayStateChanged_(false);
}

void PlaybackController::toggle() {
    if (isPlaying()) {
        pause();
    } else {
        play();
    }
}

void PlaybackController::stop() {
    pause();
    seek(core::TimelineTime(0));
}

void PlaybackController::seek(core::TimelineTime time) {
    core::TimelineTime clamped = time.clamp(core::TimelineTime(0), std::max(duration_, core::TimelineTime(0)));
    clock_.seek(clamped);
    if(onSeek_)onSeek_(clamped);
    if (onTimeChanged_) {
        onTimeChanged_(clamped);
    }
}

void PlaybackController::stepFrame(int frames, const core::FrameRate& rate) {
    int64_t tpf = (rate.numerator > 0) ? (core::TICKS_PER_SECOND * rate.denominator / rate.numerator) : 4000;
    core::TimelineTime newTime = currentTime() + core::TimelineTime(frames * tpf);
    seek(newTime.roundToFrame(rate));
}

void PlaybackController::jumpSeconds(double seconds) {
    core::TimelineTime delta = core::TimelineTime::fromSeconds(seconds);
    seek(currentTime() + delta);
}

core::TimelineTime PlaybackController::currentTime() const {
    core::TimelineTime cur = clock_.currentTime();
    if (duration_.ticks() > 0 && cur > duration_) {
        return duration_;
    }
    return cur;
}

void PlaybackController::update() {
    if (isPlaying()) {
        core::TimelineTime cur = clock_.currentTime();
        if (duration_.ticks() > 0 && cur >= duration_) {
            pause();
            seek(duration_);
        } else {
            if (onTimeChanged_) {
                onTimeChanged_(cur);
            }
        }
    }
}

} // namespace catchim::editor

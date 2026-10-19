#include "PlaybackAudioSession.h"
#include "AudioPlaybackEngine.h"
#include <cmath>
#ifdef _WIN32
#include <objbase.h>
#endif
namespace catchim::audio {
PlaybackAudioSession::PlaybackAudioSession(){thread_=std::thread([this]{run();});}
PlaybackAudioSession::~PlaybackAudioSession() {
    {std::lock_guard guard(mutex_);stopping_=true;}
    condition_.notify_all();thread_.join();
}
void PlaybackAudioSession::setState(std::shared_ptr<const media::PlaybackSnapshot> snapshot,core::TimelineTime time,bool playing) {
    {std::lock_guard guard(mutex_);snapshot_=std::move(snapshot);requestedTime_=time;playing_=playing;++serial_;
        preparing_=playing;active_=false;position_=time.toSeconds();}
    condition_.notify_all();
}
core::TimelineTime PlaybackAudioSession::requestedPosition() const {std::lock_guard guard(mutex_);return requestedTime_;}
std::optional<core::TimelineTime> PlaybackAudioSession::audiblePosition() const {
    std::lock_guard guard(mutex_);
    if(!active_||publishedSerial_!=serial_||!playing_)return std::nullopt;
    return core::TimelineTime::fromSeconds(position_);
}
PlaybackAudioSession::Statistics PlaybackAudioSession::statistics() const {
    return {available_,preparing_,active_,backend_,submitted_,partialWrites_,underruns_,resets_,position_,queued_};
}
void PlaybackAudioSession::run() {
#ifdef _WIN32
    struct ComScope {HRESULT value=CoInitializeEx(nullptr,COINIT_MULTITHREADED);~ComScope(){if(SUCCEEDED(value))CoUninitialize();}} com;
#endif
    WindowsAudioPlayer player;AudioPlaybackEngine mixer;
    uint64_t observed=0,sessionSubmitted=0;int64_t nextFrame=0,endFrame=0,startFrame=0;
    std::shared_ptr<const media::PlaybackSnapshot> snapshot;
    std::vector<float> pending;size_t pendingOffset=0;
    bool running=false,initialized=false,wasQueued=false;
    for(;;) {
        {
            std::unique_lock lock(mutex_);
            if(stopping_)break;
            if(observed!=serial_) {
                observed=serial_;snapshot=snapshot_;running=playing_&&snapshot;
                nextFrame=startFrame=std::llround(requestedTime_.toSeconds()*48000);
                endFrame=snapshot?std::llround(snapshot->project.totalDuration().toSeconds()*48000):0;
                pending.clear();pendingOffset=0;sessionSubmitted=0;wasQueued=false;
                player.reset();++resets_;queued_=0;active_=false;
                if(endFrame<=startFrame)running=false;
                if(!running)preparing_=false;
            }
            if(!running){condition_.wait(lock,[&]{return stopping_||serial_!=observed;});continue;}
        }
        if(!initialized) {
            initialized=player.init(48000,2);available_=initialized;backend_=player.backend();
            if(!initialized){preparing_=false;running=false;continue;}
        }
        {std::lock_guard guard(mutex_);if(stopping_||serial_!=observed)continue;}
        player.setVolume(volume_);
        const auto padding=player.queuedFrames();
        queued_=static_cast<double>(padding)/48000;
        if(sessionSubmitted) {
            const auto consumed=sessionSubmitted-std::min<uint64_t>(sessionSubmitted,padding);
            position_=static_cast<double>(startFrame+static_cast<int64_t>(consumed))/48000;
            if(wasQueued&&padding==0&&nextFrame<endFrame)++underruns_;
        }
        wasQueued=padding>0;
#if defined(__EMSCRIPTEN__)
        constexpr size_t targetPadding = 9600; // tolerate browser/Qt paint bursts
#else
        constexpr size_t targetPadding = 2880;
#endif
        if(padding<targetPadding&&nextFrame<endFrame) {
            if(pending.empty()) {
                const int64_t frames=std::min<int64_t>(960,endFrame-nextFrame);
                auto buffer=mixer.renderAudioSlice(*snapshot->project.activeTimeline(),
                    core::TimelineTime::fromSeconds(static_cast<double>(nextFrame)/48000),
                    core::TimelineTime::fromSeconds(static_cast<double>(frames)/48000),48000,&snapshot->library);
                pending=std::move(buffer.samples());pendingOffset=0;
            }
            // A seek/pause/edit may arrive while a cold source is being decoded.
            {std::lock_guard guard(mutex_);if(stopping_||serial_!=observed)continue;}
            const size_t requested=(pending.size()-pendingOffset)/2;
            // Keep device I/O outside the state mutex: the UI timer also reads
            // it in audiblePosition()/requestedPosition(). Browser packets own
            // their data until the asynchronous main-thread copy completes.
            const size_t accepted=player.writeSamples(pending.data()+pendingOffset,pending.size()-pendingOffset);
            std::lock_guard guard(mutex_);if(stopping_||serial_!=observed)continue;
            if(accepted<requested)++partialWrites_;
            pendingOffset+=accepted*2;sessionSubmitted+=accepted;submitted_+=accepted;
            if(accepted){publishedSerial_=observed;active_=true;preparing_=false;}
            if(pendingOffset==pending.size()) {nextFrame+=static_cast<int64_t>(pending.size()/2);pending.clear();}
            // The unaccepted tail remains pending and is retried, never skipped.
        }
        if(nextFrame>=endFrame&&player.queuedFrames()==0&&pending.empty())position_=static_cast<double>(endFrame)/48000;
        std::unique_lock lock(mutex_);
        condition_.wait_for(lock,std::chrono::milliseconds(2),[&]{return stopping_||serial_!=observed;});
    }
    player.shutdown();
}
}

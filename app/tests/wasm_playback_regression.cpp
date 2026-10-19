#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>
#include <cmath>
#include "ui/MainWindow.h"
#include "ui/theme/Theme.h"
#include "media/probe/MediaProbe.h"

using namespace catchim;

// Browser-only integration test. Uses the real UI timer, preview worker and
// WebAudio bridge; a main-thread/worker circular wait stops the heartbeat.
int main(int argc, char** argv) {
    auto* app = new QApplication(argc, argv);
    app->setStyle("Fusion");
    ui::Theme::instance().setTheme(ui::ThemeMode::Light);
    auto source = media::MediaProbe::probe("/playback-test.mp4");
    if (!source.ok()) { qCritical("WASM_PLAYBACK FAIL probe"); return 1; }
    auto asset = source.unwrap();
    auto* engine = new editor::EditorEngine;
    auto* library = new media::MediaLibrary;
    auto* renderer = new render::RenderEngine;
    auto* window = new ui::MainWindow(*engine, *library, *renderer);
    window->setPlaybackOutputVolume(0);
    window->resize(1600, 900);
    window->show();
    library->addAsset(asset);
    editor::Clip clip(core::ClipId::generate(), editor::ClipType::Video,
        asset->fileName(), core::TimelineTime(0), asset->duration());
    clip.setMediaId(asset->id());
    clip.setSourceDuration(asset->duration());
    clip.setParam("hasAudio", asset->hasAudio());
    engine->addClip(engine->activeTimeline()->mainTrack().id(), std::move(clip));
    engine->notifyProjectChanged();
    auto* preview = window->findChild<ui::PreviewWidget*>();
    auto* clock = new QElapsedTimer;
    auto* pulse = new QTimer(window);
    pulse->setInterval(20);
    clock->start();
    QObject::connect(pulse, &QTimer::timeout, window,
        [=, phase = 0, beats = 0, firstFrames = uint64_t(0), firstDecoded = uint64_t(0), firstTime = 0., pausedTime = 0.]() mutable {
        ++beats;
        const auto ms = clock->elapsed();
        if (phase == 0 && engine->playback().isPlaying()) {
            phase = 1;
            qInfo("WASM_PLAYBACK started by the Play button");
        } else if (phase == 1 && window->playbackAudioStatistics().active && preview->gpuStatistics().cpuFallbackFrames > 0) {
            // Measure steady playback separately from cold HEVC/audio startup.
            phase = 6; clock->restart();
        } else if (phase == 6 && ms >= 2000) {
            phase = 2; clock->restart(); beats = 0;
            firstFrames = preview->presentedFrames();
            firstDecoded = preview->gpuStatistics().cpuFallbackFrames;
            firstTime = engine->playback().currentTime().toSeconds();
        } else if (phase == 2 && ms >= 6000) {
            const auto advance = engine->playback().currentTime().toSeconds() - firstTime;
            const auto frames = preview->presentedFrames() - firstFrames;
            const auto decoded = preview->gpuStatistics().cpuFallbackFrames - firstDecoded;
            const bool ok = advance > 4.5 && frames > 90 && decoded > 90 && beats > 100;
            qInfo() << "WASM_PLAYBACK" << (ok ? "PASS" : "FAIL")
                << "seconds" << advance << "frames" << frames << "decoded" << decoded << "heartbeats" << beats;
            qInfo() << "WASM_RENDER" << preview->lastRenderMilliseconds()
                << "size" << preview->presentedImage().width() << preview->presentedImage().height();
            engine->pause(); pausedTime = engine->playback().currentTime().toSeconds();
            phase = 3; clock->restart();
        } else if (phase == 3 && ms >= 500) {
            qInfo() << "WASM_PAUSE" << (std::abs(engine->playback().currentTime().toSeconds() - pausedTime) < .05 ? "PASS" : "FAIL");
            engine->seek(core::TimelineTime::fromSeconds(18));
            phase = 4; clock->restart();
        } else if (phase == 4 && ms >= 1500) {
            qInfo() << "WASM_SEEK" << (std::abs(preview->presentedSeconds() - 18) < .1 ? "PASS" : "FAIL");
            engine->play(); phase = 5; clock->restart();
            firstFrames = preview->presentedFrames();
        } else if (phase == 5 && ms >= 4000) {
            qInfo() << "WASM_RESUME" << (engine->playback().currentTime().toSeconds() > 20 && preview->presentedFrames() > firstFrames + 40 ? "PASS" : "FAIL")
                << "seconds" << engine->playback().currentTime().toSeconds() << "frames" << preview->presentedFrames() - firstFrames;
            engine->pause(); pulse->stop();
        }
    });
    pulse->start();
    return app->exec();
}

#pragma once
#include "editor/project/Project.h"
#include "media/MediaLibrary.h"
namespace catchim::media {
// Immutable, independent model for background playback. Never traverse the
// live editor or its mutable asset vector from a render/audio thread.
struct PlaybackSnapshot {
    editor::Project project;
    MediaLibrary library;
    PlaybackSnapshot(const editor::Project& source,const MediaLibrary& assets):project(source) {
        for(const auto& asset:assets.assets())library.addAsset(std::make_shared<MediaAsset>(*asset));
    }
};
}

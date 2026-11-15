#include "ProjectSerializer.h"

namespace catchim::editor {

namespace {

nlohmann::json clipToJson(const Clip& clip) {
    nlohmann::json j;
    j["id"] = clip.id().str();
    j["type"] = clipTypeToString(clip.type());
    j["name"] = clip.name();
    j["startTime"] = clip.startTime().ticks();
    j["duration"] = clip.duration().ticks();
    j["trimStart"] = clip.trimStart().ticks();
    j["trimEnd"] = clip.trimEnd().ticks();
    if (clip.sourceDuration().has_value()) {
        j["sourceDuration"] = clip.sourceDuration()->ticks();
    }
    if (!clip.mediaId().isEmpty()) {
        j["mediaId"] = clip.mediaId().str();
    }
    j["hidden"] = clip.isHidden();
    j["muted"] = clip.isMuted();
    j["params"] = clip.params();

    nlohmann::json masksJson = nlohmann::json::array();
    for (const auto& m : clip.masks()) {
        masksJson.push_back({
            {"id", m.id},
            {"type", m.type},
            {"params", m.params}
        });
    }
    j["masks"] = masksJson;

    nlohmann::json effectsJson = nlohmann::json::array();
    for (const auto& eff : clip.effects()) {
        effectsJson.push_back({
            {"id", eff.id},
            {"type", eff.type},
            {"params", eff.params},
            {"enabled", eff.enabled}
        });
    }
    j["effects"] = effectsJson;

    return j;
}

Clip clipFromJson(const nlohmann::json& j) {
    core::ClipId id(j.value("id", ""));
    ClipType type = stringToClipType(j.value("type", "video"));
    std::string name = j.value("name", "");
    core::TimelineTime startTime(j.value("startTime", int64_t(0)));
    core::TimelineTime duration(j.value("duration", int64_t(0)));
    core::TimelineTime trimStart(j.value("trimStart", int64_t(0)));
    core::TimelineTime trimEnd(j.value("trimEnd", int64_t(0)));

    Clip clip(std::move(id), type, std::move(name), startTime, duration, trimStart, trimEnd);
    if (j.contains("sourceDuration") && !j["sourceDuration"].is_null()) {
        clip.setSourceDuration(core::TimelineTime(j["sourceDuration"].get<int64_t>()));
    }
    if (j.contains("mediaId") && !j["mediaId"].is_null()) {
        clip.setMediaId(core::MediaId(j["mediaId"].get<std::string>()));
    }
    clip.setHidden(j.value("hidden", false));
    clip.setMuted(j.value("muted", false));
    if (j.contains("params") && j["params"].is_object()) {
        clip.setParams(j["params"]);
    }
    if (j.contains("masks") && j["masks"].is_array()) {
        std::vector<MaskInstance> masks;
        for (const auto& mj : j["masks"]) {
            MaskInstance m;
            m.id = mj.value("id", "");
            m.type = mj.value("type", "freeform");
            if (mj.contains("params") && mj["params"].is_object()) {
                m.params = mj["params"];
            }
            masks.push_back(std::move(m));
        }
        clip.setMasks(std::move(masks));
    }
    if (j.contains("effects") && j["effects"].is_array()) {
        std::vector<EffectInstance> effects;
        for (const auto& ej : j["effects"]) {
            EffectInstance eff;
            eff.id = ej.value("id", "");
            eff.type = ej.value("type", "blur");
            if (ej.contains("params") && ej["params"].is_object()) {
                eff.params = ej["params"];
            }
            eff.enabled = ej.value("enabled", true);
            effects.push_back(std::move(eff));
        }
        clip.setEffects(std::move(effects));
    }
    return clip;
}

nlohmann::json trackToJson(const Track& track) {
    nlohmann::json j;
    j["id"] = track.id().str();
    j["name"] = track.name();
    j["type"] = trackTypeToString(track.type());
    j["muted"] = track.isMuted();
    j["hidden"] = track.isHidden();
    nlohmann::json elements = nlohmann::json::array();
    for (const auto& clip : track.clips()) {
        elements.push_back(clipToJson(clip));
    }
    j["elements"] = elements;
    return j;
}

Track trackFromJson(const nlohmann::json& j) {
    core::TrackId id(j.value("id", ""));
    TrackType type = stringToTrackType(j.value("type", "video"));
    std::string name = j.value("name", "");
    Track track(std::move(id), type, std::move(name));
    track.setMuted(j.value("muted", false));
    track.setHidden(j.value("hidden", false));
    if (j.contains("elements") && j["elements"].is_array()) {
        for (const auto& el : j["elements"]) {
            track.insertClip(clipFromJson(el));
        }
    }
    return track;
}

nlohmann::json bookmarkToJson(const Bookmark& bm) {
    nlohmann::json j;
    j["id"] = bm.id.str();
    j["time"] = bm.time.ticks();
    j["note"] = bm.note;
    j["color"] = bm.color;
    if (bm.duration.has_value()) {
        j["duration"] = bm.duration->ticks();
    }
    return j;
}

Bookmark bookmarkFromJson(const nlohmann::json& j) {
    Bookmark bm;
    bm.id = core::BookmarkId(j.value("id", ""));
    bm.time = core::TimelineTime(j.value("time", int64_t(0)));
    bm.note = j.value("note", "");
    bm.color = j.value("color", "#009dff");
    if (j.contains("duration") && !j["duration"].is_null()) {
        bm.duration = core::TimelineTime(j["duration"].get<int64_t>());
    }
    return bm;
}

} // namespace

nlohmann::json ProjectSerializer::toJson(const Project& project) {
    nlohmann::json j;
    j["version"] = project.version();

    // Metadata
    const auto& meta = project.metadata();
    j["metadata"] = {
        {"id", meta.id.str()},
        {"name", meta.name},
        {"thumbnail", meta.thumbnail},
        {"duration", project.totalDuration().ticks()},
        {"createdAt", meta.createdAt},
        {"updatedAt", meta.updatedAt}
    };

    // Settings
    const auto& s = project.settings();
    nlohmann::json bg;
    bg["type"] = (s.background.type == ProjectBackground::Type::Blur) ? "blur" : "color";
    bg["color"] = s.background.color;
    bg["blurIntensity"] = s.background.blurIntensity;

    j["settings"] = {
        {"fps", {{"numerator", s.fps.numerator}, {"denominator", s.fps.denominator}}},
        {"canvasSize", {{"width", s.canvasSize.width}, {"height", s.canvasSize.height}}},
        {"canvasSizeMode", s.canvasSizeMode},
        {"background", bg}
    };

    j["currentSceneId"] = project.currentSceneId().str();

    nlohmann::json mediaAssets = nlohmann::json::array();
    for (const auto& asset : project.mediaAssets()) {
        mediaAssets.push_back({
            {"id", asset.id},
            {"path", asset.path},
            {"displayName", asset.displayName},
            {"type", asset.type},
            {"fileSize", asset.fileSize},
            {"durationTicks", asset.durationTicks},
            {"width", asset.width},
            {"height", asset.height},
            {"fpsNumerator", asset.fpsNumerator},
            {"fpsDenominator", asset.fpsDenominator},
            {"hasAudio", asset.hasAudio},
            {"hasVideo", asset.hasVideo},
            {"sampleRate", asset.sampleRate},
            {"channels", asset.channels},
            {"codec", asset.codec}
        });
    }
    j["mediaAssets"] = std::move(mediaAssets);

    // Scenes
    nlohmann::json scenesArr = nlohmann::json::array();
    for (const auto& scene : project.scenes()) {
        nlohmann::json sceneObj;
        sceneObj["id"] = scene.id().str();
        sceneObj["name"] = scene.name();
        sceneObj["isMain"] = scene.isMain();

        // Bookmarks
        nlohmann::json bmArr = nlohmann::json::array();
        for (const auto& bm : scene.timeline().bookmarks()) {
            bmArr.push_back(bookmarkToJson(bm));
        }
        sceneObj["bookmarks"] = bmArr;

        // Tracks
        nlohmann::json tracksObj;
        nlohmann::json overlayArr = nlohmann::json::array();
        for (const auto& t : scene.timeline().overlayTracks()) {
            overlayArr.push_back(trackToJson(t));
        }
        tracksObj["overlay"] = overlayArr;
        tracksObj["main"] = trackToJson(scene.timeline().mainTrack());
        nlohmann::json audioArr = nlohmann::json::array();
        for (const auto& t : scene.timeline().audioTracks()) {
            audioArr.push_back(trackToJson(t));
        }
        tracksObj["audio"] = audioArr;

        sceneObj["tracks"] = tracksObj;
        scenesArr.push_back(sceneObj);
    }
    j["scenes"] = scenesArr;

    // View State
    const auto& vs = project.viewState();
    j["timelineViewState"] = {
        {"zoomLevel", vs.zoomLevel},
        {"scrollLeft", vs.scrollLeft},
        {"playheadTime", vs.playheadTime.ticks()}
    };

    return j;
}

core::Result<Project> ProjectSerializer::fromJson(const nlohmann::json& j) {
    if (!j.is_object()) {
        return core::Result<Project>(core::ErrorCode::FileCorrupted, "Invalid project JSON root");
    }

    std::string name = "New project";
    if (j.contains("metadata") && j["metadata"].contains("name")) {
        name = j["metadata"]["name"].get<std::string>();
    }

    Project project(name);

    if (j.contains("metadata")) {
        const auto& m = j["metadata"];
        project.metadata().id = core::ProjectId(m.value("id", ""));
        project.metadata().thumbnail = m.value("thumbnail", "");
        project.metadata().createdAt = m.value("createdAt", "");
        project.metadata().updatedAt = m.value("updatedAt", "");
    }

    // Older project files have no media manifest. Keep that format readable.
    if (j.contains("mediaAssets") && j["mediaAssets"].is_array()) {
        for (const auto& mediaJson : j["mediaAssets"]) {
            if (!mediaJson.is_object()) continue;
            ProjectMediaReference asset;
            asset.id = mediaJson.value("id", std::string{});
            asset.path = mediaJson.value("path", std::string{});
            asset.displayName = mediaJson.value("displayName", std::string{});
            asset.type = mediaJson.value("type", std::string{});
            if (asset.id.empty() || asset.path.empty() ||
                (asset.type != "video" && asset.type != "audio" && asset.type != "image")) {
                continue;
            }
            asset.fileSize = mediaJson.value("fileSize", uint64_t{0});
            asset.durationTicks = mediaJson.value("durationTicks", int64_t{0});
            asset.width = mediaJson.value("width", int32_t{0});
            asset.height = mediaJson.value("height", int32_t{0});
            asset.fpsNumerator = mediaJson.value("fpsNumerator", int32_t{30});
            asset.fpsDenominator = mediaJson.value("fpsDenominator", int32_t{1});
            if (asset.fpsDenominator <= 0) asset.fpsDenominator = 1;
            asset.hasAudio = mediaJson.value("hasAudio", false);
            asset.hasVideo = mediaJson.value("hasVideo", false);
            asset.sampleRate = mediaJson.value("sampleRate", int32_t{44100});
            asset.channels = mediaJson.value("channels", int32_t{2});
            asset.codec = mediaJson.value("codec", std::string{});
            project.mediaAssets().push_back(std::move(asset));
        }
    }

    if (j.contains("settings")) {
        const auto& s = j["settings"];
        if (s.contains("fps")) {
            project.settings().fps.numerator = s["fps"].value("numerator", 30);
            project.settings().fps.denominator = s["fps"].value("denominator", 1);
        }
        if (s.contains("canvasSize")) {
            project.settings().canvasSize.width = s["canvasSize"].value("width", 1920);
            project.settings().canvasSize.height = s["canvasSize"].value("height", 1080);
        }
        project.settings().canvasSizeMode = s.value("canvasSizeMode", "preset");
        if (s.contains("background")) {
            const auto& bg = s["background"];
            std::string bgType = bg.value("type", "color");
            project.settings().background.type = (bgType == "blur") ? ProjectBackground::Type::Blur : ProjectBackground::Type::Color;
            project.settings().background.color = bg.value("color", "#000000");
            project.settings().background.blurIntensity = bg.value("blurIntensity", 10.0);
        }
    }

    if (j.contains("currentSceneId")) {
        project.setCurrentSceneId(core::SceneId(j["currentSceneId"].get<std::string>()));
    }

    if (j.contains("scenes") && j["scenes"].is_array()) {
        project.scenes().clear();
        for (const auto& sJson : j["scenes"]) {
            core::SceneId sId(sJson.value("id", ""));
            std::string sName = sJson.value("name", "Main scene");
            bool isMain = sJson.value("isMain", false);

            Scene scene(std::move(sId), std::move(sName), isMain);

            if (sJson.contains("bookmarks") && sJson["bookmarks"].is_array()) {
                for (const auto& bmJson : sJson["bookmarks"]) {
                    scene.timeline().addBookmark(bookmarkFromJson(bmJson));
                }
            }

            if (sJson.contains("tracks")) {
                const auto& tracksJson = sJson["tracks"];
                if (tracksJson.contains("main")) {
                    scene.timeline().mainTrack() = trackFromJson(tracksJson["main"]);
                }
                if (tracksJson.contains("overlay") && tracksJson["overlay"].is_array()) {
                    for (const auto& tJson : tracksJson["overlay"]) {
                        scene.timeline().overlayTracks().push_back(trackFromJson(tJson));
                    }
                }
                if (tracksJson.contains("audio") && tracksJson["audio"].is_array()) {
                    for (const auto& tJson : tracksJson["audio"]) {
                        scene.timeline().audioTracks().push_back(trackFromJson(tJson));
                    }
                }
            }
            project.scenes().push_back(std::move(scene));
        }
    }

    if (j.contains("timelineViewState")) {
        const auto& vs = j["timelineViewState"];
        project.viewState().zoomLevel = vs.value("zoomLevel", 1.0);
        project.viewState().scrollLeft = vs.value("scrollLeft", 0.0);
        project.viewState().playheadTime = core::TimelineTime(vs.value("playheadTime", int64_t(0)));
    }

    project.setDirty(false);
    return core::Result<Project>(std::move(project));
}

std::string ProjectSerializer::serialize(const Project& project, int indent) {
    return toJson(project).dump(indent);
}

core::Result<Project> ProjectSerializer::deserialize(std::string_view jsonStr) {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonStr);
        return fromJson(j);
    } catch (const std::exception& ex) {
        return core::Result<Project>(core::ErrorCode::FileCorrupted, ex.what());
    }
}

} // namespace catchim::editor

#include "core/time/TimelineTime.h"
#include "core/time/Timecode.h"
#include "core/math/Bezier.h"
#include "editor/project/Project.h"
#include "editor/project/ProjectSerializer.h"
#include "editor/timeline/Timeline.h"
#include "editor/timeline/SnapEngine.h"
#include "editor/history/CommandHistory.h"
#include "editor/history/commands/TimelineCommands.h"
#include "editor/history/commands/AdvancedTimelineCommands.h"
#include "editor/actions/ActionRegistry.h"
#include "editor/timeline/Bookmark.h"
#include "editor/animation/Keyframe.h"
#include "editor/animation/AnimationChannel.h"
#include "export/ExportSettings.h"
#include "export/SceneExporter.h"
#include "editor/history/commands/AudioSeparationCommands.h"
#include "editor/clipboard/ClipboardManager.h"
#include "render/Compositor.h"
#include "render/effects/ClipEffectRenderer.h"
#include "render/TextRasterizer.h"
#include "render/MaskEngine.h"
#include "audio/AudioResampler.h"
#include "storage/ProjectMigrator.h"
#include "editor/timeline/TimelineZoomController.h"
#include "editor/selection/SelectionManager.h"
#include "editor/retime/RetimeEngine.h"
#include "subtitles/SubtitleCue.h"
#include "subtitles/SrtParser.h"
#include "subtitles/SrtSerializer.h"
#include "editor/history/commands/ImportSubtitlesCommand.h"
#include "render/ShapeRenderer.h"
#include "audio/AudioMastering.h"
#include "render/SafeZoneGuide.h"
#include "render/TransitionEngine.h"
#include "editor/timeline/RipplePipeline.h"
#include "storage/ProjectBundle.h"
#include "media/waveform/WaveformBucketer.h"
#include "render/HitTesting.h"
#include "export/ExportGeometryResolver.h"
#include "editor/project/ProjectDiagnostics.h"
#include "export/StillImageExporter.h"
#include "editor/EditorEngine.h"
#include "editor/timeline/RulerEngine.h"
#include "editor/timeline/TimelineViewModel.h"
#include "render/PreviewSnap.h"
#include "render/PreviewViewModel.h"
#include "audio/AudioPlaybackEngine.h"
#include "media/VideoFrameCache.h"
#include "editor/properties/PropertiesViewModel.h"
#include "subtitles/TranscriptionEngine.h"
#include "media/StickerRegistry.h"
#include "media/PresetManager.h"
#include "editor/animation/SpatialMotionPath.h"
#include "render/ColorWheelEngine.h"
#include "audio/AudioDucker.h"
#include "editor/timeline/GroupMoveEngine.h"
#include "audio/TtsEngine.h"
#include "media/SoundEffectsRegistry.h"
#include "render/FontRegistry.h"
#include "core/time/RationalFrameRate.h"
#include "editor/timeline/GroupResizeEngine.h"
#include "editor/timeline/PlacementEngine.h"
#include "audio/AudioDspFilters.h"
#include "core/utils/ColorUtils.h"
#include "editor/timeline/TimelineLayoutEngine.h"
#include "audio/AudioDisplayMetrics.h"
#include "editor/timeline/AdvancedSnapEngine.h"
#include "editor/history/commands/BatchCommand.h"
#include "editor/timeline/ElementFactory.h"
#include "editor/timeline/ElementUtils.h"
#include "editor/clipboard/ClipboardKeyframeEngine.h"
#include "editor/history/commands/TrackCommands.h"
#include "editor/history/commands/DuplicateElementsCommand.h"
#include "render/effects/BlurEffect.h"
#include "render/graphics/GraphicGeometry.h"
#include "editor/history/commands/KeyframeCommands.h"
#include "render/text/TextLayoutEngine.h"
#include "render/masks/FreeformMaskGeometry.h"
#include "editor/history/commands/MaskCommands.h"
#include "editor/history/commands/EffectCommands.h"
#include "editor/canvas/CanvasViewportController.h"
#include "editor/history/commands/ElementCommands.h"
#include "editor/animation/EffectParamAnimationEngine.h"
#include "editor/history/commands/EffectKeyframeCommands.h"
#include "render/transform/TransformHandleSession.h"
#include "editor/history/commands/SplitElementsCommand.h"
#include "render/PreviewInteractionEngine.h"
#include "audio/AudioRetimeEngine.h"
#include "editor/history/commands/MoveElementsCommand.h"
#include "editor/history/commands/InsertElementCommand.h"
#include "editor/history/commands/ProjectCommands.h"
#include "editor/history/commands/SceneCommands.h"
#include "editor/history/commands/MediaCommands.h"
#include "editor/history/commands/PreviewTracker.h"
#include "editor/timeline/AudioSeparationEngine.h"
#include "editor/history/commands/ToggleSourceAudioSeparationCommand.h"
#include "editor/timeline/TimelineDragSource.h"
#include "editor/selection/EditorSelection.h"
#include "media/waveform/WaveformSummaryEngine.h"
#include "editor/canvas/CanvasBackgroundEngine.h"
#include "editor/project/ProjectManager.h"
#include "audio/AudioStateEngine.h"
#include "render/PreviewCoordinateTransformer.h"
#include "render/ElementBoundsEngine.h"
#include "editor/actions/KeybindingEngine.h"
#include "editor/project/ProjectAutosaveEngine.h"
#include "editor/scene/ScenesManager.h"
#include "core/math/MathExpressionEvaluator.h"
#include "editor/timeline/TrackCapabilityEngine.h"
#include "storage/StorageQuotaEngine.h"
#include "editor/timeline/TimelineElementUpdatePipeline.h"
#include "export/ExportCanvasGeometryResolver.h"
#include "media/MediaAssetInspector.h"
#include "editor/timeline/TimelineCoordinateEngine.h"
#include "core/time/ProjectFrameRateEngine.h"
#include "render/guides/GuideOverlayEngine.h"
#include "editor/params/ElementParamRegistry.h"
#include "render/text/TextRenderingPrimitives.h"
#include "editor/timeline/TimelineElementBuilder.h"
#include "editor/subtitles/AssSubtitleParser.h"
#include "editor/subtitles/SubtitleElementBuilder.h"
#include "render/gradients/CssGradientEngine.h"
#include "render/canvas/CanvasTransformPipeline.h"
#include "render/diagnostics/RenderPerformanceProfiler.h"
#include "editor/diagnostics/DiagnosticsRegistry.h"
#include "render/canvas/CanvasPresetEngine.h"
#include "core/time/EuclideanFrameSnapper.h"
#include "render/masks/BuiltinMaskGeometry.h"
#include "render/masks/MaskSnapEngine.h"
#include "render/masks/MaskInteractionEngine.h"
#include "render/masks/MaskRegistry.h"
#include "render/scene/SceneNodes.h"
#include "render/scene/SceneBuilder.h"
#include "render/scene/SceneTreeResolver.h"
#include "render/scene/FrameDescriptorBuilder.h"
#include "render/compositor/RenderSurface.h"
#include "render/compositor/TextureCacheManager.h"
#include "render/compositor/CanvasRenderer.h"
#include "render/effects/EffectPreviewService.h"
#include "editor/timeline/TimelineSnappingEngine.h"
#include "editor/timeline/TimelineSnapPointSources.h"
#include "editor/animation/AnimationTargetResolver.h"
#include "editor/timeline/TimelineDefaults.h"
#include "editor/animation/AnimationTransformResolver.h"
#include "render/PreviewOverlayManager.h"
#include "editor/cancel/InteractionCancellationRegistry.h"
#include "editor/panels/PanelLayoutManager.h"
#include "media/MediaThumbnailEngine.h"
#include "render/RenderingParamsResolver.h"
#include "editor/timeline/TrackDefaults.h"
#include "core/math/MathFormattingUtils.h"
#include "export/ExportDefaults.h"
#include "editor/timeline/BookmarkEngine.h"
#include "editor/timeline/TimelineUiStore.h"
#include "editor/timeline/TrackCompatibilityEngine.h"
#include "editor/timeline/TrackInsertResolver.h"
#include "editor/timeline/GroupMoveSnapEngine.h"
#include "editor/timeline/RippleDiffEngine.h"
#include "editor/timeline/TrackElementUpdateEngine.h"
#include "editor/actions/KeybindingMigrationEngine.h"
#include "editor/animation/AnimationKeyframeQueryEngine.h"
#include "editor/history/CommandReactorPipeline.h"
#include "editor/timeline/TimelinePixelUtils.h"
#include "editor/timeline/TimelineZoomUtils.h"
#include "editor/timeline/TimelineDragData.h"
#include "render/background/BackgroundPresets.h"
#include "editor/params/ParamChannelLayoutEngine.h"
#include "core/i18n/I18nEngine.h"
#include "editor/retime/RetimeResolutionEngine.h"
#include "render/effects/EffectDefinitionRegistry.h"
#include "render/canvas/CanvasSizePresets.h"
#include "editor/scene/SceneHierarchyUtils.h"
#include "core/utils/UuidGenerator.h"
#include "core/utils/StringUtils.h"
#include "core/utils/GeometryUtils.h"
#include "core/utils/DateUtils.h"
#include "render/RenderParamResolvers.h"
#include "editor/animation/AnimationValueResolvers.h"
#include "editor/timeline/TimelineTrackDefaults.h"
#include "media/TtsVoiceRegistry.h"
#include "editor/playback/PlaybackManager.h"
#include "editor/timeline/TimelineManager.h"
#include "render/RendererManager.h"
#include "editor/project/SaveManager.h"
#include "media/MediaManager.h"
#include "audio/AudioManager.h"
#include "subtitles/TranscriptionCatalog.h"
#include "render/masks/MaskGeometryUtils.h"
#include "media/StickerIdUtils.h"
#include "editor/core/EditorCore.h"
#include "media/SavedSoundsStore.h"
#include "render/canvas/CanvasSnapMath.h"
#include "subtitles/TranscriptionCaptionBuilder.h"
#include "editor/diagnostics/DiagnosticsManager.h"
#include "render/graphics/GraphicsDefinitions.h"
#include "editor/commands/CommandManager.h"
#include "media/WaveformCache.h"
#include "render/canvas/BackgroundBlurPresets.h"
#include "render/canvas/PatternCraftGradients.h"
#include "export/ExportOptionsResolver.h"
#include "subtitles/TranscriptionService.h"
#include "media/TtsVoiceService.h"
#include "editor/panels/PanelLayoutConfig.h"
#include "audio/AudioMediaUtils.h"
#include "editor/timeline/controllers/SeekController.h"
#include "editor/timeline/controllers/PlayheadController.h"
#include "editor/timeline/controllers/KeyframeDragController.h"
#include "editor/timeline/controllers/ResizeController.h"
#include "editor/selection/SelectionStateEngine.h"
#include "editor/timeline/TimelineCreationDefaults.h"
#include "editor/timeline/controllers/TimelineDropTargetResolver.h"
#include "editor/timeline/controllers/TimelineDragDropController.h"
#include "editor/timeline/controllers/TimelineElementInteractionController.h"
#include "editor/timeline/TimelineInteractionMetrics.h"
#include "editor/timeline/TimelineElementFactory.h"
#include "editor/timeline/TimelineDragUtils.h"
#include "editor/timeline/controllers/TimelineInteractiveZoomController.h"
#include "editor/animation/GraphEditorSessionEngine.h"
#include "editor/animation/GraphEditorEasingPresets.h"
#include "editor/timeline/TrackLayoutMetrics.h"
#include "editor/timeline/SelectionHitTesting.h"
#include "editor/export/ExportMimeTypesAndLayers.h"
#include "editor/timeline/TimelineTheme.h"
#include "editor/preview/PreviewSettingsStore.h"
#include "editor/retime/RetimeRateEngine.h"
#include "editor/preview/controllers/TransformHandleController.h"
#include "editor/preview/controllers/PreviewInteractionGestureController.h"
#include "render/effects/MaskFeatherEngine.h"
#include "native/OpencutNativeCoreBindings.h"
#include "editor/panels/PanelStoreEngine.h"
#include "render/text/TextElementMeasurementEngine.h"
#include "editor/timecode/EditableTimecodeController.h"
#include "core/project/ProjectOrganizationEngine.h"
#include "storage/StorageServiceCoordinator.h"
#include "audio/AudioVolumeLineEngine.h"
#include "audio/AudioWaveformBarEngine.h"
#include "audio/TtsServiceEngine.h"
#include "editor/scene/TimelineSceneUtils.h"
#include "audio/AudibleCandidateCollector.h"
#include "editor/timeline/TimelineToolbarEngine.h"
#include "editor/timeline/RulerIntervalEngine.h"
#include "editor/timeline/TimelineExpandedLayoutEngine.h"
#include "editor/panels/AssetsPanelStoreEngine.h"
#include "editor/panels/PropertiesPanelStoreEngine.h"
#include "editor/retime/SpeedInputController.h"
#include "storage/NativeFileSystemStorageAdapter.h"
#include "editor/timeline/TimelineEdgeScrollEngine.h"
#include "editor/timeline/TimelineSnapIndicatorEngine.h"
#include "media/MediaTypeUtils.h"
#include "utils/PlatformBrowserUtils.h"
#include "media/MediaProcessingPipeline.h"
#include "editor/actions/KeybindingPersistenceEngine.h"
#include "core/time/MediaTimeUtils.h"
#include "editor/properties/PropertyDraftController.h"
#include "editor/timeline/controllers/BookmarkDragController.h"
#include "subtitles/TranscriptionLanguagesRegistry.h"
#include "media/MediaUploadToastEngine.h"
#include "editor/core/EditorAppStoreEngine.h"
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << " -> " << #cond << std::endl; \
            std::exit(1); \
        } \
    } while (0)

using namespace catchim::core;
using namespace catchim::editor;
using namespace catchim::exporting;
using namespace catchim::render;
using namespace catchim::audio;
using namespace catchim::storage;
using namespace catchim::subtitles;
using namespace catchim::media;

void runTimeTests() {
    TimelineTime t1 = TimelineTime::fromSeconds(1.0);
    TEST_ASSERT(t1.ticks() == 120'000);
    TEST_ASSERT(t1.toSeconds() == 1.0);

    TimelineTime t2 = TimelineTime::fromSeconds(0.5);
    TEST_ASSERT(t2.ticks() == 60'000);

    TimelineTime t3 = t1 + t2;
    TEST_ASSERT(t3.ticks() == 180'000);
    TEST_ASSERT(t3.toSeconds() == 1.5);

    FrameRate fps30{30, 1};
    TimelineTime unaligned = TimelineTime::fromTicks(4100);
    TimelineTime rounded = unaligned.roundToFrame(fps30);
    TEST_ASSERT(rounded.ticks() == 4000);

    TimelineTime t = TimelineTime::fromSeconds(125.5); // 00:02:05:15
    std::string tc = Timecode::format(t, TimecodeFormat::HH_MM_SS_FF, fps30);
    TEST_ASSERT(tc == "00:02:05:15");

    auto parsedOpt = Timecode::parse(tc, TimecodeFormat::HH_MM_SS_FF, fps30);
    TEST_ASSERT(parsedOpt.has_value());
    TEST_ASSERT(parsedOpt->toSeconds() == 125.5);

    std::cout << "[PASS] runTimeTests" << std::endl;
}

void runBezierTests() {
    double y1 = BezierSolver::solve(0.0, 0.25, 0.1, 0.25, 1.0);
    TEST_ASSERT(std::abs(y1 - 0.0) < 1e-4);

    double y2 = BezierSolver::solve(1.0, 0.25, 0.1, 0.25, 1.0);
    TEST_ASSERT(std::abs(y2 - 1.0) < 1e-4);

    double yMid = BezierSolver::solve(0.5, 0.25, 0.1, 0.25, 1.0);
    TEST_ASSERT(yMid > 0.0 && yMid < 1.0);

    std::cout << "[PASS] runBezierTests" << std::endl;
}

void runTimelineTests() {
    Timeline timeline;
    TEST_ASSERT(timeline.allTracks().size() == 1); // Main track by default
    TEST_ASSERT(timeline.mainTrack().name() == "Video");

    // Add audio track
    Track& audioTrack = timeline.addTrack(TrackType::Audio, "Voiceover");
    (void)audioTrack;
    TEST_ASSERT(timeline.allTracks().size() == 2);

    // Insert clip on main track
    ClipId c1 = ClipId::generate();
    Clip clip1(c1, ClipType::Video, "Sample.mp4", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(5.0));
    bool ok = timeline.addClip(timeline.mainTrack().id(), std::move(clip1));
    TEST_ASSERT(ok);
    TEST_ASSERT(timeline.totalDuration().toSeconds() == 5.0);

    // Collision test: cannot place overlapping clip at 2s with duration 4s (overlaps 0s-5s)
    ClipId c2 = ClipId::generate();
    Clip clip2(c2, ClipType::Video, "Overlap.mp4", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(4.0));
    bool collisionFail = timeline.addClip(timeline.mainTrack().id(), std::move(clip2));
    TEST_ASSERT(!collisionFail);

    // Non-overlapping placement at 6s with duration 3s
    ClipId c3 = ClipId::generate();
    Clip clip3(c3, ClipType::Video, "Second.mp4", TimelineTime::fromSeconds(6.0), TimelineTime::fromSeconds(3.0));
    bool nonOverlapOk = timeline.addClip(timeline.mainTrack().id(), std::move(clip3));
    TEST_ASSERT(nonOverlapOk);
    TEST_ASSERT(timeline.totalDuration().toSeconds() == 9.0);

    // Split clip test
    auto [leftId, rightId] = timeline.splitClip(c1, TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(leftId.has_value() && *leftId == c1);
    TEST_ASSERT(rightId.has_value());

    const Clip* leftClip = timeline.findClip(*leftId);
    const Clip* rightClip = timeline.findClip(*rightId);
    TEST_ASSERT(leftClip != nullptr);
    TEST_ASSERT(rightClip != nullptr);
    TEST_ASSERT(leftClip->duration().toSeconds() == 2.0);
    TEST_ASSERT(rightClip->startTime().toSeconds() == 2.0);
    TEST_ASSERT(rightClip->duration().toSeconds() == 3.0);

    // Snap test
    SnapResult snapRes = SnapEngine::snap(
        timeline,
        TimelineTime::fromSeconds(2.05), // near 2.0s split point
        TimelineTime::fromSeconds(0.0),
        TimelineTime::fromSeconds(0.1) // 100ms threshold
    );
    TEST_ASSERT(snapRes.hasSnapped);
    TEST_ASSERT(snapRes.snappedTime.toSeconds() == 2.0);

    std::cout << "[PASS] runTimelineTests" << std::endl;
}

void runCommandHistoryTests() {
    EditorEngine engine;
    engine.newProject("Test Project");

    Timeline* tl = engine.activeTimeline();
    TEST_ASSERT(tl != nullptr);
    TrackId mainTrackId = tl->mainTrack().id();

    ClipId c1 = ClipId::generate();
    Clip clip(c1, ClipType::Video, "Intro.mp4", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(10.0));

    // Execute Add Clip
    TEST_ASSERT(engine.canUndo() == false);
    bool added = engine.addClip(mainTrackId, std::move(clip));
    TEST_ASSERT(added);
    TEST_ASSERT(engine.canUndo() == true);
    TEST_ASSERT(engine.activeTimeline()->totalDuration().toSeconds() == 10.0);

    // Undo Add Clip
    TEST_ASSERT(engine.undo() == true);
    TEST_ASSERT(engine.activeTimeline()->totalDuration().toSeconds() == 0.0);
    TEST_ASSERT(engine.canUndo() == false);
    TEST_ASSERT(engine.canRedo() == true);

    // Redo Add Clip
    TEST_ASSERT(engine.redo() == true);
    TEST_ASSERT(engine.activeTimeline()->totalDuration().toSeconds() == 10.0);
    TEST_ASSERT(engine.canUndo() == true);

    // Split at 4s
    TEST_ASSERT(engine.splitClip(c1, TimelineTime::fromSeconds(4.0)));
    TEST_ASSERT(engine.activeTimeline()->mainTrack().clips().size() == 2);

    // Undo Split
    TEST_ASSERT(engine.undo());
    TEST_ASSERT(engine.activeTimeline()->mainTrack().clips().size() == 1);
    TEST_ASSERT(engine.activeTimeline()->mainTrack().clips()[0].duration().toSeconds() == 10.0);

    std::cout << "[PASS] runCommandHistoryTests" << std::endl;
}

void runSerializationTests() {
    Project project("Catchim Masterpiece");
    project.settings().fps = FrameRate{60, 1};
    project.settings().canvasSize = CanvasSize{3840, 2160};
    project.settings().background.color = "#112233";

    Timeline* tl = project.activeTimeline();
    TEST_ASSERT(tl != nullptr);

    ClipId cid = ClipId::generate();
    Clip clip(cid, ClipType::Video, "4k_nature.mp4", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(8.0));
    clip.setParam<double>("transform.scaleX", 1.5);
    tl->addClip(tl->mainTrack().id(), std::move(clip));

    // Serialize
    std::string jsonStr = ProjectSerializer::serialize(project);
    TEST_ASSERT(!jsonStr.empty());

    // Deserialize
    auto res = ProjectSerializer::deserialize(jsonStr);
    TEST_ASSERT(res.ok());
    Project loaded = res.unwrap();

    // Verify Parity
    TEST_ASSERT(loaded.name() == "Catchim Masterpiece");
    TEST_ASSERT(loaded.settings().fps.numerator == 60);
    TEST_ASSERT(loaded.settings().canvasSize.width == 3840);
    TEST_ASSERT(loaded.settings().background.color == "#112233");

    const Timeline* loadedTl = loaded.activeTimeline();
    TEST_ASSERT(loadedTl != nullptr);
    TEST_ASSERT(loadedTl->mainTrack().clips().size() == 1);
    const Clip& loadedClip = loadedTl->mainTrack().clips()[0];
    TEST_ASSERT(loadedClip.id() == cid);
    TEST_ASSERT(loadedClip.startTime().toSeconds() == 1.0);
    TEST_ASSERT(loadedClip.duration().toSeconds() == 8.0);
    TEST_ASSERT(loadedClip.getParam<double>("transform.scaleX", 1.0) == 1.5);

    std::cout << "[PASS] runSerializationTests" << std::endl;
}

void runActionRegistryTests() {
    auto& reg = ActionRegistry::instance();
    TEST_ASSERT(reg.findAction("split-left") != nullptr);
    TEST_ASSERT(reg.findAction("toggle-play") != nullptr);

    bool invoked = false;
    reg.bindAction("toggle-play", [&]() { invoked = true; });

    bool ok = reg.invokeAction("toggle-play");
    TEST_ASSERT(ok == true);
    TEST_ASSERT(invoked == true);

    // Invoke shortcut
    invoked = false;
    bool scOk = reg.invokeShortcut("space");
    TEST_ASSERT(scOk == true);
    TEST_ASSERT(invoked == true);

    std::cout << "[PASS] runActionRegistryTests" << std::endl;
}

void runBookmarkTests() {
    Timeline tl;
    TEST_ASSERT(tl.bookmarks().empty());

    bool added = tl.toggleBookmark(TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(added == true);
    TEST_ASSERT(tl.bookmarks().size() == 1);
    TEST_ASSERT(tl.bookmarks()[0].time.toSeconds() == 2.0);

    // Toggle again -> removes it
    bool addedAgain = tl.toggleBookmark(TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(addedAgain == false);
    TEST_ASSERT(tl.bookmarks().empty());

    // Add 3 bookmarks: 4s, 1s, 7s -> will be sorted
    tl.toggleBookmark(TimelineTime::fromSeconds(4.0));
    tl.toggleBookmark(TimelineTime::fromSeconds(1.0));
    tl.toggleBookmark(TimelineTime::fromSeconds(7.0));
    TEST_ASSERT(tl.bookmarks().size() == 3);
    TEST_ASSERT(tl.bookmarks()[0].time.toSeconds() == 1.0);
    TEST_ASSERT(tl.bookmarks()[1].time.toSeconds() == 4.0);
    TEST_ASSERT(tl.bookmarks()[2].time.toSeconds() == 7.0);

    // Next/Prev
    auto next = tl.findNextBookmark(TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(next.has_value());
    TEST_ASSERT(next->time.toSeconds() == 4.0);

    auto prev = tl.findPrevBookmark(TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(prev.has_value());
    TEST_ASSERT(prev->time.toSeconds() == 4.0);

    // Snap to bookmark
    SnapResult snap = SnapEngine::snap(
        tl,
        TimelineTime::fromSeconds(3.95),
        TimelineTime::fromSeconds(0.0),
        TimelineTime::fromSeconds(0.1)
    );
    TEST_ASSERT(snap.hasSnapped == true);
    TEST_ASSERT(snap.snappedTime.toSeconds() == 4.0);

    std::cout << "[PASS] runBookmarkTests" << std::endl;
}

void runAdvancedEditingTests() {
    EditorEngine engine;
    engine.newProject("Advanced Edit Project");
    Timeline* tl = engine.activeTimeline();
    TEST_ASSERT(tl != nullptr);

    TrackId mainTrackId = tl->mainTrack().id();
    ClipId c1 = ClipId::generate();
    engine.addClip(mainTrackId, Clip(c1, ClipType::Video, "Video.mp4", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(10.0)));

    // 1. Duplicate clip
    bool dupOk = engine.duplicateClip(c1);
    TEST_ASSERT(dupOk == true);
    TEST_ASSERT(tl->mainTrack().clips().size() == 2);
    TEST_ASSERT(tl->mainTrack().clips()[1].startTime().toSeconds() == 10.0);
    TEST_ASSERT(tl->mainTrack().clips()[1].duration().toSeconds() == 10.0);

    // Undo duplicate
    TEST_ASSERT(engine.undo() == true);
    TEST_ASSERT(tl->mainTrack().clips().size() == 1);

    // 2. Split-Left at 3s (removes 0-3s, keeps 3-10s)
    engine.seek(TimelineTime::fromSeconds(3.0));
    bool splitLeftOk = engine.splitLeftAtPlayhead();
    TEST_ASSERT(splitLeftOk == true);
    TEST_ASSERT(tl->mainTrack().clips().size() == 1);
    TEST_ASSERT(tl->mainTrack().clips()[0].startTime().toSeconds() == 3.0);
    TEST_ASSERT(tl->mainTrack().clips()[0].duration().toSeconds() == 7.0);

    // Undo Split-Left
    TEST_ASSERT(engine.undo() == true);
    TEST_ASSERT(tl->mainTrack().clips()[0].startTime().toSeconds() == 0.0);
    TEST_ASSERT(tl->mainTrack().clips()[0].duration().toSeconds() == 10.0);

    // 3. Split-Right at 6s (keeps 0-6s, removes 6-10s)
    engine.seek(TimelineTime::fromSeconds(6.0));
    bool splitRightOk = engine.splitRightAtPlayhead();
    TEST_ASSERT(splitRightOk == true);
    TEST_ASSERT(tl->mainTrack().clips().size() == 1);
    TEST_ASSERT(tl->mainTrack().clips()[0].startTime().toSeconds() == 0.0);
    TEST_ASSERT(tl->mainTrack().clips()[0].duration().toSeconds() == 6.0);

    // Undo Split-Right
    TEST_ASSERT(engine.undo() == true);
    TEST_ASSERT(tl->mainTrack().clips()[0].duration().toSeconds() == 10.0);

    // 4. Ripple Delete
    // Add second clip at 12s-16s
    ClipId c2 = ClipId::generate();
    engine.addClip(mainTrackId, Clip(c2, ClipType::Video, "Second.mp4", TimelineTime::fromSeconds(12.0), TimelineTime::fromSeconds(4.0)));
    TEST_ASSERT(tl->mainTrack().clips().size() == 2);

    // Ripple delete c1 (duration 10s) -> c2 at 12s shifts left by 10s to 2s
    bool rippleOk = engine.rippleDelete(c1);
    TEST_ASSERT(rippleOk == true);
    TEST_ASSERT(tl->mainTrack().clips().size() == 1);
    TEST_ASSERT(tl->mainTrack().clips()[0].startTime().toSeconds() == 2.0);

    // Undo Ripple Delete
    TEST_ASSERT(engine.undo() == true);
    TEST_ASSERT(tl->mainTrack().clips().size() == 2);
    TEST_ASSERT(tl->mainTrack().clips()[1].startTime().toSeconds() == 12.0);

    std::cout << "[PASS] runAdvancedEditingTests" << std::endl;
}

void runKeyframeAnimationTests() {
    AnimationChannel chan("opacity", 1.0);
    TEST_ASSERT(chan.getValueAt(TimelineTime::fromSeconds(0.0)) == 1.0);

    // Linear keyframes: 0s -> 0.0, 2s -> 100.0
    Keyframe k1;
    k1.time = TimelineTime::fromSeconds(0.0);
    k1.value = 0.0;
    k1.interpolation = KeyframeInterpolation::Linear;

    Keyframe k2;
    k2.time = TimelineTime::fromSeconds(2.0);
    k2.value = 100.0;
    k2.interpolation = KeyframeInterpolation::Linear;

    chan.addOrUpdateKeyframe(k1);
    chan.addOrUpdateKeyframe(k2);

    TEST_ASSERT(chan.getValueAt(TimelineTime::fromSeconds(0.0)) == 0.0);
    TEST_ASSERT(chan.getValueAt(TimelineTime::fromSeconds(1.0)) == 50.0);
    TEST_ASSERT(chan.getValueAt(TimelineTime::fromSeconds(2.0)) == 100.0);

    // Hold keyframe test
    Keyframe kHold;
    kHold.time = TimelineTime::fromSeconds(0.0);
    kHold.value = 10.0;
    kHold.interpolation = KeyframeInterpolation::Hold;
    chan.addOrUpdateKeyframe(kHold);

    TEST_ASSERT(chan.getValueAt(TimelineTime::fromSeconds(0.5)) == 10.0);
    TEST_ASSERT(chan.getValueAt(TimelineTime::fromSeconds(1.99)) == 10.0);
    TEST_ASSERT(chan.getValueAt(TimelineTime::fromSeconds(2.0)) == 100.0);

    // Bezier keyframe test
    Keyframe kBez;
    kBez.time = TimelineTime::fromSeconds(0.0);
    kBez.value = 0.0;
    kBez.interpolation = KeyframeInterpolation::Bezier;
    chan.addOrUpdateKeyframe(kBez);

    double midVal = chan.getValueAt(TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(midVal > 0.0 && midVal < 100.0);

    std::cout << "[PASS] runKeyframeAnimationTests" << std::endl;
}

void runExportPipelineTests() {
    Timeline tl;
    Track& t = tl.mainTrack();
    ClipId c1 = ClipId::generate();
    tl.addClip(t.id(), Clip(c1, ClipType::Video, "ExportTest.mp4", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(1.0)));

    catchim::exporting::ExportSettings settings;
    settings.resolution = CanvasSize{640, 360};
    settings.fps = FrameRate{30, 1}; // 1.0s = 30 frames

    catchim::exporting::SceneExporter exporter(tl, settings);
    double lastProgress = 0.0;
    exporter.setProgressCallback([&](double p) {
        lastProgress = p;
    });

    bool exportOk = exporter.runExport();
    TEST_ASSERT(exportOk == true);
    TEST_ASSERT(exporter.totalFrames() == 30);
    TEST_ASSERT(exporter.renderedFrames() == 30);
    TEST_ASSERT(std::abs(lastProgress - 1.0) < 1e-4);

    std::cout << "[PASS] runExportPipelineTests" << std::endl;
}

void runAudioSeparationTests() {
    Timeline tl;
    CommandHistory history;
    TrackId mainTrackId = tl.mainTrack().id();

    ClipId vId = ClipId::generate();
    tl.addClip(mainTrackId, Clip(vId, ClipType::Video, "Movie.mp4", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(10.0)));

    Clip* vClip = tl.findClip(vId);
    TEST_ASSERT(vClip != nullptr);
    TEST_ASSERT(!vClip->isMuted());
    TEST_ASSERT(tl.audioTracks().empty());

    // Extract audio
    auto extractCmd = std::make_unique<ExtractSourceAudioCommand>(tl, vId);
    bool extOk = history.execute(std::move(extractCmd));
    TEST_ASSERT(extOk == true);
    TEST_ASSERT(vClip->isMuted() == true);
    TEST_ASSERT(vClip->getParam<bool>("sourceAudioSeparated", false) == true);
    TEST_ASSERT(tl.audioTracks().size() == 1);
    TEST_ASSERT(tl.audioTracks()[0].clips().size() == 1);
    TEST_ASSERT(tl.audioTracks()[0].clips()[0].name() == "Movie.mp4 (Audio)");

    // Undo extraction
    TEST_ASSERT(history.undo() == true);
    TEST_ASSERT(vClip->isMuted() == false);
    TEST_ASSERT(vClip->getParam<bool>("sourceAudioSeparated", false) == false);
    TEST_ASSERT(tl.audioTracks()[0].clips().empty());

    // Redo extraction
    TEST_ASSERT(history.redo() == true);
    TEST_ASSERT(vClip->isMuted() == true);
    TEST_ASSERT(tl.audioTracks()[0].clips().size() == 1);

    // Recover audio
    auto recoverCmd = std::make_unique<RecoverSourceAudioCommand>(tl, vId);
    bool recOk = history.execute(std::move(recoverCmd));
    TEST_ASSERT(recOk == true);
    TEST_ASSERT(vClip->isMuted() == false);
    TEST_ASSERT(vClip->getParam<bool>("sourceAudioSeparated", false) == false);
    TEST_ASSERT(tl.audioTracks()[0].clips().empty());

    std::cout << "[PASS] runAudioSeparationTests" << std::endl;
}

void runClipboardTests() {
    Timeline tl;
    CommandHistory history;
    TrackId mainTrackId = tl.mainTrack().id();

    ClipId c1 = ClipId::generate();
    ClipId c2 = ClipId::generate();
    tl.addClip(mainTrackId, Clip(c1, ClipType::Video, "ClipA.mp4", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(3.0)));
    tl.addClip(mainTrackId, Clip(c2, ClipType::Video, "ClipB.mp4", TimelineTime::fromSeconds(8.0), TimelineTime::fromSeconds(4.0)));

    auto& cb = ClipboardManager::instance();
    bool copyOk = cb.copy(tl, {c1, c2});
    TEST_ASSERT(copyOk == true);
    TEST_ASSERT(cb.hasData() == true);
    TEST_ASSERT(cb.count() == 2);

    // Paste at 20s -> c1 at 20s (offset 0), c2 at 26s (offset 6s)
    bool pasteOk = cb.paste(tl, history, TimelineTime::fromSeconds(20.0));
    TEST_ASSERT(pasteOk == true);
    TEST_ASSERT(cb.lastPastedIds().size() == 2);
    TEST_ASSERT(tl.mainTrack().clips().size() == 4);

    const Clip* p1 = tl.findClip(cb.lastPastedIds()[0]);
    const Clip* p2 = tl.findClip(cb.lastPastedIds()[1]);
    TEST_ASSERT(p1 != nullptr && p2 != nullptr);
    TEST_ASSERT(p1->startTime().toSeconds() == 20.0);
    TEST_ASSERT(p2->startTime().toSeconds() == 26.0);

    // Undo paste
    TEST_ASSERT(history.undo() == true);
    TEST_ASSERT(tl.mainTrack().clips().size() == 2);

    std::cout << "[PASS] runClipboardTests" << std::endl;
}

void runSceneManagementTests() {
    Project project("MultiScene Project");
    TEST_ASSERT(project.scenes().size() == 1);
    TEST_ASSERT(project.activeScene() != nullptr);

    Scene& s2 = project.createScene("Scene 2");
    TEST_ASSERT(project.scenes().size() == 2);
    TEST_ASSERT(s2.name() == "Scene 2");

    bool switched = project.switchScene(s2.id());
    TEST_ASSERT(switched == true);
    TEST_ASSERT(project.activeScene()->id() == s2.id());

    // Duplicate Scene 2
    auto dupOpt = project.duplicateScene(s2.id());
    TEST_ASSERT(dupOpt.has_value());
    TEST_ASSERT(project.scenes().size() == 3);

    // Delete duplicated scene
    bool delOk = project.deleteScene(*dupOpt);
    TEST_ASSERT(delOk == true);
    TEST_ASSERT(project.scenes().size() == 2);

    std::cout << "[PASS] runSceneManagementTests" << std::endl;
}

void runVisualEffectsTests() {
    // 1. Grayscale test
    std::vector<uint8_t> pixels = { 255, 0, 0, 255 }; // Pure Red
    Compositor::applyGrayscale(pixels.data(), 1, 1);
    // 0.299 * 255 ≈ 76
    TEST_ASSERT(pixels[0] == pixels[1] && pixels[1] == pixels[2]);
    TEST_ASSERT(pixels[0] == 76);

    // 2. Chroma Key test (Green screen)
    pixels = { 0, 255, 0, 255 }; // Pure Green
    Compositor::applyChromaKey(pixels.data(), 1, 1, 0, 255, 0, 0.1, 0.05);
    TEST_ASSERT(pixels[3] == 0); // Transparent!

    // 3. Gaussian blur test
    std::vector<uint8_t> blurBuf(10 * 10 * 4, 0);
    // Set center pixel
    blurBuf[(5 * 10 + 5) * 4 + 0] = 255;
    Compositor::applyGaussianBlur(blurBuf.data(), 10, 10, 2.0);
    // Adjacent pixels now have non-zero value
    int adjIdx = (5 * 10 + 6) * 4 + 0;
    TEST_ASSERT(blurBuf[adjIdx] > 0);

    std::cout << "[PASS] runVisualEffectsTests" << std::endl;
}

void runStructuredEffectStackRenderTests() {
    using namespace catchim::core;
    using namespace catchim::editor;

    Clip clip(ClipId::generate(), ClipType::Video, "Stacked effects",
              TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(2.0));
    EffectInstance grayscale;
    grayscale.id = "effect-grayscale";
    grayscale.type = "grayscale";
    grayscale.params = {{"intensity", 1.0}};
    clip.effects().push_back(grayscale);
    EffectInstance invert;
    invert.id = "effect-invert";
    invert.type = "invert";
    invert.params = {{"intensity", 1.0}};
    clip.effects().push_back(invert);

    std::vector<uint8_t> pixels{255, 0, 0, 255};
    catchim::render::ClipEffectRenderer::apply(pixels, 1, 1, clip, TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(pixels[0] == 179 && pixels[1] == 179 && pixels[2] == 179 && pixels[3] == 255);

    clip.effects().back().enabled = false;
    pixels = {255, 0, 0, 255};
    catchim::render::ClipEffectRenderer::apply(pixels, 1, 1, clip, TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(pixels[0] == 76 && pixels[1] == 76 && pixels[2] == 76 && pixels[3] == 255);

    Clip keyed(ClipId::generate(), ClipType::Video, "Green screen",
               TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(2.0));
    EffectInstance chroma;
    chroma.id = "effect-chroma";
    chroma.type = "chroma-key";
    chroma.params = {{"keyColor", "#00FF00"}, {"similarity", 0.4}, {"smoothness", 0.1}, {"spill", 0.1}};
    keyed.effects().push_back(chroma);
    pixels = {0, 255, 0, 255};
    catchim::render::ClipEffectRenderer::apply(pixels, 1, 1, keyed, TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(pixels[3] == 0);

    std::cout << "[PASS] runStructuredEffectStackRenderTests" << std::endl;
}

void runTextRasterizerTests() {
    TextProperties props;
    props.text = "OK";
    props.fontSize = 16;
    props.textR = 255; props.textG = 255; props.textB = 255; props.textA = 255;
    props.bgR = 50; props.bgG = 50; props.bgB = 50; props.bgA = 255;
    props.padding = 4;

    int32_t w = 0, h = 0;
    auto buffer = TextRasterizer::rasterize(props, w, h);
    TEST_ASSERT(w > 0 && h > 0);
    TEST_ASSERT(buffer.size() == static_cast<size_t>(w * h * 4));
    // Verify background at top-left corner
    TEST_ASSERT(buffer[0] == 50 && buffer[1] == 50 && buffer[2] == 50 && buffer[3] == 255);

    std::cout << "[PASS] runTextRasterizerTests" << std::endl;
}

void runAudioFadeAndRampingTests() {
    // 1. Resample test: 48000 -> 24000 (half the samples)
    std::vector<float> input = { 0.0f, 0.5f, 1.0f, 0.5f, 0.0f, -0.5f, -1.0f, -0.5f };
    auto resampled = AudioResampler::resampleLinear(input.data(), input.size(), 48000, 24000);
    TEST_ASSERT(resampled.size() == 4);

    // 2. Gain ramp calculation test (10s total, fadeIn 2.0s, fadeOut 2.0s)
    double gainStart = AudioResampler::calculateGainAt(0.0, 10.0, 2.0, 2.0);
    TEST_ASSERT(gainStart == 0.0);

    double gainMidFade = AudioResampler::calculateGainAt(1.0, 10.0, 2.0, 2.0);
    TEST_ASSERT(std::abs(gainMidFade - 0.5) < 1e-4);

    double gainFull = AudioResampler::calculateGainAt(5.0, 10.0, 2.0, 2.0);
    TEST_ASSERT(std::abs(gainFull - 1.0) < 1e-4);

    double gainFadeOut = AudioResampler::calculateGainAt(9.0, 10.0, 2.0, 2.0);
    TEST_ASSERT(std::abs(gainFadeOut - 0.5) < 1e-4);

    // 3. Apply fade ramp directly to buffer
    std::vector<float> buffer(44100, 1.0f); // 1.0s of audio at 44100Hz
    AudioResampler::applyFadeRamp(buffer.data(), buffer.size(), 44100, 0.2, 0.2);
    TEST_ASSERT(buffer[0] == 0.0f);
    TEST_ASSERT(buffer[22050] > 0.99f); // center is full volume

    std::cout << "[PASS] runAudioFadeAndRampingTests" << std::endl;
}

void runCropAndTransformTests() {
    Transform t;
    TEST_ASSERT(t.isDefault() == true);
    TEST_ASSERT(t.crop.hasCrop() == false);

    t.crop.top = 20.0;
    t.crop.bottom = 10.0;
    TEST_ASSERT(t.crop.hasCrop() == true);
    TEST_ASSERT(t.isDefault() == false);

    t.anchorX = 0.0;
    t.anchorY = 1.0;
    TEST_ASSERT(t.anchorX == 0.0 && t.anchorY == 1.0);

    std::cout << "[PASS] runCropAndTransformTests" << std::endl;
}

void runShapeMaskTests() {
    // 1. Rectangle mask
    MaskDefinition rectDef;
    rectDef.shape = MaskShape::Rectangle;
    rectDef.centerX = 0.5;
    rectDef.centerY = 0.5;
    rectDef.width = 0.5; // 50x50 area in 100x100
    rectDef.height = 0.5;
    rectDef.feather = 0.0;

    auto rectMask = MaskEngine::generateMask(rectDef, 100, 100);
    TEST_ASSERT(rectMask.size() == 10000);
    // Center pixel (50, 50) is opaque
    TEST_ASSERT(rectMask[50 * 100 + 50] == 255);
    // Corner pixel (0, 0) is transparent
    TEST_ASSERT(rectMask[0] == 0);

    // 2. Inverted mask
    rectDef.inverted = true;
    auto invMask = MaskEngine::generateMask(rectDef, 100, 100);
    TEST_ASSERT(invMask[50 * 100 + 50] == 0);
    TEST_ASSERT(invMask[0] == 255);

    // 3. Ellipse mask
    MaskDefinition ellDef;
    ellDef.shape = MaskShape::Ellipse;
    ellDef.centerX = 0.5;
    ellDef.centerY = 0.5;
    ellDef.width = 0.6;
    ellDef.height = 0.6;

    auto ellMask = MaskEngine::generateMask(ellDef, 100, 100);
    TEST_ASSERT(ellMask[50 * 100 + 50] == 255);
    TEST_ASSERT(ellMask[0] == 0);

    // 4. Apply mask to RGBA layer
    std::vector<uint8_t> layer(100 * 100 * 4, 255);
    MaskEngine::applyMaskToLayer(layer.data(), rectMask.data(), 100, 100);
    TEST_ASSERT(layer[(50 * 100 + 50) * 4 + 3] == 255);
    TEST_ASSERT(layer[0 * 4 + 3] == 0);

    std::cout << "[PASS] runShapeMaskTests" << std::endl;
}

void runProjectMigrationTests() {
    nlohmann::json oldProject = {
        {"version", 18},
        {"metadata", {{"name", "Legacy 2024 Project"}}}
    };

    TEST_ASSERT(ProjectMigrator::needsMigration(oldProject) == true);

    nlohmann::json migrated = ProjectMigrator::migrate(oldProject);
    TEST_ASSERT(migrated["version"] == 31);
    TEST_ASSERT(migrated["metadata"]["name"] == "Legacy 2024 Project");
    TEST_ASSERT(migrated.contains("scenes") && migrated["scenes"].is_array());
    TEST_ASSERT(migrated.contains("settings") && migrated["settings"].contains("canvasSize"));
    TEST_ASSERT(migrated.contains("timelineViewState"));

    TEST_ASSERT(ProjectMigrator::needsMigration(migrated) == false);

    std::cout << "[PASS] runProjectMigrationTests" << std::endl;
}

void runTimelineZoomTests() {
    TimelineZoomController zoom(100.0); // 100 pixels per second at zoom 1.0
    TEST_ASSERT(zoom.zoomLevel() == 1.0);

    // 2.0s -> 200px
    double px = zoom.timeToPixel(TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(px == 200.0);

    // 350px -> 3.5s
    TimelineTime t = zoom.pixelToTime(350.0);
    TEST_ASSERT(t.toSeconds() == 3.5);

    // Zoom in x2.0
    zoom.zoomIn(2.0);
    TEST_ASSERT(zoom.zoomLevel() == 2.0);
    TEST_ASSERT(zoom.timeToPixel(TimelineTime::fromSeconds(2.0)) == 400.0);

    // Viewport fit scale test
    double fitScale = TimelineZoomController::calculateFitScale(1920.0, 1080.0, 960.0, 540.0);
    TEST_ASSERT(std::abs(fitScale - 0.5) < 1e-4);

    double preset50 = TimelineZoomController::scaleForPreset(ViewportPreset::Scale50, fitScale);
    TEST_ASSERT(preset50 == 0.50);

    std::cout << "[PASS] runTimelineZoomTests" << std::endl;
}

void runSelectionTests() {
    SelectionManager sel;
    TEST_ASSERT(sel.empty());
    sel.select("c1");
    TEST_ASSERT(sel.count() == 1 && sel.isSelected("c1"));
    TEST_ASSERT(sel.getAnchorId() == "c1");

    // Toggle add
    sel.toggle("c2");
    TEST_ASSERT(sel.count() == 2 && sel.isSelected("c2"));
    TEST_ASSERT(sel.getAnchorId() == "c2");

    // Toggle remove
    sel.toggle("c1");
    TEST_ASSERT(sel.count() == 1 && !sel.isSelected("c1") && sel.isSelected("c2"));

    // Range select
    std::vector<std::string> ordered = {"c1", "c2", "c3", "c4", "c5"};
    sel.select("c2");
    sel.selectRange("c4", ordered, false);
    TEST_ASSERT(sel.count() == 3);
    TEST_ASSERT(sel.isSelected("c2") && sel.isSelected("c3") && sel.isSelected("c4"));
    TEST_ASSERT(!sel.isSelected("c1") && !sel.isSelected("c5"));

    // Box selection
    sel.applyBoxSelection({"c1", "c5"}, false);
    TEST_ASSERT(sel.count() == 2 && sel.isSelected("c1") && sel.isSelected("c5"));

    // Prune
    std::unordered_set<std::string> valid = {"c1", "c9"};
    sel.prune(valid);
    TEST_ASSERT(sel.count() == 1 && sel.isSelected("c1"));

    sel.clear();
    TEST_ASSERT(sel.empty());

    std::cout << "[PASS] runSelectionTests" << std::endl;
}

void runRetimeEngineTests() {
    // Rate clamping
    TEST_ASSERT(RetimeEngine::clampRate(0.0) == DEFAULT_RETIME_RATE);
    TEST_ASSERT(RetimeEngine::clampRate(10.0) == MAX_RETIME_RATE);
    TEST_ASSERT(RetimeEngine::clampRate(0.001) == MIN_RETIME_RATE);
    TEST_ASSERT(RetimeEngine::clampRate(2.0) == 2.0);

    // Time conversion
    TimelineTime clipT = TimelineTime::fromSeconds(2.0);
    TimelineTime srcT = RetimeEngine::getSourceTimeAtClipTime(clipT, 2.0);
    TEST_ASSERT(srcT.toSeconds() == 4.0);

    TimelineTime clipT2 = RetimeEngine::getClipTimeAtSourceTime(srcT, 2.0);
    TEST_ASSERT(clipT2.toSeconds() == 2.0);

    // ChangeClipSpeedCommand
    Timeline timeline;
    auto clipId = ClipId::generate();
    Clip clip(clipId, ClipType::Video, "SpeedClip", TimelineTime(0), TimelineTime::fromSeconds(4.0));
    timeline.addClip(timeline.mainTrack().id(), std::move(clip));

    ChangeClipSpeedCommand cmd(timeline, clipId.str(), 2.0, true);
    TEST_ASSERT(cmd.execute());
    Clip* modified = timeline.findClip(clipId);
    TEST_ASSERT(modified != nullptr);
    TEST_ASSERT(modified->duration().toSeconds() == 2.0);

    TEST_ASSERT(cmd.undo());
    TEST_ASSERT(modified->duration().toSeconds() == 4.0);

    std::cout << "[PASS] runRetimeEngineTests" << std::endl;
}

void runSrtSubtitleTests() {
    std::string srtData =
        "1\n"
        "00:00:01,500 --> 00:00:03,500\n"
        "Hello World!\n\n"
        "2\n"
        "00:00:04.000 --> 00:00:06.250\n"
        "Catchim Video Editor\n\n";

    auto res = SrtParser::parse(srtData);
    TEST_ASSERT(res.cues.size() == 2);
    TEST_ASSERT(res.skippedCount == 0);
    TEST_ASSERT(res.cues[0].startTime.ticks() == TimelineTime::fromSeconds(1.5).ticks());
    TEST_ASSERT(res.cues[0].duration.ticks() == TimelineTime::fromSeconds(2.0).ticks());
    TEST_ASSERT(res.cues[0].text == "Hello World!");

    TEST_ASSERT(res.cues[1].startTime.ticks() == TimelineTime::fromSeconds(4.0).ticks());
    TEST_ASSERT(res.cues[1].duration.ticks() == TimelineTime::fromSeconds(2.25).ticks());
    TEST_ASSERT(res.cues[1].text == "Catchim Video Editor");

    // Serialization
    std::string serialized = SrtSerializer::serialize(res.cues);
    TEST_ASSERT(serialized.find("00:00:01,500 --> 00:00:03,500") != std::string::npos);
    TEST_ASSERT(serialized.find("Hello World!") != std::string::npos);

    // ImportSubtitlesCommand
    Timeline timeline;
    ImportSubtitlesCommand importCmd(timeline, res.cues, "Subtitle Track");
    TEST_ASSERT(importCmd.execute());
    TEST_ASSERT(importCmd.createdTrackId().has_value());
    Track* subTrack = timeline.findTrack(*importCmd.createdTrackId());
    TEST_ASSERT(subTrack != nullptr);
    TEST_ASSERT(subTrack->clips().size() == 2);

    TEST_ASSERT(importCmd.undo());
    TEST_ASSERT(timeline.findTrack(*importCmd.createdTrackId()) == nullptr);

    TEST_ASSERT(importCmd.execute());
    TEST_ASSERT(timeline.findTrack(*importCmd.createdTrackId()) != nullptr);

    std::cout << "[PASS] runSrtSubtitleTests" << std::endl;
}

void runShapeAndGradientRendererTests() {
    std::vector<uint8_t> buf(100 * 100 * 4, 0);

    // 1. Solid
    ShapeRenderer::drawSolid(buf.data(), 100, 100, ColorRGBA::fromRgb(255, 0, 0, 255));
    TEST_ASSERT(buf[0] == 255 && buf[1] == 0 && buf[2] == 0 && buf[3] == 255);

    // 2. Linear gradient
    ShapeRenderer::drawLinearGradient(buf.data(), 100, 100, ColorRGBA::fromRgb(0, 0, 0), ColorRGBA::fromRgb(255, 255, 255), 0.0);
    TEST_ASSERT(buf[0] < buf[(50 * 100 + 99) * 4]);

    // 3. Radial gradient
    ShapeRenderer::drawRadialGradient(buf.data(), 100, 100, ColorRGBA::fromRgb(255, 255, 255), ColorRGBA::fromRgb(0, 0, 0), 50);
    int centerIdx = (50 * 100 + 50) * 4;
    TEST_ASSERT(buf[centerIdx] > 200);
    TEST_ASSERT(buf[0] < 50);

    // 4. Rectangle with corner radius
    std::fill(buf.begin(), buf.end(), static_cast<uint8_t>(0));
    ShapeRenderer::drawRectangle(buf.data(), 100, 100, 20, 20, 60, 60, ColorRGBA::fromRgb(0, 255, 0), ColorRGBA::fromRgb(255, 255, 0), 2, 5);
    int rectCenter = (50 * 100 + 50) * 4;
    TEST_ASSERT(buf[rectCenter + 1] == 255);
    TEST_ASSERT(buf[0] == 0);

    // 5. Ellipse
    std::fill(buf.begin(), buf.end(), static_cast<uint8_t>(0));
    ShapeRenderer::drawEllipse(buf.data(), 100, 100, 50, 50, 30, 20, ColorRGBA::fromRgb(0, 0, 255), ColorRGBA::transparent(), 0);
    TEST_ASSERT(buf[centerIdx + 2] == 255);
    TEST_ASSERT(buf[0] == 0);

    std::cout << "[PASS] runShapeAndGradientRendererTests" << std::endl;
}

void runExtendedColorGradingTests() {
    std::vector<uint8_t> pixels = { 100, 150, 200, 255 };

    // 1. Invert
    Compositor::applyInvert(pixels.data(), 1, 1);
    TEST_ASSERT(pixels[0] == 155 && pixels[1] == 105 && pixels[2] == 55);

    Compositor::applyInvert(pixels.data(), 1, 1);
    TEST_ASSERT(pixels[0] == 100 && pixels[1] == 150 && pixels[2] == 200);

    // 2. Sepia
    Compositor::applySepia(pixels.data(), 1, 1, 1.0);
    TEST_ASSERT(pixels[0] > pixels[2]);

    // 3. Extended Color Grading (Exposure + Gamma)
    std::vector<uint8_t> expPix = { 50, 50, 50, 255 };
    Compositor::applyExtendedColorGrading(expPix.data(), 1, 1, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0);
    TEST_ASSERT(expPix[0] > 90);

    std::cout << "[PASS] runExtendedColorGradingTests" << std::endl;
}

void runAudioMasteringTests() {
    // 1. Peak detection
    std::vector<float> samples = { 0.1f, -0.8f, 0.5f, -0.2f };
    float peak = AudioMastering::computePeak(samples.data(), samples.size());
    TEST_ASSERT(std::abs(peak - 0.8f) < 1e-4f);

    // 2. Downmix stereo to mono
    std::vector<float> left = { 1.0f, 0.5f };
    std::vector<float> right = { 0.0f, 0.5f };
    std::vector<float> mono(2, 0.0f);
    AudioMastering::downmixStereo(left.data(), right.data(), mono.data(), 2);
    TEST_ASSERT(std::abs(mono[0] - 0.5f) < 1e-4f);
    TEST_ASSERT(std::abs(mono[1] - 0.5f) < 1e-4f);

    // 3. Panning test (-1.0 full left, +1.0 full right)
    std::vector<float> pLeft = { 1.0f };
    std::vector<float> pRight = { 1.0f };
    AudioMastering::applyPanning(pLeft.data(), pRight.data(), 1, -1.0f);
    TEST_ASSERT(pLeft[0] > 0.99f);
    TEST_ASSERT(pRight[0] < 0.01f);

    // 4. Limiter test (extreme peaks clamped to headroom)
    std::vector<float> loud = { 5.0f, -6.0f, 10.0f };
    LimiterConfig cfg;
    cfg.outputHeadroom = 0.95;
    AudioMastering::applyLimiter(loud.data(), loud.size(), 44100, cfg);
    float limitedPeak = AudioMastering::computePeak(loud.data(), loud.size());
    TEST_ASSERT(limitedPeak <= 1.0f);

    // 5. Peak normalization
    std::vector<float> quiet = { 0.1f, -0.2f, 0.05f };
    AudioMastering::normalizePeak(quiet.data(), quiet.size(), 0.0f); // 0 dB = peak 1.0
    float normPeak = AudioMastering::computePeak(quiet.data(), quiet.size());
    TEST_ASSERT(std::abs(normPeak - 1.0f) < 1e-3f);

    std::cout << "[PASS] runAudioMasteringTests" << std::endl;
}

void runSafeZoneGuideTests() {
    // 1. Margins
    SafeZoneMargin tiktok = SafeZoneGuide::getMargins(GuideType::TikTok);
    TEST_ASSERT(tiktok.topRatio > 0.0 && tiktok.bottomRatio > 0.0);

    // 2. Overlay rendering
    std::vector<uint8_t> buf(100 * 100 * 4, 0);
    SafeZoneGuide::renderGuideOverlay(buf.data(), 100, 100, GuideType::RuleOfThirds, ColorRGBA::fromRgb(255, 0, 0, 255));

    // Pixel at (33, 50) is on the vertical line
    int idx = (50 * 100 + 33) * 4;
    TEST_ASSERT(buf[idx] == 255 && buf[idx + 3] == 255);

    // Crosshair
    std::fill(buf.begin(), buf.end(), static_cast<uint8_t>(0));
    SafeZoneGuide::renderGuideOverlay(buf.data(), 100, 100, GuideType::Crosshair, ColorRGBA::fromRgb(0, 255, 0, 255));
    int midIdx = (50 * 100 + 50) * 4;
    TEST_ASSERT(buf[midIdx + 1] == 255);

    std::cout << "[PASS] runSafeZoneGuideTests" << std::endl;
}

void runTransitionEngineTests() {
    std::vector<uint8_t> frameA(10 * 10 * 4, 0);   // Black
    std::vector<uint8_t> frameB(10 * 10 * 4, 255); // White
    std::vector<uint8_t> dst(10 * 10 * 4, 0);

    // 1. Crossfade at 50% -> mid-gray ~127
    TransitionEngine::blend(dst.data(), frameA.data(), frameB.data(), 10, 10, TransitionType::Crossfade, 0.5);
    TEST_ASSERT(dst[0] >= 126 && dst[0] <= 128);

    // 2. Fade to black at 50% -> pure black 0
    std::fill(frameA.begin(), frameA.end(), static_cast<uint8_t>(200));
    std::fill(frameB.begin(), frameB.end(), static_cast<uint8_t>(200));
    TransitionEngine::blend(dst.data(), frameA.data(), frameB.data(), 10, 10, TransitionType::FadeToBlack, 0.5);
    TEST_ASSERT(dst[0] == 0);

    // 3. Wipe Left at 50% -> left half is frameB (200), right half is frameA (0)
    std::fill(frameA.begin(), frameA.end(), static_cast<uint8_t>(0));
    std::fill(frameB.begin(), frameB.end(), static_cast<uint8_t>(200));
    TransitionEngine::blend(dst.data(), frameA.data(), frameB.data(), 10, 10, TransitionType::WipeLeft, 0.5);
    int leftPixel = (5 * 10 + 2) * 4;  // x = 2
    int rightPixel = (5 * 10 + 8) * 4; // x = 8
    TEST_ASSERT(dst[leftPixel] == 200);
    TEST_ASSERT(dst[rightPixel] == 0);

    std::cout << "[PASS] runTransitionEngineTests" << std::endl;
}

void runRipplePipelineTests() {
    Timeline timeline;
    Track& track = timeline.mainTrack();

    auto c1Id = ClipId::generate();
    auto c2Id = ClipId::generate();
    auto c3Id = ClipId::generate();

    Clip c1(c1Id, ClipType::Video, "C1", TimelineTime(0), TimelineTime::fromSeconds(2.0));
    Clip c2(c2Id, ClipType::Video, "C2", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(2.0));
    Clip c3(c3Id, ClipType::Video, "C3", TimelineTime::fromSeconds(4.0), TimelineTime::fromSeconds(2.0));

    timeline.addClip(track.id(), std::move(c1));
    timeline.addClip(track.id(), std::move(c2));
    timeline.addClip(track.id(), std::move(c3));

    // Insert new 1.0s clip between c1 and c2 using RippleInsert
    auto newId = ClipId::generate();
    Clip newClip(newId, ClipType::Video, "Inserted", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(1.0));
    RippleInsertCommand insertCmd(timeline, track.id(), std::move(newClip));

    TEST_ASSERT(insertCmd.execute());
    Clip* postC2 = timeline.findClip(c2Id);
    Clip* postC3 = timeline.findClip(c3Id);
    TEST_ASSERT(postC2 != nullptr && postC2->startTime().toSeconds() == 3.0);
    TEST_ASSERT(postC3 != nullptr && postC3->startTime().toSeconds() == 5.0);

    // Undo insert
    TEST_ASSERT(insertCmd.undo());
    TEST_ASSERT(timeline.findClip(newId) == nullptr);
    postC2 = timeline.findClip(c2Id);
    postC3 = timeline.findClip(c3Id);
    TEST_ASSERT(postC2 != nullptr && postC2->startTime().toSeconds() == 2.0);
    TEST_ASSERT(postC3 != nullptr && postC3->startTime().toSeconds() == 4.0);

    // Ripple Move
    RippleMoveCommand moveCmd(timeline, c2Id, TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(moveCmd.execute());
    postC2 = timeline.findClip(c2Id);
    postC3 = timeline.findClip(c3Id);
    TEST_ASSERT(postC2 != nullptr && postC2->startTime().toSeconds() == 3.0);
    TEST_ASSERT(postC3 != nullptr && postC3->startTime().toSeconds() == 5.0);

    TEST_ASSERT(moveCmd.undo());
    postC2 = timeline.findClip(c2Id);
    postC3 = timeline.findClip(c3Id);
    TEST_ASSERT(postC2 != nullptr && postC2->startTime().toSeconds() == 2.0);
    TEST_ASSERT(postC3 != nullptr && postC3->startTime().toSeconds() == 4.0);

    std::cout << "[PASS] runRipplePipelineTests" << std::endl;
}

void runProjectBundleTests() {
    Project project("Bundle Test Project");
    Timeline* timeline = project.activeTimeline();
    TEST_ASSERT(timeline != nullptr);
    Track& track = timeline->mainTrack();
    auto clipId = ClipId::generate();
    Clip clip(clipId, ClipType::Video, "MediaClip", TimelineTime(0), TimelineTime::fromSeconds(5.0));
    clip.setMediaId(MediaId("asset-999"));
    timeline->addClip(track.id(), std::move(clip));

    // 1. Validate missing media in empty dir
    auto report = ProjectBundle::validateMediaReferences(project, "non_existent_dir");
    TEST_ASSERT(report.totalMediaReferences == 1);
    TEST_ASSERT(report.missingCount == 1);
    TEST_ASSERT(!report.isComplete());

    // 2. Relink test
    nlohmann::json projJson = {
        {"mediaPath", "C:/old/path/video.mp4"},
        {"assets", nlohmann::json::array({
            nlohmann::json{{"id", "a1"}},
            nlohmann::json{{"url", "C:/old/path/video.mp4"}}
        })}
    };

    std::unordered_map<std::string, std::string> map = {
        {"C:/old/path/video.mp4", "D:/new/path/video.mp4"}
    };

    size_t relinked = ProjectBundle::relinkMedia(projJson, map);
    TEST_ASSERT(relinked == 2);
    TEST_ASSERT(projJson["mediaPath"] == "D:/new/path/video.mp4");
    TEST_ASSERT(projJson["assets"][1]["url"] == "D:/new/path/video.mp4");

    std::cout << "[PASS] runProjectBundleTests" << std::endl;
}

void runWaveformBucketerTests() {
    std::vector<float> samples(1000, 0.0f);
    // Put extreme peaks in specific bucket windows (bucket size 250 -> 4 buckets)
    samples[100] = 0.5f;
    samples[350] = -0.8f;
    samples[600] = 0.95f;
    samples[900] = -0.3f;

    auto peaks = WaveformBucketer::computePeakBuckets(samples.data(), samples.size(), 250);
    TEST_ASSERT(peaks.size() == 4);
    TEST_ASSERT(std::abs(peaks[0] - 0.5f) < 1e-4f);
    TEST_ASSERT(std::abs(peaks[1] - 0.8f) < 1e-4f);
    TEST_ASSERT(std::abs(peaks[2] - 0.95f) < 1e-4f);
    TEST_ASSERT(std::abs(peaks[3] - 0.3f) < 1e-4f);

    auto waveform = WaveformBucketer::generateWaveformData(MediaId("test-audio"), samples.data(), samples.size(), 44100, 250);
    TEST_ASSERT(waveform != nullptr);
    TEST_ASSERT(waveform->buckets.size() == 4);
    TEST_ASSERT(std::abs(waveform->buckets[2].maxPeak - 0.95f) < 1e-4f);
    TEST_ASSERT(std::abs(waveform->buckets[2].minPeak - (-0.95f)) < 1e-4f);

    std::cout << "[PASS] runWaveformBucketerTests" << std::endl;
}

void runHitTestingTests() {
    Rect2D rect{10.0, 10.0, 100.0, 50.0};
    Point2D inside{50.0, 30.0};
    Point2D outside{5.0, 30.0};

    TEST_ASSERT(HitTesting::containsPoint(rect, inside) == true);
    TEST_ASSERT(HitTesting::containsPoint(rect, outside) == false);

    // Hit test Gizmo Handles
    Point2D tl{10.0, 10.0};
    TEST_ASSERT(HitTesting::hitTestGizmoHandle(rect, 0.0, tl) == GizmoHandle::TopLeft);

    Point2D rot{60.0, -14.0}; // 24px above top center
    TEST_ASSERT(HitTesting::hitTestGizmoHandle(rect, 0.0, rot) == GizmoHandle::Rotate);

    // Intersections for Marquee
    Rect2D r1{0.0, 0.0, 50.0, 50.0};
    Rect2D r2{40.0, 40.0, 50.0, 50.0};
    Rect2D r3{100.0, 100.0, 50.0, 50.0};
    TEST_ASSERT(HitTesting::intersects(r1, r2) == true);
    TEST_ASSERT(HitTesting::intersects(r1, r3) == false);

    std::vector<std::pair<std::string, Rect2D>> elems = {
        {"e1", r2},
        {"e2", r3}
    };
    auto hits = HitTesting::resolveElementIntersections(r1, elems);
    TEST_ASSERT(hits.size() == 1 && hits[0] == "e1");

    std::cout << "[PASS] runHitTestingTests" << std::endl;
}

void runExportGeometryResolverTests() {
    // 1. Round to even
    TEST_ASSERT(ExportGeometryResolver::roundToEven(1919.1) == 1920);
    TEST_ASSERT(ExportGeometryResolver::roundToEven(720.0) == 720);
    TEST_ASSERT(ExportGeometryResolver::roundToEven(1.0) == 2);

    // 2. Resolve 1080p source to 720p
    auto dims = ExportGeometryResolver::resolveDimensions(1920, 1080, ExportResolution::Res720p);
    TEST_ASSERT(dims.width == 1280 && dims.height == 720);

    // 3. Resolve vertical 9:16 to 720p height
    auto vDims = ExportGeometryResolver::resolveDimensions(1080, 1920, ExportResolution::Res720p);
    TEST_ASSERT(vDims.height == 720);
    TEST_ASSERT(vDims.width % 2 == 0); // Must be even!

    // 4. Bitrate calculation
    int64_t bitrate = ExportGeometryResolver::calculateVideoBitrate(1920, 1080, 30.0, ExportQuality::High);
    TEST_ASSERT(bitrate > 10'000'000);

    std::cout << "[PASS] runExportGeometryResolverTests" << std::endl;
}

void runProjectDiagnosticsTests() {
    Project project("Diag Project");
    Timeline* timeline = project.activeTimeline();
    Track& track = timeline->mainTrack();

    auto c1Id = ClipId::generate();
    auto c2Id = ClipId::generate();
    Clip c1(c1Id, ClipType::Video, "C1", TimelineTime(0), TimelineTime::fromSeconds(2.0));
    Clip c2(c2Id, ClipType::Video, "C2", TimelineTime::fromSeconds(3.0), TimelineTime::fromSeconds(2.0));
    // Gap of 1.0s between c1 [0, 2s] and c2 [3s, 5s]
    timeline->addClip(track.id(), std::move(c1));
    timeline->addClip(track.id(), std::move(c2));

    auto report = ProjectDiagnostics::analyze(project);
    TEST_ASSERT(report.totalClips == 2);
    TEST_ASSERT(report.gapCount() == 1);
    TEST_ASSERT(report.gaps[0].duration.toSeconds() == 1.0);
    TEST_ASSERT(!report.isClean());

    // Fill gap with clip [2s, 3s]
    auto fillId = ClipId::generate();
    Clip fillClip(fillId, ClipType::Video, "Fill", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(1.0));
    timeline->addClip(track.id(), std::move(fillClip));

    auto report2 = ProjectDiagnostics::analyze(project);
    TEST_ASSERT(report2.totalClips == 3);
    TEST_ASSERT(report2.gapCount() == 0);
    TEST_ASSERT(report2.isClean());

    std::cout << "[PASS] runProjectDiagnosticsTests" << std::endl;
}

void runStillImageExporterTests() {
    // 1. BMP Encoding
    std::vector<uint8_t> rgba = {
        255, 0, 0, 255,    0, 255, 0, 255,
        0, 0, 255, 255,    255, 255, 255, 255
    };
    auto bmp = StillImageExporter::encodeBmp(rgba.data(), 2, 2);
    TEST_ASSERT(bmp.size() == 14 + 40 + 2 * 2 * 4);
    TEST_ASSERT(bmp[0] == 'B' && bmp[1] == 'M');

    // 2. Render Frame RGBA from Project
    Project project("Snapshot Project");
    auto frame = StillImageExporter::renderFrameRgba(project, TimelineTime(0), 100, 50);
    TEST_ASSERT(frame.size() == 100 * 50 * 4);

    std::cout << "[PASS] runStillImageExporterTests" << std::endl;
}

void runRulerAndTimelineViewModelTests() {
    // 1. RulerEngine config
    auto cfg1 = RulerEngine::getRulerConfig(1.0, 30.0);
    TEST_ASSERT(cfg1.labelIntervalSeconds > 0.0);
    TEST_ASSERT(cfg1.tickIntervalSeconds > 0.0);
    TEST_ASSERT(cfg1.labelIntervalSeconds >= cfg1.tickIntervalSeconds);

    // 2. Format labels
    std::string secLabel = RulerEngine::formatRulerLabel(0.0, 30.0);
    TEST_ASSERT(secLabel == "00:00");
    std::string secLabel2 = RulerEngine::formatRulerLabel(65.0, 30.0);
    TEST_ASSERT(secLabel2 == "01:05");
    std::string frameLabel = RulerEngine::formatRulerLabel(0.5, 30.0);
    TEST_ASSERT(frameLabel == "15f");

    // 3. Ruler ticks generation
    auto ticks = RulerEngine::generateRulerTicks(
        TimelineTime::zero(),
        TimelineTime::fromSeconds(2.0),
        1.0,
        30.0
    );
    TEST_ASSERT(!ticks.empty());
    TEST_ASSERT(ticks[0].isMajor);

    // 4. TimelineViewModel
    EditorEngine engine;
    TimelineViewModel vm(engine);
    vm.setViewportWidth(800.0);
    TEST_ASSERT(vm.getViewportWidth() == 800.0);

    // Coordinate conversions
    auto t1 = vm.pixelToTime(100.0);
    double px1 = vm.timeToPixel(t1);
    TEST_ASSERT(std::abs(px1 - 100.0) < 1.0);

    // Playhead auto-scroll
    engine.playback().setDuration(TimelineTime::fromSeconds(60.0));
    engine.playback().seek(TimelineTime::fromSeconds(15.0));
    bool scrolled = vm.ensurePlayheadVisible(50.0);
    TEST_ASSERT(scrolled);
    TEST_ASSERT(vm.getScrollOffsetX() > 0.0);

    // Tracks layout
    auto layouts = vm.getTrackLayouts();
    TEST_ASSERT(!layouts.empty());
    auto trackIdx = vm.getTrackIndexAtY(20.0);
    TEST_ASSERT(trackIdx.has_value() && *trackIdx == 0);

    std::cout << "[PASS] runRulerAndTimelineViewModelTests" << std::endl;
}

void runPreviewSnapAndViewModelTests() {
    // 1. Position Snapping
    Size2D canvasSize{1920.0, 1080.0};
    Size2D elemSize{200.0, 200.0};
    // Close to center (0, 0)
    Point2D nearCenter{3.0, -4.0};
    auto snapRes = PreviewSnap::snapPosition(nearCenter, canvasSize, elemSize, 0.0);
    TEST_ASSERT(snapRes.snappedPosition.x == 0.0);
    TEST_ASSERT(snapRes.snappedPosition.y == 0.0);
    TEST_ASSERT(snapRes.activeLines.size() == 2);

    // 2. Rotation Snapping (88 deg snaps to 90 deg, 45 deg stays 45)
    auto rotSnap1 = PreviewSnap::snapRotation(88.0);
    TEST_ASSERT(rotSnap1.isSnapped && rotSnap1.snappedRotation == 90.0);
    auto rotSnap2 = PreviewSnap::snapRotation(45.0);
    TEST_ASSERT(!rotSnap2.isSnapped && rotSnap2.snappedRotation == 45.0);

    // 3. PreviewViewModel
    EditorEngine engine;
    PreviewViewModel vm(engine);
    vm.setViewportSize(960.0, 540.0);
    vm.zoomToFit();
    TEST_ASSERT(vm.getZoom() > 0.0);

    // Center canvas (0, 0) maps to viewport center
    Point2D centerScreen = vm.canvasToScreen({0.0, 0.0});
    TEST_ASSERT(std::abs(centerScreen.x - 480.0) < 1.0);
    TEST_ASSERT(std::abs(centerScreen.y - 270.0) < 1.0);

    Point2D backToCanvas = vm.screenToCanvas(centerScreen);
    TEST_ASSERT(std::abs(backToCanvas.x) < 0.01);
    TEST_ASSERT(std::abs(backToCanvas.y) < 0.01);

    // Safe zone guide toggle
    vm.setSafeZoneType(GuideType::TikTok);
    TEST_ASSERT(vm.getSafeZoneType().has_value() && *vm.getSafeZoneType() == GuideType::TikTok);

    std::cout << "[PASS] runPreviewSnapAndViewModelTests" << std::endl;
}

void runAudioPlaybackEngineTests() {
    // 1. dB to linear and linear to dB
    TEST_ASSERT(std::abs(AudioPlaybackEngine::dBToLinear(0.0) - 1.0) < 1e-4);
    TEST_ASSERT(std::abs(AudioPlaybackEngine::dBToLinear(-20.0) - 0.1) < 1e-4);
    TEST_ASSERT(std::abs(AudioPlaybackEngine::linearToDb(1.0) - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(AudioPlaybackEngine::linearToDb(0.1) - (-20.0)) < 1e-2);

    // 2. Resolve gain & waveform samples
    Clip clip(ClipId::generate(), ClipType::Audio, "TestAudio", TimelineTime(0), TimelineTime::fromSeconds(2.0));
    clip.setParam("volume", 0.0); // 0 dB
    double gain = AudioPlaybackEngine::resolveEffectiveAudioGain(clip, false, TimelineTime(0));
    TEST_ASSERT(std::abs(gain - 1.0) < 1e-4);

    auto waveformGains = AudioPlaybackEngine::buildWaveformGainSamples(clip, 10);
    TEST_ASSERT(waveformGains.size() == 10);
    TEST_ASSERT(std::abs(waveformGains[0] - 1.0f) < 1e-4f);

    // 3. Audio slice rendering
    Timeline timeline;
    Track& audioTrack = timeline.addTrack(TrackType::Audio, "Audio 1");
    timeline.addClip(audioTrack.id(), std::move(clip));

    AudioPlaybackEngine engine;
    auto slice = engine.renderAudioSlice(timeline, TimelineTime(0), TimelineTime::fromSeconds(0.5), 48000);
    TEST_ASSERT(slice.channels() == 2);
    TEST_ASSERT(slice.frameCount() == 24000);
    TEST_ASSERT(slice.samples().size() == 48000);

    // Check that samples are non-silent
    float peak = AudioMastering::computePeak(slice.samples().data(), slice.samples().size());
    TEST_ASSERT(peak > 0.0f && peak <= 1.0f);

    std::cout << "[PASS] runAudioPlaybackEngineTests" << std::endl;
}

void runVideoFrameCacheTests() {
    VideoFrameCache cache(5, 1024 * 1024); // max 5 frames, 1MB
    auto mediaId = MediaId::generate();

    std::vector<uint8_t> dummyPixels(10 * 10 * 4, 128);

    // Store frame at t = 1.0s
    cache.storeFrame(mediaId, TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(0.033), 10, 10, dummyPixels);
    TEST_ASSERT(cache.frameCount() == 1);

    // Retrieve frame at t = 1.01s (within tolerance)
    auto f = cache.getFrameAt(mediaId, TimelineTime::fromSeconds(1.01));
    TEST_ASSERT(f != nullptr);
    TEST_ASSERT(f->width == 10 && f->height == 10);
    TEST_ASSERT(f->rgbaPixels[0] == 128);

    // Seek generation test: increment generation
    uint64_t gen = cache.incrementGeneration();
    TEST_ASSERT(gen > 0);

    // Frame stored with stale generation (gen - 1) should be ignored
    cache.storeFrame(mediaId, TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(0.033), 10, 10, dummyPixels, gen - 1);
    auto fStale = cache.getFrameAt(mediaId, TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(fStale == nullptr);

    // Eviction test: fill past maxFrames
    for (int i = 0; i < 10; ++i) {
        cache.storeFrame(mediaId, TimelineTime::fromSeconds(10.0 + i), TimelineTime::fromSeconds(0.033), 10, 10, dummyPixels, gen);
    }
    TEST_ASSERT(cache.frameCount() <= 5);

    // Clear
    cache.clear();
    TEST_ASSERT(cache.frameCount() == 0);
    TEST_ASSERT(cache.memoryUsageBytes() == 0);

    std::cout << "[PASS] runVideoFrameCacheTests" << std::endl;
}

void runPropertiesViewModelTests() {
    EditorEngine engine;
    PropertiesViewModel vm(engine);

    // 1. Empty selection state
    TEST_ASSERT(vm.getSelectionState() == PropertySelectionState::Empty);
    TEST_ASSERT(vm.getSelectedCount() == 0);
    TEST_ASSERT(vm.getSelectedClip() == nullptr);

    // Add a clip and select it
    auto* tl = engine.activeTimeline();
    TEST_ASSERT(tl != nullptr);
    auto& track = tl->mainTrack();
    auto clipId = ClipId::generate();
    Clip clip(clipId, ClipType::Video, "MyVideoClip", TimelineTime(0), TimelineTime::fromSeconds(5.0));
    engine.addClip(track.id(), std::move(clip));
    engine.selectClip(clipId);

    // 2. Single selection state
    TEST_ASSERT(vm.getSelectionState() == PropertySelectionState::Single);
    TEST_ASSERT(vm.getSelectedCount() == 1);
    TEST_ASSERT(vm.getSelectedClip() != nullptr);

    // Available tabs for Video
    auto tabs = vm.getAvailableTabs();
    TEST_ASSERT(tabs.size() >= 4);
    TEST_ASSERT(tabs[0].id == "transform");

    // Active tab switching
    vm.setActiveTab("speed");
    TEST_ASSERT(vm.getActiveTab() == "speed");

    // Property mutations
    vm.setPositionX(50.0);
    TEST_ASSERT(vm.getPositionX() == 50.0);

    vm.setRotation(45.0);
    TEST_ASSERT(vm.getRotation() == 45.0);

    vm.setOpacity(0.8);
    TEST_ASSERT(std::abs(vm.getOpacity() - 0.8) < 1e-4);

    vm.setSpeed(2.0);
    TEST_ASSERT(vm.getSpeed() == 2.0);

    vm.setVolumeDb(-6.0);
    TEST_ASSERT(std::abs(vm.getVolumeDb() - (-6.0)) < 1e-4);

    // 3. Multiple selection state
    auto clipId2 = ClipId::generate();
    Clip clip2(clipId2, ClipType::Video, "SecondVideo", TimelineTime::fromSeconds(5.0), TimelineTime::fromSeconds(3.0));
    engine.addClip(track.id(), std::move(clip2));
    engine.selectClip(clipId, false);
    engine.selectClip(clipId2, true); // additive
    TEST_ASSERT(vm.getSelectionState() == PropertySelectionState::Multiple);
    TEST_ASSERT(vm.getSelectedCount() == 2);

    std::cout << "[PASS] runPropertiesViewModelTests" << std::endl;
}

void runTranscriptionAndCaptionTests() {
    using namespace catchim::subtitles;

    // 1. Test parseWhisperJson
    std::string sampleJson = R"({
        "text": "Xin chào thế giới đây là video trình diễn.",
        "segments": [
            {
                "id": 0,
                "seek": 0,
                "start": 0.0,
                "end": 2.5,
                "text": "Xin chào thế giới"
            },
            {
                "id": 1,
                "seek": 250,
                "start": 2.5,
                "end": 5.0,
                "text": "đây là video trình diễn"
            }
        ]
    })";

    auto segments = TranscriptionEngine::parseWhisperJson(sampleJson);
    TEST_ASSERT(segments.size() == 2);
    TEST_ASSERT(segments[0].text == "Xin chào thế giới");
    TEST_ASSERT(segments[0].start == 0.0);
    TEST_ASSERT(segments[0].end == 2.5);
    TEST_ASSERT(segments[1].text == "đây là video trình diễn");
    TEST_ASSERT(segments[1].start == 2.5);
    TEST_ASSERT(segments[1].end == 5.0);

    // 2. Test buildCaptionChunks with 3 words per chunk
    auto chunks = TranscriptionEngine::buildCaptionChunks(segments, 3, 0.8);
    TEST_ASSERT(chunks.size() == 4);
    TEST_ASSERT(chunks[0].text == "Xin chào thế");
    TEST_ASSERT(chunks[1].text == "giới");
    TEST_ASSERT(chunks[2].text == "đây là video");
    TEST_ASSERT(chunks[3].text == "trình diễn");

    // Start time and durations should be progressive and within segment boundaries
    TEST_ASSERT(chunks[0].startTime >= 0.0);
    TEST_ASSERT(chunks[0].duration >= 0.8);
    TEST_ASSERT(chunks[1].startTime >= chunks[0].startTime);
    TEST_ASSERT(chunks[2].startTime >= 2.5);

    // 3. Test empty segments
    auto emptyChunks = TranscriptionEngine::buildCaptionChunks({});
    TEST_ASSERT(emptyChunks.empty());

    // 4. Test invalid JSON fallback
    auto badSegments = TranscriptionEngine::parseWhisperJson("{bad json");
    TEST_ASSERT(badSegments.empty());

    std::cout << "[PASS] runTranscriptionAndCaptionTests" << std::endl;
}

void runStickerAndGraphicRegistryTests() {
    using namespace catchim::media;
    using namespace catchim::core;
    using namespace catchim::editor;

    auto& reg = StickerRegistry::instance();
    TEST_ASSERT(reg.totalCount() >= 15);

    // Check categories
    auto categories = reg.getCategories();
    TEST_ASSERT(!categories.empty());
    bool hasArrows = false, hasEmojis = false, hasBadges = false;
    for (const auto& cat : categories) {
        if (cat == "arrows") hasArrows = true;
        if (cat == "emojis") hasEmojis = true;
        if (cat == "badges") hasBadges = true;
    }
    TEST_ASSERT(hasArrows && hasEmojis && hasBadges);

    // Check sticker retrieval by category
    auto arrows = reg.getStickersByCategory("arrows");
    TEST_ASSERT(arrows.size() >= 3);

    // Check findSticker
    const auto* fire = reg.findSticker("emoji-fire");
    TEST_ASSERT(fire != nullptr);
    TEST_ASSERT(fire->category == "emojis");
    TEST_ASSERT(fire->intrinsicWidth == 256);
    TEST_ASSERT(fire->intrinsicHeight == 256);
    TEST_ASSERT(std::abs(fire->aspectRatio() - 1.0) < 1e-4);

    const auto* missing = reg.findSticker("non-existent-sticker");
    TEST_ASSERT(missing == nullptr);

    // Create sticker clip
    auto clip = reg.createStickerClip("badge-verified", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(3.5));
    TEST_ASSERT(clip.type() == ClipType::Graphic);
    TEST_ASSERT(clip.name() == "Tích xanh");
    TEST_ASSERT(clip.startTime() == TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(clip.duration() == TimelineTime::fromSeconds(3.5));
    TEST_ASSERT(clip.getParam<std::string>("stickerId", "") == "badge-verified");
    TEST_ASSERT(clip.getParam<std::string>("category", "") == "badges");
    TEST_ASSERT(clip.getParam<int>("intrinsicWidth", 0) == 256);

    std::cout << "[PASS] runStickerAndGraphicRegistryTests" << std::endl;
}

void runSpatialMotionPathTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    SpatialMotionPath path;
    TEST_ASSERT(path.pointCount() == 0);
    TEST_ASSERT(path.totalLength() == 0.0);

    // Evaluate empty
    auto emptyPose = path.evaluateAt(TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(emptyPose.x == 0.0 && emptyPose.y == 0.0);

    // Add 4 points forming an L-curve
    path.addPoint(TimelineTime::fromSeconds(0.0), 0.0, 0.0);
    path.addPoint(TimelineTime::fromSeconds(1.0), 100.0, 0.0);
    path.addPoint(TimelineTime::fromSeconds(2.0), 100.0, 100.0);
    path.addPoint(TimelineTime::fromSeconds(3.0), 200.0, 100.0);

    TEST_ASSERT(path.pointCount() == 4);
    TEST_ASSERT(path.totalLength() > 250.0);

    // Evaluate at boundary times
    auto p0 = path.evaluateAt(TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(std::abs(p0.x - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(p0.y - 0.0) < 1e-4);

    auto p1 = path.evaluateAt(TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(std::abs(p1.x - 100.0) < 1e-4);
    TEST_ASSERT(std::abs(p1.y - 0.0) < 1e-4);

    auto p3 = path.evaluateAt(TimelineTime::fromSeconds(3.0));
    TEST_ASSERT(std::abs(p3.x - 200.0) < 1e-4);
    TEST_ASSERT(std::abs(p3.y - 100.0) < 1e-4);

    // Evaluate in-between (t = 0.5s)
    auto mid = path.evaluateAt(TimelineTime::fromSeconds(0.5));
    TEST_ASSERT(mid.x > 30.0 && mid.x < 70.0);
    TEST_ASSERT(std::abs(mid.headingDegrees) < 25.0);

    // Clear
    path.clear();
    TEST_ASSERT(path.pointCount() == 0);

    std::cout << "[PASS] runSpatialMotionPathTests" << std::endl;
}

void runColorWheelGradingTests() {
    using namespace catchim::render;

    ColorWheelGrade grade;
    TEST_ASSERT(grade.isIdentity());

    // 4 RGBA pixels: Black, Mid-gray, Bright, White
    uint8_t pixels[16] = {
        0, 0, 0, 255,          // Pixel 0: Black
        128, 128, 128, 255,    // Pixel 1: Mid gray
        200, 100, 50, 255,     // Pixel 2: Color
        255, 255, 255, 255     // Pixel 3: White
    };

    uint8_t copy[16];
    std::memcpy(copy, pixels, sizeof(pixels));

    // Identity should not alter pixels
    ColorWheelEngine::applyColorWheelGrading(copy, 4, grade);
    for (size_t i = 0; i < 16; ++i) {
        TEST_ASSERT(copy[i] == pixels[i]);
    }

    // Apply Lift to Red shadow (lift.r = 0.2)
    grade.lift.r = 0.2;
    TEST_ASSERT(!grade.isIdentity());
    ColorWheelEngine::applyColorWheelGrading(copy, 4, grade);
    // Black pixel (index 0) R channel should now be raised
    TEST_ASSERT(copy[0] > 0);
    // Alpha channel (index 3, 7, 11, 15) must remain 255
    TEST_ASSERT(copy[3] == 255 && copy[7] == 255 && copy[11] == 255 && copy[15] == 255);

    // Apply Gain (highlights boost)
    ColorWheelGrade gainGrade;
    gainGrade.gain.g = 1.5;
    std::memcpy(copy, pixels, sizeof(pixels));
    ColorWheelEngine::applyColorWheelGrading(copy, 4, gainGrade);
    // Mid gray Green should be boosted
    TEST_ASSERT(copy[5] > 128);

    std::cout << "[PASS] runColorWheelGradingTests" << std::endl;
}

void runAudioDuckingTests() {
    using namespace catchim::audio;

    constexpr size_t SAMPLE_RATE = 48000;
    constexpr size_t TOTAL_SAMPLES = SAMPLE_RATE; // 1 second

    AudioBuffer music(2, static_cast<int32_t>(SAMPLE_RATE));
    music.resize(TOTAL_SAMPLES);
    AudioBuffer voice(1, static_cast<int32_t>(SAMPLE_RATE));
    voice.resize(TOTAL_SAMPLES);

    // Fill music with steady 0.5 amplitude
    for (size_t i = 0; i < TOTAL_SAMPLES; ++i) {
        music.samples()[i * 2 + 0] = 0.5f;
        music.samples()[i * 2 + 1] = 0.5f;
    }

    // Voice:
    // First 0.3s silence (0.0)
    // 0.3s to 0.8s loud speech at 0.8 amplitude (> -24dB)
    // 0.8s to 1.0s silence
    size_t sStart = SAMPLE_RATE * 3 / 10;
    size_t sEnd = SAMPLE_RATE * 8 / 10;
    for (size_t i = sStart; i < sEnd; ++i) {
        voice.samples()[i] = 0.8f;
    }

    DuckingConfig cfg;
    cfg.thresholdDb = -24.0;
    cfg.duckingDb = -12.0;
    cfg.attackTimeSec = 0.02;
    cfg.holdTimeSec = 0.05;
    cfg.releaseTimeSec = 0.05;

    AudioDucker::applyDucking(music, voice, cfg);

    // In the quiet section (e.g. sample 1000 = ~0.02s), music should still be 0.5
    float quietSample = music.samples()[1000 * 2];
    TEST_ASSERT(std::abs(quietSample - 0.5f) < 0.01f);

    // In the loud speech section (e.g. sample 25000 = ~0.52s), music should be ducked significantly
    float duckedSample = music.samples()[25000 * 2];
    TEST_ASSERT(duckedSample < 0.25f);
    TEST_ASSERT(duckedSample > 0.05f);

    std::cout << "[PASS] runAudioDuckingTests" << std::endl;
}

void runPresetManagerTests() {
    using namespace catchim::media;
    using namespace catchim::core;
    using namespace catchim::editor;

    auto& pm = PresetManager::instance();

    // 1. Aspect Ratio Presets
    const auto& ratios = pm.aspectRatios();
    TEST_ASSERT(ratios.size() >= 5);

    const auto* ar169 = pm.findAspectRatio("16:9");
    TEST_ASSERT(ar169 != nullptr);
    TEST_ASSERT(ar169->width == 1920);
    TEST_ASSERT(ar169->height == 1080);
    TEST_ASSERT(std::abs(ar169->ratio() - (16.0 / 9.0)) < 1e-4);

    const auto* ar916 = pm.findAspectRatio("9:16");
    TEST_ASSERT(ar916 != nullptr);
    TEST_ASSERT(std::abs(ar916->ratio() - (9.0 / 16.0)) < 1e-4);

    const auto* arNull = pm.findAspectRatio("unknown");
    TEST_ASSERT(arNull == nullptr);

    // 2. Text Presets
    const auto& texts = pm.textPresets();
    TEST_ASSERT(texts.size() >= 4);

    const auto* lowerThird = pm.findTextPreset("text-lowerthird");
    TEST_ASSERT(lowerThird != nullptr);
    TEST_ASSERT(lowerThird->fontSize == 28.0);
    TEST_ASSERT(lowerThird->posX == -300.0);
    TEST_ASSERT(lowerThird->posY == 250.0);

    // Create Text Clip from Preset
    auto textClip = pm.createTextClipFromPreset("text-boldcenter", "PHÁT TRỰC TIẾP", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(textClip.type() == ClipType::Text);
    TEST_ASSERT(textClip.name() == "Tiêu đề lớn");
    TEST_ASSERT(textClip.startTime() == TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(textClip.duration() == TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(textClip.getParam<std::string>("text", "") == "PHÁT TRỰC TIẾP");
    TEST_ASSERT(textClip.getParam<bool>("bold", false) == true);
    TEST_ASSERT(textClip.getParam<double>("fontSize", 0.0) == 64.0);

    // 3. Transition Presets
    const auto& transitions = pm.transitionPresets();
    TEST_ASSERT(transitions.size() >= 4);
    const auto* crossfade = pm.findTransitionPreset("trans-crossfade");
    TEST_ASSERT(crossfade != nullptr);
    TEST_ASSERT(crossfade->transitionType == "crossfade");
    TEST_ASSERT(crossfade->defaultDuration == TimelineTime::fromSeconds(1.0));

    // 4. Audio Master Presets
    const auto& audioPresets = pm.audioPresets();
    TEST_ASSERT(audioPresets.size() >= 3);
    const auto* podcast = pm.findAudioPreset("audio-podcast");
    TEST_ASSERT(podcast != nullptr);
    TEST_ASSERT(podcast->targetPeakDb == -1.5);
    TEST_ASSERT(podcast->limiterHeadroom == 0.90);

    std::cout << "[PASS] runPresetManagerTests" << std::endl;
}

void runEndToEndPipelineIntegrationTests() {
    using namespace catchim::core;
    using namespace catchim::editor;
    using namespace catchim::media;
    using namespace catchim::render;
    using namespace catchim::audio;
    using namespace catchim::exporting;
    using namespace catchim::subtitles;

    // 1. Initialize Engine & New Project
    EditorEngine engine;
    engine.newProject("Epic Production 2026");
    engine.project().settings().canvasSize = {1920, 1080};
    engine.project().settings().fps = FrameRate{30, 1};
    auto* timeline = engine.activeTimeline();
    TEST_ASSERT(timeline != nullptr);

    // 2. Set up Tracks
    auto videoTrackId = timeline->mainTrack().id();
    auto graphicTrack = timeline->addTrack(TrackType::Graphic, "Stickers Layer");
    auto textTrack = timeline->addTrack(TrackType::Text, "Titles & Subtitles");
    timeline->addTrack(TrackType::Audio, "Background Music");
    timeline->addTrack(TrackType::Audio, "Voiceover Track");

    TEST_ASSERT(timeline->allTracks().size() == 5);

    // 3. Add Video Background Clip (0s to 12s)
    auto bgVideoId = ClipId::generate();
    Clip bgClip(bgVideoId, ClipType::Video, "Hero_4K.mp4", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(12.0));
    engine.addClip(videoTrackId, std::move(bgClip));

    // 4. Add Graphic Sticker with Motion Path (t=2s to 7s)
    auto stickerClip = StickerRegistry::instance().createStickerClip("emoji-fire", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(5.0));
    engine.addClip(graphicTrack.id(), std::move(stickerClip));

    // Animate Sticker using SpatialMotionPath
    SpatialMotionPath motionPath;
    motionPath.addPoint(TimelineTime::fromSeconds(2.0), -200.0, 0.0);
    motionPath.addPoint(TimelineTime::fromSeconds(4.5), 0.0, -150.0);
    motionPath.addPoint(TimelineTime::fromSeconds(7.0), 200.0, 0.0);
    TEST_ASSERT(motionPath.totalLength() > 300.0);

    // Evaluate pose at t=4.5s
    auto midPose = motionPath.evaluateAt(TimelineTime::fromSeconds(4.5));
    TEST_ASSERT(std::abs(midPose.x - 0.0) < 1e-2);
    TEST_ASSERT(std::abs(midPose.y - (-150.0)) < 1e-2);

    // 5. Add Text Title using Preset (t=1s to 6s)
    auto titleClip = PresetManager::instance().createTextClipFromPreset("text-boldcenter", "CATCHIM 2026", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(5.0));
    engine.addClip(textTrack.id(), std::move(titleClip));

    // 6. Add Whisper Auto-Captions (t=7s to 11s)
    std::string whisperOutput = R"({
        "segments": [
            { "id": 0, "start": 7.0, "end": 9.0, "text": "Trải nghiệm video tuyệt đỉnh" },
            { "id": 1, "start": 9.0, "end": 11.0, "text": "với công nghệ C++ native" }
        ]
    })";
    auto segs = TranscriptionEngine::parseWhisperJson(whisperOutput);
    auto captionChunks = TranscriptionEngine::buildCaptionChunks(segs, 4, 0.8);
    for (const auto& chunk : captionChunks) {
        auto captionClip = Clip(ClipId::generate(), ClipType::Text, chunk.text, TimelineTime::fromSeconds(chunk.startTime), TimelineTime::fromSeconds(chunk.duration));
        captionClip.setParam("text", chunk.text);
        captionClip.setParam("fontSize", 36.0);
        captionClip.setParam("fontColor", std::string("#FFFFFF"));
        engine.addClip(textTrack.id(), std::move(captionClip));
    }

    // 7. Add Audio Tracks & Apply Intelligent Ducking
    constexpr size_t SR = 48000;
    AudioBuffer musicBuffer(2, static_cast<int32_t>(SR));
    musicBuffer.resize(SR * 5); // 5 seconds
    AudioBuffer voiceBuffer(1, static_cast<int32_t>(SR));
    voiceBuffer.resize(SR * 5);

    for (size_t i = 0; i < SR * 5; ++i) {
        musicBuffer.samples()[i * 2 + 0] = 0.6f;
        musicBuffer.samples()[i * 2 + 1] = 0.6f;
    }
    // Voice speech at t=2s to 4s
    for (size_t i = SR * 2; i < SR * 4; ++i) {
        voiceBuffer.samples()[i] = 0.85f;
    }
    DuckingConfig duckCfg;
    AudioDucker::applyDucking(musicBuffer, voiceBuffer, duckCfg);
    // Verified ducked gain in voice interval
    TEST_ASSERT(musicBuffer.samples()[(SR * 3) * 2] < 0.35f);

    // 8. 3-Way Color Wheels Grading
    ColorWheelGrade grade;
    grade.lift.r = 0.1;
    grade.gain.master = 1.2;
    uint8_t testFrame[16] = { 10, 10, 10, 255, 120, 120, 120, 255, 200, 200, 200, 255, 255, 255, 255, 255 };
    ColorWheelEngine::applyColorWheelGrading(testFrame, 4, grade);
    TEST_ASSERT(testFrame[0] > 10); // Lift raised dark red
    TEST_ASSERT(testFrame[3] == 255); // Alpha preserved

    // 9. Project Diagnostics
    auto diag = ProjectDiagnostics::analyze(engine.project());
    TEST_ASSERT(diag.totalTracks == 5);
    TEST_ASSERT(diag.totalClips >= 4);
    TEST_ASSERT(diag.totalDuration.toSeconds() >= 11.0);

    // 10. Project Serialization & Deserialization
    auto jsonV31 = ProjectSerializer::serialize(engine.project());
    TEST_ASSERT(!jsonV31.empty());
    auto loadedRes = ProjectSerializer::deserialize(jsonV31);
    TEST_ASSERT(loadedRes.ok());
    auto loadedProject = loadedRes.unwrap();
    TEST_ASSERT(loadedProject.name() == "Epic Production 2026");
    TEST_ASSERT(loadedProject.activeTimeline()->allTracks().size() == 5);

    // 11. Render Still Frame RGBA
    auto frameRgba = StillImageExporter::renderFrameRgba(engine.project(), TimelineTime::fromSeconds(3.0), 320, 180);
    TEST_ASSERT(frameRgba.size() == 320 * 180 * 4);
    TEST_ASSERT(frameRgba[3] == 255);

    std::cout << "[PASS] runEndToEndPipelineIntegrationTests" << std::endl;
}

void runGroupMoveEngineTests() {
    using namespace catchim::core;
    using namespace catchim::editor;

    Timeline timeline;
    auto videoTrackId = timeline.mainTrack().id();
    auto textTrack = timeline.addTrack(TrackType::Text, "Text Track");

    // Clip A on Video Track: t=3.0s, dur=2.0s
    auto clipAId = ClipId::generate();
    Clip clipA(clipAId, ClipType::Video, "ClipA", TimelineTime::fromSeconds(3.0), TimelineTime::fromSeconds(2.0));
    timeline.addClip(videoTrackId, std::move(clipA));

    // Clip B on Video Track: t=6.0s, dur=2.0s (offset +3.0s relative to A)
    auto clipBId = ClipId::generate();
    Clip clipB(clipBId, ClipType::Video, "ClipB", TimelineTime::fromSeconds(6.0), TimelineTime::fromSeconds(2.0));
    timeline.addClip(videoTrackId, std::move(clipB));

    // Clip C on Text Track: t=1.0s, dur=2.0s (offset -2.0s relative to A)
    auto clipCId = ClipId::generate();
    Clip clipC(clipCId, ClipType::Text, "ClipC", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(2.0));
    timeline.addClip(textTrack.id(), std::move(clipC));

    // 1. Build Move Group with A as Anchor
    std::vector<std::pair<TrackId, ClipId>> selection = {
        {videoTrackId, clipAId},
        {videoTrackId, clipBId},
        {textTrack.id(), clipCId}
    };
    auto groupOpt = GroupMoveEngine::buildMoveGroup(timeline, videoTrackId, clipAId, selection);
    TEST_ASSERT(groupOpt.has_value());
    const auto& group = groupOpt.value();
    TEST_ASSERT(group.members.size() == 3);
    TEST_ASSERT(group.anchor.clipId == clipAId);

    // Verify offsets
    for (const auto& m : group.members) {
        if (m.clipId == clipAId) TEST_ASSERT(m.timeOffset == TimelineTime::fromSeconds(0.0));
        if (m.clipId == clipBId) TEST_ASSERT(m.timeOffset == TimelineTime::fromSeconds(3.0));
        if (m.clipId == clipCId) TEST_ASSERT(m.timeOffset == TimelineTime::fromSeconds(-2.0));
    }

    // 2. Resolve Group Move to t=5.0s (Valid shift forward)
    auto planValid = GroupMoveEngine::resolveGroupMove(group, TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(planValid.clampedAnchorTime == TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(planValid.moves.size() == 3);
    for (const auto& mv : planValid.moves) {
        if (mv.clipId == clipAId) TEST_ASSERT(mv.newStartTime == TimelineTime::fromSeconds(5.0));
        if (mv.clipId == clipBId) TEST_ASSERT(mv.newStartTime == TimelineTime::fromSeconds(8.0));
        if (mv.clipId == clipCId) TEST_ASSERT(mv.newStartTime == TimelineTime::fromSeconds(3.0));
    }

    // 3. Resolve Group Move with clamping (Anchor attempted at t=1.0s -> C would be 1 - 2 = -1s < 0)
    auto planClamped = GroupMoveEngine::resolveGroupMove(group, TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(planClamped.clampedAnchorTime == TimelineTime::fromSeconds(2.0)); // Clamped so C starts at 0.0s!
    for (const auto& mv : planClamped.moves) {
        if (mv.clipId == clipCId) TEST_ASSERT(mv.newStartTime == TimelineTime::fromSeconds(0.0));
    }

    // 4. Command Execution & Undo/Redo
    GroupMoveCommand cmd(timeline, planValid);
    bool execOk = cmd.execute();
    TEST_ASSERT(execOk);
    TEST_ASSERT(timeline.findClip(clipAId)->startTime() == TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(timeline.findClip(clipBId)->startTime() == TimelineTime::fromSeconds(8.0));
    TEST_ASSERT(timeline.findClip(clipCId)->startTime() == TimelineTime::fromSeconds(3.0));

    // Undo
    bool undoOk = cmd.undo();
    TEST_ASSERT(undoOk);
    TEST_ASSERT(timeline.findClip(clipAId)->startTime() == TimelineTime::fromSeconds(3.0));
    TEST_ASSERT(timeline.findClip(clipBId)->startTime() == TimelineTime::fromSeconds(6.0));
    TEST_ASSERT(timeline.findClip(clipCId)->startTime() == TimelineTime::fromSeconds(1.0));

    std::cout << "[PASS] runGroupMoveEngineTests" << std::endl;
}

void runTtsEngineAndWavWriterTests() {
    using namespace catchim::audio;

    auto& tts = TtsEngine::instance();
    const auto& voices = tts.voices();
    TEST_ASSERT(voices.size() >= 6);

    // 1. Vietnamese voice queries
    auto viVoices = tts.findVoicesByLanguage("vi");
    TEST_ASSERT(viVoices.size() >= 3);
    const auto* hoaiMy = tts.findVoice("vi-female-sweet");
    TEST_ASSERT(hoaiMy != nullptr);
    TEST_ASSERT(hoaiMy->gender == "female");
    TEST_ASSERT(hoaiMy->language == "vi-VN");

    // 2. Duration estimation
    double durNormal = TtsEngine::estimateSpeechDuration("Đây là bài thuyết trình giới thiệu video", 1.0);
    TEST_ASSERT(durNormal > 1.0 && durNormal < 5.0);
    double durFast = TtsEngine::estimateSpeechDuration("Đây là bài thuyết trình giới thiệu video", 2.0);
    TEST_ASSERT(durFast < durNormal);

    // 3. 16-bit PCM WAV binary encoder
    constexpr int SR = 48000;
    constexpr int CH = 2;
    AudioBuffer buf(CH, SR);
    buf.resize(SR); // 1 second
    for (size_t i = 0; i < static_cast<size_t>(SR); ++i) {
        buf.samples()[i * 2 + 0] = 0.5f;
        buf.samples()[i * 2 + 1] = -0.5f;
    }

    auto wavBytes = TtsEngine::encodePcm16Wav(buf);
    size_t expectedSize = 44 + (SR * CH * 2); // 44 + 192000 = 192044
    TEST_ASSERT(wavBytes.size() == expectedSize);

    // Verify RIFF / WAVE headers
    TEST_ASSERT(std::memcmp(&wavBytes[0], "RIFF", 4) == 0);
    TEST_ASSERT(std::memcmp(&wavBytes[8], "WAVE", 4) == 0);
    TEST_ASSERT(std::memcmp(&wavBytes[12], "fmt ", 4) == 0);
    TEST_ASSERT(std::memcmp(&wavBytes[36], "data", 4) == 0);

    // Verify sample rate at byte 24
    uint32_t parsedRate = static_cast<uint32_t>(wavBytes[24]) |
                          (static_cast<uint32_t>(wavBytes[25]) << 8) |
                          (static_cast<uint32_t>(wavBytes[26]) << 16) |
                          (static_cast<uint32_t>(wavBytes[27]) << 24);
    TEST_ASSERT(parsedRate == 48000);

    std::cout << "[PASS] runTtsEngineAndWavWriterTests" << std::endl;
}

void runSoundEffectsRegistryTests() {
    using namespace catchim::media;
    using namespace catchim::core;
    using namespace catchim::editor;

    auto& reg = SoundEffectsRegistry::instance();
    TEST_ASSERT(reg.totalCount() >= 10);

    // 1. Categories
    auto categories = reg.getCategories();
    TEST_ASSERT(categories.size() >= 4);

    // 2. Find specific sound
    const auto* whoosh = reg.findEffect("whoosh-fast");
    TEST_ASSERT(whoosh != nullptr);
    TEST_ASSERT(whoosh->category == "whoosh");
    TEST_ASSERT(whoosh->sampleRate == 48000);
    TEST_ASSERT(whoosh->channels == 2);
    TEST_ASSERT(whoosh->duration == TimelineTime::fromSeconds(0.5));

    // 3. Search
    auto searchResults = reg.searchEffects("pop");
    TEST_ASSERT(!searchResults.empty());
    TEST_ASSERT(searchResults[0].id == "ui-pop");

    // 4. Create SFX Clip
    auto sfxClip = reg.createSfxClip("ui-click", TimelineTime::fromSeconds(3.5));
    TEST_ASSERT(sfxClip.type() == ClipType::Audio);
    TEST_ASSERT(sfxClip.name() == "Click chuột nhẹ (Soft Click)");
    TEST_ASSERT(sfxClip.startTime() == TimelineTime::fromSeconds(3.5));
    TEST_ASSERT(sfxClip.duration() == TimelineTime::fromSeconds(0.15));
    TEST_ASSERT(sfxClip.getParam<std::string>("sfxId", "") == "ui-click");
    TEST_ASSERT(sfxClip.getParam<std::string>("category", "") == "ui");

    std::cout << "[PASS] runSoundEffectsRegistryTests" << std::endl;
}

void runFontRegistryTests() {
    using namespace catchim::render;

    auto& fonts = FontRegistry::instance();
    const auto& allFonts = fonts.availableFonts();
    TEST_ASSERT(allFonts.size() >= 7);

    // 1. Find font
    const auto* inter = fonts.findFont("inter");
    TEST_ASSERT(inter != nullptr);
    TEST_ASSERT(inter->displayName == "Inter");
    TEST_ASSERT(inter->category == FontCategory::SansSerif);
    TEST_ASSERT(inter->isSystemFont == true);
    TEST_ASSERT(!inter->supportedWeights.empty());

    // 2. Case-insensitive lookup
    const auto* roboto = fonts.findFont("RoBoTo");
    TEST_ASSERT(roboto != nullptr);

    // 3. Fallback resolution
    std::string matchedFamily = fonts.resolveFontFamily("montserrat");
    TEST_ASSERT(matchedFamily == "Montserrat");

    std::string fallbackFamily = fonts.resolveFontFamily("NonExistentFont999");
    TEST_ASSERT(fallbackFamily == "Inter"); // Graceful fallback

    // 4. Nearest weight resolution
    int w400 = fonts.resolveNearestWeight("inter", 420);
    TEST_ASSERT(w400 == 400);

    int w700 = fonts.resolveNearestWeight("inter", 730);
    TEST_ASSERT(w700 == 700);

    std::cout << "[PASS] runFontRegistryTests" << std::endl;
}

void runRationalFrameRateTests() {
    using namespace catchim::core;

    // 1. Standard Rates
    const auto& stdRates = RationalFrameRateHelper::standardRates();
    TEST_ASSERT(stdRates.size() >= 10);

    // 2. Conversion from float
    FrameRate r23976 = RationalFrameRateHelper::fromFloat(23.976);
    TEST_ASSERT(r23976.numerator == 24000 && r23976.denominator == 1001);

    FrameRate r2997 = RationalFrameRateHelper::fromFloat(29.97);
    TEST_ASSERT(r2997.numerator == 30000 && r2997.denominator == 1001);

    FrameRate r5994 = RationalFrameRateHelper::fromFloat(59.94);
    TEST_ASSERT(r5994.numerator == 60000 && r5994.denominator == 1001);

    FrameRate r25 = RationalFrameRateHelper::fromFloat(25.0);
    TEST_ASSERT(r25.numerator == 25 && r25.denominator == 1);

    FrameRate r60 = RationalFrameRateHelper::fromFloat(60.0);
    TEST_ASSERT(r60.numerator == 60 && r60.denominator == 1);

    // 3. GCD & Rounding
    int64_t g = RationalFrameRateHelper::gcd(24000, 1001);
    TEST_ASSERT(g == 1);

    FrameRate fps30{30, 1};
    int64_t ticksPerFrame30 = 120000 / 30; // 4000 ticks
    TEST_ASSERT(ticksPerFrame30 == 4000);
    TEST_ASSERT(RationalFrameRateHelper::roundFrameTicks(4050, fps30) == 4000);
    TEST_ASSERT(RationalFrameRateHelper::roundFrameTicks(5900, fps30) == 4000 || RationalFrameRateHelper::roundFrameTicks(5900, fps30) == 8000);
    TEST_ASSERT(RationalFrameRateHelper::floorFrameTicks(5900, fps30) == 4000);
    TEST_ASSERT(RationalFrameRateHelper::lastFrameTicks(120000, fps30) == 116000);

    // 4. Project FPS Elevation for Imported Media
    std::vector<double> importedList = {24.0, 59.94, 30.0};
    auto highest = RationalFrameRateHelper::getHighestImportedVideoFps(importedList);
    TEST_ASSERT(highest.has_value());
    TEST_ASSERT(std::abs(highest.value() - 59.94) < 1e-3);

    auto raised = RationalFrameRateHelper::getRaisedProjectFpsForImportedMedia(fps30, importedList);
    TEST_ASSERT(raised.has_value());
    TEST_ASSERT(raised->numerator == 60000 && raised->denominator == 1001);

    // If imported video is lower than project FPS, no elevation
    std::vector<double> lowerList = {24.0, 25.0};
    auto notRaised = RationalFrameRateHelper::getRaisedProjectFpsForImportedMedia(fps30, lowerList);
    TEST_ASSERT(!notRaised.has_value());

    std::cout << "[PASS] runRationalFrameRateTests" << std::endl;
}

void runGroupResizeEngineTests() {
    using namespace catchim::core;
    using namespace catchim::editor;

    Timeline timeline;
    auto videoTrackId = timeline.mainTrack().id();

    // Clip 1 on Main Track: [2.0s to 6.0s] (dur 4.0s), trimStart 1.0s, trimEnd 1.0s, sourceDuration 6.0s
    auto clip1Id = ClipId::generate();
    Clip clip1(clip1Id, ClipType::Video, "Clip1", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(4.0),
               TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(1.0));
    clip1.setSourceDuration(TimelineTime::fromSeconds(6.0));
    timeline.addClip(videoTrackId, std::move(clip1));

    // Clip 2 on Main Track: [8.0s to 12.0s] (dur 4.0s), trimStart 0.5s, trimEnd 1.5s, sourceDuration 6.0s
    auto clip2Id = ClipId::generate();
    Clip clip2(clip2Id, ClipType::Video, "Clip2", TimelineTime::fromSeconds(8.0), TimelineTime::fromSeconds(4.0),
               TimelineTime::fromSeconds(0.5), TimelineTime::fromSeconds(1.5));
    clip2.setSourceDuration(TimelineTime::fromSeconds(6.0));
    timeline.addClip(videoTrackId, std::move(clip2));

    FrameRate fps30{30, 1};

    // 1. Build members
    auto members = GroupResizeEngine::buildResizeMembers(timeline, {clip1Id, clip2Id});
    TEST_ASSERT(members.size() == 2);

    // 2. Right-side resize by +0.5s
    auto resRight = GroupResizeEngine::computeGroupResize(members, ResizeSide::Right, TimelineTime::fromSeconds(0.5), fps30);
    TEST_ASSERT(resRight.deltaTime == TimelineTime::fromSeconds(0.5));
    TEST_ASSERT(resRight.updates.size() == 2);

    // Verify invariant: trimStart + duration + trimEnd == sourceDuration
    for (const auto& u : resRight.updates) {
        auto dur = u.patch.duration;
        auto ts = u.patch.trimStart;
        auto te = u.patch.trimEnd;
        TEST_ASSERT(ts + dur + te == TimelineTime::fromSeconds(6.0));
        TEST_ASSERT(dur == TimelineTime::fromSeconds(4.5));
    }

    // 3. Left-side resize by +0.5s (trims 0.5s from start)
    auto resLeft = GroupResizeEngine::computeGroupResize(members, ResizeSide::Left, TimelineTime::fromSeconds(0.5), fps30);
    TEST_ASSERT(resLeft.deltaTime == TimelineTime::fromSeconds(0.5));
    for (const auto& u : resLeft.updates) {
        auto dur = u.patch.duration;
        auto ts = u.patch.trimStart;
        auto te = u.patch.trimEnd;
        TEST_ASSERT(ts + dur + te == TimelineTime::fromSeconds(6.0));
        TEST_ASSERT(dur == TimelineTime::fromSeconds(3.5));
    }

    // 4. Neighbor bound constraint test
    // Clip1 ends at 6.0s, Clip2 starts at 8.0s -> Clip1 cannot extend to the right past 8.0s (delta max +2.0s)
    members[0].rightNeighborBound = TimelineTime::fromSeconds(7.0); // Artificially bounded at 7.0s
    auto resBounded = GroupResizeEngine::computeGroupResize(members, ResizeSide::Right, TimelineTime::fromSeconds(3.0), fps30);
    // Delta should be clamped to +1.0s max (7.0s - 6.0s = 1.0s)
    TEST_ASSERT(resBounded.deltaTime == TimelineTime::fromSeconds(1.0));

    // 5. Command Execution & Undo
    GroupResizeCommand cmd(timeline, resRight.updates);
    bool okExec = cmd.execute();
    TEST_ASSERT(okExec);
    TEST_ASSERT(timeline.findClip(clip1Id)->duration() == TimelineTime::fromSeconds(4.5));
    TEST_ASSERT(timeline.findClip(clip2Id)->duration() == TimelineTime::fromSeconds(4.5));

    bool okUndo = cmd.undo();
    TEST_ASSERT(okUndo);
    TEST_ASSERT(timeline.findClip(clip1Id)->duration() == TimelineTime::fromSeconds(4.0));
    TEST_ASSERT(timeline.findClip(clip2Id)->duration() == TimelineTime::fromSeconds(4.0));

    std::cout << "[PASS] runGroupResizeEngineTests" << std::endl;
}

void runPlacementEngineTests() {
    using namespace catchim::core;
    using namespace catchim::editor;

    Timeline timeline;
    auto videoTrackId = timeline.mainTrack().id();

    // Clip 1: [1.0s to 3.0s] (dur 2.0s)
    auto c1Id = ClipId::generate();
    Clip c1(c1Id, ClipType::Video, "Clip1", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(2.0));
    timeline.addClip(videoTrackId, std::move(c1));

    // Clip 2: [5.0s to 8.0s] (dur 3.0s)
    auto c2Id = ClipId::generate();
    Clip c2(c2Id, ClipType::Video, "Clip2", TimelineTime::fromSeconds(5.0), TimelineTime::fromSeconds(3.0));
    timeline.addClip(videoTrackId, std::move(c2));

    const auto& track = timeline.mainTrack();

    // 1. Fast Collision Check
    TEST_ASSERT(PlacementEngine::canPlaceClipOnTrack(track, TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(1.0)));
    TEST_ASSERT(!PlacementEngine::canPlaceClipOnTrack(track, TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(2.0))); // Overlaps c1
    TEST_ASSERT(PlacementEngine::canPlaceClipOnTrack(track, TimelineTime::fromSeconds(3.0), TimelineTime::fromSeconds(2.0))); // In gap [3.0, 5.0]
    TEST_ASSERT(!PlacementEngine::canPlaceClipOnTrack(track, TimelineTime::fromSeconds(4.0), TimelineTime::fromSeconds(2.0))); // Overlaps c2
    TEST_ASSERT(PlacementEngine::canPlaceClipOnTrack(track, TimelineTime::fromSeconds(8.0), TimelineTime::fromSeconds(2.0))); // After c2

    // Excluding Clip1 allows placing right over Clip1
    TEST_ASSERT(PlacementEngine::canPlaceClipOnTrack(track, TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(2.0), c1Id));

    // 2. Find Available Gap
    // Seeking gap of 1.5s from t=0.0s -> cannot fit [0.0, 1.5] because Clip1 starts at 1.0s.
    // Earliest gap is [3.0s to 4.5s]!
    auto gap = PlacementEngine::findAvailableGap(track, TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(1.5));
    TEST_ASSERT(gap == TimelineTime::fromSeconds(3.0));

    // Seeking gap of 3.0s from t=0.0s -> cannot fit [3.0, 5.0] (only 2.0s gap). Fits after Clip2 at 8.0s!
    auto gap2 = PlacementEngine::findAvailableGap(track, TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(3.0));
    TEST_ASSERT(gap2 == TimelineTime::fromSeconds(8.0));

    // 3. Magnetic Main Track start enforcement
    // Clip1 starts at 1.0s. If requested 0.5s <= 1.0s, enforced to 0.0s!
    auto enforced = PlacementEngine::enforceMainTrackStart(track, TimelineTime::fromSeconds(0.5));
    TEST_ASSERT(enforced == TimelineTime::fromSeconds(0.0));

    // If requested 3.5s > 1.0s, allowed as is
    auto allowed = PlacementEngine::enforceMainTrackStart(track, TimelineTime::fromSeconds(3.5));
    TEST_ASSERT(allowed == TimelineTime::fromSeconds(3.5));

    // 4. Clip to Track Compatibility
    TEST_ASSERT(PlacementEngine::canClipGoOnTrack(ClipType::Video, TrackType::Video));
    TEST_ASSERT(PlacementEngine::canClipGoOnTrack(ClipType::Image, TrackType::Video));
    TEST_ASSERT(!PlacementEngine::canClipGoOnTrack(ClipType::Video, TrackType::Audio));
    TEST_ASSERT(PlacementEngine::canClipGoOnTrack(ClipType::Audio, TrackType::Audio));
    TEST_ASSERT(PlacementEngine::canClipGoOnTrack(ClipType::Graphic, TrackType::Graphic));
    TEST_ASSERT(PlacementEngine::canClipGoOnTrack(ClipType::Sticker, TrackType::Graphic));

    // 5. Preferred Track Placement
    auto placeAudio = PlacementEngine::resolvePreferredTrackPlacement(timeline, TrackType::Audio, 0, "below");
    TEST_ASSERT(placeAudio.insertPosition == "below");

    std::cout << "[PASS] runPlacementEngineTests" << std::endl;
}

void runAudioDspFiltersTests() {
    using namespace catchim::audio;

    constexpr uint32_t SR = 48000;
    constexpr size_t N = SR / 4; // 0.25 seconds

    // 1. Lowpass Filter Verification
    BiquadFilter lpf;
    lpf.configure(BiquadFilterType::Lowpass, SR, 1000.0, 0.7071);

    // Generate 100 Hz signal (passband) and 10000 Hz signal (stopband)
    std::vector<float> lowFreq(N), highFreq(N);
    for (size_t i = 0; i < N; ++i) {
        double t = static_cast<double>(i) / SR;
        lowFreq[i] = static_cast<float>(std::sin(2.0 * 3.1415926535 * 100.0 * t));
        highFreq[i] = static_cast<float>(std::sin(2.0 * 3.1415926535 * 10000.0 * t));
    }

    lpf.processBuffer(lowFreq.data(), N);
    lpf.reset();
    lpf.processBuffer(highFreq.data(), N);

    // Measure RMS after filter settling (last half of samples)
    double lowRms = 0.0, highRms = 0.0;
    for (size_t i = N / 2; i < N; ++i) {
        lowRms += lowFreq[i] * lowFreq[i];
        highRms += highFreq[i] * highFreq[i];
    }
    lowRms = std::sqrt(lowRms / (N / 2));
    highRms = std::sqrt(highRms / (N / 2));

    // 100 Hz should have high RMS (~0.7), 10000 Hz should be heavily attenuated (< 0.1)
    TEST_ASSERT(lowRms > 0.6);
    TEST_ASSERT(highRms < 0.1);

    // 2. Highpass Filter Verification
    BiquadFilter hpf;
    hpf.configure(BiquadFilterType::Highpass, SR, 3000.0, 0.7071);
    for (size_t i = 0; i < N; ++i) {
        double t = static_cast<double>(i) / SR;
        lowFreq[i] = static_cast<float>(std::sin(2.0 * 3.1415926535 * 100.0 * t));
        highFreq[i] = static_cast<float>(std::sin(2.0 * 3.1415926535 * 10000.0 * t));
    }
    hpf.processBuffer(lowFreq.data(), N);
    hpf.reset();
    hpf.processBuffer(highFreq.data(), N);

    lowRms = 0.0; highRms = 0.0;
    for (size_t i = N / 2; i < N; ++i) {
        lowRms += lowFreq[i] * lowFreq[i];
        highRms += highFreq[i] * highFreq[i];
    }
    lowRms = std::sqrt(lowRms / (N / 2));
    highRms = std::sqrt(highRms / (N / 2));
    TEST_ASSERT(lowRms < 0.1);
    TEST_ASSERT(highRms > 0.6);

    // 3. Three-Band Equalizer
    ThreeBandEqualizer eq(SR);
    eq.setLowGain(6.0); // +6dB bass
    eq.setMidGain(-3.0);
    eq.setHighGain(3.0);
    TEST_ASSERT(eq.lowGain() == 6.0);
    TEST_ASSERT(eq.midGain() == -3.0);
    TEST_ASSERT(eq.highGain() == 3.0);

    std::vector<float> stereoL(N, 0.5f), stereoR(N, 0.5f);
    eq.processStereo(stereoL.data(), stereoR.data(), N);
    // Buffer processed without NaNs or infinities
    for (size_t i = 0; i < N; ++i) {
        TEST_ASSERT(std::isfinite(stereoL[i]));
        TEST_ASSERT(std::isfinite(stereoR[i]));
    }

    eq.reset();

    std::cout << "[PASS] runAudioDspFiltersTests" << std::endl;
}

void runColorUtilsTests() {
    using namespace catchim::core;

    // 1. Hex to RGB
    auto cOrange = ColorUtils::hexToRgb("#ff8000");
    TEST_ASSERT(cOrange.has_value());
    TEST_ASSERT(cOrange->r == 255 && cOrange->g == 128 && cOrange->b == 0);
    TEST_ASSERT(std::abs(cOrange->a - 1.0) < 1e-4);

    // 3-char hex expansion #f0a -> #ff00aa
    auto cShort = ColorUtils::hexToRgb("f0a");
    TEST_ASSERT(cShort.has_value());
    TEST_ASSERT(cShort->r == 255 && cShort->g == 0 && cShort->b == 170);

    // 8-char hex with alpha #00ff0080
    auto cGreenAlpha = ColorUtils::hexToRgb("#00ff0080");
    TEST_ASSERT(cGreenAlpha.has_value());
    TEST_ASSERT(cGreenAlpha->r == 0 && cGreenAlpha->g == 255 && cGreenAlpha->b == 0);
    TEST_ASSERT(std::abs(cGreenAlpha->a - (128.0 / 255.0)) < 0.01);

    // 2. RGB to Hex
    RgbColor rgb{255, 128, 0, 1.0};
    TEST_ASSERT(ColorUtils::rgbToHex(rgb, false) == "ff8000");

    // 3. Hex <-> HSV
    auto redHsv = ColorUtils::hexToHsv("#ff0000");
    TEST_ASSERT(redHsv.has_value());
    TEST_ASSERT(std::abs(redHsv->h - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(redHsv->s - 1.0) < 1e-4);
    TEST_ASSERT(std::abs(redHsv->v - 1.0) < 1e-4);

    auto greenHsv = ColorUtils::hexToHsv("#00ff00");
    TEST_ASSERT(greenHsv.has_value());
    TEST_ASSERT(std::abs(greenHsv->h - 120.0) < 1e-4);

    // 4. Hex <-> HSL
    auto blueHsl = ColorUtils::hexToHsl("#0000ff");
    TEST_ASSERT(blueHsl.has_value());
    TEST_ASSERT(std::abs(blueHsl->h - 240.0) < 1e-4);

    // 5. Alpha parsing & appending
    auto [pureRgb, alpha] = ColorUtils::parseHexAlpha("ff800080");
    TEST_ASSERT(pureRgb == "ff8000");
    TEST_ASSERT(std::abs(alpha - (128.0 / 255.0)) < 0.01);

    std::string appended = ColorUtils::appendAlpha("ff8000", 0.5);
    TEST_ASSERT(appended.size() == 8);
    TEST_ASSERT(appended.rfind("ff8000", 0) == 0);

    // 6. Text & CSS color extraction
    auto extracted1 = ColorUtils::extractColorFromText("color: #3b82f6 !important;");
    TEST_ASSERT(extracted1.has_value() && extracted1.value() == "3b82f6");

    auto extracted2 = ColorUtils::extractColorFromText("background-color: 10b981;");
    TEST_ASSERT(extracted2.has_value() && extracted2.value() == "10b981");

    auto extracted3 = ColorUtils::extractColorFromText("Some arbitrary label with #ef4444 badge");
    TEST_ASSERT(extracted3.has_value() && extracted3.value() == "ef4444");

    // 7. Format color value
    std::string rgbStr = ColorUtils::formatColorValue("ff8000", ColorFormat::Rgb);
    TEST_ASSERT(rgbStr == "255, 128, 0");

    // 8. Parse color input
    auto parsedRgb = ColorUtils::parseColorInput("255, 128, 0", ColorFormat::Rgb);
    TEST_ASSERT(parsedRgb.has_value() && parsedRgb.value() == "ff8000");

    std::cout << "[PASS] runColorUtilsTests" << std::endl;
}

void runTimelineLayoutEngineTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. Basic Track Heights
    TEST_ASSERT(TimelineLayoutEngine::getTrackHeight(TrackType::Video) == 65);
    TEST_ASSERT(TimelineLayoutEngine::getTrackHeight(TrackType::Audio) == 50);
    TEST_ASSERT(TimelineLayoutEngine::getTrackHeight(TrackType::Text) == 25);
    TEST_ASSERT(TimelineLayoutEngine::getTrackHeight(TrackType::Graphic) == 25);
    TEST_ASSERT(TimelineLayoutEngine::getTrackHeight(TrackType::Effect) == 25);

    // 2. Expanded track height
    int expVideo = TimelineLayoutEngine::getExpandedTrackHeight(TrackType::Video, 3);
    TEST_ASSERT(expVideo == 65 + 3 * 20); // 125

    // 3. Layout offsets & Cumulative heights
    std::vector<TrackType> tracks = {TrackType::Video, TrackType::Audio, TrackType::Text};
    int h0 = TimelineLayoutEngine::getCumulativeHeightBefore(tracks, 0);
    TEST_ASSERT(h0 == 0);

    int h1 = TimelineLayoutEngine::getCumulativeHeightBefore(tracks, 1);
    TEST_ASSERT(h1 == 65 + 6); // 71

    int h2 = TimelineLayoutEngine::getCumulativeHeightBefore(tracks, 2);
    TEST_ASSERT(h2 == 71 + 50 + 6); // 127

    auto offsets = TimelineLayoutEngine::getTrackLayoutOffsets(tracks);
    TEST_ASSERT(offsets.size() == 3);
    TEST_ASSERT(offsets[0] == 0 && offsets[1] == 71 && offsets[2] == 127);

    int totalH = TimelineLayoutEngine::getTotalTracksHeight(tracks);
    TEST_ASSERT(totalH == 65 + 50 + 25 + 2 * 6); // 152

    // 4. Property Labels
    TEST_ASSERT(TimelineLayoutEngine::getPropertyLabel("transform.positionX") == "Position X");
    TEST_ASSERT(TimelineLayoutEngine::getPropertyLabel("opacity") == "Opacity");
    TEST_ASSERT(TimelineLayoutEngine::getPropertyLabel("params.blurRadius") == "blurRadius");

    // 5. Expanded Rows from Clip
    Clip clip(ClipId::generate(), ClipType::Video, "Hero", TimelineTime::fromSeconds(0), TimelineTime::fromSeconds(5));
    nlohmann::json anims;
    anims["transform.positionX"] = nlohmann::json::object();
    anims["opacity"] = nlohmann::json::object();
    clip.setParam("animations", anims);

    auto rows = TimelineLayoutEngine::getExpandedRows(clip);
    TEST_ASSERT(rows.size() == 2);
    TEST_ASSERT(rows[0].propertyPath == "transform.positionX");
    TEST_ASSERT(rows[1].propertyPath == "opacity");

    int expH = TimelineLayoutEngine::getExpansionHeight(rows);
    TEST_ASSERT(expH == 40); // 2 * 20

    // 6. Track expansion height
    Track track(TrackId::generate(), TrackType::Video, "Main");
    track.insertClip(clip);
    std::unordered_set<ClipId> expandedSet = {clip.id()};
    int trackExpH = TimelineLayoutEngine::computeTrackExpansionHeight(track, expandedSet);
    TEST_ASSERT(trackExpH == 40);

    std::cout << "[PASS] runTimelineLayoutEngineTests" << std::endl;
}

void runAudioDisplayMetricsTests() {
    using namespace catchim::audio;

    // 1. Clamping
    TEST_ASSERT(AudioDisplayMetrics::clampDb(-100.0) == -60.0);
    TEST_ASSERT(AudioDisplayMetrics::clampDb(50.0) == 20.0);
    TEST_ASSERT(AudioDisplayMetrics::clampDb(0.0) == 0.0);

    // 2. Line Pos from dB
    double pos20 = AudioDisplayMetrics::getLinePosFromDb(20.0); // max gain -> top of slider (0%)
    TEST_ASSERT(std::abs(pos20 - 0.0) < 1e-3);

    double posMin = AudioDisplayMetrics::getLinePosFromDb(-60.0); // min gain -> bottom (100%)
    TEST_ASSERT(std::abs(posMin - 100.0) < 1e-3);

    double pos0 = AudioDisplayMetrics::getLinePosFromDb(0.0); // 0 dB unity gain -> around 68.4%
    TEST_ASSERT(pos0 > 60.0 && pos0 < 75.0);

    // 3. Inverse: getDbFromLinePos roundtrip
    double dbTest = -12.0;
    double pos = AudioDisplayMetrics::getLinePosFromDb(dbTest);
    double recoveredDb = AudioDisplayMetrics::getDbFromLinePos(pos);
    TEST_ASSERT(std::abs(recoveredDb - dbTest) < 0.1);

    // 4. Waveform bar fraction
    TEST_ASSERT(AudioDisplayMetrics::getBarFractionFromOutputAmplitude(0.0) == 0.0);
    TEST_ASSERT(AudioDisplayMetrics::getBarFractionFromOutputAmplitude(0.001) == 0.0); // < -40dB
    TEST_ASSERT(std::abs(AudioDisplayMetrics::getBarFractionFromOutputAmplitude(1.0) - 1.0) < 1e-3); // 0 dBFS -> 1.0

    double fracMid = AudioDisplayMetrics::getBarFractionFromOutputAmplitude(0.25);
    TEST_ASSERT(fracMid > 0.2 && fracMid < 0.9);

    std::cout << "[PASS] runAudioDisplayMetricsTests" << std::endl;
}

void runAdvancedSnapEngineTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. Zoom-adaptive snap threshold
    auto thresh1x = AdvancedSnapEngine::getTimelineSnapThresholdInTicks(1.0, 10.0);
    TEST_ASSERT(thresh1x == TimelineTime::fromSeconds(0.1)); // 10 / 100 = 0.1s

    auto thresh2x = AdvancedSnapEngine::getTimelineSnapThresholdInTicks(2.0, 10.0);
    TEST_ASSERT(thresh2x == TimelineTime::fromSeconds(0.05)); // 10 / 200 = 0.05s

    auto threshHalf = AdvancedSnapEngine::getTimelineSnapThresholdInTicks(0.5, 10.0);
    TEST_ASSERT(threshHalf == TimelineTime::fromSeconds(0.2)); // 10 / 50 = 0.2s

    // 2. Build and collect all snap points
    Timeline timeline;
    auto videoTrackId = timeline.mainTrack().id();

    // Clip: [2.0s to 5.0s] with a keyframe at relative time +1.0s (absolute 3.0s)
    auto clipId = ClipId::generate();
    Clip clip(clipId, ClipType::Video, "Clip1", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(3.0));
    nlohmann::json anims;
    nlohmann::json kfList = nlohmann::json::array();
    kfList.push_back(nlohmann::json{
        {"time", TimelineTime::fromSeconds(1.0).ticks()}
    });
    anims["transform.positionX"]["keys"] = kfList;
    clip.setParam("animations", anims);
    timeline.addClip(videoTrackId, std::move(clip));

    // Bookmark at 4.0s
    timeline.addBookmark(Bookmark{BookmarkId::generate(), TimelineTime::fromSeconds(4.0)});

    // Playhead at 0.5s
    TimelineTime playhead = TimelineTime::fromSeconds(0.5);

    auto allPoints = AdvancedSnapEngine::collectAllSnapPoints(timeline, playhead, true);
    TEST_ASSERT(allPoints.size() == 5); // playhead(0.5), clipStart(2.0), keyframe(3.0), bookmark(4.0), clipEnd(5.0)

    // Verify points are sorted ascending
    for (size_t i = 1; i < allPoints.size(); ++i) {
        TEST_ASSERT(allPoints[i - 1].time <= allPoints[i].time);
    }

    // 3. Fast binary search snap
    TimelineTime maxSnapDist = TimelineTime::fromSeconds(0.15);

    // Target 1.95s -> snaps to clipStart at 2.0s
    auto res1 = AdvancedSnapEngine::resolveSortedTimelineSnap(TimelineTime::fromSeconds(1.95), allPoints, maxSnapDist);
    TEST_ASSERT(res1.hasSnapped);
    TEST_ASSERT(res1.snappedTime == TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(res1.snapPoint->type == AdvancedSnapType::ClipStart);

    // Target 2.95s -> snaps to keyframe at 3.0s
    auto resKf = AdvancedSnapEngine::resolveSortedTimelineSnap(TimelineTime::fromSeconds(2.95), allPoints, maxSnapDist);
    TEST_ASSERT(resKf.hasSnapped);
    TEST_ASSERT(resKf.snappedTime == TimelineTime::fromSeconds(3.0));
    TEST_ASSERT(resKf.snapPoint->type == AdvancedSnapType::Keyframe);

    // Target 3.92s -> snaps to bookmark at 4.0s
    auto resBm = AdvancedSnapEngine::resolveSortedTimelineSnap(TimelineTime::fromSeconds(3.92), allPoints, maxSnapDist);
    TEST_ASSERT(resBm.hasSnapped);
    TEST_ASSERT(resBm.snappedTime == TimelineTime::fromSeconds(4.0));
    TEST_ASSERT(resBm.snapPoint->type == AdvancedSnapType::Bookmark);

    // Target 1.4s -> too far from any point (> 0.15s) -> does not snap
    auto resFar = AdvancedSnapEngine::resolveSortedTimelineSnap(TimelineTime::fromSeconds(1.4), allPoints, maxSnapDist);
    TEST_ASSERT(!resFar.hasSnapped);
    TEST_ASSERT(resFar.snappedTime == TimelineTime::fromSeconds(1.4));

    std::cout << "[PASS] runAdvancedSnapEngineTests" << std::endl;
}

void runBatchCommandAndTracksSnapshotTests() {
    using namespace catchim;
    using namespace catchim::core;
    using namespace catchim::editor;

    // 1. BatchCommand basic execution and undo
    Timeline tl;
    TrackId mainId = tl.mainTrack().id();

    Clip c1(ClipId("batch-c1"), ClipType::Video, "Clip 1", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(4.0));
    Clip c2(ClipId("batch-c2"), ClipType::Video, "Clip 2", TimelineTime::fromSeconds(4.0), TimelineTime::fromSeconds(4.0));
    Clip c3(ClipId("batch-c3"), ClipType::Video, "Clip 3", TimelineTime::fromSeconds(8.0), TimelineTime::fromSeconds(4.0));

    auto batch = std::make_unique<BatchCommand>("Add 3 Clips");
    batch->addCommand(std::make_unique<AddClipCommand>(tl, mainId, c1));
    batch->addCommand(std::make_unique<AddClipCommand>(tl, mainId, c2));
    batch->addCommand(std::make_unique<AddClipCommand>(tl, mainId, c3));

    TEST_ASSERT(batch->count() == 3);
    TEST_ASSERT(!batch->empty());
    TEST_ASSERT(batch->name() == "Add 3 Clips");
    TEST_ASSERT(tl.mainTrack().clips().empty());

    // Execute batch
    bool execOk = batch->execute();
    TEST_ASSERT(execOk);
    TEST_ASSERT(tl.mainTrack().clips().size() == 3);
    TEST_ASSERT(tl.findClip(ClipId("batch-c1")) != nullptr);
    TEST_ASSERT(tl.findClip(ClipId("batch-c2")) != nullptr);
    TEST_ASSERT(tl.findClip(ClipId("batch-c3")) != nullptr);

    // Undo batch (in reverse order)
    bool undoOk = batch->undo();
    TEST_ASSERT(undoOk);
    TEST_ASSERT(tl.mainTrack().clips().empty());

    // Re-execute
    execOk = batch->execute();
    TEST_ASSERT(execOk);
    TEST_ASSERT(tl.mainTrack().clips().size() == 3);

    // 2. BatchCommand rollback on failure
    Timeline tlFail;
    TrackId failMainId = tlFail.mainTrack().id();
    // Intentionally overlapping clips that fail canPlace
    Clip f1(ClipId("f1"), ClipType::Video, "F1", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(5.0));
    Clip f2(ClipId("f2"), ClipType::Video, "F2", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(5.0)); // Collides with f1

    auto failBatch = std::make_unique<BatchCommand>("Failing Batch");
    failBatch->addCommand(std::make_unique<AddClipCommand>(tlFail, failMainId, f1));
    failBatch->addCommand(std::make_unique<AddClipCommand>(tlFail, failMainId, f2));

    bool failRes = failBatch->execute();
    TEST_ASSERT(!failRes);
    // Should have rolled back f1
    TEST_ASSERT(tlFail.mainTrack().clips().empty());

    // 3. TimelineTracksSnapshot and TracksSnapshotCommand
    TimelineTracksSnapshot beforeSnapshot = tl.createSnapshot();
    TEST_ASSERT(beforeSnapshot.mainTrack.clips().size() == 3);

    // Modify tl by removing clips
    tl.removeClip(ClipId("batch-c2"));
    TEST_ASSERT(tl.mainTrack().clips().size() == 2);
    TimelineTracksSnapshot afterSnapshot = tl.createSnapshot();
    TEST_ASSERT(afterSnapshot.mainTrack.clips().size() == 2);

    TracksSnapshotCommand snapCmd(tl, beforeSnapshot, afterSnapshot, "Restore Clips State");
    TEST_ASSERT(snapCmd.name() == "Restore Clips State");

    // Execute snapCmd (applies afterSnapshot -> 2 clips)
    snapCmd.execute();
    TEST_ASSERT(tl.mainTrack().clips().size() == 2);

    // Undo snapCmd (restores beforeSnapshot -> 3 clips)
    snapCmd.undo();
    TEST_ASSERT(tl.mainTrack().clips().size() == 3);
    TEST_ASSERT(tl.findClip(ClipId("batch-c2")) != nullptr);

    std::cout << "[PASS] runBatchCommandAndTracksSnapshotTests" << std::endl;
}

void runElementFactoryAndUtilsTests() {
    using namespace catchim;
    using namespace catchim::core;
    using namespace catchim::editor;

    // 1. ElementFactory tests
    auto txt = ElementFactory::buildTextElement("Title", "Antigravity Studio", TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(txt.type() == ClipType::Text);
    TEST_ASSERT(txt.name() == "Title");
    TEST_ASSERT(txt.startTime() == TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(txt.duration() == ElementFactory::DEFAULT_NEW_ELEMENT_DURATION);
    TEST_ASSERT(txt.getParam<std::string>("content", "") == "Antigravity Studio");
    TEST_ASSERT(txt.getParam<double>("fontSize", 0.0) == 48.0);
    TEST_ASSERT(txt.getParam<std::string>("fontFamily", "") == "Inter");
    TEST_ASSERT(txt.getParam<std::string>("color", "") == "#FFFFFF");

    auto vid = ElementFactory::buildVideoElement(
        MediaId("vid-123"), "Intro.mp4", TimelineTime::fromSeconds(15.0), TimelineTime::fromSeconds(0.0)
    );
    TEST_ASSERT(vid.type() == ClipType::Video);
    TEST_ASSERT(vid.mediaId() == MediaId("vid-123"));
    TEST_ASSERT(vid.duration() == TimelineTime::fromSeconds(15.0));
    TEST_ASSERT(vid.sourceDuration().has_value() && vid.sourceDuration()->toSeconds() == 15.0);
    TEST_ASSERT(vid.getParam<bool>("isSourceAudioEnabled", false) == true);
    TEST_ASSERT(vid.getParam<double>("volume", 0.0) == 1.0);

    auto aud = ElementFactory::buildAudioElement(
        MediaId("aud-456"), "Voiceover.wav", TimelineTime::fromSeconds(20.0), TimelineTime::fromSeconds(1.0)
    );
    TEST_ASSERT(aud.type() == ClipType::Audio);
    TEST_ASSERT(aud.mediaId() == MediaId("aud-456"));
    TEST_ASSERT(aud.duration() == TimelineTime::fromSeconds(20.0));
    TEST_ASSERT(aud.startTime() == TimelineTime::fromSeconds(1.0));

    auto img = ElementFactory::buildImageElement(
        MediaId("img-789"), "Logo.png", TimelineTime::fromSeconds(3.0), TimelineTime::fromSeconds(5.0)
    );
    TEST_ASSERT(img.type() == ClipType::Image);
    TEST_ASSERT(img.mediaId() == MediaId("img-789"));
    TEST_ASSERT(img.duration() == TimelineTime::fromSeconds(3.0));

    auto stk = ElementFactory::buildStickerElement(
        "emoji:thumbs-up", "", TimelineTime::fromSeconds(0.0)
    );
    TEST_ASSERT(stk.type() == ClipType::Sticker);
    TEST_ASSERT(stk.name() == "thumbs-up");
    TEST_ASSERT(stk.getParam<std::string>("stickerId", "") == "emoji:thumbs-up");

    nlohmann::json gParams;
    gParams["fill"] = "#FF0000";
    auto grp = ElementFactory::buildGraphicElement("shape:rectangle", "Red Box", TimelineTime::fromSeconds(0.0), ElementFactory::DEFAULT_NEW_ELEMENT_DURATION, gParams);
    TEST_ASSERT(grp.type() == ClipType::Graphic);
    TEST_ASSERT(grp.name() == "Red Box");
    TEST_ASSERT(grp.getParam<std::string>("fill", "") == "#FF0000");

    auto eff = ElementFactory::buildEffectElement("blur:gaussian", "Gaussian Blur", TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(eff.type() == ClipType::Effect);
    TEST_ASSERT(eff.getParam<std::string>("effectType", "") == "blur:gaussian");

    // buildElementFromMedia
    auto medVid = ElementFactory::buildElementFromMedia(MediaId("mv"), "video", "Mv", TimelineTime::fromSeconds(10.0), TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(medVid.type() == ClipType::Video);
    auto medAud = ElementFactory::buildElementFromMedia(MediaId("ma"), "audio", "Ma", TimelineTime::fromSeconds(10.0), TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(medAud.type() == ClipType::Audio);
    auto medImg = ElementFactory::buildElementFromMedia(MediaId("mi"), "image", "Mi", TimelineTime::fromSeconds(10.0), TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(medImg.type() == ClipType::Image);

    // 2. ElementUtils predicates
    TEST_ASSERT(ElementUtils::canElementHaveAudio(ClipType::Video) == true);
    TEST_ASSERT(ElementUtils::canElementHaveAudio(ClipType::Audio) == true);
    TEST_ASSERT(ElementUtils::canElementHaveAudio(ClipType::Image) == false);
    TEST_ASSERT(ElementUtils::canElementHaveAudio(ClipType::Text) == false);

    TEST_ASSERT(ElementUtils::isVisualElement(ClipType::Video) == true);
    TEST_ASSERT(ElementUtils::isVisualElement(ClipType::Image) == true);
    TEST_ASSERT(ElementUtils::isVisualElement(ClipType::Text) == true);
    TEST_ASSERT(ElementUtils::isVisualElement(ClipType::Sticker) == true);
    TEST_ASSERT(ElementUtils::isVisualElement(ClipType::Graphic) == true);
    TEST_ASSERT(ElementUtils::isVisualElement(ClipType::Effect) == true);
    TEST_ASSERT(ElementUtils::isVisualElement(ClipType::Audio) == false);

    TEST_ASSERT(ElementUtils::isMaskableElement(ClipType::Video) == true);
    TEST_ASSERT(ElementUtils::isMaskableElement(ClipType::Image) == true);
    TEST_ASSERT(ElementUtils::isMaskableElement(ClipType::Text) == true);
    TEST_ASSERT(ElementUtils::isMaskableElement(ClipType::Graphic) == true);
    TEST_ASSERT(ElementUtils::isMaskableElement(ClipType::Sticker) == true);
    TEST_ASSERT(ElementUtils::isMaskableElement(ClipType::Effect) == false);
    TEST_ASSERT(ElementUtils::isMaskableElement(ClipType::Audio) == false);

    TEST_ASSERT(ElementUtils::isRetimableElement(ClipType::Video) == true);
    TEST_ASSERT(ElementUtils::isRetimableElement(ClipType::Audio) == true);
    TEST_ASSERT(ElementUtils::isRetimableElement(ClipType::Text) == false);

    TEST_ASSERT(ElementUtils::requiresMediaId(ClipType::Video) == true);
    TEST_ASSERT(ElementUtils::requiresMediaId(ClipType::Audio) == true);
    TEST_ASSERT(ElementUtils::requiresMediaId(ClipType::Image) == true);
    TEST_ASSERT(ElementUtils::requiresMediaId(ClipType::Text) == false);

    // 3. Timeline queries
    Timeline tl;
    Track& ov = tl.addTrack(TrackType::Text, "Overlay Text");
    Track& au = tl.addTrack(TrackType::Audio, "Music");

    auto t1 = ElementFactory::buildTextElement("T1", "First", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(4.0)); // 1.0s to 5.0s
    t1.setParam("fontFamily", "Roboto");
    ov.insertClip(t1);

    auto t2 = ElementFactory::buildTextElement("T2", "Second", TimelineTime::fromSeconds(6.0), TimelineTime::fromSeconds(3.0)); // 6.0s to 9.0s
    t2.setParam("fontFamily", "Inter");
    ov.insertClip(t2);

    auto m1 = ElementFactory::buildVideoElement(MediaId("v1"), "Main", TimelineTime::fromSeconds(10.0), TimelineTime::fromSeconds(0.0)); // 0.0s to 10.0s
    tl.mainTrack().insertClip(m1);

    auto a1 = ElementFactory::buildAudioElement(MediaId("a1"), "Audio", TimelineTime::fromSeconds(8.0), TimelineTime::fromSeconds(2.0)); // 2.0s to 10.0s
    au.insertClip(a1);

    // Query at t = 3.0s:
    // strict: t1 (1..5), m1 (0..10), a1 (2..10) -> all 3
    auto at3s = ElementUtils::getElementsAtTime(tl, TimelineTime::fromSeconds(3.0), true);
    TEST_ASSERT(at3s.size() == 3);

    // Query at t = 0.0s:
    // strict: 0.0 is exact start of m1 -> false
    // inclusive: m1 included
    auto at0Strict = ElementUtils::getElementsAtTime(tl, TimelineTime::fromSeconds(0.0), true);
    TEST_ASSERT(at0Strict.empty());
    auto at0Inc = ElementUtils::getElementsAtTime(tl, TimelineTime::fromSeconds(0.0), false);
    TEST_ASSERT(at0Inc.size() == 1);
    TEST_ASSERT(at0Inc[0].clipId == m1.id());

    // Font families query
    auto fonts = ElementUtils::getElementFontFamilies(tl);
    TEST_ASSERT(fonts.size() == 2);
    TEST_ASSERT(fonts[0] == "Inter" || fonts[1] == "Inter");
    TEST_ASSERT(fonts[0] == "Roboto" || fonts[1] == "Roboto");

    std::cout << "[PASS] runElementFactoryAndUtilsTests" << std::endl;
}

void runClipboardKeyframeEngineTests() {
    using namespace catchim;
    using namespace catchim::core;
    using namespace catchim::editor;

    Timeline tl;
    CommandHistory history;

    // Create source clip with animation channels
    Clip sourceClip(ClipId("src-clip"), ClipType::Video, "Source Clip", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(10.0));

    // Channel 1: opacity with keyframes at 1.0s (0.0) and 3.0s (1.0)
    auto& opChan = sourceClip.getOrCreateAnimationChannel("opacity", 1.0);
    Keyframe k1;
    k1.time = TimelineTime::fromSeconds(1.0);
    k1.value = 0.0;
    k1.interpolation = KeyframeInterpolation::Linear;
    opChan.addOrUpdateKeyframe(k1);

    Keyframe k2;
    k2.time = TimelineTime::fromSeconds(3.0);
    k2.value = 1.0;
    k2.interpolation = KeyframeInterpolation::Bezier;
    k2.bezierX1 = 0.4;
    k2.bezierY1 = 0.0;
    k2.bezierX2 = 0.2;
    k2.bezierY2 = 1.0;
    opChan.addOrUpdateKeyframe(k2);

    // Channel 2: transform.positionX with keyframes at 2.0s (50.0) and 5.0s (250.0)
    auto& posXChan = sourceClip.getOrCreateAnimationChannel("transform.positionX", 0.0);
    Keyframe k3;
    k3.time = TimelineTime::fromSeconds(2.0);
    k3.value = 50.0;
    k3.interpolation = KeyframeInterpolation::Linear;
    posXChan.addOrUpdateKeyframe(k3);

    Keyframe k4;
    k4.time = TimelineTime::fromSeconds(5.0);
    k4.value = 250.0;
    k4.interpolation = KeyframeInterpolation::Linear;
    posXChan.addOrUpdateKeyframe(k4);

    // Put sourceClip into timeline
    TrackId mainTrackId = tl.mainTrack().id();
    tl.mainTrack().insertClip(sourceClip);

    // Create target clip on an overlay track
    Track& ov = tl.addTrack(TrackType::Video, "Overlay");
    Clip targetClip(ClipId("dst-clip"), ClipType::Video, "Target Clip", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(6.0));
    ov.insertClip(targetClip);

    // 1. Test canCopy
    auto& engine = ClipboardKeyframeEngine::instance();
    engine.clear();
    TEST_ASSERT(!engine.hasData());

    std::vector<SelectedKeyframeRef> selection = {
        {mainTrackId, ClipId("src-clip"), "opacity", TimelineTime::fromSeconds(1.0)},
        {mainTrackId, ClipId("src-clip"), "opacity", TimelineTime::fromSeconds(3.0)},
        {mainTrackId, ClipId("src-clip"), "transform.positionX", TimelineTime::fromSeconds(2.0)},
        {mainTrackId, ClipId("src-clip"), "transform.positionX", TimelineTime::fromSeconds(5.0)}
    };

    TEST_ASSERT(engine.canCopy(selection) == true);

    // Test multi-clip selection rejection (canCopy should return false)
    std::vector<SelectedKeyframeRef> invalidSelection = selection;
    invalidSelection.push_back({ov.id(), ClipId("dst-clip"), "opacity", TimelineTime::fromSeconds(0.0)});
    TEST_ASSERT(engine.canCopy(invalidSelection) == false);

    // 2. Test copy
    auto entryOpt = engine.copy(tl, selection);
    TEST_ASSERT(entryOpt.has_value());
    TEST_ASSERT(engine.hasData());

    const auto& entry = *entryOpt;
    TEST_ASSERT(entry.items.size() == 4);
    TEST_ASSERT(entry.sourceClipId == ClipId("src-clip"));

    // Verify relative time offsets (min time is 1.0s):
    // k1 (1.0s) -> offset 0.0s
    // k3 (2.0s) -> offset 1.0s
    // k2 (3.0s) -> offset 2.0s
    // k4 (5.0s) -> offset 4.0s
    TEST_ASSERT(entry.items[0].timeOffset == TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(entry.items[0].propertyPath == "opacity");
    TEST_ASSERT(entry.items[0].value == 0.0);

    TEST_ASSERT(entry.items[1].timeOffset == TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(entry.items[1].propertyPath == "transform.positionX");
    TEST_ASSERT(entry.items[1].value == 50.0);

    TEST_ASSERT(entry.items[2].timeOffset == TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(entry.items[2].propertyPath == "opacity");
    TEST_ASSERT(entry.items[2].value == 1.0);
    TEST_ASSERT(entry.items[2].interpolation == KeyframeInterpolation::Bezier);
    TEST_ASSERT(entry.items[2].bezierX1 == 0.4);

    TEST_ASSERT(entry.items[3].timeOffset == TimelineTime::fromSeconds(4.0));
    TEST_ASSERT(entry.items[3].propertyPath == "transform.positionX");
    TEST_ASSERT(entry.items[3].value == 250.0);

    // 3. Test paste into targetClip at t = 1.5s
    // Target duration is 6.0s.
    // k1: 1.5s + 0.0s = 1.5s
    // k3: 1.5s + 1.0s = 2.5s
    // k2: 1.5s + 2.0s = 3.5s
    // k4: 1.5s + 4.0s = 5.5s (within 6.0s duration)
    bool pasteOk = engine.paste(tl, history, ov.id(), ClipId("dst-clip"), TimelineTime::fromSeconds(1.5));
    TEST_ASSERT(pasteOk);

    Clip* dst = tl.findClip(ClipId("dst-clip"));
    TEST_ASSERT(dst != nullptr);
    TEST_ASSERT(dst->hasAnimationChannel("opacity"));
    TEST_ASSERT(dst->hasAnimationChannel("transform.positionX"));

    auto* dstOp = dst->findAnimationChannel("opacity");
    TEST_ASSERT(dstOp != nullptr);
    TEST_ASSERT(dstOp->size() == 2);
    auto kfOp1 = dstOp->findKeyframeAt(TimelineTime::fromSeconds(1.5));
    TEST_ASSERT(kfOp1.has_value() && kfOp1->value == 0.0);
    auto kfOp2 = dstOp->findKeyframeAt(TimelineTime::fromSeconds(3.5));
    TEST_ASSERT(kfOp2.has_value() && kfOp2->value == 1.0 && kfOp2->interpolation == KeyframeInterpolation::Bezier);

    auto* dstPos = dst->findAnimationChannel("transform.positionX");
    TEST_ASSERT(dstPos != nullptr);
    TEST_ASSERT(dstPos->size() == 2);
    auto kfPos1 = dstPos->findKeyframeAt(TimelineTime::fromSeconds(2.5));
    TEST_ASSERT(kfPos1.has_value() && kfPos1->value == 50.0);
    auto kfPos2 = dstPos->findKeyframeAt(TimelineTime::fromSeconds(5.5));
    TEST_ASSERT(kfPos2.has_value() && kfPos2->value == 250.0);

    // 4. Test Undo / Redo
    TEST_ASSERT(history.canUndo());
    history.undo();

    dst = tl.findClip(ClipId("dst-clip"));
    TEST_ASSERT(dst != nullptr);
    TEST_ASSERT(!dst->hasAnimationChannel("opacity"));
    TEST_ASSERT(!dst->hasAnimationChannel("transform.positionX"));

    // Redo
    TEST_ASSERT(history.canRedo());
    history.redo();

    dst = tl.findClip(ClipId("dst-clip"));
    TEST_ASSERT(dst != nullptr);
    TEST_ASSERT(dst->hasAnimationChannel("opacity"));
    TEST_ASSERT(dst->findAnimationChannel("opacity")->size() == 2);

    // 5. Test clamping when targetTime + offset > clip.duration()
    // Paste at targetTime = 5.0s on a 6.0s clip:
    // k4 (offset 4.0s) would be 9.0s -> clamped to 6.0s!
    auto pasteCmd = engine.createPasteCommand(tl, ov.id(), ClipId("dst-clip"), TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(pasteCmd != nullptr);
    pasteCmd->execute();

    dst = tl.findClip(ClipId("dst-clip"));
    auto* clampedChan = dst->findAnimationChannel("transform.positionX");
    TEST_ASSERT(clampedChan != nullptr);
    auto clampedKf = clampedChan->findKeyframeAt(TimelineTime::fromSeconds(6.0));
    TEST_ASSERT(clampedKf.has_value() && clampedKf->value == 250.0);

    std::cout << "[PASS] runClipboardKeyframeEngineTests" << std::endl;
}

void runTrackCommandsAndDuplicationTests() {
    using namespace catchim;
    using namespace catchim::core;
    using namespace catchim::editor;

    Timeline tl;

    // 1. AddTrackCommand
    AddTrackCommand addOv(tl, TrackType::Video, "Extra Video", 0);
    bool ok = addOv.execute();
    TEST_ASSERT(ok);
    TEST_ASSERT(tl.overlayTracks().size() == 1);
    TEST_ASSERT(tl.overlayTracks()[0].name() == "Extra Video");
    TrackId addedId = addOv.trackId();
    TEST_ASSERT(tl.findTrack(addedId) != nullptr);

    // Undo AddTrackCommand
    ok = addOv.undo();
    TEST_ASSERT(ok);
    TEST_ASSERT(tl.overlayTracks().empty());
    TEST_ASSERT(tl.findTrack(addedId) == nullptr);

    // Redo / execute again
    ok = addOv.execute();
    TEST_ASSERT(ok);
    TEST_ASSERT(tl.overlayTracks().size() == 1);

    // Add audio track
    AddTrackCommand addAud(tl, TrackType::Audio, "BGM Track");
    addAud.execute();
    TEST_ASSERT(tl.audioTracks().size() == 1);

    // 2. ToggleTrackMuteCommand and ToggleTrackVisibilityCommand
    Track* ovTrack = tl.findTrack(addedId);
    TEST_ASSERT(ovTrack != nullptr);
    TEST_ASSERT(!ovTrack->isMuted());
    TEST_ASSERT(!ovTrack->isHidden());

    ToggleTrackMuteCommand muteCmd(tl, addedId);
    muteCmd.execute();
    TEST_ASSERT(ovTrack->isMuted());
    muteCmd.undo();
    TEST_ASSERT(!ovTrack->isMuted());

    ToggleTrackVisibilityCommand hideCmd(tl, addedId);
    hideCmd.execute();
    TEST_ASSERT(ovTrack->isHidden());
    hideCmd.undo();
    TEST_ASSERT(!ovTrack->isHidden());

    // 3. RemoveTrackCommand
    // Add clip to track before removal
    Clip c(ClipId("t-c1"), ClipType::Video, "Clip In Track", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(4.0));
    tl.addClip(addedId, c);
    TEST_ASSERT(ovTrack->clips().size() == 1);

    RemoveTrackCommand remCmd(tl, addedId);
    ok = remCmd.execute();
    TEST_ASSERT(ok);
    TEST_ASSERT(tl.overlayTracks().empty());
    TEST_ASSERT(tl.findTrack(addedId) == nullptr);

    // Undo RemoveTrackCommand -> should restore track and its clip!
    ok = remCmd.undo();
    TEST_ASSERT(ok);
    TEST_ASSERT(tl.overlayTracks().size() == 1);
    Track* restoredTrack = tl.findTrack(addedId);
    TEST_ASSERT(restoredTrack != nullptr);
    TEST_ASSERT(restoredTrack->clips().size() == 1);
    TEST_ASSERT(restoredTrack->clips()[0].name() == "Clip In Track");

    // 4. DuplicateElementsCommand
    // Setup animation channel on the clip
    auto& chan = restoredTrack->clips()[0].getOrCreateAnimationChannel("opacity", 1.0);
    Keyframe kf;
    kf.time = TimelineTime::fromSeconds(2.0);
    kf.value = 0.5;
    chan.addOrUpdateKeyframe(kf);

    std::vector<ElementLocation> toDup = {
        {addedId, ClipId("t-c1")}
    };
    DuplicateElementsCommand dupCmd(tl, toDup);
    ok = dupCmd.execute();
    TEST_ASSERT(ok);
    TEST_ASSERT(dupCmd.duplicatedElements().size() == 1);
    auto dupLoc = dupCmd.duplicatedElements()[0];
    TEST_ASSERT(dupLoc.clipId != ClipId("t-c1")); // new ID generated

    Clip* dupClip = tl.findClip(dupLoc.clipId);
    TEST_ASSERT(dupClip != nullptr);
    TEST_ASSERT(dupClip->name() == "Clip In Track (copy)");
    TEST_ASSERT(dupClip->hasAnimationChannel("opacity"));
    auto* dupChan = dupClip->findAnimationChannel("opacity");
    TEST_ASSERT(dupChan != nullptr);
    TEST_ASSERT(dupChan->size() == 1);
    TEST_ASSERT(dupChan->keyframes()[0].value == 0.5);

    // Undo DuplicateElementsCommand
    ok = dupCmd.undo();
    TEST_ASSERT(ok);
    TEST_ASSERT(tl.findClip(dupLoc.clipId) == nullptr);

    std::cout << "[PASS] runTrackCommandsAndDuplicationTests" << std::endl;
}

void runBlurEffectAndPassEngineTests() {
    using namespace catchim;
    using namespace catchim::render;

    // 1. intensityToSigma
    float sig1 = BlurEffect::intensityToSigma(15.0f, 1920.0f, 1920.0f);
    TEST_ASSERT(std::abs(sig1 - 3.0f) < 0.001f);

    float sig2 = BlurEffect::intensityToSigma(15.0f, 3840.0f, 1920.0f);
    TEST_ASSERT(std::abs(sig2 - 6.0f) < 0.001f);

    // 2. buildGaussianBlurPasses
    auto zeroPasses = BlurEffect::buildGaussianBlurPasses(0.0f, 0.0f);
    TEST_ASSERT(zeroPasses.empty());

    // Small blur (sigma 3.0) -> 1 iteration -> 2 passes (H and V)
    auto smallPasses = BlurEffect::buildGaussianBlurPasses(3.0f, 3.0f);
    TEST_ASSERT(smallPasses.size() == 2);
    TEST_ASSERT(smallPasses[0].shader == BlurEffect::GAUSSIAN_BLUR_SHADER);
    TEST_ASSERT(smallPasses[0].direction.first == 1.0f && smallPasses[0].direction.second == 0.0f);
    TEST_ASSERT(std::abs(smallPasses[0].sigma - 3.0f) < 0.001f);
    TEST_ASSERT(smallPasses[0].step == 1.0f);
    TEST_ASSERT(smallPasses[1].direction.first == 0.0f && smallPasses[1].direction.second == 1.0f);
    TEST_ASSERT(std::abs(smallPasses[1].sigma - 3.0f) < 0.001f);

    // Large blur (sigma 80.0) -> iterations = ceil((80*80)/(40*40)) = 4 iterations -> 8 passes
    auto largePasses = BlurEffect::buildGaussianBlurPasses(80.0f, 80.0f);
    TEST_ASSERT(largePasses.size() == 8);
    // perPassSigma = 80 / sqrt(4) = 40.0
    TEST_ASSERT(std::abs(largePasses[0].sigma - 40.0f) < 0.01f);
    // step = max(1.0, 40 / 10) = 4.0
    TEST_ASSERT(std::abs(largePasses[0].step - 4.0f) < 0.01f);

    // 3. compute1DGaussianKernel
    auto kernel0 = BlurEffect::compute1DGaussianKernel(1.0f, 0);
    TEST_ASSERT(kernel0.size() == 1 && kernel0[0] == 1.0f);

    int radius = 3;
    auto kernel = BlurEffect::compute1DGaussianKernel(1.5f, radius);
    TEST_ASSERT(kernel.size() == 7);
    // Symmetry
    TEST_ASSERT(std::abs(kernel[0] - kernel[6]) < 0.0001f);
    TEST_ASSERT(std::abs(kernel[1] - kernel[5]) < 0.0001f);
    TEST_ASSERT(std::abs(kernel[2] - kernel[4]) < 0.0001f);
    // Center is maximum
    TEST_ASSERT(kernel[3] > kernel[2]);
    TEST_ASSERT(kernel[2] > kernel[1]);
    TEST_ASSERT(kernel[1] > kernel[0]);
    // Sum to 1.0
    float sum = 0.0f;
    for (float w : kernel) sum += w;
    TEST_ASSERT(std::abs(sum - 1.0f) < 0.0001f);

    std::cout << "[PASS] runBlurEffectAndPassEngineTests" << std::endl;
}

void runGraphicGeometryAndStrokeTests() {
    using namespace catchim;
    using namespace catchim::render;

    // 1. StarGeometry
    // 5-point star, size 200x200
    auto starVerts = StarGeometry::generateVertices(200.0, 200.0, 5, 0.5, 0.0, GraphicStrokeAlign::Center);
    TEST_ASSERT(starVerts.size() == 10);

    // Center is (100, 100). First vertex (i=0) should be at top center (angle -pi/2): (100, 100 - 100) = (100, 0)
    TEST_ASSERT(std::abs(starVerts[0].x - 100.0) < 0.001);
    TEST_ASSERT(std::abs(starVerts[0].y - 0.0) < 0.001);

    // Second vertex (i=1) is inner vertex (depth 0.5 -> radius 50)
    double dist1 = std::hypot(starVerts[1].x - 100.0, starVerts[1].y - 100.0);
    TEST_ASSERT(std::abs(dist1 - 50.0) < 0.001);

    // Third vertex (i=2) is outer vertex (radius 100)
    double dist2 = std::hypot(starVerts[2].x - 100.0, starVerts[2].y - 100.0);
    TEST_ASSERT(std::abs(dist2 - 100.0) < 0.001);

    // 2. PolygonGeometry
    // 6-sided hexagon, size 300x300
    auto polyVerts = PolygonGeometry::generateVertices(300.0, 300.0, 6, 0.0, GraphicStrokeAlign::Center);
    TEST_ASSERT(polyVerts.size() == 6);
    // First vertex is at top center: (150, 0)
    TEST_ASSERT(std::abs(polyVerts[0].x - 150.0) < 0.001);
    TEST_ASSERT(std::abs(polyVerts[0].y - 0.0) < 0.001);
    // All vertices have radius 150 from center (150, 150)
    for (const auto& v : polyVerts) {
        double dist = std::hypot(v.x - 150.0, v.y - 150.0);
        TEST_ASSERT(std::abs(dist - 150.0) < 0.001);
    }

    // 3. AlignedStrokeMetrics
    double effCenter = AlignedStrokeMetrics::getEffectiveStrokeWidth(10.0, GraphicStrokeAlign::Center);
    TEST_ASSERT(effCenter == 10.0);
    double effInside = AlignedStrokeMetrics::getEffectiveStrokeWidth(10.0, GraphicStrokeAlign::Inside);
    TEST_ASSERT(effInside == 20.0);
    double effOutside = AlignedStrokeMetrics::getEffectiveStrokeWidth(10.0, GraphicStrokeAlign::Outside);
    TEST_ASSERT(effOutside == 20.0);

    auto boundsCenter = AlignedStrokeMetrics::computeExpandedBounds(100.0, 100.0, 10.0, GraphicStrokeAlign::Center);
    TEST_ASSERT(boundsCenter.minX == -5.0 && boundsCenter.maxX == 105.0);
    TEST_ASSERT(boundsCenter.width() == 110.0);

    auto boundsInside = AlignedStrokeMetrics::computeExpandedBounds(100.0, 100.0, 10.0, GraphicStrokeAlign::Inside);
    TEST_ASSERT(boundsInside.minX == 0.0 && boundsInside.maxX == 100.0);

    auto boundsOutside = AlignedStrokeMetrics::computeExpandedBounds(100.0, 100.0, 10.0, GraphicStrokeAlign::Outside);
    TEST_ASSERT(boundsOutside.minX == -10.0 && boundsOutside.maxX == 110.0);
    TEST_ASSERT(boundsOutside.width() == 120.0);

    std::cout << "[PASS] runGraphicGeometryAndStrokeTests" << std::endl;
}

void runInteractiveKeyframeCommandsTests() {
    using namespace catchim;
    using namespace catchim::core;
    using namespace catchim::editor;

    Timeline tl;
    TrackId mainId = tl.mainTrack().id();
    Clip clip(ClipId("kf-clip"), ClipType::Video, "Video Clip", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(10.0));
    tl.addClip(mainId, clip);

    // 1. UpsertKeyframeCommand
    UpsertKeyframeCommand upsert1(tl, mainId, ClipId("kf-clip"), "opacity", TimelineTime::fromSeconds(2.0), 0.5);
    bool ok = upsert1.execute();
    TEST_ASSERT(ok);

    Clip* c = tl.findClip(ClipId("kf-clip"));
    TEST_ASSERT(c != nullptr);
    TEST_ASSERT(c->hasAnimationChannel("opacity"));
    auto* chan = c->findAnimationChannel("opacity");
    TEST_ASSERT(chan->size() == 1);
    auto kfOpt = chan->findKeyframeAt(TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(kfOpt.has_value() && kfOpt->value == 0.5);

    // Undo UpsertKeyframeCommand
    ok = upsert1.undo();
    TEST_ASSERT(ok);
    TEST_ASSERT(!c->hasAnimationChannel("opacity"));

    // Redo / execute again
    ok = upsert1.execute();
    TEST_ASSERT(ok);
    TEST_ASSERT(c->hasAnimationChannel("opacity"));

    // Upsert a second keyframe at 4.0s
    UpsertKeyframeCommand upsert2(tl, mainId, ClipId("kf-clip"), "opacity", TimelineTime::fromSeconds(4.0), 1.0);
    upsert2.execute();
    chan = c->findAnimationChannel("opacity");
    TEST_ASSERT(chan->size() == 2);

    // 2. RetimeKeyframeCommand
    // Move keyframe from 2.0s to 3.0s
    RetimeKeyframeCommand retimeCmd(tl, mainId, ClipId("kf-clip"), "opacity", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(3.0));
    ok = retimeCmd.execute();
    TEST_ASSERT(ok);
    chan = c->findAnimationChannel("opacity");
    TEST_ASSERT(!chan->findKeyframeAt(TimelineTime::fromSeconds(2.0)).has_value());
    auto movedKf = chan->findKeyframeAt(TimelineTime::fromSeconds(3.0));
    TEST_ASSERT(movedKf.has_value() && movedKf->value == 0.5);

    // Undo Retime
    ok = retimeCmd.undo();
    TEST_ASSERT(ok);
    chan = c->findAnimationChannel("opacity");
    TEST_ASSERT(chan->findKeyframeAt(TimelineTime::fromSeconds(2.0)).has_value());
    TEST_ASSERT(!chan->findKeyframeAt(TimelineTime::fromSeconds(3.0)).has_value());

    // 3. UpdateKeyframeCurveCommand
    // Change keyframe at 4.0s to Bezier
    UpdateKeyframeCurveCommand curveCmd(
        tl, mainId, ClipId("kf-clip"), "opacity", TimelineTime::fromSeconds(4.0),
        KeyframeInterpolation::Bezier, {-0.3, 0.1}, {0.3, -0.1}, 0.4, 0.0, 0.2, 1.0
    );
    ok = curveCmd.execute();
    TEST_ASSERT(ok);
    chan = c->findAnimationChannel("opacity");
    auto kfCurve = chan->findKeyframeAt(TimelineTime::fromSeconds(4.0));
    TEST_ASSERT(kfCurve.has_value());
    TEST_ASSERT(kfCurve->interpolation == KeyframeInterpolation::Bezier);
    TEST_ASSERT(kfCurve->bezierX1 == 0.4);

    // Undo Curve update
    ok = curveCmd.undo();
    TEST_ASSERT(ok);
    chan = c->findAnimationChannel("opacity");
    kfCurve = chan->findKeyframeAt(TimelineTime::fromSeconds(4.0));
    TEST_ASSERT(kfCurve.has_value() && kfCurve->interpolation == KeyframeInterpolation::Linear);

    // 4. RemoveKeyframeCommand
    RemoveKeyframeCommand remKf(tl, mainId, ClipId("kf-clip"), "opacity", TimelineTime::fromSeconds(2.0));
    ok = remKf.execute();
    TEST_ASSERT(ok);
    chan = c->findAnimationChannel("opacity");
    TEST_ASSERT(chan->size() == 1);
    TEST_ASSERT(!chan->findKeyframeAt(TimelineTime::fromSeconds(2.0)).has_value());

    // Undo RemoveKeyframeCommand
    ok = remKf.undo();
    TEST_ASSERT(ok);
    chan = c->findAnimationChannel("opacity");
    TEST_ASSERT(chan->size() == 2);
    TEST_ASSERT(chan->findKeyframeAt(TimelineTime::fromSeconds(2.0)).has_value());

    // 5. Clamping test: upsert keyframe at 15.0s on a 10.0s clip
    UpsertKeyframeCommand clampUpsert(tl, mainId, ClipId("kf-clip"), "opacity", TimelineTime::fromSeconds(15.0), 0.8);
    clampUpsert.execute();
    chan = c->findAnimationChannel("opacity");
    auto clampedKf = chan->findKeyframeAt(TimelineTime::fromSeconds(10.0));
    TEST_ASSERT(clampedKf.has_value() && clampedKf->value == 0.8);

    std::cout << "[PASS] runInteractiveKeyframeCommandsTests" << std::endl;
}

void runTextLayoutAndTypographyEngineTests() {
    using namespace catchim;
    using namespace catchim::render;

    // 1. measureTextBlock
    std::vector<double> widths = {120.0, 250.0, 180.0};
    double lineHeight = 50.0;
    auto block = TextLayoutEngine::measureTextBlock(widths, lineHeight);
    TEST_ASSERT(block.lineCount == 3);
    TEST_ASSERT(block.maxWidth == 250.0);
    TEST_ASSERT(block.height == 150.0);
    // visualCenterOffset = ((3 - 1) * 50) / 2 = 50.0
    TEST_ASSERT(block.visualCenterOffset == 50.0);

    // 2. getTextRect
    auto leftRect = TextLayoutEngine::getTextRect(TextAlignment::Left, block);
    TEST_ASSERT(leftRect.left == 0.0);
    TEST_ASSERT(leftRect.top == -75.0);
    TEST_ASSERT(leftRect.width == 250.0);
    TEST_ASSERT(leftRect.height == 150.0);

    auto centerRect = TextLayoutEngine::getTextRect(TextAlignment::Center, block);
    TEST_ASSERT(centerRect.left == -125.0);
    TEST_ASSERT(centerRect.top == -75.0);

    auto rightRect = TextLayoutEngine::getTextRect(TextAlignment::Right, block);
    TEST_ASSERT(rightRect.left == -250.0);
    TEST_ASSERT(rightRect.top == -75.0);

    // 3. computeTextBackgroundRect
    auto bg = TextLayoutEngine::computeTextBackgroundRect(centerRect, 20.0, 10.0);
    TEST_ASSERT(bg.left == -145.0);
    TEST_ASSERT(bg.top == -85.0);
    TEST_ASSERT(bg.width == 290.0);
    TEST_ASSERT(bg.height == 170.0);

    // 4. wrapTextToLines
    std::string text = "Quick brown fox jumps over the lazy dog";
    auto lines = TextLayoutEngine::wrapTextToLines(text, 100.0, 10.0);
    TEST_ASSERT(lines.size() >= 4);
    for (const auto& l : lines) {
        TEST_ASSERT(!l.empty());
    }

    // Explicit newlines preservation
    std::string multiline = "Line 1\nLine 2\nLine 3";
    auto explicitLines = TextLayoutEngine::wrapTextToLines(multiline, 500.0, 10.0);
    TEST_ASSERT(explicitLines.size() == 3);
    TEST_ASSERT(explicitLines[0] == "Line 1");
    TEST_ASSERT(explicitLines[1] == "Line 2");
    TEST_ASSERT(explicitLines[2] == "Line 3");

    std::cout << "[PASS] runTextLayoutAndTypographyEngineTests" << std::endl;
}

void runFreeformMaskGeometryTests() {
    using namespace catchim;
    using namespace catchim::render;

    // 1. Serialization and Parsing
    std::vector<FreeformPathPoint> points = {
        {"p0", 10.0, 20.0, -5.0, 0.0, 5.0, 0.0},
        {"p1", 80.0, 90.0, 0.0, -8.0, 0.0, 8.0},
        {"p2", 40.0, 150.0, -6.0, 0.0, 6.0, 0.0}
    };

    std::string jsonStr = FreeformMaskGeometry::serializeFreeformPath(points);
    TEST_ASSERT(!jsonStr.empty());
    auto parsed = FreeformMaskGeometry::parseFreeformPath(jsonStr);
    TEST_ASSERT(parsed.size() == 3);
    TEST_ASSERT(parsed[0].id == "p0" && parsed[0].x == 10.0 && parsed[0].y == 20.0);
    TEST_ASSERT(parsed[1].id == "p1" && parsed[1].outY == 8.0);
    TEST_ASSERT(parsed[2].id == "p2" && parsed[2].inX == -6.0);

    // 2. Segment counts
    TEST_ASSERT(FreeformMaskGeometry::getFreeformSegmentCount(points, true) == 3);
    TEST_ASSERT(FreeformMaskGeometry::getFreeformSegmentCount(points, false) == 2);

    // 3. evaluateCubicBezier
    Vec2D p0{0.0, 0.0};
    Vec2D c0{0.0, 100.0};
    Vec2D c1{100.0, 100.0};
    Vec2D p1{100.0, 0.0};

    auto at0 = FreeformMaskGeometry::evaluateCubicBezier(p0, c0, c1, p1, 0.0);
    TEST_ASSERT(std::abs(at0.x - 0.0) < 0.001 && std::abs(at0.y - 0.0) < 0.001);

    auto at1 = FreeformMaskGeometry::evaluateCubicBezier(p0, c0, c1, p1, 1.0);
    TEST_ASSERT(std::abs(at1.x - 100.0) < 0.001 && std::abs(at1.y - 0.0) < 0.001);

    auto atHalf = FreeformMaskGeometry::evaluateCubicBezier(p0, c0, c1, p1, 0.5);
    TEST_ASSERT(std::abs(atHalf.x - 50.0) < 0.001);
    TEST_ASSERT(std::abs(atHalf.y - 75.0) < 0.001);

    // 4. insertPointOnSegment
    std::string newId = FreeformMaskGeometry::insertPointOnSegment(points, 0, 0.5, true);
    TEST_ASSERT(!newId.empty());
    TEST_ASSERT(points.size() == 4);
    TEST_ASSERT(points[1].id == newId);
    TEST_ASSERT(points[1].x > 10.0 && points[1].x < 80.0);

    // 5. recenterPath
    auto offset = FreeformMaskGeometry::recenterPath(points);
    TEST_ASSERT(offset.x != 0.0 || offset.y != 0.0);
    double minX = points[0].x, maxX = points[0].x;
    double minY = points[0].y, maxY = points[0].y;
    for (const auto& pt : points) {
        minX = std::min(minX, pt.x);
        maxX = std::max(maxX, pt.x);
        minY = std::min(minY, pt.y);
        maxY = std::max(maxY, pt.y);
    }
    TEST_ASSERT(std::abs((minX + maxX) / 2.0) < 0.001);
    TEST_ASSERT(std::abs((minY + maxY) / 2.0) < 0.001);

    std::cout << "[PASS] runFreeformMaskGeometryTests" << std::endl;
}

void runMaskCommandsAndFreeformTests() {
    using namespace catchim::core;
    using namespace catchim::editor;
    using namespace catchim::render;

    Timeline timeline;
    auto videoTrackId = timeline.mainTrack().id();

    // Create a video clip
    auto clipId = ClipId::generate();
    Clip clip(clipId, ClipType::Video, "HeroVideo", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(5.0));

    // Define initial 4-point freeform mask
    MaskInstance mask;
    mask.id = "mask_test_01";
    mask.type = "freeform";
    std::vector<FreeformPathPoint> initialPoints = {
        {"pt1", -100.0, -100.0, -20.0, 0.0, 20.0, 0.0},
        {"pt2", 100.0, -100.0, 0.0, -20.0, 0.0, 20.0},
        {"pt3", 100.0, 100.0, 20.0, 0.0, -20.0, 0.0},
        {"pt4", -100.0, 100.0, 0.0, 20.0, 0.0, -20.0}
    };
    mask.params["path"] = FreeformMaskGeometry::serializeFreeformPath(initialPoints);
    mask.params["closed"] = true;
    mask.params["inverted"] = false;

    clip.masks().push_back(mask);
    timeline.addClip(videoTrackId, std::move(clip));

    // 1. InsertCustomMaskPointCommand
    InsertCustomMaskPointCommand insertCmd(timeline, videoTrackId, clipId, "mask_test_01", 0, 0.5);
    TEST_ASSERT(insertCmd.execute());
    std::string newPtId = insertCmd.insertedPointId();
    TEST_ASSERT(!newPtId.empty());

    Clip* c = timeline.findClip(clipId);
    TEST_ASSERT(c != nullptr);
    auto currentPoints = FreeformMaskGeometry::parseFreeformPath(c->masks()[0].params["path"].get<std::string>());
    TEST_ASSERT(currentPoints.size() == 5);
    TEST_ASSERT(currentPoints[1].id == newPtId);

    // Test Undo
    TEST_ASSERT(insertCmd.undo());
    currentPoints = FreeformMaskGeometry::parseFreeformPath(c->masks()[0].params["path"].get<std::string>());
    TEST_ASSERT(currentPoints.size() == 4);

    // Re-execute to keep 5 points for deletion test
    TEST_ASSERT(insertCmd.execute());
    newPtId = insertCmd.insertedPointId();
    currentPoints = FreeformMaskGeometry::parseFreeformPath(c->masks()[0].params["path"].get<std::string>());
    TEST_ASSERT(currentPoints.size() == 5);

    // 2. DeleteCustomMaskPointsCommand
    // Delete 2 points: "pt2" and the newly inserted point
    DeleteCustomMaskPointsCommand deleteCmd(timeline, videoTrackId, clipId, "mask_test_01", {"pt2", newPtId});
    TEST_ASSERT(deleteCmd.execute());
    TEST_ASSERT(deleteCmd.didDelete());

    currentPoints = FreeformMaskGeometry::parseFreeformPath(c->masks()[0].params["path"].get<std::string>());
    TEST_ASSERT(currentPoints.size() == 3);
    // 3 points >= 3 => remains closed
    TEST_ASSERT(c->masks()[0].params["closed"].get<bool>() == true);

    // Delete another point so remaining is 2 (< 3 points => closed must become false)
    DeleteCustomMaskPointsCommand deleteCmd2(timeline, videoTrackId, clipId, "mask_test_01", {"pt3"});
    TEST_ASSERT(deleteCmd2.execute());
    currentPoints = FreeformMaskGeometry::parseFreeformPath(c->masks()[0].params["path"].get<std::string>());
    TEST_ASSERT(currentPoints.size() == 2);
    TEST_ASSERT(c->masks()[0].params["closed"].get<bool>() == false);

    // Undo deleteCmd2 -> restores to 3 points and closed = true
    TEST_ASSERT(deleteCmd2.undo());
    currentPoints = FreeformMaskGeometry::parseFreeformPath(c->masks()[0].params["path"].get<std::string>());
    TEST_ASSERT(currentPoints.size() == 3);
    TEST_ASSERT(c->masks()[0].params["closed"].get<bool>() == true);

    // 3. ToggleMaskInvertedCommand
    TEST_ASSERT(c->masks()[0].params["inverted"].get<bool>() == false);
    ToggleMaskInvertedCommand toggleCmd(timeline, videoTrackId, clipId, "mask_test_01");
    TEST_ASSERT(toggleCmd.execute());
    TEST_ASSERT(c->masks()[0].params["inverted"].get<bool>() == true);
    TEST_ASSERT(toggleCmd.undo());
    TEST_ASSERT(c->masks()[0].params["inverted"].get<bool>() == false);

    // 4. RemoveMaskCommand
    RemoveMaskCommand removeCmd(timeline, videoTrackId, clipId, "mask_test_01");
    TEST_ASSERT(removeCmd.execute());
    TEST_ASSERT(c->masks().empty());
    TEST_ASSERT(removeCmd.undo());
    TEST_ASSERT(c->masks().size() == 1);
    TEST_ASSERT(c->masks()[0].id == "mask_test_01");

    std::cout << "[PASS] runMaskCommandsAndFreeformTests" << std::endl;
}

void runEffectCommandsTests() {
    using namespace catchim::core;
    using namespace catchim::editor;

    Timeline timeline;
    auto videoTrackId = timeline.mainTrack().id();

    auto clipId = ClipId::generate();
    Clip clip(clipId, ClipType::Video, "EffectVideo", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(10.0));
    timeline.addClip(videoTrackId, std::move(clip));

    // 1. AddClipEffectCommand
    nlohmann::json blurParams = {{"intensity", 25.0}, {"direction", "both"}};
    AddClipEffectCommand addBlurCmd(timeline, videoTrackId, clipId, "blur", blurParams);
    TEST_ASSERT(addBlurCmd.execute());
    std::string blurId = addBlurCmd.createdEffectId();
    TEST_ASSERT(blurId.starts_with("eff_"));

    Clip* c = timeline.findClip(clipId);
    TEST_ASSERT(c != nullptr);
    TEST_ASSERT(c->effects().size() == 1);
    TEST_ASSERT(c->effects()[0].id == blurId);
    TEST_ASSERT(c->effects()[0].type == "blur");
    TEST_ASSERT(c->effects()[0].enabled == true);
    TEST_ASSERT(c->effects()[0].params["intensity"].get<double>() == 25.0);

    // Add a second effect: "glow"
    nlohmann::json glowParams = {{"radius", 12.0}, {"color", "#FF00AA"}};
    AddClipEffectCommand addGlowCmd(timeline, videoTrackId, clipId, "glow", glowParams);
    TEST_ASSERT(addGlowCmd.execute());
    std::string glowId = addGlowCmd.createdEffectId();
    TEST_ASSERT(c->effects().size() == 2);
    TEST_ASSERT(c->effects()[1].id == glowId);

    // 2. ToggleClipEffectCommand
    ToggleClipEffectCommand toggleCmd(timeline, videoTrackId, clipId, blurId);
    TEST_ASSERT(toggleCmd.execute());
    TEST_ASSERT(c->effects()[0].enabled == false);
    TEST_ASSERT(toggleCmd.undo());
    TEST_ASSERT(c->effects()[0].enabled == true);

    // 3. ReorderClipEffectsCommand (move index 0 to index 1)
    ReorderClipEffectsCommand reorderCmd(timeline, videoTrackId, clipId, 0, 1);
    TEST_ASSERT(reorderCmd.execute());
    TEST_ASSERT(c->effects()[0].id == glowId);
    TEST_ASSERT(c->effects()[1].id == blurId);
    TEST_ASSERT(reorderCmd.undo());
    TEST_ASSERT(c->effects()[0].id == blurId);
    TEST_ASSERT(c->effects()[1].id == glowId);

    // 4. UpdateClipEffectParamsCommand
    nlohmann::json patch = {{"intensity", 90.0}, {"newKey", "extra"}};
    UpdateClipEffectParamsCommand updateCmd(timeline, videoTrackId, clipId, blurId, patch);
    TEST_ASSERT(updateCmd.execute());
    TEST_ASSERT(c->effects()[0].params["intensity"].get<double>() == 90.0);
    TEST_ASSERT(c->effects()[0].params["newKey"].get<std::string>() == "extra");
    TEST_ASSERT(c->effects()[0].params["direction"].get<std::string>() == "both");
    TEST_ASSERT(updateCmd.undo());
    TEST_ASSERT(c->effects()[0].params["intensity"].get<double>() == 25.0);

    // 5. RemoveClipEffectCommand
    RemoveClipEffectCommand removeCmd(timeline, videoTrackId, clipId, glowId);
    TEST_ASSERT(removeCmd.execute());
    TEST_ASSERT(c->effects().size() == 1);
    TEST_ASSERT(c->effects()[0].id == blurId);
    TEST_ASSERT(removeCmd.undo());
    TEST_ASSERT(c->effects().size() == 2);

    // 6. ProjectSerializer Roundtrip Test with Masks & Effects
    MaskInstance sampleMask;
    sampleMask.id = "mask_serial_01";
    sampleMask.type = "rectangle";
    sampleMask.params = {{"width", 500.0}, {"height", 300.0}};
    c->masks().push_back(sampleMask);

    Project proj("EffectsProject");
    proj.activeScene()->timeline() = timeline;

    nlohmann::json projJson = ProjectSerializer::toJson(proj);
    auto res = ProjectSerializer::fromJson(projJson);
    TEST_ASSERT(res.ok());
    Project deserializedProj = std::move(res.value());

    const Clip* roundtripClip = deserializedProj.activeScene()->timeline().findClip(clipId);
    TEST_ASSERT(roundtripClip != nullptr);
    TEST_ASSERT(roundtripClip->effects().size() == 2);
    TEST_ASSERT(roundtripClip->effects()[0].id == blurId);
    TEST_ASSERT(roundtripClip->effects()[0].type == "blur");
    TEST_ASSERT(roundtripClip->effects()[1].id == glowId);
    TEST_ASSERT(roundtripClip->masks().size() == 1);
    TEST_ASSERT(roundtripClip->masks()[0].id == "mask_serial_01");
    TEST_ASSERT(roundtripClip->masks()[0].type == "rectangle");
    TEST_ASSERT(roundtripClip->masks()[0].params["width"].get<double>() == 500.0);

    std::cout << "[PASS] runEffectCommandsTests" << std::endl;
}

void runCanvasViewportControllerTests() {
    using namespace catchim::core;
    using namespace catchim::editor;
    using namespace catchim::render;

    // 1. Viewport Controller Initialization & Dimensions
    CanvasViewportController controller(1920.0, 1080.0, 960.0, 540.0);
    TEST_ASSERT(controller.canvasWidth() == 1920.0);
    TEST_ASSERT(controller.canvasHeight() == 1080.0);
    TEST_ASSERT(controller.viewportWidth() == 960.0);
    TEST_ASSERT(controller.viewportHeight() == 540.0);

    // Fit scale: 960 / 1920 = 0.5, 540 / 1080 = 0.5 => fitScale == 0.5
    TEST_ASSERT(std::abs(controller.fitScale() - 0.5) < 1e-6);
    TEST_ASSERT(std::abs(controller.viewportScale() - 0.5) < 1e-6);

    // 2. Zoom & Presets
    TEST_ASSERT(PREVIEW_ZOOM_PRESETS.size() == 6);
    TEST_ASSERT(PREVIEW_ZOOM_PRESETS[0] == 0.25);
    TEST_ASSERT(PREVIEW_ZOOM_PRESETS[3] == 1.00);

    controller.zoomIn(); // 1.0 * 1.25 = 1.25
    TEST_ASSERT(std::abs(controller.zoom() - 1.25) < 1e-6);
    TEST_ASSERT(std::abs(controller.viewportScale() - 0.625) < 1e-6);

    controller.zoomOut(); // 1.25 / 1.25 = 1.0
    TEST_ASSERT(std::abs(controller.zoom() - 1.0) < 1e-6);

    // Actual Size: 1.0 / fitScale = 1.0 / 0.5 = 2.0
    controller.setActualSize();
    TEST_ASSERT(std::abs(controller.zoom() - 2.0) < 1e-6);
    TEST_ASSERT(std::abs(controller.viewportScale() - 1.0) < 1e-6);

    // Viewport Percent: 50% => 0.5 / fitScale = 0.5 / 0.5 = 1.0
    controller.setViewportPercent(50.0);
    TEST_ASSERT(std::abs(controller.zoom() - 1.0) < 1e-6);

    // Zoom clamping
    controller.setZoom(100.0);
    TEST_ASSERT(controller.zoom() == PREVIEW_ZOOM_MAX); // 16.0
    controller.setZoom(0.001);
    TEST_ASSERT(controller.zoom() == PREVIEW_ZOOM_MIN); // 0.25

    // 3. Coordinate Transformations
    controller.fitToScreen();
    auto geom = controller.getGeometry();
    TEST_ASSERT(std::abs(geom.centerX - 960.0) < 1e-6);
    TEST_ASSERT(std::abs(geom.centerY - 540.0) < 1e-6);
    TEST_ASSERT(std::abs(geom.scale - 0.5) < 1e-6);

    // Screen center (480, 270) should map to canvas center (960, 540)
    Vec2D canvasCenter = controller.screenToCanvas(480.0, 270.0);
    TEST_ASSERT(std::abs(canvasCenter.x - 960.0) < 1e-4);
    TEST_ASSERT(std::abs(canvasCenter.y - 540.0) < 1e-4);

    // Canvas center (960, 540) maps to screen (480, 270)
    Vec2D screenPos = controller.canvasToOverlay(960.0, 540.0);
    TEST_ASSERT(std::abs(screenPos.x - 480.0) < 1e-4);
    TEST_ASSERT(std::abs(screenPos.y - 270.0) < 1e-4);

    // Canvas origin (0, 0) maps to screen origin (0, 0)
    Vec2D screenOrigin = controller.canvasToOverlay(0.0, 0.0);
    TEST_ASSERT(std::abs(screenOrigin.x - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(screenOrigin.y - 0.0) < 1e-4);

    // Position (relative to canvas center) (0, 0) maps to overlay center
    Vec2D overlayCenter = controller.positionToOverlay(0.0, 0.0);
    TEST_ASSERT(std::abs(overlayCenter.x - 480.0) < 1e-4);
    TEST_ASSERT(std::abs(overlayCenter.y - 270.0) < 1e-4);

    // Threshold conversion: 10 pixels on screen at scale 0.5 => 20 canvas units
    Vec2D thresh = CanvasViewportController::screenPixelsToLogicalThreshold(10.0, 0.5);
    TEST_ASSERT(std::abs(thresh.x - 20.0) < 1e-6);

    // 4. Panning & Center Clamping
    // When zoom <= 1.0, panning is ignored
    controller.panByScreenDelta(100.0, 100.0);
    TEST_ASSERT(std::abs(controller.center().x - 960.0) < 1e-6);
    TEST_ASSERT(std::abs(controller.center().y - 540.0) < 1e-6);

    // Zoom in to 2.0 (viewportScale = 1.0, visible span = 960x540)
    controller.setZoom(2.0);
    controller.panByScreenDelta(100.0, 50.0);
    TEST_ASSERT(std::abs(controller.center().x - 1060.0) < 1e-4);
    TEST_ASSERT(std::abs(controller.center().y - 590.0) < 1e-4);

    // Extreme pan is clamped within canvas boundaries
    controller.panByScreenDelta(5000.0, 5000.0);
    TEST_ASSERT(controller.center().x <= 1440.0 + 1e-4);
    TEST_ASSERT(controller.center().y <= 810.0 + 1e-4);

    // Reset pan
    controller.resetPan();
    TEST_ASSERT(std::abs(controller.center().x - 960.0) < 1e-6);
    TEST_ASSERT(std::abs(controller.center().y - 540.0) < 1e-6);

    // 5. ElementBounds & Transform Handles
    ElementBounds bounds{
        .cx = 500.0,
        .cy = 300.0,
        .width = 200.0,
        .height = 100.0,
        .rotation = 0.0
    };

    Vec2D tl = bounds.getCornerPosition(BoundsCorner::TopLeft);
    TEST_ASSERT(std::abs(tl.x - 400.0) < 1e-4);
    TEST_ASSERT(std::abs(tl.y - 250.0) < 1e-4);

    Vec2D br = bounds.getCornerPosition(BoundsCorner::BottomRight);
    TEST_ASSERT(std::abs(br.x - 600.0) < 1e-4);
    TEST_ASSERT(std::abs(br.y - 350.0) < 1e-4);

    Vec2D rightEdge = bounds.getEdgeHandlePosition(BoundsEdge::Right);
    TEST_ASSERT(std::abs(rightEdge.x - 600.0) < 1e-4);
    TEST_ASSERT(std::abs(rightEdge.y - 300.0) < 1e-4);

    // Rotate 90 degrees clockwise
    bounds.rotation = 90.0;
    Vec2D tlRot = bounds.getCornerPosition(BoundsCorner::TopLeft);
    TEST_ASSERT(std::abs(tlRot.x - 550.0) < 1e-4);
    TEST_ASSERT(std::abs(tlRot.y - 200.0) < 1e-4);

    std::cout << "[PASS] runCanvasViewportControllerTests" << std::endl;
}

void runMultiElementCommandsTests() {
    using namespace catchim::core;
    using namespace catchim::editor;

    Timeline timeline;
    auto mainTrackId = timeline.mainTrack().id();
    auto overlayTrackId = timeline.addTrack(TrackType::Video, "Overlay").id();

    // Add 2 clips to Main Track
    auto c1Id = ClipId::generate();
    Clip clip1(c1Id, ClipType::Video, "Clip1", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(4.0));
    timeline.addClip(mainTrackId, std::move(clip1));

    auto c2Id = ClipId::generate();
    Clip clip2(c2Id, ClipType::Video, "Clip2", TimelineTime::fromSeconds(4.0), TimelineTime::fromSeconds(4.0));
    timeline.addClip(mainTrackId, std::move(clip2));

    // Add 2 clips to Overlay Track
    auto c3Id = ClipId::generate();
    Clip clip3(c3Id, ClipType::Video, "Clip3", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(3.0));
    timeline.addClip(overlayTrackId, std::move(clip3));

    auto c4Id = ClipId::generate();
    Clip clip4(c4Id, ClipType::Video, "Clip4", TimelineTime::fromSeconds(5.0), TimelineTime::fromSeconds(3.0));
    timeline.addClip(overlayTrackId, std::move(clip4));

    // 1. UpdateElementsCommand (batch property updates on clip1 and clip3)
    std::vector<ElementPatch> patches = {
        ElementPatch(mainTrackId, c1Id, nlohmann::json{{"opacity", 0.5}, {"transform.positionX", 120.0}}),
        ElementPatch(overlayTrackId, c3Id, nlohmann::json{{"opacity", 0.75}, {"transform.positionY", -40.0}})
    };
    UpdateElementsCommand updateCmd(timeline, patches);
    TEST_ASSERT(updateCmd.execute());

    Clip* c1 = timeline.findClip(c1Id);
    Clip* c3 = timeline.findClip(c3Id);
    TEST_ASSERT(c1 != nullptr && c3 != nullptr);
    TEST_ASSERT(c1->params()["opacity"].get<double>() == 0.5);
    TEST_ASSERT(c1->params()["transform.positionX"].get<double>() == 120.0);
    TEST_ASSERT(c3->params()["opacity"].get<double>() == 0.75);
    TEST_ASSERT(c3->params()["transform.positionY"].get<double>() == -40.0);

    // Test Undo of UpdateElementsCommand
    TEST_ASSERT(updateCmd.undo());
    c1 = timeline.findClip(c1Id);
    c3 = timeline.findClip(c3Id);
    TEST_ASSERT(c1->params()["opacity"].get<double>() == 1.0);
    TEST_ASSERT(c1->params()["transform.positionX"].get<double>() == 0.0);
    TEST_ASSERT(c3->params()["opacity"].get<double>() == 1.0);
    TEST_ASSERT(c3->params()["transform.positionY"].get<double>() == 0.0);

    // Re-execute
    TEST_ASSERT(updateCmd.execute());
    c1 = timeline.findClip(c1Id);
    TEST_ASSERT(c1->params()["opacity"].get<double>() == 0.5);

    // 2. DeleteElementsCommand (batch delete c1 on main track and c4 on overlay track)
    std::vector<ElementLocation> toDelete = {
        ElementLocation(mainTrackId, c1Id),
        ElementLocation(overlayTrackId, c4Id)
    };
    DeleteElementsCommand deleteCmd(timeline, toDelete);
    TEST_ASSERT(deleteCmd.execute());

    TEST_ASSERT(timeline.findClip(c1Id) == nullptr);
    TEST_ASSERT(timeline.findClip(c4Id) == nullptr);
    TEST_ASSERT(timeline.findClip(c2Id) != nullptr);
    TEST_ASSERT(timeline.findClip(c3Id) != nullptr);

    // Test Undo of DeleteElementsCommand
    TEST_ASSERT(deleteCmd.undo());
    c1 = timeline.findClip(c1Id);
    Clip* c4 = timeline.findClip(c4Id);
    TEST_ASSERT(c1 != nullptr);
    TEST_ASSERT(c4 != nullptr);
    TEST_ASSERT(c1->params()["opacity"].get<double>() == 0.5);
    TEST_ASSERT(c4->name() == "Clip4");

    std::cout << "[PASS] runMultiElementCommandsTests" << std::endl;
}

void runEffectParamKeyframingTests() {
    using namespace catchim::core;
    using namespace catchim::editor;

    // 1. Path helpers
    std::string path = EffectParamAnimationEngine::buildEffectParamPath("eff_01", "intensity");
    TEST_ASSERT(path == "effects.eff_01.params.intensity");
    TEST_ASSERT(EffectParamAnimationEngine::isEffectParamPath(path));
    TEST_ASSERT(!EffectParamAnimationEngine::isEffectParamPath("transform.positionX"));

    auto parsed = EffectParamAnimationEngine::parseEffectParamPath(path);
    TEST_ASSERT(parsed.has_value());
    TEST_ASSERT(parsed->first == "eff_01");
    TEST_ASSERT(parsed->second == "intensity");

    // 2. Setup Timeline with Clip and Effect
    Timeline timeline;
    auto videoTrackId = timeline.mainTrack().id();

    auto clipId = ClipId::generate();
    Clip clip(clipId, ClipType::Video, "EffectAnimClip", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(5.0));

    EffectInstance eff;
    eff.id = "eff_01";
    eff.type = "blur";
    eff.params = {{"intensity", 10.0}, {"radius", 5.0}};
    clip.effects().push_back(eff);
    timeline.addClip(videoTrackId, std::move(clip));

    // 3. UpsertEffectParamKeyframeCommand
    // Keyframe at 0.0s: value = 0.0
    UpsertEffectParamKeyframeCommand kf0(timeline, videoTrackId, clipId, "eff_01", "intensity",
                                         TimelineTime::fromSeconds(0.0), 0.0);
    TEST_ASSERT(kf0.execute());

    // Keyframe at 2.0s: value = 50.0
    UpsertEffectParamKeyframeCommand kf1(timeline, videoTrackId, clipId, "eff_01", "intensity",
                                         TimelineTime::fromSeconds(2.0), 50.0);
    TEST_ASSERT(kf1.execute());

    // Keyframe at 4.0s: value = 100.0
    UpsertEffectParamKeyframeCommand kf2(timeline, videoTrackId, clipId, "eff_01", "intensity",
                                         TimelineTime::fromSeconds(4.0), 100.0);
    TEST_ASSERT(kf2.execute());

    Clip* c = timeline.findClip(clipId);
    TEST_ASSERT(c != nullptr);

    // 4. Time resolution
    auto p0 = EffectParamAnimationEngine::resolveEffectParamsAtTime(*c, "eff_01", TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(std::abs(p0["intensity"].get<double>() - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(p0["radius"].get<double>() - 5.0) < 1e-4);

    auto p1 = EffectParamAnimationEngine::resolveEffectParamsAtTime(*c, "eff_01", TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(std::abs(p1["intensity"].get<double>() - 25.0) < 1e-4);

    auto p2 = EffectParamAnimationEngine::resolveEffectParamsAtTime(*c, "eff_01", TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(std::abs(p2["intensity"].get<double>() - 50.0) < 1e-4);

    auto p3 = EffectParamAnimationEngine::resolveEffectParamsAtTime(*c, "eff_01", TimelineTime::fromSeconds(3.0));
    TEST_ASSERT(std::abs(p3["intensity"].get<double>() - 75.0) < 1e-4);

    auto p4 = EffectParamAnimationEngine::resolveEffectParamsAtTime(*c, "eff_01", TimelineTime::fromSeconds(4.0));
    TEST_ASSERT(std::abs(p4["intensity"].get<double>() - 100.0) < 1e-4);

    // 5. RemoveEffectParamKeyframeCommand
    RemoveEffectParamKeyframeCommand remKf(timeline, videoTrackId, clipId, "eff_01", "intensity",
                                           TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(remKf.execute());

    auto p2After = EffectParamAnimationEngine::resolveEffectParamsAtTime(*c, "eff_01", TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(std::abs(p2After["intensity"].get<double>() - 50.0) < 1e-4);

    // Undo removal
    TEST_ASSERT(remKf.undo());

    // Undo kf2
    TEST_ASSERT(kf2.undo());
    auto p4AfterUndo = EffectParamAnimationEngine::resolveEffectParamsAtTime(*c, "eff_01", TimelineTime::fromSeconds(4.0));
    TEST_ASSERT(std::abs(p4AfterUndo["intensity"].get<double>() - 50.0) < 1e-4);

    std::cout << "[PASS] runEffectParamKeyframingTests" << std::endl;
}

void runTransformHandleSessionTests() {
    using namespace catchim::core;
    using namespace catchim::editor;
    using namespace catchim::render;

    TransformHandleSession session;
    TEST_ASSERT(!session.isActive());
    TEST_ASSERT(session.kind() == HandleSessionKind::Idle);

    ElementBounds bounds{
        .cx = 500.0,
        .cy = 300.0,
        .width = 200.0,
        .height = 100.0,
        .rotation = 0.0
    };

    Transform initTransform;
    initTransform.scaleX = 1.0;
    initTransform.scaleY = 1.0;
    initTransform.rotate = 0.0;
    initTransform.positionX = 0.0;
    initTransform.positionY = 0.0;

    // 1. Corner Scale Session
    session.startCornerScale(BoundsCorner::TopRight, Vec2D{600.0, 250.0}, bounds, initTransform);
    TEST_ASSERT(session.isActive());
    TEST_ASSERT(session.kind() == HandleSessionKind::CornerScale);

    auto scaleRes = session.update(Vec2D{700.0, 200.0});
    TEST_ASSERT(std::abs(scaleRes.transform.scaleX - 2.0) < 1e-4);
    TEST_ASSERT(std::abs(scaleRes.transform.scaleY - 2.0) < 1e-4);

    auto snapScaleRes = session.update(Vec2D{601.0, 249.5});
    TEST_ASSERT(snapScaleRes.isSnapped);
    TEST_ASSERT(snapScaleRes.transform.scaleX == 1.0);
    TEST_ASSERT(snapScaleRes.transform.scaleY == 1.0);

    // 2. Edge Scale Session
    session.reset();
    TEST_ASSERT(!session.isActive());

    session.startEdgeScale(BoundsEdge::Right, Vec2D{600.0, 300.0}, bounds, initTransform);
    TEST_ASSERT(session.kind() == HandleSessionKind::EdgeScale);

    auto edgeRes = session.update(Vec2D{750.0, 300.0});
    TEST_ASSERT(std::abs(edgeRes.transform.scaleX - 2.5) < 1e-4);
    TEST_ASSERT(edgeRes.transform.scaleY == 1.0);

    // 3. Rotation Session
    session.reset();
    session.startRotation(Vec2D{500.0, 200.0}, bounds, initTransform);
    TEST_ASSERT(session.kind() == HandleSessionKind::Rotation);

    auto rotRes = session.update(Vec2D{600.0, 300.0});
    TEST_ASSERT(rotRes.isSnapped);
    TEST_ASSERT(std::abs(rotRes.transform.rotate - 90.0) < 1e-4);

    auto patch = rotRes.toPatchParams();
    TEST_ASSERT(patch["transform.rotate"].get<double>() == 90.0);

    std::cout << "[PASS] runTransformHandleSessionTests" << std::endl;
}

void runSplitElementsCommandTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    Timeline timeline;
    auto videoTrackId = timeline.mainTrack().id();
    auto audioTrackId = timeline.addTrack(TrackType::Audio, "Audio").id();

    // Setup Video Clip on Main Track: [1.0s, 5.0s] (duration 4.0s)
    auto clipId1 = ClipId::generate();
    Clip clip1(clipId1, ClipType::Video, "VideoClip", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(4.0));
    clip1.setTrimStart(TimelineTime::fromSeconds(0.5));
    clip1.setTrimEnd(TimelineTime::fromSeconds(0.5));

    // Add animation channel to clip1 with 2 keyframes at t=1.0s and t=3.0s relative
    AnimationChannel chanX("transform.positionX", 0.0);
    Keyframe kf1;
    kf1.time = TimelineTime::fromSeconds(1.0);
    kf1.value = 100.0;
    chanX.addOrUpdateKeyframe(kf1);
    Keyframe kf2;
    kf2.time = TimelineTime::fromSeconds(3.0);
    kf2.value = 300.0;
    chanX.addOrUpdateKeyframe(kf2);
    clip1.animationChannels()["transform.positionX"] = chanX;

    // Retime: rate = 2.0
    nlohmann::json rParams;
    rParams["rate"] = 2.0;
    clip1.setParam("retime", rParams);

    timeline.addClip(videoTrackId, std::move(clip1));

    // Setup Audio Clip on Audio Track: [1.0s, 5.0s] (duration 4.0s)
    auto clipId2 = ClipId::generate();
    Clip clip2(clipId2, ClipType::Audio, "AudioClip", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(4.0));
    timeline.addClip(audioTrackId, std::move(clip2));

    // 1. SplitElementsCommand with RetainSide::Both at splitTime = 2.5s (relative = 1.5s)
    TimelineTime splitTime = TimelineTime::fromSeconds(2.5);
    std::vector<ElementLocation> targets = {
        ElementLocation{videoTrackId, clipId1},
        ElementLocation{audioTrackId, clipId2}
    };

    SplitElementsCommand splitCmd(timeline, targets, splitTime, RetainSide::Both);
    bool ok = splitCmd.execute();
    TEST_ASSERT(ok);
    TEST_ASSERT(splitCmd.rightSideElements().size() == 2);

    // Verify Main Track has 2 clips now
    auto& mainClips = timeline.mainTrack().clips();
    TEST_ASSERT(mainClips.size() == 2);
    const auto& leftV = mainClips[0];
    const auto& rightV = mainClips[1];

    TEST_ASSERT(leftV.id() == clipId1);
    TEST_ASSERT(leftV.name() == "VideoClip (left)");
    TEST_ASSERT(leftV.startTime() == TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(leftV.duration() == TimelineTime::fromSeconds(1.5));

    TEST_ASSERT(rightV.id() != clipId1);
    TEST_ASSERT(rightV.name() == "VideoClip (right)");
    TEST_ASSERT(rightV.startTime() == TimelineTime::fromSeconds(2.5));
    TEST_ASSERT(rightV.duration() == TimelineTime::fromSeconds(2.5));

    // Retime source spans: at rate 2.0, leftSourceSpan = 1.5 * 2.0 = 3.0s, total = 4.0 * 2.0 = 8.0s, rightSourceSpan = 5.0s
    // leftTrimEnd was 0.5 + 5.0 = 5.5s
    // rightTrimStart was 0.5 + 3.0 = 3.5s
    TEST_ASSERT(leftV.trimEnd() == TimelineTime::fromSeconds(5.5));
    TEST_ASSERT(rightV.trimStart() == TimelineTime::fromSeconds(3.5));

    // Verify Animation splitting on left and right clips:
    // Left channel: kf at 1.0s (100.0), boundary kf at 1.5s (interpolated = 150.0)
    TEST_ASSERT(leftV.animationChannels().count("transform.positionX") == 1);
    const auto& leftChan = leftV.animationChannels().at("transform.positionX");
    TEST_ASSERT(leftChan.size() == 2);
    TEST_ASSERT(leftChan.keyframes()[0].time == TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(leftChan.keyframes()[1].time == TimelineTime::fromSeconds(1.5));
    TEST_ASSERT(std::abs(leftChan.keyframes()[1].value - 150.0) < 1e-4);

    // Right channel: boundary kf at 0.0s (150.0), shifted kf at 3.0 - 1.5 = 1.5s (300.0)
    TEST_ASSERT(rightV.animationChannels().count("transform.positionX") == 1);
    const auto& rightChan = rightV.animationChannels().at("transform.positionX");
    TEST_ASSERT(rightChan.size() == 2);
    TEST_ASSERT(rightChan.keyframes()[0].time == TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(std::abs(rightChan.keyframes()[0].value - 150.0) < 1e-4);
    TEST_ASSERT(rightChan.keyframes()[1].time == TimelineTime::fromSeconds(1.5));
    TEST_ASSERT(std::abs(rightChan.keyframes()[1].value - 300.0) < 1e-4);

    // 2. Undo restoration
    TEST_ASSERT(splitCmd.undo());
    TEST_ASSERT(timeline.mainTrack().clips().size() == 1);
    TEST_ASSERT(timeline.mainTrack().clips()[0].id() == clipId1);
    TEST_ASSERT(timeline.mainTrack().clips()[0].duration() == TimelineTime::fromSeconds(4.0));
    TEST_ASSERT(timeline.audioTracks().front().clips().size() == 1);

    // 3. Test RetainSide::Left
    SplitElementsCommand splitLeftCmd(timeline, targets, splitTime, RetainSide::Left);
    TEST_ASSERT(splitLeftCmd.execute());
    TEST_ASSERT(timeline.mainTrack().clips().size() == 1);
    TEST_ASSERT(timeline.mainTrack().clips()[0].duration() == TimelineTime::fromSeconds(1.5));
    TEST_ASSERT(splitLeftCmd.rightSideElements().empty());

    // Undo RetainSide::Left
    TEST_ASSERT(splitLeftCmd.undo());
    TEST_ASSERT(timeline.mainTrack().clips()[0].duration() == TimelineTime::fromSeconds(4.0));

    // 4. Test RetainSide::Right
    SplitElementsCommand splitRightCmd(timeline, targets, splitTime, RetainSide::Right);
    TEST_ASSERT(splitRightCmd.execute());
    TEST_ASSERT(timeline.mainTrack().clips().size() == 1);
    TEST_ASSERT(timeline.mainTrack().clips()[0].startTime() == TimelineTime::fromSeconds(2.5));
    TEST_ASSERT(timeline.mainTrack().clips()[0].duration() == TimelineTime::fromSeconds(2.5));
    TEST_ASSERT(splitRightCmd.rightSideElements().size() == 2);

    std::cout << "[PASS] runSplitElementsCommandTests" << std::endl;
}

void runPreviewInteractionEngineTests() {
    using namespace catchim::render;
    using namespace catchim::core;
    using namespace catchim::editor;

    // 1. Point in Rotated Rect
    // Center (0, 0), w = 100, h = 60, rotation = 0 deg
    TEST_ASSERT(PreviewInteractionEngine::pointInRotatedRect(0.0, 0.0, 0.0, 0.0, 100.0, 60.0, 0.0));
    TEST_ASSERT(PreviewInteractionEngine::pointInRotatedRect(45.0, 25.0, 0.0, 0.0, 100.0, 60.0, 0.0));
    TEST_ASSERT(!PreviewInteractionEngine::pointInRotatedRect(55.0, 25.0, 0.0, 0.0, 100.0, 60.0, 0.0));
    // Rotated 90 deg: width along Y, height along X
    TEST_ASSERT(PreviewInteractionEngine::pointInRotatedRect(25.0, 45.0, 0.0, 0.0, 100.0, 60.0, 90.0));
    TEST_ASSERT(!PreviewInteractionEngine::pointInRotatedRect(35.0, 45.0, 0.0, 0.0, 100.0, 60.0, 90.0));

    // 2. getHitElements and resolvePreferredHit
    TrackId trackMain = TrackId::generate();
    TrackId trackOverlay = TrackId::generate();
    ClipId clipMain = ClipId::generate();
    ClipId clipOverlay = ClipId::generate();
    ClipId clipText = ClipId::generate();

    VisualElementBounds bottomElem;
    bottomElem.trackId = trackMain;
    bottomElem.clipId = clipMain;
    bottomElem.clipType = "video";
    bottomElem.cx = 0.0;
    bottomElem.cy = 0.0;
    bottomElem.width = 200.0;
    bottomElem.height = 200.0;
    bottomElem.initialTransform.positionX = 0.0;
    bottomElem.initialTransform.positionY = 0.0;

    VisualElementBounds topElem;
    topElem.trackId = trackOverlay;
    topElem.clipId = clipOverlay;
    topElem.clipType = "sticker";
    topElem.cx = 20.0;
    topElem.cy = 20.0;
    topElem.width = 100.0;
    topElem.height = 100.0;
    topElem.initialTransform.positionX = 20.0;
    topElem.initialTransform.positionY = 20.0;

    VisualElementBounds textElem;
    textElem.trackId = trackOverlay;
    textElem.clipId = clipText;
    textElem.clipType = "text";
    textElem.cx = -150.0;
    textElem.cy = -150.0;
    textElem.width = 80.0;
    textElem.height = 40.0;
    textElem.initialParams["text"] = "Hello Catchim";

    std::vector<VisualElementBounds> visible = { bottomElem, topElem, textElem };

    // Point (20, 20) is inside both bottomElem and topElem
    auto hits = PreviewInteractionEngine::getHitElements(Point2D{20.0, 20.0}, visible);
    TEST_ASSERT(hits.size() == 2);
    // Reverse order: topmost rendered last (topElem) appears first
    TEST_ASSERT(hits[0].clipId == clipOverlay);
    TEST_ASSERT(hits[1].clipId == clipMain);

    // Preferred hit: if bottomElem is currently selected, resolvePreferredHit returns it
    std::vector<ElementLocation> sel = { ElementLocation{trackMain, clipMain} };
    const auto* pref = PreviewInteractionEngine::resolvePreferredHit(hits, sel);
    TEST_ASSERT(pref != nullptr);
    TEST_ASSERT(pref->clipId == clipMain);

    // 3. Pointer Gestures (Pending -> Dragging -> Snapping -> Commit)
    PreviewInteractionEngine engine;
    engine.onPointerDown(Point2D{20.0, 20.0}, visible, sel);
    TEST_ASSERT(engine.isPending());

    // Small move (< 0.5) stays pending
    engine.onPointerMove(Point2D{20.3, 20.2}, false, Size2D{1920.0, 1080.0});
    TEST_ASSERT(engine.isPending());

    // Move past threshold -> transitions to Dragging!
    engine.onPointerMove(Point2D{25.0, 30.0}, false, Size2D{1920.0, 1080.0});
    TEST_ASSERT(engine.isDragging());
    TEST_ASSERT(!engine.activePatches().empty());

    // Snapping test: move close to canvas center (x = 0)
    engine.onPointerMove(Point2D{22.0, 50.0}, false, Size2D{1920.0, 1080.0});
    bool hasCenterSnap = false;
    for (const auto& line : engine.activeSnapLines()) {
        if (line.type == SnapLineType::Vertical && std::abs(line.position) < 1e-3) {
            hasCenterSnap = true;
        }
    }
    TEST_ASSERT(hasCenterSnap);

    // Releasing pointer commits
    engine.onPointerUp(false);
    TEST_ASSERT(engine.gestureKind() == PreviewGestureKind::Idle);

    // 4. Click without drag updates selection
    engine.onPointerDown(Point2D{20.0, 20.0}, visible, {});
    engine.onPointerUp(false); // No move
    TEST_ASSERT(engine.selection().size() == 1);
    TEST_ASSERT(engine.selection()[0].clipId == clipOverlay);

    // Click in empty space clears selection
    engine.onPointerDown(Point2D{500.0, 500.0}, visible, engine.selection());
    engine.onPointerUp(false);
    TEST_ASSERT(engine.selection().empty());

    // 5. Double-click on text opens text editing state
    bool dbOk = engine.onDoubleClick(Point2D{-150.0, -150.0}, visible);
    TEST_ASSERT(dbOk);
    TEST_ASSERT(engine.textEditingState().has_value());
    TEST_ASSERT(engine.textEditingState()->textContent == "Hello Catchim");

    std::cout << "[PASS] runPreviewInteractionEngineTests" << std::endl;
}

void runAudioRetimeEngineTests() {
    using namespace catchim::audio;

    // 1. Rate constraints
    TEST_ASSERT(AudioRetimeEngine::clampRate(0.0001) == 0.01);
    TEST_ASSERT(AudioRetimeEngine::clampRate(99.0) == 5.0);
    TEST_ASSERT(AudioRetimeEngine::clampRate(1.5) == 1.5);

    TEST_ASSERT(AudioRetimeEngine::canMaintainPitch(0.25));
    TEST_ASSERT(AudioRetimeEngine::canMaintainPitch(4.0));
    TEST_ASSERT(!AudioRetimeEngine::canMaintainPitch(0.1));
    TEST_ASSERT(!AudioRetimeEngine::canMaintainPitch(5.0));

    TEST_ASSERT(AudioRetimeEngine::shouldMaintainPitch(1.5, true));
    TEST_ASSERT(!AudioRetimeEngine::shouldMaintainPitch(1.5, false));
    TEST_ASSERT(!AudioRetimeEngine::shouldMaintainPitch(0.1, true));

    // 2. Generate 1.0s Stereo AudioBuffer with 440 Hz Sine Wave at 44100 Hz
    constexpr int32_t SR = 44100;
    AudioBuffer src(2, SR);
    src.resize(SR); // 1 second
    for (size_t i = 0; i < static_cast<size_t>(SR); ++i) {
        float val = std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / static_cast<float>(SR));
        src.samples()[i * 2 + 0] = val * 0.8f;
        src.samples()[i * 2 + 1] = val * 0.8f;
    }

    // 3. Resampled buffer (without pitch preservation): 2x speed, duration 0.5s
    AudioBuffer resampled = AudioRetimeEngine::renderResampledBuffer(src, 0.0, 0.5, 2.0, SR);
    TEST_ASSERT(resampled.frameCount() == static_cast<size_t>(SR * 0.5));
    TEST_ASSERT(resampled.channels() == 2);
    float maxVal = 0.0f;
    for (float s : resampled.samples()) {
        maxVal = std::max(maxVal, std::abs(s));
    }
    TEST_ASSERT(maxVal > 0.5f);

    // 4. Pitch-Preserved Buffer via WSOLA:
    // Retime rate = 0.5 (slow motion 2x duration = 2.0s)
    AudioBuffer stretched = AudioRetimeEngine::renderPitchPreservedBuffer(src, 0.0, 2.0, 0.5, SR);
    TEST_ASSERT(stretched.frameCount() == static_cast<size_t>(SR * 2.0));
    TEST_ASSERT(stretched.channels() == 2);

    // Verify samples are bounded within [-1.0, 1.0] and non-empty
    float maxStretched = 0.0f;
    bool hasNaN = false;
    for (float s : stretched.samples()) {
        if (std::isnan(s) || std::isinf(s)) hasNaN = true;
        maxStretched = std::max(maxStretched, std::abs(s));
    }
    TEST_ASSERT(!hasNaN);
    TEST_ASSERT(maxStretched > 0.3f && maxStretched <= 1.0f);

    // Stereo synchronization check: left and right channels are identical for mono-in
    bool stereoSync = true;
    for (size_t i = 0; i < stretched.frameCount(); ++i) {
        if (std::abs(stretched.samples()[i * 2 + 0] - stretched.samples()[i * 2 + 1]) > 1e-4f) {
            stereoSync = false;
            break;
        }
    }
    TEST_ASSERT(stereoSync);

    // 5. Retimed buffer high-level dispatcher
    AudioBuffer retimedPitch = AudioRetimeEngine::renderRetimedBuffer(src, 0.0, 1.0, 1.5, true, SR);
    TEST_ASSERT(retimedPitch.frameCount() == static_cast<size_t>(SR));

    AudioBuffer retimedNoPitch = AudioRetimeEngine::renderRetimedBuffer(src, 0.0, 1.0, 1.5, false, SR);
    TEST_ASSERT(retimedNoPitch.frameCount() == static_cast<size_t>(SR));

    std::cout << "[PASS] runAudioRetimeEngineTests" << std::endl;
}

void runMoveElementsCommandTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    Timeline timeline;
    auto videoTrackId = timeline.mainTrack().id();
    auto overlayTrackId = timeline.addTrack(TrackType::Video, "Overlay 1").id();

    auto clipId1 = ClipId::generate();
    Clip clip1(clipId1, ClipType::Video, "Clip 1", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(2.0));
    timeline.addClip(videoTrackId, std::move(clip1));

    auto clipId2 = ClipId::generate();
    Clip clip2(clipId2, ClipType::Video, "Clip 2", TimelineTime::fromSeconds(3.0), TimelineTime::fromSeconds(2.0));
    timeline.addClip(overlayTrackId, std::move(clip2));

    // Move Clip 1 from Main to Overlay at 5.0s
    // Move Clip 2 from Overlay to dynamically created New Track at 2.0s
    TrackId newTrackId = TrackId::generate();
    std::vector<PlannedClipMove> moves = {
        PlannedClipMove{videoTrackId, overlayTrackId, clipId1, TimelineTime::fromSeconds(5.0)},
        PlannedClipMove{overlayTrackId, newTrackId, clipId2, TimelineTime::fromSeconds(2.0)}
    };

    std::vector<PlannedTrackCreation> createTracks = {
        PlannedTrackCreation{newTrackId, TrackType::Video, 0}
    };

    MoveElementsCommand moveCmd(timeline, moves, createTracks);
    TEST_ASSERT(moveCmd.execute());

    // Verify new track was created
    Track* createdTrack = timeline.findTrack(newTrackId);
    TEST_ASSERT(createdTrack != nullptr);
    TEST_ASSERT(createdTrack->type() == TrackType::Video);

    // Verify Clip 1 is on overlayTrackId at 5.0s
    Track* overlayTrack = timeline.findTrack(overlayTrackId);
    TEST_ASSERT(overlayTrack != nullptr);
    const Clip* moved1 = overlayTrack->findClip(clipId1);
    TEST_ASSERT(moved1 != nullptr);
    TEST_ASSERT(moved1->startTime() == TimelineTime::fromSeconds(5.0));

    // Verify Clip 2 is on createdTrack at 2.0s
    const Clip* moved2 = createdTrack->findClip(clipId2);
    TEST_ASSERT(moved2 != nullptr);
    TEST_ASSERT(moved2->startTime() == TimelineTime::fromSeconds(2.0));

    // Verify Main Track has 0 clips
    TEST_ASSERT(timeline.mainTrack().clips().empty());

    // Test incompatible move: try to move an Audio clip onto a Video track
    TrackId audioTrackId = timeline.addTrack(TrackType::Audio, "Audio").id();
    auto audioClipId = ClipId::generate();
    Clip aClip(audioClipId, ClipType::Audio, "Audio Clip", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(1.0));
    timeline.addClip(audioTrackId, std::move(aClip));

    std::vector<PlannedClipMove> invalidMoves = {
        PlannedClipMove{audioTrackId, videoTrackId, audioClipId, TimelineTime::fromSeconds(0.0)}
    };
    MoveElementsCommand invalidCmd(timeline, invalidMoves);
    TEST_ASSERT(!invalidCmd.execute()); // Rejects incompatible move

    // Test Undo of the valid move
    TEST_ASSERT(moveCmd.undo());
    TEST_ASSERT(timeline.mainTrack().clips().size() == 1);
    TEST_ASSERT(timeline.mainTrack().clips()[0].id() == clipId1);
    TEST_ASSERT(timeline.mainTrack().clips()[0].startTime() == TimelineTime::fromSeconds(1.0));

    std::cout << "[PASS] runMoveElementsCommandTests" << std::endl;
}

void runInsertElementCommandTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    Project project("Test Project");
    TEST_ASSERT(project.settings().canvasSize.width == 1920);
    TEST_ASSERT(project.settings().canvasSize.height == 1080);
    TEST_ASSERT(project.settings().fps.numerator == 30);

    Timeline timeline;

    // 1. Insert first visual media element into empty timeline -> should auto-fit canvas resolution and fps
    auto clipId1 = ClipId::generate();
    Clip clip1(clipId1, ClipType::Video, "4K 60fps Video", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(5.0));
    clip1.setParam("width", 3840);
    clip1.setParam("height", 2160);
    clip1.setParam("fps", 60.0);

    InsertElementPlacement autoPlacement;
    autoPlacement.mode = InsertElementPlacement::Mode::Auto;
    autoPlacement.autoTrackType = TrackType::Video;
    autoPlacement.startTime = TimelineTime::fromSeconds(0.0);

    InsertElementCommand insertCmd1(timeline, clip1, autoPlacement, &project);
    TEST_ASSERT(insertCmd1.execute());

    // Canvas size auto-fit check
    TEST_ASSERT(project.settings().canvasSize.width == 3840);
    TEST_ASSERT(project.settings().canvasSize.height == 2160);
    TEST_ASSERT(project.settings().fps.numerator == 60000); // 60.0 * 1000

    // Clip placed on Main Track
    TEST_ASSERT(timeline.mainTrack().clips().size() == 1);
    TEST_ASSERT(timeline.mainTrack().clips()[0].id() == clipId1);
    TEST_ASSERT(insertCmd1.targetTrackId() == timeline.mainTrack().id());

    // 2. Insert second video clip with overlap on Main Track -> auto-creates overlay track
    auto clipId2 = ClipId::generate();
    Clip clip2(clipId2, ClipType::Video, "B-Roll Video", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(3.0));
    InsertElementPlacement autoOverlapPlacement;
    autoOverlapPlacement.mode = InsertElementPlacement::Mode::Auto;
    autoOverlapPlacement.autoTrackType = TrackType::Video;
    autoOverlapPlacement.startTime = TimelineTime::fromSeconds(2.0); // Overlaps [0, 5s]

    InsertElementCommand insertCmd2(timeline, clip2, autoOverlapPlacement, &project);
    TEST_ASSERT(insertCmd2.execute());

    // Overlay track created
    TEST_ASSERT(!timeline.overlayTracks().empty());
    TEST_ASSERT(insertCmd2.targetTrackId() != timeline.mainTrack().id());
    Track* overlayTrack = timeline.findTrack(insertCmd2.targetTrackId());
    TEST_ASSERT(overlayTrack != nullptr);
    TEST_ASSERT(overlayTrack->clips().size() == 1);
    TEST_ASSERT(overlayTrack->clips()[0].id() == clipId2);
    TEST_ASSERT(overlayTrack->clips()[0].startTime() == TimelineTime::fromSeconds(2.0));

    // 3. Undo insert 2
    TEST_ASSERT(insertCmd2.undo());
    TEST_ASSERT(timeline.overlayTracks().empty() || timeline.findTrack(insertCmd2.targetTrackId()) == nullptr);

    // 4. Undo insert 1: project canvas restored to 1920x1080 30fps and timeline empty
    TEST_ASSERT(insertCmd1.undo());
    TEST_ASSERT(timeline.mainTrack().clips().empty());
    TEST_ASSERT(project.settings().canvasSize.width == 1920);
    TEST_ASSERT(project.settings().canvasSize.height == 1080);
    TEST_ASSERT(project.settings().fps.numerator == 30);

    std::cout << "[PASS] runInsertElementCommandTests" << std::endl;
}

void runProjectAndBookmarkCommandsTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. UpdateProjectSettingsCommand
    Project project("Settings Test");
    ProjectSettings newSettings;
    newSettings.canvasSize = CanvasSize{1280, 720};
    newSettings.fps = FrameRate{24, 1};
    newSettings.background.color = "#123456";

    UpdateProjectSettingsCommand setCmd(project, newSettings);
    TEST_ASSERT(setCmd.execute());
    TEST_ASSERT(project.settings().canvasSize.width == 1280);
    TEST_ASSERT(project.settings().canvasSize.height == 720);
    TEST_ASSERT(project.settings().fps.numerator == 24);
    TEST_ASSERT(project.settings().background.color == "#123456");

    TEST_ASSERT(setCmd.undo());
    TEST_ASSERT(project.settings().canvasSize.width == 1920);
    TEST_ASSERT(project.settings().canvasSize.height == 1080);
    TEST_ASSERT(project.settings().fps.numerator == 30);

    // 2. ToggleBookmarkCommand
    Timeline timeline;
    ToggleBookmarkCommand toggleCmd1(timeline, TimelineTime::fromSeconds(2.0), "Intro Marker", "#ff0000");
    TEST_ASSERT(toggleCmd1.execute());
    TEST_ASSERT(timeline.bookmarks().size() == 1);
    TEST_ASSERT(timeline.bookmarks()[0].time == TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(timeline.bookmarks()[0].note == "Intro Marker");
    TEST_ASSERT(timeline.bookmarks()[0].color == "#ff0000");

    BookmarkId bmId = timeline.bookmarks()[0].id;

    // Toggle again at same time -> removes it!
    ToggleBookmarkCommand toggleCmd2(timeline, TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(toggleCmd2.execute());
    TEST_ASSERT(timeline.bookmarks().empty());

    // Undo toggle 2 -> bookmark restored
    TEST_ASSERT(toggleCmd2.undo());
    TEST_ASSERT(timeline.bookmarks().size() == 1);

    // 3. MoveBookmarkCommand
    MoveBookmarkCommand moveBmCmd(timeline, bmId, TimelineTime::fromSeconds(4.5));
    TEST_ASSERT(moveBmCmd.execute());
    TEST_ASSERT(timeline.bookmarks()[0].time == TimelineTime::fromSeconds(4.5));

    TEST_ASSERT(moveBmCmd.undo());
    TEST_ASSERT(timeline.bookmarks()[0].time == TimelineTime::fromSeconds(2.0));

    // 4. UpdateBookmarkCommand
    UpdateBookmarkCommand updateBmCmd(timeline, bmId, "Chorus Section", "#00ff00");
    TEST_ASSERT(updateBmCmd.execute());
    TEST_ASSERT(timeline.bookmarks()[0].note == "Chorus Section");
    TEST_ASSERT(timeline.bookmarks()[0].color == "#00ff00");

    TEST_ASSERT(updateBmCmd.undo());
    TEST_ASSERT(timeline.bookmarks()[0].note == "Intro Marker");
    TEST_ASSERT(timeline.bookmarks()[0].color == "#ff0000");

    // 5. RemoveBookmarkCommand
    RemoveBookmarkCommand removeBmCmd(timeline, bmId);
    TEST_ASSERT(removeBmCmd.execute());
    TEST_ASSERT(timeline.bookmarks().empty());

    TEST_ASSERT(removeBmCmd.undo());
    TEST_ASSERT(timeline.bookmarks().size() == 1);

    std::cout << "[PASS] runProjectAndBookmarkCommandsTests" << std::endl;
}

void runSceneCommandsTests() {
    using namespace catchim::core;
    using namespace catchim::editor;

    Project project("Scene Tests Project");
    auto mainSceneId = project.currentSceneId();
    TEST_ASSERT(project.scenes().size() == 1);
    TEST_ASSERT(project.activeScene() != nullptr);
    TEST_ASSERT(project.activeScene()->isMain());

    // 1. Invariant: Cannot delete main scene or only scene
    DeleteSceneCommand delMainCmd(project, mainSceneId);
    TEST_ASSERT(!delMainCmd.execute());
    TEST_ASSERT(project.scenes().size() == 1);

    // 2. CreateSceneCommand
    CreateSceneCommand createCmd(project, "Scene Beta", false);
    TEST_ASSERT(createCmd.execute());
    auto betaId = createCmd.createdSceneId();
    TEST_ASSERT(project.scenes().size() == 2);
    TEST_ASSERT(project.currentSceneId() == betaId);
    TEST_ASSERT(project.activeScene()->name() == "Scene Beta");
    TEST_ASSERT(!project.activeScene()->isMain());

    // Undo create
    TEST_ASSERT(createCmd.undo());
    TEST_ASSERT(project.scenes().size() == 1);
    TEST_ASSERT(project.currentSceneId() == mainSceneId);

    // Re-execute create
    TEST_ASSERT(createCmd.execute());
    TEST_ASSERT(project.scenes().size() == 2);
    TEST_ASSERT(project.currentSceneId() == betaId);

    // 3. RenameSceneCommand
    RenameSceneCommand renameCmd(project, betaId, "Scene Beta (Renamed)");
    TEST_ASSERT(renameCmd.execute());
    TEST_ASSERT(project.findScene(betaId)->name() == "Scene Beta (Renamed)");

    TEST_ASSERT(renameCmd.undo());
    TEST_ASSERT(project.findScene(betaId)->name() == "Scene Beta");

    TEST_ASSERT(renameCmd.execute());
    TEST_ASSERT(project.findScene(betaId)->name() == "Scene Beta (Renamed)");

    // 4. DuplicateSceneCommand
    // Add a clip and bookmark to beta scene
    auto* betaScene = project.findScene(betaId);
    TEST_ASSERT(betaScene != nullptr);
    auto clipId = ClipId::generate();
    Clip c1(clipId, ClipType::Video, "BetaClip", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(4.0));
    betaScene->timeline().addClip(betaScene->timeline().mainTrack().id(), std::move(c1));
    betaScene->timeline().addBookmark(Bookmark{BookmarkId::generate(), TimelineTime::fromSeconds(2.5), "BetaBookmark", "#ff0000"});

    DuplicateSceneCommand dupCmd(project, betaId);
    TEST_ASSERT(dupCmd.execute());
    auto dupId = dupCmd.duplicatedSceneId();
    TEST_ASSERT(project.scenes().size() == 3);
    TEST_ASSERT(project.currentSceneId() == dupId);
    auto* dupScene = project.activeScene();
    TEST_ASSERT(dupScene != nullptr);
    TEST_ASSERT(dupScene->name() == "Scene Beta (Renamed) (Copy)");
    TEST_ASSERT(!dupScene->isMain());
    TEST_ASSERT(dupScene->timeline().mainTrack().clips().size() == 1);
    TEST_ASSERT(dupScene->timeline().bookmarks().size() == 1);
    TEST_ASSERT(dupScene->timeline().bookmarks().front().note == "BetaBookmark");

    // Undo duplicate
    TEST_ASSERT(dupCmd.undo());
    TEST_ASSERT(project.scenes().size() == 2);
    TEST_ASSERT(project.currentSceneId() == betaId);
    TEST_ASSERT(project.findScene(dupId) == nullptr);

    // 5. DeleteSceneCommand on secondary scene
    DeleteSceneCommand delBetaCmd(project, betaId);
    TEST_ASSERT(delBetaCmd.execute());
    TEST_ASSERT(project.scenes().size() == 1);
    TEST_ASSERT(project.currentSceneId() == mainSceneId); // Fallback to main scene
    TEST_ASSERT(project.findScene(betaId) == nullptr);

    // Undo delete
    TEST_ASSERT(delBetaCmd.undo());
    TEST_ASSERT(project.scenes().size() == 2);
    TEST_ASSERT(project.currentSceneId() == betaId); // Active scene restored to beta
    TEST_ASSERT(project.findScene(betaId) != nullptr);

    std::cout << "[PASS] runSceneCommandsTests" << std::endl;
}

void runMediaCommandsTests() {
    using namespace catchim::core;
    using namespace catchim::media;
    using namespace catchim::editor;

    MediaLibrary library;
    Project project("Media Test Project");
    project.settings().fps = FrameRate{30, 1};

    // 1. Add audio asset (no video FPS elevation)
    auto audioId = MediaId::generate();
    auto audioAsset = std::make_shared<MediaAsset>(audioId, "assets/audio/music.mp3", MediaType::Audio);
    audioAsset->setHasAudio(true);
    audioAsset->setDuration(TimelineTime::fromSeconds(120.0));

    AddMediaAssetCommand addAudioCmd(library, audioAsset, &project);
    TEST_ASSERT(addAudioCmd.execute());
    TEST_ASSERT(library.assets().size() == 1);
    TEST_ASSERT(library.findAsset(audioId) != nullptr);
    TEST_ASSERT(project.settings().fps.numerator == 30); // Project FPS unchanged

    // 2. Add high-framerate video asset (60 FPS -> project FPS should elevate)
    auto videoId = MediaId::generate();
    auto videoAsset = std::make_shared<MediaAsset>(videoId, "assets/video/gameplay60.mp4", MediaType::Video);
    videoAsset->setHasVideo(true);
    videoAsset->setFps(FrameRate{60, 1});
    videoAsset->setDuration(TimelineTime::fromSeconds(30.0));

    AddMediaAssetCommand addVideoCmd(library, videoAsset, &project);
    TEST_ASSERT(addVideoCmd.execute());
    TEST_ASSERT(library.assets().size() == 2);
    TEST_ASSERT(project.settings().fps.numerator == 60); // Project FPS elevated to 60!

    // Undo Add video asset -> project FPS must restore to 30!
    TEST_ASSERT(addVideoCmd.undo());
    TEST_ASSERT(library.assets().size() == 1);
    TEST_ASSERT(library.findAsset(videoId) == nullptr);
    TEST_ASSERT(project.settings().fps.numerator == 30); // Restored!

    // Redo Add video asset
    TEST_ASSERT(addVideoCmd.execute());
    TEST_ASSERT(library.assets().size() == 2);
    TEST_ASSERT(project.settings().fps.numerator == 60);

    // 3. UpdateMediaAssetPathCommand
    UpdateMediaAssetPathCommand updatePathCmd(library, audioId, "assets/audio/soundtrack_v2.mp3");
    TEST_ASSERT(updatePathCmd.execute());
    TEST_ASSERT(library.findAsset(audioId)->fileName() == "soundtrack_v2.mp3");

    TEST_ASSERT(updatePathCmd.undo());
    TEST_ASSERT(library.findAsset(audioId)->fileName() == "music.mp3");

    // 4. RemoveMediaAssetCommand with orphaned clip cleanup
    // Place a video clip and an audio clip on the project's active timeline
    auto* tl = project.activeTimeline();
    TEST_ASSERT(tl != nullptr);
    tl->addTrack(TrackType::Audio, "Audio Track");
    auto audioTrackId = tl->audioTracks().front().id();
    auto videoTrackId = tl->mainTrack().id();

    Clip vClip(ClipId::generate(), ClipType::Video, "Video Clip", TimelineTime::fromSeconds(0), TimelineTime::fromSeconds(5));
    vClip.setMediaId(videoId);
    tl->addClip(videoTrackId, std::move(vClip));

    Clip aClip(ClipId::generate(), ClipType::Audio, "Audio Clip", TimelineTime::fromSeconds(0), TimelineTime::fromSeconds(10));
    aClip.setMediaId(audioId);
    tl->addClip(audioTrackId, std::move(aClip));

    TEST_ASSERT(tl->mainTrack().clips().size() == 1);
    TEST_ASSERT(tl->audioTracks().front().clips().size() == 1);

    // Remove the video asset from library -> the video clip referencing it should be removed from timeline!
    RemoveMediaAssetCommand removeVideoCmd(library, videoId, &project);
    TEST_ASSERT(removeVideoCmd.execute());
    TEST_ASSERT(library.findAsset(videoId) == nullptr);
    TEST_ASSERT(library.assets().size() == 1); // Only audio asset remains
    TEST_ASSERT(tl->mainTrack().clips().empty()); // Orphaned video clip removed!
    TEST_ASSERT(tl->audioTracks().front().clips().size() == 1); // Audio clip untouched!

    // Undo Remove video asset -> video asset restored to library, clip restored to timeline!
    TEST_ASSERT(removeVideoCmd.undo());
    TEST_ASSERT(library.findAsset(videoId) != nullptr);
    TEST_ASSERT(library.assets().size() == 2);
    TEST_ASSERT(tl->mainTrack().clips().size() == 1); // Restored!
    TEST_ASSERT(tl->mainTrack().clips().front().name() == "Video Clip");

    std::cout << "[PASS] runMediaCommandsTests" << std::endl;
}

void runPreviewTrackerTests() {
    using namespace catchim::core;
    using namespace catchim::editor;

    // 1. Primitive state tracking
    PreviewTracker<int> intTracker;
    TEST_ASSERT(!intTracker.isActive());
    TEST_ASSERT(intTracker.getSnapshot() == nullptr);

    intTracker.begin(100);
    TEST_ASSERT(intTracker.isActive());
    TEST_ASSERT(intTracker.getSnapshot() != nullptr);
    TEST_ASSERT(*intTracker.getSnapshot() == 100);

    // Subsequent begin while active should NOT overwrite the initial snapshot
    intTracker.begin(999);
    TEST_ASSERT(*intTracker.getSnapshot() == 100);

    auto endedSnap = intTracker.end();
    TEST_ASSERT(endedSnap.has_value());
    TEST_ASSERT(*endedSnap == 100);
    TEST_ASSERT(!intTracker.isActive());
    TEST_ASSERT(intTracker.getSnapshot() == nullptr);

    // 2. Cancellation
    intTracker.begin(55);
    TEST_ASSERT(intTracker.isActive());
    intTracker.cancel();
    TEST_ASSERT(!intTracker.isActive());
    TEST_ASSERT(intTracker.getSnapshot() == nullptr);

    // 3. Complex state tracking (e.g. interactive drag coordinates & scale)
    struct InteractiveDragState {
        double offsetX{0.0};
        double offsetY{0.0};
        double scale{1.0};
        bool operator==(const InteractiveDragState& o) const = default;
    };

    PreviewTracker<InteractiveDragState> dragTracker;
    dragTracker.begin(InteractiveDragState{15.5, 30.0, 1.25});
    TEST_ASSERT(dragTracker.isActive());
    TEST_ASSERT(dragTracker.getSnapshot()->offsetX == 15.5);
    TEST_ASSERT(dragTracker.getSnapshot()->offsetY == 30.0);
    TEST_ASSERT(dragTracker.getSnapshot()->scale == 1.25);

    auto dragEnd = dragTracker.end();
    TEST_ASSERT(dragEnd.has_value());
    TEST_ASSERT(dragEnd->offsetX == 15.5);
    TEST_ASSERT(!dragTracker.isActive());

    std::cout << "[PASS] runPreviewTrackerTests" << std::endl;
}

void runAudioSeparationEngineAndToggleTests() {
    using namespace catchim::core;
    using namespace catchim::media;
    using namespace catchim::editor;

    // 1. AudioSeparationEngine inspections on Video Clip
    Clip vClip(ClipId::generate(), ClipType::Video, "HeroVideo", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(6.0),
               TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(1.0));
    vClip.setSourceDuration(TimelineTime::fromSeconds(8.0));
    vClip.setParam<double>("volume", 80.0);
    vClip.setParam<double>("retimeRate", 1.25);
    vClip.setParam<bool>("maintainPitch", true);

    // Add a volume animation channel
    AnimationChannel volChan("volume", 80.0);
    volChan.addOrUpdateKeyframe(Keyframe{TimelineTime::fromSeconds(0.0), 80.0});
    volChan.addOrUpdateKeyframe(Keyframe{TimelineTime::fromSeconds(2.0), 40.0});
    vClip.animationChannels()["volume"] = std::move(volChan);

    MediaAsset dummyAsset(MediaId::generate(), "video.mp4", MediaType::Video);
    dummyAsset.setHasAudio(true);
    dummyAsset.setHasVideo(true);

    TEST_ASSERT(AudioSeparationEngine::isSourceAudioEnabled(vClip));
    TEST_ASSERT(!AudioSeparationEngine::isSourceAudioSeparated(vClip));
    TEST_ASSERT(AudioSeparationEngine::canExtractSourceAudio(vClip, &dummyAsset));
    TEST_ASSERT(!AudioSeparationEngine::canRecoverSourceAudio(vClip));
    TEST_ASSERT(AudioSeparationEngine::canToggleSourceAudio(vClip, &dummyAsset));
    TEST_ASSERT(AudioSeparationEngine::doesElementHaveEnabledAudio(vClip, &dummyAsset));
    TEST_ASSERT(AudioSeparationEngine::getSourceAudioActionLabel(vClip) == "Extract audio");

    // Build separated audio clip
    Clip separatedClip = AudioSeparationEngine::buildSeparatedAudioClip(vClip);
    TEST_ASSERT(separatedClip.type() == ClipType::Audio);
    TEST_ASSERT(separatedClip.name() == "HeroVideo (Audio)");
    TEST_ASSERT(separatedClip.startTime() == TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(separatedClip.duration() == TimelineTime::fromSeconds(6.0));
    TEST_ASSERT(separatedClip.trimStart() == TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(separatedClip.trimEnd() == TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(separatedClip.sourceDuration() == TimelineTime::fromSeconds(8.0));
    TEST_ASSERT(separatedClip.getParam<double>("volume", 0.0) == 80.0);
    TEST_ASSERT(separatedClip.getParam<double>("retimeRate", 0.0) == 1.25);
    TEST_ASSERT(separatedClip.getParam<bool>("maintainPitch", false) == true);
    TEST_ASSERT(separatedClip.hasAnimationChannel("volume"));
    TEST_ASSERT(separatedClip.findAnimationChannel("volume")->keyframes().size() == 2);

    // 2. ToggleSourceAudioSeparationCommand
    Timeline timeline;
    auto videoTrackId = timeline.mainTrack().id();
    auto clipId = vClip.id();
    timeline.addClip(videoTrackId, std::move(vClip));
    TEST_ASSERT(timeline.audioTracks().empty());

    // Execute separation
    ToggleSourceAudioSeparationCommand sepCmd(timeline, videoTrackId, clipId, &dummyAsset);
    TEST_ASSERT(sepCmd.execute());
    TEST_ASSERT(sepCmd.wasSeparation());
    auto* updatedVideoClip = timeline.findClip(clipId);
    TEST_ASSERT(updatedVideoClip != nullptr);
    TEST_ASSERT(!AudioSeparationEngine::isSourceAudioEnabled(*updatedVideoClip));
    TEST_ASSERT(AudioSeparationEngine::isSourceAudioSeparated(*updatedVideoClip));
    TEST_ASSERT(AudioSeparationEngine::getSourceAudioActionLabel(*updatedVideoClip) == "Recover audio");
    TEST_ASSERT(timeline.audioTracks().size() == 1);
    TEST_ASSERT(timeline.audioTracks().front().clips().size() == 1);

    // Undo separation
    TEST_ASSERT(sepCmd.undo());
    updatedVideoClip = timeline.findClip(clipId);
    TEST_ASSERT(updatedVideoClip != nullptr);
    TEST_ASSERT(AudioSeparationEngine::isSourceAudioEnabled(*updatedVideoClip));
    TEST_ASSERT(!AudioSeparationEngine::isSourceAudioSeparated(*updatedVideoClip));
    TEST_ASSERT(timeline.audioTracks().empty());

    // Redo separation
    TEST_ASSERT(sepCmd.execute());
    TEST_ASSERT(timeline.audioTracks().size() == 1);
    TEST_ASSERT(timeline.audioTracks().front().clips().size() == 1);

    // Second toggle: Recover audio
    ToggleSourceAudioSeparationCommand recCmd(timeline, videoTrackId, clipId, &dummyAsset);
    TEST_ASSERT(recCmd.execute());
    TEST_ASSERT(!recCmd.wasSeparation()); // Recovered!
    updatedVideoClip = timeline.findClip(clipId);
    TEST_ASSERT(AudioSeparationEngine::isSourceAudioEnabled(*updatedVideoClip));
    TEST_ASSERT(timeline.audioTracks().front().clips().empty());

    // Undo recovery
    TEST_ASSERT(recCmd.undo());
    updatedVideoClip = timeline.findClip(clipId);
    TEST_ASSERT(AudioSeparationEngine::isSourceAudioSeparated(*updatedVideoClip));
    TEST_ASSERT(timeline.audioTracks().front().clips().size() == 1);

    std::cout << "[PASS] runAudioSeparationEngineAndToggleTests" << std::endl;
}

void runTimelineDragSourceTests() {
    using namespace catchim::core;
    using namespace catchim::editor;

    TimelineDragSource dragSource;
    TEST_ASSERT(!dragSource.isActive());
    TEST_ASSERT(dragSource.getActive() == nullptr);

    // 1. Begin Media drag
    MediaDragPayload mediaPayload;
    mediaPayload.id = "media-101";
    mediaPayload.name = "Intro.mp4";
    mediaPayload.mediaType = ClipType::Video;
    mediaPayload.duration = TimelineTime::fromSeconds(4.0);
    mediaPayload.hasVideo = true;
    mediaPayload.hasAudio = true;

    dragSource.begin(mediaPayload);
    TEST_ASSERT(dragSource.isActive());
    TEST_ASSERT(dragSource.getActiveType() == DragPayloadType::Media);

    Timeline timeline;
    auto videoTrackId = timeline.mainTrack().id();
    auto& audioTrack = timeline.addTrack(TrackType::Audio, "Audio");
    auto& textTrack = timeline.addTrack(TrackType::Text, "Text");

    // Check track drop compatibility
    TEST_ASSERT(TimelineDragSource::canDropOnTrack(timeline.mainTrack(), *dragSource.getActive()));
    TEST_ASSERT(!TimelineDragSource::canDropOnTrack(audioTrack, *dragSource.getActive()));
    TEST_ASSERT(!TimelineDragSource::canDropOnTrack(textTrack, *dragSource.getActive()));

    // 2. Resolve drop onto Video Track
    auto droppedClipId = TimelineDragSource::resolveDropOnTrack(timeline, videoTrackId, TimelineTime::fromSeconds(1.0), *dragSource.getActive());
    TEST_ASSERT(droppedClipId.has_value());
    auto* droppedClip = timeline.findClip(*droppedClipId);
    TEST_ASSERT(droppedClip != nullptr);
    TEST_ASSERT(droppedClip->name() == "Intro.mp4");
    TEST_ASSERT(droppedClip->startTime() == TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(droppedClip->duration() == TimelineTime::fromSeconds(4.0));
    TEST_ASSERT(droppedClip->mediaId() == MediaId("media-101"));

    // 3. Drop collision prevention: cannot drop another clip overlapping [1.0s, 5.0s]
    auto collideDrop = TimelineDragSource::resolveDropOnTrack(timeline, videoTrackId, TimelineTime::fromSeconds(2.0), *dragSource.getActive());
    TEST_ASSERT(!collideDrop.has_value());

    // 4. Text and Graphic Drag Payloads
    TextDragPayload textPayload{"txt-1", "MyTitle", "Antigravity Catchim"};
    dragSource.begin(textPayload);
    TEST_ASSERT(dragSource.getActiveType() == DragPayloadType::Text);
    TEST_ASSERT(TimelineDragSource::canDropOnTrack(textTrack, *dragSource.getActive()));
    auto textClipId = TimelineDragSource::resolveDropOnTrack(timeline, textTrack.id(), TimelineTime::fromSeconds(0.0), *dragSource.getActive());
    TEST_ASSERT(textClipId.has_value());
    TEST_ASSERT(timeline.findClip(*textClipId)->getParam<std::string>("content", "") == "Antigravity Catchim");

    // 5. Effect Drag Payload & Dropping onto Visual Clip
    EffectDragPayload effectPayload{"eff-1", "Gaussian Blur", "blur", {ClipType::Video, ClipType::Image}};
    dragSource.begin(effectPayload);
    TEST_ASSERT(dragSource.getActiveType() == DragPayloadType::Effect);

    // Dropping on video clip: allowed
    TEST_ASSERT(TimelineDragSource::canDropOnClip(*droppedClip, *dragSource.getActive()));
    TEST_ASSERT(TimelineDragSource::resolveDropOnClip(*droppedClip, *dragSource.getActive()));
    TEST_ASSERT(droppedClip->effects().size() == 1);
    TEST_ASSERT(droppedClip->effects().front().type == "blur");

    // Dropping on text clip: rejected (text is not in targetElementTypes)
    auto* textClip = timeline.findClip(*textClipId);
    TEST_ASSERT(!TimelineDragSource::canDropOnClip(*textClip, *dragSource.getActive()));

    dragSource.end();
    TEST_ASSERT(!dragSource.isActive());

    std::cout << "[PASS] runTimelineDragSourceTests" << std::endl;
}

void runEditorSelectionHierarchyTests() {
    using namespace catchim::core;
    using namespace catchim::editor;

    EditorSelection selection;
    TEST_ASSERT(!selection.getActiveSelectionKind().has_value());
    TEST_ASSERT(selection.getSelectedElements().empty());
    TEST_ASSERT(selection.getSelectedKeyframes().empty());
    TEST_ASSERT(!selection.getSelectedMaskPointSelection().has_value());

    TrackId trk1 = TrackId::generate();
    ClipId clp1 = ClipId::generate();
    ClipId clp2 = ClipId::generate();

    // 1. Elements selection tier
    selection.setSelectedElements({{trk1, clp1}, {trk1, clp2}});
    TEST_ASSERT(selection.getActiveSelectionKind().has_value());
    TEST_ASSERT(*selection.getActiveSelectionKind() == EditorSelectionKind::Elements);
    TEST_ASSERT(selection.getSelectedElements().size() == 2);

    // 2. Keyframes selection tier overrides elements
    SelectedKeyframeRef kf1{trk1, clp1, "transform.positionX", TimelineTime::fromSeconds(1.0)};
    SelectedKeyframeRef kf2{trk1, clp1, "transform.positionX", TimelineTime::fromSeconds(3.0)};
    selection.setSelectedKeyframes({kf1, kf2}, kf1);
    TEST_ASSERT(*selection.getActiveSelectionKind() == EditorSelectionKind::Keyframes);
    TEST_ASSERT(selection.getSelectedKeyframes().size() == 2);
    TEST_ASSERT(selection.getKeyframeSelectionAnchor().has_value());
    TEST_ASSERT(selection.getKeyframeSelectionAnchor()->keyframeTime == TimelineTime::fromSeconds(1.0));

    // 3. Mask points tier overrides keyframes
    SelectedMaskPointSelection maskSel{trk1, clp1, "mask-hero", {"pt-a", "pt-b"}};
    selection.setSelectedMaskPoints(maskSel);
    TEST_ASSERT(*selection.getActiveSelectionKind() == EditorSelectionKind::MaskPoints);
    TEST_ASSERT(selection.getSelectedMaskPointSelection().has_value());
    TEST_ASSERT(selection.getSelectedMaskPointSelection()->pointIds.size() == 2);
    TEST_ASSERT(selection.getSelectedKeyframes().empty()); // Keyframes cleared on mask selection

    // 4. Hierarchical clear: clearMostSpecificSelection
    // Clearing from MaskPoints tier removes mask points, falling back to Elements
    TEST_ASSERT(selection.clearMostSpecificSelection());
    TEST_ASSERT(!selection.getSelectedMaskPointSelection().has_value());
    TEST_ASSERT(selection.getActiveSelectionKind().has_value());
    TEST_ASSERT(*selection.getActiveSelectionKind() == EditorSelectionKind::Elements);

    // Second clear removes elements tier
    TEST_ASSERT(selection.clearMostSpecificSelection());
    TEST_ASSERT(!selection.getActiveSelectionKind().has_value());
    TEST_ASSERT(selection.getSelectedElements().empty());

    // 5. Snapshot & Patch restoration
    selection.setSelectedElements({{trk1, clp1}});
    auto snap = selection.getSnapshot();
    TEST_ASSERT(snap.selectedElements.size() == 1);

    // Apply patch: add keyframes via patch
    EditorSelectionPatch patch;
    patch.selectedKeyframes = std::vector<SelectedKeyframeRef>{kf1};
    selection.applySelectionPatch(patch);
    TEST_ASSERT(selection.getSelectedKeyframes().size() == 1);

    // Restore snapshot
    selection.restoreSnapshot(snap);
    TEST_ASSERT(selection.getSelectedElements().size() == 1);
    TEST_ASSERT(selection.getSelectedKeyframes().empty());

    // Full clear
    selection.clearSelection();
    TEST_ASSERT(!selection.clearMostSpecificSelection()); // Nothing to clear -> returns false

    std::cout << "[PASS] runEditorSelectionHierarchyTests" << std::endl;
}

void runWaveformSummaryEngineTests() {
    using namespace catchim::media;

    // 1. Key generator
    std::string key = WaveformSummaryEngine::buildWaveformSourceKey("media", "asset-007");
    TEST_ASSERT(key == "media:asset-007");

    // 2. Peak bucketing across 2 channels
    constexpr size_t N = 1000;
    std::vector<float> ch0(N, 0.0f);
    std::vector<float> ch1(N, 0.0f);

    ch0[250] = 0.85f;
    ch1[750] = -0.92f;

    std::vector<SampleBucket> buckets = {
        {0, 500},
        {500, 1000}
    };

    auto peaks = WaveformSummaryEngine::computePeakBuckets({ch0.data(), ch1.data()}, N, buckets);
    TEST_ASSERT(peaks.size() == 2);
    TEST_ASSERT(std::abs(peaks[0] - 0.85f) < 1e-4f);
    TEST_ASSERT(std::abs(peaks[1] - 0.92f) < 1e-4f);

    // 3. RMS bucketing on known amplitude signal
    // Constant signal of 0.7071 -> RMS = 0.7071
    std::vector<float> sine(N, 0.7071f);
    auto rmsList = WaveformSummaryEngine::computeRmsBuckets({sine.data()}, N, 48000, buckets, 0.02);
    TEST_ASSERT(rmsList.size() == 2);
    TEST_ASSERT(std::abs(rmsList[0] - 0.7071f) < 1e-3f);
    TEST_ASSERT(std::abs(rmsList[1] - 0.7071f) < 1e-3f);

    // 4. SourceWaveformSummary building & sampling
    auto summary = WaveformSummaryEngine::buildSourceWaveformSummary("media:test", {ch0.data(), ch1.data()}, N, 48000, 128);
    TEST_ASSERT(summary.sourceKey == "media:test");
    TEST_ASSERT(summary.totalSamples == N);
    TEST_ASSERT(summary.bucketSize == 128);
    // 1000 / 128 = 7.8125 -> 8 buckets
    TEST_ASSERT(summary.amplitudes.size() == 8);
    // Bucket containing index 250 is bucket index 250 / 128 = 1
    TEST_ASSERT(std::abs(summary.amplitudes[1] - 0.85f) < 1e-4f);
    // Bucket containing index 750 is bucket index 750 / 128 = 5
    TEST_ASSERT(std::abs(summary.amplitudes[5] - 0.92f) < 1e-4f);

    // Subsample summary
    auto subsampled = WaveformSummaryEngine::sampleSourceWaveformSummary(summary, buckets);
    TEST_ASSERT(subsampled.size() == 2);
    TEST_ASSERT(std::abs(subsampled[0] - 0.85f) < 1e-4f);
    TEST_ASSERT(std::abs(subsampled[1] - 0.92f) < 1e-4f);

    // 5. Timeline sample buckets with Retime scaling (2.0x speed)
    auto uiBuckets = WaveformSummaryEngine::buildWaveformSampleBuckets(
        0.0, 200.0, 10, 100.0, 2.0, 0.0, 2.0, 48000, 480000, 20.0
    );
    TEST_ASSERT(uiBuckets.size() == 10);
    // Bar 0: clip [0.0s, 0.2s] -> source [0.0s, 0.4s] -> samples [0, 19200]
    TEST_ASSERT(uiBuckets[0].bucketStart == 0);
    TEST_ASSERT(uiBuckets[0].bucketEnd == 19200);

    std::cout << "[PASS] runWaveformSummaryEngineTests" << std::endl;
}

void runCanvasBackgroundEngineTests() {
    using namespace catchim::editor;

    // 1. Presets inspection
    const auto& blurPresets = CanvasBackgroundEngine::blurPresets();
    TEST_ASSERT(blurPresets.size() == 3);
    TEST_ASSERT(std::string(blurPresets[0].label) == "Light" && blurPresets[0].value == 100);
    TEST_ASSERT(std::string(blurPresets[1].label) == "Medium" && blurPresets[1].value == 200);
    TEST_ASSERT(std::string(blurPresets[2].label) == "Heavy" && blurPresets[2].value == 500);

    const auto& canvasPresets = CanvasBackgroundEngine::canvasPresets();
    TEST_ASSERT(canvasPresets.size() >= 6);
    TEST_ASSERT(canvasPresets[0].width == 1920 && canvasPresets[0].height == 1080);
    TEST_ASSERT(std::string(canvasPresets[0].aspectRatio) == "16:9");

    // 2. Aspect Ratio Detection
    TEST_ASSERT(CanvasBackgroundEngine::detectAspectRatio(1920, 1080) == "16:9");
    TEST_ASSERT(CanvasBackgroundEngine::detectAspectRatio(1080, 1920) == "9:16");
    TEST_ASSERT(CanvasBackgroundEngine::detectAspectRatio(1080, 1080) == "1:1");
    TEST_ASSERT(CanvasBackgroundEngine::detectAspectRatio(1440, 1080) == "4:3");
    TEST_ASSERT(CanvasBackgroundEngine::detectAspectRatio(2560, 1080) == "21:9");
    TEST_ASSERT(CanvasBackgroundEngine::detectAspectRatio(800, 600) == "4:3");

    // 3. Rect Fitting: Contain vs Cover
    // Media 1920x1080 into Vertical Canvas 1080x1920
    auto containFit = CanvasBackgroundEngine::fitRectIntoCanvas(1920.0, 1080.0, 1080.0, 1920.0, FitMode::Contain);
    TEST_ASSERT(std::abs(containFit.scale - (1080.0 / 1920.0)) < 1e-4);
    TEST_ASSERT(std::abs(containFit.width - 1080.0) < 1e-4);
    TEST_ASSERT(std::abs(containFit.height - 607.5) < 1e-4);
    TEST_ASSERT(std::abs(containFit.x - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(containFit.y - (1920.0 - 607.5) / 2.0) < 1e-4);

    auto coverFit = CanvasBackgroundEngine::fitRectIntoCanvas(1920.0, 1080.0, 1080.0, 1920.0, FitMode::Cover);
    TEST_ASSERT(std::abs(coverFit.scale - (1920.0 / 1080.0)) < 1e-4);
    TEST_ASSERT(std::abs(coverFit.height - 1920.0) < 1e-4);
    TEST_ASSERT(coverFit.width > 1080.0);
    TEST_ASSERT(coverFit.x < 0.0); // Center-cropped horizontally

    // 4. Viewport fit scale
    double scale = CanvasBackgroundEngine::calculateFitScale(1920.0, 1080.0, 1000.0, 600.0, 20.0);
    // Avail: 960x560 -> scale = min(960/1920, 560/1080) = min(0.5, 0.5185) = 0.5
    TEST_ASSERT(std::abs(scale - 0.5) < 1e-4);

    std::cout << "[PASS] runCanvasBackgroundEngineTests" << std::endl;
}

void runProjectManagerTests() {
    using namespace catchim::core;
    using namespace catchim::editor;

    ProjectManager manager;
    TEST_ASSERT(manager.getActive() == nullptr);
    TEST_ASSERT(!manager.isDirty());

    // 1. Create New Project
    auto& project = manager.createNewProject("Project Galaxy");
    TEST_ASSERT(manager.getActive() != nullptr);
    TEST_ASSERT(project.name() == "Project Galaxy");
    TEST_ASSERT(project.scenes().size() == 1);
    TEST_ASSERT(project.activeScene() != nullptr);
    TEST_ASSERT(project.activeScene()->isMain());
    TEST_ASSERT(project.activeScene()->name() == "Main scene");

    // 2. Multi-Scene Duration Aggregation
    // Add clip to main scene: 0.0s to 4.5s
    auto clip1 = Clip(ClipId::generate(), ClipType::Video, "Clip1", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(4.5));
    project.activeTimeline()->addClip(project.activeTimeline()->mainTrack().id(), std::move(clip1));

    // Create a secondary scene with duration 10.0s
    auto& scene2 = project.createScene("Scene 2");
    auto clip2 = Clip(ClipId::generate(), ClipType::Video, "Clip2", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(10.0));
    scene2.timeline().addClip(scene2.timeline().mainTrack().id(), std::move(clip2));

    auto totalDur = ProjectManager::getProjectDurationFromScenes(project.scenes());
    // In web, project duration is the duration of the main scene
    TEST_ASSERT(totalDur == TimelineTime::fromSeconds(4.5));

    // 3. Project Metadata Sorting
    std::vector<ProjectMetadata> catalog = {
        {ProjectId("p1"), "Project Bravo", "", TimelineTime::fromSeconds(10.0), "2026-09-01", "2026-09-02"},
        {ProjectId("p2"), "Project Alpha", "", TimelineTime::fromSeconds(30.0), "2026-09-05", "2026-09-20"},
        {ProjectId("p3"), "Project Charlie", "", TimelineTime::fromSeconds(5.0), "2026-09-10", "2026-09-15"}
    };

    // Sort by UpdatedAt Descending
    ProjectManager::sortProjects(catalog, ProjectSortKey::UpdatedAt, SortDirection::Descending);
    TEST_ASSERT(catalog[0].id == ProjectId("p2")); // 2026-09-20
    TEST_ASSERT(catalog[1].id == ProjectId("p3")); // 2026-09-15
    TEST_ASSERT(catalog[2].id == ProjectId("p1")); // 2026-09-02

    // Sort by Name Ascending
    ProjectManager::sortProjects(catalog, ProjectSortKey::Name, SortDirection::Ascending);
    TEST_ASSERT(catalog[0].name == "Project Alpha");
    TEST_ASSERT(catalog[1].name == "Project Bravo");
    TEST_ASSERT(catalog[2].name == "Project Charlie");

    // Sort by Duration Descending
    ProjectManager::sortProjects(catalog, ProjectSortKey::Duration, SortDirection::Descending);
    TEST_ASSERT(catalog[0].duration == TimelineTime::fromSeconds(30.0));
    TEST_ASSERT(catalog[1].duration == TimelineTime::fromSeconds(10.0));
    TEST_ASSERT(catalog[2].duration == TimelineTime::fromSeconds(5.0));

    // 4. Migration State Tracking
    MigrationState mState{true, 18, 31, "Project Galaxy"};
    manager.setMigrationState(mState);
    TEST_ASSERT(manager.migrationState().isMigrating);
    TEST_ASSERT(manager.migrationState().fromVersion == 18);
    TEST_ASSERT(manager.migrationState().toVersion == 31);
    TEST_ASSERT(manager.migrationState().projectName == "Project Galaxy");

    // 5. Change Notification & Dirty State
    bool listenerFired = false;
    manager.subscribe([&]() {
        listenerFired = true;
    });

    manager.setDirty(true);
    TEST_ASSERT(manager.isDirty());
    TEST_ASSERT(listenerFired);

    manager.closeActive();
    TEST_ASSERT(manager.getActive() == nullptr);

    std::cout << "[PASS] runProjectManagerTests" << std::endl;
}

void runAudioStateEngineTests() {
    using namespace catchim::audio;
    using namespace catchim::core;
    using namespace catchim::editor;

    // 1. Math and conversions
    TEST_ASSERT(AudioStateEngine::clampDb(0.0) == 0.0);
    TEST_ASSERT(AudioStateEngine::clampDb(-100.0) == kVolumeDbMin);
    TEST_ASSERT(AudioStateEngine::clampDb(50.0) == kVolumeDbMax);
    TEST_ASSERT(AudioStateEngine::clampDb(std::numeric_limits<double>::quiet_NaN()) == 0.0);

    TEST_ASSERT(std::abs(AudioStateEngine::dBToLinear(0.0) - 1.0) < 1e-4);
    TEST_ASSERT(std::abs(AudioStateEngine::dBToLinear(-20.0) - 0.1) < 1e-4);
    TEST_ASSERT(std::abs(AudioStateEngine::linearToDb(1.0) - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(AudioStateEngine::linearToDb(0.1) - (-20.0)) < 1e-4);
    TEST_ASSERT(AudioStateEngine::linearToDb(0.0) == kVolumeDbMin);

    Clip clip(ClipId("c-audio-1"), ClipType::Audio, "Voiceover", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(AudioStateEngine::getElementVolume(clip) == 1.0);
    Clip nonAudioClip(ClipId("c-text-1"), ClipType::Text, "Title", TimelineTime(0), TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(AudioStateEngine::getElementVolume(nonAudioClip) == 0.0);
    TEST_ASSERT(!AudioStateEngine::isElementMuted(clip));
    TEST_ASSERT(!AudioStateEngine::hasAnimatedVolume(clip));

    clip.setParam("volume", -6.0);
    TEST_ASSERT(std::abs(AudioStateEngine::getElementVolume(clip) - (-6.0)) < 1e-4);

    clip.setParam("muted", true);
    TEST_ASSERT(AudioStateEngine::isElementMuted(clip));
    clip.setParam("muted", false);

    // Effective audio gain when static
    double g1 = AudioStateEngine::resolveEffectiveAudioGain(clip, false, 1.0);
    TEST_ASSERT(std::abs(g1 - AudioStateEngine::dBToLinear(-6.0)) < 1e-4);

    // When track is muted
    double gMuted = AudioStateEngine::resolveEffectiveAudioGain(clip, true, 1.0);
    TEST_ASSERT(gMuted == 0.0);

    // 3. Animated volume evaluation
    auto& chan = clip.getOrCreateAnimationChannel("volume", 0.0);
    Keyframe kf1;
    kf1.time = TimelineTime::fromSeconds(0.0);
    kf1.value = 0.0; // 0 dB -> gain 1.0
    chan.addOrUpdateKeyframe(kf1);

    Keyframe kf2;
    kf2.time = TimelineTime::fromSeconds(4.0);
    kf2.value = -20.0; // -20 dB -> gain 0.1
    chan.addOrUpdateKeyframe(kf2);

    TEST_ASSERT(AudioStateEngine::hasAnimatedVolume(clip));

    double gAt0 = AudioStateEngine::resolveEffectiveAudioGain(clip, false, 0.0);
    TEST_ASSERT(std::abs(gAt0 - 1.0) < 1e-3);

    double gAt4 = AudioStateEngine::resolveEffectiveAudioGain(clip, false, 4.0);
    TEST_ASSERT(std::abs(gAt4 - 0.1) < 1e-3);

    // 4. Waveform gain samples
    auto emptySamples = AudioStateEngine::buildWaveformGainSamples(clip, 0);
    TEST_ASSERT(emptySamples.empty());

    auto samples = AudioStateEngine::buildWaveformGainSamples(clip, 10);
    TEST_ASSERT(samples.size() == 10);
    // Gain should decrease monotonically from ~1.0 down to ~0.1
    TEST_ASSERT(samples.front() > samples.back());

    // 5. Audio gain automation
    auto automation = AudioStateEngine::buildAudioGainAutomation(clip, false, 0.0, 4.0, 1.0);
    TEST_ASSERT(automation.size() >= 5);
    TEST_ASSERT(std::abs(automation.front().gain - 1.0) < 1e-3);
    TEST_ASSERT(std::abs(automation.back().gain - 0.1) < 1e-3);

    std::cout << "[PASS] runAudioStateEngineTests" << std::endl;
}

void runPreviewCoordinateTransformerTests() {
    using namespace catchim::render;
    using namespace catchim::editor;

    PreviewViewportGeometry geom;
    geom.canvasWidth = 1920.0;
    geom.canvasHeight = 1080.0;
    geom.centerX = 960.0;
    geom.centerY = 540.0;
    geom.scale = 0.5;
    geom.viewportWidth = 960.0;
    geom.viewportHeight = 540.0;

    // 1. Canvas Origin
    Vec2D origin = PreviewCoordinateTransformer::getCanvasOrigin(geom);
    TEST_ASSERT(std::abs(origin.x - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(origin.y - 0.0) < 1e-4);

    // 2. Canvas to Overlay
    Vec2D ov0 = PreviewCoordinateTransformer::canvasToOverlay(0.0, 0.0, geom);
    TEST_ASSERT(std::abs(ov0.x - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(ov0.y - 0.0) < 1e-4);

    Vec2D ov1920 = PreviewCoordinateTransformer::canvasToOverlay(1920.0, 1080.0, geom);
    TEST_ASSERT(std::abs(ov1920.x - (1920.0 * 0.5)) < 1e-4);
    TEST_ASSERT(std::abs(ov1920.y - (1080.0 * 0.5)) < 1e-4);

    // 3. Screen to Canvas
    Rect2D vpRect{0.0, 0.0, 960.0, 540.0};
    Vec2D c0 = PreviewCoordinateTransformer::screenToCanvas(0.0, 0.0, geom, vpRect);
    TEST_ASSERT(std::abs(c0.x - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(c0.y - 0.0) < 1e-4);

    Vec2D cCenter = PreviewCoordinateTransformer::screenToCanvas(480.0, 270.0, geom, vpRect);
    TEST_ASSERT(std::abs(cCenter.x - 960.0) < 1e-4);
    TEST_ASSERT(std::abs(cCenter.y - 540.0) < 1e-4);

    // 4. Position to Overlay
    Vec2D posOv = PreviewCoordinateTransformer::positionToOverlay(0.0, 0.0, geom);
    TEST_ASSERT(std::abs(posOv.x - 480.0) < 1e-4);
    TEST_ASSERT(std::abs(posOv.y - 270.0) < 1e-4);

    // 5. Display scale & screen threshold
    Vec2D scale = PreviewCoordinateTransformer::getDisplayScale(geom);
    TEST_ASSERT(std::abs(scale.x - 0.5) < 1e-4);

    Vec2D thresh = PreviewCoordinateTransformer::screenPixelsToLogicalThreshold(geom, 10.0);
    TEST_ASSERT(std::abs(thresh.x - 20.0) < 1e-4);

    std::cout << "[PASS] runPreviewCoordinateTransformerTests" << std::endl;
}

void runElementBoundsEngineTests() {
    using namespace catchim::render;
    using namespace catchim::core;
    using namespace catchim::editor;

    // 1. Point in rotated rect
    ElementBounds b1;
    b1.cx = 500.0;
    b1.cy = 500.0;
    b1.width = 200.0;
    b1.height = 100.0;
    b1.rotation = 0.0;

    TEST_ASSERT(ElementBoundsEngine::pointInRotatedRect(500.0, 500.0, b1));
    TEST_ASSERT(ElementBoundsEngine::pointInRotatedRect(599.0, 549.0, b1));
    TEST_ASSERT(!ElementBoundsEngine::pointInRotatedRect(650.0, 500.0, b1));

    // Rotate 90 deg -> width 100 on X, 200 on Y
    b1.rotation = 90.0;
    TEST_ASSERT(ElementBoundsEngine::pointInRotatedRect(500.0, 599.0, b1));
    TEST_ASSERT(!ElementBoundsEngine::pointInRotatedRect(599.0, 500.0, b1));

    // 2. Corner and Edge Handle Positions
    ElementBounds bSquare;
    bSquare.cx = 100.0;
    bSquare.cy = 100.0;
    bSquare.width = 100.0;
    bSquare.height = 100.0;
    bSquare.rotation = 0.0;

    Vec2D tl = ElementBoundsEngine::getCornerPosition(bSquare, BoundsCorner::TopLeft);
    TEST_ASSERT(std::abs(tl.x - 50.0) < 1e-4 && std::abs(tl.y - 50.0) < 1e-4);

    Vec2D br = ElementBoundsEngine::getCornerPosition(bSquare, BoundsCorner::BottomRight);
    TEST_ASSERT(std::abs(br.x - 150.0) < 1e-4 && std::abs(br.y - 150.0) < 1e-4);

    Vec2D rightH = ElementBoundsEngine::getEdgeHandlePosition(bSquare, BoundsEdge::Right);
    TEST_ASSERT(std::abs(rightH.x - 150.0) < 1e-4 && std::abs(rightH.y - 100.0) < 1e-4);

    Vec2D rotH = ElementBoundsEngine::getRotationHandlePosition(bSquare, 24.0);
    // localY = -(50 + 24) = -74 -> y = 100 - 74 = 26.0
    TEST_ASSERT(std::abs(rotH.x - 100.0) < 1e-4 && std::abs(rotH.y - 26.0) < 1e-4);

    // 3. Compute Visual Bounds
    Clip videoClip(ClipId("c-vid"), ClipType::Video, "Intro", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(10.0));
    videoClip.setParam("x", 50.0);
    videoClip.setParam("y", -30.0);
    videoClip.setParam("scaleX", 1.5);
    videoClip.setParam("scaleY", 1.5);
    videoClip.setParam("rotation", 45.0);

    auto vBounds = ElementBoundsEngine::computeVisualBounds(videoClip, 1920.0, 1080.0, TimelineTime(0), Size2D{1920.0, 1080.0});
    TEST_ASSERT(vBounds.has_value());
    TEST_ASSERT(std::abs(vBounds->cx - (960.0 + 50.0)) < 1e-4);
    TEST_ASSERT(std::abs(vBounds->cy - (540.0 - 30.0)) < 1e-4);
    TEST_ASSERT(std::abs(vBounds->width - (1920.0 * 1.5)) < 1e-4);
    TEST_ASSERT(std::abs(vBounds->height - (1080.0 * 1.5)) < 1e-4);
    TEST_ASSERT(std::abs(vBounds->rotation - 45.0) < 1e-4);

    // Audio clip returns nullopt
    Clip audioClip(ClipId("c-aud"), ClipType::Audio, "Music", TimelineTime(0), TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(!ElementBoundsEngine::computeVisualBounds(audioClip, 1920.0, 1080.0).has_value());

    // 4. Visible Elements with Bounds across Tracks
    Track mainTrack(TrackId("t-main"), TrackType::Video, "Main Video");
    mainTrack.clips().push_back(videoClip);

    Track overlayTrack(TrackId("t-ov"), TrackType::Graphic, "Overlay Graphic");
    Clip graphicClip(ClipId("c-graph"), ClipType::Graphic, "Watermark", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(5.0));
    overlayTrack.clips().push_back(graphicClip);

    std::vector<Track> tracks = { mainTrack, overlayTrack };

    // At t=0.5s: only main video is active
    auto vis0 = ElementBoundsEngine::getVisibleElementsWithBounds(tracks, TimelineTime::fromSeconds(0.5), 1920.0, 1080.0);
    TEST_ASSERT(vis0.size() == 1);
    TEST_ASSERT(vis0[0].elementId == "c-vid");

    // At t=2.0s: both are active, overlay track (index 1) comes first in reverse z-order
    auto vis2 = ElementBoundsEngine::getVisibleElementsWithBounds(tracks, TimelineTime::fromSeconds(2.0), 1920.0, 1080.0);
    TEST_ASSERT(vis2.size() == 2);
    TEST_ASSERT(vis2[0].elementId == "c-graph");
    TEST_ASSERT(vis2[1].elementId == "c-vid");

    // 5. Hit testing and Preferred Selection resolution
    double hitX = vis2[0].bounds.cx;
    double hitY = vis2[0].bounds.cy;
    auto hits = ElementBoundsEngine::getHitElements(hitX, hitY, vis2);
    TEST_ASSERT(!hits.empty());

    // Single hit test
    auto topHit = ElementBoundsEngine::hitTest(hitX, hitY, vis2);
    TEST_ASSERT(topHit.has_value());

    // Resolve preferred hit (user clicked where both overlap, but prefers "c-vid")
    auto prefHit = ElementBoundsEngine::resolvePreferredHit(hits, {"c-vid"});
    TEST_ASSERT(prefHit.has_value());
    TEST_ASSERT(prefHit->elementId == "c-vid");

    std::cout << "[PASS] runElementBoundsEngineTests" << std::endl;
}

void runKeybindingEngineTests() {
    using namespace catchim::editor;

    // 1. Normalization
    TEST_ASSERT(KeybindingEngine::normalizeShortcut("CTRL+Z") == "ctrl+z");
    TEST_ASSERT(KeybindingEngine::normalizeShortcut("shift+Ctrl+alt+z") == "ctrl+alt+shift+z");
    TEST_ASSERT(KeybindingEngine::normalizeShortcut("cmd+c") == "ctrl+c");
    TEST_ASSERT(KeybindingEngine::normalizeShortcut("opt+shift+k") == "alt+shift+k");
    TEST_ASSERT(KeybindingEngine::normalizeShortcut("esc") == "escape");
    TEST_ASSERT(KeybindingEngine::normalizeShortcut("  ctrl + space  ") == "ctrl+space");

    // 2. Checks
    TEST_ASSERT(KeybindingEngine::isKey("a"));
    TEST_ASSERT(KeybindingEngine::isKey("space"));
    TEST_ASSERT(KeybindingEngine::isKey("delete"));
    TEST_ASSERT(!KeybindingEngine::isKey("invalid_key_123"));

    TEST_ASSERT(KeybindingEngine::isShortcutKey("ctrl+z"));
    TEST_ASSERT(KeybindingEngine::isShortcutKey("space"));
    TEST_ASSERT(KeybindingEngine::isShortcutKey("ctrl+shift+right"));

    TEST_ASSERT(KeybindingEngine::isSingleCharacterShortcut("space"));
    TEST_ASSERT(KeybindingEngine::isSingleCharacterShortcut("k"));
    TEST_ASSERT(!KeybindingEngine::isSingleCharacterShortcut("ctrl+k"));

    TEST_ASSERT(KeybindingEngine::isModifierBasedShortcut("ctrl+k"));
    TEST_ASSERT(!KeybindingEngine::isModifierBasedShortcut("k"));

    // 3. Conflict validation
    std::unordered_map<std::string, std::string> currentShortcuts = {
        {"ctrl+z", "undo"},
        {"ctrl+c", "copy-selected"}
    };

    auto noConflict = KeybindingEngine::validateKeybinding(currentShortcuts, "ctrl+z", "undo");
    TEST_ASSERT(!noConflict.has_value());

    auto conflict = KeybindingEngine::validateKeybinding(currentShortcuts, "ctrl+z", "redo");
    TEST_ASSERT(conflict.has_value());
    TEST_ASSERT(conflict->existingAction == "undo");
    TEST_ASSERT(conflict->newAction == "redo");

    auto freeKey = KeybindingEngine::validateKeybinding(currentShortcuts, "ctrl+v", "paste-copied");
    TEST_ASSERT(!freeKey.has_value());

    // 4. Default shortcuts
    auto defaults = KeybindingEngine::getDefaultShortcuts();
    TEST_ASSERT(defaults.size() >= 20);
    TEST_ASSERT(defaults["space"] == "toggle-play");
    TEST_ASSERT(defaults["s"] == "split");
    TEST_ASSERT(defaults["ctrl+z"] == "undo");
    TEST_ASSERT(defaults["ctrl+shift+z"] == "redo");

    // 5. Export / Import JSON Config
    auto exported = KeybindingEngine::exportConfig(defaults);
    TEST_ASSERT(exported.is_object());
    TEST_ASSERT(exported["space"] == "toggle-play");

    auto imported = KeybindingEngine::importConfig(exported);
    TEST_ASSERT(imported.size() == defaults.size());
    TEST_ASSERT(imported["space"] == "toggle-play");

    std::cout << "[PASS] runKeybindingEngineTests" << std::endl;
}

void runProjectAutosaveEngineTests() {
    using namespace catchim::editor;

    ProjectAutosaveEngine engine(500);
    TEST_ASSERT(engine.debounceIntervalMs() == 500);
    TEST_ASSERT(!engine.isPaused());
    TEST_ASSERT(!engine.isDirty());
    TEST_ASSERT(!engine.isSaving());
    TEST_ASSERT(!engine.hasPendingSave());

    // 1. Mark dirty & pause/resume
    engine.markDirty();
    TEST_ASSERT(engine.isDirty());
    TEST_ASSERT(engine.hasPendingSave());

    engine.pause();
    TEST_ASSERT(engine.isPaused());

    engine.resume();
    TEST_ASSERT(!engine.isPaused());

    // 2. Can save now checks
    TEST_ASSERT(!engine.canSaveNow(false, false, false)); // No project
    TEST_ASSERT(!engine.canSaveNow(true, true, false));   // Loading
    TEST_ASSERT(!engine.canSaveNow(true, false, true));   // Migrating
    TEST_ASSERT(engine.canSaveNow(true, false, false));    // Ready

    // 3. Flush execution with executor
    bool saveExecuted = false;
    engine.setSaveExecutor([&]() {
        saveExecuted = true;
        return true;
    });

    bool flushed = engine.flush(true, false, false);
    TEST_ASSERT(flushed);
    TEST_ASSERT(saveExecuted);
    TEST_ASSERT(!engine.isDirty());
    TEST_ASSERT(!engine.hasPendingSave());

    // 4. Status listener
    bool listenerNotified = false;
    engine.setStatusListener([&](bool dirty) {
        if (dirty) listenerNotified = true;
    });
    engine.markDirty();
    TEST_ASSERT(listenerNotified);

    std::cout << "[PASS] runProjectAutosaveEngineTests" << std::endl;
}

void runScenesManagerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    Project project("Multi-Scene Studio");
    ScenesManager manager(project);

    // 1. Invariant: Main scene ensured
    TEST_ASSERT(!project.scenes().empty());
    TEST_ASSERT(manager.activeScene() != nullptr);
    TEST_ASSERT(manager.activeScene()->isMain());

    auto mainSceneId = manager.activeSceneId();

    // 2. Create Scene 2
    auto s2Id = manager.createScene("B-Roll Clip Scene", false);
    TEST_ASSERT(project.scenes().size() == 2);
    TEST_ASSERT(manager.activeSceneId() == s2Id);
    TEST_ASSERT(manager.activeScene()->name() == "B-Roll Clip Scene");
    TEST_ASSERT(!manager.activeScene()->isMain());

    // 3. Rename Scene
    bool renamed = manager.renameScene(s2Id, "Cinematic B-Roll");
    TEST_ASSERT(renamed);
    TEST_ASSERT(manager.activeScene()->name() == "Cinematic B-Roll");

    // 4. Deletion constraints
    auto mainCheck = manager.canDeleteScene(mainSceneId);
    TEST_ASSERT(!mainCheck.canDelete); // Main scene protected

    auto s2Check = manager.canDeleteScene(s2Id);
    TEST_ASSERT(s2Check.canDelete);

    // 5. Active Scene Bookmarks
    TimelineTime t1 = TimelineTime::fromSeconds(2.0);
    FrameRate fps{30, 1};
    TEST_ASSERT(!manager.isBookmarkedAtTime(t1, fps));

    manager.toggleBookmark(t1, "Camera Angle Switch", "#00FF00", fps);
    TEST_ASSERT(manager.isBookmarkedAtTime(t1, fps));

    const Bookmark* bm = manager.getBookmarkAtTime(t1, fps);
    TEST_ASSERT(bm != nullptr);
    TEST_ASSERT(bm->note == "Camera Angle Switch");
    TEST_ASSERT(bm->color == "#00FF00");

    // Toggle again removes it
    manager.toggleBookmark(t1, "", "", fps);
    TEST_ASSERT(!manager.isBookmarkedAtTime(t1, fps));

    // 6. Delete scene and fallback to main
    bool deleted = manager.deleteScene(s2Id);
    TEST_ASSERT(deleted);
    TEST_ASSERT(project.scenes().size() == 1);
    TEST_ASSERT(manager.activeSceneId() == mainSceneId);

    std::cout << "[PASS] runScenesManagerTests" << std::endl;
}

void runMathExpressionEvaluatorTests() {
    using namespace catchim::core;

    // 1. Basic arithmetic expressions
    auto r1 = MathExpressionEvaluator::evaluateMathExpression("10 + 20");
    TEST_ASSERT(r1.has_value() && *r1 == 30.0);

    auto r2 = MathExpressionEvaluator::evaluateMathExpression("1920 / 2");
    TEST_ASSERT(r2.has_value() && *r2 == 960.0);

    auto r3 = MathExpressionEvaluator::evaluateMathExpression("1080 * 0.5");
    TEST_ASSERT(r3.has_value() && *r3 == 540.0);

    auto r4 = MathExpressionEvaluator::evaluateMathExpression("100 - 45");
    TEST_ASSERT(r4.has_value() && *r4 == 55.0);

    // 2. Precedence and parentheses
    auto r5 = MathExpressionEvaluator::evaluateMathExpression("(10 + 20) * 3");
    TEST_ASSERT(r5.has_value() && *r5 == 90.0);

    auto r6 = MathExpressionEvaluator::evaluateMathExpression("10 + 20 * 3");
    TEST_ASSERT(r6.has_value() && *r6 == 70.0);

    auto r7 = MathExpressionEvaluator::evaluateMathExpression("((5 + 5) * (3 + 2)) / 2");
    TEST_ASSERT(r7.has_value() && *r7 == 25.0);

    // 3. Negatives and decimals
    auto r8 = MathExpressionEvaluator::evaluateMathExpression("-15.5 + 30.5");
    TEST_ASSERT(r8.has_value() && *r8 == 15.0);

    auto r9 = MathExpressionEvaluator::evaluateMathExpression("-(10 + 5)");
    TEST_ASSERT(r9.has_value() && *r9 == -15.0);

    // 4. Safe error handling
    TEST_ASSERT(!MathExpressionEvaluator::evaluateMathExpression("10 / 0").has_value());
    TEST_ASSERT(!MathExpressionEvaluator::evaluateMathExpression("10 + ").has_value());
    TEST_ASSERT(!MathExpressionEvaluator::evaluateMathExpression("invalid * 5").has_value());
    TEST_ASSERT(!MathExpressionEvaluator::evaluateMathExpression("(10 + 5").has_value());
    TEST_ASSERT(!MathExpressionEvaluator::evaluateMathExpression("").has_value());

    // 5. snapToStep
    TEST_ASSERT(std::abs(MathExpressionEvaluator::snapToStep(12.345, 0.1) - 12.3) < 1e-4);
    TEST_ASSERT(std::abs(MathExpressionEvaluator::snapToStep(12.36, 0.1) - 12.4) < 1e-4);
    TEST_ASSERT(std::abs(MathExpressionEvaluator::snapToStep(127.0, 50.0) - 150.0) < 1e-4);

    // 6. formatNumberForDisplay
    TEST_ASSERT(MathExpressionEvaluator::formatNumberForDisplay(12.34000, 0, 4) == "12.34");
    TEST_ASSERT(MathExpressionEvaluator::formatNumberForDisplay(12.0000, 0, 4) == "12");
    TEST_ASSERT(MathExpressionEvaluator::formatNumberForDisplay(12.0000, 2, 4) == "12.00");

    // 7. isNearlyEqual
    TEST_ASSERT(MathExpressionEvaluator::isNearlyEqual(1.00005, 1.00006, 0.001));
    TEST_ASSERT(!MathExpressionEvaluator::isNearlyEqual(1.0, 1.1, 0.001));

    std::cout << "[PASS] runMathExpressionEvaluatorTests" << std::endl;
}

void runTrackCapabilityEngineTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. Capability rules
    TEST_ASSERT(TrackCapabilityEngine::canTrackHaveAudio(TrackType::Audio));
    TEST_ASSERT(TrackCapabilityEngine::canTrackHaveAudio(TrackType::Video));
    TEST_ASSERT(!TrackCapabilityEngine::canTrackHaveAudio(TrackType::Text));
    TEST_ASSERT(!TrackCapabilityEngine::canTrackHaveAudio(TrackType::Graphic));
    TEST_ASSERT(!TrackCapabilityEngine::canTrackHaveAudio(TrackType::Effect));

    TEST_ASSERT(TrackCapabilityEngine::canTrackBeHidden(TrackType::Video));
    TEST_ASSERT(TrackCapabilityEngine::canTrackBeHidden(TrackType::Text));
    TEST_ASSERT(TrackCapabilityEngine::canTrackBeHidden(TrackType::Graphic));
    TEST_ASSERT(TrackCapabilityEngine::canTrackBeHidden(TrackType::Effect));
    TEST_ASSERT(!TrackCapabilityEngine::canTrackBeHidden(TrackType::Audio));

    // 2. Timeline track and clip manipulation
    Timeline timeline;
    auto& textTrack = timeline.addTrack(TrackType::Text, "Subtitles Track");
    auto textTrackId = textTrack.id();

    auto& audioTrack = timeline.addTrack(TrackType::Audio, "Background Music");
    auto audioTrackId = audioTrack.id();

    // Text track can be hidden but not muted
    TEST_ASSERT(TrackCapabilityEngine::setTrackHidden(timeline, textTrackId, true));
    TEST_ASSERT(textTrack.isHidden());
    TEST_ASSERT(!TrackCapabilityEngine::setTrackMuted(timeline, textTrackId, true));

    // Audio track cannot be hidden but can be muted
    TEST_ASSERT(!TrackCapabilityEngine::setTrackHidden(timeline, audioTrackId, true));
    TEST_ASSERT(TrackCapabilityEngine::setTrackMuted(timeline, audioTrackId, true));
    TEST_ASSERT(audioTrack.isMuted());

    // 3. Clip hierarchy query and predicate update
    Clip textClip(ClipId("c-txt-1"), ClipType::Text, "Initial Title", TimelineTime(0), TimelineTime::fromSeconds(3.0));
    timeline.addClip(textTrackId, textClip);

    bool updated = TrackCapabilityEngine::updateClipInTimeline(
        timeline,
        ClipId("c-txt-1"),
        [](Clip& c) {
            c.setName("Refined Title");
        },
        [](const Clip& c) {
            return c.duration() > TimelineTime(0);
        }
    );
    TEST_ASSERT(updated);

    auto* foundClip = timeline.findClip(ClipId("c-txt-1"));
    TEST_ASSERT(foundClip != nullptr);
    TEST_ASSERT(foundClip->name() == "Refined Title");

    std::cout << "[PASS] runTrackCapabilityEngineTests" << std::endl;
}

void runStorageQuotaEngineTests() {
    using namespace catchim::storage;

    // 1. formatStorageBytes
    TEST_ASSERT(StorageQuotaEngine::formatStorageBytes(0) == "0 B");
    TEST_ASSERT(StorageQuotaEngine::formatStorageBytes(500) == "500 B");
    TEST_ASSERT(StorageQuotaEngine::formatStorageBytes(1024) == "1 KB");
    TEST_ASSERT(StorageQuotaEngine::formatStorageBytes(1536) == "1.5 KB");
    TEST_ASSERT(StorageQuotaEngine::formatStorageBytes(10ULL * 1024ULL * 1024ULL) == "10 MB");
    TEST_ASSERT(StorageQuotaEngine::formatStorageBytes(1536ULL * 1024ULL * 1024ULL) == "1.5 GB");
    TEST_ASSERT(StorageQuotaEngine::formatStorageBytes(2ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL) == "2 TB");

    // 2. evaluateStorageCapacity
    // When quota is unavailable
    auto unavail = StorageQuotaEngine::evaluateStorageCapacity(1000);
    TEST_ASSERT(unavail.canStore);
    TEST_ASSERT(unavail.status == StorageCapacityStatus::EstimateUnavailable);

    // Quota: 1000MB, Usage: 800MB -> Headroom: 200MB -> Reserve: 50MB -> Available: 150MB
    uint64_t quota = 1000ULL * 1024ULL * 1024ULL;
    uint64_t usage = 800ULL * 1024ULL * 1024ULL;

    // 100MB fits in 150MB
    uint64_t req100 = 100ULL * 1024ULL * 1024ULL;
    auto res1 = StorageQuotaEngine::evaluateStorageCapacity(req100, quota, usage);
    TEST_ASSERT(res1.canStore);
    TEST_ASSERT(res1.status == StorageCapacityStatus::EnoughSpace);
    TEST_ASSERT(res1.headroomBytes == 200ULL * 1024ULL * 1024ULL);
    TEST_ASSERT(res1.availableBytes == 150ULL * 1024ULL * 1024ULL);

    // 160MB exceeds 150MB
    uint64_t req160 = 160ULL * 1024ULL * 1024ULL;
    auto res2 = StorageQuotaEngine::evaluateStorageCapacity(req160, quota, usage);
    TEST_ASSERT(!res2.canStore);
    TEST_ASSERT(res2.status == StorageCapacityStatus::InsufficientSpace);

    // When headroom is less than 50MB reserve -> Available is 0
    uint64_t smallQuota = 100ULL * 1024ULL * 1024ULL;
    uint64_t highUsage = 80ULL * 1024ULL * 1024ULL; // Headroom 20MB < 50MB
    auto res3 = StorageQuotaEngine::evaluateStorageCapacity(1024, smallQuota, highUsage);
    TEST_ASSERT(!res3.canStore);
    TEST_ASSERT(res3.availableBytes == 0);

    std::cout << "[PASS] runStorageQuotaEngineTests" << std::endl;
}

void runTimelineElementUpdatePipelineTests() {
    using namespace catchim::editor;

    // 1. isRetimableType
    TEST_ASSERT(TimelineElementUpdatePipeline::isRetimableType(ClipType::Video));
    TEST_ASSERT(TimelineElementUpdatePipeline::isRetimableType(ClipType::Audio));
    TEST_ASSERT(!TimelineElementUpdatePipeline::isRetimableType(ClipType::Text));
    TEST_ASSERT(!TimelineElementUpdatePipeline::isRetimableType(ClipType::Image));
    TEST_ASSERT(!TimelineElementUpdatePipeline::isRetimableType(ClipType::Graphic));

    // 2. Basic properties patch
    Clip clip(ClipId("clip-1"), ClipType::Video, "Original Name",
             TimelineTime::fromSeconds(2), TimelineTime::fromSeconds(10));
    ElementUpdatePatch patch1;
    patch1.name = "Updated Name";
    patch1.hidden = true;
    patch1.muted = true;
    ElementUpdateContext ctx1{"track-video-1", false, {}};

    Clip updated1 = TimelineElementUpdatePipeline::applyElementUpdate(clip, patch1, ctx1);
    TEST_ASSERT(updated1.name() == "Updated Name");
    TEST_ASSERT(updated1.isHidden());
    TEST_ASSERT(updated1.isMuted());

    // 3. Derive rules: Retime changes -> duration derives
    // Original duration is 10s at 1.0x. Changing speed to 2.0x -> duration becomes 5s
    ElementUpdatePatch patch2;
    patch2.retimeRate = 2.0;
    Clip updated2 = TimelineElementUpdatePipeline::applyElementUpdate(clip, patch2, ctx1);
    TEST_ASSERT(updated2.duration() == TimelineTime::fromSeconds(5));
    TEST_ASSERT(updated2.getParam<double>("speed", 1.0) == 2.0);

    // Retime with trims: source duration = 12s, trimStart = 2s, trimEnd = 2s -> visible = 8s
    // At speed 2.0x -> duration = 4s
    Clip clipWithTrim(ClipId("clip-2"), ClipType::Video, "Trimmed",
                     TimelineTime::fromSeconds(0), TimelineTime::fromSeconds(8),
                     TimelineTime::fromSeconds(2), TimelineTime::fromSeconds(2));
    clipWithTrim.setSourceDuration(TimelineTime::fromSeconds(12));
    ElementUpdatePatch patch3;
    patch3.retimeRate = 2.0;
    Clip updated3 = TimelineElementUpdatePipeline::applyElementUpdate(clipWithTrim, patch3, ctx1);
    TEST_ASSERT(updated3.duration() == TimelineTime::fromSeconds(4));

    // Non-retimable clip (Text) does not change duration when retimeRate is patched
    Clip textClip(ClipId("clip-txt"), ClipType::Text, "Text Clip",
                  TimelineTime::fromSeconds(1), TimelineTime::fromSeconds(5));
    ElementUpdatePatch patchText;
    patchText.retimeRate = 2.0;
    Clip updatedText = TimelineElementUpdatePipeline::applyElementUpdate(textClip, patchText, ctx1);
    TEST_ASSERT(updatedText.duration() == TimelineTime::fromSeconds(5));

    // 4. Enforce rules: Clamping keyframes to duration
    Clip animClip(ClipId("clip-anim"), ClipType::Video, "Anim Clip",
                  TimelineTime::fromSeconds(0), TimelineTime::fromSeconds(10));
    auto& ch = animClip.getOrCreateAnimationChannel("opacity", 1.0);
    ch.addOrUpdateKeyframe(Keyframe{TimelineTime::fromSeconds(2), 0.2});
    ch.addOrUpdateKeyframe(Keyframe{TimelineTime::fromSeconds(5), 0.5});
    ch.addOrUpdateKeyframe(Keyframe{TimelineTime::fromSeconds(9), 0.9});
    TEST_ASSERT(ch.keyframes().size() == 3);

    // Patch duration to 4 seconds -> keyframes at 5s and 9s should be truncated
    ElementUpdatePatch patchDur;
    patchDur.duration = TimelineTime::fromSeconds(4);
    Clip updatedAnim = TimelineElementUpdatePipeline::applyElementUpdate(animClip, patchDur, ctx1);
    TEST_ASSERT(updatedAnim.duration() == TimelineTime::fromSeconds(4));
    const auto* chUpdated = updatedAnim.findAnimationChannel("opacity");
    TEST_ASSERT(chUpdated != nullptr);
    for (const auto& kf : chUpdated->keyframes()) {
        TEST_ASSERT(kf.time <= TimelineTime::fromSeconds(4));
    }

    // 5. Enforce rules: Non-negative startTime & Main Track Invariant
    ElementUpdatePatch patchNegStart;
    patchNegStart.startTime = TimelineTime::fromSeconds(-5);
    Clip updatedNeg = TimelineElementUpdatePipeline::applyElementUpdate(clip, patchNegStart, ctx1);
    TEST_ASSERT(updatedNeg.startTime() == TimelineTime::fromSeconds(0));

    // Main track zero-start invariant:
    // If it's the only clip on main track, requested startTime = 5s must snap to 0s
    ElementUpdateContext mainCtxOnly{"main", true, {}};
    ElementUpdatePatch patchStart5;
    patchStart5.startTime = TimelineTime::fromSeconds(5);
    Clip updatedMainOnly = TimelineElementUpdatePipeline::applyElementUpdate(clip, patchStart5, mainCtxOnly);
    TEST_ASSERT(updatedMainOnly.startTime() == TimelineTime::fromSeconds(0));

    // If there is another earlier clip on main track starting at 0s, requested startTime 5s is preserved
    auto otherClip = std::make_shared<Clip>(ClipId("clip-other"), ClipType::Video, "Other",
                                            TimelineTime::fromSeconds(0), TimelineTime::fromSeconds(4));
    ElementUpdateContext mainCtxWithOther{"main", true, {otherClip}};
    Clip updatedMainWithOther = TimelineElementUpdatePipeline::applyElementUpdate(clip, patchStart5, mainCtxWithOther);
    TEST_ASSERT(updatedMainWithOther.startTime() == TimelineTime::fromSeconds(5));

    std::cout << "[PASS] runTimelineElementUpdatePipelineTests" << std::endl;
}

void runExportCanvasGeometryResolverTests() {
    using namespace catchim::exporting;
    using namespace catchim::editor;

    // 1. roundToEven
    TEST_ASSERT(ExportCanvasGeometryResolver::roundToEven(480.0) == 480);
    TEST_ASSERT(ExportCanvasGeometryResolver::roundToEven(481.0) == 482);
    TEST_ASSERT(ExportCanvasGeometryResolver::roundToEven(481.4) == 482);
    TEST_ASSERT(ExportCanvasGeometryResolver::roundToEven(1.0) == 2);
    TEST_ASSERT(ExportCanvasGeometryResolver::roundToEven(0.0) == 2);

    // 2. Preset target heights
    TEST_ASSERT(ExportCanvasGeometryResolver::getPresetTargetHeight(ExportResolutionPreset::Source) == 0);
    TEST_ASSERT(ExportCanvasGeometryResolver::getPresetTargetHeight(ExportResolutionPreset::P480) == 480);
    TEST_ASSERT(ExportCanvasGeometryResolver::getPresetTargetHeight(ExportResolutionPreset::P720) == 720);
    TEST_ASSERT(ExportCanvasGeometryResolver::getPresetTargetHeight(ExportResolutionPreset::P1080) == 1080);
    TEST_ASSERT(ExportCanvasGeometryResolver::getPresetTargetHeight(ExportResolutionPreset::P1440) == 1440);
    TEST_ASSERT(ExportCanvasGeometryResolver::getPresetTargetHeight(ExportResolutionPreset::P2160) == 2160);
    TEST_ASSERT(ExportCanvasGeometryResolver::getPresetTargetHeight(ExportResolutionPreset::P4320) == 4320);

    // 3. resolveExportCanvasSize
    CanvasSize source1080{1920, 1080}; // 16:9
    auto resSource = ExportCanvasGeometryResolver::resolveExportCanvasSize(source1080, ExportResolutionPreset::Source);
    TEST_ASSERT(resSource.width == 1920 && resSource.height == 1080);

    auto res720 = ExportCanvasGeometryResolver::resolveExportCanvasSize(source1080, ExportResolutionPreset::P720);
    TEST_ASSERT(res720.width == 1280 && res720.height == 720);

    auto res480 = ExportCanvasGeometryResolver::resolveExportCanvasSize(source1080, ExportResolutionPreset::P480);
    // 480 * (16 / 9) = 853.333 -> roundToEven = 854
    TEST_ASSERT(res480.width == 854 && res480.height == 480);

    auto res4K = ExportCanvasGeometryResolver::resolveExportCanvasSize(source1080, ExportResolutionPreset::P2160);
    TEST_ASSERT(res4K.width == 3840 && res4K.height == 2160);

    // Vertical video: 1080x1920 (9:16) scaled to 720p height -> 720 * (9/16) = 405 -> roundToEven = 406
    CanvasSize sourceVertical{1080, 1920};
    auto resVert720 = ExportCanvasGeometryResolver::resolveExportCanvasSize(sourceVertical, ExportResolutionPreset::P720);
    TEST_ASSERT(resVert720.width == 406 && resVert720.height == 720);

    // 4. MIME type and Extension
    TEST_ASSERT(ExportCanvasGeometryResolver::getExportMimeType(ExportFormat::MP4) == "video/mp4");
    TEST_ASSERT(ExportCanvasGeometryResolver::getExportMimeType(ExportFormat::WebM) == "video/webm");
    TEST_ASSERT(ExportCanvasGeometryResolver::getExportFileExtension(ExportFormat::MP4) == ".mp4");
    TEST_ASSERT(ExportCanvasGeometryResolver::getExportFileExtension(ExportFormat::WebM) == ".webm");

    // 5. Preset string conversions
    TEST_ASSERT(ExportCanvasGeometryResolver::resolutionPresetToString(ExportResolutionPreset::P1080) == "1080p");
    TEST_ASSERT(ExportCanvasGeometryResolver::stringToResolutionPreset("1080p") == ExportResolutionPreset::P1080);
    TEST_ASSERT(ExportCanvasGeometryResolver::stringToResolutionPreset("unknown") == ExportResolutionPreset::Source);

    std::cout << "[PASS] runExportCanvasGeometryResolverTests" << std::endl;
}

void runMediaAssetInspectorTests() {
    using namespace catchim::media;

    // 1. mediaSupportsAudio
    TEST_ASSERT(MediaAssetInspector::mediaSupportsAudio(MediaType::Video));
    TEST_ASSERT(MediaAssetInspector::mediaSupportsAudio(MediaType::Audio));
    TEST_ASSERT(!MediaAssetInspector::mediaSupportsAudio(MediaType::Image));

    // 2. getMediaTypeFromMimeType
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromMimeType("video/mp4") == MediaType::Video);
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromMimeType("video/quicktime") == MediaType::Video);
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromMimeType("audio/mpeg") == MediaType::Audio);
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromMimeType("audio/wav") == MediaType::Audio);
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromMimeType("image/png") == MediaType::Image);
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromMimeType("image/jpeg") == MediaType::Image);
    TEST_ASSERT(!MediaAssetInspector::getMediaTypeFromMimeType("application/pdf").has_value());

    // 3. getMediaTypeFromExtension
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromExtension(".mp4") == MediaType::Video);
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromExtension("MOV") == MediaType::Video);
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromExtension(".mkv") == MediaType::Video);
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromExtension(".mp3") == MediaType::Audio);
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromExtension("wav") == MediaType::Audio);
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromExtension(".flac") == MediaType::Audio);
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromExtension(".png") == MediaType::Image);
    TEST_ASSERT(MediaAssetInspector::getMediaTypeFromExtension("JPG") == MediaType::Image);
    TEST_ASSERT(!MediaAssetInspector::getMediaTypeFromExtension(".txt").has_value());

    // 4. calculateThumbnailSize (max 1280x720)
    // 4K 16:9 (3840x2160) -> 1280x720
    auto thumb4K = MediaAssetInspector::calculateThumbnailSize(3840, 2160);
    TEST_ASSERT(thumb4K.width == 1280 && thumb4K.height == 720);

    // 1080p 16:9 (1920x1080) -> 1280x720
    auto thumb1080 = MediaAssetInspector::calculateThumbnailSize(1920, 1080);
    TEST_ASSERT(thumb1080.width == 1280 && thumb1080.height == 720);

    // Vertical video (1080x1920, 9:16) -> height constrained to 720, width = round(720 * 9/16) = 405
    auto thumbVert = MediaAssetInspector::calculateThumbnailSize(1080, 1920);
    TEST_ASSERT(thumbVert.width == 405 && thumbVert.height == 720);

    // Smaller image (800x600) -> fits directly
    auto thumbSmall = MediaAssetInspector::calculateThumbnailSize(800, 600);
    TEST_ASSERT(thumbSmall.width == 800 && thumbSmall.height == 600);

    // Invalid dimensions
    auto thumbZero = MediaAssetInspector::calculateThumbnailSize(0, 0);
    TEST_ASSERT(thumbZero.width == 0 && thumbZero.height == 0);

    // 5. getUnsupportedVideoDescription
    std::string hevcDesc = MediaAssetInspector::getUnsupportedVideoDescription("hevc");
    TEST_ASSERT(hevcDesc.find("Convert it to H.264 MP4 or try importing it in Safari.") != std::string::npos);

    std::string proresDesc = MediaAssetInspector::getUnsupportedVideoDescription("prores");
    TEST_ASSERT(proresDesc.find("PRORES cannot be decoded") != std::string::npos);

    // 6. getStorageLimitDescription
    uint64_t fileSize = 100ULL * 1024ULL * 1024ULL; // 100 MB
    uint64_t avail = 50ULL * 1024ULL * 1024ULL; // 50 MB
    std::string limitDesc = MediaAssetInspector::getStorageLimitDescription(fileSize, avail);
    TEST_ASSERT(limitDesc.find("100 MB") != std::string::npos);
    TEST_ASSERT(limitDesc.find("50 MB") != std::string::npos);

    std::string noAvailDesc = MediaAssetInspector::getStorageLimitDescription(fileSize);
    TEST_ASSERT(noAvailDesc == "File size is 100 MB.");

    std::cout << "[PASS] runMediaAssetInspectorTests" << std::endl;
}

void runTimelineCoordinateEngineTests() {
    using namespace catchim::editor;

    // 1. getTimelinePixelsPerSecond
    TEST_ASSERT(TimelineCoordinateEngine::getTimelinePixelsPerSecond(1.0) == 50.0);
    TEST_ASSERT(TimelineCoordinateEngine::getTimelinePixelsPerSecond(2.0) == 100.0);
    TEST_ASSERT(TimelineCoordinateEngine::getTimelinePixelsPerSecond(0.05) == 5.0); // Clamped to 0.1
    TEST_ASSERT(TimelineCoordinateEngine::getTimelinePixelsPerSecond(200.0) == 5000.0); // Clamped to 100.0

    // 2. timelineTimeToPixels & pixelsToTimelineTime
    TimelineTime t2s = TimelineTime::fromSeconds(2.0);
    TEST_ASSERT(TimelineCoordinateEngine::timelineTimeToPixels(t2s, 1.0) == 100.0);
    TEST_ASSERT(TimelineCoordinateEngine::timelineTimeToPixels(t2s, 2.0) == 200.0);

    TimelineTime rev1 = TimelineCoordinateEngine::pixelsToTimelineTime(100.0, 1.0);
    TEST_ASSERT(rev1 == t2s);

    TimelineTime rev2 = TimelineCoordinateEngine::pixelsToTimelineTime(200.0, 2.0);
    TEST_ASSERT(rev2 == t2s);

    TimelineTime revNeg = TimelineCoordinateEngine::pixelsToTimelineTime(-50.0, 1.0);
    TEST_ASSERT(revNeg == TimelineTime(0));

    // 3. snapPixelToDeviceGrid
    double snapped1 = TimelineCoordinateEngine::snapPixelToDeviceGrid(100.333, 1.0);
    TEST_ASSERT(snapped1 == 100.0);

    double snapped2 = TimelineCoordinateEngine::snapPixelToDeviceGrid(100.333, 2.0);
    TEST_ASSERT(snapped2 == 100.5);

    // 4. timelineTimeToSnappedPixels
    double snappedTimePx = TimelineCoordinateEngine::timelineTimeToSnappedPixels(t2s, 1.0, 1.0);
    TEST_ASSERT(snappedTimePx == 100.0);

    // 5. getCenteredLineLeft
    TEST_ASSERT(TimelineCoordinateEngine::getCenteredLineLeft(100.0, 2.0) == 99.0);
    TEST_ASSERT(TimelineCoordinateEngine::getCenteredLineLeft(50.0, 4.0) == 48.0);

    // 6. getMouseTimeFromClientX
    // clientX = 250, containerLeft = 50, scrollLeft = 0, zoom = 1.0 -> mouseX = 200 -> 4 seconds
    TimelineTime mouseT1 = TimelineCoordinateEngine::getMouseTimeFromClientX(250.0, 50.0, 0.0, 1.0);
    TEST_ASSERT(mouseT1 == TimelineTime::fromSeconds(4.0));

    // clientX = 250, containerLeft = 50, scrollLeft = 100, zoom = 1.0 -> mouseX = 300 -> 6 seconds
    TimelineTime mouseT2 = TimelineCoordinateEngine::getMouseTimeFromClientX(250.0, 50.0, 100.0, 1.0);
    TEST_ASSERT(mouseT2 == TimelineTime::fromSeconds(6.0));

    // clientX = 20, containerLeft = 50, scrollLeft = 0, zoom = 1.0 -> mouseX = -30 -> 0 seconds
    TimelineTime mouseT3 = TimelineCoordinateEngine::getMouseTimeFromClientX(20.0, 50.0, 0.0, 1.0);
    TEST_ASSERT(mouseT3 == TimelineTime(0));

    std::cout << "[PASS] runTimelineCoordinateEngineTests" << std::endl;
}

void runProjectFrameRateEngineTests() {
    using namespace catchim::core;

    // 1. getFpsPresets
    const auto& presets = ProjectFrameRateEngine::getFpsPresets();
    TEST_ASSERT(presets.size() >= 5);
    TEST_ASSERT(presets[0].fps == 24);
    TEST_ASSERT(presets[1].fps == 25);
    TEST_ASSERT(presets[2].fps == 30);
    TEST_ASSERT(presets[3].fps == 60);
    TEST_ASSERT(presets[4].fps == 120);

    // 2. frameRateToFloat & frameRatesEqual
    FrameRate fps30{30, 1};
    TEST_ASSERT(ProjectFrameRateEngine::frameRateToFloat(fps30) == 30.0);
    TEST_ASSERT(ProjectFrameRateEngine::frameRatesEqual(fps30, FrameRate{30, 1}));
    TEST_ASSERT(!ProjectFrameRateEngine::frameRatesEqual(fps30, FrameRate{60, 1}));

    // 3. floatToFrameRate
    FrameRate r23976 = ProjectFrameRateEngine::floatToFrameRate(23.976);
    TEST_ASSERT(r23976.numerator == 24000 && r23976.denominator == 1001);

    FrameRate r2997 = ProjectFrameRateEngine::floatToFrameRate(29.97);
    TEST_ASSERT(r2997.numerator == 30000 && r2997.denominator == 1001);

    FrameRate r5994 = ProjectFrameRateEngine::floatToFrameRate(59.94);
    TEST_ASSERT(r5994.numerator == 60000 && r5994.denominator == 1001);

    FrameRate r60 = ProjectFrameRateEngine::floatToFrameRate(60.0);
    TEST_ASSERT(r60.numerator == 60 && r60.denominator == 1);

    // 4. gcd
    TEST_ASSERT(ProjectFrameRateEngine::gcd(24, 36) == 12);
    TEST_ASSERT(ProjectFrameRateEngine::gcd(100, 10) == 10);

    // 5. getHighestImportedVideoFps
    std::vector<MediaAssetFpsInfo> assets = {
        {"image", std::nullopt},
        {"audio", 120.0}, // audio fps ignored
        {"video", 24.0},
        {"video", 60.0},
        {"video", -10.0} // invalid fps ignored
    };
    auto highest = ProjectFrameRateEngine::getHighestImportedVideoFps(assets);
    TEST_ASSERT(highest.has_value());
    TEST_ASSERT(*highest == 60.0);

    std::vector<MediaAssetFpsInfo> audioOnly = { {"audio", 44100.0} };
    TEST_ASSERT(!ProjectFrameRateEngine::getHighestImportedVideoFps(audioOnly).has_value());

    // 6. getRaisedProjectFpsForImportedMedia
    // Project 30fps with 60fps video import -> raises to 60fps
    auto raised = ProjectFrameRateEngine::getRaisedProjectFpsForImportedMedia(fps30, assets);
    TEST_ASSERT(raised.has_value());
    TEST_ASSERT(raised->numerator == 60 && raised->denominator == 1);

    // Project 60fps with 60fps video import -> no raise
    FrameRate fps60{60, 1};
    auto noRaise = ProjectFrameRateEngine::getRaisedProjectFpsForImportedMedia(fps60, assets);
    TEST_ASSERT(!noRaise.has_value());

    std::cout << "[PASS] runProjectFrameRateEngineTests" << std::endl;
}

void runGuideOverlayEngineTests() {
    using namespace catchim::render;

    // 1. getAllGuides & isGuideId & getGuideById
    const auto& guides = GuideOverlayEngine::getAllGuides();
    TEST_ASSERT(guides.size() >= 5);
    TEST_ASSERT(GuideOverlayEngine::isGuideId("grid"));
    TEST_ASSERT(GuideOverlayEngine::isGuideId("tiktok"));
    TEST_ASSERT(GuideOverlayEngine::isGuideId("ig-reels"));
    TEST_ASSERT(GuideOverlayEngine::isGuideId("yt-shorts"));
    TEST_ASSERT(GuideOverlayEngine::isGuideId("spotlight"));
    TEST_ASSERT(!GuideOverlayEngine::isGuideId("nonexistent"));

    auto tiktokMeta = GuideOverlayEngine::getGuideById("tiktok");
    TEST_ASSERT(tiktokMeta.has_value());
    TEST_ASSERT(tiktokMeta->domain == "tiktok.com");
    TEST_ASSERT(tiktokMeta->type == OverlayGuideType::TikTok);

    // 2. calculateGridLines
    // 1920x1080 canvas with 3x3 grid
    auto gridLines = GuideOverlayEngine::calculateGridLines(1920.0, 1080.0, GridConfig{3, 3});
    // 2 vertical lines + 2 horizontal lines = 4 lines
    TEST_ASSERT(gridLines.size() == 4);

    // Vertical line 1: x = 640
    TEST_ASSERT(gridLines[0].x1 == 640.0 && gridLines[0].y1 == 0.0 && gridLines[0].x2 == 640.0 && gridLines[0].y2 == 1080.0);
    // Vertical line 2: x = 1280
    TEST_ASSERT(gridLines[1].x1 == 1280.0 && gridLines[1].y1 == 0.0 && gridLines[1].x2 == 1280.0 && gridLines[1].y2 == 1080.0);
    // Horizontal line 1: y = 360
    TEST_ASSERT(gridLines[2].x1 == 0.0 && gridLines[2].y1 == 360.0 && gridLines[2].x2 == 1920.0 && gridLines[2].y2 == 360.0);
    // Horizontal line 2: y = 720
    TEST_ASSERT(gridLines[3].x1 == 0.0 && gridLines[3].y1 == 720.0 && gridLines[3].x2 == 1920.0 && gridLines[3].y2 == 720.0);

    // 3. calculateSafeZone
    // TikTok: top 10%, bottom 22%, left 5%, right 15%
    // On 1080x1920 vertical canvas
    auto tiktokSafe = GuideOverlayEngine::calculateSafeZone(1080.0, 1920.0, OverlayGuideType::TikTok);
    TEST_ASSERT(std::abs(tiktokSafe.x - 54.0) < 0.01);
    TEST_ASSERT(std::abs(tiktokSafe.y - 192.0) < 0.01);
    TEST_ASSERT(std::abs(tiktokSafe.width - 864.0) < 0.01);
    TEST_ASSERT(std::abs(tiktokSafe.height - 1305.6) < 0.01);

    // Grid safe zone covers whole canvas
    auto gridSafe = GuideOverlayEngine::calculateSafeZone(1920.0, 1080.0, OverlayGuideType::Grid);
    TEST_ASSERT(gridSafe.x == 0.0 && gridSafe.y == 0.0);
    TEST_ASSERT(gridSafe.width == 1920.0 && gridSafe.height == 1080.0);

    // 4. calculateSafeZoneById
    auto reelsSafe = GuideOverlayEngine::calculateSafeZoneById(1080.0, 1920.0, "ig-reels");
    TEST_ASSERT(reelsSafe.has_value());
    TEST_ASSERT(std::abs(reelsSafe->x - 54.0) < 0.01);

    auto unknownSafe = GuideOverlayEngine::calculateSafeZoneById(1080.0, 1920.0, "unknown-id");
    TEST_ASSERT(!unknownSafe.has_value());

    std::cout << "[PASS] runGuideOverlayEngineTests" << std::endl;
}

void runElementParamRegistryTests() {
    using namespace catchim::editor;

    // 1. Blend modes
    const auto& blendModes = ElementParamRegistry::getBlendModeOptions();
    TEST_ASSERT(blendModes.size() == 17);
    bool hasMultiply = false;
    bool hasOverlay = false;
    bool hasScreen = false;
    for (const auto& opt : blendModes) {
        if (opt.value == "multiply") hasMultiply = true;
        if (opt.value == "overlay") hasOverlay = true;
        if (opt.value == "screen") hasScreen = true;
    }
    TEST_ASSERT(hasMultiply && hasOverlay && hasScreen);

    // 2. Built-in element params
    auto videoParams = ElementParamRegistry::getBuiltInElementParams(ClipType::Video);
    bool hasPosX = false;
    bool hasVolume = false;
    for (const auto& p : videoParams) {
        if (p.key == "transform.positionX") hasPosX = true;
        if (p.key == "volume") hasVolume = true;
    }
    TEST_ASSERT(hasPosX && hasVolume);

    auto audioParams = ElementParamRegistry::getBuiltInElementParams(ClipType::Audio);
    bool audioHasPos = false;
    bool audioHasVol = false;
    for (const auto& p : audioParams) {
        if (p.key == "transform.positionX") audioHasPos = true;
        if (p.key == "volume") audioHasVol = true;
    }
    TEST_ASSERT(!audioHasPos && audioHasVol);

    // 3. Default param values
    auto videoDefaults = ElementParamRegistry::buildDefaultParamValues(ClipType::Video);
    TEST_ASSERT(videoDefaults["transform.scaleX"] == 1.0);
    TEST_ASSERT(videoDefaults["opacity"] == 1.0);
    TEST_ASSERT(videoDefaults["blendMode"] == "normal");
    TEST_ASSERT(videoDefaults["volume"] == 0.0);

    auto textDefaults = ElementParamRegistry::buildDefaultParamValues(ClipType::Text);
    TEST_ASSERT(textDefaults["fontSize"] == 40.0);
    TEST_ASSERT(textDefaults["fontFamily"] == "Inter");
    TEST_ASSERT(textDefaults["background.enabled"] == false);

    std::cout << "[PASS] runElementParamRegistryTests" << std::endl;
}

void runTextRenderingPrimitivesTests() {
    using namespace catchim::render;

    // 1. quoteFontFamily
    TEST_ASSERT(TextRenderingPrimitives::quoteFontFamily("Inter") == "\"Inter\"");
    TEST_ASSERT(TextRenderingPrimitives::quoteFontFamily("My \"Custom\" Font") == "\"My \\\"Custom\\\" Font\"");

    // 2. buildTextFontString
    std::string fontStr = TextRenderingPrimitives::buildTextFontString("Inter", "bold", "italic", 48.0);
    TEST_ASSERT(fontStr == "italic bold 48px \"Inter\", sans-serif");

    std::string fontStr2 = TextRenderingPrimitives::buildTextFontString("Roboto", "normal", "normal", 32.0);
    TEST_ASSERT(fontStr2 == "normal normal 32px \"Roboto\", sans-serif");

    // 3. resolveTextLayoutMetrics
    // At 1080p canvas
    auto metrics1080 = TextRenderingPrimitives::resolveTextLayoutMetrics(40.0, 1080.0, 1.2);
    TEST_ASSERT(metrics1080.scaledFontSize == 40.0);
    TEST_ASSERT(std::abs(metrics1080.lineHeightPx - 48.0) < 0.01);
    TEST_ASSERT(std::abs(metrics1080.fontSizeRatio - (40.0 / 15.0)) < 0.01);

    // At 4K canvas (2160p) -> double size
    auto metrics4K = TextRenderingPrimitives::resolveTextLayoutMetrics(40.0, 2160.0, 1.2);
    TEST_ASSERT(metrics4K.scaledFontSize == 80.0);
    TEST_ASSERT(std::abs(metrics4K.lineHeightPx - 96.0) < 0.01);

    // 4. calculateCornerRadiusPx
    // 200x100 box: shortest edge = 100, half = 50. 50% corner radius -> 25.0px
    double r50 = TextRenderingPrimitives::calculateCornerRadiusPx(200.0, 100.0, 50.0);
    TEST_ASSERT(r50 == 25.0);

    double r100 = TextRenderingPrimitives::calculateCornerRadiusPx(200.0, 100.0, 100.0);
    TEST_ASSERT(r100 == 50.0);

    double r0 = TextRenderingPrimitives::calculateCornerRadiusPx(200.0, 100.0, 0.0);
    TEST_ASSERT(r0 == 0.0);

    double rClamp = TextRenderingPrimitives::calculateCornerRadiusPx(200.0, 100.0, 150.0);
    TEST_ASSERT(rClamp == 50.0);

    std::cout << "[PASS] runTextRenderingPrimitivesTests" << std::endl;
}

void runTimelineElementBuilderTests() {
    using namespace catchim::editor;
    using namespace catchim::core;
    using namespace catchim::media;

    // 1. buildElementFromMedia: Video
    auto vClip = TimelineElementBuilder::buildElementFromMedia(
        ClipId("vid-1"),
        MediaId("media-vid"),
        MediaType::Video,
        "My Video",
        TimelineTime::fromSeconds(10.0),
        TimelineTime::fromSeconds(2.0)
    );
    TEST_ASSERT(vClip.type() == ClipType::Video);
    TEST_ASSERT(vClip.name() == "My Video");
    TEST_ASSERT(vClip.duration() == TimelineTime::fromSeconds(10.0));
    TEST_ASSERT(vClip.startTime() == TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(vClip.trimStart() == TimelineTime(0));
    TEST_ASSERT(vClip.trimEnd() == TimelineTime(0));
    TEST_ASSERT(vClip.sourceDuration().has_value() && *vClip.sourceDuration() == TimelineTime::fromSeconds(10.0));
    TEST_ASSERT(vClip.mediaId() == MediaId("media-vid"));
    TEST_ASSERT(vClip.params().contains("transform.positionX"));
    TEST_ASSERT(vClip.params().contains("volume"));

    // 2. buildElementFromMedia: Audio
    auto aClip = TimelineElementBuilder::buildElementFromMedia(
        ClipId("aud-1"),
        MediaId("media-aud"),
        MediaType::Audio,
        "Voice Track",
        TimelineTime::fromSeconds(30.0)
    );
    TEST_ASSERT(aClip.type() == ClipType::Audio);
    TEST_ASSERT(aClip.params().contains("volume"));
    TEST_ASSERT(!aClip.params().contains("transform.positionX"));

    // 3. buildElementFromMedia: Image
    auto iClip = TimelineElementBuilder::buildElementFromMedia(
        ClipId("img-1"),
        MediaId("media-img"),
        MediaType::Image,
        "Logo",
        TimelineTime::fromSeconds(5.0)
    );
    TEST_ASSERT(iClip.type() == ClipType::Image);
    TEST_ASSERT(iClip.params().contains("opacity"));

    // 4. buildTextElement
    nlohmann::json overrides = {
        {"fontSize", 72.0},
        {"color", "#ff0000"}
    };
    auto tClip = TimelineElementBuilder::buildTextElement(
        ClipId("txt-1"),
        "Custom Title",
        "Hello Catchim",
        TimelineTime::fromSeconds(1.0),
        TimelineTime::fromSeconds(4.0),
        overrides
    );
    TEST_ASSERT(tClip.type() == ClipType::Text);
    TEST_ASSERT(tClip.name() == "Custom Title");
    TEST_ASSERT(tClip.getParam<std::string>("content", "") == "Hello Catchim");
    TEST_ASSERT(tClip.getParam<double>("fontSize", 0.0) == 72.0);
    TEST_ASSERT(tClip.getParam<std::string>("color", "") == "#ff0000");

    // 5. mergeParamValues
    nlohmann::json base = {{"a", 1}, {"b", 2}};
    nlohmann::json patch = {{"b", 20}, {"c", 30}};
    auto merged = TimelineElementBuilder::mergeParamValues(base, patch);
    TEST_ASSERT(merged["a"] == 1);
    TEST_ASSERT(merged["b"] == 20);
    TEST_ASSERT(merged["c"] == 30);

    std::cout << "[PASS] runTimelineElementBuilderTests" << std::endl;
}

void runAssSubtitleParserAndElementBuilderTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. Timestamp parsing & formatting
    double t1 = AssSubtitleParser::parseTimestamp("1:02:03.45");
    TEST_ASSERT(std::abs(t1 - 3723.45) < 0.001);
    double t2 = AssSubtitleParser::parseTimestamp("0:00:05.10");
    TEST_ASSERT(std::abs(t2 - 5.10) < 0.001);
    TEST_ASSERT(AssSubtitleParser::parseTimestamp("invalid") < 0.0);

    std::string fmt = AssSubtitleParser::formatTimestamp(3723.45);
    TEST_ASSERT(fmt == "1:02:03.45");

    // 2. ASS color parsing
    auto [c1, a1] = AssSubtitleParser::parseColor("&H000000FF"); // Red
    TEST_ASSERT(c1 == "#ff0000");
    TEST_ASSERT(std::abs(a1 - 1.0) < 0.01);

    auto [c2, a2] = AssSubtitleParser::parseColor("&H8000FF00"); // Green, 50% opacity
    TEST_ASSERT(c2 == "#00ff00");
    TEST_ASSERT(std::abs(a2 - 0.498) < 0.02);

    // 3. Text stripping
    bool hadTags = false;
    std::string stripped = AssSubtitleParser::stripAssText("{\\b1\\pos(100,200)}Hello \\NWorld!\\hEnd", hadTags);
    TEST_ASSERT(hadTags == true);
    TEST_ASSERT(stripped == "Hello \nWorld! End");

    // 4. Full ASS script parsing
    std::string assScript = R"(
[Script Info]
Title: Sample ASS
PlayResX: 1280
PlayResY: 720

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,Arial,28,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1
Style: TopTitle,Roboto,36,&H0000FFFF,&H000000FF,&H00000000,&H00000000,1,0,0,0,100,100,0,0,1,2,2,8,20,20,30,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
Dialogue: 0,0:00:01.00,0:00:04.50,Default,,0,0,0,,{\i1}First subtitle cue{\i0}
Dialogue: 0,0:00:05.00,0:00:08.00,TopTitle,,0,0,0,,Header Title
Dialogue: 0,0:00:09.00,0:00:12.00,UnknownStyle,,0,0,0,,Fallback text
)";

    auto parseResult = AssSubtitleParser::parse(assScript);
    TEST_ASSERT(parseResult.cues.size() == 3);
    TEST_ASSERT(parseResult.strippedInlineTagCueCount == 1);
    TEST_ASSERT(parseResult.missingStyleCueCount == 1);

    // Cue 0: Default style
    const auto& cue0 = parseResult.cues[0];
    TEST_ASSERT(cue0.text == "First subtitle cue");
    TEST_ASSERT(cue0.startTime == TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(cue0.duration == TimelineTime::fromSeconds(3.5));
    TEST_ASSERT(cue0.style.fontFamily == "Arial");
    TEST_ASSERT(cue0.style.textAlign == SubtitleTextAlign::Center);
    TEST_ASSERT(cue0.style.placement.verticalAlign == SubtitleVerticalAlign::Bottom);

    // Cue 1: TopTitle style
    const auto& cue1 = parseResult.cues[1];
    TEST_ASSERT(cue1.text == "Header Title");
    TEST_ASSERT(cue1.style.bold == true);
    TEST_ASSERT(cue1.style.fontFamily == "Roboto");
    TEST_ASSERT(cue1.style.textAlign == SubtitleTextAlign::Center);
    TEST_ASSERT(cue1.style.placement.verticalAlign == SubtitleVerticalAlign::Top);

    // 5. SubtitleElementBuilder tests
    auto clip0 = SubtitleElementBuilder::buildSubtitleTextElement(0, cue0, 1920.0, 1080.0);
    TEST_ASSERT(clip0.type() == ClipType::Text);
    TEST_ASSERT(clip0.name() == "Caption 1");
    TEST_ASSERT(clip0.startTime() == TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(clip0.duration() == TimelineTime::fromSeconds(3.5));
    TEST_ASSERT(clip0.getParam<std::string>("content", "") == "First subtitle cue");
    TEST_ASSERT(clip0.getParam<std::string>("textAlign", "") == "center");

    auto clip1 = SubtitleElementBuilder::buildSubtitleTextElement(1, cue1, 1920.0, 1080.0);
    TEST_ASSERT(clip1.getParam<std::string>("fontWeight", "") == "bold");
    TEST_ASSERT(clip1.getParam<double>("transform.positionY", 0.0) < 0.0); // Top aligned -> negative Y in centered canvas

    // Target width and wrapping
    SubtitlePlacement pl;
    pl.marginLeftRatio = 0.1;
    pl.marginRightRatio = 0.1;
    double w = SubtitleElementBuilder::resolveTargetWidth(1000.0, pl);
    TEST_ASSERT(w == 800.0);

    std::string wrapped = SubtitleElementBuilder::wrapSubtitleText("This is a very long subtitle that should be wrapped across lines", 100.0, 10.0);
    TEST_ASSERT(wrapped.find('\n') != std::string::npos);

    std::cout << "[PASS] runAssSubtitleParserAndElementBuilderTests" << std::endl;
}

void runCssGradientEngineTests() {
    using namespace catchim::render;

    // 1. splitCssLayers with nested parentheses
    std::string multiBg = "radial-gradient(circle at 30% 70%, rgba(173, 216, 230, 0.35), transparent 60%), linear-gradient(to right, red, blue), #ffffff";
    auto layers = CssGradientEngine::splitCssLayers(multiBg);
    TEST_ASSERT(layers.size() == 3);
    TEST_ASSERT(layers[0].rfind("radial-gradient", 0) == 0);
    TEST_ASSERT(layers[1].rfind("linear-gradient", 0) == 0);
    TEST_ASSERT(layers[2] == "#ffffff");

    // 2. parseBackgroundLayers
    auto parsedLayers = CssGradientEngine::parseBackgroundLayers(multiBg);
    TEST_ASSERT(parsedLayers.size() == 3);
    TEST_ASSERT(parsedLayers[0].type == BackgroundLayerType::Gradient);
    TEST_ASSERT(parsedLayers[0].gradient.type == CssGradientType::Radial);
    TEST_ASSERT(parsedLayers[1].type == BackgroundLayerType::Gradient);
    TEST_ASSERT(parsedLayers[1].gradient.type == CssGradientType::Linear);
    TEST_ASSERT(parsedLayers[2].type == BackgroundLayerType::Color);
    TEST_ASSERT(parsedLayers[2].colorValue == "#ffffff");

    // 3. parseGradient Linear
    auto linGradOpt = CssGradientEngine::parseGradient("linear-gradient(to right, #ff0000 0%, #0000ff 100%)");
    TEST_ASSERT(linGradOpt.has_value());
    TEST_ASSERT(linGradOpt->type == CssGradientType::Linear);
    TEST_ASSERT(std::abs(linGradOpt->angleDegrees - 90.0) < 0.1);
    TEST_ASSERT(linGradOpt->colorStops.size() == 2);
    TEST_ASSERT(linGradOpt->colorStops[0].color.r == 255);
    TEST_ASSERT(linGradOpt->colorStops[0].color.b == 0);
    TEST_ASSERT(linGradOpt->colorStops[1].color.b == 255);

    // 4. parseGradient Radial
    auto radGradOpt = CssGradientEngine::parseGradient("radial-gradient(circle closest-side at 20% 80%, yellow, green)");
    TEST_ASSERT(radGradOpt.has_value());
    TEST_ASSERT(radGradOpt->type == CssGradientType::Radial);
    TEST_ASSERT(radGradOpt->radialShape == RadialShape::Circle);
    TEST_ASSERT(radGradOpt->radialExtent == RadialExtent::ClosestSide);
    TEST_ASSERT(std::abs(radGradOpt->centerXRatio - 0.20) < 0.01);
    TEST_ASSERT(std::abs(radGradOpt->centerYRatio - 0.80) < 0.01);

    // 5. Geometry calculation: resolveLinearPoints
    auto linPts = CssGradientEngine::resolveLinearPoints(1920.0, 1080.0, 180.0); // to bottom
    TEST_ASSERT(std::abs(linPts.x0 - 960.0) < 0.1);
    TEST_ASSERT(std::abs(linPts.y0 - 0.0) < 0.1);
    TEST_ASSERT(std::abs(linPts.x1 - 960.0) < 0.1);
    TEST_ASSERT(std::abs(linPts.y1 - 1080.0) < 0.1);
    TEST_ASSERT(std::abs(linPts.length - 1080.0) < 0.1);

    // 6. Geometry calculation: resolveRadialDimensions
    auto radDims = CssGradientEngine::resolveRadialDimensions(1000.0, 1000.0, RadialShape::Circle, RadialExtent::ClosestSide, 0.5, 0.5);
    TEST_ASSERT(radDims.cx == 500.0);
    TEST_ASSERT(radDims.cy == 500.0);
    TEST_ASSERT(radDims.rx == 500.0);
    TEST_ASSERT(radDims.ry == 500.0);

    // 7. normalizeColorStops & fixTransparentStops
    std::vector<CssColorStop> rawStops = {
        { {255, 0, 0, 1.0}, "red", std::nullopt },
        { {0, 0, 0, 0.0}, "transparent", std::nullopt },
        { {0, 0, 255, 1.0}, "blue", std::nullopt }
    };
    auto normalized = CssGradientEngine::normalizeColorStops(rawStops);
    TEST_ASSERT(normalized[0].offset.value() == 0.0);
    TEST_ASSERT(normalized[1].offset.value() == 0.5);
    TEST_ASSERT(normalized[2].offset.value() == 1.0);

    auto fixed = CssGradientEngine::fixTransparentStops(normalized);
    TEST_ASSERT(fixed[1].color.r == 255); // Donor from previous red stop
    TEST_ASSERT(fixed[1].color.a == 0.0);

    // 8. Color parsing
    auto hexCol = CssGradientEngine::parseColor("#3498db");
    TEST_ASSERT(hexCol.r == 0x34 && hexCol.g == 0x98 && hexCol.b == 0xdb);
    auto rgbaCol = CssGradientEngine::parseColor("rgba(10, 20, 30, 0.5)");
    TEST_ASSERT(rgbaCol.r == 10 && rgbaCol.g == 20 && rgbaCol.b == 30 && std::abs(rgbaCol.a - 0.5) < 0.01);
    auto hslCol = CssGradientEngine::parseColor("hsl(0, 100%, 50%)");
    TEST_ASSERT(hslCol.r == 255 && hslCol.g == 0 && hslCol.b == 0);

    // 9. Presets
    const auto& pcGradients = CssGradientEngine::getPatternCraftGradients();
    TEST_ASSERT(pcGradients.size() >= 10);
    const auto& palette = CssGradientEngine::getSolidColorPalette();
    TEST_ASSERT(palette.size() >= 20);

    std::cout << "[PASS] runCssGradientEngineTests" << std::endl;
}

void runCanvasTransformPipelineTests() {
    using namespace catchim::render;

    // 1. buildTransformFromParams
    nlohmann::json params = {
        {"transform.scaleX", 1.5},
        {"transform.scaleY", 2.0},
        {"transform.positionX", 100.0},
        {"transform.positionY", -50.0},
        {"transform.rotate", 45.0},
        {"opacity", 0.75},
        {"blendMode", "multiply"}
    };

    auto t = CanvasTransformPipeline::buildTransformFromParams(params);
    TEST_ASSERT(t.scaleX == 1.5);
    TEST_ASSERT(t.scaleY == 2.0);
    TEST_ASSERT(t.positionX == 100.0);
    TEST_ASSERT(t.positionY == -50.0);
    TEST_ASSERT(t.rotate == 45.0);

    TEST_ASSERT(CanvasTransformPipeline::readOpacityFromParams(params) == 0.75);
    TEST_ASSERT(CanvasTransformPipeline::readBlendModeFromParams(params) == "multiply");

    // 2. resolveTransformAtTime with keyframes
    nlohmann::json anims = {
        {"transform.positionX", {
            {"keys", {
                {{"time", 0.0}, {"value", 0.0}},
                {{"time", 1.0}, {"value", 200.0}}
            }}
        }}
    };

    Transform2D baseT;
    baseT.positionX = 0.0;
    baseT.positionY = 50.0;

    auto tAt0 = CanvasTransformPipeline::resolveTransformAtTime(baseT, anims, 0.0);
    TEST_ASSERT(tAt0.positionX == 0.0);
    TEST_ASSERT(tAt0.positionY == 50.0);

    auto tAtHalf = CanvasTransformPipeline::resolveTransformAtTime(baseT, anims, 0.5);
    TEST_ASSERT(std::abs(tAtHalf.positionX - 100.0) < 0.001);

    auto tAt1 = CanvasTransformPipeline::resolveTransformAtTime(baseT, anims, 1.0);
    TEST_ASSERT(tAt1.positionX == 200.0);

    // 3. Matrix3x3 operations
    // Identity
    auto I = Matrix3x3::identity();
    Point2D pt{15.0, -25.0};
    TEST_ASSERT(I.transformPoint(pt) == pt);

    // Translation
    auto T = Matrix3x3::translation(50.0, 100.0);
    auto ptT = T.transformPoint(pt);
    TEST_ASSERT(ptT.x == 65.0 && ptT.y == 75.0);

    // Scale
    auto S = Matrix3x3::scale(2.0, 3.0);
    auto ptS = S.transformPoint(pt);
    TEST_ASSERT(ptS.x == 30.0 && ptS.y == -75.0);

    // Rotation: 90 degrees around origin
    auto R = Matrix3x3::rotation(M_PI / 2.0);
    Point2D ptR_in{10.0, 0.0};
    auto ptR_out = R.transformPoint(ptR_in);
    TEST_ASSERT(std::abs(ptR_out.x - 0.0) < 0.001 && std::abs(ptR_out.y - 10.0) < 0.001);

    // Compose and Invert
    auto M = Matrix3x3::compose(100.0, 200.0, 30.0, 1.5, 0.8);
    Point2D testPt{40.0, -60.0};
    Point2D transformed = M.transformPoint(testPt);
    auto invOpt = M.inverse();
    TEST_ASSERT(invOpt.has_value());
    Point2D back = invOpt->transformPoint(transformed);
    TEST_ASSERT(std::abs(back.x - testPt.x) < 0.001);
    TEST_ASSERT(std::abs(back.y - testPt.y) < 0.001);

    auto invPtOpt = M.inverseTransformPoint(transformed);
    TEST_ASSERT(invPtOpt.has_value());
    TEST_ASSERT(std::abs(invPtOpt->x - testPt.x) < 0.001);
    TEST_ASSERT(std::abs(invPtOpt->y - testPt.y) < 0.001);

    std::cout << "[PASS] runCanvasTransformPipelineTests" << std::endl;
}

void runRenderPerformanceProfilerAndDiagnosticsRegistryTests() {
    using namespace catchim::render;
    using namespace catchim::editor;

    // 1. RenderPerformanceProfiler
    auto& profiler = RenderPerformanceProfiler::instance();
    profiler.reset();
    profiler.setEnabled(true);
    profiler.setFlushInterval(5); // fast cadence for testing

    profiler.recordSpan("composite", 12.5);
    profiler.recordSpan("composite", 15.0);
    profiler.recordSpan("composite", 10.0);
    profiler.recordSpan("decode", 5.0);

    profiler.incrementCounter("drawCalls", 10);
    profiler.incrementCounter("textureUploads", 2);
    profiler.onFrameComplete();

    // Second frame
    profiler.recordSpan("composite", 14.0);
    profiler.incrementCounter("drawCalls", 12);
    profiler.onFrameComplete();

    auto spanSummaries = profiler.getSpanSummaries();
    TEST_ASSERT(!spanSummaries.empty());
    TEST_ASSERT(spanSummaries[0].name == "composite");
    TEST_ASSERT(spanSummaries[0].count == 4);
    TEST_ASSERT(spanSummaries[0].maxMs == 15.0);
    TEST_ASSERT(spanSummaries[0].p50Ms >= 10.0 && spanSummaries[0].p50Ms <= 15.0);

    auto counterSummaries = profiler.getCounterSummaries();
    TEST_ASSERT(!counterSummaries.empty());
    TEST_ASSERT(counterSummaries[0].name == "drawCalls");
    TEST_ASSERT(counterSummaries[0].total == 22);
    TEST_ASSERT(counterSummaries[0].frames == 2);
    TEST_ASSERT(counterSummaries[0].perFrame == 11.0);

    // Measure span using lambda
    int computed = profiler.measureSpan("customComputation", []() {
        return 42;
    });
    TEST_ASSERT(computed == 42);

    // Trigger flush
    profiler.onFrameComplete();
    profiler.onFrameComplete();
    profiler.onFrameComplete(); // reaches 5 frames -> flushes
    TEST_ASSERT(profiler.framesSinceFlush() == 0);

    profiler.setEnabled(false);

    // 2. DiagnosticsRegistry
    auto& diagnostics = DiagnosticsRegistry::instance();
    diagnostics.clear();

    bool hasMissingMedia = false;
    bool hasZeroDurationClip = false;

    diagnostics.registerRule(
        "missing-media",
        "media",
        DiagnosticSeverity::Error,
        "Project references missing media assets",
        [&]() { return hasMissingMedia; }
    );

    diagnostics.registerRule(
        "zero-duration-clip",
        "timeline",
        DiagnosticSeverity::Caution,
        "Timeline contains zero-duration clip",
        [&]() { return hasZeroDurationClip; }
    );

    TEST_ASSERT(diagnostics.count() == 2);
    TEST_ASSERT(diagnostics.getActiveDiagnostics().empty());

    // Trigger error
    hasMissingMedia = true;
    auto activeAll = diagnostics.getActiveDiagnostics();
    TEST_ASSERT(activeAll.size() == 1);
    TEST_ASSERT(activeAll[0].id == "missing-media");
    TEST_ASSERT(activeAll[0].severity == DiagnosticSeverity::Error);

    // Filter by scope
    auto activeTimeline = diagnostics.getActiveDiagnostics("timeline");
    TEST_ASSERT(activeTimeline.empty());
    auto activeMedia = diagnostics.getActiveDiagnostics("media");
    TEST_ASSERT(activeMedia.size() == 1);

    // Subscribe / Notify
    int notificationCount = 0;
    auto subId = diagnostics.subscribe([&]() {
        notificationCount++;
    });
    diagnostics.notify();
    TEST_ASSERT(notificationCount == 1);
    diagnostics.unsubscribe(subId);
    diagnostics.notify();
    TEST_ASSERT(notificationCount == 1);

    std::cout << "[PASS] runRenderPerformanceProfilerAndDiagnosticsRegistryTests" << std::endl;
}

void runCanvasPresetEngineTests() {
    using namespace catchim::render;

    // 1. Standard presets
    const auto& presets = CanvasPresetEngine::getStandardPresets();
    TEST_ASSERT(presets.size() >= 6);
    TEST_ASSERT(presets[0].width == 1920 && presets[0].height == 1080);
    TEST_ASSERT(presets[0].aspectRatio == "16:9");

    const auto& defPreset = CanvasPresetEngine::getDefaultPreset();
    TEST_ASSERT(defPreset.width == 1920 && defPreset.height == 1080);

    // 2. Aspect ratio fraction & string
    auto [num16_9, den16_9] = CanvasPresetEngine::calculateAspectRatioFraction(1920, 1080);
    TEST_ASSERT(num16_9 == 16 && den16_9 == 9);
    TEST_ASSERT(CanvasPresetEngine::getAspectRatioString(1920, 1080) == "16:9");

    auto [num9_16, den9_16] = CanvasPresetEngine::calculateAspectRatioFraction(1080, 1920);
    TEST_ASSERT(num9_16 == 9 && den9_16 == 16);
    TEST_ASSERT(CanvasPresetEngine::getAspectRatioString(1080, 1920) == "9:16");

    auto [num1_1, den1_1] = CanvasPresetEngine::calculateAspectRatioFraction(1080, 1080);
    TEST_ASSERT(num1_1 == 1 && den1_1 == 1);
    TEST_ASSERT(CanvasPresetEngine::getAspectRatioString(1080, 1080) == "1:1");

    auto [num4_3, den4_3] = CanvasPresetEngine::calculateAspectRatioFraction(1440, 1080);
    TEST_ASSERT(num4_3 == 4 && den4_3 == 3);
    TEST_ASSERT(CanvasPresetEngine::getAspectRatioString(1440, 1080) == "4:3");

    // 3. roundToEven
    TEST_ASSERT(CanvasPresetEngine::roundToEven(1919) == 1920);
    TEST_ASSERT(CanvasPresetEngine::roundToEven(1920) == 1920);
    TEST_ASSERT(CanvasPresetEngine::roundToEven(1) == 2);

    // 4. fitCanvasToMedia
    // 4K media -> fits within 1920 maxDimension
    auto fitted4K = CanvasPresetEngine::fitCanvasToMedia(3840, 2160, 1920);
    TEST_ASSERT(fitted4K.width == 1920 && fitted4K.height == 1080);

    // Vertical video 720x1280 within 1920 maxDimension
    auto fittedVert = CanvasPresetEngine::fitCanvasToMedia(720, 1280, 1920);
    TEST_ASSERT(fittedVert.width == 720 && fittedVert.height == 1280);

    // Vertical video 1440x2560 exceeds 1920
    auto fittedLargeVert = CanvasPresetEngine::fitCanvasToMedia(1440, 2560, 1920);
    TEST_ASSERT(fittedLargeVert.height == 1920);
    TEST_ASSERT(CanvasPresetEngine::roundToEven(fittedLargeVert.width) == fittedLargeVert.width);

    std::cout << "[PASS] runCanvasPresetEngineTests" << std::endl;
}

void runEuclideanFrameSnapperTests() {
    using namespace catchim::core;

    // 1. divEuclid and remEuclid
    // Positive
    TEST_ASSERT(EuclideanFrameSnapper::divEuclid(7, 3) == 2);
    TEST_ASSERT(EuclideanFrameSnapper::remEuclid(7, 3) == 1);

    // Negative dividend: -7 / 3 in Euclidean is -3 with remainder 2 (-3 * 3 + 2 = -7)
    TEST_ASSERT(EuclideanFrameSnapper::divEuclid(-7, 3) == -3);
    TEST_ASSERT(EuclideanFrameSnapper::remEuclid(-7, 3) == 2);

    // 2. getTicksPerFrame
    FrameRate fps30{30, 1};
    auto tpf30 = EuclideanFrameSnapper::getTicksPerFrame(fps30);
    TEST_ASSERT(tpf30.has_value() && *tpf30 == 4000); // 120,000 / 30 = 4,000

    FrameRate fps60{60, 1};
    auto tpf60 = EuclideanFrameSnapper::getTicksPerFrame(fps60);
    TEST_ASSERT(tpf60.has_value() && *tpf60 == 2000); // 120,000 / 60 = 2,000

    FrameRate fps24{24, 1};
    auto tpf24 = EuclideanFrameSnapper::getTicksPerFrame(fps24);
    TEST_ASSERT(tpf24.has_value() && *tpf24 == 5000); // 120,000 / 24 = 5,000

    // 3. roundToFrame
    // At 30fps (4000 ticks/frame):
    // 0 -> 0
    // 1999 -> 0 (less than half)
    // 2000 -> 4000 (halfway rounds up)
    // 3999 -> 4000
    TEST_ASSERT(EuclideanFrameSnapper::roundToFrame(TimelineTime(0), fps30)->ticks() == 0);
    TEST_ASSERT(EuclideanFrameSnapper::roundToFrame(TimelineTime(1999), fps30)->ticks() == 0);
    TEST_ASSERT(EuclideanFrameSnapper::roundToFrame(TimelineTime(2000), fps30)->ticks() == 4000);
    TEST_ASSERT(EuclideanFrameSnapper::roundToFrame(TimelineTime(3999), fps30)->ticks() == 4000);

    // 4. floorToFrame
    TEST_ASSERT(EuclideanFrameSnapper::floorToFrame(TimelineTime(0), fps30)->ticks() == 0);
    TEST_ASSERT(EuclideanFrameSnapper::floorToFrame(TimelineTime(3999), fps30)->ticks() == 0);
    TEST_ASSERT(EuclideanFrameSnapper::floorToFrame(TimelineTime(4000), fps30)->ticks() == 4000);

    // 5. lastFrameTime
    // duration 120,000 (1 second at 30 fps has frames 0..29, last frame starts at tick 116,000)
    TimelineTime dur = TimelineTime::fromSeconds(1.0);
    auto lastFrame = EuclideanFrameSnapper::lastFrameTime(dur, fps30);
    TEST_ASSERT(lastFrame.ticks() == 116000);

    // 6. snappedSeekTime
    auto seekClamped = EuclideanFrameSnapper::snappedSeekTime(TimelineTime(125000), dur, fps30);
    TEST_ASSERT(seekClamped.ticks() == dur.ticks()); // Clamped to duration

    auto seekNormal = EuclideanFrameSnapper::snappedSeekTime(TimelineTime(2100), dur, fps30);
    TEST_ASSERT(seekNormal.ticks() == 4000);

    std::cout << "[PASS] runEuclideanFrameSnapperTests" << std::endl;
}

void runBuiltinMaskGeometryTests() {
    using namespace catchim::render;

    // 1. Default square mask params
    auto p1 = BuiltinMaskGeometry::getDefaultSquareMaskParams(1920.0, 1080.0);
    TEST_ASSERT(p1.centerX == 0.0);
    TEST_ASSERT(p1.centerY == 0.0);
    TEST_ASSERT(std::abs(p1.width - (1080.0 * 0.6 / 1920.0)) < 0.0001);
    TEST_ASSERT(std::abs(p1.height - 0.6) < 0.0001);
    TEST_ASSERT(p1.rotation == 0.0);
    TEST_ASSERT(p1.scale == 1.0);
    TEST_ASSERT(p1.strokeAlign == "center");

    // 2. Stroke offset
    TEST_ASSERT(BuiltinMaskGeometry::getStrokeOffset("inside", 4.0) == -2.0);
    TEST_ASSERT(BuiltinMaskGeometry::getStrokeOffset("outside", 4.0) == 2.0);
    TEST_ASSERT(BuiltinMaskGeometry::getStrokeOffset("center", 4.0) == 0.0);

    // 3. Rotate point
    Point2D rotated = BuiltinMaskGeometry::rotatePoint(10.0, 0.0, 0.0, 0.0, M_PI / 2.0);
    TEST_ASSERT(std::abs(rotated.x - 0.0) < 0.0001);
    TEST_ASSERT(std::abs(rotated.y - 10.0) < 0.0001);

    // 4. Compute feather update
    double feather = BuiltinMaskGeometry::computeFeatherUpdate(0.0, 11.0, 0.0, 1.0, 0.0);
    TEST_ASSERT(feather == 100.0);

    // 5. Compute box mask param update
    // Position drag
    auto pPos = BuiltinMaskGeometry::computeBoxMaskParamUpdate("position", "", p1, 192.0, 108.0, 1920.0, 1080.0);
    TEST_ASSERT(std::abs(pPos.centerX - 0.1) < 0.0001);
    TEST_ASSERT(std::abs(pPos.centerY - 0.1) < 0.0001);

    // Rotation drag
    auto pRot = BuiltinMaskGeometry::computeBoxMaskParamUpdate("rotation", "", p1, 0.0, 10.0, 1920.0, 1080.0);
    TEST_ASSERT(std::abs(pRot.rotation - 90.0) < 0.0001);

    // Edge drag (right)
    auto pEdge = BuiltinMaskGeometry::computeBoxMaskParamUpdate("edge", "right", p1, 96.0, 0.0, 1920.0, 1080.0);
    TEST_ASSERT(std::abs(pEdge.width - (p1.width + 0.1)) < 0.0001);

    // 6. Shape-specific geometries
    auto rectCorners = BuiltinMaskGeometry::buildRectangleCorners(p1, 1920.0, 1080.0);
    TEST_ASSERT(rectCorners.size() == 4);

    auto ellipseGeom = BuiltinMaskGeometry::buildEllipseParams(p1, 1920.0, 1080.0);
    TEST_ASSERT(ellipseGeom.maskWidth > 0.0 && ellipseGeom.maskHeight > 0.0);

    auto cbParams = BuiltinMaskGeometry::getDefaultCinematicBarsMaskParams(1920.0, 1080.0);
    TEST_ASSERT(cbParams.width >= 1.0);
    auto cbCorners = BuiltinMaskGeometry::buildCinematicBarsCorners(cbParams, 1920.0, 1080.0);
    TEST_ASSERT(cbCorners.size() == 4);

    auto diamondPts = BuiltinMaskGeometry::buildDiamondPoints(p1, 1920.0, 1080.0);
    TEST_ASSERT(diamondPts.size() == 4);

    auto heartCurves = BuiltinMaskGeometry::buildHeartCurves(p1, 1920.0, 1080.0);
    TEST_ASSERT(heartCurves.rightBranch.start.x == heartCurves.leftBranch.end.x);

    auto starVertices = BuiltinMaskGeometry::buildStarVertices(p1, 1920.0, 1080.0);
    TEST_ASSERT(starVertices.size() == 10);

    // 7. Split mask line & polygon clipping
    SplitMaskParams sp;
    sp.centerX = 0.0;
    sp.centerY = 0.0;
    sp.rotation = 0.0;
    auto strokeSeg = BuiltinMaskGeometry::getSplitMaskStrokeSegment(sp, 1920.0, 1080.0);
    TEST_ASSERT(strokeSeg.has_value());
    TEST_ASSERT(std::abs(strokeSeg->first.x - 960.0) < 0.0001);
    TEST_ASSERT(std::abs(strokeSeg->second.x - 960.0) < 0.0001);

    auto splitPoly = BuiltinMaskGeometry::buildSplitPolygonVertices(sp, 1920.0, 1080.0);
    TEST_ASSERT(splitPoly.size() >= 3);
    double polyArea = BuiltinMaskGeometry::computePolygonArea(splitPoly);
    TEST_ASSERT(polyArea > 0.0);
    // For vertical cut at x=960 of 1920x1080 canvas, half area is 960 * 1080 = 1,036,800
    TEST_ASSERT(std::abs(polyArea - (960.0 * 1080.0)) < 1.0);

    std::cout << "[PASS] runBuiltinMaskGeometryTests" << std::endl;
}

void runMaskSnapEngineTests() {
    using namespace catchim::render;
    using namespace catchim::editor;

    ElementBounds bounds{ .cx = 960.0, .cy = 540.0, .width = 800.0, .height = 600.0, .rotation = 0.0 };
    Size2D canvasSize{ .width = 1920.0, .height = 1080.0 };

    RectangleMaskParams startParams;
    startParams.centerX = 0.0;
    startParams.centerY = 0.0;
    startParams.width = 0.5;
    startParams.height = 0.5;
    startParams.rotation = 0.0;
    startParams.scale = 1.0;

    // 1. Local center and snap geometry
    Point2D localCenter = MaskSnapEngine::getMaskLocalCenter(0.1, -0.2, bounds);
    TEST_ASSERT(std::abs(localCenter.x - 80.0) < 0.0001);
    TEST_ASSERT(std::abs(localCenter.y - (-120.0)) < 0.0001);

    auto restored = MaskSnapEngine::setMaskLocalCenter(localCenter, bounds);
    TEST_ASSERT(std::abs(restored.first - 0.1) < 0.0001);
    TEST_ASSERT(std::abs(restored.second - (-0.2)) < 0.0001);

    auto snapGeom = MaskSnapEngine::getMaskSnapGeometry(startParams, bounds);
    TEST_ASSERT(snapGeom.size.width == 400.0);
    TEST_ASSERT(snapGeom.size.height == 300.0);

    // 2. Snap position
    RectangleMaskParams proposedPos = startParams;
    proposedPos.centerX = 0.002; // Close to center (0) -> should snap
    auto snapPosRes = MaskSnapEngine::snapBoxMaskInteraction(
        "position", "", startParams, proposedPos, bounds, canvasSize, {10.0, 10.0}
    );
    TEST_ASSERT(std::abs(snapPosRes.params.centerX - 0.0) < 0.0001);

    // 3. Snap rotation
    RectangleMaskParams proposedRot = startParams;
    proposedRot.rotation = 89.0; // Near 90 deg -> snaps to 90
    auto snapRotRes = MaskSnapEngine::snapBoxMaskInteraction(
        "rotation", "", startParams, proposedRot, bounds, canvasSize
    );
    TEST_ASSERT(snapRotRes.params.rotation == 90.0);

    // 4. Snap split mask interaction
    SplitMaskParams splitProposed;
    splitProposed.centerX = 0.001;
    splitProposed.centerY = 0.0;
    splitProposed.rotation = 1.0; // Near 0 deg -> snaps to 0
    auto splitSnapRes = MaskSnapEngine::snapSplitMaskInteraction(
        "rotation", splitProposed, bounds, canvasSize
    );
    TEST_ASSERT(splitSnapRes.params.rotation == 0.0);

    std::cout << "[PASS] runMaskSnapEngineTests" << std::endl;
}

void runMaskInteractionAndRegistryTests() {
    using namespace catchim::render;
    using namespace catchim::editor;

    ElementBounds bounds{ .cx = 960.0, .cy = 540.0, .width = 800.0, .height = 600.0, .rotation = 0.0 };

    // 1. Line mask points & handle positions
    auto linePts = MaskInteractionEngine::getLineMaskLinePoints(0.0, 0.0, 0.0, bounds);
    TEST_ASSERT(linePts.first.x == linePts.second.x); // Vertical line has identical x

    auto lineHandles = MaskInteractionEngine::getLineMaskHandlePositions(0.0, 0.0, 0.0, 50.0, bounds, 1.0);
    TEST_ASSERT(lineHandles.size() == 2);
    TEST_ASSERT(lineHandles[0].idKind == "rotation");
    TEST_ASSERT(lineHandles[1].idKind == "feather");

    // 2. Box mask handles for different size modes
    // "width-height": 4 corners + 3 edges + rotation + feather = 9 handles
    auto boxHandlesWH = MaskInteractionEngine::getBoxMaskHandlePositions(
        0.0, 0.0, 0.5, 0.5, 0.0, 0.0, "width-height", bounds, 1.0
    );
    TEST_ASSERT(boxHandlesWH.size() == 9);

    // "height-only": 2 edges + rotation + feather = 4 handles
    auto boxHandlesH = MaskInteractionEngine::getBoxMaskHandlePositions(
        0.0, 0.0, 0.5, 0.5, 0.0, 0.0, "height-only", bounds, 1.0
    );
    TEST_ASSERT(boxHandlesH.size() == 4);

    // "width-only": 2 edges + rotation + feather = 4 handles
    auto boxHandlesW = MaskInteractionEngine::getBoxMaskHandlePositions(
        0.0, 0.0, 0.5, 0.5, 0.0, 0.0, "width-only", bounds, 1.0
    );
    TEST_ASSERT(boxHandlesW.size() == 4);

    // "uniform": 1 scale corner + rotation + feather = 3 handles
    auto boxHandlesU = MaskInteractionEngine::getBoxMaskHandlePositions(
        0.0, 0.0, 0.5, 0.5, 0.0, 0.0, "uniform", bounds, 1.0
    );
    TEST_ASSERT(boxHandlesU.size() == 3);

    // 3. Overlays
    auto rectOverlay = MaskInteractionEngine::getBoxMaskRectOverlay(0.0, 0.0, 0.5, 0.5, 0.0, bounds);
    TEST_ASSERT(rectOverlay.width == 400.0);
    TEST_ASSERT(rectOverlay.height == 300.0);

    // 4. MaskRegistry
    const auto& types = MaskRegistry::getAllMaskTypes();
    TEST_ASSERT(types.size() == 9);
    TEST_ASSERT(MaskRegistry::isRegistered("split"));
    TEST_ASSERT(MaskRegistry::isRegistered("rectangle"));
    TEST_ASSERT(MaskRegistry::isRegistered("cinematic-bars"));
    TEST_ASSERT(MaskRegistry::isRegistered("ellipse"));
    TEST_ASSERT(MaskRegistry::isRegistered("heart"));
    TEST_ASSERT(MaskRegistry::isRegistered("diamond"));
    TEST_ASSERT(MaskRegistry::isRegistered("star"));
    TEST_ASSERT(MaskRegistry::isRegistered("text"));
    TEST_ASSERT(MaskRegistry::isRegistered("freeform"));
    TEST_ASSERT(!MaskRegistry::isRegistered("non_existent_mask"));

    auto featSplit = MaskRegistry::getFeatures("split");
    TEST_ASSERT(featSplit.sizeMode == "none");

    auto featCb = MaskRegistry::getFeatures("cinematic-bars");
    TEST_ASSERT(featCb.sizeMode == "height-only");

    auto featRect = MaskRegistry::getFeatures("rectangle");
    TEST_ASSERT(featRect.sizeMode == "width-height");

    auto featText = MaskRegistry::getFeatures("text");
    TEST_ASSERT(featText.sizeMode == "uniform");

    // Build default
    auto defText = MaskRegistry::buildDefault("text", 1920.0, 1080.0);
    TEST_ASSERT(defText["content"] == "Mask");
    TEST_ASSERT(MaskRegistry::isActive("text", defText));

    nlohmann::json emptyText = { {"content", "   "} };
    TEST_ASSERT(!MaskRegistry::isActive("text", emptyText));

    auto defRect = MaskRegistry::buildDefault("rectangle", 1920.0, 1080.0);
    TEST_ASSERT(MaskRegistry::isActive("rectangle", defRect));

    std::cout << "[PASS] runMaskInteractionAndRegistryTests" << std::endl;
}

void runSceneNodesAndBuilderTests() {
    using namespace catchim;
    using namespace catchim::render;
    using namespace catchim::core;
    using namespace catchim::editor;

    // 1. RootNode & child management
    auto root = std::make_shared<RootNode>(TimelineTime::fromSeconds(10.0));
    TEST_ASSERT(root->type() == SceneNodeType::Root);
    TEST_ASSERT(root->duration() == TimelineTime::fromSeconds(10.0));
    root->setDuration(TimelineTime::fromSeconds(20.0));
    TEST_ASSERT(root->duration() == TimelineTime::fromSeconds(20.0));
    TEST_ASSERT(root->children().empty());

    auto colorNode = std::make_shared<ColorNode>("#ff5500");
    TEST_ASSERT(colorNode->type() == SceneNodeType::Color);
    TEST_ASSERT(colorNode->color() == "#ff5500");
    colorNode->setColor("#00ff00");
    TEST_ASSERT(colorNode->color() == "#00ff00");

    root->addChild(colorNode);
    TEST_ASSERT(root->children().size() == 1);
    TEST_ASSERT(root->children()[0]->type() == SceneNodeType::Color);

    root->removeChild(colorNode);
    TEST_ASSERT(root->children().empty());

    root->addChild(colorNode);
    root->clearChildren();
    TEST_ASSERT(root->children().empty());

    // 2. BlurBackgroundNode
    auto blurNode = std::make_shared<BlurBackgroundNode>(
        "media_bg", "file://bg.mp4", "video",
        TimelineTime::fromSeconds(5.0), TimelineTime::fromSeconds(1.0),
        TimelineTime::fromSeconds(0.5), TimelineTime::fromSeconds(0.0), 25.0
    );
    TEST_ASSERT(blurNode->type() == SceneNodeType::BlurBackground);
    TEST_ASSERT(blurNode->mediaId() == "media_bg");
    TEST_ASSERT(blurNode->url() == "file://bg.mp4");
    TEST_ASSERT(blurNode->mediaType() == "video");
    TEST_ASSERT(blurNode->duration() == TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(blurNode->timeOffset() == TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(blurNode->trimStart() == TimelineTime::fromSeconds(0.5));
    TEST_ASSERT(blurNode->trimEnd() == TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(blurNode->blurIntensity() == 25.0);

    // 3. EffectLayerNode
    nlohmann::json effParams = { {"intensity", 0.75} };
    auto effNode = std::make_shared<EffectLayerNode>(
        "vignette", effParams,
        TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(4.0)
    );
    TEST_ASSERT(effNode->type() == SceneNodeType::EffectLayer);
    TEST_ASSERT(effNode->effectType() == "vignette");
    TEST_ASSERT(effNode->effectParams()["intensity"] == 0.75);
    TEST_ASSERT(effNode->timeOffset() == TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(effNode->duration() == TimelineTime::fromSeconds(4.0));

    // 4. VideoNode & ImageNode
    Transform2D tform;
    tform.positionX = 100.0;
    tform.positionY = 50.0;
    tform.scaleX = 1.2;
    tform.scaleY = 1.2;
    auto videoNode = std::make_shared<VideoNode>(
        "elem_vid", "media_vid", "file://vid.mp4",
        TimelineTime::fromSeconds(10.0), TimelineTime::fromSeconds(0.0),
        TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(0.0),
        tform, 0.9, "screen", std::vector<editor::AnimationChannel>{},
        nlohmann::json::array(), nlohmann::json::array(), 1.5
    );
    TEST_ASSERT(videoNode->type() == SceneNodeType::Video);
    TEST_ASSERT(videoNode->elementId() == "elem_vid");
    TEST_ASSERT(videoNode->mediaId() == "media_vid");
    TEST_ASSERT(videoNode->url() == "file://vid.mp4");
    TEST_ASSERT(videoNode->duration() == TimelineTime::fromSeconds(10.0));
    TEST_ASSERT(videoNode->opacity() == 0.9);
    TEST_ASSERT(videoNode->blendMode() == "screen");
    TEST_ASSERT(videoNode->retimeRate() == 1.5);
    TEST_ASSERT(videoNode->transform().positionX == 100.0);

    auto imgNode = std::make_shared<ImageNode>(
        "elem_img", "media_img", "file://img.png",
        TimelineTime::fromSeconds(8.0), TimelineTime::fromSeconds(1.0),
        TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(0.0),
        tform, 1.0, "normal", std::vector<editor::AnimationChannel>{},
        nlohmann::json::array(), nlohmann::json::array(), 4096.0
    );
    TEST_ASSERT(imgNode->type() == SceneNodeType::Image);
    TEST_ASSERT(imgNode->elementId() == "elem_img");
    TEST_ASSERT(imgNode->maxSourceSize() == 4096.0);

    // 5. TextNode
    auto textNode = std::make_shared<TextNode>(
        "elem_text", TimelineTime::fromSeconds(6.0), TimelineTime::fromSeconds(0.5),
        "Catchim Video", "Roboto", 24.0, "bold", "italic", "center", "underline",
        1.5, 1.4, "#ffffff", "#000000", true, tform, 0.95
    );
    TEST_ASSERT(textNode->type() == SceneNodeType::Text);
    TEST_ASSERT(textNode->content() == "Catchim Video");
    TEST_ASSERT(textNode->fontFamily() == "Roboto");
    TEST_ASSERT(textNode->fontSize() == 24.0);
    TEST_ASSERT(textNode->fontWeight() == "bold");
    TEST_ASSERT(textNode->fontStyle() == "italic");
    TEST_ASSERT(textNode->textAlign() == "center");
    TEST_ASSERT(textNode->textDecoration() == "underline");
    TEST_ASSERT(textNode->letterSpacing() == 1.5);
    TEST_ASSERT(textNode->lineHeight() == 1.4);
    TEST_ASSERT(textNode->textColor() == "#ffffff");
    TEST_ASSERT(textNode->backgroundColor() == "#000000");
    TEST_ASSERT(textNode->backgroundEnabled() == true);
    TEST_ASSERT(textNode->opacity() == 0.95);

    // 6. StickerNode & GraphicNode
    auto stickerNode = std::make_shared<StickerNode>(
        "elem_stk", "heart_icon", 128.0, 128.0,
        TimelineTime::fromSeconds(4.0), TimelineTime::fromSeconds(0.0),
        TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(0.0),
        tform
    );
    TEST_ASSERT(stickerNode->type() == SceneNodeType::Sticker);
    TEST_ASSERT(stickerNode->stickerId() == "heart_icon");
    TEST_ASSERT(stickerNode->intrinsicWidth() == 128.0);
    TEST_ASSERT(stickerNode->intrinsicHeight() == 128.0);

    nlohmann::json graphicParams = { {"shape", "circle"}, {"fill", "#ffff00"} };
    auto graphicNode = std::make_shared<GraphicNode>(
        "elem_grp", "shape_def", graphicParams,
        TimelineTime::fromSeconds(3.0), TimelineTime::fromSeconds(0.0),
        TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(0.0),
        tform
    );
    TEST_ASSERT(graphicNode->type() == SceneNodeType::Graphic);
    TEST_ASSERT(graphicNode->definitionId() == "shape_def");
    TEST_ASSERT(graphicNode->params()["shape"] == "circle");

    // 7. SceneBuilder tests
    SceneBuilderParams params;
    params.canvasWidth = 1920.0;
    params.canvasHeight = 1080.0;
    params.background.type = "color";
    params.background.color = "#112233";
    params.duration = TimelineTime::fromSeconds(15.0);

    auto builtRoot = SceneBuilder::buildScene(params);
    TEST_ASSERT(builtRoot != nullptr);
    TEST_ASSERT(builtRoot->duration() == TimelineTime::fromSeconds(15.0));
    TEST_ASSERT(builtRoot->children().size() == 1);
    TEST_ASSERT(builtRoot->children()[0]->type() == SceneNodeType::Color);
    auto builtColor = std::dynamic_pointer_cast<ColorNode>(builtRoot->children()[0]);
    TEST_ASSERT(builtColor != nullptr);
    TEST_ASSERT(builtColor->color() == "#112233");

    // Test SceneBuilder with tracks and blur background
    Track track1(TrackId("track_1"), TrackType::Video, "Video Track");
    Clip clip1(ClipId("clip_1"), ClipType::Video, "Intro Video",
               TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(5.0));
    clip1.setMediaId(MediaId("media_1"));
    clip1.setParam("position_x", 200.0);
    clip1.setParam("position_y", 100.0);
    clip1.setParam("scale_x", 1.0);
    clip1.setParam("scale_y", 1.0);
    clip1.setParam("opacity", 1.0);
    track1.insertClip(clip1);

    params.tracks.push_back(track1);
    params.background.type = "blur";
    params.background.blurIntensity = 20.0;

    auto media1 = std::make_shared<media::MediaAsset>(MediaId("media_1"), "D:/test/intro.mp4", media::MediaType::Video);
    media1->setDimensions(1920, 1080);
    params.mediaAssets.push_back(media1);

    auto blurRoot = SceneBuilder::buildScene(params);
    TEST_ASSERT(blurRoot != nullptr);
    TEST_ASSERT(blurRoot->children().size() >= 2); // Blur node + Video node
    TEST_ASSERT(blurRoot->children()[0]->type() == SceneNodeType::BlurBackground);
    auto builtBlur = std::dynamic_pointer_cast<BlurBackgroundNode>(blurRoot->children()[0]);
    TEST_ASSERT(builtBlur != nullptr);
    TEST_ASSERT(builtBlur->mediaId() == "media_1");
    TEST_ASSERT(builtBlur->blurIntensity() == 20.0);

    std::cout << "[PASS] runSceneNodesAndBuilderTests" << std::endl;
}

void runSceneTreeResolverTests() {
    using namespace catchim;
    using namespace catchim::render;
    using namespace catchim::core;
    using namespace catchim::editor;

    // 1. VisualNode resolution (within vs outside bounds)
    Transform2D baseTransform;
    baseTransform.positionX = 50.0;
    baseTransform.positionY = 50.0;
    baseTransform.scaleX = 1.0;
    baseTransform.scaleY = 1.0;
    baseTransform.rotate = 0.0;

    std::vector<AnimationChannel> anims;
    AnimationChannel posXChannel("position_x", 50.0);
    posXChannel.addOrUpdateKeyframe({TimelineTime::fromSeconds(0.0), 50.0, KeyframeInterpolation::Linear});
    posXChannel.addOrUpdateKeyframe({TimelineTime::fromSeconds(4.0), 250.0, KeyframeInterpolation::Linear});
    anims.push_back(posXChannel);

    VideoNode vidNode(
        "clip_test", "media_test", "file://test.mp4",
        TimelineTime::fromSeconds(4.0), // duration
        TimelineTime::fromSeconds(2.0), // timeOffset
        TimelineTime::fromSeconds(1.0), // trimStart
        TimelineTime::fromSeconds(0.0), // trimEnd
        baseTransform, 0.8, "normal", anims,
        nlohmann::json::array(), nlohmann::json::array(), 2.0 // retimeRate = 2.0
    );

    // Before clip start: t = 1.0s (offset is 2.0s) -> nullopt
    auto resBefore = SceneTreeResolver::resolveVisualNode(vidNode, TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(!resBefore.has_value());

    // After clip end: t = 6.5s (offset 2.0 + duration 4.0 = 6.0s) -> nullopt
    auto resAfter = SceneTreeResolver::resolveVisualNode(vidNode, TimelineTime::fromSeconds(6.5));
    TEST_ASSERT(!resAfter.has_value());

    // Exactly inside: t = 4.0s
    // elapsed = 4.0 - 2.0 = 2.0s
    // localTime = elapsed = 2.0s
    // sourceTime = trimStart + elapsed * retimeRate = 1.0 + 2.0 * 2.0 = 5.0s
    auto resActive = SceneTreeResolver::resolveVisualNode(vidNode, TimelineTime::fromSeconds(4.0));
    TEST_ASSERT(resActive.has_value());
    TEST_ASSERT(resActive->localTime == TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(resActive->sourceTime == TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(resActive->opacity == 0.8);
    // posX at localTime 2.0s: lerp(50, 250, 2/4) = 50 + 200 * 0.5 = 150.0
    TEST_ASSERT(std::abs(resActive->transform.positionX - 150.0) < 1e-4);

    // 2. TextNode resolution
    TextNode txtNode(
        "txt_test", TimelineTime::fromSeconds(5.0), TimelineTime::fromSeconds(1.0),
        "Hello World", "Arial", 30.0, "normal", "normal", "center", "none",
        0.0, 1.2, "#ff0000", "#0000ff", true, baseTransform, 1.0
    );

    auto txtBefore = SceneTreeResolver::resolveTextNode(txtNode, TimelineTime::fromSeconds(0.5));
    TEST_ASSERT(!txtBefore.has_value());

    auto txtActive = SceneTreeResolver::resolveTextNode(txtNode, TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(txtActive.has_value());
    TEST_ASSERT(txtActive->textColor == "#ff0000");
    TEST_ASSERT(txtActive->backgroundColor == "#0000ff");
    TEST_ASSERT(txtActive->localTime == TimelineTime::fromSeconds(1.0)); // 2.0 - 1.0 = 1.0s

    // 3. BlurBackgroundNode resolution
    BlurBackgroundNode blurNode(
        "media_bg", "file://bg.mp4", "video",
        TimelineTime::fromSeconds(6.0), TimelineTime::fromSeconds(0.0),
        TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(0.0), 30.0
    );
    auto blurRes = SceneTreeResolver::resolveBlurBackgroundNode(blurNode, TimelineTime::fromSeconds(3.0));
    TEST_ASSERT(blurRes.has_value());
    TEST_ASSERT(!blurRes->passes.empty());

    // 4. EffectLayerNode resolution
    nlohmann::json effParams = { {"radius", 0.5} };
    EffectLayerNode effLayer(
        "blur", effParams, TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(4.0)
    );
    auto effRes = SceneTreeResolver::resolveEffectLayerNode(effLayer, TimelineTime::fromSeconds(2.5));
    TEST_ASSERT(effRes.has_value());
    TEST_ASSERT(!effRes->effectPassGroups.empty());

    auto effResOut = SceneTreeResolver::resolveEffectLayerNode(effLayer, TimelineTime::fromSeconds(6.0));
    TEST_ASSERT(!effResOut.has_value());

    std::cout << "[PASS] runSceneTreeResolverTests" << std::endl;
}

void runFrameDescriptorBuilderTests() {
    using namespace catchim;
    using namespace catchim::render;
    using namespace catchim::core;

    // 1. fullCanvasTransform
    auto fullQuad = FrameDescriptorBuilder::fullCanvasTransform(1920, 1080);
    TEST_ASSERT(fullQuad.centerX == 960.0);
    TEST_ASSERT(fullQuad.centerY == 540.0);
    TEST_ASSERT(fullQuad.width == 1920.0);
    TEST_ASSERT(fullQuad.height == 1080.0);
    TEST_ASSERT(fullQuad.rotationDegrees == 0.0);
    TEST_ASSERT(!fullQuad.flipX);
    TEST_ASSERT(!fullQuad.flipY);

    // 2. computeVisualTransform
    Transform2D tform;
    tform.positionX = 100.0;
    tform.positionY = -50.0;
    tform.scaleX = 2.0;
    tform.scaleY = 1.5;
    tform.rotate = 45.0;

    auto quad = FrameDescriptorBuilder::computeVisualTransform(tform, 800.0, 600.0, 1920, 1080);
    // Canvas center (960, 540) + offset (100, -50) = (1060, 490)
    TEST_ASSERT(std::abs(quad.centerX - 1060.0) < 1e-4);
    TEST_ASSERT(std::abs(quad.centerY - 490.0) < 1e-4);
    // containScale for 800x600 into 1920x1080: min(1920/800 = 2.4, 1080/600 = 1.8) = 1.8
    // scaledWidth: 800 * 1.8 * 2.0 = 2880.0
    // scaledHeight: 600 * 1.8 * 1.5 = 1620.0
    TEST_ASSERT(std::abs(quad.width - 2880.0) < 1e-4);
    TEST_ASSERT(std::abs(quad.height - 1620.0) < 1e-4);
    TEST_ASSERT(std::abs(quad.rotationDegrees - 45.0) < 1e-4);
    TEST_ASSERT(quad.flipX == false);
    TEST_ASSERT(quad.flipY == false);

    // 3. transformHash
    std::string hash1 = FrameDescriptorBuilder::transformHash(quad);
    TEST_ASSERT(!hash1.empty());
    auto quad2 = quad;
    quad2.rotationDegrees = 90.0;
    std::string hash2 = FrameDescriptorBuilder::transformHash(quad2);
    TEST_ASSERT(hash1 != hash2);

    // 4. buildFrameDescriptor
    auto root = std::make_shared<RootNode>(TimelineTime::fromSeconds(10.0));
    root->addChild(std::make_shared<ColorNode>("#ff0000")); // Node 0: ColorNode

    nlohmann::json masks = nlohmann::json::array({
        {
            {"id", "mask_1"},
            {"type", "rectangle"},
            {"params", {{"feather", 10.0}, {"inverted", true}}}
        }
    });

    auto vid = std::make_shared<VideoNode>(
        "vid_elem", "vid_media", "file://clip.mp4",
        TimelineTime::fromSeconds(5.0), TimelineTime::fromSeconds(0.0),
        TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(0.0),
        tform, 0.85, "multiply", std::vector<editor::AnimationChannel>{},
        nlohmann::json::array(), masks, 1.0
    );
    root->addChild(vid); // Node 1: VideoNode

    auto frameDesc = FrameDescriptorBuilder::buildFrameDescriptor(root, TimelineTime::fromSeconds(2.0), 1920, 1080);
    TEST_ASSERT(frameDesc.width == 1920);
    TEST_ASSERT(frameDesc.height == 1080);

    // Check items: Item 0 is ColorNode, Item 1 is VideoNode
    TEST_ASSERT(frameDesc.items.size() == 2);
    const auto& item0 = frameDesc.items[0];
    TEST_ASSERT(item0.type == FrameItemType::Layer);
    TEST_ASSERT(item0.textureId == "root:0:color");

    const auto& item1 = frameDesc.items[1];
    TEST_ASSERT(item1.type == FrameItemType::Layer);
    TEST_ASSERT(item1.textureId == "root:1:source");
    TEST_ASSERT(item1.opacity == 0.85);
    TEST_ASSERT(item1.blendMode == "multiply");
    TEST_ASSERT(item1.mask.has_value());
    TEST_ASSERT(item1.mask->textureId == "root:1:mask");
    TEST_ASSERT(item1.mask->feather == 10.0);
    TEST_ASSERT(item1.mask->inverted == true);

    // Check textures: 1 rendered color, 1 external video source, 1 rendered mask
    TEST_ASSERT(frameDesc.textures.size() == 3);
    bool foundColor = false;
    bool foundExternal = false;
    bool foundMask = false;
    for (const auto& tex : frameDesc.textures) {
        if (tex.kind == TextureUploadKind::Rendered && tex.id == "root:0:color") {
            foundColor = true;
        }
        if (tex.kind == TextureUploadKind::External && tex.id == "root:1:source") {
            foundExternal = true;
        }
        if (tex.kind == TextureUploadKind::Rendered && tex.id == "root:1:mask") {
            foundMask = true;
        }
    }
    TEST_ASSERT(foundColor);
    TEST_ASSERT(foundExternal);
    TEST_ASSERT(foundMask);

    std::cout << "[PASS] runFrameDescriptorBuilderTests" << std::endl;
}

void runRenderSurfaceTests() {
    using namespace catchim::render;

    // 1. Construction and properties
    RenderSurface surf(100, 50);
    TEST_ASSERT(surf.width() == 100);
    TEST_ASSERT(surf.height() == 50);
    TEST_ASSERT(surf.stride() == 400);
    TEST_ASSERT(surf.byteSize() == 100 * 50 * sizeof(uint32_t));
    TEST_ASSERT(surf.data() != nullptr);

    // 2. Clear with uint8_t
    surf.clear(255, 128, 64, 255);
    uint32_t expected1 = RenderSurface::makeRgba(255, 128, 64, 255);
    TEST_ASSERT(surf.getPixel(0, 0) == expected1);
    TEST_ASSERT(surf.getPixel(50, 25) == expected1);
    TEST_ASSERT(surf.getPixel(99, 49) == expected1);

    // 3. Pixel manipulation & bounds checking
    surf.setPixel(20, 20, RenderSurface::makeRgba(0, 0, 0, 255));
    TEST_ASSERT(surf.getPixel(20, 20) == RenderSurface::makeRgba(0, 0, 0, 255));
    surf.setPixel(-1, 0, 123); // Should not crash
    surf.setPixel(100, 50, 123); // Should not crash
    TEST_ASSERT(surf.getPixel(-1, 0) == 0);
    TEST_ASSERT(surf.getPixel(100, 50) == 0);

    // 4. Resize and clear with array<double, 4>
    surf.resize(200, 100);
    TEST_ASSERT(surf.width() == 200);
    TEST_ASSERT(surf.height() == 100);
    surf.clear({1.0, 0.0, 0.0, 1.0}); // Pure red
    uint8_t r, g, b, a;
    RenderSurface::unpackRgba(surf.getPixel(10, 10), r, g, b, a);
    TEST_ASSERT(r == 255 && g == 0 && b == 0 && a == 255);

    // 5. copyFrom
    RenderSurface patch(20, 20);
    patch.clear(0, 255, 0, 255); // Green patch
    surf.copyFrom(patch, 30, 30);
    RenderSurface::unpackRgba(surf.getPixel(30, 30), r, g, b, a);
    TEST_ASSERT(r == 0 && g == 255 && b == 0 && a == 255);
    RenderSurface::unpackRgba(surf.getPixel(0, 0), r, g, b, a);
    TEST_ASSERT(r == 255 && g == 0 && b == 0); // Original red untouched

    // 6. blendOver with normal and multiply
    RenderSurface bg(100, 100);
    bg.clear(255, 0, 0, 255); // Red

    RenderSurface fg(100, 100);
    fg.clear(0, 0, 255, 255); // Blue

    QuadTransformDescriptor tform{
        .centerX = 50.0,
        .centerY = 50.0,
        .width = 100.0,
        .height = 100.0,
        .rotationDegrees = 0.0,
        .flipX = false,
        .flipY = false
    };

    // Blend 50% blue over red -> purple
    bg.blendOver(fg, tform, 0.5, "normal");
    RenderSurface::unpackRgba(bg.getPixel(50, 50), r, g, b, a);
    TEST_ASSERT(r > 100 && r < 200);
    TEST_ASSERT(b > 100 && b < 200);
    TEST_ASSERT(a == 255);

    std::cout << "[PASS] runRenderSurfaceTests" << std::endl;
}

void runTextureCacheManagerTests() {
    using namespace catchim::render;

    TextureCacheManager cache;
    TEST_ASSERT(cache.cachedCount() == 0);
    TEST_ASSERT(cache.cacheHits() == 0);
    TEST_ASSERT(cache.cacheMisses() == 0);

    // 1. registerExternalTexture
    auto extSurf = std::make_shared<RenderSurface>(640, 480);
    extSurf->clear(10, 20, 30, 255);
    cache.registerExternalTexture("video_frame_1", extSurf);
    TEST_ASSERT(cache.cachedCount() == 1);
    TEST_ASSERT(cache.getTexture("video_frame_1") == extSurf);

    // 2. rasterizeRenderedTexture with contentHash
    bool drawn = false;
    cache.rasterizeRenderedTexture("text_tex", "hash_v1", 200, 50, [&](RenderSurface& s) {
        s.clear(255, 255, 255, 255);
        drawn = true;
    });
    TEST_ASSERT(drawn);
    TEST_ASSERT(cache.cachedCount() == 2);
    TEST_ASSERT(cache.getTexture("text_tex") != nullptr);
    TEST_ASSERT(cache.cacheMisses() == 1);

    // Calling again with same hash -> hit!
    drawn = false;
    cache.rasterizeRenderedTexture("text_tex", "hash_v1", 200, 50, [&](RenderSurface&) {
        drawn = true;
    });
    TEST_ASSERT(!drawn); // Callback should not be called on cache hit
    TEST_ASSERT(cache.cacheHits() == 1);

    // Calling with new hash -> miss & re-drawn
    drawn = false;
    cache.rasterizeRenderedTexture("text_tex", "hash_v2", 200, 50, [&](RenderSurface&) {
        drawn = true;
    });
    TEST_ASSERT(drawn);
    TEST_ASSERT(cache.cacheMisses() == 2);

    // 3. syncTextures with automatic eviction
    std::vector<TextureUploadDescriptor> descriptors = {
        TextureUploadDescriptor{
            .kind = TextureUploadKind::External,
            .id = "active_ext",
            .contentHash = "",
            .width = 100,
            .height = 100
        },
        TextureUploadDescriptor{
            .kind = TextureUploadKind::Rendered,
            .id = "active_rend",
            .contentHash = "hash_render_1",
            .width = 100,
            .height = 100
        }
    };

    cache.syncTextures(descriptors);
    // Previous "video_frame_1" and "text_tex" should be evicted
    TEST_ASSERT(cache.getTexture("video_frame_1") == nullptr);
    TEST_ASSERT(cache.getTexture("text_tex") == nullptr);
    TEST_ASSERT(cache.getTexture("active_ext") != nullptr);
    TEST_ASSERT(cache.getTexture("active_rend") != nullptr);
    TEST_ASSERT(cache.cachedCount() == 2);

    // Second sync with same descriptors -> both hit
    uint64_t hitsBefore = cache.cacheHits();
    cache.syncTextures(descriptors);
    TEST_ASSERT(cache.cacheHits() == hitsBefore + 2);

    // 4. Clear
    cache.clear();
    TEST_ASSERT(cache.cachedCount() == 0);
    TEST_ASSERT(cache.cacheHits() == 0);
    TEST_ASSERT(cache.cacheMisses() == 0);

    std::cout << "[PASS] runTextureCacheManagerTests" << std::endl;
}

void runCanvasRendererAndEffectPreviewTests() {
    using namespace catchim;
    using namespace catchim::render;
    using namespace catchim::core;

    // 1. CanvasRenderer
    CanvasRenderer renderer(CanvasRendererParams{
        .width = 640,
        .height = 360,
        .fps = {30, 1}
    });
    TEST_ASSERT(renderer.width() == 640);
    TEST_ASSERT(renderer.height() == 360);

    auto root = std::make_shared<RootNode>(TimelineTime::fromSeconds(10.0));
    root->addChild(std::make_shared<ColorNode>("#00ff00")); // Green background

    RenderSurface outputSurface(640, 360);
    renderer.render(root, TimelineTime::fromSeconds(1.0), outputSurface);

    TEST_ASSERT(outputSurface.width() == 640);
    TEST_ASSERT(outputSurface.height() == 360);

    // 2. EffectPreviewService
    EffectPreviewService previewService;
    TEST_ASSERT(previewService.previewSize() == 160);

    auto testSrc = previewService.getTestSource(160, 160);
    TEST_ASSERT(testSrc != nullptr);
    TEST_ASSERT(testSrc->width() == 160 && testSrc->height() == 160);

    // Test blur preview
    RenderSurface blurThumb;
    previewService.renderPreview("blur", { {"intensity", 10.0} }, blurThumb);
    TEST_ASSERT(blurThumb.width() == 160 && blurThumb.height() == 160);

    // Test vignette preview
    RenderSurface vigThumb;
    previewService.renderPreview("vignette", { {"intensity", 0.6}, {"radius", 0.5} }, vigThumb);
    TEST_ASSERT(vigThumb.width() == 160 && vigThumb.height() == 160);

    // Test grayscale preview: r == g == b
    RenderSurface grayThumb;
    previewService.renderPreview("grayscale", {}, grayThumb);
    TEST_ASSERT(grayThumb.width() == 160 && grayThumb.height() == 160);
    uint32_t p = grayThumb.getPixel(50, 50);
    uint8_t r, g, b, a;
    RenderSurface::unpackRgba(p, r, g, b, a);
    TEST_ASSERT(r == g && g == b);
    TEST_ASSERT(a == 255);

    // Test invert preview
    RenderSurface invThumb;
    previewService.renderPreview("invert", {}, invThumb);
    TEST_ASSERT(invThumb.width() == 160 && invThumb.height() == 160);

    std::cout << "[PASS] runCanvasRendererAndEffectPreviewTests" << std::endl;
}

void runTimelineSnappingEngineTests() {
    using namespace catchim::editor;
    using namespace catchim::editor::snapping;
    using namespace catchim::core;

    // 1. Constants & Type Strings
    double defaultSnapPx = TimelineSnappingEngine::DEFAULT_TIMELINE_SNAP_THRESHOLD_PX;
    double basePixelsPerSec = TimelineSnappingEngine::BASE_TIMELINE_PIXELS_PER_SECOND;
    TEST_ASSERT(defaultSnapPx == 10.0);
    TEST_ASSERT(basePixelsPerSec == 100.0);
    TEST_ASSERT(std::string(snapPointTypeToString(TimelineSnapPointType::ElementStart)) == "element-start");
    TEST_ASSERT(std::string(snapPointTypeToString(TimelineSnapPointType::ElementEnd)) == "element-end");
    TEST_ASSERT(std::string(snapPointTypeToString(TimelineSnapPointType::Playhead)) == "playhead");
    TEST_ASSERT(std::string(snapPointTypeToString(TimelineSnapPointType::Bookmark)) == "bookmark");
    TEST_ASSERT(std::string(snapPointTypeToString(TimelineSnapPointType::Keyframe)) == "keyframe");

    // 2. Threshold in Ticks calculation
    // threshold = (10.0 / (100.0 * 1.0)) * 120,000 = 12,000 ticks
    auto thresh1 = TimelineSnappingEngine::getTimelineSnapThresholdInTicks(1.0, 10.0);
    TEST_ASSERT(thresh1.ticks() == 12000);

    // at zoom 2.0 -> threshold = 6,000 ticks
    auto thresh2 = TimelineSnappingEngine::getTimelineSnapThresholdInTicks(2.0, 10.0);
    TEST_ASSERT(thresh2.ticks() == 6000);

    // at zoom 0.5 -> threshold = 24,000 ticks
    auto threshHalf = TimelineSnappingEngine::getTimelineSnapThresholdInTicks(0.5, 10.0);
    TEST_ASSERT(threshHalf.ticks() == 24000);

    // at invalid zoom <= 0 -> returns 0 ticks
    auto threshZero = TimelineSnappingEngine::getTimelineSnapThresholdInTicks(0.0, 10.0);
    TEST_ASSERT(threshZero.ticks() == 0);

    // 3. isSortedByTime check
    std::vector<TimelineSnapPoint> emptyPoints;
    TEST_ASSERT(TimelineSnappingEngine::isSortedByTime(emptyPoints));

    std::vector<TimelineSnapPoint> sortedPoints = {
        TimelineSnapPoint{.time = TimelineTime(1000), .type = TimelineSnapPointType::ElementStart},
        TimelineSnapPoint{.time = TimelineTime(2000), .type = TimelineSnapPointType::ElementEnd},
        TimelineSnapPoint{.time = TimelineTime(3000), .type = TimelineSnapPointType::Playhead}
    };
    TEST_ASSERT(TimelineSnappingEngine::isSortedByTime(sortedPoints));

    std::vector<TimelineSnapPoint> unsortedPoints = {
        TimelineSnapPoint{.time = TimelineTime(3000), .type = TimelineSnapPointType::Playhead},
        TimelineSnapPoint{.time = TimelineTime(1000), .type = TimelineSnapPointType::ElementStart}
    };
    TEST_ASSERT(!TimelineSnappingEngine::isSortedByTime(unsortedPoints));

    // 4. buildTimelineSnapPoints & buildSortedTimelineSnapPoints
    std::vector<TimelineSnapPointSource> sources = {
        []() -> std::vector<TimelineSnapPoint> {
            return {
                TimelineSnapPoint{.time = TimelineTime(5000), .type = TimelineSnapPointType::ElementStart},
                TimelineSnapPoint{.time = TimelineTime(1000), .type = TimelineSnapPointType::ElementEnd}
            };
        },
        []() -> std::vector<TimelineSnapPoint> {
            return {
                TimelineSnapPoint{.time = TimelineTime(3000), .type = TimelineSnapPointType::Keyframe}
            };
        }
    };

    auto builtPoints = TimelineSnappingEngine::buildTimelineSnapPoints(sources);
    TEST_ASSERT(builtPoints.size() == 3);
    TEST_ASSERT(builtPoints[0].time.ticks() == 5000);
    TEST_ASSERT(builtPoints[1].time.ticks() == 1000);
    TEST_ASSERT(builtPoints[2].time.ticks() == 3000);

    auto sortedBuilt = TimelineSnappingEngine::buildSortedTimelineSnapPoints(sources);
    TEST_ASSERT(sortedBuilt.size() == 3);
    TEST_ASSERT(sortedBuilt[0].time.ticks() == 1000);
    TEST_ASSERT(sortedBuilt[1].time.ticks() == 3000);
    TEST_ASSERT(sortedBuilt[2].time.ticks() == 5000);
    TEST_ASSERT(TimelineSnappingEngine::isSortedByTime(sortedBuilt));

    // 5. Linear and Sorted Snapping Resolution
    // Target 2950 within distance 100 -> snaps to 3000 (distance = 50)
    auto resLinear = TimelineSnappingEngine::resolveTimelineSnapLinear(
        TimelineTime(2950), sortedBuilt, TimelineTime(100)
    );
    TEST_ASSERT(resLinear.snappedTime.ticks() == 3000);
    TEST_ASSERT(resLinear.snapPoint.has_value());
    TEST_ASSERT(resLinear.snapDistanceTicks == 50);

    auto resSorted = TimelineSnappingEngine::resolveSortedTimelineSnap(
        TimelineTime(2950), sortedBuilt, TimelineTime(100)
    );
    TEST_ASSERT(resSorted.snappedTime.ticks() == 3000);
    TEST_ASSERT(resSorted.snapPoint.has_value());
    TEST_ASSERT(resSorted.snapDistanceTicks == 50);

    // Target 2500 within distance 100 -> out of reach -> no snap
    auto resNone = TimelineSnappingEngine::resolveSortedTimelineSnap(
        TimelineTime(2500), sortedBuilt, TimelineTime(100)
    );
    TEST_ASSERT(resNone.snappedTime.ticks() == 2500);
    TEST_ASSERT(!resNone.snapPoint.has_value());
    TEST_ASSERT(resNone.snapDistanceTicks == std::numeric_limits<int64_t>::max());

    // Generic resolveTimelineSnap (which dispatches sorted or linear)
    auto resGeneric = TimelineSnappingEngine::resolveTimelineSnap(
        TimelineTime(2950), sortedBuilt, TimelineTime(100)
    );
    TEST_ASSERT(resGeneric.snappedTime.ticks() == 3000);

    std::cout << "[PASS] runTimelineSnappingEngineTests" << std::endl;
}

void runTimelineSnapPointSourcesTests() {
    using namespace catchim::editor;
    using namespace catchim::editor::snapping;
    using namespace catchim::core;

    // 1. Setup Tracks & Clips
    Track track1(TrackId("t1"), TrackType::Video, "Video Track");

    Clip clip1(ClipId("c1"), ClipType::Video, "Clip 1", TimelineTime(0), TimelineTime(120000));
    Clip clip2(ClipId("c2"), ClipType::Video, "Clip 2", TimelineTime(150000), TimelineTime(60000));

    track1.insertClip(clip1);
    track1.insertClip(clip2);

    Track track2(TrackId("t2"), TrackType::Audio, "Audio Track");

    Clip clip3(ClipId("c3"), ClipType::Audio, "Clip 3", TimelineTime(60000), TimelineTime(120000));

    track2.insertClip(clip3);

    std::vector<Track> tracks = { track1, track2 };

    // 2. Element Edge Snap Points
    auto edgePoints = TimelineSnapPointSources::getElementEdgeSnapPoints(tracks);
    // c1: 0, 120000; c2: 150000, 210000; c3: 60000, 180000 -> total 6
    TEST_ASSERT(edgePoints.size() == 6);

    // Exclude c2
    std::unordered_set<std::string> excluded = { "c2" };
    auto edgeExcluded = TimelineSnapPointSources::getElementEdgeSnapPoints(tracks, excluded);
    TEST_ASSERT(edgeExcluded.size() == 4);
    for (const auto& pt : edgeExcluded) {
        TEST_ASSERT(pt.elementId != "c2");
    }

    // 3. Playhead Snap Points
    auto playheadPts = TimelineSnapPointSources::getPlayheadSnapPoints(TimelineTime(75000));
    TEST_ASSERT(playheadPts.size() == 1);
    TEST_ASSERT(playheadPts[0].type == TimelineSnapPointType::Playhead);
    TEST_ASSERT(playheadPts[0].time.ticks() == 75000);

    // 4. Animation Keyframe Snap Points
    // Add keyframes to track1's clip1 and clip2
    auto& c1Ref = tracks[0].clips()[0];
    Keyframe kf1{.time = TimelineTime(10000), .value = 0.5};
    Keyframe kf2{.time = TimelineTime(50000), .value = 1.0};
    c1Ref.getOrCreateAnimationChannel("opacity", 1.0).addOrUpdateKeyframe(kf1);
    c1Ref.getOrCreateAnimationChannel("opacity", 1.0).addOrUpdateKeyframe(kf2);

    auto& c2Ref = tracks[0].clips()[1];
    Keyframe kf3{.time = TimelineTime(20000), .value = 2.0};
    c2Ref.getOrCreateAnimationChannel("scale_x", 1.0).addOrUpdateKeyframe(kf3);

    auto kfPoints = TimelineSnapPointSources::getAnimationKeyframeSnapPoints(tracks);
    // kf1: 0 + 10000 = 10000
    // kf2: 0 + 50000 = 50000
    // kf3: 150000 + 20000 = 170000
    TEST_ASSERT(kfPoints.size() == 3);

    // Exclude c1
    auto kfPointsExcluded = TimelineSnapPointSources::getAnimationKeyframeSnapPoints(tracks, { "c1" });
    TEST_ASSERT(kfPointsExcluded.size() == 1);
    TEST_ASSERT(kfPointsExcluded[0].elementId == "c2");
    TEST_ASSERT(kfPointsExcluded[0].time.ticks() == 170000);

    std::cout << "[PASS] runTimelineSnapPointSourcesTests" << std::endl;
}

void runAnimationTargetResolverTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. Transform / Element parameters
    Clip clip(ClipId("test_clip"), ClipType::Video, "Test Clip", TimelineTime(0), TimelineTime(120000));
    clip.setParam("position_x", 120.0);
    clip.setParam("position_y", -45.0);
    clip.setParam("scale_x", 1.5);
    clip.setParam("scale_y", 1.5);
    clip.setParam("rotate", 45.0);
    clip.setParam("opacity", 0.85);
    clip.setParam("volume", 1.2);

    // position_x
    auto targetPosX = AnimationTargetResolver::resolveAnimationTarget(clip, "position_x");
    TEST_ASSERT(targetPosX.has_value());
    TEST_ASSERT(targetPosX->channelLayout == "scalar");
    TEST_ASSERT(targetPosX->getBaseValue(clip) == 120.0);
    targetPosX->setBaseValue(clip, 200.0);
    TEST_ASSERT(clip.getParam<double>("position_x", 0.0) == 200.0);

    // transform.positionY
    auto targetPosY = AnimationTargetResolver::resolveAnimationTarget(clip, "transform.positionY");
    TEST_ASSERT(targetPosY.has_value());
    TEST_ASSERT(targetPosY->getBaseValue(clip) == -45.0);

    // scale_x with NumericRange [0.01, 100.0]
    auto targetScaleX = AnimationTargetResolver::resolveAnimationTarget(clip, "scale_x");
    TEST_ASSERT(targetScaleX.has_value());
    TEST_ASSERT(targetScaleX->numericRange.has_value());
    TEST_ASSERT(targetScaleX->numericRange->min == 0.01);
    TEST_ASSERT(targetScaleX->numericRange->max == 100.0);
    TEST_ASSERT(targetScaleX->coerceValue(0.005) == 0.01);
    TEST_ASSERT(targetScaleX->coerceValue(150.0) == 100.0);
    targetScaleX->setBaseValue(clip, 0.0001);
    TEST_ASSERT(clip.getParam<double>("scale_x", 1.0) == 0.01);

    // opacity with NumericRange [0.0, 1.0]
    auto targetOpacity = AnimationTargetResolver::resolveAnimationTarget(clip, "opacity");
    TEST_ASSERT(targetOpacity.has_value());
    TEST_ASSERT(targetOpacity->numericRange.has_value());
    TEST_ASSERT(targetOpacity->coerceValue(-0.5) == 0.0);
    TEST_ASSERT(targetOpacity->coerceValue(1.5) == 1.0);

    // volume with NumericRange [0.0, 2.0]
    auto targetVolume = AnimationTargetResolver::resolveAnimationTarget(clip, "volume");
    TEST_ASSERT(targetVolume.has_value());
    TEST_ASSERT(targetVolume->numericRange.has_value());
    TEST_ASSERT(targetVolume->numericRange->max == 2.0);

    // 2. Graphic parameters
    Clip graphicClip(ClipId("graphic_clip"), ClipType::Graphic, "Graphic Clip", TimelineTime(0), TimelineTime(120000));
    graphicClip.setParam("fill_opacity", 0.75);

    auto targetGraphic = AnimationTargetResolver::resolveAnimationTarget(graphicClip, "graphics.fill_opacity");
    TEST_ASSERT(targetGraphic.has_value());
    TEST_ASSERT(targetGraphic->getBaseValue(graphicClip) == 0.75);
    targetGraphic->setBaseValue(graphicClip, 0.35);
    TEST_ASSERT(graphicClip.getParam<double>("fill_opacity", 0.0) == 0.35);

    // Non-graphic clip should return nullopt for graphics.*
    auto nonGraphicTarget = AnimationTargetResolver::resolveAnimationTarget(clip, "graphics.fill_opacity");
    TEST_ASSERT(!nonGraphicTarget.has_value());

    // 3. Effect parameters
    EffectInstance eff;
    eff.id = "blur_1";
    eff.type = "blur";
    eff.params["radius"] = 12.5;

    clip.setEffects({ eff });

    auto targetEff = AnimationTargetResolver::resolveAnimationTarget(clip, "effects.blur_1.params.radius");
    TEST_ASSERT(targetEff.has_value());
    TEST_ASSERT(targetEff->getBaseValue(clip) == 12.5);
    targetEff->setBaseValue(clip, 30.0);

    // Verify updated effect param in clip
    TEST_ASSERT(clip.effects()[0].params["radius"].get<double>() == 30.0);

    // Non-existent effect id
    auto missingEff = AnimationTargetResolver::resolveAnimationTarget(clip, "effects.missing.params.radius");
    TEST_ASSERT(!missingEff.has_value());

    // Invalid path
    auto invalidTarget = AnimationTargetResolver::resolveAnimationTarget(clip, "invalid.unknown.param");
    TEST_ASSERT(!invalidTarget.has_value());

    std::cout << "[PASS] runAnimationTargetResolverTests" << std::endl;
}

void runTimelineDefaultsTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. Durations
    int64_t defSec = TimelineDefaults::DEFAULT_NEW_ELEMENT_DURATION_SECONDS;
    TEST_ASSERT(defSec == 5);
    auto defDur = TimelineDefaults::defaultNewElementDuration();
    TEST_ASSERT(defDur.ticks() == 5 * 120000);

    // toElementDurationTicks
    TEST_ASSERT(TimelineDefaults::toElementDurationTicks(std::nullopt).ticks() == 600000);
    TEST_ASSERT(TimelineDefaults::toElementDurationTicks(-1.0).ticks() == 600000);
    TEST_ASSERT(TimelineDefaults::toElementDurationTicks(2.5).ticks() == 300000);

    // 2. Default element params
    auto trans = TimelineDefaults::defaultTransform();
    TEST_ASSERT(trans.scaleX == 1.0);
    TEST_ASSERT(trans.scaleY == 1.0);
    TEST_ASSERT(trans.positionX == 0.0);
    TEST_ASSERT(trans.positionY == 0.0);
    TEST_ASSERT(trans.rotate == 0.0);

    TEST_ASSERT(TimelineDefaults::defaultOpacity() == 1.0);
    TEST_ASSERT(TimelineDefaults::defaultBlendMode() == "normal");
    TEST_ASSERT(TimelineDefaults::defaultVolume() == 0.0);

    // 3. Default text element params
    auto textParams = TimelineDefaults::defaultTextParams();
    TEST_ASSERT(textParams.content == "Default text");
    TEST_ASSERT(textParams.fontSize == 15.0);
    TEST_ASSERT(textParams.fontFamily == "Arial");
    TEST_ASSERT(textParams.color == "#ffffff");
    TEST_ASSERT(textParams.textAlign == "center");
    TEST_ASSERT(textParams.letterSpacing == 0.0);
    TEST_ASSERT(textParams.lineHeight == 1.2);
    TEST_ASSERT(!textParams.background.enabled);
    TEST_ASSERT(textParams.background.color == "#000000");

    auto textJson = TimelineDefaults::buildDefaultTextElementParams();
    TEST_ASSERT(textJson["content"] == "Default text");
    TEST_ASSERT(textJson["fontSize"] == 15.0);
    TEST_ASSERT(textJson["opacity"] == 1.0);
    TEST_ASSERT(textJson["blendMode"] == "normal");
    TEST_ASSERT(textJson["transform.scaleX"] == 1.0);

    // 4. Default view state
    auto viewState = TimelineDefaults::defaultTimelineViewState();
    TEST_ASSERT(viewState.zoomLevel == 1.0);
    TEST_ASSERT(viewState.scrollLeft == 0.0);
    TEST_ASSERT(viewState.playheadTime.ticks() == 0);

    std::cout << "[PASS] runTimelineDefaultsTests" << std::endl;
}

void runAnimationTransformResolverTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. Setup Clip with base parameters
    Clip clip(ClipId("anim_clip"), ClipType::Video, "Anim Clip", TimelineTime(0), TimelineTime(240000));
    clip.setParam("position_x", 50.0);
    clip.setParam("position_y", 75.0);
    clip.setParam("scale_x", 1.2);
    clip.setParam("scale_y", 1.2);
    clip.setParam("rotate", 15.0);
    clip.setParam("opacity", 0.9);
    clip.setParam("volume", 0.8);

    // Without keyframe animation, returns base params
    auto baseTrans = AnimationTransformResolver::resolveTransformAtTime(clip, TimelineTime(0));
    TEST_ASSERT(baseTrans.positionX == 50.0);
    TEST_ASSERT(baseTrans.positionY == 75.0);
    TEST_ASSERT(baseTrans.scaleX == 1.2);
    TEST_ASSERT(baseTrans.scaleY == 1.2);
    TEST_ASSERT(baseTrans.rotate == 15.0);
    TEST_ASSERT(AnimationTransformResolver::resolveOpacityAtTime(clip, TimelineTime(0)) == 0.9);
    TEST_ASSERT(AnimationTransformResolver::resolveVolumeAtTime(clip, TimelineTime(0)) == 0.8);

    // 2. Add animation channels
    Keyframe kfPos0{.time = TimelineTime(0), .value = 0.0};
    Keyframe kfPos1{.time = TimelineTime(120000), .value = 100.0}; // 1 sec
    clip.getOrCreateAnimationChannel("position_x", 0.0).addOrUpdateKeyframe(kfPos0);
    clip.getOrCreateAnimationChannel("position_x", 0.0).addOrUpdateKeyframe(kfPos1);

    Keyframe kfOp0{.time = TimelineTime(0), .value = 0.2};
    Keyframe kfOp1{.time = TimelineTime(120000), .value = 1.0};
    clip.getOrCreateAnimationChannel("opacity", 1.0).addOrUpdateKeyframe(kfOp0);
    clip.getOrCreateAnimationChannel("opacity", 1.0).addOrUpdateKeyframe(kfOp1);

    Keyframe kfVol0{.time = TimelineTime(0), .value = 0.5};
    Keyframe kfVol1{.time = TimelineTime(120000), .value = 1.5};
    clip.getOrCreateAnimationChannel("volume", 1.0).addOrUpdateKeyframe(kfVol0);
    clip.getOrCreateAnimationChannel("volume", 1.0).addOrUpdateKeyframe(kfVol1);

    // At t = 60,000 ticks (0.5s):
    // position_x = 50.0 (interpolated)
    // position_y = 75.0 (base fallback)
    // opacity = 0.6 (interpolated)
    // volume = 1.0 (interpolated)
    auto midTrans = AnimationTransformResolver::resolveTransformAtTime(clip, TimelineTime(60000));
    TEST_ASSERT(std::abs(midTrans.positionX - 50.0) < 1e-3);
    TEST_ASSERT(midTrans.positionY == 75.0);
    TEST_ASSERT(midTrans.scaleX == 1.2);
    TEST_ASSERT(std::abs(AnimationTransformResolver::resolveOpacityAtTime(clip, TimelineTime(60000)) - 0.6) < 1e-3);
    TEST_ASSERT(std::abs(AnimationTransformResolver::resolveVolumeAtTime(clip, TimelineTime(60000)) - 1.0) < 1e-3);

    // Clamping checks
    Keyframe kfOpNeg{.time = TimelineTime(240000), .value = -2.0};
    clip.getOrCreateAnimationChannel("opacity", 1.0).addOrUpdateKeyframe(kfOpNeg);
    TEST_ASSERT(AnimationTransformResolver::resolveOpacityAtTime(clip, TimelineTime(240000)) == 0.0);

    Keyframe kfVolHigh{.time = TimelineTime(240000), .value = 5.0};
    clip.getOrCreateAnimationChannel("volume", 1.0).addOrUpdateKeyframe(kfVolHigh);
    TEST_ASSERT(AnimationTransformResolver::resolveVolumeAtTime(clip, TimelineTime(240000)) == 2.0);

    std::cout << "[PASS] runAnimationTransformResolverTests" << std::endl;
}

void runPreviewOverlayManagerTests() {
    using namespace catchim::render;

    PreviewOverlayManager manager;

    // 1. Register definitions
    OverlayDefinition defGrid{ .id = "grid", .label = "Grid Lines", .defaultVisible = true };
    OverlayDefinition defSafe{ .id = "safe_zone", .label = "Safe Zone Guide", .defaultVisible = false };
    OverlayDefinition defHisto{ .id = "histogram", .label = "Color Histogram", .defaultVisible = true };

    manager.registerDefinition(defGrid);
    manager.registerDefinition(defSafe);
    manager.registerDefinition(defHisto);

    TEST_ASSERT(manager.definitions().size() == 3);

    // 2. Check default visibility
    TEST_ASSERT(manager.isOverlayVisible("grid") == true);
    TEST_ASSERT(manager.isOverlayVisible("safe_zone") == false);
    TEST_ASSERT(manager.isOverlayVisible("histogram") == true);
    TEST_ASSERT(manager.isOverlayVisible("unknown") == true); // default fallback

    // 3. Set visibility
    manager.setOverlayVisible("safe_zone", true);
    manager.setOverlayVisible("grid", false);
    TEST_ASSERT(manager.isOverlayVisible("safe_zone") == true);
    TEST_ASSERT(manager.isOverlayVisible("grid") == false);

    // 4. Controls
    auto controls = manager.getControls();
    TEST_ASSERT(controls.size() == 3);
    TEST_ASSERT(controls[0].id == "grid" && !controls[0].isVisible);
    TEST_ASSERT(controls[1].id == "safe_zone" && controls[1].isVisible);
    TEST_ASSERT(controls[2].id == "histogram" && controls[2].isVisible);

    // 5. Reset visibility
    manager.resetVisibility();
    TEST_ASSERT(manager.isOverlayVisible("grid") == true);
    TEST_ASSERT(manager.isOverlayVisible("safe_zone") == false);

    // 6. Merge sources
    OverlaySourceResult src1{
        .definitions = { defGrid, defSafe },
        .instances = {
            OverlayInstance{ .id = "inst1", .mount = OverlayMount{ .kind = OverlayMountKind::Hud } },
            OverlayInstance{ .id = "inst2", .mount = OverlayMount{ .kind = OverlayMountKind::Scene } }
        }
    };
    OverlaySourceResult src2{
        .definitions = { defGrid, defHisto }, // defGrid is duplicate
        .instances = {
            OverlayInstance{ .id = "inst3", .mount = OverlayMount{ .kind = OverlayMountKind::Viewport } }
        }
    };

    auto merged = PreviewOverlayManager::mergeSources({ src1, src2 });
    TEST_ASSERT(merged.definitions.size() == 3); // grid, safe_zone, histogram
    TEST_ASSERT(merged.instances.size() == 3);    // inst1, inst2, inst3

    std::cout << "[PASS] runPreviewOverlayManagerTests" << std::endl;
}

void runInteractionCancellationRegistryTests() {
    using namespace catchim::editor;

    InteractionCancellationRegistry registry;

    // 1. Initial state: cancel returns false
    TEST_ASSERT(registry.activeCancellerCount() == 0);
    TEST_ASSERT(registry.cancelInteraction() == false);

    // 2. Register single canceller
    bool called1 = false;
    auto token1 = registry.registerCanceller([&]() { called1 = true; });
    TEST_ASSERT(token1 != 0);
    TEST_ASSERT(registry.activeCancellerCount() == 1);

    TEST_ASSERT(registry.cancelInteraction() == true);
    TEST_ASSERT(called1 == true);
    TEST_ASSERT(registry.activeCancellerCount() == 0);
    TEST_ASSERT(registry.cancelInteraction() == false);

    // 3. Register multiple, unregister one
    int callCountA = 0;
    int callCountB = 0;
    int callCountC = 0;
    auto tokA = registry.registerCanceller([&]() { callCountA++; });
    auto tokB = registry.registerCanceller([&]() { callCountB++; });
    auto tokC = registry.registerCanceller([&]() { callCountC++; });
    TEST_ASSERT(tokA != 0 && tokB != 0 && tokC != 0);
    TEST_ASSERT(registry.activeCancellerCount() == 3);

    TEST_ASSERT(registry.unregisterCanceller(tokB) == true);
    TEST_ASSERT(registry.unregisterCanceller(tokB) == false); // already unregistered
    TEST_ASSERT(registry.activeCancellerCount() == 2);

    TEST_ASSERT(registry.cancelAll() == true);
    TEST_ASSERT(callCountA == 1);
    TEST_ASSERT(callCountB == 0);
    TEST_ASSERT(callCountC == 1);
    TEST_ASSERT(registry.activeCancellerCount() == 0);

    // 4. ScopedRegistration RAII
    bool scopedCalled = false;
    {
        auto scoped = registry.createScopedRegistration([&]() { scopedCalled = true; });
        TEST_ASSERT(registry.activeCancellerCount() == 1);
        // Exiting block unregisters automatically
    }
    TEST_ASSERT(registry.activeCancellerCount() == 0);
    TEST_ASSERT(registry.cancelInteraction() == false);
    TEST_ASSERT(scopedCalled == false);

    // 5. Exception safety
    bool afterThrowCalled = false;
    registry.registerCanceller([]() { throw std::runtime_error("Simulated cancel failure"); });
    registry.registerCanceller([&]() { afterThrowCalled = true; });
    TEST_ASSERT(registry.activeCancellerCount() == 2);

    TEST_ASSERT(registry.cancelInteraction() == true);
    TEST_ASSERT(afterThrowCalled == true);
    TEST_ASSERT(registry.activeCancellerCount() == 0);

    std::cout << "[PASS] runInteractionCancellationRegistryTests" << std::endl;
}

void runPanelLayoutManagerTests() {
    using namespace catchim::editor;

    PanelLayoutManager manager;

    // 1. Check default sizes
    const auto& def = manager.sizes();
    TEST_ASSERT(def.tools == 25.0);
    TEST_ASSERT(def.preview == 50.0);
    TEST_ASSERT(def.properties == 25.0);
    TEST_ASSERT(def.mainContent == 50.0);
    TEST_ASSERT(def.timeline == 50.0);

    // 2. Set individual panel and listener notification
    int notifyCount = 0;
    manager.addListener([&](const PanelSizes& s) {
        notifyCount++;
        TEST_ASSERT(s.tools == 30.0);
    });

    manager.setPanel(PanelId::Tools, 30.0);
    TEST_ASSERT(manager.getPanel(PanelId::Tools) == 30.0);
    TEST_ASSERT(notifyCount == 1);

    // 3. Set all panels and reset
    manager.clearListeners();
    PanelSizes custom{ .tools = 20.0, .preview = 60.0, .properties = 20.0, .mainContent = 40.0, .timeline = 60.0 };
    manager.setPanels(custom);
    TEST_ASSERT(manager.sizes() == custom);

    manager.resetPanels();
    TEST_ASSERT(manager.sizes() == PanelLayoutManager::defaultSizes());

    // 4. JSON Serialization & Deserialization
    manager.setPanel(PanelId::Preview, 55.0);
    auto json = manager.toJson();
    TEST_ASSERT(json.contains("version") && json["version"] == 2);
    TEST_ASSERT(json.contains("panels"));
    TEST_ASSERT(json["panels"]["preview"] == 55.0);

    PanelLayoutManager manager2;
    TEST_ASSERT(manager2.fromJson(json) == true);
    TEST_ASSERT(manager2.getPanel(PanelId::Preview) == 55.0);

    // 5. Migration from v1 legacy schema
    nlohmann::json legacyJson = {
        {"toolsPanel", 18.0},
        {"previewPanel", 62.0},
        {"propertiesPanel", 20.0},
        {"timeline", 45.0}
    };
    auto migrated = PanelLayoutManager::migrateJson(legacyJson);
    TEST_ASSERT(migrated.tools == 18.0);
    TEST_ASSERT(migrated.preview == 62.0);
    TEST_ASSERT(migrated.properties == 20.0);
    TEST_ASSERT(migrated.mainContent == 50.0); // default fallback
    TEST_ASSERT(migrated.timeline == 45.0);

    // 6. Clamping helper
    TEST_ASSERT(PanelLayoutManager::clampPanelSize(2.0, 5.0, 95.0) == 5.0);
    TEST_ASSERT(PanelLayoutManager::clampPanelSize(98.0, 5.0, 95.0) == 95.0);
    TEST_ASSERT(PanelLayoutManager::clampPanelSize(45.0, 5.0, 95.0) == 45.0);

    // 7. PanelId string conversion
    TEST_ASSERT(panelIdToString(PanelId::Tools) == "tools");
    TEST_ASSERT(panelIdFromString("tools") == PanelId::Tools);
    TEST_ASSERT(panelIdFromString("previewPanel") == PanelId::Preview);
    TEST_ASSERT(!panelIdFromString("invalid").has_value());

    std::cout << "[PASS] runPanelLayoutManagerTests" << std::endl;
}

void runMediaThumbnailEngineTests() {
    using namespace catchim;
    using namespace catchim::media;
    using namespace catchim::render;
    using namespace catchim::core;

    // 1. calculateThumbnailSize (max 1280x720, aspect ratio preserving)
    // 16:9 standard: 1920x1080 -> 1280x720
    auto s1 = MediaThumbnailEngine::calculateThumbnailSize(1920, 1080);
    TEST_ASSERT(s1.width == 1280 && s1.height == 720);

    // 2:1 widescreen: 4000x2000 -> 1280x640
    auto s2 = MediaThumbnailEngine::calculateThumbnailSize(4000, 2000);
    TEST_ASSERT(s2.width == 1280 && s2.height == 640);

    // Vertical video (9:16 approx): 800x1200 -> height clamped to 720, width = round(720 * (800/1200)) = 480
    auto s3 = MediaThumbnailEngine::calculateThumbnailSize(800, 1200);
    TEST_ASSERT(s3.width == 480 && s3.height == 720);

    // Smaller than limits: 640x360 -> untouched
    auto s4 = MediaThumbnailEngine::calculateThumbnailSize(640, 360);
    TEST_ASSERT(s4.width == 640 && s4.height == 360);

    // Degenerate inputs
    auto s0 = MediaThumbnailEngine::calculateThumbnailSize(0, 0);
    TEST_ASSERT(s0.width == 0 && s0.height == 0);

    // 2. supportsAudio
    TEST_ASSERT(MediaThumbnailEngine::supportsAudio(MediaType::Video) == true);
    TEST_ASSERT(MediaThumbnailEngine::supportsAudio(MediaType::Audio) == true);
    TEST_ASSERT(MediaThumbnailEngine::supportsAudio(MediaType::Image) == false);

    MediaAsset vidAsset(core::MediaId("v1"), "test.mp4", MediaType::Video);
    MediaAsset imgAsset(core::MediaId("i1"), "test.png", MediaType::Image);
    TEST_ASSERT(MediaThumbnailEngine::supportsAudio(&vidAsset) == true);
    TEST_ASSERT(MediaThumbnailEngine::supportsAudio(&imgAsset) == false);
    TEST_ASSERT(MediaThumbnailEngine::supportsAudio(nullptr) == false);

    // 3. MIME type detection
    TEST_ASSERT(MediaThumbnailEngine::detectMediaTypeFromMime("image/png") == MediaType::Image);
    TEST_ASSERT(MediaThumbnailEngine::detectMediaTypeFromMime("video/mp4") == MediaType::Video);
    TEST_ASSERT(MediaThumbnailEngine::detectMediaTypeFromMime("audio/mpeg") == MediaType::Audio);
    TEST_ASSERT(!MediaThumbnailEngine::detectMediaTypeFromMime("application/json").has_value());

    // 4. Path / Extension detection
    TEST_ASSERT(MediaThumbnailEngine::detectMediaTypeFromPath("assets/photo.JPG") == MediaType::Image);
    TEST_ASSERT(MediaThumbnailEngine::detectMediaTypeFromPath("assets/movie.mkv") == MediaType::Video);
    TEST_ASSERT(MediaThumbnailEngine::detectMediaTypeFromPath("assets/song.flac") == MediaType::Audio);
    TEST_ASSERT(!MediaThumbnailEngine::detectMediaTypeFromPath("assets/doc.pdf").has_value());

    // General detectMediaType
    TEST_ASSERT(MediaThumbnailEngine::detectMediaType("video/webm") == MediaType::Video);
    TEST_ASSERT(MediaThumbnailEngine::detectMediaType("clip.mov") == MediaType::Video);

    // 5. createThumbnailSurface with bilinear resampling
    render::RenderSurface bigSurface(1920, 1080);
    bigSurface.fill(render::RenderSurface::makeRgba(100, 150, 200, 255));

    auto thumbSurface = MediaThumbnailEngine::createThumbnailSurface(bigSurface);
    TEST_ASSERT(thumbSurface.width() == 1280);
    TEST_ASSERT(thumbSurface.height() == 720);

    // Verify sampled pixel color
    uint8_t r, g, b, a;
    render::RenderSurface::unpackRgba(thumbSurface.getPixel(640, 360), r, g, b, a);
    TEST_ASSERT(r == 100 && g == 150 && b == 200 && a == 255);

    std::cout << "[PASS] runMediaThumbnailEngineTests" << std::endl;
}

void runRenderingParamsResolverTests() {
    using namespace catchim::render;

    // 1. Build transform from params
    std::unordered_map<std::string, double> params = {
        {"transform.scaleX", 1.5},
        {"transform.scaleY", 2.0},
        {"transform.positionX", 100.0},
        {"transform.positionY", -50.0},
        {"transform.rotate", 45.0},
        {"opacity", 0.85}
    };
    auto t = RenderingParamsResolver::buildTransformFromParams(params);
    TEST_ASSERT(t.scaleX == 1.5);
    TEST_ASSERT(t.scaleY == 2.0);
    TEST_ASSERT(t.position.x == 100.0);
    TEST_ASSERT(t.position.y == -50.0);
    TEST_ASSERT(t.rotate == 45.0);

    // Empty params fall back to defaults
    std::unordered_map<std::string, double> emptyParams;
    auto defT = RenderingParamsResolver::buildTransformFromParams(emptyParams);
    TEST_ASSERT(defT.scaleX == 1.0);
    TEST_ASSERT(defT.scaleY == 1.0);
    TEST_ASSERT(defT.position.x == 0.0);
    TEST_ASSERT(defT.position.y == 0.0);
    TEST_ASSERT(defT.rotate == 0.0);

    // 2. Read opacity
    TEST_ASSERT(RenderingParamsResolver::readOpacityFromParams(params) == 0.85);
    TEST_ASSERT(RenderingParamsResolver::readOpacityFromParams(emptyParams, 1.0) == 1.0);

    // 3. Read blend mode and all 17 blend modes validation
    const auto& allModes = RenderingParamsResolver::allBlendModes();
    TEST_ASSERT(allModes.size() == 17);
    for (const auto& m : allModes) {
        TEST_ASSERT(RenderingParamsResolver::isBlendMode(m) == true);
        auto parsed = RenderingParamsResolver::blendModeFromString(m);
        TEST_ASSERT(parsed.has_value());
        TEST_ASSERT(RenderingParamsResolver::blendModeToString(*parsed) == m);
    }
    TEST_ASSERT(RenderingParamsResolver::isBlendMode("invalid-mode") == false);

    std::unordered_map<std::string, std::string> strParams = { {"blendMode", "overlay"} };
    TEST_ASSERT(RenderingParamsResolver::readBlendModeFromParams(strParams) == "overlay");
    std::unordered_map<std::string, std::string> invalidStrParams = { {"blendMode", "unknown"} };
    TEST_ASSERT(RenderingParamsResolver::readBlendModeFromParams(invalidStrParams, "normal") == "normal");

    std::cout << "[PASS] runRenderingParamsResolverTests" << std::endl;
}

void runTrackDefaultsTests() {
    using namespace catchim::editor;

    // 1. Default track names
    TEST_ASSERT(TrackDefaults::defaultVideoTrackName() == "Video track");
    TEST_ASSERT(TrackDefaults::defaultTextTrackName() == "Text track");
    TEST_ASSERT(TrackDefaults::defaultAudioTrackName() == "Audio track");
    TEST_ASSERT(TrackDefaults::defaultGraphicTrackName() == "Graphic track");
    TEST_ASSERT(TrackDefaults::defaultEffectTrackName() == "Effect track");

    TEST_ASSERT(TrackDefaults::getDefaultTrackName("video") == "Video track");
    TEST_ASSERT(TrackDefaults::getDefaultTrackName("audio") == "Audio track");
    TEST_ASSERT(TrackDefaults::getDefaultTrackName("text") == "Text track");
    TEST_ASSERT(TrackDefaults::getDefaultTrackName("graphic") == "Graphic track");
    TEST_ASSERT(TrackDefaults::getDefaultTrackName("effect") == "Effect track");
    TEST_ASSERT(TrackDefaults::getDefaultTrackName("unknown") == "Track");

    // 2. dB limits and conversions
    static_assert(TrackDefaults::VOLUME_DB_MIN == -60.0);
    static_assert(TrackDefaults::VOLUME_DB_MAX == 20.0);

    // 1.0 linear = 0 dB
    TEST_ASSERT(std::abs(TrackDefaults::linearToDb(1.0) - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(TrackDefaults::dbToLinear(0.0) - 1.0) < 1e-4);

    // Roundtrip test
    double testDb = -12.0;
    double linear = TrackDefaults::dbToLinear(testDb);
    double roundtripDb = TrackDefaults::linearToDb(linear);
    TEST_ASSERT(std::abs(testDb - roundtripDb) < 1e-3);

    // Clamping
    TEST_ASSERT(TrackDefaults::clampDb(-100.0) == -60.0);
    TEST_ASSERT(TrackDefaults::clampDb(50.0) == 20.0);
    TEST_ASSERT(TrackDefaults::clampLinear(-0.5) == 0.0);
    TEST_ASSERT(TrackDefaults::clampLinear(3.5) == 2.0);

    std::cout << "[PASS] runTrackDefaultsTests" << std::endl;
}

void runMathFormattingUtilsTests() {
    using namespace catchim::core;

    // 1. clamp & clampRound
    TEST_ASSERT(MathFormattingUtils::clamp(5.0, 0.0, 10.0) == 5.0);
    TEST_ASSERT(MathFormattingUtils::clamp(-5.0, 0.0, 10.0) == 0.0);
    TEST_ASSERT(MathFormattingUtils::clamp(15.0, 0.0, 10.0) == 10.0);
    TEST_ASSERT(MathFormattingUtils::clampRound(5.6, 0.0, 10.0) == 6);
    TEST_ASSERT(MathFormattingUtils::clampRound(5.4, 0.0, 10.0) == 5);

    // 2. getFractionDigitsForStep & snapToStep
    TEST_ASSERT(MathFormattingUtils::getFractionDigitsForStep(1.0) == 0);
    TEST_ASSERT(MathFormattingUtils::getFractionDigitsForStep(0.1) == 1);
    TEST_ASSERT(MathFormattingUtils::getFractionDigitsForStep(0.05) == 2);
    TEST_ASSERT(MathFormattingUtils::getFractionDigitsForStep(0.001) == 3);

    TEST_ASSERT(std::abs(MathFormattingUtils::snapToStep(1.234, 0.1) - 1.2) < 1e-4);
    TEST_ASSERT(std::abs(MathFormattingUtils::snapToStep(1.26, 0.1) - 1.3) < 1e-4);
    TEST_ASSERT(std::abs(MathFormattingUtils::snapToStep(1.23, 0.05) - 1.25) < 1e-4);

    // 3. isNearlyEqual
    TEST_ASSERT(MathFormattingUtils::isNearlyEqual(1.00001, 1.00002, 0.001) == true);
    TEST_ASSERT(MathFormattingUtils::isNearlyEqual(1.0, 1.1, 0.001) == false);

    // 4. formatNumberForDisplay (trailing zeroes trimmed)
    TEST_ASSERT(MathFormattingUtils::formatNumberForDisplay(12.340000, std::nullopt, 0, 6) == "12.34");
    TEST_ASSERT(MathFormattingUtils::formatNumberForDisplay(12.000000, std::nullopt, 0, 6) == "12");
    TEST_ASSERT(MathFormattingUtils::formatNumberForDisplay(12.000000, std::nullopt, 2, 6) == "12.00");
    TEST_ASSERT(MathFormattingUtils::formatNumberForDisplay(0.0, std::nullopt, 0, 6) == "0");

    // 5. dimensionToAspectRatio (GCD aspect ratio)
    TEST_ASSERT(MathFormattingUtils::dimensionToAspectRatio(1920, 1080) == "16:9");
    TEST_ASSERT(MathFormattingUtils::dimensionToAspectRatio(1080, 1920) == "9:16");
    TEST_ASSERT(MathFormattingUtils::dimensionToAspectRatio(1440, 1080) == "4:3");
    TEST_ASSERT(MathFormattingUtils::dimensionToAspectRatio(1080, 1080) == "1:1");
    TEST_ASSERT(MathFormattingUtils::dimensionToAspectRatio(2560, 1440) == "16:9");
    TEST_ASSERT(MathFormattingUtils::dimensionToAspectRatio(3840, 2160) == "16:9");

    std::cout << "[PASS] runMathFormattingUtilsTests" << std::endl;
}

void runExportDefaultsTests() {
    using namespace catchim::exporting;

    // 1. Default export options
    const auto& opts = ExportDefaults::defaultExportOptions();
    TEST_ASSERT(opts.format == "mp4");
    TEST_ASSERT(opts.quality == "high");
    TEST_ASSERT(opts.resolution == "source");
    TEST_ASSERT(opts.includeAudio == true);

    // 2. MIME types
    TEST_ASSERT(ExportDefaults::mimeTypeMp4() == "video/mp4");
    TEST_ASSERT(ExportDefaults::mimeTypeWebm() == "video/webm");
    TEST_ASSERT(ExportDefaults::getMimeTypeForFormat("mp4") == "video/mp4");
    TEST_ASSERT(ExportDefaults::getMimeTypeForFormat("webm") == "video/webm");
    TEST_ASSERT(ExportDefaults::getMimeTypeForFormat("other") == "video/mp4");

    // 3. Supported catalogs
    const auto& formats = ExportDefaults::supportedFormats();
    TEST_ASSERT(formats.size() == 2);
    TEST_ASSERT(formats[0] == "mp4" && formats[1] == "webm");

    const auto& qualities = ExportDefaults::qualityPresets();
    TEST_ASSERT(qualities.size() == 4);
    TEST_ASSERT(qualities[0] == "draft" && qualities[2] == "high");

    const auto& resolutions = ExportDefaults::resolutionPresets();
    TEST_ASSERT(resolutions.size() == 5);
    TEST_ASSERT(resolutions[0] == "source" && resolutions[1] == "4k");

    std::cout << "[PASS] runExportDefaultsTests" << std::endl;
}

void runBookmarkEngineTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    std::vector<Bookmark> bookmarks;

    // 1. Initial state
    TEST_ASSERT(BookmarkEngine::findBookmarkIndex(bookmarks, TimelineTime(120000)) == -1);
    TEST_ASSERT(!BookmarkEngine::isBookmarkAtTime(bookmarks, TimelineTime(120000)));

    // 2. Toggle in array (adds if missing)
    bookmarks = BookmarkEngine::toggleBookmarkInArray(bookmarks, TimelineTime(120000)); // 1.0s
    bookmarks = BookmarkEngine::toggleBookmarkInArray(bookmarks, TimelineTime(240000)); // 2.0s
    bookmarks = BookmarkEngine::toggleBookmarkInArray(bookmarks, TimelineTime(60000));  // 0.5s
    TEST_ASSERT(bookmarks.size() == 3);
    // Sorted by time: 0.5s, 1.0s, 2.0s
    TEST_ASSERT(bookmarks[0].time == TimelineTime(60000));
    TEST_ASSERT(bookmarks[1].time == TimelineTime(120000));
    TEST_ASSERT(bookmarks[2].time == TimelineTime(240000));

    // Toggle existing removes it
    bookmarks = BookmarkEngine::toggleBookmarkInArray(bookmarks, TimelineTime(120000));
    TEST_ASSERT(bookmarks.size() == 2);
    TEST_ASSERT(!BookmarkEngine::isBookmarkAtTime(bookmarks, TimelineTime(120000)));

    // 3. Update bookmark
    bookmarks = BookmarkEngine::updateBookmarkInArray(
        bookmarks,
        TimelineTime(60000),
        "Verse 1",
        "#ff0000",
        TimelineTime(30000)
    );
    const auto* bm0 = BookmarkEngine::getBookmarkAtTime(bookmarks, TimelineTime(60000));
    TEST_ASSERT(bm0 != nullptr);
    TEST_ASSERT(bm0->note == "Verse 1");
    TEST_ASSERT(bm0->color == "#ff0000");
    TEST_ASSERT(bm0->duration.has_value() && *bm0->duration == TimelineTime(30000));

    // 4. Move bookmark
    bookmarks = BookmarkEngine::moveBookmarkInArray(bookmarks, TimelineTime(60000), TimelineTime(300000));
    TEST_ASSERT(!BookmarkEngine::isBookmarkAtTime(bookmarks, TimelineTime(60000)));
    TEST_ASSERT(BookmarkEngine::isBookmarkAtTime(bookmarks, TimelineTime(300000)));

    // 5. Active bookmarks query (start=300000, duration=30000 -> active in [300000, 330000])
    auto activeMid = BookmarkEngine::getBookmarksActiveAtTime(bookmarks, TimelineTime(315000));
    TEST_ASSERT(activeMid.size() == 1);
    TEST_ASSERT(activeMid[0].note == "Verse 1");
    auto activeOutside = BookmarkEngine::getBookmarksActiveAtTime(bookmarks, TimelineTime(350000));
    TEST_ASSERT(activeOutside.empty());

    // 6. Snap points with exclusion
    auto snaps = BookmarkEngine::getBookmarkSnapPoints(bookmarks, TimelineTime(240000));
    TEST_ASSERT(snaps.size() == 1);
    TEST_ASSERT(snaps[0].time == TimelineTime(300000));
    TEST_ASSERT(snaps[0].type == "bookmark");

    // Remove
    bookmarks = BookmarkEngine::removeBookmarkFromArray(bookmarks, TimelineTime(300000));
    TEST_ASSERT(bookmarks.size() == 1);

    std::cout << "[PASS] runBookmarkEngineTests" << std::endl;
}

void runTimelineUiStoreTests() {
    using namespace catchim::editor;

    TimelineUiStore store;

    // 1. Defaults
    TEST_ASSERT(store.isSnappingEnabled() == true);
    TEST_ASSERT(store.isRippleEditingEnabled() == false);
    TEST_ASSERT(store.expandedElementIds().empty());

    // 2. Toggles & listeners
    int listenerCalls = 0;
    store.addChangeListener([&]() { listenerCalls++; });

    store.toggleSnapping();
    TEST_ASSERT(store.isSnappingEnabled() == false);
    TEST_ASSERT(listenerCalls == 1);

    store.setRippleEditingEnabled(true);
    TEST_ASSERT(store.isRippleEditingEnabled() == true);
    TEST_ASSERT(listenerCalls == 2);

    // 3. Expanded elements
    store.setElementExpanded("elem_1", true);
    TEST_ASSERT(store.isElementExpanded("elem_1") == true);
    TEST_ASSERT(store.isElementExpanded("elem_2") == false);

    store.toggleElementExpanded("elem_1");
    TEST_ASSERT(store.isElementExpanded("elem_1") == false);

    store.toggleElementExpanded("elem_2");
    TEST_ASSERT(store.isElementExpanded("elem_2") == true);
    store.clearExpandedElements();
    TEST_ASSERT(store.expandedElementIds().empty());

    // 4. JSON serialization & hydration
    store.setSnappingEnabled(false);
    store.setRippleEditingEnabled(true);
    auto j = store.toJson();
    TEST_ASSERT(j["name"] == "timeline-store");
    TEST_ASSERT(j["snappingEnabled"] == false);
    TEST_ASSERT(j["rippleEditingEnabled"] == true);

    TimelineUiStore store2;
    TEST_ASSERT(store2.fromJson(j) == true);
    TEST_ASSERT(store2.isSnappingEnabled() == false);
    TEST_ASSERT(store2.isRippleEditingEnabled() == true);

    std::cout << "[PASS] runTimelineUiStoreTests" << std::endl;
}

void runTrackCompatibilityEngineTests() {
    using namespace catchim::editor;

    // 1. getTrackTypeForElementType
    TEST_ASSERT(TrackCompatibilityEngine::getTrackTypeForElementType("audio") == "audio");
    TEST_ASSERT(TrackCompatibilityEngine::getTrackTypeForElementType("text") == "text");
    TEST_ASSERT(TrackCompatibilityEngine::getTrackTypeForElementType("sticker") == "graphic");
    TEST_ASSERT(TrackCompatibilityEngine::getTrackTypeForElementType("graphic") == "graphic");
    TEST_ASSERT(TrackCompatibilityEngine::getTrackTypeForElementType("effect") == "effect");
    TEST_ASSERT(TrackCompatibilityEngine::getTrackTypeForElementType("video") == "video");
    TEST_ASSERT(TrackCompatibilityEngine::getTrackTypeForElementType("image") == "video");
    TEST_ASSERT(TrackCompatibilityEngine::getTrackTypeForElementType("unknown").empty());

    // 2. canElementGoOnTrack
    TEST_ASSERT(TrackCompatibilityEngine::canElementGoOnTrack("audio", "audio") == true);
    TEST_ASSERT(TrackCompatibilityEngine::canElementGoOnTrack("audio", "video") == false);
    TEST_ASSERT(TrackCompatibilityEngine::canElementGoOnTrack("image", "video") == true);
    TEST_ASSERT(TrackCompatibilityEngine::canElementGoOnTrack("sticker", "graphic") == true);

    // 3. validateElementTrackCompatibility
    auto validRes = TrackCompatibilityEngine::validateElementTrackCompatibility("text", "text");
    TEST_ASSERT(validRes.isValid == true);
    TEST_ASSERT(validRes.errorMessage.empty());

    auto invalidRes = TrackCompatibilityEngine::validateElementTrackCompatibility("audio", "video");
    TEST_ASSERT(invalidRes.isValid == false);
    TEST_ASSERT(invalidRes.errorMessage == "audio elements cannot be placed on video tracks");

    std::cout << "[PASS] runTrackCompatibilityEngineTests" << std::endl;
}

void runTrackInsertResolverTests() {
    using namespace catchim::editor;

    // Overlay tracks: 2, Audio tracks: 1 -> Total tracks = 2 + 1 + 1 = 4.
    // Index 0, 1: overlay tracks. Index 2: main track. Index 3: audio track.
    size_t overlayCount = 2;
    size_t audioCount = 1;

    // 1. getDefaultInsertIndexForTrack
    // Audio track inserts at end: overlayCount + 1 + audioCount = 2 + 1 + 1 = 4
    TEST_ASSERT(TrackInsertResolver::getDefaultInsertIndexForTrack(overlayCount, audioCount, "audio") == 4);
    // Effect track inserts at 0
    TEST_ASSERT(TrackInsertResolver::getDefaultInsertIndexForTrack(overlayCount, audioCount, "effect") == 0);
    // Video/Graphic/Text inserts above main track: overlayCount = 2
    TEST_ASSERT(TrackInsertResolver::getDefaultInsertIndexForTrack(overlayCount, audioCount, "video") == 2);
    TEST_ASSERT(TrackInsertResolver::getDefaultInsertIndexForTrack(overlayCount, audioCount, "text") == 2);

    // 2. getHighestInsertIndexForTrack
    // Audio highest is right below main track: overlayCount + 1 = 3
    TEST_ASSERT(TrackInsertResolver::getHighestInsertIndexForTrack(overlayCount, "audio") == 3);
    TEST_ASSERT(TrackInsertResolver::getHighestInsertIndexForTrack(overlayCount, "video") == 0);

    // 3. resolvePreferredNewTrackPlacement
    // Audio track with preferred index <= mainTrackIndex (2) is constrained to mainTrackIndex + 1 (3)
    auto resAudio = TrackInsertResolver::resolvePreferredNewTrackPlacement(overlayCount, audioCount, "audio", 1, "above");
    TEST_ASSERT(resAudio.insertIndex == 3);
    TEST_ASSERT(resAudio.insertPosition == "below");

    // Video track with insertIndex > mainTrackIndex (2) is constrained to mainTrackIndex (2)
    auto resVideo = TrackInsertResolver::resolvePreferredNewTrackPlacement(overlayCount, audioCount, "video", 3, "below");
    TEST_ASSERT(resVideo.insertIndex == 2);
    TEST_ASSERT(resVideo.insertPosition == "above");

    // Normal placement
    auto resNorm = TrackInsertResolver::resolvePreferredNewTrackPlacement(overlayCount, audioCount, "video", 0, "below");
    TEST_ASSERT(resNorm.insertIndex == 1);
    TEST_ASSERT(resNorm.insertPosition == "below");

    std::cout << "[PASS] runTrackInsertResolverTests" << std::endl;
}

void runGroupMoveSnapEngineTests() {
    using namespace catchim;
    using namespace catchim::core;
    using namespace catchim::editor;

    Timeline timeline;
    auto& overlayTrack = timeline.addTrack(TrackType::Video, "Overlay 1");
    auto& audioTrack = timeline.addTrack(TrackType::Audio, "Audio 1");

    // 1. getDisplayTrackPlacements
    auto placements = GroupMoveSnapEngine::getDisplayTrackPlacements(timeline);
    TEST_ASSERT(placements.size() == 3);
    TEST_ASSERT(placements[0].section == GroupTrackSection::Overlay);
    TEST_ASSERT(placements[0].displayIndex == 0);
    TEST_ASSERT(placements[1].section == GroupTrackSection::Main);
    TEST_ASSERT(placements[1].displayIndex == 1);
    TEST_ASSERT(placements[2].section == GroupTrackSection::Audio);
    TEST_ASSERT(placements[2].displayIndex == 2);

    // 2. getTrackPlacementById & getTrackPlacementByDisplayIndex
    auto mainPlacementOpt = GroupMoveSnapEngine::getTrackPlacementById(timeline, timeline.mainTrack().id());
    TEST_ASSERT(mainPlacementOpt.has_value());
    TEST_ASSERT(mainPlacementOpt->section == GroupTrackSection::Main);
    TEST_ASSERT(mainPlacementOpt->displayIndex == 1);

    auto disp0Opt = GroupMoveSnapEngine::getTrackPlacementByDisplayIndex(timeline, 0);
    TEST_ASSERT(disp0Opt.has_value());
    TEST_ASSERT(disp0Opt->trackId == overlayTrack.id());

    auto dispInv = GroupMoveSnapEngine::getTrackPlacementByDisplayIndex(timeline, 99);
    TEST_ASSERT(!dispInv.has_value());

    // 3. Add clips for group move
    ClipId clip1Id = ClipId::generate();
    Clip clip1(clip1Id, ClipType::Video, "Clip 1", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(3.0));
    overlayTrack.insertClip(clip1);

    ClipId clip2Id = ClipId::generate();
    Clip clip2(clip2Id, ClipType::Video, "Clip 2 (Anchor)", TimelineTime::fromSeconds(5.0), TimelineTime::fromSeconds(4.0));
    timeline.mainTrack().insertClip(clip2);

    ClipId clip3Id = ClipId::generate();
    Clip clip3(clip3Id, ClipType::Audio, "Clip 3", TimelineTime::fromSeconds(5.0), TimelineTime::fromSeconds(4.0));
    audioTrack.insertClip(clip3);

    // 4. buildMoveGroup
    std::vector<std::pair<TrackId, ClipId>> selected = {
        {timeline.mainTrack().id(), clip2Id},
        {overlayTrack.id(), clip1Id}
    };
    auto groupOpt = GroupMoveSnapEngine::buildMoveGroup(timeline, timeline.mainTrack().id(), clip2Id, selected);
    TEST_ASSERT(groupOpt.has_value());
    TEST_ASSERT(groupOpt->anchor.elementId == clip2Id);
    TEST_ASSERT(groupOpt->members.size() == 2);
    TEST_ASSERT(groupOpt->anchor.timeOffset == TimelineTime::zero());

    // Member clip 1 has timeOffset = 2.0s - 5.0s = -3.0s
    auto m1It = std::find_if(groupOpt->members.begin(), groupOpt->members.end(), [&](const GroupMoveMember& m) {
        return m.elementId == clip1Id;
    });
    TEST_ASSERT(m1It != groupOpt->members.end());
    TEST_ASSERT(m1It->timeOffset == TimelineTime::fromSeconds(-3.0));

    // 5. buildMoveGroupSnapPoints
    auto snapPoints = GroupMoveSnapEngine::buildMoveGroupSnapPoints(*groupOpt, timeline, TimelineTime::fromSeconds(10.0));
    TEST_ASSERT(!snapPoints.empty());

    // 6. snapGroupEdges
    auto snapRes = GroupMoveSnapEngine::snapGroupEdges(*groupOpt, TimelineTime::fromSeconds(12.0), timeline, TimelineTime::fromSeconds(10.0), 1.0, snapPoints);
    TEST_ASSERT(snapRes.snappedAnchorStartTime >= TimelineTime::zero());

    // 7. resolveGroupMove with ExistingTrackTarget
    ExistingTrackTarget exTarget{.anchorTargetTrackId = timeline.mainTrack().id()};
    auto exMoveRes = GroupMoveSnapEngine::resolveGroupMove(*groupOpt, timeline, TimelineTime::fromSeconds(10.0), exTarget);
    TEST_ASSERT(exMoveRes.has_value());
    TEST_ASSERT(exMoveRes->moves.size() == 2);
    TEST_ASSERT(exMoveRes->createTracks.empty());

    // 8. resolveGroupMove with NewTracksTarget
    NewTracksTarget newTarget{
        .anchorInsertIndex = 0,
        .newTrackIds = {TrackId::generate(), TrackId::generate()}
    };
    auto newMoveRes = GroupMoveSnapEngine::resolveGroupMove(*groupOpt, timeline, TimelineTime::fromSeconds(10.0), newTarget);
    TEST_ASSERT(newMoveRes.has_value());
    TEST_ASSERT(newMoveRes->createTracks.size() == 2);
    TEST_ASSERT(newMoveRes->moves.size() == 2);

    std::cout << "[PASS] runGroupMoveSnapEngineTests" << std::endl;
}

void runRippleDiffEngineTests() {
    using namespace catchim;
    using namespace catchim::core;
    using namespace catchim::editor;

    // 1. normalizeIntervals
    std::vector<TimeInterval> rawIntervals = {
        {TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(3.0)},
        {TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(4.0)},
        {TimelineTime::fromSeconds(5.0), TimelineTime::fromSeconds(2.0)}, // invalid
        {TimelineTime::fromSeconds(6.0), TimelineTime::fromSeconds(8.0)}
    };
    auto norm = RippleDiffEngine::normalizeIntervals(rawIntervals);
    TEST_ASSERT(norm.size() == 2);
    TEST_ASSERT(norm[0].startTime == TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(norm[0].endTime == TimelineTime::fromSeconds(4.0));
    TEST_ASSERT(norm[1].startTime == TimelineTime::fromSeconds(6.0));
    TEST_ASSERT(norm[1].endTime == TimelineTime::fromSeconds(8.0));

    // 2. subtractSingleInterval
    TimeInterval src{TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(10.0)};
    std::vector<TimeInterval> overlaps = {
        {TimelineTime::fromSeconds(3.0), TimelineTime::fromSeconds(7.0)}
    };
    auto sub1 = RippleDiffEngine::subtractSingleInterval(src, overlaps);
    TEST_ASSERT(sub1.size() == 2);
    TEST_ASSERT(sub1[0].startTime == TimelineTime::fromSeconds(0.0));
    TEST_ASSERT(sub1[0].endTime == TimelineTime::fromSeconds(3.0));
    TEST_ASSERT(sub1[1].startTime == TimelineTime::fromSeconds(7.0));
    TEST_ASSERT(sub1[1].endTime == TimelineTime::fromSeconds(10.0));

    // 3. subtractIntervalSets
    std::vector<TimeInterval> srcSets = {
        {TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(5.0)},
        {TimelineTime::fromSeconds(10.0), TimelineTime::fromSeconds(15.0)}
    };
    std::vector<TimeInterval> overlapSets = {
        {TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(4.0)},
        {TimelineTime::fromSeconds(12.0), TimelineTime::fromSeconds(13.0)}
    };
    auto subSets = RippleDiffEngine::subtractIntervalSets(srcSets, overlapSets);
    TEST_ASSERT(subSets.size() == 4);

    // 4. computeTrackRippleAdjustments & computeRippleAdjustments
    Timeline beforeTimeline;
    ClipId c1 = ClipId::generate();
    ClipId c2 = ClipId::generate();
    Clip clipA(c1, ClipType::Video, "Clip A", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(5.0));
    Clip clipB(c2, ClipType::Video, "Clip B", TimelineTime::fromSeconds(6.0), TimelineTime::fromSeconds(4.0));
    beforeTimeline.mainTrack().insertClip(clipA);
    beforeTimeline.mainTrack().insertClip(clipB);

    Timeline afterTimeline;
    afterTimeline.mainTrack().insertClip(clipB); // Clip A deleted

    auto adjustments = RippleDiffEngine::computeRippleAdjustments(beforeTimeline, afterTimeline);
    TEST_ASSERT(adjustments.size() == 1);
    TEST_ASSERT(adjustments[0].trackId == beforeTimeline.mainTrack().id());
    TEST_ASSERT(adjustments[0].afterTime == TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(adjustments[0].shiftAmount == TimelineTime::fromSeconds(5.0));

    // 5. applyRippleAdjustments
    Timeline targetTimeline;
    Clip clipTarget(c2, ClipType::Video, "Target", TimelineTime::fromSeconds(6.0), TimelineTime::fromSeconds(4.0));
    targetTimeline.mainTrack().insertClip(clipTarget);

    RippleDiffEngine::applyRippleAdjustments(targetTimeline, adjustments);
    const Clip* shifted = targetTimeline.mainTrack().findClip(c2);
    TEST_ASSERT(shifted != nullptr);
    TEST_ASSERT(shifted->startTime() == TimelineTime::fromSeconds(1.0)); // 6.0 - 5.0 = 1.0

    std::cout << "[PASS] runRippleDiffEngineTests" << std::endl;
}

void runTrackElementUpdateEngineTests() {
    using namespace catchim;
    using namespace catchim::core;
    using namespace catchim::editor;

    Timeline timeline;
    auto& overlay = timeline.addTrack(TrackType::Video, "Overlay A");
    ClipId clipId = ClipId::generate();
    Clip clip(clipId, ClipType::Video, "Element A", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(4.0));
    overlay.insertClip(clip);

    // 1. findTrackInTimeline
    Track* foundTrack = TrackElementUpdateEngine::findTrackInTimeline(timeline, overlay.id());
    TEST_ASSERT(foundTrack != nullptr);
    TEST_ASSERT(foundTrack->id() == overlay.id());

    const Track* foundConst = TrackElementUpdateEngine::findTrackInTimeline(std::as_const(timeline), timeline.mainTrack().id());
    TEST_ASSERT(foundConst != nullptr);

    Track* notFound = TrackElementUpdateEngine::findTrackInTimeline(timeline, TrackId::generate());
    TEST_ASSERT(notFound == nullptr);

    // 2. updateTrackInTimeline
    bool trackUpdated = TrackElementUpdateEngine::updateTrackInTimeline(timeline, overlay.id(), [](Track& trk) {
        trk.setName("Renamed Overlay");
    });
    TEST_ASSERT(trackUpdated);
    TEST_ASSERT(overlay.name() == "Renamed Overlay");

    // 3. updateElementInTrack with predicate
    bool updatedFail = TrackElementUpdateEngine::updateElementInTrack(
        overlay, clipId, [](Clip& c) { c.setName("Ignored"); },
        [](const Clip& c) { return c.duration() > TimelineTime::fromSeconds(10.0); }
    );
    TEST_ASSERT(!updatedFail);

    bool updatedSuccess = TrackElementUpdateEngine::updateElementInTrack(
        overlay, clipId, [](Clip& c) { c.setDuration(TimelineTime::fromSeconds(6.0)); },
        [](const Clip& c) { return c.duration() == TimelineTime::fromSeconds(4.0); }
    );
    TEST_ASSERT(updatedSuccess);
    TEST_ASSERT(overlay.findClip(clipId)->duration() == TimelineTime::fromSeconds(6.0));

    // 4. updateElementInTimeline
    bool timelineElementUpdated = TrackElementUpdateEngine::updateElementInTimeline(
        timeline, overlay.id(), clipId, [](Clip& c) { c.setName("New Element Name"); }
    );
    TEST_ASSERT(timelineElementUpdated);
    TEST_ASSERT(overlay.findClip(clipId)->name() == "New Element Name");

    std::cout << "[PASS] runTrackElementUpdateEngineTests" << std::endl;
}

void runKeybindingMigrationEngineTests() {
    using namespace catchim;
    using namespace catchim::editor;

    // 1. parse & serialize PersistedKeybindingsState
    nlohmann::json validJson = {
        {"keybindings", {{"k", "play"}, {"s", "split"}}},
        {"isCustomized", true}
    };
    auto parsedOpt = KeybindingMigrationEngine::parsePersistedKeybindingsState(validJson);
    TEST_ASSERT(parsedOpt.has_value());
    TEST_ASSERT(parsedOpt->isCustomized == true);
    TEST_ASSERT(parsedOpt->keybindings.at("k") == "play");

    auto serialized = KeybindingMigrationEngine::serializePersistedKeybindingsState(*parsedOpt);
    TEST_ASSERT(serialized["isCustomized"] == true);
    TEST_ASSERT(serialized["keybindings"]["s"] == "split");

    nlohmann::json invalidJson = {{"keybindings", 123}};
    TEST_ASSERT(!KeybindingMigrationEngine::parsePersistedKeybindingsState(invalidJson).has_value());

    // 2. v2ToV3
    nlohmann::json v2State = {
        {"keybindings", {
            {"c", "split-selected"},
            {"q", "split-selected-left"},
            {"w", "split-selected-right"}
        }},
        {"isCustomized", false}
    };
    auto v3State = KeybindingMigrationEngine::v2ToV3(v2State);
    TEST_ASSERT(v3State["keybindings"]["c"] == "split");
    TEST_ASSERT(v3State["keybindings"]["q"] == "split-left");
    TEST_ASSERT(v3State["keybindings"]["w"] == "split-right");

    // 3. v3ToV4
    nlohmann::json v3Input = {
        {"keybindings", {{"ctrl+v", "paste-selected"}}},
        {"isCustomized", false}
    };
    auto v4State = KeybindingMigrationEngine::v3ToV4(v3Input);
    TEST_ASSERT(v4State["keybindings"]["ctrl+v"] == "paste-copied");

    // 4. v4ToV5
    nlohmann::json v4Input = {
        {"keybindings", {{"k", "play"}}},
        {"isCustomized", false}
    };
    auto v5State = KeybindingMigrationEngine::v4ToV5(v4Input);
    TEST_ASSERT(v5State["keybindings"]["escape"] == "deselect-all");

    // 5. v5ToV6
    auto v6State = KeybindingMigrationEngine::v5ToV6(v5State);
    TEST_ASSERT(v6State["keybindings"]["escape"] == "cancel-interaction");

    // 6. v6ToV7
    nlohmann::json v6Input = {
        {"keybindings", {{"b", "split-element"}}},
        {"isCustomized", false}
    };
    auto v7State = KeybindingMigrationEngine::v6ToV7(v6Input);
    TEST_ASSERT(v7State["keybindings"]["b"] == "split");

    // 7. runMigrations full pipeline
    nlohmann::json legacyState = {
        {"keybindings", {
            {"c", "split-selected"},
            {"ctrl+v", "paste-selected"},
            {"b", "split-element"}
        }},
        {"isCustomized", false}
    };
    auto fullMigrated = KeybindingMigrationEngine::runMigrations(legacyState, 2);
    TEST_ASSERT(fullMigrated["keybindings"]["c"] == "split");
    TEST_ASSERT(fullMigrated["keybindings"]["ctrl+v"] == "paste-copied");
    TEST_ASSERT(fullMigrated["keybindings"]["escape"] == "cancel-interaction");
    TEST_ASSERT(fullMigrated["keybindings"]["b"] == "split");

    std::cout << "[PASS] runKeybindingMigrationEngineTests" << std::endl;
}

void runAnimationKeyframeQueryEngineTests() {
    using namespace catchim;
    using namespace catchim::core;
    using namespace catchim::editor;

    // 1. getAllAnimationPropertyPaths & isRecognizedPropertyPath
    const auto& paths = AnimationKeyframeQueryEngine::getAllAnimationPropertyPaths();
    TEST_ASSERT(paths.size() == 14);
    TEST_ASSERT(AnimationKeyframeQueryEngine::isRecognizedPropertyPath("transform.scaleX"));
    TEST_ASSERT(AnimationKeyframeQueryEngine::isRecognizedPropertyPath("opacity"));
    TEST_ASSERT(!AnimationKeyframeQueryEngine::isRecognizedPropertyPath("nonexistent.property"));

    // 2. getPropertyPathsForGroup
    auto scaleGroup = AnimationKeyframeQueryEngine::getPropertyPathsForGroup("transform.scale");
    TEST_ASSERT(scaleGroup.size() == 2);
    TEST_ASSERT(scaleGroup[0] == "transform.scaleX");
    TEST_ASSERT(scaleGroup[1] == "transform.scaleY");

    // 3. Channels setup
    std::unordered_map<std::string, AnimationChannel> channels;
    AnimationChannel chScaleX("transform.scaleX", 1.0);
    Keyframe kf1{.time = TimelineTime::fromSeconds(2.0), .value = 1.5, .interpolation = KeyframeInterpolation::Linear};
    chScaleX.addOrUpdateKeyframe(kf1);

    AnimationChannel chScaleY("transform.scaleY", 1.0);
    Keyframe kf2{.time = TimelineTime::fromSeconds(2.0), .value = 1.5, .interpolation = KeyframeInterpolation::Bezier};
    Keyframe kf3{.time = TimelineTime::fromSeconds(4.0), .value = 2.0, .interpolation = KeyframeInterpolation::Hold};
    chScaleY.addOrUpdateKeyframe(kf2);
    chScaleY.addOrUpdateKeyframe(kf3);

    channels["transform.scaleX"] = chScaleX;
    channels["transform.scaleY"] = chScaleY;

    // 4. getGroupKeyframesAtTime & hasGroupKeyframeAtTime
    TEST_ASSERT(AnimationKeyframeQueryEngine::hasGroupKeyframeAtTime(channels, "transform.scale", TimelineTime::fromSeconds(2.0)));
    TEST_ASSERT(!AnimationKeyframeQueryEngine::hasGroupKeyframeAtTime(channels, "transform.scale", TimelineTime::fromSeconds(8.0)));

    auto groupKeys = AnimationKeyframeQueryEngine::getGroupKeyframesAtTime(channels, "transform.scale", TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(groupKeys.size() == 2);

    // 5. getElementKeyframes
    auto allKeys = AnimationKeyframeQueryEngine::getElementKeyframes(channels);
    TEST_ASSERT(allKeys.size() == 3);
    TEST_ASSERT(allKeys[0].time == TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(allKeys[2].time == TimelineTime::fromSeconds(4.0));

    // 6. hasKeyframesForPath & getKeyframeAtTime
    TEST_ASSERT(AnimationKeyframeQueryEngine::hasKeyframesForPath(channels, "transform.scaleX"));
    TEST_ASSERT(!AnimationKeyframeQueryEngine::hasKeyframesForPath(channels, "opacity"));

    auto kfOpt = AnimationKeyframeQueryEngine::getKeyframeAtTime(channels, "transform.scaleY", TimelineTime::fromSeconds(4.0));
    TEST_ASSERT(kfOpt.has_value());
    TEST_ASSERT(kfOpt->value == 2.0);
    TEST_ASSERT(kfOpt->interpolation == KeyframeInterpolation::Hold);

    std::cout << "[PASS] runAnimationKeyframeQueryEngineTests" << std::endl;
}

void runCommandReactorPipelineTests() {
    using namespace catchim;
    using namespace catchim::core;
    using namespace catchim::editor;

    CommandReactorPipeline pipeline;

    int callCountA = 0;
    int callCountB = 0;

    size_t tokA = pipeline.registerReactor([&]() { callCountA++; });
    size_t tokB = pipeline.registerReactor([&]() { callCountB++; });

    TEST_ASSERT(tokA != 0);
    TEST_ASSERT(tokB != 0);
    TEST_ASSERT(pipeline.reactorCount() == 2);

    pipeline.notifyReactors();
    TEST_ASSERT(callCountA == 1);
    TEST_ASSERT(callCountB == 1);

    pipeline.unregisterReactor(tokA);
    TEST_ASSERT(pipeline.reactorCount() == 1);

    pipeline.notifyReactors();
    TEST_ASSERT(callCountA == 1);
    TEST_ASSERT(callCountB == 2);

    pipeline.clear();
    TEST_ASSERT(pipeline.reactorCount() == 0);

    // pruneEmptyTracks tests
    Timeline timeline;
    auto& overlayWithClip = timeline.addTrack(TrackType::Video, "Overlay With Clip");
    ClipId cId = ClipId::generate();
    overlayWithClip.insertClip(Clip(cId, ClipType::Video, "Clip", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(3.0)));

    auto& overlayEmpty = timeline.addTrack(TrackType::Video, "Empty Overlay");
    (void)overlayEmpty;

    auto& audioEmpty1 = timeline.addTrack(TrackType::Audio, "Empty Audio 1");
    auto& audioEmpty2 = timeline.addTrack(TrackType::Audio, "Empty Audio 2");
    (void)audioEmpty1;
    (void)audioEmpty2;

    TEST_ASSERT(timeline.overlayTracks().size() == 2);
    TEST_ASSERT(timeline.audioTracks().size() == 2);

    auto pruneRes = CommandReactorPipeline::pruneEmptyTracks(timeline);
    TEST_ASSERT(pruneRes.changed == true);
    TEST_ASSERT(pruneRes.overlayTracksPruned == 1);
    TEST_ASSERT(pruneRes.audioTracksPruned == 2);
    TEST_ASSERT(timeline.overlayTracks().size() == 1);
    TEST_ASSERT(timeline.audioTracks().empty());

    // Second prune produces no changes
    auto pruneRes2 = CommandReactorPipeline::pruneEmptyTracks(timeline);
    TEST_ASSERT(pruneRes2.changed == false);

    // createAutoPruneReactor
    bool onPrunedCalled = false;
    timeline.addTrack(TrackType::Video, "Another Empty");
    auto reactor = CommandReactorPipeline::createAutoPruneReactor(timeline, [&](const PruneResult& res) {
        onPrunedCalled = true;
        TEST_ASSERT(res.overlayTracksPruned == 1);
    });

    reactor();
    TEST_ASSERT(onPrunedCalled);

    std::cout << "[PASS] runCommandReactorPipelineTests" << std::endl;
}

void runTimelinePixelUtilsTests() {
    double basePx = TimelinePixelUtils::BASE_TIMELINE_PIXELS_PER_SECOND;
    TEST_ASSERT(basePx == 50.0);
    double zMin = TimelinePixelUtils::TIMELINE_ZOOM_MIN;
    TEST_ASSERT(zMin == 0.1);
    double zMax = TimelinePixelUtils::TIMELINE_ZOOM_MAX;
    TEST_ASSERT(zMax == 100.0);
    double lineW = TimelinePixelUtils::TIMELINE_INDICATOR_LINE_WIDTH_PX;
    TEST_ASSERT(lineW == 2.0);

    TEST_ASSERT(TimelinePixelUtils::getTimelinePixelsPerSecond(1.0) == 50.0);
    TEST_ASSERT(TimelinePixelUtils::getTimelinePixelsPerSecond(2.0) == 100.0);

    const auto t2 = TimelineTime::fromSeconds(2.0);
    TEST_ASSERT(TimelinePixelUtils::timelineTimeToPixels(t2, 1.0) == 100.0);
    TEST_ASSERT(TimelinePixelUtils::timelineTimeToPixels(t2, 0.5) == 50.0);

    // Grid snapping
    TEST_ASSERT(TimelinePixelUtils::snapPixelToDeviceGrid(10.33, 1.0) == 10.0);
    TEST_ASSERT(TimelinePixelUtils::snapPixelToDeviceGrid(10.33, 2.0) == 10.5);
    TEST_ASSERT(TimelinePixelUtils::snapPixelToDeviceGrid(10.74, 2.0) == 10.5);
    TEST_ASSERT(TimelinePixelUtils::snapPixelToDeviceGrid(10.76, 2.0) == 11.0);

    // timelineTimeToSnappedPixels
    const auto t01 = TimelineTime::fromSeconds(0.1);
    TEST_ASSERT(TimelinePixelUtils::timelineTimeToSnappedPixels(t01, 1.0, 1.0) == 5.0);

    // getCenteredLineLeft
    TEST_ASSERT(TimelinePixelUtils::getCenteredLineLeft(100.0, 2.0) == 99.0);
    TEST_ASSERT(TimelinePixelUtils::getCenteredLineLeft(50.0, 4.0) == 48.0);

    std::cout << "[PASS] runTimelinePixelUtilsTests" << std::endl;
}

void runTimelineZoomUtilsTests() {
    // Zoom min for duration 10s and container 1000px:
    // availableWidth = 1000 * 0.25 = 250; zoomToFit = 250 / (10 * 50) = 0.5
    const auto d10 = TimelineTime::fromSeconds(10.0);
    const double minZoom = TimelineZoomUtils::getTimelineZoomMin(d10, 1000.0);
    TEST_ASSERT(std::abs(minZoom - 0.5) < 1e-4);

    // Zoom percent
    TEST_ASSERT(TimelineZoomUtils::getZoomPercent(0.1, 0.1, 100.0) == 0.0);
    TEST_ASSERT(TimelineZoomUtils::getZoomPercent(100.0, 0.1, 100.0) == 1.0);

    // Padding px
    const double padAtMin = TimelineZoomUtils::getTimelinePaddingPx(1000.0, 0.1, 0.1);
    TEST_ASSERT(std::abs(padAtMin - 750.0) < 1e-4); // 1000 * 0.75

    // Slider <-> Zoom mapping
    TEST_ASSERT(std::abs(TimelineZoomUtils::sliderToZoom(0.0, 0.1, 100.0) - 0.1) < 1e-4);
    TEST_ASSERT(std::abs(TimelineZoomUtils::sliderToZoom(1.0, 0.1, 100.0) - 100.0) < 1e-4);

    TEST_ASSERT(std::abs(TimelineZoomUtils::zoomToSlider(0.1, 0.1, 100.0) - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(TimelineZoomUtils::zoomToSlider(100.0, 0.1, 100.0) - 1.0) < 1e-4);

    // Roundtrip
    const double testSlider = 0.42;
    const double roundtripZoom = TimelineZoomUtils::sliderToZoom(testSlider, 0.1, 100.0);
    const double roundtripSlider = TimelineZoomUtils::zoomToSlider(roundtripZoom, 0.1, 100.0);
    TEST_ASSERT(std::abs(roundtripSlider - testSlider) < 1e-4);

    std::cout << "[PASS] runTimelineZoomUtilsTests" << std::endl;
}

void runTimelineDragDataTests() {
    TEST_ASSERT(TimelineDragEngine::DEFAULT_NEW_ELEMENT_DURATION == TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(TimelineDragEngine::toElementDurationTicks(std::nullopt) == TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(TimelineDragEngine::toElementDurationTicks(3.5) == TimelineTime::fromSeconds(3.5));

    // mouseTimeFromClientX: clientX=300, containerLeft=100, scrollLeft=50, zoom=1.0
    // mouseX = 300 - 100 + 50 = 250; pixelsPerSec = 50; seconds = 5.0
    const auto mouseTime = TimelineDragEngine::getMouseTimeFromClientX(300.0, 100.0, 50.0, 1.0);
    TEST_ASSERT(std::abs(mouseTime.toSeconds() - 5.0) < 1e-4);

    // Drag data variant
    MediaDragData media{
        .id = "m_101",
        .name = "clip.mp4",
        .mediaType = ClipType::Video
    };
    TimelineDragData d1 = media;
    TEST_ASSERT(TimelineDragEngine::getDragDataId(d1) == "m_101");
    TEST_ASSERT(TimelineDragEngine::getDragDataName(d1) == "clip.mp4");
    TEST_ASSERT(TimelineDragEngine::getDragDataType(d1) == "media");

    TextDragData text{
        .id = "t_202",
        .name = "My Text",
        .content = "Sample Subtitle"
    };
    TimelineDragData d2 = text;
    TEST_ASSERT(TimelineDragEngine::getDragDataId(d2) == "t_202");
    TEST_ASSERT(TimelineDragEngine::getDragDataName(d2) == "My Text");
    TEST_ASSERT(TimelineDragEngine::getDragDataType(d2) == "text");

    StickerDragData sticker{
        .id = "s_303",
        .name = "Star",
        .stickerId = "star_icon"
    };
    TimelineDragData d3 = sticker;
    TEST_ASSERT(TimelineDragEngine::getDragDataType(d3) == "sticker");

    GraphicDragData graphic{
        .id = "g_404",
        .name = "Rectangle",
        .definitionId = "rect_1",
        .params = nlohmann::json{{"fill", "#ff0000"}}
    };
    TimelineDragData d4 = graphic;
    TEST_ASSERT(TimelineDragEngine::getDragDataType(d4) == "graphic");

    EffectDragData effect{
        .id = "e_505",
        .name = "Blur",
        .effectType = "gaussian_blur",
        .targetElementTypes = {ClipType::Video, ClipType::Image}
    };
    TimelineDragData d5 = effect;
    TEST_ASSERT(TimelineDragEngine::getDragDataType(d5) == "effect");

    std::cout << "[PASS] runTimelineDragDataTests" << std::endl;
}

void runBackgroundPresetsTests() {
    int defBlur = BackgroundPresets::DEFAULT_BACKGROUND_BLUR_INTENSITY;
    TEST_ASSERT(defBlur == 10);
    std::string defColor = BackgroundPresets::DEFAULT_BACKGROUND_COLOR;
    TEST_ASSERT(defColor == "#000000");

    const auto& presets = BackgroundPresets::getBlurPresets();
    TEST_ASSERT(presets.size() == 3);

    auto pLight = BackgroundPresets::findBlurPreset("Light");
    TEST_ASSERT(pLight.has_value() && pLight->value == 100);

    auto pMed = BackgroundPresets::findBlurPreset("Medium");
    TEST_ASSERT(pMed.has_value() && pMed->value == 200);

    auto pHeavy = BackgroundPresets::findBlurPreset("Heavy");
    TEST_ASSERT(pHeavy.has_value() && pHeavy->value == 500);

    auto pNone = BackgroundPresets::findBlurPreset("Invalid");
    TEST_ASSERT(!pNone.has_value());

    TEST_ASSERT(BackgroundPresets::clampBlurIntensity(-5) == 0);
    TEST_ASSERT(BackgroundPresets::clampBlurIntensity(1500) == 1000);
    TEST_ASSERT(BackgroundPresets::clampBlurIntensity(250) == 250);

    std::cout << "[PASS] runBackgroundPresetsTests" << std::endl;
}

void runParamChannelLayoutEngineTests() {
    // srgb <-> linear
    TEST_ASSERT(std::abs(ParamChannelLayoutEngine::srgbToLinear(0.0) - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(ParamChannelLayoutEngine::srgbToLinear(1.0) - 1.0) < 1e-4);
    TEST_ASSERT(std::abs(ParamChannelLayoutEngine::linearToSrgb(0.0) - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(ParamChannelLayoutEngine::linearToSrgb(1.0) - 1.0) < 1e-4);

    const double roundtrip = ParamChannelLayoutEngine::linearToSrgb(ParamChannelLayoutEngine::srgbToLinear(0.5));
    TEST_ASSERT(std::abs(roundtrip - 0.5) < 1e-4);

    // parseColorToLinearRgba
    auto white = ParamChannelLayoutEngine::parseColorToLinearRgba("#ffffff");
    TEST_ASSERT(white.has_value());
    TEST_ASSERT(std::abs(white->r - 1.0) < 1e-4);
    TEST_ASSERT(std::abs(white->g - 1.0) < 1e-4);
    TEST_ASSERT(std::abs(white->b - 1.0) < 1e-4);
    TEST_ASSERT(std::abs(white->a - 1.0) < 1e-4);

    auto black = ParamChannelLayoutEngine::parseColorToLinearRgba("#000000");
    TEST_ASSERT(black.has_value());
    TEST_ASSERT(black->r == 0.0 && black->g == 0.0 && black->b == 0.0);

    // formatLinearRgba
    TEST_ASSERT(ParamChannelLayoutEngine::formatLinearRgba(white.value()) == "#ffffff");
    TEST_ASSERT(ParamChannelLayoutEngine::formatLinearRgba(black.value()) == "#000000");

    // Coercion
    TEST_ASSERT(ParamChannelLayoutEngine::coerceParamValueNumber(10.33, 0.0, 20.0, 0.5) == 10.5);
    TEST_ASSERT(ParamChannelLayoutEngine::coerceParamValueNumber(25.0, 0.0, 20.0, 1.0) == 20.0);
    TEST_ASSERT(ParamChannelLayoutEngine::coerceParamValueNumber(-5.0, 0.0, 20.0, 1.0) == 0.0);

    auto selA = ParamChannelLayoutEngine::coerceParamValueSelect("optionA", {"optionA", "optionB"});
    TEST_ASSERT(selA.has_value() && *selA == "optionA");

    auto selC = ParamChannelLayoutEngine::coerceParamValueSelect("optionC", {"optionA", "optionB"});
    TEST_ASSERT(!selC.has_value());

    std::cout << "[PASS] runParamChannelLayoutEngineTests" << std::endl;
}

void runI18nEngineTests() {
    auto& i18n = I18nEngine::instance();
    TEST_ASSERT(i18n.setLocale("en"));
    TEST_ASSERT(i18n.currentLocale() == "en");

    // Lookup in English
    TEST_ASSERT(i18n.t("common.apply") == "Apply");
    TEST_ASSERT(i18n.t("common.cancel") == "Cancel");
    TEST_ASSERT(i18n.t("settings.aspectRatio") == "Aspect ratio");
    TEST_ASSERT(i18n.hasKey("settings.aspectRatio"));

    // Switch to Vietnamese
    TEST_ASSERT(i18n.setLocale("vi"));
    TEST_ASSERT(i18n.currentLocale() == "vi");
    TEST_ASSERT(i18n.t("common.apply") == "Áp dụng");
    TEST_ASSERT(i18n.t("common.cancel") == "Hủy");
    TEST_ASSERT(i18n.t("settings.aspectRatio") == "Tỉ lệ khung hình");

    // Template interpolation
    std::string exportMsg = i18n.t("export.exporting", {{"name", "VideoFinal"}});
    TEST_ASSERT(exportMsg == "Đang xuất VideoFinal...");

    // Custom translation
    i18n.addTranslation("vi", "custom.hello", "Xin chào {user}!");
    TEST_ASSERT(i18n.t("custom.hello", {{"user", "Catchim"}}) == "Xin chào Catchim!");

    // Missing key fallback
    TEST_ASSERT(i18n.t("missing.key.123") == "missing.key.123");

    // Invalid locale rejection
    TEST_ASSERT(!i18n.setLocale("invalid_locale"));

    // Restore to English
    i18n.setLocale("en");

    std::cout << "[PASS] runI18nEngineTests" << std::endl;
}

void runRetimeResolutionEngineTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. Rate clamping
    TEST_ASSERT(RetimeResolutionEngine::clampRetimeRate(0.005) == RetimeResolutionEngine::MIN_RATE);
    TEST_ASSERT(RetimeResolutionEngine::clampRetimeRate(10.0) == RetimeResolutionEngine::MAX_RATE);
    TEST_ASSERT(RetimeResolutionEngine::clampRetimeRate(2.5) == 2.5);
    TEST_ASSERT(RetimeResolutionEngine::clampRetimeRate(-1.0) == RetimeResolutionEngine::DEFAULT_RETIME_RATE);

    // 2. Pitch maintenance capability
    TEST_ASSERT(!RetimeResolutionEngine::canMaintainPitch(0.0));
    TEST_ASSERT(RetimeResolutionEngine::canMaintainPitch(0.25));
    TEST_ASSERT(RetimeResolutionEngine::canMaintainPitch(2.0));
    TEST_ASSERT(RetimeResolutionEngine::shouldMaintainPitch(2.0, true));
    TEST_ASSERT(!RetimeResolutionEngine::shouldMaintainPitch(2.0, false));

    // 3. Time conversions
    auto clipTime = TimelineTime::fromSeconds(2.0);
    auto sourceTimeAt2x = RetimeResolutionEngine::getSourceTimeAtClipTime(clipTime, 2.0);
    TEST_ASSERT(sourceTimeAt2x == TimelineTime::fromSeconds(4.0));

    auto sourceTimeAtHalf = RetimeResolutionEngine::getSourceTimeAtClipTime(clipTime, 0.5);
    TEST_ASSERT(sourceTimeAtHalf == TimelineTime::fromSeconds(1.0));

    auto clipTimeFrom4sAt2x = RetimeResolutionEngine::getClipTimeAtSourceTime(TimelineTime::fromSeconds(4.0), 2.0);
    TEST_ASSERT(clipTimeFrom4sAt2x == TimelineTime::fromSeconds(2.0));

    // 4. Span conversions
    auto sourceSpan = TimelineTime::fromSeconds(10.0);
    auto timelineDuration = RetimeResolutionEngine::getTimelineDurationForSourceSpan(sourceSpan, 2.0);
    TEST_ASSERT(timelineDuration == TimelineTime::fromSeconds(5.0));

    auto span = RetimeResolutionEngine::getSourceSpanAtClipTime(
        TimelineTime::fromSeconds(2.0), // clipTime
        2.0                             // rate
    );
    TEST_ASSERT(span == TimelineTime::fromSeconds(4.0));

    std::cout << "[PASS] runRetimeResolutionEngineTests" << std::endl;
}

void runEffectDefinitionRegistryTests() {
    using namespace catchim::render;

    // 1. Intensity to Sigma
    TEST_ASSERT(EffectDefinitionRegistry::intensityToSigma(0.0) == 0.0);
    TEST_ASSERT(EffectDefinitionRegistry::intensityToSigma(15.0) == 3.0);
    TEST_ASSERT(EffectDefinitionRegistry::intensityToSigma(50.0) == 10.0);

    // 2. Gaussian Blur Passes
    auto passes = EffectDefinitionRegistry::buildGaussianBlurPasses(3.0, 3.0);
    TEST_ASSERT(passes.size() == 2);
    TEST_ASSERT(passes[0].shader == EffectDefinitionRegistry::GAUSSIAN_BLUR_SHADER);
    TEST_ASSERT(passes[0].direction.first == 1.0f && passes[0].direction.second == 0.0f);
    TEST_ASSERT(std::abs(passes[0].sigma - 3.0f) < 0.001f);
    TEST_ASSERT(passes[1].direction.first == 0.0f && passes[1].direction.second == 1.0f);
    TEST_ASSERT(std::abs(passes[1].sigma - 3.0f) < 0.001f);

    // 3. Registry query
    const auto& defs = EffectDefinitionRegistry::definitions();
    TEST_ASSERT(!defs.empty());

    const auto* blurDef = EffectDefinitionRegistry::findDefinition("blur");
    TEST_ASSERT(blurDef != nullptr);
    TEST_ASSERT(blurDef->type == "blur");
    TEST_ASSERT(blurDef->parameters.size() == 1);
    TEST_ASSERT(blurDef->parameters[0].id == "intensity");

    TEST_ASSERT(EffectDefinitionRegistry::findDefinition("non_existent") == nullptr);

    // 4. Instance & Pass Resolution
    auto instance = EffectDefinitionRegistry::buildDefaultEffectInstance("blur");
    TEST_ASSERT(instance.type == "blur");
    TEST_ASSERT(instance.params["intensity"] == 15.0);

    auto resolved = EffectDefinitionRegistry::resolveEffectPasses(instance, 1920.0, 1080.0);
    TEST_ASSERT(resolved.size() == 2);
    TEST_ASSERT(std::abs(resolved[0].sigma - 3.0f) < 0.001f);
    TEST_ASSERT(std::abs(resolved[1].sigma - 3.0f) < 0.001f);

    std::cout << "[PASS] runEffectDefinitionRegistryTests" << std::endl;
}

void runCanvasSizePresetsTests() {
    using namespace catchim::render;
    using namespace catchim::editor;

    // 1. Default size
    CanvasSize defaultSize = CanvasSizePresets::DEFAULT_CANVAS_SIZE;
    TEST_ASSERT(defaultSize.width == 1920);
    TEST_ASSERT(defaultSize.height == 1080);

    // 2. Presets list
    const auto& presets = CanvasSizePresets::defaultCanvasPresets();
    TEST_ASSERT(presets.size() == 4);
    TEST_ASSERT(presets[0] == CanvasSize(1920, 1080));
    TEST_ASSERT(presets[1] == CanvasSize(1080, 1920));
    TEST_ASSERT(presets[2] == CanvasSize(1080, 1080));
    TEST_ASSERT(presets[3] == CanvasSize(1440, 1080));

    // 3. Preset check
    TEST_ASSERT(CanvasSizePresets::isDefaultPreset(CanvasSize(1920, 1080)));
    TEST_ASSERT(CanvasSizePresets::isDefaultPreset(CanvasSize(1080, 1920)));
    TEST_ASSERT(!CanvasSizePresets::isDefaultPreset(CanvasSize(1280, 720)));

    // 4. Aspect ratio
    TEST_ASSERT(std::abs(CanvasSizePresets::getAspectRatio(CanvasSize(1920, 1080)) - (16.0 / 9.0)) < 1e-6);
    TEST_ASSERT(std::abs(CanvasSizePresets::getAspectRatio(CanvasSize(1080, 1920)) - (9.0 / 16.0)) < 1e-6);
    TEST_ASSERT(std::abs(CanvasSizePresets::getAspectRatio(CanvasSize(1080, 1080)) - 1.0) < 1e-6);
    TEST_ASSERT(CanvasSizePresets::getAspectRatio(CanvasSize(1920, 0)) == 0.0);

    std::cout << "[PASS] runCanvasSizePresetsTests" << std::endl;
}

void runSceneHierarchyUtilsTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. buildDefaultScene
    auto scene1 = SceneHierarchyUtils::buildDefaultScene("Intro", false);
    TEST_ASSERT(scene1.name() == "Intro");
    TEST_ASSERT(!scene1.isMain());

    auto scene2 = SceneHierarchyUtils::buildDefaultScene("Main", true);
    TEST_ASSERT(scene2.name() == "Main");
    TEST_ASSERT(scene2.isMain());

    // 2. getMainScene & ensureMainScene
    std::vector<Scene> scenes;
    TEST_ASSERT(SceneHierarchyUtils::getMainScene(scenes) == nullptr);

    scenes.push_back(std::move(scene1));
    TEST_ASSERT(SceneHierarchyUtils::getMainScene(scenes) == nullptr);

    SceneHierarchyUtils::ensureMainScene(scenes);
    TEST_ASSERT(scenes.size() == 2);
    TEST_ASSERT(scenes[0].isMain());
    TEST_ASSERT(scenes[0].name() == "Main scene");
    TEST_ASSERT(SceneHierarchyUtils::getMainScene(scenes) == &scenes[0]);

    // 3. canDeleteScene
    auto resMain = SceneHierarchyUtils::canDeleteScene(scenes[0]);
    TEST_ASSERT(!resMain.canDelete);
    TEST_ASSERT(resMain.reason == "Cannot delete main scene");

    auto resOther = SceneHierarchyUtils::canDeleteScene(scenes[1]);
    TEST_ASSERT(resOther.canDelete);
    TEST_ASSERT(resOther.reason.empty());

    // 4. getFallbackSceneAfterDelete
    auto mainId = scenes[0].id();
    auto otherId = scenes[1].id();
    // Deleting other while on other -> fallback to main
    const auto* fallback = SceneHierarchyUtils::getFallbackSceneAfterDelete(scenes, otherId, otherId);
    TEST_ASSERT(fallback != nullptr && fallback->id() == mainId);

    // Deleting other while on main -> stays on main
    fallback = SceneHierarchyUtils::getFallbackSceneAfterDelete(scenes, otherId, mainId);
    TEST_ASSERT(fallback != nullptr && fallback->id() == mainId);

    // 5. findCurrentScene
    TEST_ASSERT(SceneHierarchyUtils::findCurrentScene(scenes, otherId) == &scenes[1]);
    TEST_ASSERT(SceneHierarchyUtils::findCurrentScene(scenes, std::nullopt) == &scenes[0]);

    // 6. calculateTotalDuration & getProjectDurationFromScenes
    // Add clip to main scene's timeline
    scenes[0].timeline().mainTrack().clips().push_back(Clip(
        ClipId::generate(),
        ClipType::Video,
        "clip1",
        TimelineTime::fromSeconds(1.0),
        TimelineTime::fromSeconds(4.0)
    ));
    auto dur = SceneHierarchyUtils::getProjectDurationFromScenes(scenes);
    TEST_ASSERT(dur == TimelineTime::fromSeconds(5.0));

    std::cout << "[PASS] runSceneHierarchyUtilsTests" << std::endl;
}

void runUuidGeneratorTests() {
    using namespace catchim::core;

    // 1. Basic generation and validity
    auto uuid = UuidGenerator::generateUUID();
    TEST_ASSERT(uuid.size() == 36);
    TEST_ASSERT(UuidGenerator::isValidUUID(uuid));

    // 2. Multiple unique UUIDs
    std::unordered_set<std::string> seen;
    for (int i = 0; i < 50; ++i) {
        auto u = UuidGenerator::generateUUID();
        TEST_ASSERT(UuidGenerator::isValidUUID(u));
        TEST_ASSERT(seen.insert(u).second);
    }

    // 3. Validity verification
    TEST_ASSERT(!UuidGenerator::isValidUUID("not-a-uuid"));
    TEST_ASSERT(!UuidGenerator::isValidUUID("12345678-1234-1234-1234-123456789abc")); // version != 4
    TEST_ASSERT(!UuidGenerator::isValidUUID("12345678-1234-4234-0234-123456789abc")); // variant != 8/9/a/b
    TEST_ASSERT(UuidGenerator::isValidUUID("12345678-1234-4234-8234-123456789abc"));
    TEST_ASSERT(UuidGenerator::isValidUUID("12345678-1234-4234-9234-123456789abc"));
    TEST_ASSERT(UuidGenerator::isValidUUID("12345678-1234-4234-a234-123456789abc"));
    TEST_ASSERT(UuidGenerator::isValidUUID("12345678-1234-4234-b234-123456789abc"));

    std::cout << "[PASS] runUuidGeneratorTests" << std::endl;
}

void runStringUtilsTests() {
    using namespace catchim::core;

    // 1. capitalizeFirstLetter
    TEST_ASSERT(StringUtils::capitalizeFirstLetter("hello") == "Hello");
    TEST_ASSERT(StringUtils::capitalizeFirstLetter("Hello") == "Hello");
    TEST_ASSERT(StringUtils::capitalizeFirstLetter("") == "");
    TEST_ASSERT(StringUtils::capitalizeFirstLetter("a") == "A");

    // 2. uppercase & lowercase
    TEST_ASSERT(StringUtils::uppercase("hello world") == "HELLO WORLD");
    TEST_ASSERT(StringUtils::lowercase("HELLO WORLD") == "hello world");

    // 3. Platform keys
#if defined(__APPLE__)
    TEST_ASSERT(StringUtils::isAppleDevice());
    TEST_ASSERT(StringUtils::getPlatformSpecialKey() == "\xE2\x8C\x98");
    TEST_ASSERT(StringUtils::getPlatformAlternateKey() == "\xE2\x8C\xA5");
#else
    TEST_ASSERT(!StringUtils::isAppleDevice());
    TEST_ASSERT(StringUtils::getPlatformSpecialKey() == "Ctrl");
    TEST_ASSERT(StringUtils::getPlatformAlternateKey() == "Alt");
#endif

    std::cout << "[PASS] runStringUtilsTests" << std::endl;
}

void runGeometryUtilsTests() {
    using namespace catchim::core;

    // 1. GCD calculations
    TEST_ASSERT(GeometryUtils::gcd(1920, 1080) == 120);
    TEST_ASSERT(GeometryUtils::gcd(1080, 1920) == 120);
    TEST_ASSERT(GeometryUtils::gcd(1080, 1080) == 1080);
    TEST_ASSERT(GeometryUtils::gcd(1440, 1080) == 360);

    // 2. Aspect ratio strings
    std::string ar1 = GeometryUtils::dimensionToAspectRatio(1920, 1080);
    TEST_ASSERT(ar1 == "16:9");
    std::string ar2 = GeometryUtils::dimensionToAspectRatio(1080, 1920);
    TEST_ASSERT(ar2 == "9:16");
    std::string ar3 = GeometryUtils::dimensionToAspectRatio(1080, 1080);
    TEST_ASSERT(ar3 == "1:1");
    std::string ar4 = GeometryUtils::dimensionToAspectRatio(1440, 1080);
    TEST_ASSERT(ar4 == "4:3");
    std::string ar5 = GeometryUtils::dimensionToAspectRatio(1080, 1350);
    TEST_ASSERT(ar5 == "4:5");

    // 3. Fallbacks and float overloads
    std::string ar6 = GeometryUtils::dimensionToAspectRatio(0, 1080);
    TEST_ASSERT(ar6 == "0:0");
    std::string ar7 = GeometryUtils::dimensionToAspectRatio(1920.0, 1080.0);
    TEST_ASSERT(ar7 == "16:9");
    std::string ar8 = GeometryUtils::dimensionToAspectRatio(1080.0, 1920.0);
    TEST_ASSERT(ar8 == "9:16");

    std::cout << "[PASS] runGeometryUtilsTests" << std::endl;
}

void runDateUtilsTests() {
    using namespace catchim::core;

    // 1. Direct year/month/day
    TEST_ASSERT(DateUtils::formatDate(2026, 9, 20) == "Sep 20, 2026");
    TEST_ASSERT(DateUtils::formatDate(2025, 1, 1) == "Jan 1, 2025");
    TEST_ASSERT(DateUtils::formatDate(2024, 12, 31) == "Dec 31, 2024");

    // 2. Month clamping
    TEST_ASSERT(DateUtils::formatDate(2026, 0, 5) == "Jan 5, 2026");
    TEST_ASSERT(DateUtils::formatDate(2026, 13, 5) == "Jan 5, 2026");

    // 3. System clock time_point
    auto now = std::chrono::system_clock::now();
    auto str = DateUtils::formatDate(now);
    TEST_ASSERT(!str.empty());
    TEST_ASSERT(str.find("202") != std::string::npos);

    std::cout << "[PASS] runDateUtilsTests" << std::endl;
}

void runRenderParamResolversTests() {
    using namespace catchim::render;
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. Blend mode checks
    TEST_ASSERT(RenderParamResolvers::isBlendMode("normal"));
    TEST_ASSERT(RenderParamResolvers::isBlendMode("multiply"));
    TEST_ASSERT(RenderParamResolvers::isBlendMode("screen"));
    TEST_ASSERT(RenderParamResolvers::isBlendMode("overlay"));
    TEST_ASSERT(!RenderParamResolvers::isBlendMode("invalid"));

    // 2. Parsing from json
    nlohmann::json p1 = {
        {"blendMode", "multiply"},
        {"opacity", 0.75},
        {"transform.positionX", 15.0},
        {"transform.positionY", -25.0},
        {"transform.scaleX", 1.5},
        {"transform.scaleY", 0.8},
        {"transform.rotate", 45.0}
    };
    TEST_ASSERT(RenderParamResolvers::readBlendModeFromParams(p1) == "multiply");
    TEST_ASSERT(RenderParamResolvers::readOpacityFromParams(p1) == 0.75);

    auto t = RenderParamResolvers::buildTransformFromParams(p1);
    TEST_ASSERT(t.positionX == 15.0);
    TEST_ASSERT(t.positionY == -25.0);
    TEST_ASSERT(t.scaleX == 1.5);
    TEST_ASSERT(t.scaleY == 0.8);
    TEST_ASSERT(t.rotate == 45.0);
    TEST_ASSERT(t.opacity == 0.75);
    TEST_ASSERT(t.blendMode == "multiply");

    // 3. Defaults
    nlohmann::json pEmpty = nlohmann::json::object();
    TEST_ASSERT(RenderParamResolvers::readBlendModeFromParams(pEmpty) == "normal");
    TEST_ASSERT(RenderParamResolvers::readOpacityFromParams(pEmpty) == 1.0);

    // 4. Animated transform resolution
    AnimationChannel posX("transform.positionX");
    posX.addOrUpdateKeyframe(Keyframe{TimelineTime::fromSeconds(0.0), 0.0});
    posX.addOrUpdateKeyframe(Keyframe{TimelineTime::fromSeconds(2.0), 100.0});

    std::vector<AnimationChannel> channels = { posX };
    auto resolved = RenderParamResolvers::resolveTransformAtTime(t, channels, TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(std::abs(resolved.positionX - 50.0) < 1e-4);

    std::cout << "[PASS] runRenderParamResolversTests" << std::endl;
}

void runAnimationValueResolversTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. Opacity and Number resolution
    TEST_ASSERT(AnimationValueResolvers::resolveOpacityAtTime(0.8, nullptr, TimelineTime::fromSeconds(1.0)) == 0.8);
    TEST_ASSERT(AnimationValueResolvers::resolveNumberAtTime(42.0, nullptr, TimelineTime::fromSeconds(1.0)) == 42.0);

    AnimationChannel opChannel("opacity");
    opChannel.addOrUpdateKeyframe(Keyframe{TimelineTime::fromSeconds(0.0), 0.2});
    opChannel.addOrUpdateKeyframe(Keyframe{TimelineTime::fromSeconds(2.0), 0.8});
    auto opVal = AnimationValueResolvers::resolveOpacityAtTime(1.0, &opChannel, TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(std::abs(opVal - 0.5) < 1e-4);

    // 2. Color resolution
    AnimationChannel greenChannel("color.g");
    greenChannel.addOrUpdateKeyframe(Keyframe{TimelineTime::fromSeconds(0.0), 0.0});
    greenChannel.addOrUpdateKeyframe(Keyframe{TimelineTime::fromSeconds(2.0), 255.0});
    std::vector<AnimationChannel> colorChannels = { greenChannel };

    auto col = AnimationValueResolvers::resolveColorAtTime("#ff0000", colorChannels, TimelineTime::fromSeconds(1.0));
    auto rgb = ColorUtils::hexToRgb(col);
    TEST_ASSERT(rgb.has_value());
    TEST_ASSERT(rgb->r == 255);
    TEST_ASSERT(rgb->g >= 126 && rgb->g <= 129);
    TEST_ASSERT(rgb->b == 0);

    // 3. Normalized cubic bezier & curve handles
    Keyframe leftKey;
    leftKey.time = TimelineTime::fromSeconds(0.0);
    leftKey.value = 0.0;
    leftKey.rightHandle = KeyframeHandle{
        .x = static_cast<double>(TimelineTime::fromSeconds(0.333).ticks()),
        .y = 20.0
    };

    Keyframe rightKey;
    rightKey.time = TimelineTime::fromSeconds(1.0);
    rightKey.value = 100.0;
    rightKey.leftHandle = KeyframeHandle{
        .x = static_cast<double>(-TimelineTime::fromSeconds(0.333).ticks()),
        .y = -20.0
    };

    auto bezierOpt = AnimationValueResolvers::getNormalizedCubicBezierForScalarSegment(leftKey, rightKey);
    TEST_ASSERT(bezierOpt.has_value());
    TEST_ASSERT(bezierOpt->x1 >= 0.0 && bezierOpt->x1 <= 1.0);
    TEST_ASSERT(bezierOpt->x2 >= 0.0 && bezierOpt->x2 <= 1.0);

    auto handlesOpt = AnimationValueResolvers::getCurveHandlesForNormalizedCubicBezier(leftKey, rightKey, *bezierOpt);
    TEST_ASSERT(handlesOpt.has_value());
    TEST_ASSERT(std::abs(handlesOpt->rightHandle.y - 20.0) < 1e-4);
    TEST_ASSERT(std::abs(handlesOpt->leftHandle.y - (-20.0)) < 1e-4);

    std::cout << "[PASS] runAnimationValueResolversTests" << std::endl;
}

void runTimelineTrackDefaultsTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 1. Default track names
    TEST_ASSERT(TimelineTrackDefaults::getDefaultTrackName(TrackType::Video) == "Video track");
    TEST_ASSERT(TimelineTrackDefaults::getDefaultTrackName(TrackType::Text) == "Text track");
    TEST_ASSERT(TimelineTrackDefaults::getDefaultTrackName(TrackType::Audio) == "Audio track");
    TEST_ASSERT(TimelineTrackDefaults::getDefaultTrackName(TrackType::Graphic) == "Graphic track");
    TEST_ASSERT(TimelineTrackDefaults::getDefaultTrackName(TrackType::Effect) == "Effect track");

    // 2. Volume dB limits
    TEST_ASSERT(TimelineTrackDefaults::clampVolumeDb(-100.0) == -60.0);
    TEST_ASSERT(TimelineTrackDefaults::clampVolumeDb(50.0) == 20.0);
    TEST_ASSERT(TimelineTrackDefaults::clampVolumeDb(0.0) == 0.0);

    // 3. Playhead snap points
    auto pts = TimelineTrackDefaults::getPlayheadSnapPoints(TimelineTime::fromSeconds(3.5));
    TEST_ASSERT(pts.size() == 1);
    TEST_ASSERT(pts[0].time == TimelineTime::fromSeconds(3.5));
    TEST_ASSERT(pts[0].type == snapping::TimelineSnapPointType::Playhead);

    std::cout << "[PASS] runTimelineTrackDefaultsTests" << std::endl;
}

void runTtsVoiceRegistryTests() {
    using namespace catchim::media;

    // 1. All voices query
    const auto& voices = TtsVoiceRegistry::getAllVoices();
    TEST_ASSERT(voices.size() >= 9);

    // 2. Lookup by id
    const auto* gptVoice = TtsVoiceRegistry::findVoiceById("vi-custom-gpt-sovits");
    TEST_ASSERT(gptVoice != nullptr);
    TEST_ASSERT(gptVoice->engine == "gpt-sovits");
    TEST_ASSERT(gptVoice->langCode == "vi");

    const auto* sweetVoice = TtsVoiceRegistry::findVoiceById("vi-female-sweet");
    TEST_ASSERT(sweetVoice != nullptr);
    TEST_ASSERT(sweetVoice->name == "Hoài My (Nữ miền Bắc)");
    TEST_ASSERT(sweetVoice->gender == "female");

    const auto* warmVoice = TtsVoiceRegistry::findVoiceById("vi-male-warm");
    TEST_ASSERT(warmVoice != nullptr);
    TEST_ASSERT(warmVoice->name == "Nam Minh (Nam miền Bắc)");
    TEST_ASSERT(warmVoice->gender == "male");

    const auto* googleVoice = TtsVoiceRegistry::findVoiceById("vi-female-google");
    TEST_ASSERT(googleVoice != nullptr);
    TEST_ASSERT(googleVoice->engine == "google");

    // 3. Filter by language
    auto viVoices = TtsVoiceRegistry::getVoicesByLanguage("vi-VN");
    TEST_ASSERT(viVoices.size() >= 9);

    // 4. Filter by category
    auto trendingVoices = TtsVoiceRegistry::getVoicesByCategory("trending");
    TEST_ASSERT(trendingVoices.size() >= 5);

    // 5. Filter by gender
    auto femaleVoices = TtsVoiceRegistry::getVoicesByGender("female");
    TEST_ASSERT(femaleVoices.size() >= 4);

    auto maleVoices = TtsVoiceRegistry::getVoicesByGender("male");
    TEST_ASSERT(maleVoices.size() >= 4);

    std::cout << "[PASS] runTtsVoiceRegistryTests" << std::endl;
}

void runPlaybackManagerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    PlaybackManager pm;
    TEST_ASSERT(!pm.isPlaying());
    TEST_ASSERT(pm.currentTime().ticks() == 0);
    TEST_ASSERT(pm.volume() == 1.0);
    TEST_ASSERT(!pm.isMuted());
    TEST_ASSERT(!pm.isScrubbing());

    int stateChanges = 0;
    pm.subscribe([&]() { ++stateChanges; });

    TimelineTime totalDur = TimelineTime::fromSeconds(10.0); // 1,200,000 ticks

    // Play & Pause & Toggle
    pm.play(totalDur);
    TEST_ASSERT(pm.isPlaying());
    TEST_ASSERT(stateChanges >= 1);

    pm.pause();
    TEST_ASSERT(!pm.isPlaying());

    pm.toggle(totalDur);
    TEST_ASSERT(pm.isPlaying());
    pm.toggle(totalDur);
    TEST_ASSERT(!pm.isPlaying());

    // Seek
    int seekCalls = 0;
    TimelineTime lastSeekTime{0};
    pm.onSeek([&](TimelineTime t) {
        ++seekCalls;
        lastSeekTime = t;
    });

    pm.seek(TimelineTime::fromSeconds(4.0), totalDur);
    TEST_ASSERT(pm.currentTime().toSeconds() == 4.0);
    TEST_ASSERT(seekCalls == 1);
    TEST_ASSERT(lastSeekTime.toSeconds() == 4.0);

    // Seek clamp < 0
    pm.seek(TimelineTime(-5000), totalDur);
    TEST_ASSERT(pm.currentTime().ticks() == 0);

    // Seek clamp > totalDuration
    pm.seek(TimelineTime::fromSeconds(20.0), totalDur);
    TEST_ASSERT(pm.currentTime() == totalDur);

    // Volume & Mute
    pm.setVolume(0.5);
    TEST_ASSERT(pm.volume() == 0.5);
    TEST_ASSERT(!pm.isMuted());

    pm.mute();
    TEST_ASSERT(pm.isMuted());
    TEST_ASSERT(pm.volume() == 0.0);

    pm.unmute();
    TEST_ASSERT(!pm.isMuted());
    TEST_ASSERT(pm.volume() == 0.5);

    pm.toggleMute();
    TEST_ASSERT(pm.isMuted());
    pm.toggleMute();
    TEST_ASSERT(!pm.isMuted());

    // Scrubbing
    pm.setScrubbing(true);
    TEST_ASSERT(pm.isScrubbing());
    pm.setScrubbing(false);
    TEST_ASSERT(!pm.isScrubbing());

    // Step forward & backward
    FrameRate fps{30, 1}; // 4000 ticks per frame
    pm.seek(TimelineTime::fromSeconds(1.0), totalDur);
    pm.stepForward(totalDur, fps);
    TEST_ASSERT(pm.currentTime().ticks() == TimelineTime::fromSeconds(1.0).ticks() + 4000);

    pm.stepBackward(totalDur, fps);
    TEST_ASSERT(pm.currentTime().ticks() == TimelineTime::fromSeconds(1.0).ticks());

    // Reconcile timeline scope
    pm.seek(TimelineTime::fromSeconds(8.0), totalDur);
    pm.play(totalDur);
    // Shorten duration to 5.0 seconds -> should clamp and pause
    pm.reconcileTimelineScope(TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(pm.currentTime().toSeconds() == 5.0);
    TEST_ASSERT(!pm.isPlaying());

    std::cout << "[PASS] runPlaybackManagerTests" << std::endl;
}

void runTimelineManagerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    Project project("Test Project");
    TimelineManager tm(project);

    int changeCount = 0;
    tm.subscribe([&]() { ++changeCount; });

    TEST_ASSERT(tm.getTotalDuration().ticks() == 0);
    TEST_ASSERT(tm.getLastFrameTime().ticks() == 0);

    // Add track
    auto t1 = tm.addTrack(TrackType::Video, "Video 1");
    auto t2 = tm.addTrack(TrackType::Audio, "Audio 1");
    TEST_ASSERT(changeCount == 2);
    TEST_ASSERT(project.isDirty());

    // Toggle mute and visibility
    TEST_ASSERT(tm.toggleTrackMute(t1));
    TEST_ASSERT(tm.toggleTrackVisibility(t1));

    // Insert elements
    Clip c1(ClipId::generate(), ClipType::Video, "Clip 1", TimelineTime(0), TimelineTime::fromSeconds(3.0));
    auto c1Id = c1.id();
    TEST_ASSERT(tm.insertElement(t1, std::move(c1)));
    TEST_ASSERT(tm.getTotalDuration().toSeconds() == 3.0);

    // Duplicate element
    auto dupIds = tm.duplicateElements({ c1Id });
    TEST_ASSERT(dupIds.size() == 1);
    TEST_ASSERT(tm.getTotalDuration().toSeconds() == 6.0);

    // Split element
    TEST_ASSERT(tm.splitElements({ c1Id }, TimelineTime::fromSeconds(1.5)));

    // Move element
    TEST_ASSERT(tm.moveElement(dupIds[0], t1, TimelineTime::fromSeconds(7.0)));
    TEST_ASSERT(tm.getTotalDuration().toSeconds() == 10.0);

    // Last frame time
    FrameRate fps{30, 1};
    auto lastFrame = tm.getLastFrameTime(fps);
    TEST_ASSERT(lastFrame.ticks() == TimelineTime::fromSeconds(10.0).ticks() - fps.frameDuration().ticks());

    // Delete elements
    TEST_ASSERT(tm.deleteElements(dupIds));

    // Remove track
    TEST_ASSERT(tm.removeTrack(t2));

    std::cout << "[PASS] runTimelineManagerTests" << std::endl;
}

void runRendererManagerTests() {
    using namespace catchim::render;
    using namespace catchim::core;
    using namespace catchim::exporting;
    using namespace catchim::editor;

    RendererManager rm;
    TEST_ASSERT(!rm.isDegraded());

    int notifyCount = 0;
    rm.subscribe([&]() { ++notifyCount; });

    rm.setDegraded(true);
    TEST_ASSERT(rm.isDegraded());
    TEST_ASSERT(notifyCount == 1);

    // Snapshot with valid canvas
    auto snap = rm.createSnapshot(TimelineTime::fromSeconds(1.5), CanvasSize{1920, 1080}, "My Project");
    TEST_ASSERT(snap.success);
    TEST_ASSERT(!snap.filename.empty());
    TEST_ASSERT(snap.width == 1920);
    TEST_ASSERT(snap.height == 1080);

    // Snapshot with invalid canvas
    auto snapInvalid = rm.createSnapshot(TimelineTime(0), CanvasSize{0, 0});
    TEST_ASSERT(!snapInvalid.success);

    // Export with empty project
    ExportSettings settings;
    settings.outputPath = "output.mp4";
    auto expEmpty = rm.exportProject(settings, TimelineTime(0));
    TEST_ASSERT(!expEmpty.success);

    // Export with cancellation
    auto expCancel = rm.exportProject(settings, TimelineTime::fromSeconds(5.0), nullptr, []() { return true; });
    TEST_ASSERT(!expCancel.success);
    TEST_ASSERT(expCancel.isCancelled);

    // Export successful with progress
    double lastProgress = 0.0;
    auto expOk = rm.exportProject(
        settings,
        TimelineTime::fromSeconds(5.0),
        [&](double p) { lastProgress = p; }
    );
    TEST_ASSERT(expOk.success);
    TEST_ASSERT(lastProgress == 1.0);
    std::string outP = expOk.outputPath;
    TEST_ASSERT(outP == "output.mp4");
    TEST_ASSERT(expOk.durationSeconds == 5.0);
    TEST_ASSERT(expOk.totalFrames == 150); // 5s * 30fps

    std::cout << "[PASS] runRendererManagerTests" << std::endl;
}

void runSaveManagerTests() {
    using namespace catchim::editor;

    int saveCount = 0;
    SaveManager sm([&]() { ++saveCount; }, 500);

    TEST_ASSERT(sm.debounceMs() == 500);
    TEST_ASSERT(!sm.isDirty());
    TEST_ASSERT(!sm.isPaused());
    TEST_ASSERT(!sm.isSaving());

    // When not running, markDirty does not immediately trigger saveAction
    sm.markDirty();
    TEST_ASSERT(sm.isDirty());
    TEST_ASSERT(saveCount == 0);

    // Start -> markDirty triggers saveNow
    sm.start();
    sm.markDirty();
    TEST_ASSERT(saveCount == 1);
    TEST_ASSERT(!sm.isDirty());

    // Pause -> markDirty does not trigger until resume
    sm.pause();
    TEST_ASSERT(sm.isPaused());
    sm.markDirty(true);
    TEST_ASSERT(saveCount == 1);
    TEST_ASSERT(sm.isDirty());

    sm.resume();
    TEST_ASSERT(!sm.isPaused());
    TEST_ASSERT(saveCount == 2);
    TEST_ASSERT(!sm.isDirty());

    // Flush
    sm.flush();
    TEST_ASSERT(saveCount == 3);

    // Stop
    sm.stop();
    sm.markDirty();
    TEST_ASSERT(saveCount == 3);

    std::cout << "[PASS] runSaveManagerTests" << std::endl;
}

void runMediaManagerTests() {
    using namespace catchim::media;
    using namespace catchim::core;

    MediaManager mm;
    TEST_ASSERT(mm.getAssets().empty());
    TEST_ASSERT(!mm.isLoadingMedia());

    int changeCount = 0;
    mm.subscribe([&]() { ++changeCount; });

    // Add media assets
    MediaAsset a1(MediaId("asset_1"), "/path/video1.mp4", MediaType::Video);
    MediaAsset a2(MediaId("asset_2"), "/path/audio1.mp3", MediaType::Audio);

    TEST_ASSERT(mm.addMediaAsset(a1));
    TEST_ASSERT(mm.addMediaAsset(a2));
    TEST_ASSERT(!mm.addMediaAsset(a1)); // duplicate id returns false
    TEST_ASSERT(mm.getAssets().size() == 2);
    TEST_ASSERT(changeCount == 2);

    // Find asset
    const auto* found = mm.findAsset(MediaId("asset_1"));
    TEST_ASSERT(found != nullptr);
    std::string aName = found->fileName();
    TEST_ASSERT(aName == "video1.mp4");

    const auto* notFound = mm.findAsset(MediaId("asset_none"));
    TEST_ASSERT(notFound == nullptr);

    // Remove single asset
    TEST_ASSERT(mm.removeMediaAsset(MediaId("asset_1")));
    TEST_ASSERT(mm.getAssets().size() == 1);
    TEST_ASSERT(!mm.removeMediaAsset(MediaId("asset_1"))); // already removed

    // Remove batch assets
    MediaAsset a3(MediaId("asset_3"), "/path/img.png", MediaType::Image);
    mm.addMediaAsset(a3);
    size_t removed = mm.removeMediaAssets({ MediaId("asset_2"), MediaId("asset_3") });
    TEST_ASSERT(removed == 2);
    TEST_ASSERT(mm.getAssets().empty());

    // Set assets & clear
    mm.setAssets({ a1, a2, a3 });
    TEST_ASSERT(mm.getAssets().size() == 3);
    mm.clearAllAssets();
    TEST_ASSERT(mm.getAssets().empty());

    // Loading state
    mm.setIsLoading(true);
    TEST_ASSERT(mm.isLoadingMedia());
    mm.setIsLoading(false);
    TEST_ASSERT(!mm.isLoadingMedia());

    std::cout << "[PASS] runMediaManagerTests" << std::endl;
}

void runAudioManagerTests() {
    using namespace catchim::audio;
    using namespace catchim::core;
    using namespace catchim::editor;

    AudioManager am;
    TEST_ASSERT(am.masterVolume() == 1.0);
    TEST_ASSERT(!am.isMuted());
    TEST_ASSERT(!am.hasSoloTracks());

    int notifyCount = 0;
    am.subscribe([&]() { ++notifyCount; });

    // Master volume & mute
    am.setMasterVolume(0.75);
    TEST_ASSERT(am.masterVolume() == 0.75);
    TEST_ASSERT(notifyCount == 1);

    am.setMuted(true);
    TEST_ASSERT(am.isMuted());

    am.setMuted(false);
    TEST_ASSERT(!am.isMuted());

    // Track mute and solo
    TrackId t1("t1");
    TrackId t2("t2");

    am.setTrackMute(t1, true);
    TEST_ASSERT(am.isTrackMuted(t1));
    TEST_ASSERT(!am.isTrackMuted(t2));

    am.setTrackMute(t1, false);
    TEST_ASSERT(!am.isTrackMuted(t1));

    am.setTrackSolo(t2, true);
    TEST_ASSERT(am.isTrackSolo(t2));
    TEST_ASSERT(am.hasSoloTracks());
    TEST_ASSERT(!am.isTrackSolo(t1));

    am.setTrackSolo(t2, false);
    TEST_ASSERT(!am.hasSoloTracks());

    // Collect active audio clips
    Timeline timeline;
    auto track1Id = timeline.addTrack(TrackType::Audio, "Track 1").id();
    auto track2Id = timeline.addTrack(TrackType::Audio, "Track 2").id();

    Clip c1(ClipId("c1"), ClipType::Audio, "Clip 1", TimelineTime(0), TimelineTime::fromSeconds(4.0));
    Clip c2(ClipId("c2"), ClipType::Audio, "Clip 2", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(5.0));

    timeline.addClip(track1Id, std::move(c1));
    timeline.addClip(track2Id, std::move(c2));

    // At time 1.0s, lookahead 2.0s: window is [1.0s, 3.0s]
    // c1 is [0, 4.0] -> active
    // c2 is [2.0, 7.0] -> active (starts at 2.0 <= 3.0)
    auto activeClips = am.collectActiveAudioClips(timeline, TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(activeClips.size() == 2);

    // Track 1 solo -> only track 1 allowed
    am.setTrackSolo(track1Id, true);
    auto soloClips = am.collectActiveAudioClips(timeline, TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(2.0));
    TEST_ASSERT(soloClips.size() == 2);
    // clip on track 2 has effective gain 0 / muted because not soloed
    for (const auto& info : soloClips) {
        if (info.trackId == track1Id) {
            TEST_ASSERT(info.effectiveGain > 0.0);
            TEST_ASSERT(!info.isMuted);
        } else {
            TEST_ASSERT(info.effectiveGain == 0.0);
            TEST_ASSERT(info.isMuted);
        }
    }

    std::cout << "[PASS] runAudioManagerTests" << std::endl;
}

void runTranscriptionCatalogTests() {
    using namespace catchim::subtitles;

    const auto& languages = TranscriptionCatalog::getSupportedLanguages();
    TEST_ASSERT(languages.size() == 10);

    const auto* vi = TranscriptionCatalog::findLanguageByCode("vi");
    TEST_ASSERT(vi != nullptr);
    TEST_ASSERT(vi->name == "Vietnamese");
    TEST_ASSERT(vi->nameVi == "Tiếng Việt");

    const auto* en = TranscriptionCatalog::findLanguageByCode("en");
    TEST_ASSERT(en != nullptr);
    TEST_ASSERT(en->name == "English");
    TEST_ASSERT(en->nameVi == "Tiếng Anh");

    const auto* invalid = TranscriptionCatalog::findLanguageByCode("xx");
    TEST_ASSERT(invalid == nullptr);

    const auto& models = TranscriptionCatalog::getAvailableModels();
    TEST_ASSERT(models.size() == 4);

    const auto* smallModel = TranscriptionCatalog::findModelById("whisper-small");
    TEST_ASSERT(smallModel != nullptr);
    TEST_ASSERT(smallModel->name == "Small");
    TEST_ASSERT(smallModel->huggingFaceId == "onnx-community/whisper-small");

    const auto* turboModel = TranscriptionCatalog::findModelById("whisper-large-v3-turbo");
    TEST_ASSERT(turboModel != nullptr);
    TEST_ASSERT(turboModel->name == "Large v3 Turbo");

    std::string defModel = TranscriptionCatalog::DEFAULT_TRANSCRIPTION_MODEL;
    TEST_ASSERT(defModel == "whisper-small");
    size_t defWords = TranscriptionCatalog::DEFAULT_WORDS_PER_CAPTION;
    TEST_ASSERT(defWords == 3);
    double minDur = TranscriptionCatalog::MIN_CAPTION_DURATION_SECONDS;
    TEST_ASSERT(minDur == 0.8);

    std::cout << "[PASS] runTranscriptionCatalogTests" << std::endl;
}

void runMaskGeometryUtilsTests() {
    using namespace catchim::render;

    // 1. Half-plane sign
    double s1 = MaskGeometryUtils::halfPlaneSign(0.0, 0.0, 0.0, 1.0, 5.0, 10.0);
    TEST_ASSERT(s1 == 10.0);

    double s2 = MaskGeometryUtils::halfPlaneSign(0.0, 0.0, 0.0, 1.0, 5.0, -3.0);
    TEST_ASSERT(s2 == -3.0);

    // 2. Line-edge intersection
    // Line: x = 5 (lineX = 5, lineY = 0, normalX = 1, normalY = 0)
    // Segment from (0, 2) to (10, 2) -> intersects at (5, 2)
    auto inter1 = MaskGeometryUtils::lineEdgeIntersection(5.0, 0.0, 1.0, 0.0, 0.0, 2.0, 10.0, 2.0);
    TEST_ASSERT(inter1.has_value());
    TEST_ASSERT(std::abs(inter1->x - 5.0) < 1e-6);
    TEST_ASSERT(std::abs(inter1->y - 2.0) < 1e-6);

    // Segment from (0, 2) to (4, 2) -> does not reach x = 5
    auto inter2 = MaskGeometryUtils::lineEdgeIntersection(5.0, 0.0, 1.0, 0.0, 0.0, 2.0, 4.0, 2.0);
    TEST_ASSERT(!inter2.has_value());

    // Parallel segment from (0, 2) to (0, 8) with normal (1, 0) -> parallel, distance1 = distance2 = -5, denom = 0
    auto interParallel = MaskGeometryUtils::lineEdgeIntersection(5.0, 0.0, 1.0, 0.0, 0.0, 2.0, 0.0, 8.0);
    TEST_ASSERT(!interParallel.has_value());

    // 3. Feather update
    // startFeather = 50, delta = (11, 0), dir = (1, 0) -> projection = 11 -> deltaFeather = 11 / 0.11 = 100 -> 150
    double f1 = MaskGeometryUtils::computeFeatherUpdate(50.0, 11.0, 0.0, 1.0, 0.0);
    TEST_ASSERT(f1 == 150.0);

    // Clamping to [0, MAX_FEATHER]
    double fMin = MaskGeometryUtils::computeFeatherUpdate(50.0, -110.0, 0.0, 1.0, 0.0);
    TEST_ASSERT(fMin == 0.0);

    double fMax = MaskGeometryUtils::computeFeatherUpdate(950.0, 55.0, 0.0, 1.0, 0.0);
    TEST_ASSERT(fMax == MaskGeometryUtils::MAX_FEATHER);

    std::cout << "[PASS] runMaskGeometryUtilsTests" << std::endl;
}

void runStickerIdUtilsTests() {
    using namespace catchim::media;

    // 1. Parsing valid sticker IDs
    auto p1 = StickerIdUtils::parseStickerId("emoji:fire");
    TEST_ASSERT(p1.providerId == "emoji");
    TEST_ASSERT(p1.providerValue == "fire");

    auto p2 = StickerIdUtils::parseStickerId("  flags  :  vn  ");
    TEST_ASSERT(p2.providerId == "flags");
    TEST_ASSERT(p2.providerValue == "vn");

    // 2. Try parse with invalid formats
    ParsedStickerId out;
    TEST_ASSERT(!StickerIdUtils::tryParseStickerId("", out));
    TEST_ASSERT(!StickerIdUtils::tryParseStickerId("invalid", out));
    TEST_ASSERT(!StickerIdUtils::tryParseStickerId(":value", out));
    TEST_ASSERT(!StickerIdUtils::tryParseStickerId("provider:", out));
    TEST_ASSERT(StickerIdUtils::tryParseStickerId("custom:heart", out));
    TEST_ASSERT(out.providerId == "custom");
    TEST_ASSERT(out.providerValue == "heart");

    // 3. Build sticker ID
    std::string built = StickerIdUtils::buildStickerId("shapes", "star");
    TEST_ASSERT(built == "shapes:star");

    // 4. Categories and fallback size
    const auto& categories = StickerIdUtils::getStickerCategories();
    TEST_ASSERT(categories.size() == 3);
    int fallbackSize = StickerIdUtils::STICKER_INTRINSIC_SIZE_FALLBACK;
    TEST_ASSERT(fallbackSize == 200);

    std::cout << "[PASS] runStickerIdUtilsTests" << std::endl;
}

void runEditorCoreTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    EditorCore::reset();
    auto& core = EditorCore::getInstance();

    // Verify all managers are instantiated and accessible
    TEST_ASSERT(core.project().getActive() != nullptr);
    TEST_ASSERT(!core.playback().isPlaying());
    TEST_ASSERT(!core.renderer().isDegraded());
    TEST_ASSERT(core.save().isDirty() == false);
    TEST_ASSERT(core.media().getAssets().empty());
    TEST_ASSERT(core.audio().masterVolume() == 1.0);
    TEST_ASSERT(core.selection().getSelectedElements().empty());
    TEST_ASSERT(core.clipboard().count() == 0);
    TEST_ASSERT(!core.command().canUndo());

    // Reactor registration and execution
    int reactorRuns = 0;
    core.registerReactor([&]() { ++reactorRuns; });
    core.runReactors();
    TEST_ASSERT(reactorRuns == 1);

    // Adding element triggers timeline and reconciles playback scope
    auto trackId = core.timeline().addTrack(TrackType::Video, "Video 1");
    Clip clip(ClipId::generate(), ClipType::Video, "Test Clip", TimelineTime(0), TimelineTime::fromSeconds(4.0));
    core.timeline().insertElement(trackId, std::move(clip));

    TEST_ASSERT(core.timeline().getTotalDuration().toSeconds() == 4.0);

    // Seek within scope and seek past scope
    core.playback().seek(TimelineTime::fromSeconds(2.0), core.timeline().getTotalDuration());
    TEST_ASSERT(core.playback().currentTime().toSeconds() == 2.0);

    core.playback().seek(TimelineTime::fromSeconds(10.0), core.timeline().getTotalDuration());
    TEST_ASSERT(core.playback().currentTime().toSeconds() == 4.0);

    EditorCore::reset();
    std::cout << "[PASS] runEditorCoreTests" << std::endl;
}

void runSavedSoundsStoreTests() {
    using namespace catchim::media;

    SavedSoundsStore store;
    TEST_ASSERT(store.count() == 0);

    int changeCount = 0;
    store.subscribe([&]() { ++changeCount; });

    SoundEffect s1;
    s1.id = 1001;
    s1.name = "Laser Blast";
    s1.username = "sound_master";
    s1.previewUrl = "https://example.com/laser.mp3";
    s1.duration = 1.25;
    s1.tags = {"sci-fi", "laser", "weapon"};
    s1.license = "CC0";

    SoundEffect s2;
    s2.id = 1002;
    s2.name = "Explosion";
    s2.username = "boom_fx";
    s2.previewUrl = "https://example.com/boom.mp3";
    s2.duration = 3.5;
    s2.tags = {"explosion", "heavy"};
    s2.license = "CC-BY";

    // Save sound
    store.saveSoundEffect(s1);
    TEST_ASSERT(store.count() == 1);
    TEST_ASSERT(store.isSoundSaved(1001));
    TEST_ASSERT(!store.isSoundSaved(1002));
    TEST_ASSERT(changeCount == 1);

    // Duplicate save -> no change
    store.saveSoundEffect(s1);
    TEST_ASSERT(store.count() == 1);
    TEST_ASSERT(changeCount == 1);

    // Toggle sound: toggle existing removes it, toggle new adds it
    store.toggleSavedSound(s2);
    TEST_ASSERT(store.count() == 2);
    TEST_ASSERT(store.isSoundSaved(1002));

    store.toggleSavedSound(s1);
    TEST_ASSERT(store.count() == 1);
    TEST_ASSERT(!store.isSoundSaved(1001));

    // Remove sound
    TEST_ASSERT(store.removeSavedSound(1002));
    TEST_ASSERT(store.count() == 0);
    TEST_ASSERT(!store.removeSavedSound(1002));

    // Clear sounds
    store.saveSoundEffect(s1);
    store.saveSoundEffect(s2);
    TEST_ASSERT(store.count() == 2);
    store.clearSavedSounds();
    TEST_ASSERT(store.count() == 0);

    std::cout << "[PASS] runSavedSoundsStoreTests" << std::endl;
}

void runCanvasSnapMathTests() {
    using namespace catchim::render;

    // 1. Angle snapping
    // Close to 0 deg
    TEST_ASSERT(CanvasSnapMath::snapAngle(2.0, 5.0) == 0.0);
    TEST_ASSERT(CanvasSnapMath::snapAngle(-3.0, 5.0) == 0.0);

    // Close to 90 deg
    TEST_ASSERT(CanvasSnapMath::snapAngle(88.0, 5.0) == 90.0);
    TEST_ASSERT(CanvasSnapMath::snapAngle(93.0, 5.0) == 90.0);

    // Close to 180 deg
    TEST_ASSERT(CanvasSnapMath::snapAngle(177.0, 5.0) == 180.0);

    // Close to 270 deg
    TEST_ASSERT(CanvasSnapMath::snapAngle(272.0, 5.0) == 270.0);

    // Close to 360 deg
    TEST_ASSERT(CanvasSnapMath::snapAngle(358.0, 5.0) == 360.0);

    // Far from 90 multiples -> untouched
    TEST_ASSERT(CanvasSnapMath::snapAngle(45.0, 5.0) == 45.0);
    TEST_ASSERT(CanvasSnapMath::snapAngle(130.0, 5.0) == 130.0);

    // 2. Scalar axis snapping
    TEST_ASSERT(CanvasSnapMath::snapScalar(103.0, 100.0, 5.0) == 100.0);
    TEST_ASSERT(CanvasSnapMath::snapScalar(110.0, 100.0, 5.0) == 110.0);

    // 3. Canvas bounds & center snapping
    // Element 100x100 on canvas 1920x1080 (center is (960, 540))
    // x = 907 -> elemCenter is 957 -> diff to 960 is 3 <= 8 -> snaps to 910
    auto r1 = CanvasSnapMath::snapToCanvas(907.0, 200.0, 100.0, 100.0, 1920.0, 1080.0, 8.0);
    TEST_ASSERT(r1.snappedX);
    TEST_ASSERT(r1.x == 910.0);
    TEST_ASSERT(!r1.snappedY);

    // Left edge: x = 4 -> snaps to 0
    auto r2 = CanvasSnapMath::snapToCanvas(4.0, 200.0, 100.0, 100.0, 1920.0, 1080.0, 8.0);
    TEST_ASSERT(r2.snappedX);
    TEST_ASSERT(r2.x == 0.0);

    // Right edge: x = 1818 -> right is 1918 -> diff to 1920 is 2 <= 8 -> snaps to 1820
    auto r3 = CanvasSnapMath::snapToCanvas(1818.0, 200.0, 100.0, 100.0, 1920.0, 1080.0, 8.0);
    TEST_ASSERT(r3.snappedX);
    TEST_ASSERT(r3.x == 1820.0);

    // Center Y: y = 487 -> elemCenter is 537 -> diff to 540 is 3 <= 8 -> snaps to 490
    auto r4 = CanvasSnapMath::snapToCanvas(200.0, 487.0, 100.0, 100.0, 1920.0, 1080.0, 8.0);
    TEST_ASSERT(!r4.snappedX);
    TEST_ASSERT(r4.snappedY);
    TEST_ASSERT(r4.y == 490.0);

    std::cout << "[PASS] runCanvasSnapMathTests" << std::endl;
}


void runTranscriptionCaptionBuilderTests() {
    using namespace catchim::subtitles;

    std::vector<TranscriptionSegment> segments = {
        {"Hello world this is a test segment for captions", 0.0, 3.5},
        {"Second short chunk", 4.0, 5.0}
    };

    auto chunks = TranscriptionCaptionBuilder::buildCaptionChunks(segments, 3, 0.8);
    TEST_ASSERT(chunks.size() == 4);
    TEST_ASSERT(chunks[0].text == "Hello world this");
    TEST_ASSERT(chunks[0].startTime >= 0.0);
    TEST_ASSERT(chunks[0].duration >= 0.8);
    TEST_ASSERT(chunks[1].text == "is a test");
    TEST_ASSERT(chunks[2].text == "segment for captions");
    TEST_ASSERT(chunks[3].text == "Second short chunk");

    // Empty segments handling
    std::vector<TranscriptionSegment> emptySegments = {
        {"   ", 0.0, 1.0},
        {"", 1.0, 2.0}
    };
    auto emptyChunks = TranscriptionCaptionBuilder::buildCaptionChunks(emptySegments);
    TEST_ASSERT(emptyChunks.empty());

    // Single word minimum duration clamping
    std::vector<TranscriptionSegment> fastSegments = {
        {"Quick", 0.0, 0.1}
    };
    auto fastChunks = TranscriptionCaptionBuilder::buildCaptionChunks(fastSegments, 3, 0.8);
    TEST_ASSERT(fastChunks.size() == 1);
    TEST_ASSERT(fastChunks[0].duration >= 0.8);

    std::cout << "[PASS] runTranscriptionCaptionBuilderTests" << std::endl;
}

void runDiagnosticsManagerTests() {
    using namespace catchim::editor;

    EditorCore::reset();
    auto& core = EditorCore::getInstance();
    DiagnosticsManager diagnostics(core);

    TEST_ASSERT(diagnostics.registrationCount() == 0);

    DiagnosticRegistration reg1;
    reg1.id = "test.error";
    reg1.scope = "test";
    reg1.severity = DiagnosticSeverity::Error;
    reg1.message = "Test error message";
    bool triggerError = false;
    reg1.check = [&](EditorCore&) { return triggerError; };
    diagnostics.registerDiagnostic(std::move(reg1));

    TEST_ASSERT(diagnostics.registrationCount() == 1);
    TEST_ASSERT(diagnostics.getActive().empty());

    triggerError = true;
    auto active = diagnostics.getActive();
    TEST_ASSERT(active.size() == 1);
    TEST_ASSERT(active[0].id == "test.error");
    TEST_ASSERT(active[0].severity == DiagnosticSeverity::Error);

    auto activeTest = diagnostics.getActive(std::string("test"));
    TEST_ASSERT(activeTest.size() == 1);
    auto activeOther = diagnostics.getActive(std::string("other"));
    TEST_ASSERT(activeOther.empty());

    registerTranscriptionDiagnostics(diagnostics);
    TEST_ASSERT(diagnostics.registrationCount() == 2);

    auto transActive = diagnostics.getActive(std::string(TRANSCRIPTION_DIAGNOSTICS_SCOPE));
    TEST_ASSERT(transActive.size() == 1);
    TEST_ASSERT(transActive[0].id == "transcription.no_audio");

    auto audioTrackId = core.timeline().addTrack(TrackType::Audio, "Audio Track");
    Clip audioClip(ClipId::generate(), ClipType::Audio, "Voice", TimelineTime(0), TimelineTime::fromSeconds(2.0));
    core.timeline().insertElement(audioTrackId, std::move(audioClip));

    auto transActiveAfter = diagnostics.getActive(std::string(TRANSCRIPTION_DIAGNOSTICS_SCOPE));
    TEST_ASSERT(transActiveAfter.empty());

    int notifyCount = 0;
    size_t subId = diagnostics.subscribe([&]() { ++notifyCount; });
    diagnostics.notify();
    TEST_ASSERT(notifyCount == 1);
    diagnostics.unsubscribe(subId);
    diagnostics.notify();
    TEST_ASSERT(notifyCount == 1);

    EditorCore::reset();
    std::cout << "[PASS] runDiagnosticsManagerTests" << std::endl;
}

void runGraphicsDefinitionsTests() {
    using namespace catchim::render;

    TEST_ASSERT(GraphicsDefinitions::calculateStrokeInset(GraphicStrokeAlign::Center, 10.0) == 5.0);
    TEST_ASSERT(GraphicsDefinitions::calculateStrokeInset(GraphicStrokeAlign::Inside, 10.0) == 10.0);
    TEST_ASSERT(GraphicsDefinitions::calculateStrokeInset(GraphicStrokeAlign::Outside, 10.0) == 0.0);

    auto rect = GraphicsDefinitions::calculateRectangleBounds(200.0, 100.0, 4.0, GraphicStrokeAlign::Center, 25.0);
    TEST_ASSERT(rect.x == 2.0);
    TEST_ASSERT(rect.y == 2.0);
    TEST_ASSERT(rect.width == 196.0);
    TEST_ASSERT(rect.height == 96.0);
    TEST_ASSERT(rect.cornerRadius > 0.0);

    auto ellipse = GraphicsDefinitions::calculateEllipseBounds(300.0, 150.0, 6.0, GraphicStrokeAlign::Inside);
    TEST_ASSERT(ellipse.centerX == 150.0);
    TEST_ASSERT(ellipse.centerY == 75.0);
    TEST_ASSERT(ellipse.radiusX == 144.0);
    TEST_ASSERT(ellipse.radiusY == 69.0);

    auto pentagon = GraphicsDefinitions::buildPolygonVertices(100.0, 100.0, 50.0, 5);
    TEST_ASSERT(pentagon.size() == 5);
    TEST_ASSERT(std::abs(pentagon[0].x - 100.0) < 1e-4);
    TEST_ASSERT(std::abs(pentagon[0].y - 50.0) < 1e-4);

    auto star = GraphicsDefinitions::buildStarVertices(100.0, 100.0, 5, 50.0, 25.0);
    TEST_ASSERT(star.size() == 10);
    TEST_ASSERT(std::abs(star[0].x - 100.0) < 1e-4);
    TEST_ASSERT(std::abs(star[0].y - 50.0) < 1e-4);

    auto starDepth = GraphicsDefinitions::buildStarVerticesWithDepth(100.0, 100.0, 5, 50.0, 50.0);
    TEST_ASSERT(starDepth.size() == 10);

    std::cout << "[PASS] runGraphicsDefinitionsTests" << std::endl;
}

void runCommandManagerTests() {
    using namespace catchim::editor;

    EditorCore::reset();
    auto& core = EditorCore::getInstance();
    CommandManager cmdManager(core);

    TEST_ASSERT(!cmdManager.canUndo());
    TEST_ASSERT(!cmdManager.canRedo());

    class TestCommand : public EditorCommand {
    public:
        TestCommand(int& val) : val_(val) {}
        bool execute() override { ++val_; return true; }
        bool undo() override { --val_; return true; }
        std::string name() const override { return "TestCommand"; }
    private:
        int& val_;
    };

    int value = 10;
    int reactorRuns = 0;
    cmdManager.registerReactor([&]() { ++reactorRuns; });

    cmdManager.execute(std::make_shared<TestCommand>(value));
    TEST_ASSERT(value == 11);
    TEST_ASSERT(cmdManager.canUndo());
    TEST_ASSERT(!cmdManager.canRedo());
    TEST_ASSERT(reactorRuns == 1);

    cmdManager.undo();
    TEST_ASSERT(value == 10);
    TEST_ASSERT(!cmdManager.canUndo());
    TEST_ASSERT(cmdManager.canRedo());
    TEST_ASSERT(reactorRuns == 2);

    cmdManager.redo();
    TEST_ASSERT(value == 11);
    TEST_ASSERT(cmdManager.canUndo());
    TEST_ASSERT(!cmdManager.canRedo());
    TEST_ASSERT(reactorRuns == 3);

    cmdManager.clear();
    TEST_ASSERT(!cmdManager.canUndo());
    TEST_ASSERT(!cmdManager.canRedo());

    EditorCore::reset();
    std::cout << "[PASS] runCommandManagerTests" << std::endl;
}

void runWaveformCacheTests() {
    using namespace catchim::media;

    auto& cache = WaveformCache::instance();
    cache.clearAll();
    TEST_ASSERT(cache.size() == 0);

    const std::string key = "audio:clip_123";
    TEST_ASSERT(!cache.has(key));
    TEST_ASSERT(!cache.get(key).has_value());

    SourceWaveformSummary summary;
    summary.sourceKey = key;
    summary.sampleRate = 48000;
    summary.totalSamples = 96000;
    summary.bucketSize = 128;
    summary.amplitudes = {0.1f, 0.5f, 0.8f, 0.3f};

    cache.put(key, summary);
    TEST_ASSERT(cache.has(key));
    TEST_ASSERT(cache.size() == 1);

    auto retrieved = cache.get(key);
    TEST_ASSERT(retrieved.has_value());
    TEST_ASSERT(retrieved->sourceKey == key);
    TEST_ASSERT(retrieved->sampleRate == 48000);
    TEST_ASSERT(retrieved->totalSamples == 96000);
    TEST_ASSERT(retrieved->amplitudes.size() == 4);
    TEST_ASSERT(retrieved->amplitudes[2] == 0.8f);

    cache.clearSource(key);
    TEST_ASSERT(!cache.has(key));
    TEST_ASSERT(cache.size() == 0);

    std::cout << "[PASS] runWaveformCacheTests" << std::endl;
}

void runBackgroundBlurPresetsTests() {
    using namespace catchim::render;

    const auto& presets = BackgroundBlurPresets::getPresets();
    TEST_ASSERT(presets.size() == 3);
    TEST_ASSERT(presets[0].label == "Light" && presets[0].value == 100.0);
    TEST_ASSERT(presets[1].label == "Medium" && presets[1].value == 200.0);
    TEST_ASSERT(presets[2].label == "Heavy" && presets[2].value == 500.0);

    auto found = BackgroundBlurPresets::findPresetByLabel("Medium");
    TEST_ASSERT(found.has_value());
    TEST_ASSERT(found->value == 200.0);

    auto notFound = BackgroundBlurPresets::findPresetByLabel("Ultra");
    TEST_ASSERT(!notFound.has_value());

    TEST_ASSERT(BackgroundBlurPresets::isValidIntensity(50.0));
    TEST_ASSERT(!BackgroundBlurPresets::isValidIntensity(-1.0));
    TEST_ASSERT(!BackgroundBlurPresets::isValidIntensity(1001.0));

    TEST_ASSERT(BackgroundBlurPresets::clampIntensity(1500.0) == 1000.0);
    TEST_ASSERT(BackgroundBlurPresets::clampIntensity(-50.0) == 0.0);
    TEST_ASSERT(BackgroundBlurPresets::clampIntensity(250.0) == 250.0);

    double defaultBlur = BackgroundBlurPresets::DEFAULT_BACKGROUND_BLUR_INTENSITY;
    TEST_ASSERT(defaultBlur == 10.0);
    std::string defaultColor = BackgroundBlurPresets::DEFAULT_BACKGROUND_COLOR;
    TEST_ASSERT(defaultColor == "#000000");

    std::cout << "[PASS] runBackgroundBlurPresetsTests" << std::endl;
}

void runPatternCraftGradientsTests() {
    using namespace catchim::render;

    const auto& gradients = PatternCraftGradients::getGradients();
    TEST_ASSERT(gradients.size() >= 22);

    auto grad0 = PatternCraftGradients::findGradientByIndex(0);
    TEST_ASSERT(grad0.has_value());
    TEST_ASSERT(!grad0->empty());

    auto gradOut = PatternCraftGradients::findGradientByIndex(999);
    TEST_ASSERT(!gradOut.has_value());

    const auto& solids = PatternCraftGradients::getSolidColors();
    TEST_ASSERT(solids.size() >= 100);

    TEST_ASSERT(PatternCraftGradients::isValidHexColor("#ffffff"));
    TEST_ASSERT(PatternCraftGradients::isValidHexColor("#000"));
    TEST_ASSERT(PatternCraftGradients::isValidHexColor("#12345678"));
    TEST_ASSERT(!PatternCraftGradients::isValidHexColor("ffffff"));
    TEST_ASSERT(!PatternCraftGradients::isValidHexColor("#xyz"));

    std::cout << "[PASS] runPatternCraftGradientsTests" << std::endl;
}

void runExportOptionsResolverTests() {
    using namespace catchim::exporting;
    using namespace catchim::editor;

    TEST_ASSERT(ExportOptionsResolver::roundToEven(3.1) == 4);
    TEST_ASSERT(ExportOptionsResolver::roundToEven(4.0) == 4);
    TEST_ASSERT(ExportOptionsResolver::roundToEven(1.2) == 2);

    TEST_ASSERT(ExportOptionsResolver::getResolutionHeight("1080p") == 1080);
    TEST_ASSERT(ExportOptionsResolver::getResolutionHeight("720p") == 720);
    TEST_ASSERT(ExportOptionsResolver::getResolutionHeight("source") == 0);

    CanvasSize source1080{1920, 1080};
    auto res720 = ExportOptionsResolver::resolveExportCanvasSize(source1080, "720p");
    TEST_ASSERT(res720.width == 1280);
    TEST_ASSERT(res720.height == 720);

    auto resSource = ExportOptionsResolver::resolveExportCanvasSize(source1080, "source");
    TEST_ASSERT(resSource.width == 1920);
    TEST_ASSERT(resSource.height == 1080);

    TEST_ASSERT(ExportOptionsResolver::getExportMimeType(ExportFormat::MP4) == "video/mp4");
    TEST_ASSERT(ExportOptionsResolver::getExportMimeType(ExportFormat::WebM) == "video/webm");
    TEST_ASSERT(ExportOptionsResolver::getExportFileExtension(ExportFormat::MP4) == ".mp4");
    TEST_ASSERT(ExportOptionsResolver::getExportFileExtension(ExportFormat::WebM) == ".webm");

    const auto& resList = ExportOptionsResolver::getSupportedResolutions();
    TEST_ASSERT(!resList.empty());

    std::cout << "[PASS] runExportOptionsResolverTests" << std::endl;
}

void runTranscriptionServiceTests() {
    using namespace catchim::subtitles;

    TranscriptionService service;
    TEST_ASSERT(service.currentModel() == "whisper-small");

    service.setModel("whisper-tiny");
    TEST_ASSERT(service.currentModel() == "whisper-tiny");

    // Empty audio data
    int progressCalls = 0;
    auto resultEmpty = service.transcribe({}, "en", "whisper-tiny", [&](const TranscriptionProgress& p) {
        ++progressCalls;
        TEST_ASSERT(p.status == TranscriptionStatus::LoadingModel || p.status == TranscriptionStatus::Complete);
    });
    TEST_ASSERT(resultEmpty.segments.empty());
    TEST_ASSERT(progressCalls > 0);

    // Audio data with cancel test
    service.cancel();
    TEST_ASSERT(service.isCancelled());
    auto resultCancel = service.transcribe({0.1f, 0.2f, 0.3f}, "vi", "whisper-small");
    TEST_ASSERT(resultCancel.segments.empty());

    // Successful transcription simulation
    service.reset();
    TEST_ASSERT(!service.isCancelled());
    std::vector<float> audioData(16000, 0.05f); // 1 second of audio
    auto result = service.transcribe(audioData, "en", "whisper-small");
    TEST_ASSERT(!result.text.empty());
    TEST_ASSERT(result.segments.size() == 1);
    TEST_ASSERT(result.segments[0].end >= 1.0);

    std::cout << "[PASS] runTranscriptionServiceTests" << std::endl;
}

void runTtsVoiceServiceTests() {
    using namespace catchim::media;

    const auto& voices = TtsVoiceService::getAllVoices();
    TEST_ASSERT(voices.size() >= 5);

    auto viVoices = TtsVoiceService::filterByLanguage("vi");
    TEST_ASSERT(!viVoices.empty());
    TEST_ASSERT(viVoices[0].language.rfind("vi", 0) == 0);

    auto femaleVoices = TtsVoiceService::filterByGender("female");
    TEST_ASSERT(!femaleVoices.empty());
    TEST_ASSERT(femaleVoices[0].gender == "female");

    auto funVoices = TtsVoiceService::filterByCategory("fun");
    TEST_ASSERT(!funVoices.empty());

    auto found = TtsVoiceService::findVoiceById("vi-female-sweet");
    TEST_ASSERT(found.has_value());
    TEST_ASSERT(!found->name.empty());

    auto notFound = TtsVoiceService::findVoiceById("non-existent-voice");
    TEST_ASSERT(!notFound.has_value());

    TEST_ASSERT(TtsVoiceService::validateOptions(1.0, 0.0, 1.0));
    TEST_ASSERT(!TtsVoiceService::validateOptions(0.1, 0.0, 1.0)); // speed too low
    TEST_ASSERT(!TtsVoiceService::validateOptions(1.0, 100.0, 1.0)); // pitch too high
    TEST_ASSERT(!TtsVoiceService::validateOptions(1.0, 0.0, 1.5)); // volume too high

    std::cout << "[PASS] runTtsVoiceServiceTests" << std::endl;
}

void runPanelLayoutConfigTests() {
    using namespace catchim::editor;

    auto ratios = PanelLayoutConfig::getDefaultRatios();
    TEST_ASSERT(ratios.tools == 25.0);
    TEST_ASSERT(ratios.preview == 50.0);
    TEST_ASSERT(ratios.properties == 25.0);
    TEST_ASSERT(ratios.mainContent == 50.0);
    TEST_ASSERT(ratios.timeline == 50.0);

    TEST_ASSERT(PanelLayoutConfig::isValidRatio(25.0));
    TEST_ASSERT(PanelLayoutConfig::isValidRatio(0.0));
    TEST_ASSERT(PanelLayoutConfig::isValidRatio(100.0));
    TEST_ASSERT(!PanelLayoutConfig::isValidRatio(-5.0));
    TEST_ASSERT(!PanelLayoutConfig::isValidRatio(105.0));

    TEST_ASSERT(PanelLayoutConfig::clampPanelSize(30.0, 10.0, 50.0) == 30.0);
    TEST_ASSERT(PanelLayoutConfig::clampPanelSize(5.0, 10.0, 50.0) == 10.0);
    TEST_ASSERT(PanelLayoutConfig::clampPanelSize(60.0, 10.0, 50.0) == 50.0);

    std::cout << "[PASS] runPanelLayoutConfigTests" << std::endl;
}

void runAudioMediaUtilsTests() {
    using namespace catchim::audio;
    using namespace catchim::editor;
    using namespace catchim::core;

    // Stereo downmixing
    float left[4] = {1.0f, -0.5f, 0.0f, 0.8f};
    float right[4] = {0.0f, 0.5f, -0.4f, 0.2f};
    float out[4] = {0.0f};

    AudioMediaUtils::downmixStereo(left, right, out, 4);
    TEST_ASSERT(std::abs(out[0] - 0.5f) < 1e-4f);
    TEST_ASSERT(std::abs(out[1] - 0.0f) < 1e-4f);
    TEST_ASSERT(std::abs(out[2] - (-0.2f)) < 1e-4f);
    TEST_ASSERT(std::abs(out[3] - 0.5f) < 1e-4f);

    // dB and linear conversions
    TEST_ASSERT(std::abs(AudioMediaUtils::dBToLinear(0.0) - 1.0) < 1e-4);
    TEST_ASSERT(std::abs(AudioMediaUtils::dBToLinear(-6.0206) - 0.5) < 1e-3);
    TEST_ASSERT(std::abs(AudioMediaUtils::linearToDb(1.0) - 0.0) < 1e-4);
    TEST_ASSERT(std::abs(AudioMediaUtils::linearToDb(0.5) - (-6.0206)) < 1e-3);
    TEST_ASSERT(AudioMediaUtils::linearToDb(0.0) <= -100.0);

    // Timeline audio presence
    Timeline emptyTimeline;
    TEST_ASSERT(!AudioMediaUtils::timelineHasAudio(emptyTimeline));

    auto& track = emptyTimeline.addTrack(TrackType::Audio, "Voiceover");
    Clip audioClip(ClipId::generate(), ClipType::Audio, "Vocal", TimelineTime(0), TimelineTime::fromSeconds(3.0));
    emptyTimeline.addClip(track.id(), std::move(audioClip));

    TEST_ASSERT(AudioMediaUtils::timelineHasAudio(emptyTimeline));
    auto audible = AudioMediaUtils::collectAudibleClips(emptyTimeline);
    TEST_ASSERT(audible.size() == 1);

    std::cout << "[PASS] runAudioMediaUtilsTests" << std::endl;
}

void runSeekControllerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    SeekConfig config;
    config.zoomLevel = 2.0;
    config.duration = TimelineTime::fromSeconds(10.0);
    config.activeProjectFps = FrameRate{30, 1};

    bool clearedSelection = false;
    config.clearSelectedElements = [&]() { clearedSelection = true; };

    TimelineTime seekTarget{0};
    config.seek = [&](TimelineTime t) { seekTarget = t; };

    TimelineViewState viewState;
    config.setTimelineViewState = [&](const TimelineViewState& vs) { viewState = vs; };

    SeekController controller(config);
    TEST_ASSERT(!controller.isPending());

    // 1. Pixel to time
    // 50px/sec * 2.0 zoom = 100 px/sec
    auto t1 = SeekController::pixelToTime(100.0, 0.0, 0.0, 2.0, TimelineTime::fromSeconds(10.0));
    TEST_ASSERT(std::abs(t1.toSeconds() - 1.0) < 1e-3);

    // 2. Click gesture detection
    PendingSeekSession session{SeekSource::Tracks, 100.0, 50.0, 1000};
    TEST_ASSERT(SeekController::isClickGesture(102.0, 51.0, 1200, session));
    TEST_ASSERT(!SeekController::isClickGesture(120.0, 50.0, 1200, session)); // moved too far
    TEST_ASSERT(!SeekController::isClickGesture(100.0, 50.0, 1600, session)); // too slow (> 500ms)

    // 3. Mouse down & click interaction
    controller.onMouseDown(SeekSource::Tracks, 150.0, 40.0, 2000);
    TEST_ASSERT(controller.isPending());

    bool handled = controller.onClick(SeekSource::Tracks, 151.0, 41.0, 2100, 0.0, 0.0);
    TEST_ASSERT(handled);
    TEST_ASSERT(!controller.isPending());
    TEST_ASSERT(clearedSelection);
    TEST_ASSERT(seekTarget > TimelineTime(0));
    TEST_ASSERT(viewState.playheadTime == seekTarget);

    std::cout << "[PASS] runSeekControllerTests" << std::endl;
}

void runPlayheadControllerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    PlayheadConfig config;
    config.zoomLevel = 1.0;
    config.duration = TimelineTime::fromSeconds(20.0);
    config.activeProjectFps = FrameRate{30, 1};

    TimelineTime currentSeek{0};
    config.seek = [&](TimelineTime t) { currentSeek = t; };

    bool isScrubbing = false;
    config.setScrubbing = [&](bool s) { isScrubbing = s; };

    config.snapPointsProvider = [&]() -> std::vector<TimelineTime> {
        return {TimelineTime::fromSeconds(5.0), TimelineTime::fromSeconds(10.0)};
    };

    PlayheadController controller(config);
    TEST_ASSERT(!controller.isScrubbing());

    // 1. Pixel to time
    auto pt = PlayheadController::pixelToTime(250.0, 0.0, 1.0, TimelineTime::fromSeconds(20.0));
    TEST_ASSERT(std::abs(pt.toSeconds() - 5.0) < 1e-3);

    // 2. Playhead scrub interaction
    controller.onPlayheadMouseDown(100.0, 0.0);
    TEST_ASSERT(controller.isScrubbing());
    TEST_ASSERT(isScrubbing);

    controller.handleMouseMove(248.0, 0.0); // Near 250px (5.0s), should snap to 5.0s
    TEST_ASSERT(std::abs(currentSeek.toSeconds() - 5.0) < 1e-3);

    controller.handleMouseUp(248.0, 0.0, 0.0);
    TEST_ASSERT(!controller.isScrubbing());
    TEST_ASSERT(!isScrubbing);

    // 3. Playback auto-scroll
    config.isPlaying = true;
    controller.setConfig(config);
    double scrollLeft = 0.0;
    // Playhead at 15s (750px), viewport 500px, content 1000px
    bool scrolled = controller.handlePlaybackUpdate(TimelineTime::fromSeconds(15.0), 500.0, 1000.0, scrollLeft);
    TEST_ASSERT(scrolled);
    TEST_ASSERT(scrollLeft > 0.0);

    std::cout << "[PASS] runPlayheadControllerTests" << std::endl;
}

void runKeyframeDragControllerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    KeyframeDragConfig config;
    config.zoomLevel = 1.0;
    config.fps = FrameRate{30, 1};
    config.elementDuration = TimelineTime::fromSeconds(5.0);
    config.displayedStartTime = TimelineTime::fromSeconds(2.0);

    std::vector<std::string> committedIds;
    int64_t committedDelta = 0;
    config.commitDrag = [&](const std::vector<std::string>& ids, int64_t delta) {
        committedIds = ids;
        committedDelta = delta;
    };

    KeyframeDragController controller(config);
    TEST_ASSERT(!controller.isActive());

    // 1. Clamping helper
    auto clamped1 = KeyframeDragController::calculateClampedTime(
        TimelineTime::fromSeconds(2.0),
        TimelineTime::fromSeconds(1.0).ticks(),
        TimelineTime::fromSeconds(5.0)
    );
    TEST_ASSERT(std::abs(clamped1.toSeconds() - 3.0) < 1e-3);

    auto clamped2 = KeyframeDragController::calculateClampedTime(
        TimelineTime::fromSeconds(2.0),
        TimelineTime::fromSeconds(10.0).ticks(),
        TimelineTime::fromSeconds(5.0)
    );
    TEST_ASSERT(std::abs(clamped2.toSeconds() - 5.0) < 1e-3);

    // 2. Mouse down and drag
    controller.onKeyframeMouseDown({"kf-1", "kf-2"}, 100.0);
    TEST_ASSERT(controller.isActive());
    TEST_ASSERT(!controller.dragState().isDragging); // Still pending

    // Move past threshold (5px)
    controller.handleMouseMove(160.0); // +60px = +1.2s
    TEST_ASSERT(controller.dragState().isDragging);
    TEST_ASSERT(controller.dragState().draggingKeyframeIds.size() == 2);
    TEST_ASSERT(controller.dragState().deltaTicks > 0);

    controller.handleMouseUp();
    TEST_ASSERT(!controller.isActive());
    TEST_ASSERT(committedIds.size() == 2);
    TEST_ASSERT(committedDelta > 0);

    std::cout << "[PASS] runKeyframeDragControllerTests" << std::endl;
}

void runResizeControllerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    ResizeConfig config;
    config.zoomLevel = 1.0;
    config.fps = FrameRate{30, 1};
    config.snappingEnabled = true;

    std::vector<GroupResizeUpdate> previewUpdates;
    config.previewElements = [&](const std::vector<GroupResizeUpdate>& u) { previewUpdates = u; };

    std::vector<GroupResizeUpdate> committedUpdates;
    config.commitElements = [&](const std::vector<GroupResizeUpdate>& u) { committedUpdates = u; };

    ResizeController controller(config);
    TEST_ASSERT(!controller.isResizing());

    GroupResizeMember member;
    member.trackId = TrackId("track-1");
    member.elementId = ClipId("clip-10");
    member.startTime = TimelineTime::fromSeconds(2.0);
    member.duration = TimelineTime::fromSeconds(4.0);
    member.trimStart = TimelineTime(0);
    member.trimEnd = TimelineTime(0);

    controller.onResizeStart(ResizeSide::Right, 300.0, {member});
    TEST_ASSERT(controller.isResizing());

    // Move right by 50px (+1.0s)
    controller.handleMouseMove(350.0);
    TEST_ASSERT(!previewUpdates.empty());

    controller.handleMouseUp();
    TEST_ASSERT(!controller.isResizing());
    TEST_ASSERT(!committedUpdates.empty());

    std::cout << "[PASS] runResizeControllerTests" << std::endl;
}

void runSelectionStateEngineTests() {
    using namespace catchim::editor;

    // 1. Deduplication
    auto deduped = SelectionStateEngine::dedupeIds({"a", "b", "a", "c", "b"});
    TEST_ASSERT(deduped.size() == 3);
    TEST_ASSERT(deduped[0] == "a" && deduped[1] == "b" && deduped[2] == "c");

    // 2. Replace & Clear
    auto state = SelectionStateEngine::replaceSelection({"item-1", "item-2"});
    TEST_ASSERT(state.selectedIds.size() == 2);
    TEST_ASSERT(state.anchorId.has_value() && *state.anchorId == "item-2");

    auto emptyState = SelectionStateEngine::clearSelection();
    TEST_ASSERT(emptyState.selectedIds.empty());
    TEST_ASSERT(!emptyState.anchorId.has_value());

    // 3. Prune
    auto pruned = SelectionStateEngine::pruneSelection(state, {"item-2", "item-3"});
    TEST_ASSERT(pruned.selectedIds.size() == 1);
    TEST_ASSERT(pruned.selectedIds[0] == "item-2");
    TEST_ASSERT(pruned.anchorId.has_value() && *pruned.anchorId == "item-2");

    // 4. Toggle
    auto toggled = SelectionStateEngine::toggleSelection(pruned, "item-4");
    TEST_ASSERT(toggled.selectedIds.size() == 2);
    TEST_ASSERT(SelectionStateEngine::isSelected(toggled, "item-4"));

    auto untoggled = SelectionStateEngine::toggleSelection(toggled, "item-4");
    TEST_ASSERT(untoggled.selectedIds.size() == 1);
    TEST_ASSERT(!SelectionStateEngine::isSelected(untoggled, "item-4"));

    // 5. Select range
    std::vector<std::string> ordered = {"x1", "x2", "x3", "x4", "x5"};
    SelectionState rangeBase = SelectionStateEngine::replaceSelection({"x2"});
    auto rangeSelected = SelectionStateEngine::selectRange(rangeBase, ordered, "x4", false);
    TEST_ASSERT(rangeSelected.selectedIds.size() == 3); // x2, x3, x4

    // 6. Box selection
    BoxSelectionChange boxChange{
        .intersectedIds = {"x3", "x4"},
        .initialSelectedIds = {"x1"},
        .initialAnchorId = "x1",
        .isAdditive = true
    };
    auto boxState = SelectionStateEngine::applyBoxSelection(boxChange);
    TEST_ASSERT(boxState.selectedIds.size() == 3);

    // 7. Scope management
    bool cleared = false;
    SelectionStateEngine::activateScope(ScopeEntry{
        .hasSelection = []() { return true; },
        .clear = [&]() { cleared = true; },
        .clearActive = [&]() { cleared = true; }
    });
    TEST_ASSERT(SelectionStateEngine::hasActiveScopeSelection());
    TEST_ASSERT(SelectionStateEngine::clearActiveScope());
    TEST_ASSERT(cleared);
    SelectionStateEngine::resetActiveScope();

    std::cout << "[PASS] runSelectionStateEngineTests" << std::endl;
}

void runTimelineCreationDefaultsTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    TEST_ASSERT(TimelineCreationDefaults::defaultNewElementDuration() == TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(TimelineCreationDefaults::toElementDurationTicks(std::nullopt) == TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(TimelineCreationDefaults::toElementDurationTicks(2.5) == TimelineTime::fromSeconds(2.5));

    TEST_ASSERT(TimelineCreationDefaults::getDefaultTrackName(TrackType::Video) == "Video track");
    TEST_ASSERT(TimelineCreationDefaults::getDefaultTrackName(TrackType::Audio) == "Audio track");
    TEST_ASSERT(TimelineCreationDefaults::getDefaultTrackName(TrackType::Text) == "Text track");
    TEST_ASSERT(TimelineCreationDefaults::getDefaultTrackName(TrackType::Graphic) == "Graphic track");
    TEST_ASSERT(TimelineCreationDefaults::getDefaultTrackName(TrackType::Effect) == "Effect track");
    TEST_ASSERT(TimelineCreationDefaults::getDefaultTrackName("graphic") == "Graphic track");

    TEST_ASSERT(TimelineCreationDefaults::isVolumeDbValid(0.0));
    TEST_ASSERT(TimelineCreationDefaults::isVolumeDbValid(-60.0));
    TEST_ASSERT(TimelineCreationDefaults::isVolumeDbValid(20.0));
    TEST_ASSERT(!TimelineCreationDefaults::isVolumeDbValid(-65.0));
    TEST_ASSERT(!TimelineCreationDefaults::isVolumeDbValid(25.0));

    TEST_ASSERT(TimelineCreationDefaults::clampVolumeDb(30.0) == 20.0);
    TEST_ASSERT(TimelineCreationDefaults::clampVolumeDb(-100.0) == -60.0);

    TEST_ASSERT(TimelineCreationDefaults::isZoomLevelValid(1.0));
    TEST_ASSERT(!TimelineCreationDefaults::isZoomLevelValid(0.05));
    TEST_ASSERT(!TimelineCreationDefaults::isZoomLevelValid(150.0));

    std::cout << "[PASS] runTimelineCreationDefaultsTests" << std::endl;
}

void runTimelineDropTargetResolverTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    Timeline timeline;
    auto& vTrack = timeline.addTrack(TrackType::Video, "Main Video");
    Clip clip1(ClipId::generate(), ClipType::Video, "Clip1", TimelineTime(0), TimelineTime::fromSeconds(4.0));
    timeline.addClip(vTrack.id(), std::move(clip1));

    // 1. Get track at Y
    auto trackAtY = TimelineDropTargetResolver::getTrackAtY(20.0, timeline);
    TEST_ASSERT(trackAtY.has_value());
    TEST_ASSERT(trackAtY->trackIndex == 0);

    // 2. Find element at position
    auto found = TimelineDropTargetResolver::findElementAtPosition(
        50.0, // 50px = 1.0s
        timeline,
        0,
        {ClipType::Video},
        50.0,
        1.0
    );
    TEST_ASSERT(found.has_value());
    TEST_ASSERT(found->trackId == vTrack.id());

    // 3. Compute drop target
    ComputeDropTargetParams params;
    params.clipType = ClipType::Video;
    params.mouseX = 300.0; // 6.0s (after Clip1)
    params.mouseY = 20.0;
    params.elementDuration = TimelineTime::fromSeconds(3.0);
    params.pixelsPerSecond = 50.0;
    params.zoomLevel = 1.0;

    auto dropTarget = TimelineDropTargetResolver::computeDropTarget(timeline, params);
    TEST_ASSERT(!dropTarget.isNewTrack);
    TEST_ASSERT(dropTarget.trackIndex == 0);
    TEST_ASSERT(dropTarget.xPosition == TimelineTime::fromSeconds(6.0));

    // 4. Drop line Y
    double dropY0 = TimelineDropTargetResolver::getDropLineY(dropTarget, timeline);
    TEST_ASSERT(dropY0 == 0.0);

    dropTarget.trackIndex = 1;
    double dropY1 = TimelineDropTargetResolver::getDropLineY(dropTarget, timeline);
    TEST_ASSERT(dropY1 > 0.0);

    std::cout << "[PASS] runTimelineDropTargetResolverTests" << std::endl;
}

void runTimelineDragDropControllerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    Timeline timeline;
    auto& track = timeline.addTrack(TrackType::Video, "Video Track");
    Clip clip(ClipId::generate(), ClipType::Video, "Base", TimelineTime(0), TimelineTime::fromSeconds(5.0));
    timeline.addClip(track.id(), std::move(clip));

    bool dropped = false;
    DropTarget receivedTarget;
    ClipType receivedType = ClipType::Video;

    DragDropConfig config;
    config.zoomLevel = 1.0;
    config.playheadTime = TimelineTime(0);
    config.onDropElement = [&](const DropTarget& dt, ClipType ct) {
        dropped = true;
        receivedTarget = dt;
        receivedType = ct;
    };

    TimelineDragDropController controller(config);
    TEST_ASSERT(!controller.isOver());

    controller.onDragEnter(ClipType::Text);
    TEST_ASSERT(controller.isOver());

    controller.onDragOver(timeline, 300.0, 10.0, ClipType::Text, TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(controller.isOver());
    TEST_ASSERT(controller.state().dropTarget.has_value());

    controller.onDrop(timeline, 300.0, 10.0, ClipType::Text, TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(!controller.isOver());
    TEST_ASSERT(dropped);
    TEST_ASSERT(receivedType == ClipType::Text);

    std::cout << "[PASS] runTimelineDragDropControllerTests" << std::endl;
}

void runTimelineElementInteractionControllerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    bool movesCommitted = false;
    TimelineTime committedDelta{0};

    ElementInteractionConfig config;
    config.zoomLevel = 1.0;
    config.onCommitMoves = [&](const std::vector<ClipId>& /*ids*/, TimelineTime delta) {
        movesCommitted = true;
        committedDelta = delta;
    };

    TimelineElementInteractionController controller(config);
    TEST_ASSERT(!controller.hasSession());

    TrackId tId("t-1");
    ClipId cId("c-1");

    // 1. Mouse down (start drag session)
    controller.onMouseDown(tId, cId, TimelineTime::fromSeconds(2.0), 100.0, 50.0, {cId});
    TEST_ASSERT(controller.hasSession());
    TEST_ASSERT(!controller.isDragging());

    // 2. Move past 5px threshold
    controller.handleMouseMove(150.0, 50.0); // +50px = +1.0s
    TEST_ASSERT(controller.isDragging());
    TEST_ASSERT(std::abs(controller.currentDeltaTime().toSeconds() - 1.0) < 1e-3);

    // 3. Mouse up commits move
    controller.handleMouseUp(150.0, 50.0, false);
    TEST_ASSERT(!controller.hasSession());
    TEST_ASSERT(!controller.isDragging());
    TEST_ASSERT(movesCommitted);
    TEST_ASSERT(std::abs(committedDelta.toSeconds() - 1.0) < 1e-3);

    std::cout << "[PASS] runTimelineElementInteractionControllerTests" << std::endl;
}

void runTimelineInteractionMetricsTests() {
    using namespace catchim::editor;

    double dragThreshold = TimelineInteractionMetrics::TIMELINE_DRAG_THRESHOLD_PX;
    TEST_ASSERT(dragThreshold == 5.0);
    double wheelStep = TimelineInteractionMetrics::TIMELINE_HORIZONTAL_WHEEL_STEP_PX;
    TEST_ASSERT(wheelStep == 40.0);

    double zoomedIn = TimelineInteractionMetrics::calculateZoomIn(1.0);
    TEST_ASSERT(std::abs(zoomedIn - 1.7) < 1e-4);

    double zoomedOut = TimelineInteractionMetrics::calculateZoomOut(1.7);
    TEST_ASSERT(std::abs(zoomedOut - 1.0) < 1e-4);

    TEST_ASSERT(TimelineInteractionMetrics::isDragGesture(0.0, 0.0, 6.0, 0.0));
    TEST_ASSERT(!TimelineInteractionMetrics::isDragGesture(0.0, 0.0, 2.0, 2.0));

    TEST_ASSERT(TimelineInteractionMetrics::calculateWheelScrollDelta(10.0) == 40.0);
    TEST_ASSERT(TimelineInteractionMetrics::calculateWheelScrollDelta(-10.0) == -40.0);

    std::cout << "[PASS] runTimelineInteractionMetricsTests" << std::endl;
}

void runTimelineElementFactoryTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    auto graphic = TimelineElementFactory::buildGraphicElement(ClipId("g-1"), "circle", TimelineTime(0), TimelineTime::fromSeconds(3.0));
    TEST_ASSERT(graphic.type() == ClipType::Graphic);
    TEST_ASSERT(graphic.duration() == TimelineTime::fromSeconds(3.0));

    auto sticker = TimelineElementFactory::buildStickerElement(ClipId("s-1"), "star_badge");
    TEST_ASSERT(sticker.type() == ClipType::Sticker);

    auto effect = TimelineElementFactory::buildEffectElement(ClipId("e-1"), "gaussian_blur");
    TEST_ASSERT(effect.type() == ClipType::Effect);

    // Introspection
    TEST_ASSERT(TimelineElementFactory::canElementHaveAudio(ClipType::Video));
    TEST_ASSERT(TimelineElementFactory::canElementHaveAudio(ClipType::Audio));
    TEST_ASSERT(!TimelineElementFactory::canElementHaveAudio(ClipType::Text));

    TEST_ASSERT(TimelineElementFactory::isVisualElement(ClipType::Video));
    TEST_ASSERT(TimelineElementFactory::isVisualElement(ClipType::Image));
    TEST_ASSERT(TimelineElementFactory::isVisualElement(ClipType::Text));
    TEST_ASSERT(!TimelineElementFactory::isVisualElement(ClipType::Audio));

    TEST_ASSERT(TimelineElementFactory::isMaskableElement(ClipType::Video));
    TEST_ASSERT(!TimelineElementFactory::isMaskableElement(ClipType::Audio));

    TEST_ASSERT(TimelineElementFactory::isRetimableElement(ClipType::Video));
    TEST_ASSERT(TimelineElementFactory::isRetimableElement(ClipType::Audio));
    TEST_ASSERT(!TimelineElementFactory::isRetimableElement(ClipType::Text));

    TEST_ASSERT(TimelineElementFactory::requiresMediaId(ClipType::Video));
    TEST_ASSERT(!TimelineElementFactory::requiresMediaId(ClipType::Text));

    std::cout << "[PASS] runTimelineElementFactoryTests" << std::endl;
}

void runTimelineDragUtilsTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // 50px/sec at zoom 1.0: 100px = 2.0s
    auto t1 = TimelineDragUtils::getMouseTimeFromClientX(150.0, 50.0, 0.0, 1.0);
    TEST_ASSERT(std::abs(t1.toSeconds() - 2.0) < 1e-4);

    // Frame-snapped at 30 fps
    FrameRate fps{30, 1};
    auto t2 = TimelineDragUtils::getMouseTimeSnapped(151.0, 50.0, 0.0, 1.0, fps);
    TEST_ASSERT(t2 >= TimelineTime(0));

    // Clamping
    auto clamped = TimelineDragUtils::clampTimeToDuration(
        TimelineTime::fromSeconds(15.0),
        TimelineTime::fromSeconds(10.0)
    );
    TEST_ASSERT(clamped == TimelineTime::fromSeconds(10.0));

    std::cout << "[PASS] runTimelineDragUtilsTests" << std::endl;
}

void runTimelineInteractiveZoomControllerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    InteractiveZoomConfig cfg;
    cfg.minZoom = 0.1;
    cfg.maxZoom = 100.0;
    cfg.getCurrentPlayheadTime = []() { return TimelineTime::fromSeconds(2.0); };

    TimelineInteractiveZoomController controller(cfg, 1.0);
    TEST_ASSERT(controller.zoomLevel() == 1.0);

    // Clamping
    TEST_ASSERT(TimelineInteractiveZoomController::clampZoom(150.0, 0.1, 100.0) == 100.0);
    TEST_ASSERT(TimelineInteractiveZoomController::clampZoom(0.01, 0.1, 100.0) == 0.1);

    // Wheel zoom
    bool handled = controller.handleWheel(0.0, -10.0, true, false, 5000.0, 1000.0);
    TEST_ASSERT(handled);
    TEST_ASSERT(controller.zoomLevel() > 1.0);

    // Shift scroll should be ignored by zoom
    bool ignored = controller.handleWheel(0.0, -10.0, true, true);
    TEST_ASSERT(!ignored);

    // Threshold anchoring
    controller.setZoomLevel(0.2, 5000.0, 1000.0);
    TEST_ASSERT(!controller.isInPlayheadAnchorMode());
    controller.setZoomLevel(0.8, 5000.0, 1000.0);
    TEST_ASSERT(controller.isInPlayheadAnchorMode());

    std::cout << "[PASS] runTimelineInteractiveZoomControllerTests" << std::endl;
}

void runGraphEditorSessionEngineTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    Timeline timeline;
    auto& track = timeline.addTrack(TrackType::Video, "Video Track");
    Clip clip(ClipId("c-1"), ClipType::Video, "Clip 1", TimelineTime(0), TimelineTime::fromSeconds(10.0));

    // Add animation channel
    AnimationChannel channel("transform.positionX", 0.0);
    Keyframe kf1{TimelineTime(0), 0.0, KeyframeInterpolation::Bezier};
    Keyframe kf2{TimelineTime::fromSeconds(2.0), 100.0, KeyframeInterpolation::Bezier};
    Keyframe kf3{TimelineTime::fromSeconds(4.0), 100.0, KeyframeInterpolation::Linear};
    channel.addOrUpdateKeyframe(kf1);
    channel.addOrUpdateKeyframe(kf2);
    channel.addOrUpdateKeyframe(kf3);
    clip.animationChannels()["transform.positionX"] = std::move(channel);

    timeline.addClip(track.id(), std::move(clip));

    // 1. Empty selection -> NoKeyframeSelected
    auto stateEmpty = GraphEditorSessionEngine::resolveSelectionState(timeline, {});
    TEST_ASSERT(!stateEmpty.isReady);
    TEST_ASSERT(stateEmpty.reason == GraphEditorUnavailableReason::NoKeyframeSelected);

    // 2. Valid selection
    SelectedKeyframeRef ref{track.id(), ClipId("c-1"), "transform.positionX", TimelineTime(0)};
    std::vector<SelectedKeyframeRef> selectedKeyframes = {ref};
    auto stateReady = GraphEditorSessionEngine::resolveSelectionState(timeline, selectedKeyframes);
    TEST_ASSERT(stateReady.isReady);
    TEST_ASSERT(stateReady.reason == GraphEditorUnavailableReason::None);
    TEST_ASSERT(stateReady.segments.size() == 1);
    TEST_ASSERT(stateReady.segments[0].propertyPath == "transform.positionX");

    // 3. Reference span
    const Clip* retrieved = timeline.findClip(ClipId("c-1"));
    TEST_ASSERT(retrieved != nullptr);
    const auto* ch = retrieved->findAnimationChannel("transform.positionX");
    TEST_ASSERT(ch != nullptr);
    double span = GraphEditorSessionEngine::getReferenceSpanValue(*ch, 1);
    TEST_ASSERT(span == 100.0);

    std::cout << "[PASS] runGraphEditorSessionEngineTests" << std::endl;
}

void runGraphEditorEasingPresetsTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    const auto& builtins = GraphEditorEasingPresets::builtinPresets();
    TEST_ASSERT(builtins.size() == 6);
    TEST_ASSERT(builtins[0].id == "smooth");
    TEST_ASSERT(builtins[5].id == "linear");

    // Exact match
    auto matched = GraphEditorEasingPresets::findMatchingPreset(CubicBezier{0.0, 0.0, 1.0, 1.0});
    TEST_ASSERT(matched.has_value());
    TEST_ASSERT(matched->id == "linear");

    // Within tolerance
    auto matchedTol = GraphEditorEasingPresets::findMatchingPreset(CubicBezier{0.01, 0.01, 0.99, 1.01}, 0.02);
    TEST_ASSERT(matchedTol.has_value());
    TEST_ASSERT(matchedTol->id == "linear");

    // Custom presets
    GraphEditorEasingPresets store;
    store.addCustomPreset("custom-1", "My Ease", CubicBezier{0.1, 0.2, 0.3, 0.4});
    TEST_ASSERT(store.customPresets().size() == 1);
    TEST_ASSERT(store.allPresets().size() == 7);

    auto customMatch = GraphEditorEasingPresets::findMatchingPreset(
        CubicBezier{0.1, 0.2, 0.3, 0.4},
        0.02,
        store.customPresets()
    );
    TEST_ASSERT(customMatch.has_value());
    TEST_ASSERT(customMatch->id == "custom-1");

    bool removed = store.removeCustomPreset("custom-1");
    TEST_ASSERT(removed);
    TEST_ASSERT(store.customPresets().empty());

    std::cout << "[PASS] runGraphEditorEasingPresetsTests" << std::endl;
}

void runTrackLayoutMetricsTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    TEST_ASSERT(TrackLayoutMetrics::getTrackHeight(TrackType::Video) == 65.0);
    TEST_ASSERT(TrackLayoutMetrics::getTrackHeight(TrackType::Audio) == 50.0);
    TEST_ASSERT(TrackLayoutMetrics::getTrackHeight(TrackType::Text) == 25.0);

    TEST_ASSERT(TrackLayoutMetrics::getExpandedTrackHeight(TrackType::Video, 2) == 105.0); // 65 + 2*20

    Track t1(TrackId("t-1"), TrackType::Video, "V1");
    Track t2(TrackId("t-2"), TrackType::Audio, "A1");
    Track t3(TrackId("t-3"), TrackType::Text, "T1");
    std::vector<const Track*> tracks = {&t1, &t2, &t3};

    // Cumulative height before index 1 (just t1)
    double h0 = TrackLayoutMetrics::getCumulativeHeightBefore(tracks, 1);
    TEST_ASSERT(h0 == 65.0 + 6.0);

    // Cumulative height before index 2 (t1 + t2)
    double h1 = TrackLayoutMetrics::getCumulativeHeightBefore(tracks, 2);
    TEST_ASSERT(h1 == (65.0 + 6.0) + (50.0 + 6.0));

    auto offsets = TrackLayoutMetrics::getTrackLayoutOffsets(tracks);
    TEST_ASSERT(offsets.size() == 3);
    TEST_ASSERT(offsets[0] == 0.0);
    TEST_ASSERT(offsets[1] == 71.0);
    TEST_ASSERT(offsets[2] == 127.0);

    double totalH = TrackLayoutMetrics::getTotalTracksHeight(tracks);
    TEST_ASSERT(totalH == 65.0 + 50.0 + 25.0 + 2 * 6.0);

    std::cout << "[PASS] runTrackLayoutMetricsTests" << std::endl;
}

void runSelectionHitTestingTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // Normalization
    SelectionPoint p1{100.0, 200.0};
    SelectionPoint p2{50.0, 250.0};
    auto rect = SelectionHitTesting::getNormalizedRectangle(p1, p2);
    TEST_ASSERT(rect.left == 50.0);
    TEST_ASSERT(rect.top == 200.0);
    TEST_ASSERT(rect.right == 100.0);
    TEST_ASSERT(rect.bottom == 250.0);

    // Intersection
    SelectionRectangle a{0.0, 0.0, 100.0, 100.0};
    SelectionRectangle b{50.0, 50.0, 150.0, 150.0};
    SelectionRectangle c{200.0, 200.0, 300.0, 300.0};
    TEST_ASSERT(SelectionHitTesting::isRectangleIntersecting(a, b));
    TEST_ASSERT(!SelectionHitTesting::isRectangleIntersecting(a, c));

    // Multi-track marquee element intersection
    Track t1(TrackId("t-1"), TrackType::Video, "V1");
    Clip c1(ClipId("c-1"), ClipType::Video, "Clip 1", TimelineTime::fromSeconds(1.0), TimelineTime::fromSeconds(3.0));
    t1.insertClip(c1);

    Track t2(TrackId("t-2"), TrackType::Audio, "A1");
    Clip c2(ClipId("c-2"), ClipType::Audio, "Clip 2", TimelineTime::fromSeconds(5.0), TimelineTime::fromSeconds(3.0));
    t2.insertClip(c2);

    std::vector<const Track*> tracks = {&t1, &t2};

    // Marquee covering c1 (50px/sec -> 1.0s = 50px, 4.0s = 200px)
    SelectionPoint start{40.0, 10.0};
    SelectionPoint end{150.0, 60.0};
    auto selected = SelectionHitTesting::resolveTimelineElementIntersections(tracks, 1.0, start, end);
    TEST_ASSERT(selected.size() == 1);
    TEST_ASSERT(selected[0].elementId == ClipId("c-1"));

    std::cout << "[PASS] runSelectionHitTestingTests" << std::endl;
}

void runExportMimeTypesAndLayersTests() {
    using namespace catchim::editor;

    TEST_ASSERT(ExportMimeTypes::getMimeTypeForExtension("mp4") == "video/mp4");
    TEST_ASSERT(ExportMimeTypes::getMimeTypeForExtension(".webm") == "video/webm");
    TEST_ASSERT(ExportMimeTypes::getExtensionForMimeType("video/webm") == "webm");
    TEST_ASSERT(ExportMimeTypes::getExtensionForMimeType("video/mp4") == "mp4");

    int trackContent = TimelineLayers::TRACK_CONTENT;
    int dragLine = TimelineLayers::DRAG_LINE;
    int playhead = TimelineLayers::PLAYHEAD;
    int snapIndicator = TimelineLayers::SNAP_INDICATOR;

    TEST_ASSERT(trackContent < dragLine);
    TEST_ASSERT(dragLine < playhead);
    TEST_ASSERT(playhead < snapIndicator);

    int hx = PreviewPenCursor::HOTSPOT_X;
    int hy = PreviewPenCursor::HOTSPOT_Y;
    TEST_ASSERT(hx == 1);
    TEST_ASSERT(hy == 1);
    TEST_ASSERT(std::strstr(PreviewPenCursor::getSvgContent(), "<svg") != nullptr);

    std::cout << "[PASS] runExportMimeTypesAndLayersTests" << std::endl;
}

void runTimelineThemeTests() {
    using namespace catchim::editor;

    auto themeText = TimelineTheme::getTrackTheme(TrackType::Text);
    TEST_ASSERT(themeText.hexColor == "#5DBAA0");
    TEST_ASSERT(!themeText.waveformColor.has_value());

    auto themeAudio = TimelineTheme::getTrackTheme(TrackType::Audio);
    TEST_ASSERT(themeAudio.hexColor == "#8F5DBA");
    TEST_ASSERT(themeAudio.waveformColor.has_value());

    TEST_ASSERT(TimelineTheme::getTimelineElementClassName(TrackType::Graphic) == "bg-[#BA5D7A]");
    TEST_ASSERT(TimelineTheme::getTrackHexColor(TrackType::Effect) == "#5d93ba");
    TEST_ASSERT(TimelineTheme::DEFAULT_TIMELINE_BOOKMARK_COLOR == "#009dff");

    std::cout << "[PASS] runTimelineThemeTests" << std::endl;
}

void runPreviewSettingsStoreTests() {
    using namespace catchim::editor;

    PreviewSettingsStore store;
    TEST_ASSERT(!store.activeGuide().has_value());
    TEST_ASSERT(store.gridConfig().rows == 3);
    TEST_ASSERT(store.gridConfig().cols == 3);

    // Guide toggling
    store.toggleGuide("rule-of-thirds");
    TEST_ASSERT(store.activeGuide().has_value());
    TEST_ASSERT(*store.activeGuide() == "rule-of-thirds");

    store.toggleGuide("rule-of-thirds");
    TEST_ASSERT(!store.activeGuide().has_value());

    // Overlay visibility
    TEST_ASSERT(store.isOverlayVisible("safe-zone", true));
    store.setOverlayVisibility("safe-zone", false);
    TEST_ASSERT(!store.isOverlayVisible("safe-zone", true));
    store.toggleOverlayVisibility("safe-zone");
    TEST_ASSERT(store.isOverlayVisible("safe-zone", true));

    // Serialization
    auto j = store.toJson();
    auto loaded = PreviewSettingsStore::fromJson(j);
    TEST_ASSERT(loaded.isOverlayVisible("safe-zone", true));
    TEST_ASSERT(loaded.gridConfig().rows == 3);

    size_t presetCount = PreviewSettingsStore::PREVIEW_ZOOM_PRESETS.size();
    TEST_ASSERT(presetCount == 6);

    std::cout << "[PASS] runPreviewSettingsStoreTests" << std::endl;
}

void runRetimeRateEngineTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    TEST_ASSERT(RetimeRateEngine::clampRetimeRate(0.0) == 1.0);
    TEST_ASSERT(RetimeRateEngine::clampRetimeRate(10.0) == 5.0);
    TEST_ASSERT(RetimeRateEngine::clampRetimeRate(0.001) == 0.01);

    TEST_ASSERT(RetimeRateEngine::canMaintainPitch(2.0));
    TEST_ASSERT(!RetimeRateEngine::canMaintainPitch(-1.0));
    TEST_ASSERT(RetimeRateEngine::shouldMaintainPitch(2.0, true));
    TEST_ASSERT(!RetimeRateEngine::shouldMaintainPitch(2.0, false));

    auto cfg = RetimeRateEngine::buildConstantRetime(2.0, true);
    TEST_ASSERT(cfg.rate == 2.0);
    TEST_ASSERT(cfg.maintainPitch);

    // 2.0x speed: 3.0s in clip = 6.0s in source
    TEST_ASSERT(RetimeRateEngine::getSourceTimeAtClipTime(3.0, 2.0) == 6.0);
    TEST_ASSERT(RetimeRateEngine::getClipTimeAtSourceTime(6.0, 2.0) == 3.0);
    TEST_ASSERT(RetimeRateEngine::getTimelineDurationForSourceSpan(6.0, 2.0) == 3.0);

    auto split = RetimeRateEngine::splitRetimeAtClipTime(cfg, 1.5);
    TEST_ASSERT(split.first.rate == 2.0);
    TEST_ASSERT(split.second.rate == 2.0);

    std::cout << "[PASS] runRetimeRateEngineTests" << std::endl;
}

void runTransformHandleControllerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    TEST_ASSERT(TransformHandleController::clampScaleNonZero(0.0001) == 0.001);
    TEST_ASSERT(TransformHandleController::clampScaleNonZero(-0.0001) == -0.001);
    TEST_ASSERT(TransformHandleController::clampScaleNonZero(2.5) == 2.5);

    double dist = TransformHandleController::getCornerDistance(100.0, 100.0, 0.0, HandleCorner::TopLeft);
    TEST_ASSERT(dist > 0.0);

    TransformHandleController controller;
    TEST_ASSERT(controller.isIdle());

    controller.startCornerScale(TrackId("t-1"), ClipId("c-1"), HandleCorner::BottomRight, 200.0, 100.0, 0.0);
    TEST_ASSERT(controller.sessionKind() == TransformSessionKind::CornerScale);
    TEST_ASSERT(controller.cornerSession().has_value());
    TEST_ASSERT(controller.cornerSession()->corner == HandleCorner::BottomRight);

    controller.startEdgeScale(TrackId("t-1"), ClipId("c-1"), HandleEdge::Right, 200.0, 100.0, 0.0);
    TEST_ASSERT(controller.sessionKind() == TransformSessionKind::EdgeScale);

    controller.startRotation(TrackId("t-1"), ClipId("c-1"), 0.5, 45.0);
    TEST_ASSERT(controller.sessionKind() == TransformSessionKind::Rotation);

    controller.endSession();
    TEST_ASSERT(controller.isIdle());

    // Scale animation reset
    Clip clip(ClipId("c-test"), ClipType::Video, "Test", TimelineTime(0), TimelineTime::fromSeconds(5.0));
    TEST_ASSERT(!TransformHandleController::shouldClearScaleAnimation(clip));
    clip.getOrCreateAnimationChannel("transform.scaleX", 1.0);
    TEST_ASSERT(TransformHandleController::shouldClearScaleAnimation(clip));
    TransformHandleController::clearScaleAnimationChannels(clip);
    TEST_ASSERT(!TransformHandleController::shouldClearScaleAnimation(clip));

    std::cout << "[PASS] runTransformHandleControllerTests" << std::endl;
}

void runPreviewInteractionGestureControllerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    TEST_ASSERT(!PreviewInteractionGestureController::movedPastDragThreshold({0.0, 0.0}, {0.2, 0.2}));
    TEST_ASSERT(PreviewInteractionGestureController::movedPastDragThreshold({0.0, 0.0}, {0.6, 0.0}));

    ElementRef el1{TrackId("t1"), ClipId("c1")};
    ElementRef el2{TrackId("t1"), ClipId("c2")};

    // Target not in selection -> [dragTarget]
    auto sel1 = PreviewInteractionGestureController::buildDragSelection({el1}, el2);
    TEST_ASSERT(sel1.size() == 1);
    TEST_ASSERT(sel1[0] == el2);

    // Target already in selection -> reordered with target first
    auto sel2 = PreviewInteractionGestureController::buildDragSelection({el1, el2}, el2);
    TEST_ASSERT(sel2.size() == 2);
    TEST_ASSERT(sel2[0] == el2);
    TEST_ASSERT(sel2[1] == el1);

    PreviewInteractionGestureController controller;
    TEST_ASSERT(controller.isIdle());

    controller.startPending({100.0, 100.0}, {el1});
    TEST_ASSERT(controller.isPending());

    // Move below threshold (0.2px)
    controller.handleMove({100.2, 100.2});
    TEST_ASSERT(controller.isPending());

    // Move above threshold (1.0px)
    controller.handleMove({101.0, 100.0});
    TEST_ASSERT(controller.isDragging());

    controller.endGesture();
    TEST_ASSERT(controller.isIdle());

    // Text editing
    TEST_ASSERT(!controller.isEditingText());
    controller.startTextEdit(TrackId("t1"), ClipId("c1"), "Hello");
    TEST_ASSERT(controller.isEditingText());
    TEST_ASSERT(controller.editingTextState()->text == "Hello");

    controller.commitTextEdit("Hello World");
    TEST_ASSERT(!controller.isEditingText());

    std::cout << "[PASS] runPreviewInteractionGestureControllerTests" << std::endl;
}

void runMaskFeatherEngineTests() {
    using namespace catchim::render;

    auto surface = MaskFeatherEngine::createSurface(50, 50);
    TEST_ASSERT(surface.isValid());
    TEST_ASSERT(surface.pixels.size() == 50 * 50 * 4);

    double clamped = MaskFeatherEngine::clampFeather(100.0, 50, 50);
    TEST_ASSERT(clamped == 25.0);

    // Set center block to alpha 255
    for (int y = 20; y < 30; ++y) {
        for (int x = 20; x < 30; ++x) {
            surface.pixels[static_cast<size_t>((y * 50 + x) * 4 + 3)] = 255;
        }
    }

    uint8_t alphaBefore = surface.pixels[static_cast<size_t>((25 * 50 + 25) * 4 + 3)];
    TEST_ASSERT(alphaBefore == 255);

    MaskFeatherEngine::applyMaskFeather(surface, 5.0);

    // Adjacent pixel that was 0 should now have feathered alpha > 0
    uint8_t alphaNeighbor = surface.pixels[static_cast<size_t>((19 * 50 + 20) * 4 + 3)];
    TEST_ASSERT(alphaNeighbor > 0);

    std::cout << "[PASS] runMaskFeatherEngineTests" << std::endl;
}

void runOpencutNativeCoreBindingsTests() {
    using namespace catchim::native;
    using namespace catchim::core;

    int64_t ticksSec = OpencutNativeCoreBindings::ticksPerSecond();
    TEST_ASSERT(ticksSec == 120000);

    int64_t ticks = OpencutNativeCoreBindings::fromSeconds(1.5);
    TEST_ASSERT(ticks == 180000);
    double sec = OpencutNativeCoreBindings::toSeconds(180000);
    TEST_ASSERT(std::abs(sec - 1.5) < 0.0001);

    int64_t rounded = OpencutNativeCoreBindings::roundToFrame(1000, 30, 1);
    TEST_ASSERT(rounded == 0);
    int64_t floored = OpencutNativeCoreBindings::floorToFrame(4500, 30, 1);
    TEST_ASSERT(floored == 4000);
    int64_t last = OpencutNativeCoreBindings::lastFrame(120000, 30, 1);
    TEST_ASSERT(last == 116000);

    double fadeMid = OpencutNativeCoreBindings::evaluateFade(1.0, 5.0, 2.0, 2.0);
    TEST_ASSERT(std::abs(fadeMid - 0.5) < 0.001);

    double maskRectAlpha = OpencutNativeCoreBindings::evaluateMaskAlpha(
        0.0, 0.0, 0, 0.0, 0.0, 100.0, 100.0, 0.0, 0.0, false
    );
    TEST_ASSERT(maskRectAlpha == 1.0);

    double bezierVal = OpencutNativeCoreBindings::solveBezier(0.5, 0.0, 0.0, 1.0, 1.0);
    TEST_ASSERT(std::abs(bezierVal - 0.5) < 0.01);

    double p = OpencutNativeCoreBindings::evaluateBezierPoint(0.5, 0.0, 0.0, 1.0, 1.0);
    TEST_ASSERT(std::abs(p - 0.5) < 0.01);

    uint8_t buffer[16] = {0};
    OpencutNativeCoreBindings::clearBufferRgba(buffer, 2, 2, 255, 128, 64, 255);
    TEST_ASSERT(buffer[0] == 255);
    TEST_ASSERT(buffer[1] == 128);
    TEST_ASSERT(buffer[2] == 64);
    TEST_ASSERT(buffer[3] == 255);

    TEST_ASSERT(OpencutNativeCoreBindings::pointInRotatedRect(0.0, 0.0, 0.0, 0.0, 50.0, 50.0, 45.0));
    TEST_ASSERT(!OpencutNativeCoreBindings::pointInRotatedRect(100.0, 100.0, 0.0, 0.0, 50.0, 50.0, 0.0));

    float audioSamples[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    OpencutNativeCoreBindings::applyGainRamp(audioSamples, 4, 0.0f, 1.0f);
    TEST_ASSERT(audioSamples[0] == 0.0f);
    TEST_ASSERT(audioSamples[3] == 1.0f);

    float dest[2] = {0.5f, 0.5f};
    float src[2] = {1.0f, 1.0f};
    OpencutNativeCoreBindings::mixAudioBuffers(dest, src, 2, 0.5f);
    TEST_ASSERT(std::abs(dest[0] - 1.0f) < 0.001f);

    float peak = OpencutNativeCoreBindings::computeBufferPeak(dest, 2);
    TEST_ASSERT(std::abs(peak - 1.0f) < 0.001f);

    OpencutNativeCoreBindings::clampBufferSamples(dest, 2, 0.8f);
    TEST_ASSERT(std::abs(dest[0] - 0.8f) < 0.001f);

    float left[2] = {0.2f, 0.6f};
    float right[2] = {0.4f, 0.8f};
    float outStereo[2] = {0.0f, 0.0f};
    OpencutNativeCoreBindings::downmixStereo(left, right, outStereo, 2);
    TEST_ASSERT(std::abs(outStereo[0] - 0.3f) < 0.001f);
    TEST_ASSERT(std::abs(outStereo[1] - 0.7f) < 0.001f);

    std::cout << "[PASS] runOpencutNativeCoreBindingsTests" << std::endl;
}

void runPanelStoreEngineTests() {
    using namespace catchim::editor;

    PanelStoreEngine store;
    TEST_ASSERT(store.getPanels().tools == 25.0);
    TEST_ASSERT(store.getPanels().preview == 50.0);

    store.setPanel(PanelId::Tools, 30.0);
    TEST_ASSERT(store.getPanels().tools == 30.0);

    store.setPanel("preview", 45.0);
    TEST_ASSERT(store.getPanels().preview == 45.0);

    store.setPanels(20.0, 55.0);
    TEST_ASSERT(store.getPanels().tools == 20.0);
    TEST_ASSERT(store.getPanels().preview == 55.0);

    store.resetPanels();
    TEST_ASSERT(store.getPanels().tools == 25.0);

    auto json = store.toJson();
    TEST_ASSERT(json["version"] == 2);
    TEST_ASSERT(json["panels"]["tools"] == 25.0);

    // v1 schema migration test
    nlohmann::json v1Json = {
        {"toolsPanel", 18.0},
        {"previewPanel", 62.0},
        {"propertiesPanel", 20.0}
    };
    auto migrated = PanelStoreEngine::fromJson(v1Json);
    TEST_ASSERT(migrated.getPanels().tools == 18.0);
    TEST_ASSERT(migrated.getPanels().preview == 62.0);
    TEST_ASSERT(migrated.getPanels().properties == 20.0);

    TEST_ASSERT(PanelStoreEngine::panelIdToString(PanelId::Timeline) == "timeline");
    auto pOpt = PanelStoreEngine::panelIdFromString("mainContent");
    TEST_ASSERT(pOpt.has_value() && *pOpt == PanelId::MainContent);

    std::cout << "[PASS] runPanelStoreEngineTests" << std::endl;
}

void runTextElementMeasurementEngineTests() {
    using namespace catchim::render;
    using namespace catchim::editor;
    using namespace catchim::core;

    std::unordered_map<std::string, std::string> params = {
        {"background.enabled", "true"},
        {"background.color", "#ff0000"},
        {"background.cornerRadius", "12.0"},
        {"background.paddingX", "10.0"},
        {"background.paddingY", "6.0"},
        {"background.offsetX", "2.0"},
        {"background.offsetY", "-1.0"}
    };

    auto bg = TextElementMeasurementEngine::buildTextBackgroundFromParams(params);
    TEST_ASSERT(bg.enabled);
    TEST_ASSERT(bg.color == "#ff0000");
    TEST_ASSERT(bg.cornerRadius == 12.0);
    TEST_ASSERT(bg.paddingX == 10.0);
    TEST_ASSERT(bg.paddingY == 6.0);

    Clip clip(ClipId("c-text"), ClipType::Text, "Text", TimelineTime(0), TimelineTime::fromSeconds(5.0));
    clip.getOrCreateAnimationChannel("background.paddingX", 10.0);
    clip.findAnimationChannel("background.paddingX")->addOrUpdateKeyframe(Keyframe(TimelineTime(0), 10.0));
    clip.findAnimationChannel("background.paddingX")->addOrUpdateKeyframe(Keyframe(TimelineTime::fromSeconds(2.0), 20.0));

    auto resolved = TextElementMeasurementEngine::resolveBackgroundAtTime(bg, &clip, TimelineTime::fromSeconds(1.0));
    TEST_ASSERT(resolved.enabled);
    TEST_ASSERT(std::abs(resolved.paddingX - 15.0) < 0.01);

    auto measured = TextElementMeasurementEngine::measureElement(params, nullptr, TimelineTime(0), 100.0, 40.0);
    TEST_ASSERT(measured.textWidth == 100.0);
    TEST_ASSERT(measured.textHeight == 40.0);
    TEST_ASSERT(measured.visualRect.width == 100.0 + 20.0);
    TEST_ASSERT(measured.visualRect.height == 40.0 + 12.0);

    std::cout << "[PASS] runTextElementMeasurementEngineTests" << std::endl;
}

void runEditableTimecodeControllerTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    EditableTimecodeController controller;
    TEST_ASSERT(!controller.isEditing());
    TEST_ASSERT(!controller.hasError());

    controller.startEditing("00:00:01:00");
    TEST_ASSERT(controller.isEditing());
    TEST_ASSERT(controller.inputValue() == "00:00:01:00");

    controller.setInputValue("00:00:02:15");
    TEST_ASSERT(controller.inputValue() == "00:00:02:15");

    auto applied = controller.applyEdit(TimecodeFormat::HH_MM_SS_FF, FrameRate{30, 1}, TimelineTime::fromSeconds(10.0));
    TEST_ASSERT(applied.has_value());
    TEST_ASSERT(!controller.hasError());
    TEST_ASSERT(!controller.isEditing());
    TEST_ASSERT(applied->toSeconds() == 2.5);

    // Invalid input
    controller.startEditing("invalid:timecode");
    auto invalidApplied = controller.applyEdit(TimecodeFormat::HH_MM_SS_FF, FrameRate{30, 1});
    TEST_ASSERT(!invalidApplied.has_value());
    TEST_ASSERT(controller.hasError());

    controller.cancelEditing();
    TEST_ASSERT(!controller.isEditing());
    TEST_ASSERT(!controller.hasError());

    std::string formatted = EditableTimecodeController::formatTime(TimelineTime::fromSeconds(1.0), TimecodeFormat::HH_MM_SS_FF, FrameRate{30, 1});
    TEST_ASSERT(!formatted.empty());

    std::cout << "[PASS] runEditableTimecodeControllerTests" << std::endl;
}

void runProjectOrganizationEngineTests() {
    using namespace catchim::core;

    TEST_ASSERT(ProjectOrganizationEngine::sortOptionToString(ProjectSortOption::NameAsc) == "name-asc");
    auto opt = ProjectOrganizationEngine::sortOptionFromString("createdAt-desc");
    TEST_ASSERT(opt.has_value() && *opt == ProjectSortOption::CreatedAtDesc);

    std::vector<ProjectSummary> projects = {
        {"1", "Beta Project", 100, 300, 10.0, ""},
        {"2", "Alpha Project", 200, 100, 20.0, ""},
        {"3", "Gamma Project", 300, 200, 30.0, ""}
    };

    auto sortedByName = ProjectOrganizationEngine::filterAndSortProjects(projects, "", ProjectSortOption::NameAsc);
    TEST_ASSERT(sortedByName.size() == 3);
    TEST_ASSERT(sortedByName[0].name == "Alpha Project");

    auto filtered = ProjectOrganizationEngine::filterAndSortProjects(projects, "beta", ProjectSortOption::NameAsc);
    TEST_ASSERT(filtered.size() == 1);
    TEST_ASSERT(filtered[0].id == "1");

    // Duplicate name parsing and generation
    auto [base, num] = ProjectOrganizationEngine::parseDuplicateBaseName("(2) My Video");
    TEST_ASSERT(base == "My Video");
    TEST_ASSERT(num.has_value() && *num == 2);

    std::vector<std::string> existing = {"My Video", "(1) My Video", "(2) My Video"};
    std::string dupName = ProjectOrganizationEngine::generateDuplicateName("My Video", existing);
    TEST_ASSERT(dupName == "(3) My Video");

    // Audio buffer stripping
    nlohmann::json projJson = {
        {"scenes", nlohmann::json::array({
            {
                {"tracks", {
                    {"audio", nlohmann::json::array({
                        {
                            {"elements", nlohmann::json::array({
                                {
                                    {"id", "el-1"},
                                    {"buffer", {1, 2, 3}}
                                }
                            })}
                        }
                    })}
                }}
            }
        })}
    };
    ProjectOrganizationEngine::stripAudioBuffers(projJson);
    TEST_ASSERT(!projJson["scenes"][0]["tracks"]["audio"][0]["elements"][0].contains("buffer"));

    std::cout << "[PASS] runProjectOrganizationEngineTests" << std::endl;
}

void runStorageServiceCoordinatorTests() {
    using namespace catchim::storage;
    using namespace catchim::core;

    const auto& defCfg = StorageServiceCoordinator::defaultConfig();
    TEST_ASSERT(defCfg.version == 1);
    TEST_ASSERT(defCfg.projectsDb == "video-editor-projects");

    std::string mediaStore = StorageServiceCoordinator::getProjectMediaStoreName("proj-123");
    TEST_ASSERT(mediaStore == "video-editor-media-proj-123");

    std::string folder = StorageServiceCoordinator::getProjectMediaFilesFolder("proj-123");
    TEST_ASSERT(folder == "media-files-proj-123");

    // Bookmark normalization
    nlohmann::json bmArray = nlohmann::json::array({
        120000,
        {
            {"time", 240000},
            {"note", "Intro marker"},
            {"color", "#ffaa00"},
            {"duration", 60000}
        }
    });

    auto bookmarks = StorageServiceCoordinator::normalizeBookmarks(bmArray);
    TEST_ASSERT(bookmarks.size() == 2);
    TEST_ASSERT(bookmarks[0].time.ticks() == 120000);
    TEST_ASSERT(!bookmarks[0].note.has_value());
    TEST_ASSERT(bookmarks[1].time.ticks() == 240000);
    TEST_ASSERT(bookmarks[1].note.has_value() && *bookmarks[1].note == "Intro marker");
    TEST_ASSERT(bookmarks[1].duration.has_value() && bookmarks[1].duration->ticks() == 60000);

    TEST_ASSERT(StorageServiceCoordinator::isQuotaExceededError("QuotaExceededError: storage is full"));
    TEST_ASSERT(!StorageServiceCoordinator::isQuotaExceededError("Network timeout"));

    std::cout << "[PASS] runStorageServiceCoordinatorTests" << std::endl;
}

void runAudioVolumeLineEngineTests() {
    using namespace catchim::audio;

    TEST_ASSERT(AudioVolumeLineEngine::clampVolumeDb(-100.0) == -60.0);
    TEST_ASSERT(AudioVolumeLineEngine::clampVolumeDb(50.0) == 12.0);
    TEST_ASSERT(AudioVolumeLineEngine::clampVolumeDb(0.0) == 0.0);

    // Position percent mapping
    double pos0 = AudioVolumeLineEngine::getLinePositionPercent(0.0);
    TEST_ASSERT(pos0 > 60.0 && pos0 < 75.0);

    double posMin = AudioVolumeLineEngine::getLinePositionPercent(-60.0);
    TEST_ASSERT(std::abs(posMin - 100.0) < 1e-3);

    // Round-trip test
    double dbTest = -12.0;
    double posTest = AudioVolumeLineEngine::getLinePositionPercent(dbTest);
    double recoveredDb = AudioVolumeLineEngine::getDbFromLinePosition(posTest);
    TEST_ASSERT(std::abs(recoveredDb - dbTest) < 0.1);

    // Pointer calculation: rectTop = 100, rectHeight = 100.
    // clientY = 100 -> relativeY = 0 -> percent = 0 -> top (clamped to max volume +12 dB)
    double volTop = AudioVolumeLineEngine::getVolumeFromPointer(100.0, 100.0, 100.0);
    TEST_ASSERT(volTop > 10.0 && volTop <= 12.0);
    // clientY = 200 -> relativeY = 100 -> percent = 100 -> bottom (-60 dB)
    double volBottom = AudioVolumeLineEngine::getVolumeFromPointer(200.0, 100.0, 100.0);
    TEST_ASSERT(std::abs(volBottom - (-60.0)) < 1e-3);

    // Formatting
    TEST_ASSERT(AudioVolumeLineEngine::formatVolumeLabel(-60.0) == "-60.0 dB");
    TEST_ASSERT(AudioVolumeLineEngine::formatVolumeLabel(0.0) == "0.0 dB");
    TEST_ASSERT(AudioVolumeLineEngine::formatVolumeLabel(6.0) == "+6.0 dB");
    TEST_ASSERT(AudioVolumeLineEngine::formatVolumeLabel(-12.5) == "-12.5 dB");

    std::cout << "[PASS] runAudioVolumeLineEngineTests" << std::endl;
}

void runAudioWaveformBarEngineTests() {
    using namespace catchim::audio;

    // Gain sampling
    std::vector<double> gainSamples = {1.0, 1.5, 2.0};
    double gain = AudioWaveformBarEngine::sampleGainAtClipTime(gainSamples, 5.0, 10.0);
    TEST_ASSERT(std::abs(gain - 1.5) < 1e-4);
    double gainClamped = AudioWaveformBarEngine::sampleGainAtClipTime(gainSamples, 12.0, 10.0);
    TEST_ASSERT(std::abs(gainClamped - 2.0) < 1e-4);

    // Waveform bars calculation
    std::vector<float> amplitudes = {0.2f, 0.8f, 1.5f, 0.4f};
    double width = 8.0;   // 8px / step(2px) = 4 bars
    double height = 40.0;
    auto bars = AudioWaveformBarEngine::calculateWaveformBars(amplitudes, width, height, gainSamples, 10.0);
    TEST_ASSERT(bars.size() == 4);
    TEST_ASSERT(bars[0].x == 0.0);
    TEST_ASSERT(bars[1].x == 2.0);
    TEST_ASSERT(bars[2].x == 4.0);
    TEST_ASSERT(bars[3].x == 6.0);

    // 3rd bar (amp 1.5 * gain > 1.0) should be burnt
    TEST_ASSERT(bars[2].isBurnt == true);

    std::cout << "[PASS] runAudioWaveformBarEngineTests" << std::endl;
}

void runTtsServiceEngineTests() {
    using namespace catchim::audio;

    catchim::audio::TtsSynthesisOptions opts;
    opts.text = "Hello world";
    opts.voiceId = "alloy";
    opts.speed = 3.0; // out of bounds
    opts.pitch = -100.0; // out of bounds
    opts.volume = 1.5; // out of bounds

    bool valid = TtsServiceEngine::validateOptions(opts);
    TEST_ASSERT(valid);
    TEST_ASSERT(opts.speed == TtsServiceEngine::MAX_SPEED);
    TEST_ASSERT(opts.pitch == TtsServiceEngine::MIN_PITCH);
    TEST_ASSERT(opts.volume == TtsServiceEngine::MAX_VOLUME);

    // Empty text
    opts.text = "   ";
    TEST_ASSERT(!TtsServiceEngine::validateOptions(opts));

    // Audio duration
    TEST_ASSERT(std::abs(TtsServiceEngine::calculateAudioDuration(44100, 44100) - 1.0) < 1e-4);
    TEST_ASSERT(std::abs(TtsServiceEngine::calculateAudioDuration(22050, 44100) - 0.5) < 1e-4);

    // Encode PCM to WAV
    std::vector<float> samples(100, 0.5f);
    auto wavBytes = TtsServiceEngine::encodePcmToWav(samples, 44100);
    TEST_ASSERT(wavBytes.size() == 44 + 100 * sizeof(int16_t));
    TEST_ASSERT(wavBytes[0] == 'R' && wavBytes[1] == 'I' && wavBytes[2] == 'F' && wavBytes[3] == 'F');
    TEST_ASSERT(wavBytes[8] == 'W' && wavBytes[9] == 'A' && wavBytes[10] == 'V' && wavBytes[11] == 'E');

    std::cout << "[PASS] runTtsServiceEngineTests" << std::endl;
}

void runTimelineSceneUtilsTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    std::vector<Scene> scenes;
    Scene s1(SceneId("s1"), "Scene 1", false);
    Scene s2(SceneId("s2"), "Scene 2", true);
    scenes.push_back(s1);
    scenes.push_back(s2);

    const Scene* mainScene = TimelineSceneUtils::getMainScene(scenes);
    TEST_ASSERT(mainScene != nullptr);
    TEST_ASSERT(mainScene->id() == SceneId("s2"));

    TEST_ASSERT(!TimelineSceneUtils::canDeleteScene(s2)); // cannot delete main scene
    TEST_ASSERT(TimelineSceneUtils::canDeleteScene(s1));  // can delete non-main scene

    // Fallback after delete
    const Scene* fallback = TimelineSceneUtils::getFallbackSceneAfterDelete(scenes, SceneId("s1"), SceneId("s1"));
    TEST_ASSERT(fallback != nullptr);
    TEST_ASSERT(fallback->id() == SceneId("s2"));

    // Ensure main scene if none
    std::vector<Scene> scenesWithoutMain;
    Scene s3(SceneId("s3"), "Scene 3", false);
    scenesWithoutMain.push_back(s3);
    TimelineSceneUtils::ensureMainScene(scenesWithoutMain);
    TEST_ASSERT(scenesWithoutMain[0].isMain());

    std::cout << "[PASS] runTimelineSceneUtilsTests" << std::endl;
}

void runAudibleCandidateCollectorTests() {
    using namespace catchim::audio;
    using namespace catchim::editor;
    using namespace catchim::core;

    Timeline timeline;
    auto& tAudio = timeline.addTrack(TrackType::Audio, "Voiceover");
    Clip c1(ClipId("c1"), ClipType::Audio, "Clip 1", TimelineTime::fromSeconds(0.0), TimelineTime::fromSeconds(5.0));
    c1.setMediaId(MediaId("media-1"));
    c1.params()["volume"] = 0.8;
    timeline.addClip(tAudio.id(), std::move(c1));

    auto& tMuted = timeline.addTrack(TrackType::Audio, "Muted Track");
    tMuted.setMuted(true);
    Clip c2(ClipId("c2"), ClipType::Audio, "Clip 2", TimelineTime::fromSeconds(2.0), TimelineTime::fromSeconds(4.0));
    timeline.addClip(tMuted.id(), std::move(c2));

    auto candidates = AudibleCandidateCollector::collectAudibleCandidates(timeline);
    TEST_ASSERT(candidates.size() == 1);
    TEST_ASSERT(candidates[0].clipId == ClipId("c1"));
    TEST_ASSERT(std::abs(candidates[0].volume - 0.8) < 1e-4);

    TEST_ASSERT(AudibleCandidateCollector::timelineHasAudio(timeline) == true);
    auto totalAudible = AudibleCandidateCollector::calculateTotalAudibleDuration(timeline);
    TEST_ASSERT(totalAudible.toSeconds() >= 5.0);

    // Empty timeline test
    Timeline emptyTl;
    TEST_ASSERT(AudibleCandidateCollector::timelineHasAudio(emptyTl) == false);

    std::cout << "[PASS] runAudibleCandidateCollectorTests" << std::endl;
}

void runTimelineToolbarEngineTests() {
    using namespace catchim::editor;

    double zoom = 1.0;
    double zoomedIn = TimelineToolbarEngine::zoomIn(zoom);
    TEST_ASSERT(zoomedIn > zoom);
    TEST_ASSERT(std::abs(zoomedIn - 1.7) < 1e-4);

    double zoomedOut = TimelineToolbarEngine::zoomOut(zoomedIn);
    TEST_ASSERT(std::abs(zoomedOut - 1.0) < 1e-4);

    // Clamping
    double maxZoomed = TimelineToolbarEngine::zoomIn(90.0, 0.01, 100.0, 2.0);
    TEST_ASSERT(maxZoomed == 100.0);

    // Playhead anchored check
    TEST_ASSERT(!TimelineToolbarEngine::isPlayheadAnchored(100.0, 1000.0));
    TEST_ASSERT(TimelineToolbarEngine::isPlayheadAnchored(500.0, 1000.0));

    std::cout << "[PASS] runTimelineToolbarEngineTests" << std::endl;
}

void runRulerIntervalEngineTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // Default zoom 1.0, 30fps
    auto cfg = RulerIntervalEngine::getRulerConfig(1.0, 30.0);
    TEST_ASSERT(cfg.labelIntervalSeconds > 0.0);
    TEST_ASSERT(cfg.tickIntervalSeconds > 0.0);
    TEST_ASSERT(cfg.labelIntervalSeconds >= cfg.tickIntervalSeconds);

    // Zoomed in (10x): labels and ticks become denser in seconds
    auto cfgZoomedIn = RulerIntervalEngine::getRulerConfig(10.0, 30.0);
    TEST_ASSERT(cfgZoomedIn.labelIntervalSeconds <= cfg.labelIntervalSeconds);

    // shouldShowLabel
    TEST_ASSERT(RulerIntervalEngine::shouldShowLabel(0.0, 1.0));
    TEST_ASSERT(RulerIntervalEngine::shouldShowLabel(2.0, 1.0));
    TEST_ASSERT(!RulerIntervalEngine::shouldShowLabel(0.5, 1.0));

    // isSecondBoundary
    TEST_ASSERT(RulerIntervalEngine::isSecondBoundary(1.0));
    TEST_ASSERT(RulerIntervalEngine::isSecondBoundary(120.0));
    TEST_ASSERT(!RulerIntervalEngine::isSecondBoundary(1.25));

    // formatTimestamp & formatRulerLabel
    TEST_ASSERT(RulerIntervalEngine::formatTimestamp(0.0) == "00:00");
    TEST_ASSERT(RulerIntervalEngine::formatTimestamp(65.0) == "01:05");
    TEST_ASSERT(RulerIntervalEngine::formatTimestamp(3665.0) == "1:01:05");

    // Label format on second boundary
    TEST_ASSERT(RulerIntervalEngine::formatRulerLabel(2.0, 30.0) == "00:02");
    // Between seconds: e.g. 2.5s at 30fps -> 15f
    TEST_ASSERT(RulerIntervalEngine::formatRulerLabel(2.5, 30.0) == "15f");

    std::cout << "[PASS] runRulerIntervalEngineTests" << std::endl;
}

void runTimelineExpandedLayoutEngineTests() {
    using namespace catchim::editor;
    using namespace catchim::core;

    // Property labels
    TEST_ASSERT(TimelineExpandedLayoutEngine::getPropertyLabel("transform.positionX") == "Position X");
    TEST_ASSERT(TimelineExpandedLayoutEngine::getPropertyLabel("opacity") == "Opacity");
    TEST_ASSERT(TimelineExpandedLayoutEngine::getPropertyLabel("volume") == "Volume");
    TEST_ASSERT(TimelineExpandedLayoutEngine::getPropertyLabel("background.cornerRadius") == "Corner Radius");
    TEST_ASSERT(TimelineExpandedLayoutEngine::getPropertyLabel("params.fontSize") == "fontSize");
    TEST_ASSERT(TimelineExpandedLayoutEngine::getPropertyLabel("effects.blur.radius") == "radius");

    // Expanded rows ordering
    std::vector<std::string> paths = {
        "effects.blur.radius",
        "transform.positionX",
        "volume",
        "background.color"
    };
    auto rows = TimelineExpandedLayoutEngine::getExpandedRowsFromPaths(paths);
    TEST_ASSERT(rows.size() == 4);
    // Group ordering: transform (0) -> volume (1) -> background (2) -> effects (4)
    TEST_ASSERT(rows[0].propertyPath == "transform.positionX");
    TEST_ASSERT(rows[1].propertyPath == "volume");
    TEST_ASSERT(rows[2].propertyPath == "background.color");
    TEST_ASSERT(rows[3].propertyPath == "effects.blur.radius");

    // Height calculation: 4 rows * 24.0 = 96.0 px
    TEST_ASSERT(TimelineExpandedLayoutEngine::getExpansionHeight(rows) == 96.0);

    // Track expansion height
    Track track(TrackId("t1"), TrackType::Video, "Video Track");
    Clip clip(ClipId("c1"), ClipType::Video, "Clip 1", TimelineTime::zero(), TimelineTime::fromSeconds(5.0));
    clip.getOrCreateAnimationChannel("transform.positionX").addOrUpdateKeyframe(Keyframe{ .time = TimelineTime::zero(), .value = 0.0 });
    clip.getOrCreateAnimationChannel("opacity").addOrUpdateKeyframe(Keyframe{ .time = TimelineTime::zero(), .value = 1.0 });
    track.clips().push_back(clip);

    std::unordered_set<std::string> expandedIds = {"c1"};
    double trackH = TimelineExpandedLayoutEngine::computeTrackExpansionHeight(track, expandedIds);
    TEST_ASSERT(trackH == 48.0); // 2 rows * 24.0

    auto trackRows = TimelineExpandedLayoutEngine::getTrackExpandedRows(track, expandedIds);
    TEST_ASSERT(trackRows.size() == 2);

    std::cout << "[PASS] runTimelineExpandedLayoutEngineTests" << std::endl;
}

void runAssetsPanelStoreEngineTests() {
    using namespace catchim::editor;

    AssetsPanelStoreEngine store;
    TEST_ASSERT(store.activeTab() == AssetsTab::Media);
    TEST_ASSERT(store.mediaViewMode() == MediaViewMode::Grid);
    TEST_ASSERT(store.mediaSortBy() == MediaSortKey::Name);
    TEST_ASSERT(store.mediaSortOrder() == MediaSortOrder::Asc);

    store.setActiveTab(AssetsTab::Stickers);
    TEST_ASSERT(store.activeTab() == AssetsTab::Stickers);
    TEST_ASSERT(std::string(AssetsPanelStoreEngine::tabToString(store.activeTab())) == "stickers");

    store.requestRevealMedia("media-789");
    TEST_ASSERT(store.activeTab() == AssetsTab::Media);
    TEST_ASSERT(store.highlightMediaId().has_value() && *store.highlightMediaId() == "media-789");

    store.clearHighlight();
    TEST_ASSERT(!store.highlightMediaId().has_value());

    store.setMediaViewMode(MediaViewMode::List);
    store.setMediaSort(MediaSortKey::Size, MediaSortOrder::Desc);

    auto json = store.toJson();
    AssetsPanelStoreEngine restored;
    restored.fromJson(json);
    TEST_ASSERT(restored.mediaViewMode() == MediaViewMode::List);
    TEST_ASSERT(restored.mediaSortBy() == MediaSortKey::Size);
    TEST_ASSERT(restored.mediaSortOrder() == MediaSortOrder::Desc);

    std::cout << "[PASS] runAssetsPanelStoreEngineTests" << std::endl;
}

void runPropertiesPanelStoreEngineTests() {
    using namespace catchim::editor;

    PropertiesPanelStoreEngine store;
    TEST_ASSERT(store.getActiveTab("video", "basic") == "basic");
    TEST_ASSERT(!store.isTransformScaleLocked());

    store.setActiveTab("video", "transform");
    TEST_ASSERT(store.getActiveTab("video") == "transform");

    store.setTransformScaleLocked(true);
    TEST_ASSERT(store.isTransformScaleLocked());
    store.toggleTransformScaleLock();
    TEST_ASSERT(!store.isTransformScaleLocked());

    store.setTransformScaleLocked(true);
    auto j = store.toJson();
    PropertiesPanelStoreEngine restored;
    restored.fromJson(j);
    TEST_ASSERT(restored.isTransformScaleLocked());
    TEST_ASSERT(restored.getActiveTab("video") == "transform");

    std::cout << "[PASS] runPropertiesPanelStoreEngineTests" << std::endl;
}

void runSpeedInputControllerTests() {
    using namespace catchim::editor;

    // Display formatting
    TEST_ASSERT(SpeedInputController::rateToDisplay(1.0) == "1.00");
    TEST_ASSERT(SpeedInputController::rateToDisplay(0.5) == "0.50");
    TEST_ASSERT(SpeedInputController::rateToDisplay(2.25) == "2.25");

    // Input parsing
    auto parsed = SpeedInputController::parseSpeedInput("1.256");
    TEST_ASSERT(parsed.has_value());
    TEST_ASSERT(std::abs(*parsed - 1.26) < 1e-4); // snapped to 0.01

    TEST_ASSERT(!SpeedInputController::parseSpeedInput("abc").has_value());
    TEST_ASSERT(!SpeedInputController::parseSpeedInput("").has_value());

    // Clamping
    auto parsedLarge = SpeedInputController::parseSpeedInput("100.0");
    TEST_ASSERT(parsedLarge.has_value());
    TEST_ASSERT(*parsedLarge == RetimeRateEngine::MAX_RETIME_RATE);

    // Retime builder
    auto defRetime = SpeedInputController::buildRetime(1.0, false);
    TEST_ASSERT(!defRetime.has_value()); // 1.0 without pitch maintenance is nullopt

    auto customRetime = SpeedInputController::buildRetime(1.5, true);
    TEST_ASSERT(customRetime.has_value());
    TEST_ASSERT(customRetime->rate == 1.5);
    TEST_ASSERT(customRetime->maintainPitch == true);

    // Maintain pitch compatibility
    TEST_ASSERT(SpeedInputController::canMaintainPitch(ClipType::Audio));
    TEST_ASSERT(SpeedInputController::canMaintainPitch(ClipType::Video));
    TEST_ASSERT(!SpeedInputController::canMaintainPitch(ClipType::Text));

    std::cout << "[PASS] runSpeedInputControllerTests" << std::endl;
}

void runNativeFileSystemStorageAdapterTests() {
    using namespace catchim::storage;

    NativeFileSystemStorageAdapter adapter("test_adapter");
    adapter.clear();

    std::vector<uint8_t> sampleData = {0xDE, 0xAD, 0xBE, 0xEF};
    bool okSet = adapter.set("sample.bin", sampleData);
    TEST_ASSERT(okSet);
    TEST_ASSERT(adapter.exists("sample.bin"));
    TEST_ASSERT(adapter.size("sample.bin") == 4);

    auto loaded = adapter.get("sample.bin");
    TEST_ASSERT(loaded.has_value());
    TEST_ASSERT(*loaded == sampleData);

    auto keys = adapter.list();
    TEST_ASSERT(keys.size() == 1);
    TEST_ASSERT(keys[0] == "sample.bin");

    bool okRemove = adapter.remove("sample.bin");
    TEST_ASSERT(okRemove);
    TEST_ASSERT(!adapter.exists("sample.bin"));

    // Multiple files and clear
    adapter.set("f1.dat", sampleData);
    adapter.set("f2.dat", sampleData);
    TEST_ASSERT(adapter.list().size() == 2);
    adapter.clear();
    TEST_ASSERT(adapter.list().empty());

    std::cout << "[PASS] runNativeFileSystemStorageAdapterTests" << std::endl;
}

void runTimelineEdgeScrollEngineTests() {
    using namespace catchim::editor::timeline;

    EdgeScrollParams p;
    p.viewportWidth = 1000.0;
    p.scrollLeft = 200.0;
    p.scrollMax = 2000.0;
    p.edgeThreshold = 100.0;
    p.maxScrollSpeed = 15.0;

    // Middle of viewport -> no scroll
    p.mouseXRelative = 500.0;
    TEST_ASSERT(computeEdgeScrollDelta(p) == 0.0);

    // Left edge (x = 50, half threshold) -> -7.5
    p.mouseXRelative = 50.0;
    double leftDelta = computeEdgeScrollDelta(p);
    TEST_ASSERT(std::abs(leftDelta - (-7.5)) < 1e-4);

    // Left edge when scrollLeft == 0 -> delta is 0 (cannot scroll left further)
    p.scrollLeft = 0.0;
    TEST_ASSERT(computeEdgeScrollDelta(p) == 0.0);
    p.scrollLeft = 200.0;

    // Right edge (x = 950, half threshold from right) -> +7.5
    p.mouseXRelative = 950.0;
    double rightDelta = computeEdgeScrollDelta(p);
    TEST_ASSERT(std::abs(rightDelta - 7.5) < 1e-4);

    // Right edge when scrollLeft == scrollMax -> delta is 0
    p.scrollLeft = 2000.0;
    TEST_ASSERT(computeEdgeScrollDelta(p) == 0.0);

    // Clamping
    TEST_ASSERT(applyScrollDelta(10.0, -20.0, 500.0) == 0.0);
    TEST_ASSERT(applyScrollDelta(490.0, 20.0, 500.0) == 500.0);
    TEST_ASSERT(applyScrollDelta(100.0, 25.0, 500.0) == 125.0);

    std::cout << "[PASS] runTimelineEdgeScrollEngineTests" << std::endl;
}

void runTimelineSnapIndicatorEngineTests() {
    using namespace catchim::editor::timeline;

    SnapIndicatorParams p;
    p.snapTimeTicks = catchim::core::TimelineTime::fromSeconds(1.0).ticks(); // 1 second
    p.hasSnapPoint = true;
    p.zoomLevel = 1.0;
    p.scrollLeft = 20.0;
    p.containerHeight = 400.0;
    p.edgePaddingPx = 8.0;

    auto pos = computeSnapIndicatorPosition(p);
    // At zoom 1.0, 1 sec = 50px. Track labels = 140px. Total left = 140 + 50 - 20 = 170px.
    TEST_ASSERT(std::abs(pos.leftPosition - 170.0) < 1e-4);
    TEST_ASSERT(pos.topPosition == 0.0);
    TEST_ASSERT(pos.height == 392.0);

    // Default container height fallback
    p.containerHeight = 0.0;
    auto posDef = computeSnapIndicatorPosition(p);
    TEST_ASSERT(posDef.height == 392.0);

    // No snap point -> time is 0
    p.hasSnapPoint = false;
    auto posNoSnap = computeSnapIndicatorPosition(p);
    // time 0 -> 0px. Total left = 140 + 0 - 20 = 120px.
    TEST_ASSERT(std::abs(posNoSnap.leftPosition - 120.0) < 1e-4);

    std::cout << "[PASS] runTimelineSnapIndicatorEngineTests" << std::endl;
}

void runMediaTypeUtilsTests() {
    using namespace catchim::media;

    TEST_ASSERT(mediaSupportsAudio(MediaType::Audio));
    TEST_ASSERT(mediaSupportsAudio(MediaType::Video));
    TEST_ASSERT(!mediaSupportsAudio(MediaType::Image));

    TEST_ASSERT(mediaSupportsAudioOpt(MediaType::Audio));
    TEST_ASSERT(!mediaSupportsAudioOpt(std::nullopt));

    TEST_ASSERT(getMediaTypeFromMime("image/png") == MediaType::Image);
    TEST_ASSERT(getMediaTypeFromMime("image/jpeg") == MediaType::Image);
    TEST_ASSERT(getMediaTypeFromMime("video/mp4") == MediaType::Video);
    TEST_ASSERT(getMediaTypeFromMime("video/webm") == MediaType::Video);
    TEST_ASSERT(getMediaTypeFromMime("audio/mpeg") == MediaType::Audio);
    TEST_ASSERT(getMediaTypeFromMime("audio/wav") == MediaType::Audio);
    TEST_ASSERT(!getMediaTypeFromMime("application/pdf").has_value());

    std::cout << "[PASS] runMediaTypeUtilsTests" << std::endl;
}

void runPlatformBrowserUtilsTests() {
    using namespace catchim::utils;

#if defined(_WIN32)
    TEST_ASSERT(!isAppleDevice());
    TEST_ASSERT(getPlatformSpecialKey() == "Ctrl");
    TEST_ASSERT(getPlatformAlternateKey() == "Alt");
#endif

    TEST_ASSERT(isTypableDOMElement(DOMElementTag::ContentEditable));
    TEST_ASSERT(isTypableDOMElement(DOMElementTag::Input, false));
    TEST_ASSERT(!isTypableDOMElement(DOMElementTag::Input, true));
    TEST_ASSERT(isTypableDOMElement(DOMElementTag::Textarea, false));
    TEST_ASSERT(!isTypableDOMElement(DOMElementTag::Textarea, true));
    TEST_ASSERT(!isTypableDOMElement(DOMElementTag::Other));

    TEST_ASSERT(isScrollableOverflow("auto", "visible"));
    TEST_ASSERT(isScrollableOverflow("hidden", "scroll"));
    TEST_ASSERT(!isScrollableOverflow("hidden", "hidden"));

    std::cout << "[PASS] runPlatformBrowserUtilsTests" << std::endl;
}

void runMediaProcessingPipelineTests() {
    using namespace catchim::media;

    std::string descHevc = MediaProcessingPipeline::getUnsupportedVideoDescription("hevc");
    TEST_ASSERT(descHevc.find("Safari") != std::string::npos);
    TEST_ASSERT(descHevc.find("HEVC") != std::string::npos);

    std::string descOther = MediaProcessingPipeline::getUnsupportedVideoDescription("prores");
    TEST_ASSERT(descOther.find("PRORES") != std::string::npos);
    TEST_ASSERT(descOther.find("Safari") == std::string::npos);

    std::string limitDesc = MediaProcessingPipeline::getStorageLimitDescription(1024 * 1024, 512 * 1024);
    TEST_ASSERT(limitDesc.find("MB") != std::string::npos);
    TEST_ASSERT(limitDesc.find("512 KB") != std::string::npos);

    TEST_ASSERT(MediaProcessingPipeline::isVideoCodecSupported("h264"));
    TEST_ASSERT(MediaProcessingPipeline::isVideoCodecSupported("av1"));
    TEST_ASSERT(!MediaProcessingPipeline::isVideoCodecSupported("hevc"));

    // validateAndPrepare with unsupported mime
    auto badMime = MediaProcessingPipeline::validateAndPrepare("test.bin", "application/octet-stream", 100, 1000);
    TEST_ASSERT(!badMime.success);

    // validateAndPrepare quota exceeded
    auto quotaExceeded = MediaProcessingPipeline::validateAndPrepare("clip.mp4", "video/mp4", 5000, 1000);
    TEST_ASSERT(!quotaExceeded.success);

    // validateAndPrepare valid
    auto valid = MediaProcessingPipeline::validateAndPrepare("clip.mp4", "video/mp4", 500, 1000);
    TEST_ASSERT(valid.success);
    TEST_ASSERT(valid.asset.has_value());
    TEST_ASSERT(valid.asset->type == MediaType::Video);
    TEST_ASSERT(valid.asset->name == "clip.mp4");

    std::cout << "[PASS] runMediaProcessingPipelineTests" << std::endl;
}

void runKeybindingPersistenceEngineTests() {
    using namespace catchim::editor;

    DecodedKeybindingsState state;
    state.isCustomized = true;
    state.keybindings["ctrl+s"] = "save_project";
    state.keybindings["ctrl+z"] = "undo";

    auto jsonVal = KeybindingPersistenceEngine::serializeKeybindingsState(state);
    TEST_ASSERT(jsonVal["isCustomized"] == true);
    TEST_ASSERT(jsonVal["keybindings"]["ctrl+s"] == "save_project");

    auto decoded = KeybindingPersistenceEngine::decodePersistedKeybindingsState(jsonVal);
    TEST_ASSERT(decoded.has_value());
    TEST_ASSERT(decoded->isCustomized == true);
    TEST_ASSERT(decoded->keybindings.size() == 2);
    TEST_ASSERT(decoded->keybindings["ctrl+s"] == "save_project");

    // Invalid json decode (missing expected fields)
    nlohmann::json invalidJson = nlohmann::json::array();
    auto badDecoded = KeybindingPersistenceEngine::decodePersistedKeybindingsState(invalidJson);
    TEST_ASSERT(!badDecoded.has_value());

    // Strict import parsing
    nlohmann::json validConfig;
    validConfig["ctrl+c"] = "copy";
    validConfig["ctrl+v"] = "paste";
    auto imported = KeybindingPersistenceEngine::parseImportedKeybindings(validConfig);
    TEST_ASSERT(imported.size() == 2);
    TEST_ASSERT(imported["ctrl+c"] == "copy");

    // Strict import parsing error
    bool caught = false;
    try {
        nlohmann::json nonObject = "not_an_object";
        KeybindingPersistenceEngine::parseImportedKeybindings(nonObject);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    TEST_ASSERT(caught);

    std::cout << "[PASS] runKeybindingPersistenceEngineTests" << std::endl;
}

void runMediaTimeUtilsTests() {
    using namespace catchim::core;

    TEST_ASSERT(MediaTimeUtils::roundMediaTime(1.4) == 1LL);
    TEST_ASSERT(MediaTimeUtils::roundMediaTime(1.5) == 2LL);
    TEST_ASSERT(MediaTimeUtils::roundMediaTime(-1.5) == -2LL);
    TEST_ASSERT(MediaTimeUtils::roundMediaTime(0.0) == 0LL);

    TEST_ASSERT(MediaTimeUtils::addMediaTime(10LL, 20LL) == 30LL);
    TEST_ASSERT(MediaTimeUtils::subMediaTime(50LL, 20LL) == 30LL);
    TEST_ASSERT(MediaTimeUtils::maxMediaTime(10LL, 25LL) == 25LL);
    TEST_ASSERT(MediaTimeUtils::minMediaTime(10LL, 25LL) == 10LL);
    TEST_ASSERT(MediaTimeUtils::clampMediaTime(5LL, 10LL, 20LL) == 10LL);
    TEST_ASSERT(MediaTimeUtils::clampMediaTime(25LL, 10LL, 20LL) == 20LL);
    TEST_ASSERT(MediaTimeUtils::clampMediaTime(15LL, 10LL, 20LL) == 15LL);

    FrameRate fps30{30, 1};
    TimelineTime t1 = TimelineTime::fromSeconds(1.0);
    TEST_ASSERT(MediaTimeUtils::roundFrameTime(t1, fps30) == t1);
    TEST_ASSERT(MediaTimeUtils::roundFrameTicks(t1.ticks(), fps30) == t1.ticks());

    TimelineTime dur = TimelineTime::fromSeconds(3.0);
    TimelineTime lastF = MediaTimeUtils::lastFrameMediaTime(dur, fps30);
    TEST_ASSERT(lastF < dur);
    TEST_ASSERT(lastF == dur - fps30.frameDuration());

    TimelineTime seekPast = TimelineTime::fromSeconds(10.0);
    TimelineTime snapped = MediaTimeUtils::snapSeekMediaTime(seekPast, dur, fps30);
    TEST_ASSERT(snapped == lastF);

    std::cout << "[PASS] runMediaTimeUtilsTests" << std::endl;
}

void runPropertyDraftControllerTests() {
    using namespace catchim::editor;

    TEST_ASSERT(PropertyDraftController::looksLikeExpression("100 + 20"));
    TEST_ASSERT(PropertyDraftController::looksLikeExpression("50*2"));
    TEST_ASSERT(PropertyDraftController::looksLikeExpression("100/4"));
    TEST_ASSERT(PropertyDraftController::looksLikeExpression("10 - 2"));
    TEST_ASSERT(!PropertyDraftController::looksLikeExpression("-5"));
    TEST_ASSERT(!PropertyDraftController::looksLikeExpression("120"));
    TEST_ASSERT(!PropertyDraftController::looksLikeExpression(""));

    PropertyDraftController ctrl("100");
    TEST_ASSERT(!ctrl.isEditing());
    TEST_ASSERT(ctrl.displayValue() == "100");

    ctrl.startEditing();
    TEST_ASSERT(ctrl.isEditing());
    ctrl.updateDraft("100 + 50");
    TEST_ASSERT(ctrl.displayValue() == "100 + 50");

    std::string committed = ctrl.commitEditing(true);
    TEST_ASSERT(!ctrl.isEditing());
    TEST_ASSERT(committed == "150");
    TEST_ASSERT(ctrl.displayValue() == "150");

    // Cancel editing
    ctrl.startEditing();
    ctrl.updateDraft("999");
    ctrl.cancelEditing();
    TEST_ASSERT(!ctrl.isEditing());
    TEST_ASSERT(ctrl.displayValue() == "150");

    std::cout << "[PASS] runPropertyDraftControllerTests" << std::endl;
}

void runBookmarkDragControllerTests() {
    using namespace catchim::editor::timeline;
    using namespace catchim::core;

    BookmarkDragController ctrl;
    TEST_ASSERT(!ctrl.isDragging());

    TimelineTime bmTime = TimelineTime::fromSeconds(2.0);
    ctrl.startPendingDrag(bmTime, 100.0, 100.0);
    TEST_ASSERT(!ctrl.isDragging());

    // Small movement (< 3px threshold)
    ctrl.updateDrag(101.0, 101.0, 1.0, false);
    TEST_ASSERT(!ctrl.isDragging());

    // Movement exceeding 3px threshold
    ctrl.updateDrag(150.0, 100.0, 1.0, false); // +50px at zoom 1.0 (50px/s) -> +1.0s
    TEST_ASSERT(ctrl.isDragging());
    TEST_ASSERT(std::abs(ctrl.state().currentTime.toSeconds() - 3.0) < 0.05);

    // Snapping
    TimelineTime snapCandidate = TimelineTime::fromSeconds(3.05);
    ctrl.updateDrag(150.0, 100.0, 1.0, false, {snapCandidate}, 10.0);
    TEST_ASSERT(ctrl.state().snappedPoint.has_value());
    TEST_ASSERT(*ctrl.state().snappedPoint == snapCandidate);
    TEST_ASSERT(ctrl.state().currentTime == snapCandidate);

    // Shift bypasses snapping
    ctrl.updateDrag(150.0, 100.0, 1.0, true, {snapCandidate}, 10.0);
    TEST_ASSERT(!ctrl.state().snappedPoint.has_value());

    auto finalTime = ctrl.endDrag();
    TEST_ASSERT(finalTime.has_value());
    TEST_ASSERT(!ctrl.isDragging());

    std::cout << "[PASS] runBookmarkDragControllerTests" << std::endl;
}

void runTranscriptionLanguagesRegistryTests() {
    using namespace catchim::subtitles;

    const auto& all = TranscriptionLanguagesRegistry::getAllLanguages();
    TEST_ASSERT(all.size() == 10);

    auto vi = TranscriptionLanguagesRegistry::findByCode("vi");
    TEST_ASSERT(vi.has_value());
    TEST_ASSERT(vi->name == "Vietnamese");
    TEST_ASSERT(vi->nameVi == "Tiếng Việt");

    auto en = TranscriptionLanguagesRegistry::findByCode("en");
    TEST_ASSERT(en.has_value());
    TEST_ASSERT(en->name == "English");

    TEST_ASSERT(TranscriptionLanguagesRegistry::isValidLanguageCode("ja"));
    TEST_ASSERT(!TranscriptionLanguagesRegistry::isValidLanguageCode("nonexistent"));

    auto fr = TranscriptionLanguagesRegistry::findByName("Tiếng Pháp");
    TEST_ASSERT(fr.has_value());
    TEST_ASSERT(fr->code == "fr");

    std::cout << "[PASS] runTranscriptionLanguagesRegistryTests" << std::endl;
}

void runMediaUploadToastEngineTests() {
    using namespace catchim::media;

    TEST_ASSERT(MediaUploadToastEngine::getAssetLabel(1) == "media asset");
    TEST_ASSERT(MediaUploadToastEngine::getAssetLabel(3) == "media assets");

    TEST_ASSERT(MediaUploadToastEngine::formatLoadingMessage(2) == "Uploading media assets...");

    TEST_ASSERT(MediaUploadToastEngine::formatSuccessMessage(1, {"clip.mp4"}) == "clip.mp4 has been uploaded");
    TEST_ASSERT(MediaUploadToastEngine::formatSuccessMessage(1, {}) == "1 media asset has been uploaded");
    TEST_ASSERT(MediaUploadToastEngine::formatSuccessMessage(5) == "5 media assets have been uploaded");
    TEST_ASSERT(MediaUploadToastEngine::formatSuccessMessage(0) == "No media assets were uploaded");

    TEST_ASSERT(MediaUploadToastEngine::formatErrorMessage(4) == "Failed to upload media assets");

    std::cout << "[PASS] runMediaUploadToastEngineTests" << std::endl;
}

void runEditorAppStoreEngineTests() {
    using namespace catchim::editor;

    EditorAppStoreEngine store;
    TEST_ASSERT(store.isInitializing());
    TEST_ASSERT(!store.isPanelsReady());

    TEST_ASSERT(!store.getCanvasPresets().empty());
    TEST_ASSERT(store.getCanvasPresets()[0].name == "16:9 Landscape");

    store.initializeApp();
    TEST_ASSERT(!store.isInitializing());
    TEST_ASSERT(store.isPanelsReady());

    store.resetState();
    TEST_ASSERT(store.isInitializing());
    TEST_ASSERT(!store.isPanelsReady());

    std::cout << "[PASS] runEditorAppStoreEngineTests" << std::endl;
}

int main() {
    std::cout << "Starting Catchim C++ Core & Editor Parity Tests..." << std::endl;
    runTimeTests();
    runBezierTests();
    runTimelineTests();
    runCommandHistoryTests();
    runSerializationTests();
    runActionRegistryTests();
    runBookmarkTests();
    runAdvancedEditingTests();
    runKeyframeAnimationTests();
    runExportPipelineTests();
    runAudioSeparationTests();
    runClipboardTests();
    runSceneManagementTests();
    runVisualEffectsTests();
    runStructuredEffectStackRenderTests();
    runTextRasterizerTests();
    runAudioFadeAndRampingTests();
    runCropAndTransformTests();
    runShapeMaskTests();
    runProjectMigrationTests();
    runTimelineZoomTests();
    runSelectionTests();
    runRetimeEngineTests();
    runSrtSubtitleTests();
    runShapeAndGradientRendererTests();
    runExtendedColorGradingTests();
    runAudioMasteringTests();
    runSafeZoneGuideTests();
    runTransitionEngineTests();
    runRipplePipelineTests();
    runProjectBundleTests();
    runWaveformBucketerTests();
    runHitTestingTests();
    runExportGeometryResolverTests();
    runProjectDiagnosticsTests();
    runStillImageExporterTests();
    runRulerAndTimelineViewModelTests();
    runPreviewSnapAndViewModelTests();
    runAudioPlaybackEngineTests();
    runVideoFrameCacheTests();
    runPropertiesViewModelTests();
    runTranscriptionAndCaptionTests();
    runStickerAndGraphicRegistryTests();
    runSpatialMotionPathTests();
    runColorWheelGradingTests();
    runAudioDuckingTests();
    runPresetManagerTests();
    runEndToEndPipelineIntegrationTests();
    runGroupMoveEngineTests();
    runTtsEngineAndWavWriterTests();
    runSoundEffectsRegistryTests();
    runFontRegistryTests();
    runRationalFrameRateTests();
    runGroupResizeEngineTests();
    runPlacementEngineTests();
    runAudioDspFiltersTests();
    runColorUtilsTests();
    runTimelineLayoutEngineTests();
    runAudioDisplayMetricsTests();
    runAdvancedSnapEngineTests();
    runBatchCommandAndTracksSnapshotTests();
    runElementFactoryAndUtilsTests();
    runClipboardKeyframeEngineTests();
    runTrackCommandsAndDuplicationTests();
    runBlurEffectAndPassEngineTests();
    runGraphicGeometryAndStrokeTests();
    runInteractiveKeyframeCommandsTests();
    runTextLayoutAndTypographyEngineTests();
    runFreeformMaskGeometryTests();
    runMaskCommandsAndFreeformTests();
    runEffectCommandsTests();
    runCanvasViewportControllerTests();
    runMultiElementCommandsTests();
    runEffectParamKeyframingTests();
    runTransformHandleSessionTests();
    runSplitElementsCommandTests();
    runPreviewInteractionEngineTests();
    runAudioRetimeEngineTests();
    runMoveElementsCommandTests();
    runInsertElementCommandTests();
    runProjectAndBookmarkCommandsTests();
    runSceneCommandsTests();
    runMediaCommandsTests();
    runPreviewTrackerTests();
    runAudioSeparationEngineAndToggleTests();
    runTimelineDragSourceTests();
    runEditorSelectionHierarchyTests();
    runWaveformSummaryEngineTests();
    runCanvasBackgroundEngineTests();
    runProjectManagerTests();
    runAudioStateEngineTests();
    runPreviewCoordinateTransformerTests();
    runElementBoundsEngineTests();
    runKeybindingEngineTests();
    runProjectAutosaveEngineTests();
    runScenesManagerTests();
    runMathExpressionEvaluatorTests();
    runTrackCapabilityEngineTests();
    runStorageQuotaEngineTests();
    runTimelineElementUpdatePipelineTests();
    runExportCanvasGeometryResolverTests();
    runMediaAssetInspectorTests();
    runTimelineCoordinateEngineTests();
    runProjectFrameRateEngineTests();
    runGuideOverlayEngineTests();
    runElementParamRegistryTests();
    runTextRenderingPrimitivesTests();
    runTimelineElementBuilderTests();
    runAssSubtitleParserAndElementBuilderTests();
    runCssGradientEngineTests();
    runCanvasTransformPipelineTests();
    runRenderPerformanceProfilerAndDiagnosticsRegistryTests();
    runCanvasPresetEngineTests();
    runEuclideanFrameSnapperTests();
    runBuiltinMaskGeometryTests();
    runMaskSnapEngineTests();
    runMaskInteractionAndRegistryTests();
    runSceneNodesAndBuilderTests();
    runSceneTreeResolverTests();
    runFrameDescriptorBuilderTests();
    runRenderSurfaceTests();
    runTextureCacheManagerTests();
    runCanvasRendererAndEffectPreviewTests();
    runTimelineSnappingEngineTests();
    runTimelineSnapPointSourcesTests();
    runAnimationTargetResolverTests();
    runTimelineDefaultsTests();
    runAnimationTransformResolverTests();
    runPreviewOverlayManagerTests();
    runInteractionCancellationRegistryTests();
    runPanelLayoutManagerTests();
    runMediaThumbnailEngineTests();
    runRenderingParamsResolverTests();
    runTrackDefaultsTests();
    runMathFormattingUtilsTests();
    runExportDefaultsTests();
    runBookmarkEngineTests();
    runTimelineUiStoreTests();
    runTrackCompatibilityEngineTests();
    runTrackInsertResolverTests();
    runGroupMoveSnapEngineTests();
    runRippleDiffEngineTests();
    runTrackElementUpdateEngineTests();
    runKeybindingMigrationEngineTests();
    runAnimationKeyframeQueryEngineTests();
    runCommandReactorPipelineTests();
    runTimelinePixelUtilsTests();
    runTimelineZoomUtilsTests();
    runTimelineDragDataTests();
    runBackgroundPresetsTests();
    runParamChannelLayoutEngineTests();
    runI18nEngineTests();
    runRetimeResolutionEngineTests();
    runEffectDefinitionRegistryTests();
    runCanvasSizePresetsTests();
    runSceneHierarchyUtilsTests();
    runUuidGeneratorTests();
    runStringUtilsTests();
    runGeometryUtilsTests();
    runDateUtilsTests();
    runRenderParamResolversTests();
    runAnimationValueResolversTests();
    runTimelineTrackDefaultsTests();
    runTtsVoiceRegistryTests();
    runPlaybackManagerTests();
    runTimelineManagerTests();
    runRendererManagerTests();
    runSaveManagerTests();
    runMediaManagerTests();
    runAudioManagerTests();
    runTranscriptionCatalogTests();
    runMaskGeometryUtilsTests();
    runStickerIdUtilsTests();
    runEditorCoreTests();
    runSavedSoundsStoreTests();
    runCanvasSnapMathTests();
    runTranscriptionCaptionBuilderTests();
    runDiagnosticsManagerTests();
    runGraphicsDefinitionsTests();
    runCommandManagerTests();
    runWaveformCacheTests();
    runBackgroundBlurPresetsTests();
    runPatternCraftGradientsTests();
    runExportOptionsResolverTests();
    runTranscriptionServiceTests();
    runTtsVoiceServiceTests();
    runPanelLayoutConfigTests();
    runAudioMediaUtilsTests();
    runSeekControllerTests();
    runPlayheadControllerTests();
    runKeyframeDragControllerTests();
    runResizeControllerTests();
    runSelectionStateEngineTests();
    runTimelineCreationDefaultsTests();
    runTimelineDropTargetResolverTests();
    runTimelineDragDropControllerTests();
    runTimelineElementInteractionControllerTests();
    runTimelineInteractionMetricsTests();
    runTimelineElementFactoryTests();
    runTimelineDragUtilsTests();
    runTimelineInteractiveZoomControllerTests();
    runGraphEditorSessionEngineTests();
    runGraphEditorEasingPresetsTests();
    runTrackLayoutMetricsTests();
    runSelectionHitTestingTests();
    runExportMimeTypesAndLayersTests();
    runTimelineThemeTests();
    runPreviewSettingsStoreTests();
    runRetimeRateEngineTests();
    runTransformHandleControllerTests();
    runPreviewInteractionGestureControllerTests();
    runMaskFeatherEngineTests();
    runOpencutNativeCoreBindingsTests();
    runPanelStoreEngineTests();
    runTextElementMeasurementEngineTests();
    runEditableTimecodeControllerTests();
    runProjectOrganizationEngineTests();
    runStorageServiceCoordinatorTests();
    runAudioVolumeLineEngineTests();
    runAudioWaveformBarEngineTests();
    runTtsServiceEngineTests();
    runTimelineSceneUtilsTests();
    runAudibleCandidateCollectorTests();
    runTimelineToolbarEngineTests();
    runRulerIntervalEngineTests();
    runTimelineExpandedLayoutEngineTests();
    runAssetsPanelStoreEngineTests();
    runPropertiesPanelStoreEngineTests();
    runSpeedInputControllerTests();
    runNativeFileSystemStorageAdapterTests();
    runTimelineEdgeScrollEngineTests();
    runTimelineSnapIndicatorEngineTests();
    runMediaTypeUtilsTests();
    runPlatformBrowserUtilsTests();
    runMediaProcessingPipelineTests();
    runKeybindingPersistenceEngineTests();
    runMediaTimeUtilsTests();
    runPropertyDraftControllerTests();
    runBookmarkDragControllerTests();
    runTranscriptionLanguagesRegistryTests();
    runMediaUploadToastEngineTests();
    runEditorAppStoreEngineTests();
    std::cout << ">>> ALL 241 PARITY TEST SUITES PASSED SUCCESSFULLY! <<<" << std::endl;
    return 0;
}

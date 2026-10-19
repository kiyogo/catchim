cmake_minimum_required(VERSION 3.20)
project(CatchimApp VERSION 0.1.0 LANGUAGES CXX)

# An unset build type otherwise produces -O0 WASM: the per-pixel preview
# compositor then cannot keep up with playback, even with native decoding.
if(EMSCRIPTEN AND NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler options & warnings
if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(EMSCRIPTEN)
        # Qt's multithreaded WASM kit links with shared memory. Every app
        # translation unit (including the platform-neutral core libraries)
        # must be compiled with the matching atomics/bulk-memory features.
        add_compile_options(-pthread)
    endif()
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Third party: nlohmann_json
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Check for the portable Qt modules first. Qt WebEngine is a desktop-only
# module, so including it in the main component lookup makes a valid
# Qt-for-WebAssembly installation look like Qt is missing altogether.
find_package(Qt6 COMPONENTS Core Gui Widgets Svg OpenGL Network QUIET)
if(Qt6_FOUND)
    message(STATUS "Qt6 found: ${Qt6_VERSION}")
    set(HAVE_QT6 ON)
    add_compile_definitions(HAVE_QT6)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    # Qt WebEngine embeds Chromium and is not available in Qt for WebAssembly.
    # Keep it an optional desktop host rather than a requirement for Qt UI.
    if(NOT EMSCRIPTEN)
        find_package(Qt6 COMPONENTS WebEngineWidgets WebChannel QUIET)
        if(TARGET Qt6::WebEngineWidgets AND TARGET Qt6::WebChannel)
            message(STATUS "Qt WebEngine desktop host enabled")
            set(HAVE_QT6_WEBENGINE ON)
            add_compile_definitions(HAVE_QT6_WEBENGINE HAVE_QT6_WEBCHANNEL)
        endif()
    endif()
else()
    message(STATUS "Qt6 not found directly on system path. Core, Editor, Media, and Tests can still build.")
    set(HAVE_QT6 OFF)
endif()

# Core library (Zero Qt dependency, Pure C++20)
add_library(catchim_core STATIC
    src/core/logging/Logger.cpp
    src/core/logging/Logger.h
    src/core/errors/Errors.h
    src/core/time/TimelineTime.cpp
    src/core/time/TimelineTime.h
    src/core/time/RationalFrameRate.cpp
    src/core/time/RationalFrameRate.h
    src/core/time/Timecode.cpp
    src/core/time/Timecode.h
    src/core/ids/Ids.h
    src/core/math/Bezier.cpp
    src/core/math/Bezier.h
    src/core/math/MathExpressionEvaluator.cpp
    src/core/math/MathExpressionEvaluator.h
    src/core/time/ProjectFrameRateEngine.cpp
    src/core/time/ProjectFrameRateEngine.h
    src/core/time/EuclideanFrameSnapper.cpp
    src/core/time/EuclideanFrameSnapper.h
    src/core/math/MathFormattingUtils.cpp
    src/core/math/MathFormattingUtils.h
    src/core/utils/ColorUtils.cpp
    src/core/utils/ColorUtils.h
    src/core/i18n/I18nEngine.cpp
    src/core/i18n/I18nEngine.h
    src/core/utils/UuidGenerator.cpp
    src/core/utils/UuidGenerator.h
    src/core/utils/StringUtils.cpp
    src/core/utils/StringUtils.h
    src/core/utils/GeometryUtils.cpp
    src/core/utils/GeometryUtils.h
    src/core/utils/DateUtils.cpp
    src/core/utils/DateUtils.h
    src/native/OpencutNativeCoreBindings.cpp
    src/native/OpencutNativeCoreBindings.h
    src/core/project/ProjectOrganizationEngine.cpp
    src/core/project/ProjectOrganizationEngine.h
    src/storage/StorageServiceCoordinator.cpp
    src/storage/StorageServiceCoordinator.h
    src/utils/PlatformBrowserUtils.cpp
    src/utils/PlatformBrowserUtils.h
    src/core/time/MediaTimeUtils.cpp
    src/core/time/MediaTimeUtils.h
)
target_link_libraries(catchim_core PUBLIC nlohmann_json::nlohmann_json)

# Editor library (Project, Timeline, Tracks, Clips, History, Commands)
add_library(catchim_editor STATIC
    src/editor/project/ProjectSettings.h
    src/editor/project/Project.cpp
    src/editor/project/Project.h
    src/editor/project/ProjectSerializer.cpp
    src/editor/project/ProjectSerializer.h
    src/editor/timeline/Clip.cpp
    src/editor/timeline/Clip.h
    src/editor/timeline/Track.cpp
    src/editor/timeline/Track.h
    src/editor/timeline/Timeline.cpp
    src/editor/timeline/Timeline.h
    src/editor/timeline/Bookmark.h
    src/editor/timeline/SnapEngine.cpp
    src/editor/timeline/SnapEngine.h
    src/editor/history/Command.h
    src/editor/history/CommandHistory.cpp
    src/editor/history/CommandHistory.h
    src/editor/history/commands/TimelineCommands.cpp
    src/editor/history/commands/TimelineCommands.h
    src/editor/history/commands/AdvancedTimelineCommands.cpp
    src/editor/history/commands/AdvancedTimelineCommands.h
    src/editor/history/commands/AudioSeparationCommands.cpp
    src/editor/history/commands/AudioSeparationCommands.h
    src/editor/history/commands/BatchCommand.cpp
    src/editor/history/commands/BatchCommand.h
    src/editor/history/commands/TrackCommands.cpp
    src/editor/history/commands/TrackCommands.h
    src/editor/history/commands/DuplicateElementsCommand.cpp
    src/editor/history/commands/DuplicateElementsCommand.h
    src/editor/history/commands/KeyframeCommands.cpp
    src/editor/history/commands/KeyframeCommands.h
    src/editor/history/commands/EffectCommands.cpp
    src/editor/history/commands/EffectCommands.h
    src/editor/history/commands/ElementCommands.cpp
    src/editor/history/commands/ElementCommands.h
    src/editor/history/commands/SplitElementsCommand.cpp
    src/editor/history/commands/SplitElementsCommand.h
    src/editor/history/commands/MoveElementsCommand.cpp
    src/editor/history/commands/MoveElementsCommand.h
    src/editor/history/commands/InsertElementCommand.cpp
    src/editor/history/commands/InsertElementCommand.h
    src/editor/history/commands/ProjectCommands.cpp
    src/editor/history/commands/ProjectCommands.h
    src/editor/history/commands/SceneCommands.cpp
    src/editor/history/commands/SceneCommands.h
    src/editor/history/commands/MediaCommands.cpp
    src/editor/history/commands/MediaCommands.h
    src/editor/history/commands/PreviewTracker.h
    src/editor/history/commands/ToggleSourceAudioSeparationCommand.cpp
    src/editor/history/commands/ToggleSourceAudioSeparationCommand.h
    src/editor/timeline/AudioSeparationEngine.cpp
    src/editor/timeline/AudioSeparationEngine.h
    src/editor/timeline/TimelineDragSource.cpp
    src/editor/timeline/TimelineDragSource.h
    src/editor/selection/EditorSelection.cpp
    src/editor/selection/EditorSelection.h
    src/editor/animation/EffectParamAnimationEngine.cpp
    src/editor/animation/EffectParamAnimationEngine.h
    src/editor/history/commands/EffectKeyframeCommands.cpp
    src/editor/history/commands/EffectKeyframeCommands.h
    src/editor/clipboard/ClipboardManager.cpp
    src/editor/clipboard/ClipboardManager.h
    src/editor/clipboard/ClipboardKeyframeEngine.cpp
    src/editor/clipboard/ClipboardKeyframeEngine.h
    src/editor/animation/Keyframe.h
    src/editor/animation/AnimationChannel.cpp
    src/editor/animation/AnimationChannel.h
    src/editor/animation/SpatialMotionPath.cpp
    src/editor/animation/SpatialMotionPath.h
    src/editor/timeline/ElementFactory.cpp
    src/editor/timeline/ElementFactory.h
    src/editor/timeline/ElementUtils.cpp
    src/editor/timeline/ElementUtils.h
    src/editor/actions/ActionRegistry.cpp
    src/editor/actions/ActionRegistry.h
    src/editor/playback/PlaybackClock.cpp
    src/editor/playback/PlaybackClock.h
    src/editor/playback/PlaybackController.cpp
    src/editor/playback/PlaybackController.h
    src/editor/timeline/TimelineZoomController.cpp
    src/editor/timeline/TimelineZoomController.h
    src/editor/selection/SelectionManager.cpp
    src/editor/selection/SelectionManager.h
    src/editor/retime/RetimeEngine.cpp
    src/editor/retime/RetimeEngine.h
    src/editor/history/commands/ImportSubtitlesCommand.cpp
    src/editor/history/commands/ImportSubtitlesCommand.h
    src/editor/timeline/RipplePipeline.cpp
    src/editor/timeline/RipplePipeline.h
    src/editor/timeline/RulerEngine.cpp
    src/editor/timeline/RulerEngine.h
    src/editor/timeline/TimelineViewModel.cpp
    src/editor/timeline/TimelineViewModel.h
    src/editor/timeline/GroupMoveEngine.cpp
    src/editor/timeline/GroupMoveEngine.h
    src/editor/timeline/GroupResizeEngine.cpp
    src/editor/timeline/GroupResizeEngine.h
    src/editor/timeline/PlacementEngine.cpp
    src/editor/timeline/PlacementEngine.h
    src/editor/timeline/TimelineLayoutEngine.cpp
    src/editor/timeline/TimelineLayoutEngine.h
    src/editor/timeline/AdvancedSnapEngine.cpp
    src/editor/timeline/AdvancedSnapEngine.h
    src/editor/properties/PropertiesViewModel.cpp
    src/editor/properties/PropertiesViewModel.h
    src/editor/project/ProjectDiagnostics.cpp
    src/editor/project/ProjectDiagnostics.h
    src/editor/project/ProjectManager.cpp
    src/editor/project/ProjectManager.h
    src/editor/canvas/CanvasBackgroundEngine.cpp
    src/editor/canvas/CanvasBackgroundEngine.h
    src/editor/actions/KeybindingEngine.cpp
    src/editor/actions/KeybindingEngine.h
    src/editor/project/ProjectAutosaveEngine.cpp
    src/editor/project/ProjectAutosaveEngine.h
    src/editor/scene/ScenesManager.cpp
    src/editor/scene/ScenesManager.h
    src/editor/timeline/TrackCapabilityEngine.cpp
    src/editor/timeline/TrackCapabilityEngine.h
    src/editor/timeline/TimelineElementUpdatePipeline.cpp
    src/editor/timeline/TimelineElementUpdatePipeline.h
    src/editor/timeline/TimelineCoordinateEngine.cpp
    src/editor/timeline/TimelineCoordinateEngine.h
    src/editor/params/ElementParamRegistry.cpp
    src/editor/params/ElementParamRegistry.h
    src/editor/timeline/TimelineElementBuilder.cpp
    src/editor/timeline/TimelineElementBuilder.h
    src/editor/subtitles/AssSubtitleParser.cpp
    src/editor/subtitles/AssSubtitleParser.h
    src/editor/subtitles/SubtitleElementBuilder.cpp
    src/editor/subtitles/SubtitleElementBuilder.h
    src/editor/diagnostics/DiagnosticsRegistry.cpp
    src/editor/diagnostics/DiagnosticsRegistry.h
    src/editor/timeline/TimelineSnappingEngine.cpp
    src/editor/timeline/TimelineSnappingEngine.h
    src/editor/timeline/TimelineSnapPointSources.cpp
    src/editor/timeline/TimelineSnapPointSources.h
    src/editor/animation/AnimationTargetResolver.cpp
    src/editor/animation/AnimationTargetResolver.h
    src/editor/timeline/TimelineDefaults.cpp
    src/editor/timeline/TimelineDefaults.h
    src/editor/animation/AnimationTransformResolver.cpp
    src/editor/animation/AnimationTransformResolver.h
    src/editor/cancel/InteractionCancellationRegistry.cpp
    src/editor/cancel/InteractionCancellationRegistry.h
    src/editor/panels/PanelLayoutManager.cpp
    src/editor/panels/PanelLayoutManager.h
    src/editor/timeline/TrackDefaults.cpp
    src/editor/timeline/TrackDefaults.h
    src/editor/timeline/BookmarkEngine.cpp
    src/editor/timeline/BookmarkEngine.h
    src/editor/timeline/TimelineUiStore.cpp
    src/editor/timeline/TimelineUiStore.h
    src/editor/timeline/TrackCompatibilityEngine.cpp
    src/editor/timeline/TrackCompatibilityEngine.h
    src/editor/timeline/TrackInsertResolver.cpp
    src/editor/timeline/TrackInsertResolver.h
    src/editor/timeline/GroupMoveSnapEngine.cpp
    src/editor/timeline/GroupMoveSnapEngine.h
    src/editor/timeline/RippleDiffEngine.cpp
    src/editor/timeline/RippleDiffEngine.h
    src/editor/timeline/TrackElementUpdateEngine.cpp
    src/editor/timeline/TrackElementUpdateEngine.h
    src/editor/actions/KeybindingMigrationEngine.cpp
    src/editor/actions/KeybindingMigrationEngine.h
    src/editor/animation/AnimationKeyframeQueryEngine.cpp
    src/editor/animation/AnimationKeyframeQueryEngine.h
    src/editor/history/CommandReactorPipeline.cpp
    src/editor/history/CommandReactorPipeline.h
    src/editor/timeline/TimelinePixelUtils.cpp
    src/editor/timeline/TimelinePixelUtils.h
    src/editor/timeline/TimelineZoomUtils.cpp
    src/editor/timeline/TimelineZoomUtils.h
    src/editor/timeline/TimelineDragData.cpp
    src/editor/timeline/TimelineDragData.h
    src/editor/params/ParamChannelLayoutEngine.cpp
    src/editor/params/ParamChannelLayoutEngine.h
    src/editor/retime/RetimeResolutionEngine.cpp
    src/editor/retime/RetimeResolutionEngine.h
    src/editor/scene/SceneHierarchyUtils.cpp
    src/editor/scene/SceneHierarchyUtils.h
    src/editor/animation/AnimationValueResolvers.cpp
    src/editor/animation/AnimationValueResolvers.h
    src/editor/timeline/TimelineTrackDefaults.cpp
    src/editor/timeline/TimelineTrackDefaults.h
    src/editor/playback/PlaybackManager.cpp
    src/editor/playback/PlaybackManager.h
    src/editor/timeline/TimelineManager.cpp
    src/editor/timeline/TimelineManager.h
    src/editor/project/SaveManager.cpp
    src/editor/project/SaveManager.h
    src/editor/EditorEngine.cpp
    src/editor/EditorEngine.h
    src/editor/core/EditorCore.cpp
    src/editor/core/EditorCore.h
    src/editor/diagnostics/DiagnosticsManager.cpp
    src/editor/diagnostics/DiagnosticsManager.h
    src/editor/commands/CommandManager.cpp
    src/editor/commands/CommandManager.h
    src/editor/panels/PanelLayoutConfig.cpp
    src/editor/panels/PanelLayoutConfig.h
    src/editor/timeline/controllers/SeekController.cpp
    src/editor/timeline/controllers/SeekController.h
    src/editor/timeline/controllers/PlayheadController.cpp
    src/editor/timeline/controllers/PlayheadController.h
    src/editor/timeline/controllers/KeyframeDragController.cpp
    src/editor/timeline/controllers/KeyframeDragController.h
    src/editor/timeline/controllers/ResizeController.cpp
    src/editor/timeline/controllers/ResizeController.h
    src/editor/selection/SelectionStateEngine.cpp
    src/editor/selection/SelectionStateEngine.h
    src/editor/timeline/TimelineCreationDefaults.cpp
    src/editor/timeline/TimelineCreationDefaults.h
    src/editor/timeline/controllers/TimelineDropTargetResolver.cpp
    src/editor/timeline/controllers/TimelineDropTargetResolver.h
    src/editor/timeline/controllers/TimelineDragDropController.cpp
    src/editor/timeline/controllers/TimelineDragDropController.h
    src/editor/timeline/controllers/TimelineElementInteractionController.cpp
    src/editor/timeline/controllers/TimelineElementInteractionController.h
    src/editor/timeline/TimelineInteractionMetrics.cpp
    src/editor/timeline/TimelineInteractionMetrics.h
    src/editor/timeline/TimelineElementFactory.cpp
    src/editor/timeline/TimelineElementFactory.h
    src/editor/timeline/TimelineDragUtils.cpp
    src/editor/timeline/TimelineDragUtils.h
    src/editor/timeline/controllers/TimelineInteractiveZoomController.cpp
    src/editor/timeline/controllers/TimelineInteractiveZoomController.h
    src/editor/animation/GraphEditorSessionEngine.cpp
    src/editor/animation/GraphEditorSessionEngine.h
    src/editor/animation/GraphEditorEasingPresets.cpp
    src/editor/animation/GraphEditorEasingPresets.h
    src/editor/timeline/TrackLayoutMetrics.cpp
    src/editor/timeline/TrackLayoutMetrics.h
    src/editor/timeline/SelectionHitTesting.cpp
    src/editor/timeline/SelectionHitTesting.h
    src/editor/export/ExportMimeTypesAndLayers.cpp
    src/editor/export/ExportMimeTypesAndLayers.h
    src/editor/timeline/TimelineTheme.cpp
    src/editor/timeline/TimelineTheme.h
    src/editor/preview/PreviewSettingsStore.cpp
    src/editor/preview/PreviewSettingsStore.h
    src/editor/retime/RetimeRateEngine.cpp
    src/editor/retime/RetimeRateEngine.h
    src/editor/preview/controllers/TransformHandleController.cpp
    src/editor/preview/controllers/TransformHandleController.h
    src/editor/preview/controllers/PreviewInteractionGestureController.cpp
    src/editor/preview/controllers/PreviewInteractionGestureController.h
    src/editor/panels/PanelStoreEngine.cpp
    src/editor/panels/PanelStoreEngine.h
    src/editor/timecode/EditableTimecodeController.cpp
    src/editor/timecode/EditableTimecodeController.h
    src/editor/scene/TimelineSceneUtils.cpp
    src/editor/scene/TimelineSceneUtils.h
    src/editor/timeline/TimelineToolbarEngine.cpp
    src/editor/timeline/TimelineToolbarEngine.h
    src/editor/timeline/RulerIntervalEngine.cpp
    src/editor/timeline/RulerIntervalEngine.h
    src/editor/timeline/TimelineExpandedLayoutEngine.cpp
    src/editor/timeline/TimelineExpandedLayoutEngine.h
    src/editor/panels/AssetsPanelStoreEngine.cpp
    src/editor/panels/AssetsPanelStoreEngine.h
    src/editor/panels/PropertiesPanelStoreEngine.cpp
    src/editor/panels/PropertiesPanelStoreEngine.h
    src/editor/retime/SpeedInputController.cpp
    src/editor/retime/SpeedInputController.h
    src/editor/timeline/TimelineEdgeScrollEngine.cpp
    src/editor/timeline/TimelineEdgeScrollEngine.h
    src/editor/timeline/TimelineSnapIndicatorEngine.cpp
    src/editor/timeline/TimelineSnapIndicatorEngine.h
    src/editor/actions/KeybindingPersistenceEngine.cpp
    src/editor/actions/KeybindingPersistenceEngine.h
    src/editor/properties/PropertyDraftController.cpp
    src/editor/properties/PropertyDraftController.h
    src/editor/timeline/controllers/BookmarkDragController.cpp
    src/editor/timeline/controllers/BookmarkDragController.h
    src/editor/core/EditorAppStoreEngine.cpp
    src/editor/core/EditorAppStoreEngine.h
)
target_link_libraries(catchim_editor PUBLIC catchim_core catchim_subtitles)

# Subtitles library (SRT parser, serializer, transcription chunking)
add_library(catchim_subtitles STATIC
    src/subtitles/SubtitleCue.h
    src/subtitles/SrtParser.cpp
    src/subtitles/SrtParser.h
    src/subtitles/SrtSerializer.cpp
    src/subtitles/SrtSerializer.h
    src/subtitles/TranscriptionEngine.cpp
    src/subtitles/TranscriptionEngine.h
    src/subtitles/TranscriptionCatalog.cpp
    src/subtitles/TranscriptionCatalog.h
    src/subtitles/TranscriptionCaptionBuilder.cpp
    src/subtitles/TranscriptionCaptionBuilder.h
    src/subtitles/TranscriptionService.cpp
    src/subtitles/TranscriptionService.h
    src/subtitles/TranscriptionLanguagesRegistry.cpp
    src/subtitles/TranscriptionLanguagesRegistry.h
)
target_link_libraries(catchim_subtitles PUBLIC catchim_core)

# Media library (Media asset, Probe, Decoders, Waveform, Thumbnail, Stickers, Presets, SFX)
add_library(catchim_media STATIC
    src/media/MediaAsset.cpp
    src/media/MediaAsset.h
    src/media/MediaLibrary.cpp
    src/media/MediaLibrary.h
    src/media/probe/MediaProbe.cpp
    src/media/probe/MediaProbe.h
    src/media/waveform/WaveformGenerator.cpp
    src/media/waveform/WaveformGenerator.h
    src/media/waveform/WaveformBucketer.cpp
    src/media/waveform/WaveformBucketer.h
    src/media/waveform/WaveformSummaryEngine.cpp
    src/media/waveform/WaveformSummaryEngine.h
    src/media/thumbnail/ThumbnailGenerator.cpp
    src/media/thumbnail/ThumbnailGenerator.h
    src/media/MediaThumbnailEngine.cpp
    src/media/MediaThumbnailEngine.h
    src/media/VideoFrameCache.cpp
    src/media/VideoFrameCache.h
    src/media/StickerRegistry.cpp
    src/media/StickerRegistry.h
    src/media/PresetManager.cpp
    src/media/PresetManager.h
    src/media/SoundEffectsRegistry.cpp
    src/media/SoundEffectsRegistry.h
    src/media/MediaAssetInspector.cpp
    src/media/MediaAssetInspector.h
    src/media/TtsVoiceRegistry.cpp
    src/media/TtsVoiceRegistry.h
    src/media/MediaManager.cpp
    src/media/MediaManager.h
    src/media/StickerIdUtils.cpp
    src/media/StickerIdUtils.h
    src/media/SavedSoundsStore.cpp
    src/media/SavedSoundsStore.h
    src/media/WaveformCache.cpp
    src/media/WaveformCache.h
    src/media/TtsVoiceService.cpp
    src/media/TtsVoiceService.h
    src/media/MediaTypeUtils.cpp
    src/media/MediaTypeUtils.h
    src/media/MediaProcessingPipeline.cpp
    src/media/MediaProcessingPipeline.h
    src/media/decoder/NativeVideoDecoder.cpp
    src/media/decoder/NativeVideoDecoder.h
    src/media/decoder/NativeAudioDecoder.cpp
    src/media/decoder/NativeAudioDecoder.h
    src/media/MediaUploadToastEngine.cpp
    src/media/MediaUploadToastEngine.h
)
target_link_libraries(catchim_media PUBLIC catchim_core catchim_editor catchim_storage)
if(WIN32)
    target_link_libraries(catchim_media PUBLIC mfplat mfreadwrite mfuuid propsys d3d11 dxgi)
endif()
if(HAVE_QT6)
    target_link_libraries(catchim_media PUBLIC Qt6::Gui)
endif()

# Render & Audio libraries
add_library(catchim_render STATIC
    src/render/Transform.h
    src/render/Compositor.cpp
    src/render/Compositor.h
    src/render/RenderEngine.cpp
    src/render/RenderEngine.h
    src/render/TextRasterizer.cpp
    src/render/TextRasterizer.h
    src/render/MaskEngine.cpp
    src/render/MaskEngine.h
    src/render/ShapeRenderer.cpp
    src/render/ShapeRenderer.h
    src/render/SafeZoneGuide.cpp
    src/render/SafeZoneGuide.h
    src/render/TransitionEngine.cpp
    src/render/TransitionEngine.h
    src/render/HitTesting.cpp
    src/render/HitTesting.h
    src/render/PreviewSnap.cpp
    src/render/PreviewSnap.h
    src/render/PreviewViewModel.cpp
    src/render/PreviewViewModel.h
    src/render/ColorWheelEngine.cpp
    src/render/ColorWheelEngine.h
    src/render/FontRegistry.cpp
    src/render/FontRegistry.h
    src/render/effects/BlurEffect.cpp
    src/render/effects/BlurEffect.h
    src/render/effects/MaskFeatherEngine.cpp
    src/render/effects/MaskFeatherEngine.h
    src/render/graphics/GraphicGeometry.cpp
    src/render/graphics/GraphicGeometry.h
    src/render/text/TextLayoutEngine.cpp
    src/render/text/TextLayoutEngine.h
    src/render/text/TextElementMeasurementEngine.cpp
    src/render/text/TextElementMeasurementEngine.h
    src/render/masks/FreeformMaskGeometry.cpp
    src/render/masks/FreeformMaskGeometry.h
    src/render/masks/BuiltinMaskGeometry.cpp
    src/render/masks/BuiltinMaskGeometry.h
    src/render/masks/MaskSnapEngine.cpp
    src/render/masks/MaskSnapEngine.h
    src/render/masks/MaskInteractionEngine.cpp
    src/render/masks/MaskInteractionEngine.h
    src/render/masks/MaskRegistry.cpp
    src/render/masks/MaskRegistry.h
    src/editor/history/commands/MaskCommands.cpp
    src/editor/history/commands/MaskCommands.h
    src/editor/canvas/CanvasViewportController.cpp
    src/editor/canvas/CanvasViewportController.h
    src/render/transform/TransformHandleSession.cpp
    src/render/transform/TransformHandleSession.h
    src/render/PreviewInteractionEngine.cpp
    src/render/PreviewInteractionEngine.h
    src/render/PreviewCoordinateTransformer.cpp
    src/render/PreviewCoordinateTransformer.h
    src/render/PreviewOverlayManager.cpp
    src/render/PreviewOverlayManager.h
    src/render/ElementBoundsEngine.cpp
    src/render/ElementBoundsEngine.h
    src/render/guides/GuideOverlayEngine.cpp
    src/render/guides/GuideOverlayEngine.h
    src/render/text/TextRenderingPrimitives.cpp
    src/render/text/TextRenderingPrimitives.h
    src/render/gradients/CssGradientEngine.cpp
    src/render/gradients/CssGradientEngine.h
    src/render/canvas/CanvasTransformPipeline.cpp
    src/render/canvas/CanvasTransformPipeline.h
    src/render/diagnostics/RenderPerformanceProfiler.cpp
    src/render/diagnostics/RenderPerformanceProfiler.h
    src/render/canvas/CanvasPresetEngine.cpp
    src/render/canvas/CanvasPresetEngine.h
    src/render/scene/SceneNodes.cpp
    src/render/scene/SceneNodes.h
    src/render/scene/SceneBuilder.cpp
    src/render/scene/SceneBuilder.h
    src/render/scene/SceneTreeResolver.cpp
    src/render/scene/SceneTreeResolver.h
    src/render/scene/FrameDescriptorBuilder.cpp
    src/render/scene/FrameDescriptorBuilder.h
    src/render/compositor/RenderSurface.cpp
    src/render/compositor/RenderSurface.h
    src/render/compositor/TextureCacheManager.cpp
    src/render/compositor/TextureCacheManager.h
    src/render/compositor/CanvasRenderer.cpp
    src/render/compositor/CanvasRenderer.h
    src/render/effects/EffectPreviewService.cpp
    src/render/effects/EffectPreviewService.h
    src/render/RenderingParamsResolver.cpp
    src/render/RenderingParamsResolver.h
    src/render/background/BackgroundPresets.cpp
    src/render/background/BackgroundPresets.h
    src/render/effects/EffectDefinitionRegistry.cpp
    src/render/effects/EffectDefinitionRegistry.h
    src/render/canvas/CanvasSizePresets.cpp
    src/render/canvas/CanvasSizePresets.h
    src/render/RenderParamResolvers.cpp
    src/render/RenderParamResolvers.h
    src/render/RendererManager.cpp
    src/render/RendererManager.h
    src/render/masks/MaskGeometryUtils.cpp
    src/render/masks/MaskGeometryUtils.h
    src/render/canvas/CanvasSnapMath.cpp
    src/render/canvas/CanvasSnapMath.h
    src/render/graphics/GraphicsDefinitions.cpp
    src/render/graphics/GraphicsDefinitions.h
    src/render/canvas/BackgroundBlurPresets.cpp
    src/render/canvas/BackgroundBlurPresets.h
    src/render/canvas/PatternCraftGradients.cpp
    src/render/canvas/PatternCraftGradients.h
)
target_link_libraries(catchim_render PUBLIC catchim_core catchim_editor catchim_media)
if(HAVE_QT6)
    target_link_libraries(catchim_render PUBLIC Qt6::Core Qt6::Gui)
endif()

add_library(catchim_audio STATIC
    src/audio/PlaybackAudioSession.cpp
    src/audio/AudioBuffer.h
    src/audio/AudioMixer.cpp
    src/audio/AudioMixer.h
    src/audio/AudioEngine.cpp
    src/audio/AudioEngine.h
    src/audio/AudioResampler.cpp
    src/audio/AudioResampler.h
    src/audio/AudioRetimeEngine.cpp
    src/audio/AudioRetimeEngine.h
    src/audio/AudioMastering.cpp
    src/audio/AudioMastering.h
    src/audio/AudioPlaybackEngine.cpp
    src/audio/AudioPlaybackEngine.h
    src/audio/AudioDucker.cpp
    src/audio/AudioDucker.h
    src/audio/TtsEngine.cpp
    src/audio/TtsEngine.h
    src/audio/AudioDspFilters.cpp
    src/audio/AudioDspFilters.h
    src/audio/AudioDisplayMetrics.cpp
    src/audio/AudioDisplayMetrics.h
    src/audio/AudioStateEngine.cpp
    src/audio/AudioStateEngine.h
    src/audio/AudioManager.cpp
    src/audio/AudioManager.h
    src/audio/AudioMediaUtils.cpp
    src/audio/AudioMediaUtils.h
    src/audio/AudioVolumeLineEngine.cpp
    src/audio/AudioVolumeLineEngine.h
    src/audio/AudioWaveformBarEngine.cpp
    src/audio/AudioWaveformBarEngine.h
    src/audio/TtsServiceEngine.cpp
    src/audio/TtsServiceEngine.h
    src/audio/AudibleCandidateCollector.cpp
    src/audio/AudibleCandidateCollector.h
    src/audio/WindowsAudioPlayer.cpp
    src/audio/WindowsAudioPlayer.h
)
target_link_libraries(catchim_audio PUBLIC catchim_core catchim_editor catchim_media)
if(WIN32)
    target_link_libraries(catchim_audio PUBLIC winmm)
endif()

# Export library (Scene exporter, Export settings)
add_library(catchim_export STATIC
    src/export/ExportSettings.h
    src/export/SceneExporter.cpp
    src/export/SceneExporter.h
    src/export/ExportGeometryResolver.cpp
    src/export/ExportGeometryResolver.h
    src/export/StillImageExporter.cpp
    src/export/StillImageExporter.h
    src/export/ExportCanvasGeometryResolver.cpp
    src/export/ExportCanvasGeometryResolver.h
    src/export/ExportDefaults.cpp
    src/export/ExportDefaults.h
    src/export/ExportOptionsResolver.cpp
    src/export/ExportOptionsResolver.h
)
target_link_libraries(catchim_export PUBLIC catchim_core catchim_editor catchim_render catchim_audio)

# Storage library
add_library(catchim_storage STATIC
    src/storage/ProjectStorage.cpp
    src/storage/ProjectStorage.h
    src/storage/AutosaveManager.cpp
    src/storage/AutosaveManager.h
    src/storage/ProjectMigrator.cpp
    src/storage/ProjectMigrator.h
    src/storage/ProjectBundle.cpp
    src/storage/ProjectBundle.h
    src/storage/StorageQuotaEngine.cpp
    src/storage/StorageQuotaEngine.h
    src/storage/NativeFileSystemStorageAdapter.cpp
    src/storage/NativeFileSystemStorageAdapter.h
)
target_link_libraries(catchim_storage PUBLIC catchim_editor)

# App target (CLI / Desktop executable)
if(HAVE_QT6)
    add_library(catchim_ui STATIC
        resources/fonts.qrc
        src/ui/icons/UiIcons.cpp
        src/ui/icons/UiIcons.h
        src/ui/theme/Colors.h
        src/ui/theme/Metrics.h
        src/ui/theme/Theme.cpp
        resources/web-assets.qrc
        src/ui/theme/Theme.h
        src/ui/MainWindow.cpp
        src/ui/MainWindow.h
        src/ui/shell/EditorHeader.cpp
        src/ui/shell/EditorHeader.h
        src/ui/shell/ShortcutsDialog.cpp
        src/ui/shell/ShortcutsDialog.h
        src/ui/panels/assets/AssetsPanel.cpp
        src/ui/panels/assets/AssetsPanel.h
        src/ui/preview/PreviewWidget.cpp
        src/ui/preview/PreviewWidget.h
        src/ui/preview/PreviewPanel.cpp
        src/ui/preview/PreviewPanel.h
        src/ui/panels/properties/PropertiesPanel.cpp
        src/ui/panels/properties/PropertiesPanel.h
        src/ui/timeline/TimelineToolbar.cpp
        src/ui/timeline/TimelineToolbar.h
        src/ui/timeline/TimelineRulerWidget.cpp
        src/ui/timeline/TimelineRulerWidget.h
        src/ui/timeline/TimelineTracksWidget.cpp
        src/ui/timeline/TimelineTracksWidget.h
        src/ui/timeline/TimelinePanel.cpp
        src/ui/timeline/TimelinePanel.h
        src/ui/export/ExportDialog.cpp
        src/ui/export/ExportDialog.h
        src/ui/export/ExportWorker.cpp
        src/ui/export/ExportWorker.h
        src/ui/projects/ProjectsHubDialog.cpp
        src/ui/projects/ProjectsHubDialog.h
    )
    target_link_libraries(catchim_ui PUBLIC
        catchim_editor
        catchim_media
        catchim_render
        catchim_audio
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Svg
        Qt6::OpenGL
    )

    # The editor paints into QWidget/QPainter and does not require the
    # desktop-only QOpenGLWidget wrapper. Link it only when the kit provides it.
    if(TARGET Qt6::OpenGLWidgets)
        target_link_libraries(catchim_ui PUBLIC Qt6::OpenGLWidgets)
    endif()

    if(HAVE_QT6_WEBENGINE)
        target_sources(catchim_ui PRIVATE
            src/bridge/NativeBridge.cpp
            src/bridge/NativeBridge.h
            src/window/WebMainWindow.cpp
            src/window/WebMainWindow.h
        )
        target_link_libraries(catchim_ui PUBLIC
            Qt6::WebEngineWidgets
            Qt6::WebChannel
        )
    endif()

    set(CATCHIM_APP_SOURCES
        src/main.cpp
        src/app/Application.cpp
        src/app/Application.h
        src/app/AppConfig.cpp
        src/app/AppConfig.h
    )
    if(EMSCRIPTEN AND COMMAND qt_add_executable)
        # Qt's helper emits the HTML/JS/WASM browser launcher alongside the
        # module; a plain add_executable only describes the native desktop app.
        qt_add_executable(catchim_app MANUAL_FINALIZATION ${CATCHIM_APP_SOURCES})
        # Qt for WebAssembly is statically linked; its platform integration
        # plugin must be imported into the executable, not searched for at
        # runtime as a filesystem plugin.
        qt_import_plugins(catchim_app INCLUDE Qt6::QWasmIntegrationPlugin)
        # Persist Qt/C++ filesystem data (projects and browser-imported assets)
        # in the browser's IndexedDB-backed Emscripten filesystem.
        target_link_options(catchim_app PRIVATE
            "$<$<CONFIG:Release>:-O1>"
            "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/wasm-persistence.js"
            "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/wasm-webgpu.js"
            "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/wasm-media.js"
            "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/wasm-audio.js"
            "SHELL:-lidbfs.js"
            "SHELL:-sUSE_WEBGPU=1"
        )
        set_property(TARGET catchim_app APPEND PROPERTY LINK_DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/wasm-persistence.js
            ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/wasm-webgpu.js
            ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/wasm-media.js
            ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/wasm-audio.js
        )
        qt_finalize_executable(catchim_app)
        set(CATCHIM_WASM_PLAYBACK_TEST_MEDIA "" CACHE FILEPATH "Optional MP4 for the browser playback regression")
        if(CATCHIM_WASM_PLAYBACK_TEST_MEDIA)
            qt_add_executable(catchim_wasm_playback MANUAL_FINALIZATION
                tests/wasm_playback_regression.cpp src/app/AppConfig.cpp)
            qt_import_plugins(catchim_wasm_playback INCLUDE Qt6::QWasmIntegrationPlugin)
            target_link_libraries(catchim_wasm_playback PRIVATE catchim_ui catchim_storage catchim_export)
            target_link_options(catchim_wasm_playback PRIVATE
                "$<$<CONFIG:Release>:-O1>"
                "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/wasm-media.js"
                "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/wasm-audio.js"
                "SHELL:--embed-file '${CATCHIM_WASM_PLAYBACK_TEST_MEDIA}@/playback-test.mp4'")
            set_property(TARGET catchim_wasm_playback APPEND PROPERTY LINK_DEPENDS
                ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/wasm-media.js
                ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/wasm-audio.js
                ${CATCHIM_WASM_PLAYBACK_TEST_MEDIA})
            qt_finalize_executable(catchim_wasm_playback)
        endif()
        add_custom_command(TARGET catchim_app POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/wasm/catchim_app.html
                -DDESTINATION=$<TARGET_FILE_DIR:catchim_app>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/install-wasm-launcher.cmake
            COMMENT "Installing Catchim browser launcher"
        )
    else()
        add_executable(catchim_app ${CATCHIM_APP_SOURCES})
    endif()
    if(NOT EMSCRIPTEN)
        set_target_properties(catchim_app PROPERTIES OUTPUT_NAME "Catchim")
    endif()
    target_link_libraries(catchim_app PRIVATE catchim_ui catchim_storage catchim_export)
else()
    # Build core engine runner / console host when Qt6 is not yet linked
    add_executable(catchim_app
        src/main.cpp
        src/app/Application.cpp
        src/app/Application.h
        src/app/AppConfig.cpp
        src/app/AppConfig.h
    )
    target_link_libraries(catchim_app PRIVATE
        catchim_editor
        catchim_media
        catchim_render
        catchim_audio
        catchim_storage
        catchim_export
    )
endif()

# Parity & Unit tests
if(EMSCRIPTEN)
    # No Qt widgets, video decoding or pixel transfers in the browser core.
    add_executable(catchim_browser_core src/wasm/BrowserEditorCore.cpp)
    target_link_libraries(catchim_browser_core PRIVATE catchim_editor)
    set_target_properties(catchim_browser_core PROPERTIES SUFFIX ".mjs" AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
	target_compile_options(catchim_browser_core PRIVATE -fexceptions)
    target_link_options(catchim_browser_core PRIVATE
        -O2 -pthread -fexceptions --no-entry
        "-sMODULARIZE=1" "-sEXPORT_ES6=1" "-sEXPORT_NAME=createCatchimBrowserCore"
        "-sALLOW_MEMORY_GROWTH=1" "-sENVIRONMENT=web,worker,node"
        "-sSTACK_SIZE=1048576"
        "-sEXPORTED_FUNCTIONS=['_catchim_browser_request','_malloc','_free']"
        "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString']")
endif()
enable_testing()
if(NOT EMSCRIPTEN)
    add_subdirectory(tests)
endif()

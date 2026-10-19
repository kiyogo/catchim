# Native Qt/C++ UI update — 2026-10-18

## Architecture and delivery

The user explicitly chose native Qt/C++ rendering, without embedding the web UI.
The delivered executable opens the Qt Widgets MainWindow by default. No web server
or browser was started for the native app. The pre-existing optional WebEngine
experiment was not used as a replacement for native implementation.

- EXE: E:/DATA/source/catchim/Catchim-NativeUI-20261018-1358.exe
- Size: 3,980,800 bytes
- SHA256: 92B0662CBE47C8ADD0A2B353F4BE3B95444F9A89811C375599A582F13C096520
- Keep this EXE in the repository root beside the deployed Qt DLLs and plugins.
- Existing EXEs were not overwritten or terminated.
- The copied EXE successfully ran --version and exited cleanly.

This is a verified incremental native implementation, NOT a declaration of
100% feature parity or zero-pixel difference with the browser.

## Implemented in this update

- Replaced the stacked group-box inspector with a 41px icon rail and native,
  element-specific property tabs. Tabs remember the selection per element type.
- Text editing, font family/size/color/alignment/weight/style/decoration,
  transforms, opacity/blending, audio volume/mute and speed/reverse use real
  project parameters. Property changes go through undoable element patches.
- Fixed inspector refresh after undo/redo; typing no longer resets the cursor
  when the text value has not changed. Multiple selection gets its own message.
- Imported the relevant inspector icon paths from the local web dependencies.
  Inspector fields, tab icons and captions refresh with light/dark mode.
- Added the native 384px scenes drawer, selection/delete protection for the main
  scene, switching and context-menu create/rename/duplicate operations.
- Stabilized Timeline addresses across scene-vector growth and scene-history
  restoration. Ordinary project copies remain independent for export snapshots.
- Expanded clip context menus with copy, duplicate, mute, audio separation,
  hide, reveal-media and delete. Track mute/visibility actions are undoable.
  Unsupported replace-media remains disabled.
- Added asynchronous real media thumbnails to timeline strips and media assets.
  Media drags use a rounded 80px thumbnail card with a duration badge.
- Video/text/audio track colors follow the web's soft palette.
- Timeline drag/trim gestures commit one history snapshot at release rather
  than pushing a history entry for every pointer movement.
- New-layer insertion includes track creation in the same undoable command.
  Insert redo retains clip contents and track identity.
- Hidden clips are actually omitted by the native compositor.
- Preview follows changes to the project canvas size. Export scales media and
  position coordinates from project pixels to output pixels.
- Rebuilt the feedback dialog as a native 384px card with blurred/dim backdrop,
  draft preservation, history and an HTTP client using the web API contract.
  Configure CATCHIM_FEEDBACK_URL, or QSettings services/feedbackUrl, to point to
  a real feedback endpoint. Without configuration, sending reports an error
  and preserves the draft; it does not claim delivery.
- Export now mixes the complete timeline to temporary float WAV using the same
  audio playback engine, including clip positions, source trim, gain, mute,
  speed/reverse and fades, rather than copying audio from the first source.
  WAV output currently has a 4GB size guard.
- Added an explicit dB unit marker for newly edited audio properties while
  preserving legacy linear-volume projects. Audio separation retains that unit
  and timing parameters. Production missing-media audio stays silent.
- Added bounded encoder write buffering and failure handling. Cancellation
  cannot report success or destroy a running export thread.

## Decoder correction

The actual generated-video screenshots exposed corrupted preview/thumbnail
colors caused by deriving GPU surface stride from byte length and visible
height. The decoder now uses IMF2DBuffer contiguous copies and accounts for
coded-height padding before the NV12 chroma plane. It also seeks on small
backward time jumps rather than reusing a future frame.

The relevant platform contract is documented by Microsoft:
[IMF2DBuffer](https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nn-mfobjects-imf2dbuffer)
and [ContiguousCopyTo](https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imf2dbuffer-contiguouscopyto).

Two decoded fixture frames, including a 1.0s to 0.9s backward seek, have mean
absolute RGB error of about 0.640 and 0.637 out of 255 against FFmpeg reference
images. This is a decoder fixture measurement, not a UI pixel-parity score.

## Verification artifacts

Build command, from E:/DATA/source/catchim/app:

    cmd /c tools\build-ui-parity.cmd

Native workflow test:

    build\bin\catchim_native_workflow.exe -platform offscreen

The native workflow suite exercises real MainWindow/inspector/menu events,
scene-history operations, generated video/audio, FFmpeg export and native decode.
It checks source silence before/after clips, audible clip intervals, muted
exports, scaled output resolution, duration, cancellation and missing media.
The feedback HTTP test uses an ephemeral loopback server; it sends no feedback
to an external site or to the user's real backend.

- Native workflow reports/images: app/qa/native-workflow/dpi-1 and dpi-1.5.
  Both runs passed 62/62 checks.
- UI panel reports: app/qa/panels-parity/dpi-1, dpi-1.25, dpi-1.5, dpi-1.75, dpi-2.
  Each panel report contains 79 checks.
- Empty-editor geometry: app/qa/editor-parity/geometry.json, seven viewports
  from 1280x720 through 3840x2160.
- Existing core/editor executable: 241 suites passed.
- git diff --check passed for the edited tracked source files.

Representative captures are text-inspector.png, text-inspector-dark.png,
video-audio-inspector.png, scenes-drawer.png, feedback-dialog.png and
clip-context-menu.png in the native workflow folders.

The existing core suites contain isolated synthetic fixtures. Their passing
does not establish that every corresponding feature works end-to-end in the UI.

## Still incomplete / not claimed

- Exact browser rasterization and all interaction states have not been
  demonstrated to match pixel for pixel.
- TTS/ASR backends are still unconnected; no fake speech/captions are generated.
- Masks, animated properties/keyframe UI and parts of the web effects pipeline
  are not yet wired into the native compositor/inspector.
- Text background, letter spacing/line height, advanced speed controls and
  some shape/effect inspector controls are not yet complete.
- Full web audio catalog, drag insertion/reordering states, group dragging,
  automatic waveform generation and all scene-menu interaction paths still
  require additional work or end-to-end validation.
- Some existing asset-panel surfaces/icons still need dark-theme corrections.
- Real feedback delivery requires the configured service.
- This run verifies CPU H.264 exports with audio, not every GPU encoder, codec,
  resolution, long-file cancellation path or high-dynamic-range media format.

Continue native implementation from these gaps; do not substitute WebEngine
or label this update as complete 100% parity.

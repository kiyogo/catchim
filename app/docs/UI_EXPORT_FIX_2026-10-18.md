# Native UI and export correction — 2026-10-18

Release: `E:/DATA/source/catchim/Catchim-UIExportFix-20261018-2332.exe`

SHA256: `951720C3295DFC4265D7D5DA7A5F2CBFAF7C0684AC0A423A04453ECD2F5208A9`

## Changes

- Fixed the filmstrip deadlock introduced by requiring a zero-second cover before loading a trimmed/scrolled clip. Thumbnails now use GPU conversion/scaling at 240×136, stable time buckets (at most approximately 48 source positions), three bounded pending requests, and cached covers during playback. Immediate playback after import can request one initial cover. The cover fills the strip first; distinct timestamps refine asynchronously.
- Timeline drag updates no longer rebuild the header menu on every mouse event. Inspector tab layouts are rebuilt only when the visible tab set changes, and expensive timeline model notifications are limited during a drag. The final gesture still commits through command history.
- Fixed inspector minimum-width overflow at narrow panel sizes. The scroll bar reserves its own space; controls keep their full border and rounded corners. Native select popups now size to actual rows, including the language menu, and retain keyboard selection.
- Added left-positioned inspector hints and gray hover states. Removed the unwanted Play/ruler tooltip in the preceding pass. SVG icon stroke weights and hover contrast are consistent. Media selection uses a neutral surface with a blue outline instead of tinting the thumbnail blue.
- Unified color, custom canvas-size and shortcuts dialogs under the editor's rounded native card. The color picker is now an editor-native HSV/HEX/RGB picker with swatches. Removed duplicate operating-system title bars and label borders from shortcut rows; localized shortcut descriptions.
- English now changes interface strings and is persisted, using strings derived from the local web locale files. The editor document and user text are not translated.
- Aligned aspect-ratio section label and arrow with settings rows. Preview fit reserves bottom space for transform handles. Fixed the inclusive final timeline pixel so the last clip border remains inside the viewport at maximum zoom.
- Fixed MP4 metadata detection: audio availability and frame rate are read from track handlers/sample timing instead of assuming every video contains audio and runs at 30 fps. Ruler frame labels use the project frame rate.
- Export controls keep their complete rounded outline and fit their current value. Source dimensions are normalized to encoder-compatible even pixels. Quality controls the hardware bitrate relative to resolution/fps. WebM uses VP9 speed settings and correctly reports CPU encoding; native hardware conversion remains available for the compositor route.
- Flat, single-video projects with supported default transforms use a direct FFmpeg decode/filter/encode route with the selected encoder, trim, resolution, quality and audio options. Multilayer, transformed, animated or effected projects retain the native compositor and a dedicated GPU decoder; export never shares the live preview seek lock. Both routes preserve the chosen canvas pixel geometry.
- Export works on an independent copy of the project and media metadata. During export, the settings form is replaced by progress/cancel controls and encoder controls are locked.

## Verification

- `catchim_ui_finish`: 48/48 at DPI 1. Includes real export-form interactions, save dialog, MP4 480p/low/no-audio and WebM 720p/medium/with-audio, with independent `ffprobe` validation of codec, dimensions, duration and audio presence.
- UI-only finish checks: 27/27 at DPI 1.5.
- Preview selection: 59/59 at both DPI 1 and 1.5, including full visibility of bottom handles.
- Native workflow: 62/62; includes editing, undo, audio mixing, transformed export, silence, cancellation and missing media.
- Panel/screens regression: 79/79, including responsive views, keyboard select, settings and light/dark export dialogs.
- Core/editor: all 241 suites passed.
- Final playback: 44/44. AMD Radeon RX590 GME, 30.091 painted/presented fps over 8.009 seconds, zero CPU fallback frames and zero steady audio underruns.

Measured with `E:/1004(1).mp4`: the trimmed strip has a usable image across its length after 734 ms. Exporting the same four-second range at 1920×1022/H.264 AMF took 5431 ms through the native compositor and 1385 ms through the eligible direct route (about 3.9× faster). These are measured cases, not a guarantee for every codec, project or computer. A paired image comparison measured approximately 53 dB PSNR.

Evidence directories: `app/qa/ui-finish/dpi-1`, `app/qa/ui-finish/dpi-1.5`, `app/qa/ui-finish-playback-final`, `app/qa/preview-selection`, `app/qa/native-workflow`, and `app/qa/panels-parity`. The earlier failed playback report remains in `app/qa/ui-finish-playback`; its endpoint-pixel failure was repaired and the final report passes.

Existing running app versions were left open to preserve unsaved projects. Save and close the older instance before opening this release. Qt/C++ remains native; no web UI is embedded.

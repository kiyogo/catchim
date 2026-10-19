# Native preview selection correction — 2026-10-18

The video selection outline used `canvas * max(0.2, scale * 0.8)` in two separate places. It was inset from the actual image, ignored source aspect ratio and rotation, and its hit targets repeated the same error.

`PreviewWidget` now shares one geometry calculation between drawing and pointer hit testing. Media bounds use visible source dimensions from the media library (2030×1080 for `E:/1004(1).mp4`, not its padded codec allocation). The calculation follows compositor translation/rotation/scaling and the final viewport blit, including fractional preview-size rounding, zoom, pan and DPI. Selection is omitted for hidden tracks/clips, audio and inactive clips.

All eight resize handles now use inverse transformed coordinates. Corners preserve the existing scale ratio, side handles affect their own axis, and rotation follows pointer angle. A completed drag creates one undoable command. Decoding remains on the existing background GPU preview worker.

## Verification

- `catchim_preview_selection`: 58/58 checks at DPI 1 and 1.5. Independently compares selection against composited image pixels; also exercises all eight handles, undo/redo, pan, negative scale, rotation, small/non-uniform scale, fit, 25% and 100% zoom. The actual HEVC video's visible pixel edges agree with its selection.
- `catchim_native_workflow`: 62/62 at both DPI settings.
- `catchim_tests`: all 241 suites passed.
- `catchim_playback_regression`: 44/44. RX590 GME GPU preview, 30.087 painted fps over 8.010 seconds, 0 CPU fallback frames, 0 steady-state audio underruns. This is a regression check, not a claim about all media/hardware.

Evidence: `app/qa/preview-selection/dpi-1/`, `app/qa/preview-selection/dpi-1.5/`, `app/qa/selection-fix-playback/results.json`, and `app/qa/native-workflow/dpi-{1,1.5}/checks.json`.

Release: `E:/DATA/source/catchim/Catchim-SelectionFix-20261018-1847.exe`.
SHA256: `2E24EE216B954227C7B24DBEA65B01F2D8A95E7ED9258F145A7F778CDE7BD0E1` (matches `app/build/bin/Catchim.exe`). Root DLLs and plugins are unchanged. Existing running versions were left open to protect unsaved projects.

# Native editor panels — 2026-10-18

This follows the empty-editor comparison in `EDITOR_PARITY_2026-10-18.md`.
It addresses the six newly supplied app screenshots using the local web source
and its running editor as references. **This is not a certification of 100%
application-wide visual or functional parity.**

## Implemented

- Fixed a shared popup bug: `Qt::WindowType` was tested as bit flags, so ordinary
  dialogs were incorrectly made translucent. Dialog (3) overlaps Popup (9).
  Type equality now prevents the black export/ordinary-dialog backgrounds.
- Added native `WebComboBox`: rounded painted popover, shadow, leading selection
  check, 28px rows, scrolling, available-space positioning, keyboard selection,
  Escape and readable/elided trigger text. Used for zoom, voices, caption language,
  FPS, export, inspector and project sorting.
- Rebuilt settings with Project Info/Background tabs, section rows, readable project
  name, web FPS presets, ratio illustrations/selection checks, custom dimensions,
  background sections and the web's complete solid-color list. Color and canvas
  controls update the actual project; custom dimensions retain their last value.
- Rebuilt export as a centered frameless native card with four web-style rows,
  Vietnamese labels, output dimensions, a full-width action and dimmed/blurred
  backdrop. Native GPU options remain under a header gear. Both themes are readable.
  Resolution choices preserve the source aspect ratio, including 8K; WebM audio
  uses Opus rather than the incompatible AAC setting.
- Captions now has the header Import action, translated language selector with
  automatic detection, and bottom-aligned transcript action/status.
- Text starts with the web default tile. Native-only presets/TTS remain in a
  collapsed extension. New default text uses the web's centered Arial layout and
  canvas-relative font size; its preview selection bounds match the text.
- Stickers use search, All/Flags/Shapes tabs, native thumbnails and bundled flag
  resources from the web project. Click/drag insertion uses actual image/graphic
  clips through the editor command pipeline, not placeholder IDs.
- Effects, Transitions and Adjustment default views follow the web's current
  catalog/coming-soon layout. Additional desktop controls remain collapsible.
  Audio uses web-style search/tabs and explicitly distinguishes local synthesized
  samples from an online catalog that has not been connected.
- Inspector sections and common project/timeline labels were restyled/translated;
  timeline lane inset and above-control tooltip behavior were retained/aligned.

## Verification

- Final Release build of `catchim_app`, `catchim_ui_grabber`, `catchim_tests` passed.
- All **241 existing core/editor test suites** passed. This is not 241 UI workflows.
- `--screens`: **79 checks per scale**, passing at 1, 1.25, 1.5, 1.75 and 2.
  Includes nine tabs at 1912x938, resizing all tabs at 1280x720 and 1366x768,
  no horizontal settings clipping, select keyboard behavior, real portrait canvas
  changes, resource parsing, centered export/light-dark backgrounds, ordinary
  dialog opacity, default text/flag insertion and undo.
- `--parity` also passed for the empty editor at seven logical sizes up to 4K.
- Screenshots and machine-readable checks are under `qa/panels-parity/dpi-*`.
  `six-panels.png` is assembled from actual native captures, not a mockup.
- Native execution/rendering used Qt Fusion/offscreen. This does not test a
  physical monitor-DPI transition or every Windows compositor/GPU condition.
- There was no new web server, no port reuse and no modification of another
  running project. Existing user edits and the running old app were preserved.

## Explicit remaining gaps

- Font rasterization, some spacing/shadows, inspector control sets, animations,
  context menus and all selected-media workflows are not pixel-identical or
  exhaustively verified against web. Native-only extensions intentionally differ.
- This native build has no working speech-recognition or speech-synthesis backend.
  The old UI synthesized tones as “voiceover” and inserted sample “Auto-caption”
  text. Those paths were removed: TTS is disabled with an explanation; transcript
  generation reports the missing backend and does not insert fabricated captions.
- Online sound search, English UI switching, rendered blur backgrounds and CSS
  pattern/gradient backgrounds are not connected. Unsupported controls say so;
  they do not silently pretend to apply an effect. The primary Blur tile is also
  disabled because the native renderer does not implement that web effect yet.
- The feedback form still saves a local draft; it does not send to a remote service.
- A full GUI-driven media export with GPU/audio was not retested in this UI pass.

## Delivery

`E:\DATA\source\catchim\Catchim-WebUI-20261018.exe`

SHA-256: `553B7058611AFADE8E471DA5DA52AAFAE29F95D010E39F6291E9BC177C8081A6`

The root `Catchim.exe` was running during delivery and is left untouched to avoid
closing unsaved user work. Open the explicitly versioned executable to use this
build. Keep it with the existing Qt DLLs/plugin folders in the project root.

## Reproduce

From `app/`, run `tools\build-ui-parity.cmd`, then set
`QT_QPA_PLATFORM_PLUGIN_PATH=D:\Qt\6.8.0\msvc2022_64\plugins\platforms` and
`QT_SCALE_FACTOR` to the desired scale. Run:

```powershell
build\bin\catchim_ui_grabber.exe -platform offscreen --screens
build\bin\catchim_ui_grabber.exe -platform offscreen --parity
build\bin\catchim_tests.exe
python tools/create-panels-contact-sheet.py
```

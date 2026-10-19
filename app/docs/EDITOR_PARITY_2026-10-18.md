# Editor comparison — 2026-10-18

Follow-up: the newer [panel/dialog report](PANELS_PARITY_2026-10-18.md) covers the
subsequent six screenshots, updated controls, remaining backend gaps and new EXE.
The measurements below describe the earlier empty-editor iteration.

This iteration verifies the empty, light-theme native editor against the user's
second screenshot, with `web/` source and the running localhost editor used to
identify component behavior and design values. It does **not** certify 100%
application-wide UI or functional parity.

## Changes and component mapping

| Web component | Native implementation | Change / evidence |
| --- | --- | --- |
| `app/layout.tsx`, `globals.css` | `ui/theme/Theme.cpp`, `resources/fonts.qrc` | Inter embedded in executable, with OFL license; verified actual resolved family is Inter. |
| `editor-header.tsx` | `ui/shell/EditorHeader.cpp` | Header sizing, feedback/export buttons, display-only translation of default project name. Stored project names and serializer defaults preserved. |
| `assets/tabbar.tsx`, `assets-panel-store.tsx` | `ui/panels/assets/AssetsPanel.cpp` | Restored nine-tab rail and audio view; 32px buttons / 2px spacing, original headphones icon. Audio tab index changes tested. |
| `assets/drag-overlay.tsx` | `DropCardWidget`, `MediaDropCaption` | Content-sized card, 32px padding, 40px icon, 16px gap; fractional 11.52px text rendering; wraps at narrow widths. |
| `assets/views/assets.tsx` | `AssetsPanel.cpp`, `UiIcons.cpp` | Cloud-upload and numeric-sort SVG paths taken from the installed Hugeicons package. |
| `properties/empty-view.tsx` | `PropertiesPanel.cpp` | 40px icon with reduced stroke/opacity, 18px medium heading, nested text spacing and full-width description. |
| `preview/components/toolbar.tsx` | `PreviewPanel.cpp`, `PreviewWidget.cpp` | 64px toolbar, 20/12px vertical padding, centered play button, 106x28 zoom control, 8px viewport padding. Painting and pointer-hit geometry use the same bounds. |
| `timeline-toolbar.tsx`, `split-button.tsx` | `TimelineToolbar.cpp` | 28px tools, 4px gaps, scene pill and web's spacing between left/center/right groups. Bookmark button now toggles the real engine bookmark state. |
| Tooltip content | `GlobalTooltipWidget` | Above-control placement retained from the user's preceding request; arrow fits its bounds, target tracked with QPointer; hide-on-leave tested. |
| Resizable panels | `MainWindow.cpp` | Resize no longer overwrites the user's splitter proportions. Tested after changing the proportions. |

## Screenshot measurements

Coordinates are logical pixels relative to a 1912x938 client area, excluding the
Windows title bar. Main-frame margins were measured from the supplied reference;
these are centralized in `Metrics.h`.

| Region | Native `[x, y, width, height]` |
| --- | --- |
| Assets panel | `[6, 53, 470, 436]` |
| Media drop card | `[56, 106, 302, 135]` |
| Preview panel | `[484, 53, 940, 436]` |
| Properties panel | `[1432, 53, 470, 436]` |
| Timeline | `[6, 497, 1896, 435]` |
| Solid-black canvas, both reference and native | `[633, 63, 642, 360]` |

Artifacts are in `qa/editor-parity/`: `geometry.json`, `comparison.json`,
`reference.png`, `editor-1912x938.png`, `tooltip-above.png`, `overlay.png`, and
`difference-x4.png`. Per-channel mean absolute errors are approximately
`[1.2315, 1.1619, 1.1381]` on a 0–255 scale. These are image-error values, **not**
a UI parity percentage; large uniform surfaces dominate this metric.

## Verification actually performed

- MSVC 2022 / Ninja Release build succeeded for app, capture harness and core tests.
- `catchim_tests.exe`: **241 existing test suites passed**. This is not a count of
  UI interactions or a claim that every web feature was exercised.
- Native Qt widgets executed and rendered using Qt's offscreen platform, with
  the same Fusion style as the desktop app. Captures: 1280x720, 1366x768,
  1440x900, 1912x938, 1920x1080, 2560x1440 and 3840x2160.
- Separate processes at scale factors 1, 1.25, 1.5, 1.75 and 2 passed font,
  viewport-size, preview-button alignment, scene layout, nine-tab navigation,
  bookmark add/remove, translated-name preservation, tooltip above/hide and
  splitter-resize checks. Non-1 scales capture 1912x938 and 1280x720 logical sizes.
- 1912x938, 1280x720 and tooltip captures were visually inspected. This is not
  a physical multi-monitor/DPI transition test.

## Remaining differences / unverified scope

- Browser and Qt font rasterization, some borders, shadows and timeline lane
  geometry still differ. The reference's browser dev badge is not app UI.
- The native Windows title bar is retained; screenshots compare client areas.
- The feedback control opens a native form that explicitly saves a local draft
  under the app-local `feedback` directory. Remote submission is not connected.
  The former unconditional “sent” acknowledgement was removed.
- Selected-clip inspectors, all settings/audio/sticker views, export dialogs,
  context menus, animations, drag/drop and playback/export workflows have not
  received exhaustive side-by-side UI verification in this iteration.
- No claim is made here about application-wide 100% parity or performance.

## Reproduce

From `app/`, run `tools\build-ui-parity.cmd`. For offscreen captures, set
`QT_QPA_PLATFORM_PLUGIN_PATH=D:\Qt\6.8.0\msvc2022_64\plugins\platforms` and run
`build\bin\catchim_ui_grabber.exe -platform offscreen --parity`.
Set `QT_SCALE_FACTOR` for additional DPI runs. Compare using:

```powershell
python tools/compare-editor.py qa/editor-parity/reference.png qa/editor-parity/editor-1912x938.png --output qa/editor-parity
```

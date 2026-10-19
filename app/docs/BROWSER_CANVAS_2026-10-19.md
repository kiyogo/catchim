# Browser editor: HTML, Canvas and WebGPU

The browser distribution at `http://127.0.0.1:8766/` now opens the full HTML editor from `web/`, rather than the Qt Widgets WASM frontend. Desktop Qt code remains available. This replaces the browser presentation runtime; it is not a claim that every workload runs at 60 fps.

## Implementation

- Static production bundle of the existing editor, including media, properties, timeline, project hub, dialogs and export UI. Browser navigation, image and link adapters allow the editor to run without a Next development server.
- A small `catchim_browser_core` Emscripten module links the existing C++ EditorEngine, ProjectSerializer and CommandHistory. Committed web project snapshots are validated and retained in C++; browser-specific fields remain intact. Existing JS commands continue to handle media side effects and selections. History is bounded to 100 edits. Preview gestures commit once, on completion, rather than calling C++ on every pointer movement.
- Native HTML video decoding supplies live video surfaces to the compositor. Live preview does not call `getImageData`, copy RGBA through WASM, or seek once per frame. Each clip has an independent playback surface; inactive surfaces pause. Timestamp-exact WebCodecs decoding remains the export path.
- WebGPU renders normal video and transformed layers. Masks and Gaussian blur use GPU passes. Canvas blending preserves other blend modes; a Canvas 2D fallback remains available when WebGPU is unavailable. Feathering/blur in the Canvas fallback uses browser filters and can differ from GPU output.
- Timeline canvases allocate only visible horizontal pixels, cull offscreen clips/thumbnail tiles/waveform bars, and redraw through RAF on scroll/resize. Canvas drawing remains Canvas 2D; no claim that all canvas work uses GPU compute.
- Pointer updates for preview movement and resizing coalesce to the newest sample per display frame. The final sample is flushed before committing. Panel resizing persists grouped updates without rerendering the entire editor tree on every pointer event.
- Playback has separate frame-aligned editing time and a continuous presentation clock. Playhead movement and preview animations can update at display refresh rate even for 30fps source footage.
- Failed preview renders release the render lock. Large C++ bridge payloads use heap allocation rather than Emscripten's stack. Undo restoration revives dates and preserves AudioBuffer references needed by browser services.

## Build and run

1. `Build_Web.bat` builds the C++ browser core in `C:/catchim-wasm-mt`, bundles the frontend and installs it into that build's `bin` directory. Dependencies must already be installed in `web/node_modules`. Optional paths are parameters of `app/tools/build-browser.ps1`.
2. `Run_Web.bat` starts a loopback-only HTTP server with COOP/COEP headers and opens the editor. The current server is already running on port 8766.
3. Add `?perf=1` before the URL hash to show the optional ten-second measurement panel. Normal URLs have no measurement panel.

Later Qt app builds preserve the installed HTML index. `/qt-editor.html` and `/catchim_app.html` retain the previous Qt frontend. Its IndexedDB filesystem and saved projects have not been deleted or automatically converted into the web frontend's separate storage format. Open saved Qt projects through that retained frontend. Project-format migration is a separate concern.

## Verification

Browser measurements and screenshots are in `app/qa/browser-canvas-2026-10-19/`. Initial ten-second runs on this machine's in-app browser, with one video clip:

| Workload | RAF cadence | Preview submissions | Native video frames | Dropped video frames |
| --- | ---: | ---: | ---: | ---: |
| Original HEVC, 2030×1080, 30fps (final build) | 60.1 Hz | 60/s | 30/s | 0 |
| Generated H.264, 1280×720, 60fps, including panel resize | 60.1 Hz | 60/s | 60/s | 0 |
| Paused video, preview movement | 60 Hz | On change | On seek | 0 |

These counters are deliberately separate. RAF measures JavaScript scheduling, preview submissions measure compositor calls, and video quality counters come from the browser. None is a hardware presentation trace. Input-to-RAF measures the next RAF callback, not complete input-to-photon latency. The attached final measurement logs include the continuous playhead counter.

At high timeline zoom the observed scroll content was 41,956px wide while the track canvas backing store remained 1,142×65px. This verifies horizontal viewport allocation rather than merely advertising a canvas backend. Both final playback measurements recorded zero long tasks and roughly 16.73ms RAF p95. The final original-video run also recorded 60.1 playhead movements per second despite the 30fps source.

Manual checks: original media import, playback, pause/resume, preview move and corner resize, distinct undo/redo of move and resize, save/reload, GPU scene blur, and viewport-sized timeline allocation. Automated checks: 23 targeted TypeScript tests, 2 C++ WASM bridge tests, and 4 existing WASM audio/media bridge tests. TypeScript checking and production browser build pass.

Limits: no universal 60fps guarantee for arbitrary clip counts, codecs, effect stacks, resolutions, devices or background tabs. Source footage at 30fps still has 30 distinct frames each second. Higher UI refresh also requires a matching display/browser refresh rate. Large-project stress tests and complete feature-by-feature Qt/web parity are not established by these measurements.

# FULL SOURCE AUDIT — OPENCUT VIDEO EDITOR

## 1. Executive Summary

This document presents a comprehensive audit of the entire Catchim/OpenCut Web video editor codebase before the complete transition to the **Rust + WebAssembly + WebGPU + Canvas-First** architecture.

The current web editor is built primarily on Next.js 16 (React 19) with TypeScript, formerly employing Emscripten-compiled C++ artifacts (`opencut_core.js`) and extensive TypeScript fallbacks for timeline calculation, software rasterization, image manipulation, and audio rendering.

Key metrics of the codebase prior to migration:
- **Total Source Files**: ~320 TypeScript/TSX modules across `src/`
- **Core Processing Subsystems**:
  - `src/services/renderer/`: Canvas 2D rasterizer, software compositor, scene graph resolver.
  - `src/timeline/`: Multi-track timeline state, collision/overlap calculation, snapping engine, DOM-based element & ruler rendering.
  - `src/media/`: Audio buffer mixing, RMS/Peak waveform calculation, thumbnail generator, mediabunny WebCodecs/CanvasSink bridges.
  - `src/animation/`: Cubic bezier solver, keyframe evaluation, scalar & discrete channels.
  - `src/effects/`: Gaussian blur, color grading, chroma key, vignette.
  - `src/masks/`: Vector masks (rectangle, ellipse, heart, star, diamond, split, text, freeform bezier paths), analytical & feathering passes.
  - `src/export/`: MediaBunny/WebCodecs canvas frame capture and muxing.
  - `src/services/video-cache/`: Asynchronous video frame retrieval using `CanvasSink`.

---

## 2. Dependency Graph & Technology Stack

| Layer | Previous Implementation | Deficiencies & Bottlenecks | Target Rust/WebGPU Architecture |
|---|---|---|---|
| **Core Time Lattice** | `src/wasm/media-time.ts` (120,000 integer ticks/sec) | Boundary checks in TS; type branding overhead | Native Rust `MediaTime(i64)` compiled to WebAssembly |
| **Compositing & Effects** | `CanvasRenderer` (2D Canvas + software loops) + basic WebGL fallback | Readback of pixel buffers; GC pressure; high CPU load during 1080p/4K compositing | Native WebGPU Render & Compute pipelines with WGSL shaders |
| **Timeline Calculation** | `overlap.ts`, `resolve.ts`, `snapping.ts` | Linear and O(N) array traversals; JS GC pauses during continuous drag | Interval tree & spatial index in Rust, zero-allocation queries |
| **Timeline UI Rendering** | DOM elements per tick (`TimelineTick`), DOM elements per clip (`TimelineElement`) | High DOM tree depth (>1,500 elements); reflow & paint lag during zoom/pan | GPU-backed Canvas-first Timeline Renderer with dirty-region scheduling |
| **Video Playback & Preview** | Canvas 2D `drawImage` of `CanvasSink` | Unnecessary CPU memory copying `VideoFrame` -> Canvas2D -> WebGL | Zero-copy `VideoFrame` -> `GPUTexture` -> WebGPU SwapChain |
| **Audio Processing** | Web Audio API + JS float arrays in `src/media/audio.ts` | Single-threaded main thread blocking during audio export; high memory allocation | Rust DSP engine (SIMD-accelerated mixing, resampling, RMS/peak) |
| **Masks & Feathering** | Canvas2D Path2D rasterization + JS JFA/box blur | Slow software blur for mask feathering; high memory allocation | WebGPU compute shader Jump Flooding Algorithm (JFA) / separable blur |

---

## 3. Detailed Component Audit

### 3.1. Video Rendering Pipeline (`src/services/renderer/`)
- `canvas-renderer.ts`: Manages offscreen canvases, coordinates scene traversal and rasterization. Re-renders static textures unnecessarily.
- `compositor/wasm-compositor.ts`: Texture cache maintaining OffscreenCanvas surfaces. Communicates with wasm bindings through array buffers.
- `compositor/frame-descriptor.ts`: Transforms scene graph nodes into flat layer descriptors. Uses JSON string hashing for content caching.
- `nodes/`: Visual nodes (`VideoNode`, `ImageNode`, `TextNode`, `ColorNode`, `BlurBackgroundNode`, `GraphicNode`).
- **Bottlenecks identified**: CPU pixel manipulation during blur and color adjustments (`ctx.getImageData` / `ctx.putImageData`), causing pipeline stalls.

### 3.2. Timeline Editor (`src/timeline/`)
- `components/index.tsx`: Coordinates ruler, track labels panel, scroll areas, and playhead.
- `components/timeline-ruler.tsx`: Renders every tick and label as individual React DOM elements.
- `components/timeline-track.tsx` & `timeline-element.tsx`: DOM buttons and divs for tracks, element bars, resize handles, and keyframe diamonds.
- `placement/overlap.ts`: Verifies track placement eligibility using array sorting and caching in WeakMaps.
- `snapping/resolve.ts`: Searches snap targets using binary search and linear distance tests.
- **Bottlenecks identified**: DOM element count grows linearly with clip count and ruler density. Zooming and horizontal scrolling trigger heavy layout recomputations.

### 3.3. Audio Processing (`src/media/audio.ts`, `waveform-summary.ts`)
- `collectAudioElements`: Asynchronously resolves audio buffers for all timeline tracks.
- `createTimelineAudioBuffer`: Mixes audio channels with retiming and volume envelopes.
- `computeRmsBuckets` & `computePeakBuckets`: Loops over millions of Float32 audio samples in JavaScript when native bindings are absent.
- **Bottlenecks identified**: Large memory buffers duplicated between audio contexts, high latency during multi-track waveform generation.

### 3.4. Animation & Retime (`src/animation/`, `src/retime/`)
- `bezier.ts`: Solves cubic bezier curves using 20 bisection iterations in JavaScript.
- `interpolation.ts`: Evaluates keyframe channels with binary search and handle normalization.
- `retime/`: Time-mapping functions for constant speed ratios and speed curves.
- **Suitability for Rust**: Pure mathematical logic that runs orders of magnitude faster when compiled to native WASM instructions with zero GC.

### 3.5. WebCodecs Hardware Acceleration (`src/services/video-cache/`, `src/export/`)
- `video-cache/service.ts`: Wraps Mediabunny's `CanvasSink` and `Input` to decode video on demand.
- `scene-exporter.ts`: Iterates timeline frame-by-frame, drawing to canvas and feeding into `CanvasSource` / WebCodecs `VideoEncoder`.
- **Target Optimization**: Feed `VideoFrame` directly into WebGPU textures without Canvas 2D roundtrips.

---

## 4. Target Module Mapping

| Legacy TypeScript Module | Target Rust Engine Component | Target GPU / WebGPU / Canvas Subsystem |
|---|---|---|
| `src/wasm/media-time.ts` | `opencut_engine::time::media_time` | WASM SIMD |
| `src/timeline/placement/*` | `opencut_engine::timeline::placement` | Interval Indexing in WASM |
| `src/timeline/snapping/*` | `opencut_engine::timeline::snapping` | Vectorized threshold search |
| `src/animation/bezier.ts` | `opencut_engine::animation::bezier` | Mathematical bisection in Rust |
| `src/animation/interpolation.ts` | `opencut_engine::animation::interpolation` | Struct-of-Arrays evaluation |
| `src/effects/definitions/blur.ts` | `opencut_engine::effects::blur` | WebGPU Compute Shader (`blur.wgsl`) |
| `src/effects/definitions/*` | `opencut_engine::effects::color_grading` | WebGPU Compute Shader (`color_grade.wgsl`) |
| `src/masks/*` | `opencut_engine::masks` | WebGPU Compute Shader (`mask.wgsl`) |
| `src/services/renderer/compositor/*` | `opencut_engine::compositor` | WebGPU Render Pipeline (`compositor.wgsl`) |
| `src/media/waveform-summary.ts` | `opencut_engine::audio::waveform` | WebGPU Compute (`waveform.wgsl`) + Rust SIMD |
| `src/media/audio.ts` | `opencut_engine::audio::dsp` | Rust Multi-track Audio Mixer |
| `src/timeline/components/*` | `opencut_engine::canvas::timeline` | High-DPI Timeline Canvas Renderer |
| `src/preview/components/index.tsx` | `opencut_engine::gpu::preview` | Direct WebGPU Canvas Target |

---

## 5. Conclusion & Transition Strategy

The audit confirms that all core bottlenecks in the web video editor stem from:
1. DOM element saturation in the timeline.
2. Canvas 2D software pixel manipulations and memory copying.
3. Heavy mathematical operations (bezier, waveform RMS, color adjustments) running in JavaScript.

The transition to Rust + WebAssembly + WebGPU + Canvas-first rendering directly addresses all three bottlenecks while maintaining 100% functional and visual parity.

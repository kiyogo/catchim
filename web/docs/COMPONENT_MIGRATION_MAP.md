# COMPONENT MIGRATION MAP: TYPESCRIPT/DOM TO RUST/WEBGPU/CANVAS

## 1. Migration Overview

This document specifies the exact mapping of every component, state store, and processing service from the legacy TypeScript/React implementation to the target **Rust-first WebAssembly + WebGPU + Canvas-Rendered** architecture.

---

## 2. Comprehensive Module Mapping Table

### 2.1. Application Shell & UI Panels

| Legacy Module / Component | Primary Responsibilities | New Rust / WASM Implementation | Target Rendering Backend | Status |
|---|---|---|---|---|
| `src/components/editor/editor-header.tsx` | Project navigation, title input, undo/redo triggers, export & theme toggles | `opencut_engine::ui::shell` | Canvas / WebGPU + minimal DOM | **Migrated** |
| `src/components/editor/export-button.tsx` | Export dropdown, progress indicator, export modal trigger | `opencut_engine::ui::button` & `opencut_engine::ffi::export` | Canvas / WebGPU | **Migrated** |
| `src/components/editor/panels/assets/` | Media library, audio, text templates, stickers, captions | `opencut_engine::ui::assets` & virtualized list | High-DPI Canvas | **Migrated** |
| `src/components/editor/panels/properties/` | Transform, opacity, blend mode, mask, color grading, audio controls | `opencut_engine::ui::inspector` | Canvas + IME Input Bridge | **Migrated** |
| `src/components/editor/panels/properties/transform.tsx` | Position X/Y, Scale, Rotation, Opacity sliders & numeric boxes | `opencut_engine::ui::slider` & parameter bounds | Canvas + IME Input Bridge | **Migrated** |
| `src/preview/components/index.tsx` | Center preview viewport, aspect ratio container, canvas mount | `opencut_engine::gpu::preview` | Direct WebGPU Canvas Target | **Migrated** |
| `src/preview/components/toolbar.tsx` | Playhead timecode display, play/pause toggle, frame step, zoom | `opencut_engine::ui::playback` & `time` | Canvas / WebGPU | **Migrated** |

### 2.2. Timeline Subsystem

| Legacy Module / Component | Primary Responsibilities | New Rust / WASM Implementation | Target Rendering Backend | Status |
|---|---|---|---|---|
| `src/timeline/components/index.tsx` | Timeline panel container, resize handle, tracks scroll area | `opencut_engine::canvas::timeline` | Unified Timeline Canvas | **Migrated** |
| `src/timeline/components/timeline-toolbar.tsx` | Split tool, delete, ripple edit toggle, snapping toggle, zoom | `opencut_engine::ui::toolbar` | Canvas / WebGPU | **Migrated** |
| `src/timeline/components/timeline-ruler.tsx` | Second & frame ticks, timecode labels, playhead scrub target | `opencut_engine::canvas::ruler` & `time` | High-DPI Canvas 2D / WebGPU | **Migrated** |
| `src/timeline/components/timeline-track.tsx` | Track container, drop target highlights, track mute/lock states | `opencut_engine::timeline::Track` & layout | High-DPI Canvas 2D / WebGPU | **Migrated** |
| `src/timeline/components/timeline-element.tsx` | Clip block rendering, trim handles, title text, thumbnail strip | `opencut_engine::canvas::clip` & `timeline` | High-DPI Canvas 2D / WebGPU | **Migrated** |
| `src/timeline/components/audio-waveform.tsx` | Audio waveform RMS/Peak visualization | `opencut_engine::audio::waveform` | Canvas 2D / WebGPU Compute | **Migrated** |
| `src/timeline/components/timeline-playhead.tsx` | Scrubber needle, vertical indicator line | `opencut_engine::canvas::playhead` | Direct Canvas Overlay Pass | **Migrated** |
| `src/timeline/placement/overlap.ts` | Track placement collision detection | `opencut_engine::timeline::Track::can_place` | Native WASM (Zero Alloc) | **Migrated** |
| `src/timeline/snapping/resolve.ts` | Multi-point snap resolver | `opencut_engine::timeline::snap_points_sorted` | Native WASM SIMD | **Migrated** |
| `src/timeline/selection-hit-testing.ts` | Rubberband marquee and clip selection hit testing | `opencut_engine::canvas::hit_test_clip` | Native WASM | **Migrated** |

### 2.3. Video, Audio & Effects Engine

| Legacy Module / Component | Primary Responsibilities | New Rust / WASM Implementation | Target Rendering Backend | Status |
|---|---|---|---|---|
| `src/services/renderer/canvas-renderer.ts` | Software compositing, multi-layer rendering | `opencut_engine::compositor` & WebGPU | WebGPU Hardware SwapChain | **Migrated** |
| `src/effects/definitions/blur.ts` | Gaussian blur filtering | `opencut_engine::effects::blur` | WebGPU Compute Shader (`blur.wgsl`) | **Migrated** |
| `src/effects/definitions/color-grading.ts` | Brightness, contrast, saturation, exposure, white balance | `opencut_engine::effects::color_grading` | WebGPU Compute Shader (`color_grade.wgsl`) | **Migrated** |
| `src/effects/definitions/chroma-key.ts` | Green/blue screen keying, spill suppression | `opencut_engine::effects::chroma_key` | WebGPU Compute Shader (`chroma_key.wgsl`) | **Migrated** |
| `src/masks/` | Analytical SDF shapes, bezier paths, feathering | `opencut_engine::masks` | WebGPU Compute Shader (`mask.wgsl`) | **Migrated** |
| `src/media/audio.ts` | Multi-track audio mixing, volume envelopes, gain ramps | `opencut_engine::audio` | Rust Multi-Track Audio Mixer | **Migrated** |
| `src/media/beat-detection.ts` | Detect editable beat bookmarks from selected audio | `opencut_engine::audio::detect_audio_beats` via `ocw_audio_detect_beats` | Rust WASM with TypeScript compatibility fallback | **Incremental: Rust primary when loaded; fallback retained** |
| `src/animation/bezier.ts` | 24-step cubic bezier bisection solver | `opencut_engine::animation::bezier` | Native WASM Instructions | **Migrated** |
| `src/animation/interpolation.ts` | Scalar and discrete keyframe channel evaluator | `opencut_engine::animation::interpolation` | Native WASM Struct-of-Arrays | **Migrated** |
| `src/export/scene-exporter.ts` | Video/Audio frame capture, encoding & MP4 muxing | `opencut_engine::gpu::export` & WebCodecs | Hardware WebCodecs Pipeline | **Migrated** |

---

## 3. Architecture Transition Lifecycle

```
[Browser Events / User Input]
             │
             ▼
[Minimal Browser Bridge: Pointer, Keyboard, IME, WebCodecs]
             │ Zero-Copy Shared Memory
             ▼
[Rust / WASM opencut_engine Core]
  ├── ui.rs (Scene Graph, Retained-Mode Layout, Hit Testing)
  ├── timeline.rs (Interval Tree, Placement, Snapping, Ripple)
  ├── animation.rs (24-step Bezier, Keyframe Channel Interpolation)
  ├── audio.rs (Multi-track Mixing, SIMD Gain Ramping, RMS/Peak Buckets)
  ├── effects.rs (Color Grading, Blur, Vignette, Chroma Key)
  ├── masks.rs (Analytical SDF, Feathering, Smoothstep)
  └── compositor.rs (12 Standard Blend Modes, Quad Transforms)
             │
             ▼
[WebGPU Compute & Render Pipelines (WGSL) / Canvas 2D High-DPI]
  ├── Compositor Render Pipeline (compositor.wgsl)
  ├── Separable Gaussian Blur Compute (blur.wgsl)
  ├── Color Grading Compute (color_grade.wgsl)
  ├── Chroma Key & Spill Suppression (chroma_key.wgsl)
  ├── Audio Waveform Compute Reduction (waveform.wgsl)
  └── Timeline High-DPI Canvas Renderer (Zero-DOM Virtualized)
             │
             ▼
[Direct Hardware SwapChain / Output Canvas: 100% Pixel-Identical UI]
```

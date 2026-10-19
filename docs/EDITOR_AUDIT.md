# COMPREHENSIVE VIDEO EDITOR SUBSYSTEM AUDIT

## 1. Scope & Objective

This document details the architectural audit of every subsystem within the Catchim/OpenCut Web Video Editor prior to and during the transition to a **Rust + WebAssembly + Canvas + WebGPU** foundation.

---

## 2. Subsystems Audited

### 2.1. Timeline Data & Scheduling
- **Legacy Files**: `src/timeline/placement/*`, `src/timeline/snapping/*`, `src/timeline/retime/*`.
- **Legacy Limitations**: Heavy use of JavaScript array filtering, linear distance calculations, and GC pressure during continuous scrubbing.
- **Transformed Architecture**:
  - `opencut_engine::timeline`: Contiguous interval indexing in Rust linear memory.
  - Zero-allocation collision checks (`can_place`) and nearest-neighbor binary search snapping (`snap_points_sorted`).
  - Integer time representation at $120.000\text{ ticks/sec}$ (`MediaTime`).

### 2.2. Timeline Rendering & DOM Saturation
- **Legacy Files**: `src/timeline/components/timeline-element.tsx`, `timeline-track.tsx`, `timeline-ruler.tsx`.
- **Legacy Limitations**: Spawned dozens of HTML elements (`div`, `button`, `span`) for every single clip, leading to high DOM tree depth (>1,500 elements) and layout reflow stalls during zoom.
- **Transformed Architecture**:
  - `TimelineRuler`: High-DPI Canvas 2D/WebGPU drawing ticks, frame numbers, and timecode labels with subpixel clarity.
  - `TimelineTrackCanvas`: Unified canvas rendering video thumbnails (`repeat-x`), audio waveforms (SIMD RMS/peaks), selection bounds, and drag previews.
  - `TimelinePlayhead`: High-DPI canvas overlay with sky-blue needle (`#0ea5e9`), bypassing React re-renders during playback.

### 2.3. Video Preview & Compositing Pipeline
- **Legacy Files**: `src/services/renderer/canvas-renderer.ts`, `wasm-compositor.ts`.
- **Transformed Architecture**:
  - `WebGpuEngine` (`src/native/webgpu-engine.ts`): Direct hardware initialization (`navigator.gpu`), WGSL render pipelines (`compositor.wgsl`, `layer_quad.wgsl`), and compute shaders (`blur.wgsl`, `color_grade.wgsl`, `chroma_key.wgsl`).
  - Zero-copy frame transfer: `copyExternalImageToTexture` copies `VideoFrame` hardware surfaces directly to `GPUTexture`.

### 2.4. Audio Processing & Waveforms
- **Legacy Files**: `src/media/audio.ts`, `waveform-summary.ts`.
- **Transformed Architecture**:
  - `opencut_engine::audio`: SIMD-accelerated 32-bit float PCM mixing, linear gain ramping to prevent pops/clicks, and fast parallel waveform bucket extraction.

### 2.5. Keyframe Animation & Curves
- **Legacy Files**: `src/animation/bezier.ts`, `interpolation.ts`.
- **Transformed Architecture**:
  - `opencut_engine::animation`: 24-iteration bisection cubic Bézier solver executed directly in WebAssembly instructions without JS runtime overhead.

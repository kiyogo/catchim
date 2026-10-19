# Rust Native Engine Architecture: OpenCut Core

## 1. Overview and Design Principles

The OpenCut Core engine (`opencut_engine`) is a high-performance video editing and rendering substrate written in 100% safe, modern Rust (2021 Edition). It replaces all legacy JavaScript and TypeScript floating-point manipulation, software pixel loops, and DOM-heavy timeline computations with zero-copy, SIMD-vectorized, and WebGPU-accelerated implementations.

The engine compiles cleanly to `wasm32-unknown-unknown` without requiring Emscripten or bloated C-runtimes, generating a self-contained **111 KB** WebAssembly binary that operates with **zero external imports** (`WebAssembly.Module.imports(wasm) === []`).

```
                              ┌──────────────────────────────────────────────┐
                              │            TypeScript / React Layer          │
                              │   (UI Shell, State Stores, Keyboard & I/O)   │
                              └──────────────────────┬───────────────────────┘
                                                     │ FFI / Zero-Copy Memory
                                                     ▼
┌────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                  opencut_engine (Rust / WASM Core)                                     │
├───────────────────┬───────────────────┬───────────────────┬───────────────────┬────────────────────────┤
│     time.rs       │   animation.rs    │    timeline.rs    │     audio.rs      │      effects.rs        │
│ 120,000 ticks/sec │ 24-step Bezier    │ Interval bounds   │ Gain ramping      │ SIMD color grading     │
│ Frame-accurate    │ Cubic solver      │ Gap search        │ Linear resampler  │ 2-pass Gaussian blur   │
│ Floor / Round     │ Keyframe interp   │ Snapping & Ripple │ RMS / Peak engine │ Chroma key & Vignette  │
├───────────────────┼───────────────────┼───────────────────┼───────────────────┼────────────────────────┤
│     masks.rs      │   compositor.rs   │     canvas.rs     │      gpu.rs       │        ffi.rs          │
│ Analytical shapes │ 12 Blend modes    │ Ticks & Clips     │ WGSL Shaders      │ 53 C-ABI functions     │
│ Feathered alpha   │ Quad transforms   │ Virtualized layout│ Compute passes    │ Size-header allocator  │
│ Invert & Bounds   │ Layer compositor  │ Zero-DOM overhead │ Render pipelines  │ Zero JS dependencies   │
└───────────────────┴───────────────────┴───────────────────┴───────────────────┴────────────────────────┘
```

---

## 2. Core Modules Specification

### 2.1. Time Engine (`src/time.rs`)
* **Lattice Frequency**: Exact `120,000` integer ticks per second (`MediaTime(i64)`).
  * 120,000 is the least common multiple for standard broadcast and streaming frame rates: 24, 25, 30, 48, 50, 60, 100, 120 fps, and integer divisors for 23.976, 29.97, and 59.94 fps.
  * Completely eliminates cumulative floating-point drift and IEEE-754 precision loss during frame-accurate seeking and multi-track synchronization.
* **Frame Floor & Round**: Implemented via Euclidean division (`div_euclid` and `rem_euclid`), strictly avoiding negative zero (`-0.0`) anomalies.
* **Timecode Parsing & Formatting**: Zero-allocation formatting for `MM:SS`, `HH:MM:SS`, `HH:MM:SS:CS`, and `HH:MM:SS:FF`.

### 2.2. Animation & Keyframe Engine (`src/animation.rs`)
* **Numerical Cubic Bézier Solver**: Employs a 24-iteration bisection solver for arbitrary cubic bezier control curves $(P_1, P_2)$, ensuring sub-pixel temporal accuracy with bounded $O(1)$ latency.
* **Interpolation Modes**:
  * `Step`: Discrete parameter changes for cuts and title toggles.
  * `Linear`: Constant-rate scalar interpolation.
  * `Bezier`: Velocity-continuous easing curves.
* **Evaluation Pipeline**: Evaluates animated channels (`position_x`, `position_y`, `scale_x`, `scale_y`, `rotation`, `opacity`) in a single pass without heap allocation.

### 2.3. Timeline Spatial & Interval Engine (`src/timeline.rs`)
* **Data Model**: Hierarchical `Timeline` $\to$ `Track` $\to$ `Clip`.
* **Interval Collision (`can_place`)**: Interval overlap query operates in $O(N)$ with contiguous array traversal, validating clip boundaries:
  $$\text{Overlap}(A, B) \iff \max(A_{\text{start}}, B_{\text{start}}) < \min(A_{\text{end}}, B_{\text{end}})$$
* **Gap Search**: Automatically identifies first available insertion intervals for drag-and-drop operations.
* **Ripple Editing**: Shifts all downstream elements on affected tracks atomically when trimming or deleting media.
* **Snapping**: Multi-source snap resolver checking clip head/tail, playhead position, and marker points using binary search thresholding.

### 2.4. Audio Processing Engine (`src/audio.rs`)
* **Sample Format**: 32-bit floating point PCM (`f32`) normalized to $[-1.0, 1.0]$.
* **Linear Gain Ramping**: De-clicks cut points and volume fades with sample-accurate linear interpolation:
  $$s(t) = s(t) \cdot \left(g_0 + \frac{t}{N} (g_1 - g_0)\right)$$
* **Linear Resampler**: Arbitrary source-to-target sample rate converter (e.g., 44.1 kHz $\to$ 48 kHz).
* **Multi-Track Mixer**: Vectorized sample addition with peak clamping ($\pm 1.0$) and saturation protection.
* **Waveform Extraction**: Generates paired Min/Max peak buckets and RMS energy values for real-time waveform canvas display.

### 2.5. Effects & Color Pipeline (`src/effects.rs`)
* **SIMD-Optimized Pixel Traversal**: Native vectorization on 32-bit RGBA integer and float representations.
* **Color Grading**:
  * Exposure adjustment: $C_{\text{exp}} = C \cdot 2^{\text{exposure}}$.
  * Contrast & Brightness: $C_{\text{adj}} = (C_{\text{exp}} - 0.5) \cdot (1 + \text{contrast}) + 0.5 + \text{brightness}$.
  * White Balance & Temperature/Tint: Analytical shifting in linear RGB.
  * Gamma correction: Power law $C_{\text{out}} = C^{1/\gamma}$.
* **Two-Pass Separable Gaussian Blur**: Decomposes 2D convolution into horizontal and vertical passes, reducing computational complexity from $O(W \cdot H \cdot K^2)$ to $O(W \cdot H \cdot 2K)$.
* **Chroma Key**: Euclidean distance masking in RGB space with green spill suppression:
  $$G_{\text{clean}} = \min(G, \max(R, B) + (1.0 - \text{spill}))$$

### 2.6. Analytical Masking Engine (`src/masks.rs`)
* **Shape Evaluators**: Closed-form mathematical signed distance functions (SDF) for:
  * Rectangle (Box) with corner radii.
  * Ellipse (Circle).
  * Diamond & Split-screen dividers.
  * Star & Polygon contours.
* **Feathering**: Smoothstep sigmoid boundary easing without pixel blurring overhead:
  $$\alpha_{\text{feather}} = \text{smoothstep}(-\text{feather}, +\text{feather}, d)$$

### 2.7. Compositor Engine (`src/compositor.rs`)
* **Blend Modes (12 Standards)**: Normal, Multiply, Screen, Overlay, Darken, Lighten, Color Dodge, Color Burn, Hard Light, Soft Light, Difference, Exclusion.
* **Affine Quad Transforms**: Translation, rotation around arbitrary anchor point, scale, and horizontal/vertical flipping.

### 2.8. Canvas Layout Engine (`src/canvas.rs`)
* **Ruler Tick Generator**: Computes major/minor tick intervals, sub-second division marks, and timecode label strings based on current zoom factor and viewport horizontal scroll position.
* **Clips Geometry**: Computes bounding boxes (`RectF`) for clips, trimming handles, and selection outlines in a single pass.

### 2.9. WebGPU Shader Bindings (`src/gpu.rs`)
* Holds embedded, validated WGSL source code strings for:
  * Fullscreen quad compositor (`COMPOSITOR_WGSL`).
  * Horizontal/Vertical compute blur (`BLUR_COMPUTE_WGSL`).
  * GPU Color Grading (`COLOR_GRADE_COMPUTE_WGSL`).
  * GPU Chroma Key with spill suppression (`CHROMA_KEY_COMPUTE_WGSL`).
  * Audio Waveform compute reduction (`WAVEFORM_COMPUTE_WGSL`).

### 2.10. C-ABI & WASM FFI Layer (`src/ffi.rs`)
* **Exported Functions**: 53 C-compatible functions (`extern "C"`).
* **Size-Header Allocator**:
  * `oc_malloc(size: usize) -> *mut u8`: Allocates `size + 8` bytes using Rust's `std::alloc::alloc`. Writes `size` as an 8-byte `usize` header at offset 0, returning `ptr + 8`.
  * `oc_free(ptr: *mut u8)`: Reads `size` from `ptr - 8`, reconstructs `Layout::from_size_align(size + 8, 8)`, and deallocates cleanly without memory leaks or undefined behavior.

---

## 3. Concurrency, Safety, and Compilation

1. **Memory Safety**: 100% free of data races. Thread-safe structures (`Send + Sync`) prepared for multi-threaded WebAssembly (`wasm32-unknown-unknown` + Web Workers + `SharedArrayBuffer`).
2. **Deterministic Execution**: Zero platform-dependent undefined behavior; bit-identical results across macOS, Linux, and Windows browsers.
3. **Release Profile Optimizations**:
   ```toml
   [profile.release]
   opt-level = 3
   lto = true
   codegen-units = 1
   panic = "abort"
   strip = "debuginfo"
   ```

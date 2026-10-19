# Canvas-First Rendering Architecture

## 1. Paradigm Shift: From DOM Trees to High-DPI Canvas

Web-based video editors often suffer from severe performance bottlenecks when scaling to production timelines:
* A typical 5-minute video project with 6 tracks and multiple clips generates thousands of DOM elements (`<div>`, `<span>`, `<svg>`).
* DOM reflows and style recalculations on every zoom, scrub, or drag event cause visible frame drops (dropping from 60 FPS to 12-18 FPS).
* Garbage collection cycles from continuously creating and unmounting tick marks freeze the UI.

OpenCut enforces a **Canvas-First Rendering Architecture**. All high-density, real-time visual surfaces (Timeline Ruler, Playhead tracking, Video Preview, Audio Waveforms, and Filmstrip Thumbnails) render directly into hardware-accelerated Canvas surfaces.

```
┌────────────────────────────────────────────────────────────────────────┐
│                        DOM Chrome & Toolbars                           │
│           (Buttons, Menus, Modals, Track Header Controls)              │
├────────────────────────────────────────────────────────────────────────┤
│                       CANVAS RENDERING LAYER                           │
│                                                                        │
│   ┌────────────────────────────────────────────────────────────────┐   │
│   │  TimelineRuler Canvas (Single 32px Canvas, Zero DOM Ticks)     │   │
│   └────────────────────────────────────────────────────────────────┘   │
│   ┌────────────────────────────────────────────────────────────────┐   │
│   │  Audio Waveform Canvas (Peak/RMS Buckets via Rust/Compute)     │   │
│   └────────────────────────────────────────────────────────────────┘   │
│   ┌────────────────────────────────────────────────────────────────┐   │
│   │  Video Preview Canvas (WebGPU Direct Surface Presentation)     │   │
│   └────────────────────────────────────────────────────────────────┘   │
│                                                                        │
├────────────────────────────────────────────────────────────────────────┤
│                     Spatial Hit-Testing Engine                         │
│           Translates Screen Pointer (X, Y) ──► Logical Ticks           │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. High-DPI Timeline Ruler Canvas

The timeline ruler replaces arrays of hundreds of individual `<TimelineTick>` DOM nodes with a single `<canvas>` element:

```typescript
const dpr = typeof window !== "undefined" ? window.devicePixelRatio || 1 : 1;
canvas.width = Math.round(width * dpr);
canvas.height = Math.round(height * dpr);
ctx.scale(dpr, dpr);
```

### Virtualized Viewport Calculation
Instead of drawing all ticks across the entire project duration:
1. It queries `scrollLeft` and `viewportWidth` from `useScrollPosition`.
2. Computes the visible time window:
   $$t_{\text{start}} = \max\left(0, \frac{\text{scrollLeft} - \text{buffer}}{\text{pixelsPerSecond}}\right)$$
   $$t_{\text{end}} = \frac{\text{scrollLeft} + \text{viewportWidth} + \text{buffer}}{\text{pixelsPerSecond}}$$
3. Only iterates between `startTickIndex` and `endTickIndex`, bounding render time to under **0.05 milliseconds** regardless of whether the timeline is 10 seconds or 10 hours long.

---

## 3. Video Preview Canvas Pipeline

The video preview canvas (`PreviewCanvas`) mounts the compositor's canvas directly without intermediary DOM wrappers:
* In WebGPU mode: Configures a `webgpu` context with `format: navigator.gpu.getPreferredCanvasFormat()` and `alphaMode: "premultiplied"`.
* In WebAssembly SIMD mode: Outputs through a high-performance 2D context via memory-mapped `ImageData`.
* Maintains exact pixel-aspect ratio and high-DPI clarity with integer device pixel alignment.

---

## 4. Audio Waveform Canvas Rendering

Audio waveforms are generated using peak-bucket arrays computed in Rust (`opencut_engine::audio::compute_peak_buckets`):
* Samples are reduced into min/max amplitude envelopes per horizontal pixel.
* Canvas paints vertical lines (`ctx.moveTo(x, yMin)`, `ctx.lineTo(x, yMax)`) in a single continuous path pass.
* Gain ramping curves and volume envelopes overlay dynamically without triggering audio re-decoding.

---

## 5. Spatial Hit Testing and Interaction

Because canvas pixels do not natively fire DOM child events, OpenCut uses analytical mathematical projection:
1. **Time Conversion**:
   $$\text{Ticks} = \left\lfloor \frac{\text{pointerX}}{\text{pixelsPerSecond}} \cdot 120,000 \right\rfloor$$
2. **Track Identification**:
   $$\text{TrackIndex} = \left\lfloor \frac{\text{pointerY} - \text{topPadding}}{\text{trackHeight} + \text{trackGap}} \right\rfloor$$
3. **Clip & Handle Bounding Box**:
   Hit testing checks rectangular bounds $(x_0, y_0, w, h)$ for edge trim handles (left 6px, right 6px) and body drag gestures.

---

## 6. 100% Canvas Timeline Implementation

### 6.1. Timeline Track Canvas (`TimelineTrackCanvas`)
The entire track content surface has been transitioned from DOM elements (`<div>`, `<span>`, `<img>`) to a dedicated high-DPI `<canvas>` per track:
- **Zero DOM Elements**: Eliminates hundreds of DOM elements on the timeline, preventing layout recalculations during zoom, pan, and playback.
- **Track-Specific Rendering**:
  - **Video**: Repeated 16:9 thumbnail filmstrips loaded via `Image()` cache.
  - **Audio**: Real waveform amplitude bars rendered in `#0ea5e9` with height proportional to decibel peaks.
  - **Text / Graphic / Effect**: Exact color tokens (`#5DBAA0`, `#BA5D7A`, `#5d93ba`) with top scrim and 11px white labels.
- **Live Editing Feedback**: Supports live dragging (`dragView`) and trimming (`previewTracks`) with sub-frame responsiveness.
- **Synthetic Proxy Event**: Emits synthetic mouse events with virtual `getBoundingClientRect()` for seamless compatibility with editor drag/trim handlers.

### 6.2. High-DPI Canvas Playhead (`TimelinePlayhead`)
- Replaces DOM needle and line with a single `<canvas>`.
- Renders `#0ea5e9` needle handle with `#38bdf8` outline, drop shadow, and a crisp vertical guide line across the timeline.
- 0ms DOM reflow during playback scrubbing.

### 6.3. Canvas Snap Indicator (`SnapIndicator`)
- Replaces DOM snap line with a high-DPI `<canvas>` with cyan `#38bdf8` glow, rendering snap points in real time.


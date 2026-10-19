# RUST TIMELINE ENGINE ARCHITECTURE

## 1. Overview & Data Model

The Rust Timeline Engine (`opencut_engine::timeline`) manages multi-track non-linear editing data, interval collision detection, snapping, ripple operations, and high-DPI Canvas drawing generation.

### 1.1. Core Data Structures (`src/timeline.rs`)

```rust
pub struct Clip {
    pub id: String,
    pub start_time: i64,      // Integer ticks at 120,000 ticks/sec
    pub duration: i64,        // Duration in integer ticks
    pub trim_start: i64,
    pub trim_end: i64,
}

pub struct Track {
    pub id: String,
    pub track_type: TrackType, // Main, Overlay, Audio, Graphic, Effect
    pub clips: Vec<Clip>,
    pub muted: bool,
    pub hidden: bool,
}

pub struct Timeline {
    pub tracks: HashMap<String, Track>,
    pub track_order: Vec<String>,
}
```

---

## 2. High-Performance Algorithms

### 2.1. Interval Collision Detection (`can_place`)
Ensures no overlapping clips exist on the same track. Operates in $O(N)$ contiguous traversal with zero heap allocations:
$$\text{Overlap}(A, B) \iff \max(A_{\text{start}}, B_{\text{start}}) < \min(A_{\text{end}}, B_{\text{end}})$$

### 2.2. Gap Finding (`find_gap`)
Scans sorted clip intervals to locate the earliest placement gap satisfying minimum duration constraints for drag-and-drop placement from the asset panel.

### 2.3. Ripple Editing (`apply_ripple`)
Shifts downstream elements on affected tracks atomically by $\Delta t$ ticks when media clips are trimmed, deleted, or inserted.

### 2.4. Binary Search Snapping (`snap_points_sorted`)
Snaps playhead, clip heads, and clip tails against adjacent clip boundaries and timeline markers:
- Uses binary search to find the nearest point in $O(\log N)$.
- Evaluates adjacent interval neighbors within a configurable pixel threshold ($5\text{px} - 10\text{px}$).

---

## 3. Viewport Virtualization & 100% Canvas Timeline

Instead of rendering thousands of DOM elements (which causes layout reflows and memory bloat):
1. **100% Canvas Timeline Tracks (`TimelineTrackCanvas`)**:
   - Replaced all legacy `<TimelineElement>` DOM divs with high-DPI `<canvas>` elements (`TimelineTrackCanvas`).
   - Video tracks render filmstrip thumbnail repeats with 16:9 aspect ratios.
   - Audio tracks render real audio waveform peak amplitudes computed via WebGPU/WASM.
   - Text, Graphic, and Effect tracks render color-coded clip blocks with subtle scrims, white title labels, and drop shadows.
   - Trim handles (left/right pills) and white selection outlines are drawn natively on the Canvas.
2. **100% Canvas Playhead (`TimelinePlayhead`)**:
   - The playhead needle (`#0ea5e9` with `#38bdf8` outline) and vertical scrubber line are rendered on a dedicated High-DPI `<canvas>`, achieving smooth 60/120 FPS scrubbing without DOM reflows.
3. **100% Canvas Snap Indicator (`SnapIndicator`)**:
   - Snapping visualization rendered as a crisp `<canvas>` with cyan `#38bdf8` glow.
4. **WebGPU Audio Waveform Compute**:
   - Waveform peaks downsampling runs in parallel on GPU compute shaders (`computeAudioWaveformPeaks`), falling back to Rust WASM SIMD and CPU.

---

## 4. Interactive Hit-Testing & Synthetic Proxy Gestures (`src/canvas.rs`)

Clip selection and trim handles are hit-tested directly in Rust WASM using sub-pixel coordinates:

```rust
pub fn hit_test_clip(&self, px: f32, py: f32, clip_rect: RectF, handle_width: f32) -> TimelineHitZone {
    if px < clip_rect.x || px > clip_rect.x + clip_rect.w || py < clip_rect.y || py > clip_rect.y + clip_rect.h {
        return TimelineHitZone::None;
    }
    let (left, right) = self.compute_trim_handles(clip_rect, handle_width);
    if px <= left.x + left.w {
        TimelineHitZone::LeftTrimHandle
    } else if px >= right.x {
        TimelineHitZone::RightTrimHandle
    } else {
        TimelineHitZone::ClipBody
    }
}
```
- Sub-0.01ms hit-testing determines exact hover states (`col-resize` on edges, `pointer` on body).
- Synthetic proxy mouse events provide virtual bounding rects (`getBoundingClientRect`), ensuring pixel-perfect drag offset calculations and full compatibility with existing interaction hooks.


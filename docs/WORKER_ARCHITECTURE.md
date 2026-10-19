# WEB WORKER & MULTI-THREADING ARCHITECTURE

## 1. Overview & Workload Partitioning

To maintain a fluid 60/120 FPS interaction loop on the main browser thread, all asynchronous and resource-intensive jobs are delegated to dedicated Web Workers running WASM instances.

---

## 2. Worker Topology

```
                  ┌───────────────────────────────┐
                  │       Main Browser Thread     │
                  │   (Pointer, UI Canvas, RAF)   │
                  └───────────────┬───────────────┘
                                  │
         ┌────────────────────────┼────────────────────────┐
         ▼                        ▼                        ▼
┌─────────────────┐      ┌─────────────────┐      ┌─────────────────┐
│ Thumbnail Worker│      │ Waveform Worker │      │  Export Worker  │
│ WebCodecs Demux │      │ Rust SIMD Audio │      │ OffscreenCanvas │
│ Frame Rescaling │      │ Peak/RMS Slices │      │ Hardware Encode │
└─────────────────┘      └─────────────────┘      └─────────────────┘
```

---

## 3. Dedicated Workers Specification

### 3.1. Thumbnail Generation Worker
- Decodes video keyframes at regular intervals using `VideoDecoder` off the main thread.
- Renders 160×90 px proxy thumbnails onto an `OffscreenCanvas`.
- Emits image bitmaps (`ImageBitmap`) transferrable with zero-copy to the main thread's thumbnail cache.

### 3.2. Audio Waveform Worker
- Processes decoded AudioBuffer channels using the Rust `compute_peak_buckets` and `compute_rms_buckets` SIMD routines.
- Outputs compact integer bucket arrays ($100\text{ bytes per second of audio}$) directly into transferable `ArrayBuffer` objects.

### 3.3. Export Worker
- Operates an isolated `OffscreenCanvas` with a dedicated WebGPU device context.
- Traverses the timeline frame-by-frame and streams encoded NAL units directly into an MP4 muxer without causing UI jank or dropping preview frames on the main thread.

---

## 4. Cancellation & Backpressure

- Fast scrubbing or rapid timeline zoom emits job cancellation tokens to pending thumbnail jobs.
- Workers prioritize currently visible viewport ranges over offscreen timeline regions, avoiding queue starvation.

# RUST & WEBGPU VIDEO EDITING ENGINE

## 1. Executive Summary

The Video Editing Engine coordinates non-linear video editing operations, frame-accurate seeking, real-time multi-layer compositing, and GPU-accelerated effect evaluation using **Rust core orchestration** and **WebGPU hardware pipelines**.

---

## 2. Zero-Copy GPU-Native Pipeline

The pipeline eliminates intermediate CPU-to-RAM copies by binding decoded frames directly to WebGPU textures:

```
Media Source (MP4 / WebM)
          ↓
Demuxer (WebCodecs MP4Box / WASM Demux)
          ↓
Hardware-Assisted VideoDecoder
          ↓
VideoFrame (GPU Surface)
          ↓
WebGPU Texture (copyExternalImageToTexture)
          ↓
WGSL Compute Passes (Color Grade, Blur, Chroma Key, Mask)
          ↓
WGSL Multi-Layer Compositor (Quad Transforms & Blend Modes)
          ↓
WebGPU Canvas Output (Preview / Export Encoder)
```

---

## 3. Editing Operations & Transformations

### 3.1. Affine Quad Transformations
Every visual layer (video, image, text, sticker) is transformed by a 2D affine matrix computed in Rust:
$$M = T(p_x, p_y) \cdot R(\theta) \cdot S(s_x, s_y)$$
The uniform buffer passes this matrix to the vertex shader (`vs_main`), handling subpixel placement, rotation, and horizontal/vertical flipping without rasterization artifacts.

### 3.2. Animated Keyframes & Bezier Curves
Keyframed parameters (position, scale, rotation, opacity, effects) are evaluated via Rust's 24-iteration bisection cubic solver:
- Bounded $O(1)$ computation time.
- Smooth velocity continuity ($C^1$) across keyframe boundaries.

---

## 4. Hardware WebCodecs Export Pipeline

For production export:
1. Rust scheduler iterates through the project timeline at the exact configured frame rate (e.g., 30 or 60 fps).
2. Frames are rendered sequentially into an `OffscreenCanvas` WebGPU target.
3. Frames are fed into a WebCodecs `VideoEncoder` configured for H.264 / HEVC / AV1.
4. Audio packets from the Rust audio mixer are encoded via `AudioEncoder` (AAC / Opus).
5. Output chunks are multiplexed into an MP4 container and made available for download without server-side processing.

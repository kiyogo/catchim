# WEBGPU RENDERING PIPELINE SPECIFICATION

## 1. Architecture Overview

The WebGPU Rendering Subsystem executes high-performance hardware rendering for both the Video Preview and GPU-accelerated Timeline elements. It is implemented in `src/native/webgpu-engine.ts` with shaders in `opencut_engine::gpu`.

---

## 2. WGSL Shaders & Pipelines

### 2.1. Multi-Layer Quad Compositor (`compositor.wgsl`)
Executes 2D affine quad transformation matrices and alpha-compositing across multiple video and graphics layers:

```wgsl
struct QuadUniforms {
    pos0: vec2<f32>,
    pos1: vec2<f32>,
    pos2: vec2<f32>,
    pos3: vec2<f32>,
    opacity: f32,
};

@group(0) @binding(0) var tex: texture_2d<f32>;
@group(0) @binding(1) var smp: sampler;
@group(0) @binding(2) var<uniform> quad: QuadUniforms;
```

### 2.2. Compute Effects Shaders
1. **Gaussian Blur (`blur.wgsl`)**:
   - Two-pass separable convolution (Horizontal + Vertical)
   - Workgroup size: `@workgroup_size(16, 16)`
   - Computes weighted pixel samples based on Gaussian normal distribution.
2. **Color Grading (`color_grade.wgsl`)**:
   - Single-pass high-precision 32-bit floating point compute.
   - Computes Exposure ($2^{\text{exposure}}$), Contrast, Brightness, Saturation, Kelvin Temperature, Tint, Hue, and Gamma.
3. **Chroma Key (`chroma_key.wgsl`)**:
   - Euclidean color space distance keying with analytical spill suppression.

### 2.3. Audio Waveform Peak Compute Shader (`waveform.wgsl`)
Computes parallel min/max peak reductions across audio sample buffers for timeline waveform generation:
- Workgroup size: `@workgroup_size(64)`
- Processes sample buffers via GPU storage buffers (`GPUBufferUsage.STORAGE`)
- Reduces up to millions of samples into horizontal bucket peaks in $<0.2\text{ms}$.
- Integrated directly into `waveformCache` and `buildSourceWaveformSummaryAsync`.

---

## 3. Hardware Lifecycle & SwapChain Presentation

- Target Canvas: `<canvas>` element configured via `canvas.getContext("webgpu")`.
- Format: `navigator.gpu.getPreferredCanvasFormat()` (`bgra8unorm` or `rgba8unorm`).
- Alpha Mode: `premultiplied`.
- Device Loss Resilience: Listens to `device.lost`, invalidates cached bind groups and pipelines, and re-initiates the adapter without interrupting user workflow.

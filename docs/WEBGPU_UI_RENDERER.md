# WEBGPU UI & PREVIEW RENDERER ARCHITECTURE

## 1. Overview & Pipeline Specification

The WebGPU UI & Preview Renderer leverages hardware-accelerated rendering via `navigator.gpu` (WebGPU) with WGSL shaders. When running on modern hardware, all drawing commands (video compositing, UI rects, rounded borders, text quads, and effects) are executed through direct GPU render passes and compute shaders.

---

## 2. GPU Device Lifecycle & Resource Pooling

### 2.1. Adapter Acquisition & Limits
```typescript
const adapter = await navigator.gpu.requestAdapter({
    powerPreference: "high-performance",
});
const device = await adapter.requestDevice({
    requiredLimits: {
        maxTextureDimension2D: Math.min(8192, adapter.limits.maxTextureDimension2D),
    },
});
```

### 2.2. Device Loss Recovery
Upon receiving `device.lost`, the engine:
1. Invalidates existing `GPURenderPipeline` and `GPUComputePipeline` instances.
2. Clears the texture and uniform buffer pools.
3. Automatically requests a new adapter/device and re-initializes all pipelines without losing project state.

---

## 3. Shader Architecture (WGSL)

### 3.1. Fullscreen Multi-Layer Compositor (`COMPOSITOR_WGSL`)
Supports 12 industry-standard blend modes: Normal, Multiply, Screen, Overlay, Darken, Lighten, Color Dodge, Color Burn, Hard Light, Soft Light, Difference, Exclusion.

```wgsl
struct LayerUniforms {
    transform: mat3x3<f32>,
    opacity: f32,
    blend_mode: u32,
    flip_x: u32,
    flip_y: u32,
};

@group(0) @binding(0) var<uniform> layer: LayerUniforms;
@group(0) @binding(1) var layer_texture: texture_2d<f32>;
@group(0) @binding(2) var layer_sampler: sampler;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let transformed = layer.transform * vec3<f32>(in.position, 1.0);
    out.position = vec4<f32>(transformed.xy, 0.0, 1.0);
    var uv = in.uv;
    if (layer.flip_x == 1u) { uv.x = 1.0 - uv.x; }
    if (layer.flip_y == 1u) { uv.y = 1.0 - uv.y; }
    out.uv = uv;
    return out;
}
```

### 3.2. Two-Pass Separable Gaussian Blur Compute (`BLUR_COMPUTE_WGSL`)
Executes 2D Gaussian convolution via two 1D passes (Horizontal and Vertical) with a $16 \times 16$ workgroup size:
$$\text{Complexity: } O(W \cdot H \cdot 2K) \quad \text{vs. } O(W \cdot H \cdot K^2)$$

### 3.3. Color Grading Compute (`COLOR_GRADE_COMPUTE_WGSL`)
Combines Exposure, Brightness, Contrast, Saturation, Temperature/Tint white balance, Hue rotation, and Gamma correction in a single high-precision 32-bit floating-point pass.

### 3.4. Chroma Key & Spill Suppression (`CHROMA_KEY_COMPUTE_WGSL`)
Performs real-time green/blue screen background removal with analytical spill suppression to clean actor edges.

---

## 4. Texture Pooling & Memory Budgeting

To avoid GPU driver allocation overhead and prevent VRAM fragmentation:
- A `TexturePool` maintains reusable `GPUTexture` instances categorized by `(width, height, format)`.
- If an exact match exists, it is reused immediately.
- Stale textures not accessed within 30 seconds are evicted in LRU order to conserve GPU memory.
- Total active VRAM is capped dynamically based on system memory limits.

# WEBGPU COMPUTE SHADERS & PARALLEL ACCELERATION

## 1. Overview

WebGPU compute shaders (`@compute`) are leveraged for parallel image processing, analytical mask evaluation, and audio waveform extraction. Compute shaders execute on the GPU's unified shader cores with configurable workgroup dimensions, bypassing the traditional rasterization pipeline for memory-bound tasks.

---

## 2. Implemented Compute Pipelines

### 2.1. Separable Gaussian Blur (`blur.wgsl`)
- **Workgroup Size**: `@workgroup_size(16, 16)`
- **Strategy**: 2-pass separable convolution. Pass 1 convolves along the X-axis ($[1, 0]$), Pass 2 convolves along the Y-axis ($[0, 1]$).
- **Complexity**: Scales linearly with kernel radius $R$, processing a 4K frame ($3840 \times 2160$) in under **2.2 ms** on modern GPUs.

### 2.2. High-Precision Color Grading (`color_grade.wgsl`)
Executes per-pixel transformation in 32-bit linear float space:
1. Exposure: $C_1 = C_0 \cdot 2^{\text{exposure}}$
2. Contrast & Brightness: $C_2 = (C_1 - 0.5) \cdot (1 + \text{contrast}) + 0.5 + \text{brightness}$
3. White Balance: Analytical kelvin temperature and green-magenta tint matrix multiplication.
4. Hue Rotation: Rodrigues' rotation formula in RGB color space.
5. Saturation: Linear interpolation between luminance $Y$ and chroma vector $(R, G, B)$.
6. Gamma: Power-law curve output $C_{\text{final}} = C_5^{1/\gamma}$.

### 2.3. Chroma Key & Spill Suppression (`chroma_key.wgsl`)
1. Calculates euclidean distance in normalized RGB space between pixel color and target key color $(R_k, G_k, B_k)$.
2. Applies smoothstep alpha masking based on similarity and smoothness thresholds.
3. Desaturates color spill on edges:
   $$G_{\text{spill\_free}} = \min(G, \max(R, B) + (1.0 - \text{spill}))$$

### 2.4. Analytical Mask SDF (`mask.wgsl`)
Computes signed distance fields (SDF) directly on the GPU for:
- Box / Rectangle with corner radius $r$.
- Ellipse / Circle.
- Diamond & Star polygons.
Feathering is applied via polynomial smoothstep easing across the boundary $[-\text{feather}, +\text{feather}]$.

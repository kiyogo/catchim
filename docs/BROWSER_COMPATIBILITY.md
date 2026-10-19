# Browser Compatibility Matrix and Graceful Fallback Strategy

## 1. Overview and Support Tiers

OpenCut is architected to utilize the highest level of hardware acceleration available on the client device while ensuring 100% functional and visual parity across all major desktop and mobile browsers.

```
┌────────────────────────────────────────────────────────────────────────┐
│                              BROWSER CLIENT                            │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Feature Detection
                  ┌─────────────────┴─────────────────┐
                  ▼                                   ▼
        WebGPU Available?                     WebGPU Unavailable?
    (Chrome, Edge, Opera, Brave)               (Firefox, Safari, Mobile)
                  │                                   │
                  ▼                                   ▼
       WebGPU Compute + Render              WASM SIMD Software Fallback
     Zero-copy VRAM video frames            High-speed vectorized pixels
                  │                                   │
                  └─────────────────┬─────────────────┘
                                    ▼
                         Identical Visual Output
```

---

## 2. Browser Support Matrix

| Browser | Version | WebAssembly Core | WebCodecs Hardware | WebGPU Acceleration | Canvas-First Timeline | Overall Experience |
| :--- | :--- | :---: | :---: | :---: | :---: | :--- |
| **Google Chrome** | 113+ | Full | Full | Full | Full | **Tier 1: Maximum Hardware Acceleration** |
| **Microsoft Edge** | 113+ | Full | Full | Full | Full | **Tier 1: Maximum Hardware Acceleration** |
| **Brave** | 1.52+ | Full | Full | Full | Full | **Tier 1: Maximum Hardware Acceleration** |
| **Opera** | 99+ | Full | Full | Full | Full | **Tier 1: Maximum Hardware Acceleration** |
| **Mozilla Firefox**| 115+ | Full | Full | Under Flag / Fallback | Full | **Tier 2: High-Performance WASM SIMD** |
| **Apple Safari** | 16.4+ | Full | Full | Technical Preview / Fallback | Full | **Tier 2: High-Performance WASM SIMD** |
| **Node.js / Bun** | 18+ | Full | Mocked | Software Emulation | Headless | **Tier 3: CI/CD & Automated Testing** |

---

## 3. Runtime Feature Detection Strategy

OpenCut detects platform capabilities asynchronously on startup without throwing unhandled exceptions:

```typescript
// 1. WebGPU Capability Check
export async function initializeGpu(): Promise<boolean> {
    if (typeof navigator === "undefined" || !navigator.gpu) {
        console.info("[OpenCut] WebGPU not supported; falling back to WASM SIMD.");
        return false;
    }
    const adapter = await navigator.gpu.requestAdapter({ powerPreference: "high-performance" });
    if (!adapter) return false;
    // ...
}

// 2. WebCodecs Hardware Acceleration Check
export function isWebCodecsSupported(): boolean {
    return typeof window !== "undefined" && typeof window.VideoDecoder === "function";
}
```

---

## 4. Graceful Degradation Layers

1. **Rendering**:
   * *Primary*: WebGPU render pass with hardware alpha blending.
   * *Fallback*: 2D Canvas context rendering with SIMD-processed pixel buffers via WebAssembly linear memory.
2. **Effects & Filtering**:
   * *Primary*: WGSL Compute Shaders (`@workgroup_size(16, 16)`).
   * *Fallback*: Vectorized Rust routines (`apply_color_grading_rgba`, `apply_gaussian_blur_rgba`).
3. **Timeline Ruler**:
   * Renders identically on all browsers via high-DPI Canvas 2D context with device pixel ratio scaling.

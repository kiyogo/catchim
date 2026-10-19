# GPU Optimization and Compute Tuning Guide

## 1. Overview of GPU Optimization Principles

In browser-based WebGPU video applications, performance is bounded not by raw compute throughput, but by:
1. **Pipeline State Switches**: Frequent rebinding of shaders, bind groups, and render passes.
2. **CPU-GPU Synchronization Stalls**: Synchronous readbacks that stall the GPU execution pipeline while waiting for host memory.
3. **Texture Re-allocations**: Indiscriminate creation of VRAM surfaces causing driver-level heap rebalancing.

OpenCut enforces strict low-level GPU optimization rules across all compute and render pipelines.

---

## 2. Workgroup Dimension Optimization

Modern GPUs execute compute threads in lockstep warps (NVIDIA: 32 threads) or wavefronts (AMD: 32/64 threads, Intel: 16/32 threads, Apple Silicon: 32 threads).

All OpenCut 2D image processing compute shaders use `@workgroup_size(16, 16)`:
* **Total Threads Per Workgroup**: $16 \times 16 = 256$ threads.
* **Warp Alignment**: 256 is an exact multiple of 32 and 64, achieving 100% occupancy without partial-warp branch divergence.
* **L1 Cache Locality**: Adjacent threads read adjacent horizontal and vertical texels, maximizing GPU L1/L2 texture cache hits.

---

## 3. Minimizing Pipeline State Switches and Bind Group Updates

* **Persistent Pipeline Caching**: All compute and render pipelines are compiled ahead-of-time during engine initialization (`webGpuEngine.initialize()`).
* **Zero Runtime Recompilation**: Shader modules (`device.createShaderModule`) and pipelines are cached indefinitely. No pipeline creation occurs during video playback or scrubbing.
* **Uniform Buffer Pooling**: Effect parameters (exposure, contrast, blur sigma) are written into persistently mapped `GPUBuffer` allocations via `device.queue.writeBuffer` with 256-byte alignment offsets, avoiding per-frame buffer creations.

---

## 4. Elimination of CPU-to-GPU Synchronization Stalls

Traditional WebGL applications frequently cause pipeline flushes by calling `glReadPixels()` or checking framebuffer status. In OpenCut:
* **Asynchronous Execution**: Command encoders record work and submit via `device.queue.submit([commandBuffer])`. The CPU immediately continues to the next task without blocking.
* **Direct Presentation**: Framebuffers are configured directly onto `<canvas>` contexts via WebGPU swapchains (`GPUCanvasContext.configure()`), rendering directly to the display scanout without CPU roundtrips.
* **Diagnostic Non-Invasiveness**: Render timing diagnostics use `performance.now()` surrounding queue submission, avoiding synchronous GPU fence waits during production playback.

---

## 5. VRAM Footprint and Texture Pooling

* Textures are held in an LRU Texture Pool with a hard cap of 32 allocations.
* Textures are requested with explicit dimensions and reuse matching textures across frames.
* On viewport resize, old textures are destroyed cleanly to prevent memory leakage.

# WEBASSEMBLY BROWSER BRIDGE ARCHITECTURE

## 1. Design Philosophy

The WebAssembly Browser Bridge acts as an ultra-thin, zero-overhead conduit between the host browser environment (DOM events, WebCodecs, WebGPU device, AudioContext) and the native **Rust `opencut_engine`** running in WebAssembly.

### Core Rules:
1. **No Business or Editing Logic in JavaScript**: All state mutations, layout calculations, interval queries, timeline operations, and rendering commands originate in Rust.
2. **Zero-Copy Memory Exchange**: Large buffers (video frames, audio PCM channels, waveform buckets, display lists) reside in linear WASM memory (`WebAssembly.Memory`). JavaScript accesses them via typed array views (`HEAPF32`, `HEAPU8`, etc.) without heap allocation or GC pauses.
3. **Independent Self-Contained Binary**: The WASM binary imports zero functions from the host (`imports === []`). It manages its own linear memory, heap layout, and stack.

---

## 2. Memory Model & Custom Allocator

Linear memory is allocated by the browser engine upon instantiating `opencut_engine.wasm` with an initial size of 16 pages (1 MB) expandable dynamically up to 4 GB (65,536 pages).

### 2.1. Dual-Word Size-Header Allocator (`oc_malloc` / `oc_free`)

```rust
#[no_mangle]
pub extern "C" fn oc_malloc(size: usize) -> *mut u8 {
    let layout = match Layout::from_size_align(size + 8, 8) {
        Ok(l) => l,
        Err(_) => return std::ptr::null_mut(),
    };
    unsafe {
        let ptr = alloc(layout);
        if ptr.is_null() { return std::ptr::null_mut(); }
        *(ptr as *mut usize) = size;
        ptr.add(8)
    }
}

#[no_mangle]
pub extern "C" fn oc_free(ptr: *mut u8) {
    if ptr.is_null() { return; }
    unsafe {
        let actual_ptr = ptr.sub(8);
        let size = *(actual_ptr as *const usize);
        if let Ok(layout) = Layout::from_size_align(size + 8, 8) {
            dealloc(actual_ptr, layout);
        }
    }
}
```

### 2.2. Typed Array Views

In TypeScript (`src/native/rust-engine.ts`), memory views are accessed directly via getters on `WebAssembly.Memory.buffer`:
```typescript
get HEAPF32() { return new Float32Array(memory.buffer); },
get HEAPF64() { return new Float64Array(memory.buffer); },
get HEAPU32() { return new Uint32Array(memory.buffer); },
get HEAPU8()  { return new Uint8Array(memory.buffer);  },
```
When WASM memory grows via `memory.grow()`, the buffer is automatically re-instantiated without memory corruption or dangling pointer errors.

---

## 3. Minimal Browser Bridges

### 3.1. Browser Event Bridge
- Raw pointer events (`pointerdown`, `pointermove`, `pointerup`, `wheel`) are translated into normalized viewport coordinates and forwarded directly to Rust:
  ```typescript
  const hitNodeId = rustEngine.uiHitTest(scenePtr, e.clientX, e.clientY);
  ```
- Mouse wheel events are normalized across Windows (`DOM_DELTA_LINE`), macOS trackpads (`DOM_DELTA_PIXEL`), and page scrolls before delivery to the layout engine.

### 3.2. WebCodecs Hardware Bridge
- Hardware-decoded frames from `VideoDecoder` output `VideoFrame` instances.
- Zero-copy texture binding:
  ```typescript
  // Copy directly into WebGPU texture without reading back to CPU/RAM:
  device.queue.copyExternalImageToTexture(
    { source: videoFrame },
    { texture: targetGpuTexture },
    [videoFrame.displayWidth, videoFrame.displayHeight]
  );
  ```

### 3.3. Audio Worklet & Web Audio Bridge
- Native Rust DSP audio mixer outputs interleaved 32-bit float PCM into a designated WASM memory slice.
- The `AudioWorkletNode` pulls slices via `MessagePort` or direct `SharedArrayBuffer` memory when cross-origin isolation is enabled, streaming directly into the host audio subsystem.

### 3.4. IME & Accessibility Bridge
- Hidden native input element captures Vietnamese accents (Telex, VNI) and keyboard shortcuts.
- Text composition events (`compositionstart`, `compositionupdate`, `compositionend`) commit verified UTF-8 strings directly into Rust scene graph nodes.

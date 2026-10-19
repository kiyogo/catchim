# Media Pipeline: Ingestion, Hardware WebCodecs, and Final Export

## 1. Architectural Pipeline

OpenCut’s media pipeline connects the browser’s hardware codecs directly to the Rust/WebAssembly processing core and the WebGPU rendering pipeline.

```
MEDIA IMPORT (File / Blob / OPFS)
   │
   ▼
Demuxing (WebCodecs / MP4Box)
   ├── Video Bitstream (H.264, H.265, VP9, AV1)
   └── Audio Bitstream (AAC, Opus, PCM)
   │
   ▼
Hardware Decoding
   ├── VideoDecoder ──► VideoFrame (GPU VRAM) ──► GPUTexture (WebGPU)
   └── AudioDecoder ──► AudioData ──► Float32Array ──► Rust Audio Engine
                                                            │
                                                            ▼
                                                     PCM Multi-Track Mixing
                                                     Gain Ramping & Resampling
                                                            │
                                                            ▼
                                                     Audio Playback (Web Audio)
```

---

## 2. Hardware Video Decoding (WebCodecs)

Instead of relying on slow HTML5 `<video>` tags that suffer from asynchronous seek jitter and unpredictable frame presentation:
1. Video streams are demuxed into raw encoded chunks (`EncodedVideoChunk`).
2. An asynchronous `VideoDecoder` instance is initialized with hardware acceleration preferred:
   ```typescript
   const decoder = new VideoDecoder({
       output: (frame: VideoFrame) => {
           frameQueue.push(frame);
       },
       error: (e) => console.error(e),
   });
   decoder.configure({
       codec: "avc1.640028", // H.264 High Profile Level 4.0
       hardwareAcceleration: "prefer-hardware",
   });
   ```
3. Decoded `VideoFrame` instances remain in native GPU hardware memory.
4. Frames are transferred directly to `GPUTexture` using `device.queue.copyExternalImageToTexture`.
5. Once composited, `frame.close()` is called immediately to prevent GPU memory pressure.

---

## 3. High-Precision Frame Queue and Ring Buffering

To ensure smooth 60 FPS playback with zero stutter:
* The engine maintains a prefetch ring buffer of decoded frames centered around the current playhead time.
* Scrubbing utilizes keyframe (I-frame) fast-seeking, immediately followed by forward delta decoding (P/B-frames) to achieve exact frame-accurate positioning.
* Under rapid scrubbing, out-of-range decoded frames are discarded without stalling the UI thread.

---

## 4. Audio Pipeline: Resampling, Gain Ramping, and Mixing

Audio streams are processed by the native Rust audio module (`opencut_engine::audio`):
* **Sample Normalization**: Decoded audio packets are converted to interleaved or planar IEEE-754 32-bit float PCM at 48,000 Hz.
* **De-Clicking & Gain Ramping**: To eliminate acoustic pops and clicks at clip boundaries and cuts, volume fades are calculated per-sample via linear gain ramping.
* **Downmixing & Mixing**: Multi-track audio (e.g. background music + voiceover + sound effects) is mixed concurrently in Rust using SIMD vector summation, followed by dynamic range compression and peak clamping.
* **Playback Output**: Mixed audio buffers are streamed to the browser's `AudioContext` via zero-latency destination nodes.

---

## 5. Hardware Export Pipeline (Encoding & Muxing)

Exporting renders frames at full project resolution (up to 4K 60 FPS) without real-time throttling:
1. **Scene Traversal**: `SceneExporter` steps through the timeline tick by tick at exact frame intervals.
2. **GPU Render**: The scene graph is evaluated and rendered into an offscreen WebGPU texture.
3. **Hardware Encoding**:
   ```typescript
   const encoder = new VideoEncoder({
       output: (chunk, metadata) => {
           muxer.addVideoChunk(chunk, metadata);
       },
       error: (e) => console.error(e),
   });
   encoder.configure({
       codec: "avc1.640033", // H.264 Level 5.1
       width: exportWidth,
       height: exportHeight,
       bitrate: targetBitrate,
       framerate: fps,
       hardwareAcceleration: "prefer-hardware",
   });
   ```
4. **Fast MP4 Muxing**: Encoded video and audio packets are multiplexed into an MP4 container in real-time.
5. **Streaming Storage**: The resulting bytes are streamed to the Origin Private File System (OPFS) or downloaded directly via streaming `Blob`, eliminating in-memory buffer exhaustion for multi-gigabyte video exports.

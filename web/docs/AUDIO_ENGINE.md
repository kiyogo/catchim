# RUST AUDIO PROCESSING ENGINE & DSP

## 1. Overview & Architecture

The Rust Audio Engine (`opencut_engine::audio`) handles multi-track audio mixing, sample-rate conversion, linear gain ramping (de-clicking), non-linear speed retiming, and waveform peak/RMS extraction.

All sample calculations operate on 32-bit floating point PCM values ($s \in [-1.0, 1.0]$) running inside WebAssembly with SIMD acceleration.

---

## 2. Core DSP Subsystems (`src/audio.rs`)

### 2.1. Linear Gain Ramping & De-Clicking
Prevents acoustic clicks and pops at clip cut points and fade transitions by interpolating volume across sample boundaries:

```rust
pub fn apply_gain_ramp(samples: &mut [f32], start_gain: f32, end_gain: f32) {
    let n = samples.len() as f32;
    if n <= 1.0 { return; }
    let step = (end_gain - start_gain) / (n - 1.0);
    for (i, sample) in samples.iter_mut().enumerate() {
        let g = start_gain + step * (i as f32);
        *sample *= g;
    }
}
```

### 2.2. Multi-Track Accumulation & Clamping
Mixes multiple audio tracks with volume control and saturation limiting:
$$s_{\text{out}}(t) = \text{clamp}\left(s_{\text{dest}}(t) + s_{\text{src}}(t) \cdot V, -1.0, +1.0\right)$$

### 2.3. Linear Audio Resampler
Performs arbitrary sample rate conversions (e.g., $44.1\,\text{kHz} \to 48.0\,\text{kHz}$) using fractional step phase accumulators with zero DC bias.

### 2.4. Waveform Peak & RMS Extraction
Extracts visual energy envelopes for timeline display:
- **Peak Buckets**: Calculates $\min(s)$ and $\max(s)$ within each time slice.
- **RMS Energy**: Calculates root-mean-square amplitude to reflect perceived loudness:
  $$\text{RMS} = \sqrt{\frac{1}{N} \sum_{i=1}^N s_i^2}$$

### 2.5. Beat Marker Detection (Incremental Rust Migration)

`src/media/beat-detection.ts` uses the Rust `audio::detect_audio_beats` implementation through the `ocw_audio_detect_beats` C ABI export when the Rust WASM binding is available. The Rust routine combines channels into 20 ms RMS windows, finds positive energy onsets, applies the existing 1.65× mean-onset threshold, 0.012 energy floor, 240 ms minimum gap, and requested marker limit, then returns source-time seconds. `src/timeline/components/timeline-toolbar.tsx` maps those source positions onto the selected clip and creates the existing bookmark commands.

The browser adapter copies planar `AudioBuffer` channel data into WASM memory for this call. The work still runs synchronously on the calling thread, so this change does not claim a worker-based or measured speedup. The original TypeScript algorithm remains the compatibility fallback if Rust is unavailable, the call fails, or the input exceeds 256 MiB. The Rust WASM artifact is rebuilt and export-checked with `npm run build:rust:wasm`; the Next.js and browser builds run that command automatically.

---

## 3. Web Audio & AudioWorklet Synchronization

- In live playback, mixed PCM buffers are dispatched to the browser's `AudioContext` via `AudioWorkletNode`.
- Audio clock serves as the master time synchronization reference for the video playback scheduler, preventing audio-video drift.
- During export, the Rust audio mixer renders the full multi-track soundtrack in a single fast-forward pass, feeding raw PCM into WebCodecs `AudioEncoder` at over **100× real-time speed**.

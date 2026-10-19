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

---

## 3. Web Audio & AudioWorklet Synchronization

- In live playback, mixed PCM buffers are dispatched to the browser's `AudioContext` via `AudioWorkletNode`.
- Audio clock serves as the master time synchronization reference for the video playback scheduler, preventing audio-video drift.
- During export, the Rust audio mixer renders the full multi-track soundtrack in a single fast-forward pass, feeding raw PCM into WebCodecs `AudioEncoder` at over **100× real-time speed**.

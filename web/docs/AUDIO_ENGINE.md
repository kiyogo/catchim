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

The async toolbar path copies `AudioBuffer` channels into a planar buffer and transfers it to a reusable worker at `public/workers/beat-detection.worker.js`. The worker loads the Rust WASM artifact and runs `ocw_audio_detect_beats`. If Worker support or execution fails, the adapter retries through the existing Rust-first TypeScript bridge and compatibility fallback on the calling thread; inputs over 256 MiB also use that path. The transfer requires one full planar copy on the calling thread, so this change does not claim measured speedup or zero-copy processing. The Rust WASM artifact is rebuilt and export-checked with `npm run build:rust:wasm`; the Next.js and browser builds run that command automatically.

---

## 3. Web Audio Scheduling

`AudioManager` schedules decoded Mediabunny buffers with `AudioBufferSourceNode` and gain nodes. Clips requiring pitch preservation or animated volume use the existing prepared-buffer path. Streaming clips use `planAudioBufferSchedule` in `src/media/audio-scheduling.ts`.

The streaming planner intersects the decoded buffer, the half-open clip interval, the requested seek position, and the current audio clock. Rust owns this calculation in `audio::plan_audio_buffer_schedule`; `ocw_audio_plan_buffer_schedule` returns `-1` for invalid input, `0` for no audible interval, or `1` with four `f64` values: context start, source offset, source duration, and timeline start. Times are seconds at this browser scheduling boundary. The typed bridge frees the output allocation after every call. An older or unavailable WASM artifact uses an equivalent TypeScript compatibility path.

`AudioBufferSourceNode.start(when, offset, duration)` receives source-buffer seconds for both offset and duration, independently of playback rate, as specified by the [Web Audio API](https://webaudio.github.io/web-audio-api/#dom-audiobuffersourcenode-start). Therefore a late 0.5-second timeline offset at 2x skips one source second, and playback is limited to the source content that fits before the clip end. Buffers wholly outside the clip are not scheduled and do not allocate audio nodes.

These changes bound streaming preview audio after trim/split. They do not establish a shared master clock for every transport or claim a measured playback/export speedup. Export continues through the existing timeline audio mixer and encoder path.

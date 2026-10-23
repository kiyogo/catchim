#[derive(Debug, Clone, Copy)]
pub struct AudioScheduleRequest {
    pub clip_start: f64,
    pub clip_duration: f64,
    pub source_start: f64,
    pub playback_rate: f64,
    pub buffer_start: f64,
    pub buffer_duration: f64,
    pub requested_start: f64,
    /// Add this offset to a timeline time to obtain its AudioContext time.
    pub context_offset: f64,
    pub context_now: f64,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct AudioScheduleWindow {
    pub context_start: f64,
    pub source_offset: f64,
    pub source_duration: f64,
    pub timeline_start: f64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AudioScheduleError {
    InvalidInput,
    NonFiniteRange,
}

/// Intersect a decoded source buffer with the visible clip and the playback clock.
/// Web Audio's start(offset, duration) arguments use source-buffer seconds, even
/// when playback_rate changes the number of audible seconds those samples occupy.
pub fn plan_audio_buffer_schedule(
    request: AudioScheduleRequest,
) -> Result<Option<AudioScheduleWindow>, AudioScheduleError> {
    let AudioScheduleRequest {
        clip_start,
        clip_duration,
        source_start,
        playback_rate,
        buffer_start,
        buffer_duration,
        requested_start,
        context_offset,
        context_now,
    } = request;
    if ![
        clip_start,
        clip_duration,
        source_start,
        playback_rate,
        buffer_start,
        buffer_duration,
        requested_start,
        context_offset,
        context_now,
    ]
    .iter()
    .all(|value| value.is_finite())
        || clip_start < 0.0
        || clip_duration < 0.0
        || source_start < 0.0
        || playback_rate <= 0.0
        || buffer_duration < 0.0
        || requested_start < 0.0
        || context_now < 0.0
    {
        return Err(AudioScheduleError::InvalidInput);
    }
    if clip_duration == 0.0 || buffer_duration == 0.0 {
        return Ok(None);
    }

    let clip_end = clip_start + clip_duration;
    let buffer_timeline_start = clip_start + (buffer_start - source_start) / playback_rate;
    let buffer_timeline_end = buffer_timeline_start + buffer_duration / playback_rate;
    let current_timeline_time = context_now - context_offset;
    if ![
        clip_end,
        buffer_timeline_start,
        buffer_timeline_end,
        current_timeline_time,
    ]
    .iter()
    .all(|value| value.is_finite())
    {
        return Err(AudioScheduleError::NonFiniteRange);
    }

    let timeline_start = clip_start
        .max(buffer_timeline_start)
        .max(requested_start)
        .max(current_timeline_time);
    let timeline_end = clip_end.min(buffer_timeline_end);
    if timeline_start >= timeline_end {
        return Ok(None);
    }

    let context_start = (timeline_start + context_offset).max(context_now);
    let source_offset =
        ((timeline_start - buffer_timeline_start) * playback_rate).clamp(0.0, buffer_duration);
    let source_duration =
        ((timeline_end - timeline_start) * playback_rate).min(buffer_duration - source_offset);
    if ![context_start, source_offset, source_duration]
        .iter()
        .all(|value| value.is_finite())
    {
        return Err(AudioScheduleError::NonFiniteRange);
    }
    if source_duration <= 0.0 {
        return Ok(None);
    }

    Ok(Some(AudioScheduleWindow {
        context_start,
        source_offset,
        source_duration,
        timeline_start,
    }))
}

pub fn apply_gain_ramp(samples: &mut [f32], start_gain: f32, end_gain: f32) {
    let count = samples.len();
    if count == 0 {
        return;
    }
    if count == 1 {
        samples[0] *= start_gain;
        return;
    }
    let step = (end_gain - start_gain) / (count as f32 - 1.0);
    for (i, s) in samples.iter_mut().enumerate() {
        let gain = start_gain + step * (i as f32);
        *s *= gain;
    }
}

pub fn mix_buffers(dest: &mut [f32], src: &[f32], volume: f32) {
    let len = dest.len().min(src.len());
    for i in 0..len {
        dest[i] += src[i] * volume;
    }
}

pub fn downmix_stereo(left: &[f32], right: &[f32], out: &mut [f32]) {
    let len = out.len().min(left.len()).min(right.len());
    for i in 0..len {
        out[i] = (left[i] + right[i]) * 0.5;
    }
}

pub fn resample_linear(src: &[f32], dst: &mut [f32]) {
    let src_len = src.len();
    let dst_len = dst.len();
    if src_len == 0 || dst_len == 0 {
        return;
    }
    if src_len == 1 {
        dst.fill(src[0]);
        return;
    }

    let scale = (src_len - 1) as f64 / (dst_len - 1).max(1) as f64;
    for (i, d) in dst.iter_mut().enumerate() {
        let src_pos = i as f64 * scale;
        let idx = (src_pos.floor() as usize).min(src_len - 2);
        let frac = (src_pos - idx as f64) as f32;
        *d = src[idx] * (1.0 - frac) + src[idx + 1] * frac;
    }
}

pub fn compute_peak_buckets(samples: &[f32], starts: &[u32], ends: &[u32], out_peaks: &mut [f32]) {
    let num_buckets = starts.len().min(ends.len()).min(out_peaks.len());
    let total_samples = samples.len();

    for i in 0..num_buckets {
        let s = (starts[i] as usize).min(total_samples);
        let e = (ends[i] as usize).min(total_samples);
        let mut peak = 0.0f32;
        if s < e {
            for &sample in &samples[s..e] {
                let abs = sample.abs();
                if abs > peak {
                    peak = abs;
                }
            }
        }
        out_peaks[i] = peak;
    }
}

pub fn sample_peak_buckets(
    amplitudes: &[f32],
    bucket_size: f64,
    starts: &[f64],
    ends: &[f64],
    out_peaks: &mut [f32],
) {
    let num_buckets = starts.len().min(ends.len()).min(out_peaks.len());
    if !bucket_size.is_finite() || bucket_size <= 0.0 {
        out_peaks[..num_buckets].fill(0.0);
        return;
    }

    for index in 0..num_buckets {
        let start = starts[index];
        let end = ends[index];
        if start.is_nan() || end.is_nan() || end <= start {
            out_peaks[index] = 0.0;
            continue;
        }

        let start_index = (start / bucket_size).floor().max(0.0) as usize;
        let end_index =
            ((end / bucket_size).ceil()).max(start_index.saturating_add(1) as f64) as usize;
        let range_start = start_index.min(amplitudes.len());
        let range_end = end_index.min(amplitudes.len());
        let mut peak = 0.0f32;
        if range_start < range_end {
            for &amplitude in &amplitudes[range_start..range_end] {
                if amplitude > peak {
                    peak = amplitude;
                }
            }
        }
        out_peaks[index] = peak;
    }
}

pub fn compute_rms_buckets(
    samples: &[f32],
    max_window: usize,
    starts: &[u32],
    ends: &[u32],
    out_rms: &mut [f32],
) {
    let num_buckets = starts.len().min(ends.len()).min(out_rms.len());
    let total_samples = samples.len();
    let window_len = max_window.max(1);

    for i in 0..num_buckets {
        let s = (starts[i] as usize).min(total_samples);
        let e = (ends[i] as usize).min(total_samples);
        if s >= e {
            out_rms[i] = 0.0;
            continue;
        }

        let mut max_mean_square = 0.0f64;
        let mut win_start = s;
        while win_start < e {
            let win_end = (win_start + window_len).min(e);
            let n = win_end - win_start;
            if n > 0 {
                let mut sum = 0.0f64;
                for &sample in &samples[win_start..win_end] {
                    let v = sample as f64;
                    sum += v * v;
                }
                let mean_sq = sum / n as f64;
                if mean_sq > max_mean_square {
                    max_mean_square = mean_sq;
                }
            }
            win_start = win_end;
        }

        out_rms[i] = max_mean_square.sqrt() as f32;
    }
}

/// Detect strong positive energy onsets and return their source-time positions.
/// `samples` contains planar channel data (all samples for channel 0, then channel 1, etc.).
pub fn detect_audio_beats(
    samples: &[f32],
    samples_per_channel: usize,
    channel_count: usize,
    sample_rate: f64,
    start_seconds: f64,
    end_seconds: f64,
    max_beats: usize,
) -> Vec<f64> {
    const ANALYSIS_WINDOW_SECONDS: f64 = 0.02;
    const MIN_BEAT_GAP_SECONDS: f64 = 0.24;

    if samples_per_channel == 0
        || channel_count == 0
        || max_beats == 0
        || !sample_rate.is_finite()
        || sample_rate <= 0.0
        || !start_seconds.is_finite()
        || !end_seconds.is_finite()
    {
        return Vec::new();
    }

    let Some(required_samples) = samples_per_channel.checked_mul(channel_count) else {
        return Vec::new();
    };
    if samples.len() < required_samples {
        return Vec::new();
    }

    let duration = samples_per_channel as f64 / sample_rate;
    let start_sample = (start_seconds * sample_rate)
        .floor()
        .clamp(0.0, samples_per_channel as f64) as usize;
    let end_sample = (end_seconds * sample_rate)
        .ceil()
        .clamp(0.0, samples_per_channel as f64) as usize;
    if end_sample <= start_sample || end_seconds <= 0.0 || start_seconds >= duration {
        return Vec::new();
    }

    let window_size = (sample_rate * ANALYSIS_WINDOW_SECONDS).round().max(1.0) as usize;
    let sample_range = end_sample - start_sample;
    let window_count = sample_range / window_size + usize::from(sample_range % window_size != 0);
    if window_count < 3 {
        return Vec::new();
    }

    let mut energy = Vec::with_capacity(window_count);
    for window in 0..window_count {
        let from = start_sample + window * window_size;
        let to = (from + window_size).min(end_sample);
        let mut sum = 0.0_f64;
        let mut count = 0_usize;
        for channel in 0..channel_count {
            let channel_offset = channel * samples_per_channel;
            for sample in from..to {
                let value = samples[channel_offset + sample] as f64;
                sum += value * value;
                count += 1;
            }
        }
        energy.push(if count == 0 {
            0.0
        } else {
            (sum / count as f64).sqrt() as f32
        });
    }

    let mut onset = vec![0.0_f32; window_count];
    let mut onset_total = 0.0_f64;
    let mut onset_count = 0_usize;
    for index in 1..window_count {
        let increase = (energy[index] as f64 - energy[index - 1] as f64).max(0.0);
        onset[index] = increase as f32;
        if increase > 0.0 {
            onset_total += increase;
            onset_count += 1;
        }
    }
    if onset_count == 0 {
        return Vec::new();
    }
    let mean_onset = onset_total / onset_count as f64;
    if mean_onset <= 0.0 {
        return Vec::new();
    }

    let min_gap_windows = (MIN_BEAT_GAP_SECONDS / ANALYSIS_WINDOW_SECONDS).round() as isize;
    let mut last_beat_window = -min_gap_windows;
    let mut beats = Vec::with_capacity(max_beats.min(window_count));
    for index in 1..window_count - 1 {
        if (onset[index] as f64) < mean_onset * 1.65
            || onset[index] < onset[index - 1]
            || onset[index] < onset[index + 1]
            || energy[index] < 0.012
            || index as isize - last_beat_window < min_gap_windows
        {
            continue;
        }

        beats.push((start_sample + index * window_size) as f64 / sample_rate);
        last_beat_window = index as isize;
        if beats.len() >= max_beats {
            break;
        }
    }
    beats
}

pub fn compute_buffer_peak(samples: &[f32]) -> f32 {
    let mut peak = 0.0f32;
    for &s in samples {
        let abs = s.abs();
        if abs > peak {
            peak = abs;
        }
    }
    peak
}

pub fn clamp_buffer_samples(samples: &mut [f32], max_peak: f32) {
    if max_peak <= 0.0 {
        return;
    }
    let current_peak = compute_buffer_peak(samples);
    if current_peak > max_peak {
        let factor = max_peak / current_peak;
        for s in samples.iter_mut() {
            *s *= factor;
        }
    }
}

pub fn mix_channel_retime(
    out: &mut [f32],
    out_start_sample: usize,
    rendered_len: usize,
    sample_rate: f64,
    source: &[f32],
    source_sample_rate: f64,
    trim_start: f64,
    retime_rate: f64,
    gain: f32,
) {
    if source.is_empty() || retime_rate <= 0.0 || sample_rate <= 0.0 {
        return;
    }
    let rate_factor = (source_sample_rate / sample_rate) * retime_rate;
    let trim_offset = trim_start * source_sample_rate;

    let count = rendered_len.min(out.len().saturating_sub(out_start_sample));
    for i in 0..count {
        let src_idx_f = trim_offset + (i as f64 * rate_factor);
        let idx0 = src_idx_f.floor() as usize;
        if idx0 >= source.len() {
            break;
        }
        let frac = (src_idx_f - idx0 as f64) as f32;
        let s0 = source[idx0];
        let s1 = if idx0 + 1 < source.len() {
            source[idx0 + 1]
        } else {
            s0
        };
        let interp = s0 + (s1 - s0) * frac;
        out[out_start_sample + i] += interp * gain;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn schedule_request() -> AudioScheduleRequest {
        AudioScheduleRequest {
            clip_start: 10.0,
            clip_duration: 2.0,
            source_start: 4.0,
            playback_rate: 1.0,
            buffer_start: 4.0,
            buffer_duration: 3.0,
            requested_start: 10.0,
            context_offset: 90.0,
            context_now: 99.0,
        }
    }

    #[test]
    fn test_audio_schedule_clips_buffer_at_trimmed_end() {
        let window = plan_audio_buffer_schedule(schedule_request())
            .unwrap()
            .unwrap();
        assert_eq!(
            window,
            AudioScheduleWindow {
                context_start: 100.0,
                source_offset: 0.0,
                source_duration: 2.0,
                timeline_start: 10.0,
            }
        );
    }

    #[test]
    fn test_audio_schedule_scales_late_offsets_and_duration_by_rate() {
        for rate in [0.5, 2.0] {
            let window = plan_audio_buffer_schedule(AudioScheduleRequest {
                playback_rate: rate,
                buffer_duration: 10.0,
                context_now: 100.5,
                ..schedule_request()
            })
            .unwrap()
            .unwrap();
            assert_eq!(window.context_start, 100.5);
            assert_eq!(window.timeline_start, 10.5);
            assert_eq!(window.source_offset, 0.5 * rate);
            assert_eq!(window.source_duration, 1.5 * rate);
        }
    }

    #[test]
    fn test_audio_schedule_intersects_seek_start_and_source_trim() {
        let window = plan_audio_buffer_schedule(AudioScheduleRequest {
            buffer_start: 3.0,
            buffer_duration: 4.0,
            requested_start: 11.0,
            ..schedule_request()
        })
        .unwrap()
        .unwrap();
        assert_eq!(window.context_start, 101.0);
        assert_eq!(window.source_offset, 2.0);
        assert_eq!(window.source_duration, 1.0);
    }

    #[test]
    fn test_audio_schedule_does_not_play_outside_half_open_clip() {
        for request in [
            AudioScheduleRequest {
                buffer_start: 6.0,
                ..schedule_request()
            },
            AudioScheduleRequest {
                buffer_start: 1.0,
                ..schedule_request()
            },
            AudioScheduleRequest {
                context_now: 102.0,
                ..schedule_request()
            },
            AudioScheduleRequest {
                requested_start: 12.0,
                ..schedule_request()
            },
            AudioScheduleRequest {
                buffer_duration: 0.0,
                ..schedule_request()
            },
            AudioScheduleRequest {
                clip_duration: 0.0,
                ..schedule_request()
            },
        ] {
            assert_eq!(plan_audio_buffer_schedule(request), Ok(None));
        }
    }

    #[test]
    fn test_audio_schedule_rejects_invalid_and_overflowing_inputs() {
        for request in [
            AudioScheduleRequest {
                playback_rate: 0.0,
                ..schedule_request()
            },
            AudioScheduleRequest {
                playback_rate: f64::NAN,
                ..schedule_request()
            },
            AudioScheduleRequest {
                clip_duration: -1.0,
                ..schedule_request()
            },
            AudioScheduleRequest {
                buffer_duration: f64::INFINITY,
                ..schedule_request()
            },
        ] {
            assert_eq!(
                plan_audio_buffer_schedule(request),
                Err(AudioScheduleError::InvalidInput)
            );
        }
        assert_eq!(
            plan_audio_buffer_schedule(AudioScheduleRequest {
                clip_start: f64::MAX,
                clip_duration: f64::MAX,
                ..schedule_request()
            }),
            Err(AudioScheduleError::NonFiniteRange)
        );
    }

    #[test]
    fn test_gain_ramp() {
        let mut samples = vec![1.0, 1.0, 1.0];
        apply_gain_ramp(&mut samples, 0.0, 1.0);
        assert!((samples[0] - 0.0).abs() < 1e-5);
        assert!((samples[1] - 0.5).abs() < 1e-5);
        assert!((samples[2] - 1.0).abs() < 1e-5);
    }

    #[test]
    fn test_peak_buckets() {
        let samples = vec![0.1, -0.8, 0.3, 0.4, 0.9, -0.2];
        let starts = vec![0, 3];
        let ends = vec![3, 6];
        let mut peaks = vec![0.0, 0.0];
        compute_peak_buckets(&samples, &starts, &ends, &mut peaks);
        assert!((peaks[0] - 0.8).abs() < 1e-5);
        assert!((peaks[1] - 0.9).abs() < 1e-5);
    }

    #[test]
    fn test_sample_peak_buckets_matches_sample_ranges() {
        let amplitudes = vec![0.1, 0.8, 0.3, 0.9, 0.2];
        let starts = vec![0.0, 4.0, 8.0, 3.0, 10.0, 2.0];
        let ends = vec![4.0, 8.0, 10.0, 5.0, 12.0, 2.0];
        let mut peaks = vec![0.0; starts.len()];

        sample_peak_buckets(&amplitudes, 2.0, &starts, &ends, &mut peaks);

        assert_eq!(peaks, vec![0.8, 0.9, 0.2, 0.8, 0.0, 0.0]);
    }

    #[test]
    fn test_sample_peak_buckets_clears_output_for_invalid_bucket_size() {
        let mut peaks = vec![1.0, 1.0];

        sample_peak_buckets(&[0.5], 0.0, &[0.0, 1.0], &[1.0, 2.0], &mut peaks);

        assert_eq!(peaks, vec![0.0, 0.0]);
    }

    #[test]
    fn test_sample_peak_buckets_handles_infinite_sample_bounds() {
        let amplitudes = vec![0.2, 0.8, 0.3];
        let starts = vec![f64::NEG_INFINITY, 0.0];
        let ends = vec![2.0, f64::INFINITY];
        let mut peaks = vec![0.0; starts.len()];

        sample_peak_buckets(&amplitudes, 2.0, &starts, &ends, &mut peaks);

        assert_eq!(peaks, vec![0.2, 0.8]);
    }

    fn beat_test_samples(channel_count: usize) -> Vec<f32> {
        let samples_per_channel = 1_000;
        let mut samples = vec![0.0; samples_per_channel * channel_count];
        let levels = [(5, 0.05), (20, 0.10), (35, 0.90), (50, 0.15), (65, 0.20)];
        for channel in 0..channel_count {
            let channel_offset = channel * samples_per_channel;
            for (window, level) in levels {
                let start = channel_offset + window * 10;
                samples[start..start + 10].fill(level);
            }
        }
        samples
    }

    #[test]
    fn test_detect_audio_beats_matches_onset_threshold_and_gap() {
        let samples = beat_test_samples(1);
        let beats = detect_audio_beats(&samples, 1_000, 1, 500.0, 0.0, 2.0, 120);
        assert_eq!(beats, vec![0.7]);
    }

    #[test]
    fn test_detect_audio_beats_respects_channels_range_and_limit() {
        let samples = beat_test_samples(2);
        let beats = detect_audio_beats(&samples, 1_000, 2, 500.0, 0.5, 1.5, 1);
        assert_eq!(beats, vec![0.7]);
    }

    #[test]
    fn test_detect_audio_beats_returns_empty_for_silence_or_invalid_inputs() {
        let silence = vec![0.0; 1_000];
        assert!(detect_audio_beats(&silence, 1_000, 1, 500.0, 0.0, 2.0, 120).is_empty());
        assert!(detect_audio_beats(&silence, 1_000, 1, 0.0, 0.0, 2.0, 120).is_empty());
        assert!(detect_audio_beats(&silence, 1_000, 2, 500.0, 0.0, 2.0, 120).is_empty());
    }
}

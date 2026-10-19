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
        let s1 = if idx0 + 1 < source.len() { source[idx0 + 1] } else { s0 };
        let interp = s0 + (s1 - s0) * frac;
        out[out_start_sample + i] += interp * gain;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

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
}

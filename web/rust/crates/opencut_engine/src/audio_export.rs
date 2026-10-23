//! PCM serialization for browser WAV export. Planar input, interleaved LE output.
pub fn encode_pcm16(samples: &[f32], channels: usize, frames: usize, output: &mut [u8]) -> bool {
    let Some(count) = channels.checked_mul(frames) else {
        return false;
    };
    if channels == 0
        || channels > 32
        || count != samples.len()
        || count.checked_mul(2) != Some(output.len())
    {
        return false;
    }
    for frame in 0..frames {
        for channel in 0..channels {
            let sample = samples[channel * frames + frame];
            let sample = if sample.is_finite() {
                sample.clamp(-1.0, 1.0)
            } else {
                0.0
            };
            let quantized = (sample * if sample < 0.0 { 32768.0 } else { 32767.0 }).round() as i16;
            let index = (frame * channels + channel) * 2;
            output[index..index + 2].copy_from_slice(&quantized.to_le_bytes());
        }
    }
    true
}

#[no_mangle]
pub unsafe extern "C" fn ocw_audio_encode_pcm16(
    input: *const f32,
    channels: u32,
    frames: u32,
    output: *mut u8,
) -> i32 {
    if channels == 0 || channels > 32 || frames > 65_536 || input.is_null() || output.is_null() {
        return 0;
    }
    let count = channels as usize * frames as usize;
    let samples = std::slice::from_raw_parts(input, count);
    let bytes = std::slice::from_raw_parts_mut(output, count * 2);
    i32::from(encode_pcm16(
        samples,
        channels as usize,
        frames as usize,
        bytes,
    ))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn encodes_interleaved_clipped_signed_pcm_and_sanitizes_nonfinite_samples() {
        let input = [-2.0, 0.0, 2.0, f32::NAN, 1.0, -1.0, 0.5, f32::INFINITY];
        let mut output = [0_u8; 16];
        assert!(encode_pcm16(&input, 2, 4, &mut output));
        let actual: Vec<i16> = output
            .chunks_exact(2)
            .map(|b| i16::from_le_bytes([b[0], b[1]]))
            .collect();
        assert_eq!(actual, [-32768, 32767, 0, -32768, 32767, 16384, 0, 0]);
        assert!(!encode_pcm16(&input, 0, 4, &mut output));
        assert!(!encode_pcm16(&input, 1, 4, &mut output));
    }
}

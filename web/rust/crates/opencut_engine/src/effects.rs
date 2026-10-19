use std::f32::consts::PI;

pub fn apply_color_grading_rgba(
    pixels: &mut [u8],
    width: usize,
    height: usize,
    brightness: f32,
    contrast: f32,
    saturation: f32,
    exposure: f32,
    temperature: f32,
    tint: f32,
    hue: f32,
    gamma: f32,
) {
    let total_pixels = width * height;
    if pixels.len() < total_pixels * 4 {
        return;
    }

    let exp_mult = 2.0f32.powf(exposure);
    let inv_gamma = if gamma > 0.0 { 1.0 / gamma } else { 1.0 };
    let temp_r = 1.0 + temperature * 0.1;
    let temp_b = 1.0 - temperature * 0.1;
    let tint_g = 1.0 + tint * 0.1;

    let hue_rad = hue * PI / 180.0;
    let cos_h = hue_rad.cos();
    let sin_h = hue_rad.sin();

    for chunk in pixels.chunks_exact_mut(4) {
        let mut r = chunk[0] as f32 / 255.0;
        let mut g = chunk[1] as f32 / 255.0;
        let mut b = chunk[2] as f32 / 255.0;

        // Exposure & Brightness
        r = (r * exp_mult) + brightness;
        g = (g * exp_mult) + brightness;
        b = (b * exp_mult) + brightness;

        // Contrast
        r = (r - 0.5) * (1.0 + contrast) + 0.5;
        g = (g - 0.5) * (1.0 + contrast) + 0.5;
        b = (b - 0.5) * (1.0 + contrast) + 0.5;

        // Temperature & Tint
        r *= temp_r;
        g *= tint_g;
        b *= temp_b;

        // Saturation
        let luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
        let sat_factor = 1.0 + saturation;
        r = luma + (r - luma) * sat_factor;
        g = luma + (g - luma) * sat_factor;
        b = luma + (b - luma) * sat_factor;

        // Hue rotation (YIQ approximation)
        if hue.abs() > 0.001 {
            let u = -0.14713 * r - 0.28886 * g + 0.436 * b;
            let v = 0.615 * r - 0.51499 * g - 0.10001 * b;
            let u_rot = u * cos_h - v * sin_h;
            let v_rot = u * sin_h + v * cos_h;
            r = luma + 1.13983 * v_rot;
            g = luma - 0.39465 * u_rot - 0.58060 * v_rot;
            b = luma + 2.03211 * u_rot;
        }

        // Gamma correction
        if (gamma - 1.0).abs() > 0.001 {
            r = r.max(0.0).powf(inv_gamma);
            g = g.max(0.0).powf(inv_gamma);
            b = b.max(0.0).powf(inv_gamma);
        }

        chunk[0] = (r.clamp(0.0, 1.0) * 255.0).round() as u8;
        chunk[1] = (g.clamp(0.0, 1.0) * 255.0).round() as u8;
        chunk[2] = (b.clamp(0.0, 1.0) * 255.0).round() as u8;
    }
}

pub fn apply_gaussian_blur_rgba(
    pixels: &mut [u8],
    width: usize,
    height: usize,
    radius: usize,
    sigma: f32,
) {
    if radius == 0 || width == 0 || height == 0 || pixels.len() < width * height * 4 {
        return;
    }

    let actual_sigma = if sigma > 0.0 { sigma } else { (radius as f32) * 0.5 };
    let size = radius * 2 + 1;
    let mut kernel = Vec::with_capacity(size);
    let mut sum = 0.0f32;

    for i in 0..size {
        let x = (i as i32 - radius as i32) as f32;
        let g = (-0.5 * (x * x) / (actual_sigma * actual_sigma)).exp();
        kernel.push(g);
        sum += g;
    }
    for k in &mut kernel {
        *k /= sum;
    }

    let mut temp = vec![0u8; width * height * 4];

    // Horizontal pass
    for y in 0..height {
        let row_offset = y * width * 4;
        for x in 0..width {
            let mut r = 0.0f32;
            let mut g = 0.0f32;
            let mut b = 0.0f32;
            let mut a = 0.0f32;

            for (k_idx, &weight) in kernel.iter().enumerate() {
                let sample_x = (x as i32 + k_idx as i32 - radius as i32).clamp(0, width as i32 - 1) as usize;
                let offset = row_offset + sample_x * 4;
                r += pixels[offset] as f32 * weight;
                g += pixels[offset + 1] as f32 * weight;
                b += pixels[offset + 2] as f32 * weight;
                a += pixels[offset + 3] as f32 * weight;
            }

            let out_idx = row_offset + x * 4;
            temp[out_idx] = r.round() as u8;
            temp[out_idx + 1] = g.round() as u8;
            temp[out_idx + 2] = b.round() as u8;
            temp[out_idx + 3] = a.round() as u8;
        }
    }

    // Vertical pass
    for y in 0..height {
        for x in 0..width {
            let mut r = 0.0f32;
            let mut g = 0.0f32;
            let mut b = 0.0f32;
            let mut a = 0.0f32;

            for (k_idx, &weight) in kernel.iter().enumerate() {
                let sample_y = (y as i32 + k_idx as i32 - radius as i32).clamp(0, height as i32 - 1) as usize;
                let offset = (sample_y * width + x) * 4;
                r += temp[offset] as f32 * weight;
                g += temp[offset + 1] as f32 * weight;
                b += temp[offset + 2] as f32 * weight;
                a += temp[offset + 3] as f32 * weight;
            }

            let out_idx = (y * width + x) * 4;
            pixels[out_idx] = r.round() as u8;
            pixels[out_idx + 1] = g.round() as u8;
            pixels[out_idx + 2] = b.round() as u8;
            pixels[out_idx + 3] = a.round() as u8;
        }
    }
}

pub fn apply_vignette_rgba(
    pixels: &mut [u8],
    width: usize,
    height: usize,
    amount: f32,
    softness: f32,
    roundness: f32,
) {
    if pixels.len() < width * height * 4 || amount <= 0.001 {
        return;
    }

    let cx = width as f32 * 0.5;
    let cy = height as f32 * 0.5;
    let max_dist = (cx * cx + cy * cy).sqrt();
    let soft = softness.max(0.01);

    for y in 0..height {
        let dy = (y as f32 - cy) * (1.0 + (1.0 - roundness));
        let dy2 = dy * dy;
        let row_offset = y * width * 4;

        for x in 0..width {
            let dx = x as f32 - cx;
            let dist = (dx * dx + dy2).sqrt() / max_dist;
            let falloff = ((dist - (1.0 - soft)) / soft).clamp(0.0, 1.0);
            let factor = 1.0 - (falloff * amount);

            let idx = row_offset + x * 4;
            pixels[idx] = (pixels[idx] as f32 * factor).round() as u8;
            pixels[idx + 1] = (pixels[idx + 1] as f32 * factor).round() as u8;
            pixels[idx + 2] = (pixels[idx + 2] as f32 * factor).round() as u8;
        }
    }
}

pub fn apply_chroma_key_rgba(
    pixels: &mut [u8],
    width: usize,
    height: usize,
    key_r: f32,
    key_g: f32,
    key_b: f32,
    similarity: f32,
    smoothness: f32,
    spill: f32,
) {
    if pixels.len() < width * height * 4 {
        return;
    }

    let sim = similarity.max(0.01);
    let smooth = smoothness.max(0.001);

    for chunk in pixels.chunks_exact_mut(4) {
        let r = chunk[0] as f32 / 255.0;
        let g = chunk[1] as f32 / 255.0;
        let b = chunk[2] as f32 / 255.0;

        let diff_r = r - key_r;
        let diff_g = g - key_g;
        let diff_b = b - key_b;
        let dist = (diff_r * diff_r + diff_g * diff_g + diff_b * diff_b).sqrt();

        let alpha = ((dist - sim) / smooth).clamp(0.0, 1.0);

        // Spill suppression
        if spill > 0.0 && alpha < 1.0 {
            let max_non_key = r.max(b);
            let excess = (g - max_non_key).max(0.0);
            let suppressed_g = g - excess * spill * (1.0 - alpha);
            chunk[1] = (suppressed_g * 255.0).round() as u8;
        }

        chunk[3] = (chunk[3] as f32 * alpha).round() as u8;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_color_grading_brightness() {
        let mut pixels = vec![100, 100, 100, 255];
        apply_color_grading_rgba(&mut pixels, 1, 1, 0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
        assert!(pixels[0] > 100);
    }

    #[test]
    fn test_gaussian_blur() {
        let mut pixels = vec![
            255, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255,
            0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255,
            0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255,
        ];
        apply_gaussian_blur_rgba(&mut pixels, 3, 3, 1, 1.0);
        // Center pixel should have received spread energy
        assert!(pixels[4 * 4] > 0);
    }
}

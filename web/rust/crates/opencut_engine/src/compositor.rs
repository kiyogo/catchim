use std::f32::consts::PI;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BlendMode {
    Normal = 0,
    Multiply = 1,
    Screen = 2,
    Overlay = 3,
    Darken = 4,
    Lighten = 5,
    ColorDodge = 6,
    ColorBurn = 7,
    HardLight = 8,
    SoftLight = 9,
    Difference = 10,
    Exclusion = 11,
}

impl BlendMode {
    pub fn from_u32(val: u32) -> Self {
        match val {
            1 => BlendMode::Multiply,
            2 => BlendMode::Screen,
            3 => BlendMode::Overlay,
            4 => BlendMode::Darken,
            5 => BlendMode::Lighten,
            6 => BlendMode::ColorDodge,
            7 => BlendMode::ColorBurn,
            8 => BlendMode::HardLight,
            9 => BlendMode::SoftLight,
            10 => BlendMode::Difference,
            11 => BlendMode::Exclusion,
            _ => BlendMode::Normal,
        }
    }
}

#[inline]
pub fn blend_channel(base: f32, blend: f32, mode: BlendMode) -> f32 {
    match mode {
        BlendMode::Normal => blend,
        BlendMode::Multiply => base * blend,
        BlendMode::Screen => 1.0 - (1.0 - base) * (1.0 - blend),
        BlendMode::Overlay => {
            if base < 0.5 {
                2.0 * base * blend
            } else {
                1.0 - 2.0 * (1.0 - base) * (1.0 - blend)
            }
        }
        BlendMode::Darken => base.min(blend),
        BlendMode::Lighten => base.max(blend),
        BlendMode::ColorDodge => {
            if blend >= 1.0 {
                1.0
            } else {
                (base / (1.0 - blend)).min(1.0)
            }
        }
        BlendMode::ColorBurn => {
            if blend <= 0.0 {
                0.0
            } else {
                (1.0 - (1.0 - base) / blend).max(0.0)
            }
        }
        BlendMode::HardLight => {
            if blend < 0.5 {
                2.0 * base * blend
            } else {
                1.0 - 2.0 * (1.0 - base) * (1.0 - blend)
            }
        }
        BlendMode::SoftLight => {
            if blend < 0.5 {
                base - (1.0 - 2.0 * blend) * base * (1.0 - base)
            } else {
                let d = if base <= 0.25 {
                    ((16.0 * base - 12.0) * base + 4.0) * base
                } else {
                    base.sqrt()
                };
                base + (2.0 * blend - 1.0) * (d - base)
            }
        }
        BlendMode::Difference => (base - blend).abs(),
        BlendMode::Exclusion => base + blend - 2.0 * base * blend,
    }
}

pub fn clear_buffer_rgba(buf: &mut [u8], width: usize, height: usize, r: f32, g: f32, b: f32, a: f32) {
    let total = width * height * 4;
    if buf.len() < total {
        return;
    }
    let ru = (r.clamp(0.0, 1.0) * 255.0).round() as u8;
    let gu = (g.clamp(0.0, 1.0) * 255.0).round() as u8;
    let bu = (b.clamp(0.0, 1.0) * 255.0).round() as u8;
    let au = (a.clamp(0.0, 1.0) * 255.0).round() as u8;

    for chunk in buf[..total].chunks_exact_mut(4) {
        chunk[0] = ru;
        chunk[1] = gu;
        chunk[2] = bu;
        chunk[3] = au;
    }
}

#[derive(Debug, Clone, Copy)]
pub struct QuadTransform {
    pub center_x: f32,
    pub center_y: f32,
    pub width: f32,
    pub height: f32,
    pub rotation_degrees: f32,
    pub flip_x: bool,
    pub flip_y: bool,
}

pub fn composite_layer_rgba(
    dest: &mut [u8],
    dest_w: usize,
    dest_h: usize,
    src: &[u8],
    src_w: usize,
    src_h: usize,
    transform: QuadTransform,
    opacity: f32,
    blend_mode: BlendMode,
    mask_alpha: Option<&[f32]>,
) {
    if dest.len() < dest_w * dest_h * 4 || src.len() < src_w * src_h * 4 || opacity <= 0.001 {
        return;
    }

    let rad = -transform.rotation_degrees * PI / 180.0;
    let cos_r = rad.cos();
    let sin_r = rad.sin();

    let half_w = transform.width * 0.5;
    let half_h = transform.height * 0.5;
    if half_w <= 0.0 || half_h <= 0.0 {
        return;
    }

    let scale_x = src_w as f32 / transform.width;
    let scale_y = src_h as f32 / transform.height;

    // Bounding box in dest
    let max_extent = (half_w * half_w + half_h * half_h).sqrt();
    let min_x = ((transform.center_x - max_extent).floor() as i32).max(0) as usize;
    let max_x = ((transform.center_x + max_extent).ceil() as i32).min(dest_w as i32) as usize;
    let min_y = ((transform.center_y - max_extent).floor() as i32).max(0) as usize;
    let max_y = ((transform.center_y + max_extent).ceil() as i32).min(dest_h as i32) as usize;

    for y in min_y..max_y {
        let dy = y as f32 - transform.center_y;
        let dest_row_offset = y * dest_w * 4;

        for x in min_x..max_x {
            let dx = x as f32 - transform.center_x;

            // Rotate into layer space
            let mut lx = dx * cos_r - dy * sin_r;
            let mut ly = dx * sin_r + dy * cos_r;

            if transform.flip_x {
                lx = -lx;
            }
            if transform.flip_y {
                ly = -ly;
            }

            if lx < -half_w || lx > half_w || ly < -half_h || ly > half_h {
                continue;
            }

            // Map to source pixel coordinates
            let sx = ((lx + half_w) * scale_x).clamp(0.0, (src_w - 1) as f32) as usize;
            let sy = ((ly + half_h) * scale_y).clamp(0.0, (src_h - 1) as f32) as usize;

            let src_idx = (sy * src_w + sx) * 4;
            let src_r = src[src_idx] as f32 / 255.0;
            let src_g = src[src_idx + 1] as f32 / 255.0;
            let src_b = src[src_idx + 2] as f32 / 255.0;
            let mut src_a = (src[src_idx + 3] as f32 / 255.0) * opacity;

            if let Some(mask) = mask_alpha {
                let mask_idx = y * dest_w + x;
                if mask_idx < mask.len() {
                    src_a *= mask[mask_idx];
                }
            }

            if src_a <= 0.001 {
                continue;
            }

            let dest_idx = dest_row_offset + x * 4;
            let dst_r = dest[dest_idx] as f32 / 255.0;
            let dst_g = dest[dest_idx + 1] as f32 / 255.0;
            let dst_b = dest[dest_idx + 2] as f32 / 255.0;
            let dst_a = dest[dest_idx + 3] as f32 / 255.0;

            // Blend colors
            let blended_r = blend_channel(dst_r, src_r, blend_mode);
            let blended_g = blend_channel(dst_g, src_g, blend_mode);
            let blended_b = blend_channel(dst_b, src_b, blend_mode);

            // Standard alpha over compositing
            let out_a = src_a + dst_a * (1.0 - src_a);
            let out_r = if out_a > 0.0 {
                (blended_r * src_a + dst_r * dst_a * (1.0 - src_a)) / out_a
            } else {
                0.0
            };
            let out_g = if out_a > 0.0 {
                (blended_g * src_a + dst_g * dst_a * (1.0 - src_a)) / out_a
            } else {
                0.0
            };
            let out_b = if out_a > 0.0 {
                (blended_b * src_a + dst_b * dst_a * (1.0 - src_a)) / out_a
            } else {
                0.0
            };

            dest[dest_idx] = (out_r.clamp(0.0, 1.0) * 255.0).round() as u8;
            dest[dest_idx + 1] = (out_g.clamp(0.0, 1.0) * 255.0).round() as u8;
            dest[dest_idx + 2] = (out_b.clamp(0.0, 1.0) * 255.0).round() as u8;
            dest[dest_idx + 3] = (out_a.clamp(0.0, 1.0) * 255.0).round() as u8;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_blend_modes() {
        assert_eq!(blend_channel(0.5, 0.5, BlendMode::Multiply), 0.25);
        assert_eq!(blend_channel(0.5, 0.5, BlendMode::Screen), 0.75);
    }

    #[test]
    fn test_composite_simple() {
        let mut dest = vec![0u8; 10 * 10 * 4];
        let src = vec![255u8; 10 * 10 * 4];
        let t = QuadTransform {
            center_x: 5.0,
            center_y: 5.0,
            width: 10.0,
            height: 10.0,
            rotation_degrees: 0.0,
            flip_x: false,
            flip_y: false,
        };
        composite_layer_rgba(&mut dest, 10, 10, &src, 10, 10, t, 1.0, BlendMode::Normal, None);
        assert_eq!(dest[0], 255);
        assert_eq!(dest[3], 255);
    }
}

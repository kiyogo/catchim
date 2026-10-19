use std::f32::consts::PI;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MaskType {
    Rectangle = 0,
    Ellipse = 1,
    Star = 2,
    Heart = 3,
    Diamond = 4,
    Split = 5,
    CinematicBars = 6,
    Freeform = 7,
}

impl MaskType {
    pub fn from_u32(val: u32) -> Self {
        match val {
            1 => MaskType::Ellipse,
            2 => MaskType::Star,
            3 => MaskType::Heart,
            4 => MaskType::Diamond,
            5 => MaskType::Split,
            6 => MaskType::CinematicBars,
            7 => MaskType::Freeform,
            _ => MaskType::Rectangle,
        }
    }
}

pub fn evaluate_mask_alpha(
    px: f32,
    py: f32,
    mask_type: MaskType,
    cx: f32,
    cy: f32,
    sx: f32,
    sy: f32,
    rot_degrees: f32,
    feather: f32,
    inverted: bool,
) -> f32 {
    let rad = -rot_degrees * PI / 180.0;
    let cos_r = rad.cos();
    let sin_r = rad.sin();

    // Translate to center
    let dx = px - cx;
    let dy = py - cy;

    // Rotate into local mask space
    let lx = dx * cos_r - dy * sin_r;
    let ly = dx * sin_r + dy * cos_r;

    let half_w = (sx * 0.5).max(0.001);
    let half_h = (sy * 0.5).max(0.001);
    let f = feather.max(0.0001);

    let raw_alpha = match mask_type {
        MaskType::Rectangle => {
            let dist_x = lx.abs() - half_w;
            let dist_y = ly.abs() - half_h;
            let dist = dist_x.max(dist_y);
            1.0 - (dist / f).clamp(0.0, 1.0)
        }
        MaskType::Ellipse => {
            let nx = lx / half_w;
            let ny = ly / half_h;
            let dist = (nx * nx + ny * ny).sqrt();
            let boundary_dist = (dist - 1.0) * half_w.min(half_h);
            1.0 - (boundary_dist / f).clamp(0.0, 1.0)
        }
        MaskType::Diamond => {
            let nx = (lx / half_w).abs();
            let ny = (ly / half_h).abs();
            let dist = nx + ny;
            let boundary_dist = (dist - 1.0) * half_w.min(half_h);
            1.0 - (boundary_dist / f).clamp(0.0, 1.0)
        }
        MaskType::Split => {
            // Split line along vertical axis in local space
            let dist = lx;
            (0.5 - (dist / f)).clamp(0.0, 1.0)
        }
        MaskType::CinematicBars => {
            let dist_y = ly.abs() - half_h;
            1.0 - (dist_y / f).clamp(0.0, 1.0)
        }
        MaskType::Heart => {
            let nx = (lx / half_w).abs();
            let ny = ly / half_h;
            // Approximate cardiod heart curve
            let heart_bound = (nx * nx + ny * ny - 1.0).powi(3) - nx * nx * ny * ny * ny;
            if heart_bound <= 0.0 {
                1.0
            } else {
                1.0 - (heart_bound / (f * 0.1)).clamp(0.0, 1.0)
            }
        }
        MaskType::Star => {
            let angle = ly.atan2(lx);
            let dist = (lx * lx + ly * ly).sqrt();
            // 5-point star modulation
            let star_radius = half_w * (0.65 + 0.35 * (angle * 5.0).cos());
            1.0 - ((dist - star_radius) / f).clamp(0.0, 1.0)
        }
        MaskType::Freeform => {
            // Default to rectangular bounds
            let dist_x = lx.abs() - half_w;
            let dist_y = ly.abs() - half_h;
            let dist = dist_x.max(dist_y);
            1.0 - (dist / f).clamp(0.0, 1.0)
        }
    };

    if inverted {
        1.0 - raw_alpha
    } else {
        raw_alpha
    }
}

pub fn apply_mask_to_buffer(
    pixels: &mut [u8],
    width: usize,
    height: usize,
    mask_type: MaskType,
    cx: f32,
    cy: f32,
    sx: f32,
    sy: f32,
    rot: f32,
    feather: f32,
    inverted: bool,
) {
    if pixels.len() < width * height * 4 {
        return;
    }

    for y in 0..height {
        let py = y as f32;
        let row_offset = y * width * 4;
        for x in 0..width {
            let px = x as f32;
            let alpha = evaluate_mask_alpha(px, py, mask_type, cx, cy, sx, sy, rot, feather, inverted);
            let idx = row_offset + x * 4 + 3;
            let orig_a = pixels[idx] as f32;
            pixels[idx] = (orig_a * alpha).round() as u8;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mask_center() {
        let alpha = evaluate_mask_alpha(100.0, 100.0, MaskType::Rectangle, 100.0, 100.0, 50.0, 50.0, 0.0, 1.0, false);
        assert_eq!(alpha, 1.0);

        let alpha_outside = evaluate_mask_alpha(200.0, 200.0, MaskType::Rectangle, 100.0, 100.0, 50.0, 50.0, 0.0, 1.0, false);
        assert_eq!(alpha_outside, 0.0);
    }
}

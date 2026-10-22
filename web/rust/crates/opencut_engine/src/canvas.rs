use crate::time::{format_timecode, FrameRate, TimecodeFormat, TICKS_PER_SECOND};

#[derive(Debug, Clone, Copy)]
pub struct RectF {
    pub x: f32,
    pub y: f32,
    pub w: f32,
    pub h: f32,
}

#[derive(Debug, Clone, Copy)]
pub struct ColorRgba {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}

#[derive(Debug, Clone)]
pub struct CanvasClipRenderItem {
    pub clip_id: String,
    pub rect: RectF,
    pub title: String,
    pub color: ColorRgba,
    pub is_selected: bool,
    pub is_muted: bool,
    pub has_keyframes: bool,
}

#[derive(Debug, Clone)]
pub struct CanvasRulerTickItem {
    pub x: f32,
    pub height: f32,
    pub label: Option<String>,
}

#[derive(Debug, Clone)]
pub struct TimelineCanvasFrame {
    pub width: f32,
    pub height: f32,
    pub ticks: Vec<CanvasRulerTickItem>,
    pub clips: Vec<CanvasClipRenderItem>,
    pub playhead_x: f32,
}

pub fn select_rect_intersections(
    rectangles: &[f64],
    selection: &[f64],
    out_indices: &mut [i32],
) -> usize {
    if selection.len() < 4 || rectangles.len() % 4 != 0 {
        return 0;
    }

    let selection_left = selection[0];
    let selection_top = selection[1];
    let selection_right = selection[2];
    let selection_bottom = selection[3];
    let mut selected_count = 0;

    for (index, rectangle) in rectangles.chunks_exact(4).enumerate() {
        let intersects = !(rectangle[2] < selection_left
            || rectangle[0] > selection_right
            || rectangle[3] < selection_top
            || rectangle[1] > selection_bottom);
        if intersects && selected_count < out_indices.len() {
            let Ok(index) = i32::try_from(index) else {
                break;
            };
            out_indices[selected_count] = index;
            selected_count += 1;
        }
    }

    selected_count
}

pub struct TimelineCanvasLayoutEngine {
    pub base_pixels_per_second: f32,
    pub ruler_height: f32,
    pub track_height: f32,
    pub track_gap: f32,
}

impl Default for TimelineCanvasLayoutEngine {
    fn default() -> Self {
        Self {
            base_pixels_per_second: 50.0,
            ruler_height: 32.0,
            track_height: 48.0,
            track_gap: 4.0,
        }
    }
}

impl TimelineCanvasLayoutEngine {
    pub fn time_to_pixels(&self, ticks: i64, zoom: f32) -> f32 {
        (ticks as f32 / TICKS_PER_SECOND as f32) * (self.base_pixels_per_second * zoom)
    }

    pub fn compute_ruler_ticks(
        &self,
        duration_ticks: i64,
        zoom: f32,
        scroll_left: f32,
        viewport_w: f32,
        fps: FrameRate,
    ) -> Vec<CanvasRulerTickItem> {
        let pps = self.base_pixels_per_second * zoom;
        if pps <= 0.0 {
            return Vec::new();
        }

        // Determine tick interval based on zoom
        let (tick_interval_sec, label_interval_sec) = if pps < 10.0 {
            (10.0, 30.0)
        } else if pps < 30.0 {
            (5.0, 10.0)
        } else if pps < 80.0 {
            (1.0, 5.0)
        } else if pps < 200.0 {
            (0.5, 1.0)
        } else {
            (0.1, 0.5)
        };

        let start_sec = ((scroll_left - 100.0).max(0.0) / pps) as f64;
        let end_sec = ((scroll_left + viewport_w + 100.0) / pps) as f64;
        let total_duration_sec = (duration_ticks as f64 / TICKS_PER_SECOND as f64).max(end_sec);

        let mut ticks = Vec::new();
        let mut cur_sec = (start_sec / tick_interval_sec).floor() * tick_interval_sec;

        while cur_sec <= end_sec && cur_sec <= total_duration_sec {
            let x = (cur_sec as f32) * pps;
            let is_label = (cur_sec % label_interval_sec).abs() < 1e-4;
            let tick_h = if is_label { 14.0 } else { 7.0 };
            let label = if is_label {
                let ticks_val = (cur_sec * TICKS_PER_SECOND as f64).round() as i64;
                Some(format_timecode(ticks_val, TimecodeFormat::MmSs, fps))
            } else {
                None
            };

            ticks.push(CanvasRulerTickItem {
                x,
                height: tick_h,
                label,
            });

            cur_sec += tick_interval_sec;
        }

        ticks
    }

    pub fn compute_clip_rect(
        &self,
        start_ticks: i64,
        duration_ticks: i64,
        zoom: f32,
        track_index: usize,
    ) -> RectF {
        let x = self.time_to_pixels(start_ticks, zoom);
        let w = self.time_to_pixels(duration_ticks, zoom);
        let y = self.ruler_height + (track_index as f32) * (self.track_height + self.track_gap);
        RectF {
            x,
            y,
            w,
            h: self.track_height,
        }
    }

    pub fn compute_trim_handles(&self, clip_rect: RectF, handle_width: f32) -> (RectF, RectF) {
        let hw = handle_width.min(clip_rect.w * 0.4);
        let left = RectF {
            x: clip_rect.x,
            y: clip_rect.y,
            w: hw,
            h: clip_rect.h,
        };
        let right = RectF {
            x: clip_rect.x + clip_rect.w - hw,
            y: clip_rect.y,
            w: hw,
            h: clip_rect.h,
        };
        (left, right)
    }

    pub fn hit_test_clip(
        &self,
        px: f32,
        py: f32,
        clip_rect: RectF,
        handle_width: f32,
    ) -> TimelineHitZone {
        if px < clip_rect.x
            || px > clip_rect.x + clip_rect.w
            || py < clip_rect.y
            || py > clip_rect.y + clip_rect.h
        {
            return TimelineHitZone::None;
        }
        let (left, right) = self.compute_trim_handles(clip_rect, handle_width);
        if px <= left.x + left.w {
            TimelineHitZone::LeftTrimHandle
        } else if px >= right.x {
            TimelineHitZone::RightTrimHandle
        } else {
            TimelineHitZone::ClipBody
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TimelineHitZone {
    None = 0,
    ClipBody = 1,
    LeftTrimHandle = 2,
    RightTrimHandle = 3,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_canvas_layout_ruler() {
        let engine = TimelineCanvasLayoutEngine::default();
        let ticks = engine.compute_ruler_ticks(120_000 * 60, 1.0, 0.0, 1000.0, FrameRate::FPS_30);
        assert!(!ticks.is_empty());
        assert!(ticks[0].x >= 0.0);
    }

    #[test]
    fn test_clip_geometry_and_hit_test() {
        let engine = TimelineCanvasLayoutEngine::default();
        let rect = engine.compute_clip_rect(0, 120_000 * 2, 1.0, 0);
        assert_eq!(rect.x, 0.0);
        assert_eq!(rect.w, 100.0);
        assert_eq!(rect.y, 32.0);

        let hit_body = engine.hit_test_clip(50.0, 40.0, rect, 8.0);
        assert_eq!(hit_body, TimelineHitZone::ClipBody);

        let hit_left = engine.hit_test_clip(4.0, 40.0, rect, 8.0);
        assert_eq!(hit_left, TimelineHitZone::LeftTrimHandle);

        let hit_right = engine.hit_test_clip(96.0, 40.0, rect, 8.0);
        assert_eq!(hit_right, TimelineHitZone::RightTrimHandle);

        let miss = engine.hit_test_clip(150.0, 40.0, rect, 8.0);
        assert_eq!(miss, TimelineHitZone::None);
    }

    #[test]
    fn test_selection_rect_intersections_keep_touching_edges_and_order() {
        let rectangles = [
            0.0, 0.0, 10.0, 10.0, // inside
            10.0, 3.0, 20.0, 8.0, // shares the selection's right edge
            10.5, 0.0, 12.0, 2.0, // just outside
        ];
        let selection = [0.0, 0.0, 10.0, 10.0];
        let mut indices = [0; 3];

        let count = select_rect_intersections(&rectangles, &selection, &mut indices);

        assert_eq!(count, 2);
        assert_eq!(&indices[..count], &[0, 1]);
    }
}

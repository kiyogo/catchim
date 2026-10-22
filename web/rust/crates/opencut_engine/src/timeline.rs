use std::collections::HashMap;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TrackType {
    Overlay = 0,
    Main = 1,
    Audio = 2,
    Graphic = 3,
    Effect = 4,
}

#[derive(Debug, Clone)]
pub struct Clip {
    pub id: String,
    pub start_time: i64,
    pub duration: i64,
    pub trim_start: i64,
    pub trim_end: i64,
}

#[derive(Debug, Clone)]
pub struct Track {
    pub id: String,
    pub track_type: TrackType,
    pub clips: Vec<Clip>,
    pub muted: bool,
    pub hidden: bool,
}

impl Track {
    pub fn new(id: String, track_type: TrackType) -> Self {
        Self {
            id,
            track_type,
            clips: Vec::new(),
            muted: false,
            hidden: false,
        }
    }

    pub fn insert_clip(&mut self, clip: Clip) -> bool {
        // Ensure no overlap
        if !self.can_place(clip.start_time, clip.duration, &clip.id) {
            return false;
        }
        self.clips.push(clip);
        self.clips.sort_by_key(|c| c.start_time);
        true
    }

    pub fn remove_clip(&mut self, clip_id: &str) -> Option<Clip> {
        if let Some(pos) = self.clips.iter().position(|c| c.id == clip_id) {
            Some(self.clips.remove(pos))
        } else {
            None
        }
    }

    pub fn can_place(&self, start: i64, duration: i64, exclude_id: &str) -> bool {
        let end = start + duration;
        for c in &self.clips {
            if !exclude_id.is_empty() && c.id == exclude_id {
                continue;
            }
            let c_end = c.start_time + c.duration;
            if start < c_end && end > c.start_time {
                return false;
            }
        }
        true
    }

    pub fn find_gap(&self, duration: i64, min_start: i64) -> i64 {
        let mut cursor = min_start;
        for c in &self.clips {
            let c_end = c.start_time + c.duration;
            if c.start_time >= cursor + duration {
                return cursor;
            }
            if c_end > cursor {
                cursor = c_end;
            }
        }
        cursor
    }

    pub fn apply_ripple(&mut self, after_time: i64, delta_ticks: i64) {
        for c in &mut self.clips {
            if c.start_time >= after_time {
                c.start_time += delta_ticks;
            }
        }
    }
}

pub struct Timeline {
    pub tracks: HashMap<String, Track>,
    pub track_order: Vec<String>,
}

impl Default for Timeline {
    fn default() -> Self {
        Self::new()
    }
}

impl Timeline {
    pub fn new() -> Self {
        Self {
            tracks: HashMap::new(),
            track_order: Vec::new(),
        }
    }

    pub fn add_track(&mut self, track_id: String, track_type: TrackType) {
        if !self.tracks.contains_key(&track_id) {
            self.tracks
                .insert(track_id.clone(), Track::new(track_id.clone(), track_type));
            self.track_order.push(track_id);
        }
    }

    pub fn can_place(&self, track_id: &str, start: i64, duration: i64, exclude_id: &str) -> bool {
        if let Some(track) = self.tracks.get(track_id) {
            track.can_place(start, duration, exclude_id)
        } else {
            false
        }
    }

    pub fn insert_clip(&mut self, track_id: &str, clip: Clip) -> bool {
        if let Some(track) = self.tracks.get_mut(track_id) {
            track.insert_clip(clip)
        } else {
            false
        }
    }

    pub fn total_duration(&self) -> i64 {
        let mut max_end = 0;
        for track in self.tracks.values() {
            for clip in &track.clips {
                let end = clip.start_time + clip.duration;
                if end > max_end {
                    max_end = end;
                }
            }
        }
        max_end
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct SnapResult {
    pub snapped_time: i64,
    pub matched_index: i32,
    pub distance: i64,
}

pub fn snap_points_sorted(target: i64, points: &[i64], threshold: f64) -> SnapResult {
    if points.is_empty() {
        return SnapResult {
            snapped_time: target,
            matched_index: -1,
            distance: i64::MAX,
        };
    }

    let lower_bound = |value: i64| {
        let mut low = 0;
        let mut high = points.len();
        while low < high {
            let mid = low + (high - low) / 2;
            if points[mid] < value {
                low = mid + 1;
            } else {
                high = mid;
            }
        }
        low
    };
    let first_candidate = lower_bound((target as f64 - threshold).floor() as i64);
    let last_candidate_time = (target as f64 + threshold).ceil() as i64;

    let mut best_dist = u64::MAX;
    let mut best_val = target;
    let mut best_idx = -1;

    for (i, &p) in points.iter().enumerate().skip(first_candidate) {
        if p > last_candidate_time {
            break;
        }
        let dist = target.abs_diff(p);
        if dist as f64 <= threshold && dist < best_dist {
            best_dist = dist;
            best_val = p;
            best_idx = i as i32;
        }
    }

    SnapResult {
        snapped_time: best_val,
        matched_index: best_idx,
        distance: best_dist.min(i64::MAX as u64) as i64,
    }
}

pub fn snap_points_linear(target: i64, points: &[i64], threshold: i64) -> SnapResult {
    let mut best_dist = i64::MAX;
    let mut best_val = target;
    let mut best_idx = -1;

    for (i, &p) in points.iter().enumerate() {
        let dist = (target - p).abs();
        if dist <= threshold && dist < best_dist {
            best_dist = dist;
            best_val = p;
            best_idx = i as i32;
        }
    }

    SnapResult {
        snapped_time: best_val,
        matched_index: best_idx,
        distance: best_dist,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_track_placement() {
        let mut track = Track::new("t1".into(), TrackType::Main);
        assert!(track.insert_clip(Clip {
            id: "c1".into(),
            start_time: 0,
            duration: 100,
            trim_start: 0,
            trim_end: 0,
        }));
        assert!(!track.can_place(50, 50, ""));
        assert!(track.can_place(50, 50, "c1"));
        assert!(track.can_place(100, 50, ""));
    }

    #[test]
    fn test_snapping() {
        let points = vec![0, 100, 250, 500];
        let res = snap_points_sorted(98, &points, 5.0);
        assert_eq!(res.snapped_time, 100);
        assert_eq!(res.matched_index, 1);

        let res2 = snap_points_sorted(90, &points, 5.0);
        assert_eq!(res2.snapped_time, 90);
        assert_eq!(res2.matched_index, -1);
    }

    #[test]
    fn test_sorted_snapping_preserves_first_tie_and_duplicate() {
        let points = vec![90, 100, 100, 110];
        let tie = snap_points_sorted(105, &points, 10.0);
        assert_eq!(tie.snapped_time, 100);
        assert_eq!(tie.matched_index, 1);

        let duplicate = snap_points_sorted(100, &points, 0.0);
        assert_eq!(duplicate.snapped_time, 100);
        assert_eq!(duplicate.matched_index, 1);
    }

    #[test]
    fn test_sorted_snapping_keeps_fractional_threshold() {
        let points = vec![100];
        let res = snap_points_sorted(103, &points, 3.5);
        assert_eq!(res.matched_index, 0);

        let outside = snap_points_sorted(104, &points, 3.5);
        assert_eq!(outside.matched_index, -1);
    }
}

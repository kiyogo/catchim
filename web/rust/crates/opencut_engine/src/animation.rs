#[derive(Debug, Clone, Copy, PartialEq)]
pub struct BezierPoint {
    pub t: f64,
    pub v: f64,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct BezierHandle {
    pub dt: f64,
    pub dv: f64,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum SegmentType {
    Linear,
    Step,
    Bezier,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum ExtrapolationMode {
    Hold,
    Linear,
}

#[derive(Debug, Clone)]
pub struct ScalarKeyframe {
    pub time: f64,
    pub value: f64,
    pub segment_to_next: SegmentType,
    pub left_handle: Option<BezierHandle>,
    pub right_handle: Option<BezierHandle>,
}

#[derive(Debug, Clone)]
pub struct ScalarChannel {
    pub keys: Vec<ScalarKeyframe>,
    pub extrapolation_before: ExtrapolationMode,
    pub extrapolation_after: ExtrapolationMode,
}

pub fn evaluate_bezier_1d(progress: f64, p0: f64, p1: f64, p2: f64, p3: f64) -> f64 {
    let mt = 1.0 - progress;
    let mt2 = mt * mt;
    let mt3 = mt2 * mt;
    let t2 = progress * progress;
    let t3 = t2 * progress;

    mt3 * p0 + 3.0 * mt2 * progress * p1 + 3.0 * mt * t2 * p2 + t3 * p3
}

pub fn solve_bezier_t_for_x(time: f64, p0: f64, p1: f64, p2: f64, p3: f64) -> f64 {
    if time <= p0 {
        return 0.0;
    }
    if time >= p3 {
        return 1.0;
    }

    let mut lower = 0.0;
    let mut upper = 1.0;

    for _ in 0..24 {
        let mid = (lower + upper) * 0.5;
        let estimate = evaluate_bezier_1d(mid, p0, p1, p2, p3);
        if estimate < time {
            lower = mid;
        } else {
            upper = mid;
        }
    }

    (lower + upper) * 0.5
}

pub fn default_right_handle(left: &ScalarKeyframe, right: &ScalarKeyframe) -> BezierHandle {
    let span = right.time - left.time;
    let delta = right.value - left.value;
    BezierHandle {
        dt: span / 3.0,
        dv: delta / 3.0,
    }
}

pub fn default_left_handle(left: &ScalarKeyframe, right: &ScalarKeyframe) -> BezierHandle {
    let span = right.time - left.time;
    let delta = right.value - left.value;
    BezierHandle {
        dt: -span / 3.0,
        dv: -delta / 3.0,
    }
}

impl ScalarChannel {
    pub fn evaluate(&self, time: f64, fallback: f64) -> f64 {
        if self.keys.is_empty() {
            return fallback;
        }

        let first = &self.keys[0];
        let last = &self.keys[self.keys.len() - 1];

        if time <= first.time {
            if time < first.time && self.extrapolation_before == ExtrapolationMode::Linear && self.keys.len() > 1 {
                let second = &self.keys[1];
                let span = second.time - first.time;
                if span > 0.0 {
                    return first.value + ((time - first.time) / span) * (second.value - first.value);
                }
            }
            return first.value;
        }

        if time >= last.time {
            if time > last.time && self.extrapolation_after == ExtrapolationMode::Linear && self.keys.len() > 1 {
                let second_last = &self.keys[self.keys.len() - 2];
                let span = last.time - second_last.time;
                if span > 0.0 {
                    return last.value + ((time - last.time) / span) * (last.value - second_last.value);
                }
            }
            return last.value;
        }

        // Binary search segment
        let mut low = 0;
        let mut high = self.keys.len() - 1;
        while low + 1 < high {
            let mid = (low + high) / 2;
            if self.keys[mid].time <= time {
                low = mid;
            } else {
                high = mid;
            }
        }

        let left = &self.keys[low];
        let right = &self.keys[high];

        if time == right.time {
            return right.value;
        }

        match left.segment_to_next {
            SegmentType::Step => left.value,
            SegmentType::Linear => {
                let span = right.time - left.time;
                if span <= 0.0 {
                    return right.value;
                }
                let progress = ((time - left.time) / span).clamp(0.0, 1.0);
                left.value + (right.value - left.value) * progress
            }
            SegmentType::Bezier => {
                let right_h = left.right_handle.unwrap_or_else(|| default_right_handle(left, right));
                let left_h = right.left_handle.unwrap_or_else(|| default_left_handle(left, right));

                let p0x = left.time;
                let p1x = left.time + right_h.dt;
                let p2x = right.time + left_h.dt;
                let p3x = right.time;

                let t = solve_bezier_t_for_x(time, p0x, p1x, p2x, p3x);

                let p0y = left.value;
                let p1y = left.value + right_h.dv;
                let p2y = right.value + left_h.dv;
                let p3y = right.value;

                evaluate_bezier_1d(t, p0y, p1y, p2y, p3y)
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_bezier_linear() {
        let t = solve_bezier_t_for_x(0.5, 0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0);
        assert!((t - 0.5).abs() < 1e-4);
        let y = evaluate_bezier_1d(t, 0.0, 10.0 / 3.0, 20.0 / 3.0, 10.0);
        assert!((y - 5.0).abs() < 1e-3);
    }

    #[test]
    fn test_channel_evaluation() {
        let channel = ScalarChannel {
            keys: vec![
                ScalarKeyframe {
                    time: 0.0,
                    value: 0.0,
                    segment_to_next: SegmentType::Linear,
                    left_handle: None,
                    right_handle: None,
                },
                ScalarKeyframe {
                    time: 100.0,
                    value: 200.0,
                    segment_to_next: SegmentType::Linear,
                    left_handle: None,
                    right_handle: None,
                },
            ],
            extrapolation_before: ExtrapolationMode::Hold,
            extrapolation_after: ExtrapolationMode::Hold,
        };

        assert_eq!(channel.evaluate(50.0, 0.0), 100.0);
        assert_eq!(channel.evaluate(-10.0, 0.0), 0.0);
        assert_eq!(channel.evaluate(150.0, 0.0), 200.0);
    }
}

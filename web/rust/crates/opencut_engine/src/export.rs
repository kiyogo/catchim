//! Rational frame scheduling shared by browser exporters.
use crate::time::{FrameRate, TICKS_PER_SECOND};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ExportRange {
    pub start: i64,
    pub end: i64,
}

impl ExportRange {
    pub fn new(timeline_duration: i64, start: i64, end: i64) -> Option<Self> {
        if start < 0 || end <= start || end > timeline_duration {
            return None;
        }
        Some(Self { start, end })
    }

    pub fn duration(self) -> i64 {
        self.end - self.start
    }
}

/// Caller supplies three aligned f64 output slots in WASM memory.
#[no_mangle]
pub unsafe extern "C" fn ocw_export_resolve_range(
    duration: f64,
    start: f64,
    end: f64,
    out: *mut f64,
) -> i32 {
    if out.is_null()
        || [duration, start, end]
            .iter()
            .any(|v| !v.is_finite() || *v < 0.0 || v.fract() != 0.0 || *v > 9_007_199_254_740_991.0)
    {
        return 0;
    }
    let Some(range) = ExportRange::new(duration as i64, start as i64, end as i64) else {
        return 0;
    };
    std::slice::from_raw_parts_mut(out, 3).copy_from_slice(&[
        range.start as f64,
        range.end as f64,
        range.duration() as f64,
    ]);
    1
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ExportFrameSample {
    pub time_ticks: i64,
    pub timestamp_seconds: f64,
    pub duration_seconds: f64,
}

#[derive(Debug, Clone, Copy)]
pub struct ExportTiming {
    duration: i64,
    rate: FrameRate,
    count: u64,
}

impl ExportTiming {
    pub fn new(duration: i64, rate: FrameRate) -> Option<Self> {
        if duration < 0 || rate.numerator == 0 || rate.denominator == 0 {
            return None;
        }
        let numerator = i128::from(duration) * i128::from(rate.numerator);
        let denominator = i128::from(TICKS_PER_SECOND) * i128::from(rate.denominator);
        let count = u64::try_from((numerator + denominator - 1) / denominator).ok()?;
        Some(Self {
            duration,
            rate,
            count,
        })
    }

    pub fn frame_count(&self) -> u64 {
        self.count
    }

    pub fn sample(&self, index: u64) -> Option<ExportFrameSample> {
        if index >= self.count {
            return None;
        }
        let num = i128::from(self.rate.numerator);
        let den = i128::from(self.rate.denominator);
        let clock = i128::from(TICKS_PER_SECOND);
        let frame_numerator = i128::from(index) * den;
        let tick_numerator = frame_numerator * clock;
        let rounded_ticks = (tick_numerator + num / 2) / num;
        // Quantization must never sample the exclusive timeline end.
        let time_ticks = rounded_ticks.min(i128::from(self.duration - 1)) as i64;
        let remaining = i128::from(self.duration) * num - tick_numerator;
        let duration_numerator = remaining.min(clock * den);
        Some(ExportFrameSample {
            time_ticks,
            timestamp_seconds: frame_numerator as f64 / num as f64,
            duration_seconds: duration_numerator as f64 / (clock * num) as f64,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn validates_exclusive_export_ranges_without_clamping() {
        assert_eq!(
            ExportRange::new(120_000, 30_000, 90_000)
                .unwrap()
                .duration(),
            60_000
        );
        for (duration, start, end) in [
            (0, 0, 0),
            (100, -1, 50),
            (100, 50, 50),
            (100, 60, 50),
            (100, 0, 101),
        ] {
            assert!(ExportRange::new(duration, start, end).is_none());
        }
        assert!(ExportRange::new(i64::MAX, 0, i64::MAX).is_some());
    }

    #[test]
    fn covers_partial_frame_without_extending_the_timeline() {
        let plan = ExportTiming::new(120_001, FrameRate::FPS_30).unwrap();
        assert_eq!(plan.frame_count(), 31);
        let last = plan.sample(30).unwrap();
        assert_eq!(last.time_ticks, 120_000);
        assert_eq!(last.timestamp_seconds, 1.0);
        assert_eq!(last.duration_seconds, 1.0 / 120_000.0);
        assert!(plan.sample(31).is_none());
    }

    #[test]
    fn zero_and_sub_frame_timelines() {
        let empty = ExportTiming::new(0, FrameRate::FPS_30).unwrap();
        assert_eq!(empty.frame_count(), 0);
        assert!(empty.sample(0).is_none());
        let tiny = ExportTiming::new(1, FrameRate::FPS_30).unwrap();
        assert_eq!(tiny.frame_count(), 1);
        assert_eq!(tiny.sample(0).unwrap().duration_seconds, 1.0 / 120_000.0);
    }

    #[test]
    fn fractional_rate_does_not_accumulate_rounded_tick_error() {
        let rate = FrameRate {
            numerator: 12_345,
            denominator: 101,
        };
        let plan = ExportTiming::new(TICKS_PER_SECOND * 3_600, rate).unwrap();
        let last = plan.sample(plan.frame_count() - 1).unwrap();
        let expected = (plan.frame_count() - 1) as f64 * 101.0 / 12_345.0;
        assert_eq!(last.timestamp_seconds, expected);
        assert!(last.time_ticks < TICKS_PER_SECOND * 3_600);
        assert!((last.timestamp_seconds + last.duration_seconds - 3_600.0).abs() < 1e-9);
    }

    #[test]
    fn rounds_sample_inside_exclusive_end() {
        let plan = ExportTiming::new(
            2,
            FrameRate {
                numerator: 80_001,
                denominator: 1,
            },
        )
        .unwrap();
        assert_eq!(plan.frame_count(), 2);
        assert_eq!(plan.sample(1).unwrap().time_ticks, 1);
    }

    #[test]
    fn rejects_invalid_inputs_and_handles_large_duration_without_overflow() {
        assert!(ExportTiming::new(-1, FrameRate::FPS_30).is_none());
        assert!(ExportTiming::new(
            1,
            FrameRate {
                numerator: 0,
                denominator: 1
            }
        )
        .is_none());
        assert!(ExportTiming::new(
            1,
            FrameRate {
                numerator: 30,
                denominator: 0
            }
        )
        .is_none());
        let plan = ExportTiming::new(i64::MAX, FrameRate::FPS_30).unwrap();
        assert!(plan.frame_count() > 0);
        assert!(plan.sample(plan.frame_count() - 1).is_some());
    }
}

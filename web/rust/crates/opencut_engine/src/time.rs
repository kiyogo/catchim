pub const TICKS_PER_SECOND: i64 = 120_000;
pub const TICKS_PER_SECOND_F64: f64 = 120_000.0;
pub const SECONDS_PER_HOUR: i64 = 3_600;
pub const SECONDS_PER_MINUTE: i64 = 60;
pub const CENTISECONDS_PER_SECOND: i64 = 100;
pub const TICKS_PER_CENTISECOND: i64 = TICKS_PER_SECOND / CENTISECONDS_PER_SECOND;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Default)]
#[repr(transparent)]
pub struct MediaTime(pub i64);

impl MediaTime {
    pub const ZERO: MediaTime = MediaTime(0);

    #[inline]
    pub const fn new(ticks: i64) -> Self {
        MediaTime(ticks)
    }

    #[inline]
    pub fn from_seconds(seconds: f64) -> Self {
        if !seconds.is_finite() {
            return Self::ZERO;
        }
        let rounded = (seconds * TICKS_PER_SECOND_F64).round();
        MediaTime(rounded as i64)
    }

    #[inline]
    pub fn to_seconds(self) -> f64 {
        self.0 as f64 / TICKS_PER_SECOND_F64
    }

    #[inline]
    pub fn add(self, other: MediaTime) -> Self {
        MediaTime(self.0 + other.0)
    }

    #[inline]
    pub fn sub(self, other: MediaTime) -> Self {
        MediaTime(self.0 - other.0)
    }

    #[inline]
    pub fn clamp(self, min: MediaTime, max: MediaTime) -> Self {
        if self.0 < min.0 {
            min
        } else if self.0 > max.0 {
            max
        } else {
            self
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct FrameRate {
    pub numerator: u32,
    pub denominator: u32,
}

impl FrameRate {
    pub const FPS_30: FrameRate = FrameRate { numerator: 30, denominator: 1 };
    pub const FPS_60: FrameRate = FrameRate { numerator: 60, denominator: 1 };

    #[inline]
    pub fn ticks_per_frame(&self) -> Option<i64> {
        if self.numerator == 0 || self.denominator == 0 {
            return None;
        }
        let tick_num = TICKS_PER_SECOND * self.denominator as i64;
        let tick_den = self.numerator as i64;
        if tick_num % tick_den != 0 {
            return None;
        }
        Some(tick_num / tick_den)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TimecodeFormat {
    MmSs = 0,
    HhMmSs = 1,
    HhMmSsCs = 2,
    HhMmSsFf = 3,
}

impl TimecodeFormat {
    pub fn from_u32(val: u32) -> Self {
        match val {
            0 => TimecodeFormat::MmSs,
            1 => TimecodeFormat::HhMmSs,
            3 => TimecodeFormat::HhMmSsFf,
            _ => TimecodeFormat::HhMmSsCs,
        }
    }
}

#[inline]
pub fn round_to_frame(time: i64, rate: FrameRate) -> Option<i64> {
    let tpf = rate.ticks_per_frame()?;
    let remainder = time.rem_euclid(tpf);
    let floor = time.div_euclid(tpf);
    let frame = if remainder * 2 >= tpf { floor + 1 } else { floor };
    Some(frame * tpf)
}

#[inline]
pub fn floor_to_frame(time: i64, rate: FrameRate) -> Option<i64> {
    let tpf = rate.ticks_per_frame()?;
    let floor = time.div_euclid(tpf);
    Some(floor * tpf)
}

#[inline]
pub fn last_frame_time(duration: i64, rate: FrameRate) -> Option<i64> {
    if duration <= 0 {
        return Some(0);
    }
    let last_tick = duration - 1;
    floor_to_frame(last_tick, rate)
}

#[inline]
pub fn snapped_seek_time(time: i64, duration: i64, rate: FrameRate) -> Option<i64> {
    let snapped = round_to_frame(time, rate)?;
    Some(snapped.clamp(0, duration))
}

pub fn format_timecode(time: i64, format: TimecodeFormat, rate: FrameRate) -> String {
    let total_ticks = time.max(0);
    let total_seconds = total_ticks / TICKS_PER_SECOND;
    let hour_ticks = SECONDS_PER_HOUR * TICKS_PER_SECOND;
    let minute_ticks = SECONDS_PER_MINUTE * TICKS_PER_SECOND;

    let hours = total_ticks / hour_ticks;
    let minutes = (total_ticks % hour_ticks) / minute_ticks;
    let seconds = total_seconds % SECONDS_PER_MINUTE;
    let second_ticks = total_ticks % TICKS_PER_SECOND;
    let centiseconds = second_ticks / TICKS_PER_CENTISECOND;

    match format {
        TimecodeFormat::MmSs => format!("{:02}:{:02}", minutes, seconds),
        TimecodeFormat::HhMmSs => format!("{:02}:{:02}:{:02}", hours, minutes, seconds),
        TimecodeFormat::HhMmSsCs => {
            format!("{:02}:{:02}:{:02}:{:02}", hours, minutes, seconds, centiseconds)
        }
        TimecodeFormat::HhMmSsFf => {
            let tpf = rate.ticks_per_frame().unwrap_or(4000);
            let frames = second_ticks / tpf;
            format!("{:02}:{:02}:{:02}:{:02}", hours, minutes, seconds, frames)
        }
    }
}

pub fn parse_timecode(s: &str, format: TimecodeFormat, rate: FrameRate) -> Option<i64> {
    let trimmed = s.trim();
    if trimmed.is_empty() {
        return None;
    }
    let parts: Vec<&str> = trimmed.split(':').collect();
    let nums: Vec<i64> = parts.iter().map(|p| p.parse::<i64>().ok()).collect::<Option<Vec<_>>>()?;

    match (format, nums.as_slice()) {
        (TimecodeFormat::MmSs, [minutes, seconds]) => {
            if *seconds >= SECONDS_PER_MINUTE {
                return None;
            }
            Some((minutes * SECONDS_PER_MINUTE + seconds) * TICKS_PER_SECOND)
        }
        (TimecodeFormat::HhMmSs, [hours, minutes, seconds]) => {
            if *minutes >= SECONDS_PER_MINUTE || *seconds >= SECONDS_PER_MINUTE {
                return None;
            }
            Some((hours * SECONDS_PER_HOUR + minutes * SECONDS_PER_MINUTE + seconds) * TICKS_PER_SECOND)
        }
        (TimecodeFormat::HhMmSsCs, [hours, minutes, seconds, centiseconds]) => {
            if *minutes >= SECONDS_PER_MINUTE
                || *seconds >= SECONDS_PER_MINUTE
                || *centiseconds >= CENTISECONDS_PER_SECOND
            {
                return None;
            }
            Some(
                (hours * SECONDS_PER_HOUR + minutes * SECONDS_PER_MINUTE + seconds) * TICKS_PER_SECOND
                    + centiseconds * TICKS_PER_CENTISECOND,
            )
        }
        (TimecodeFormat::HhMmSsFf, [hours, minutes, seconds, frames]) => {
            let tpf = rate.ticks_per_frame()?;
            let bound = (rate.numerator as f64 / rate.denominator as f64).ceil() as i64;
            if *minutes >= SECONDS_PER_MINUTE || *seconds >= SECONDS_PER_MINUTE || *frames >= bound {
                return None;
            }
            Some(
                (hours * SECONDS_PER_HOUR + minutes * SECONDS_PER_MINUTE + seconds) * TICKS_PER_SECOND
                    + frames * tpf,
            )
        }
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_media_time() {
        let t = MediaTime::from_seconds(1.5);
        assert_eq!(t.0, 180_000);
        assert_eq!(t.to_seconds(), 1.5);
    }

    #[test]
    fn test_timecode_roundtrip() {
        let fps = FrameRate::FPS_30;
        let time = 120_000 * 5 + 60_000; // 5.5s
        let formatted = format_timecode(time, TimecodeFormat::HhMmSsCs, fps);
        assert_eq!(formatted, "00:00:05:50");
        let parsed = parse_timecode(&formatted, TimecodeFormat::HhMmSsCs, fps);
        assert_eq!(parsed, Some(time));
    }
}

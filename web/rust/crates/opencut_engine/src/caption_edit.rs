//! Caption timing rules. All persisted timestamps use integer ticks.
const MAX_JS_TICKS: i64 = 9_007_199_254_740_991;

pub fn validate_range(start: i64, end: i64) -> Option<(i64, i64)> {
    if start < 0 || end <= start || end > MAX_JS_TICKS {
        return None;
    }
    Some((start, end.checked_sub(start)?))
}

pub fn offset_ranges(ranges: &[(i64, i64)], offset: i64) -> Option<Vec<(i64, i64)>> {
    ranges
        .iter()
        .map(|&(start, duration)| {
            let end = start.checked_add(duration)?;
            validate_range(start, end)?;
            validate_range(start.checked_add(offset)?, end.checked_add(offset)?)
        })
        .collect()
}

pub fn split_range(start: i64, duration: i64, time: i64) -> Option<(i64, i64)> {
    let end = start.checked_add(duration)?;
    validate_range(start, end)?;
    if time <= start || time >= end {
        return None;
    }
    Some((time - start, end - time))
}

pub fn merge_ranges(first: (i64, i64), second: (i64, i64)) -> Option<(i64, i64)> {
    let first_end = first.0.checked_add(first.1)?;
    let second_end = second.0.checked_add(second.1)?;
    validate_range(first.0, first_end)?;
    validate_range(second.0, second_end)?;
    validate_range(first.0.min(second.0), first_end.max(second_end))
}

fn tick(value: f64) -> Option<i64> {
    (value.is_finite() && value.fract() == 0.0 && value.abs() <= MAX_JS_TICKS as f64)
        .then_some(value as i64)
}

unsafe fn write_pair(result: Option<(i64, i64)>, output: *mut f64) -> i32 {
    if output.is_null() || (output as usize) % std::mem::align_of::<f64>() != 0 {
        return 0;
    }
    let Some((first, second)) = result else {
        return 0;
    };
    output.write(first as f64);
    output.add(1).write(second as f64);
    1
}

/// # Safety
/// `output` must reference two writable aligned f64 values.
#[no_mangle]
pub unsafe extern "C" fn ocw_caption_validate_range(start: f64, end: f64, output: *mut f64) -> i32 {
    write_pair(
        tick(start)
            .zip(tick(end))
            .and_then(|(start, end)| validate_range(start, end)),
        output,
    )
}

/// # Safety
/// `input` and `output` must each reference `count * 2` aligned f64 values.
#[no_mangle]
pub unsafe extern "C" fn ocw_caption_offset_ranges(
    input: *const f64,
    count: usize,
    offset: f64,
    output: *mut f64,
) -> i32 {
    if count == 0 {
        return if tick(offset).is_some() { 1 } else { 0 };
    }
    if count > 1_000_000
        || input.is_null()
        || output.is_null()
        || (input as usize) % std::mem::align_of::<f64>() != 0
        || (output as usize) % std::mem::align_of::<f64>() != 0
    {
        return 0;
    }
    let Some(offset) = tick(offset) else {
        return 0;
    };
    let mut ranges = Vec::with_capacity(count);
    for pair in std::slice::from_raw_parts(input, count * 2).chunks_exact(2) {
        let Some(range) = tick(pair[0]).zip(tick(pair[1])) else {
            return 0;
        };
        ranges.push(range);
    }
    let Some(result) = offset_ranges(&ranges, offset) else {
        return 0;
    };
    for (index, (start, duration)) in result.into_iter().enumerate() {
        output.add(index * 2).write(start as f64);
        output.add(index * 2 + 1).write(duration as f64);
    }
    1
}

/// # Safety
/// `output` must reference two writable aligned f64 values.
#[no_mangle]
pub unsafe extern "C" fn ocw_caption_split_range(
    start: f64,
    duration: f64,
    time: f64,
    output: *mut f64,
) -> i32 {
    write_pair(
        tick(start)
            .zip(tick(duration))
            .zip(tick(time))
            .and_then(|((start, duration), time)| split_range(start, duration, time)),
        output,
    )
}

/// # Safety
/// `output` must reference two writable aligned f64 values.
#[no_mangle]
pub unsafe extern "C" fn ocw_caption_merge_ranges(
    first_start: f64,
    first_duration: f64,
    second_start: f64,
    second_duration: f64,
    output: *mut f64,
) -> i32 {
    write_pair(
        tick(first_start)
            .zip(tick(first_duration))
            .zip(tick(second_start).zip(tick(second_duration)))
            .and_then(|(first, second)| merge_ranges(first, second)),
        output,
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn split_preserves_span_and_rejects_boundary_ticks() {
        assert_eq!(split_range(120, 91, 150), Some((30, 61)));
        assert_eq!(split_range(120, 91, 120), None);
        assert_eq!(split_range(120, 91, 211), None);
    }

    #[test]
    fn offsets_are_atomic_and_preserve_spacing() {
        assert_eq!(
            offset_ranges(&[(10, 3), (20, 4)], -10),
            Some(vec![(0, 3), (10, 4)])
        );
        assert_eq!(offset_ranges(&[(10, 3), (20, 4)], -11), None);
        assert_eq!(offset_ranges(&[(MAX_JS_TICKS - 1, 1)], 1), None);
    }

    #[test]
    fn merge_handles_gaps_overlaps_and_contained_cues() {
        assert_eq!(merge_ranges((10, 5), (20, 5)), Some((10, 15)));
        assert_eq!(merge_ranges((10, 20), (15, 5)), Some((10, 20)));
        assert_eq!(merge_ranges((20, 5), (10, 15)), Some((10, 15)));
    }

    #[test]
    fn rejects_invalid_and_unsafe_ranges() {
        assert_eq!(validate_range(-1, 10), None);
        assert_eq!(validate_range(0, 0), None);
        assert_eq!(validate_range(0, MAX_JS_TICKS + 1), None);
        assert_eq!(offset_ranges(&[(i64::MAX, 1)], 0), None);
        assert_eq!(tick(f64::NAN), None);
        assert_eq!(tick(1.5), None);
    }

    #[test]
    fn abi_failure_does_not_write_partial_offsets() {
        let input = [10.0, 3.0, 20.0, 4.0];
        let mut output = [99.0; 4];
        assert_eq!(
            unsafe { ocw_caption_offset_ranges(input.as_ptr(), 2, -11.0, output.as_mut_ptr()) },
            0
        );
        assert_eq!(output, [99.0; 4]);
    }
}

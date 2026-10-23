/// Close positive gaps between connected groups of clips on one track.
///
/// The first clip's position and all overlap offsets are retained. Results use
/// the original input order so callers can keep clip IDs and track ordering.
pub fn compact_track_gaps(spans: &[(i64, i64)]) -> Option<Vec<i64>> {
    let mut ordered = Vec::with_capacity(spans.len());
    for (index, &(start, duration)) in spans.iter().enumerate() {
        if start < 0 || duration <= 0 {
            return None;
        }
        ordered.push((index, start, start.checked_add(duration)?));
    }
    ordered.sort_by_key(|&(index, start, _)| (start, index));

    let mut starts = vec![0; spans.len()];
    let mut component_end: Option<i64> = None;
    let mut removed_gap = 0_i64;
    for (index, start, end) in ordered {
        if let Some(previous_end) = component_end {
            if start > previous_end {
                removed_gap = removed_gap.checked_add(start - previous_end)?;
            }
        }
        starts[index] = start.checked_sub(removed_gap)?;
        component_end = Some(component_end.map_or(end, |previous| previous.max(end)));
    }
    Some(starts)
}

#[cfg(test)]
mod tests {
    use super::compact_track_gaps;

    #[test]
    fn preserves_the_first_clip_and_returns_starts_in_input_order() {
        let spans = [(30, 5), (10, 5), (20, 5)];
        assert_eq!(compact_track_gaps(&spans), Some(vec![20, 10, 15]));
        assert_eq!(spans, [(30, 5), (10, 5), (20, 5)]);
    }

    #[test]
    fn preserves_overlaps_nested_spans_and_equal_starts() {
        assert_eq!(
            compact_track_gaps(&[(10, 20), (10, 5), (15, 5), (40, 5), (42, 8), (60, 5)]),
            Some(vec![10, 10, 15, 30, 32, 40]),
        );
    }

    #[test]
    fn keeps_touching_clips_and_compacts_only_positive_gaps() {
        assert_eq!(
            compact_track_gaps(&[(5, 5), (10, 5), (25, 5)]),
            Some(vec![5, 10, 15]),
        );
    }

    #[test]
    fn handles_empty_single_clip_and_fractional_frame_tick_values() {
        assert_eq!(compact_track_gaps(&[]), Some(vec![]));
        assert_eq!(compact_track_gaps(&[(123, 1)]), Some(vec![123]));
        assert_eq!(
            compact_track_gaps(&[(1, 4004), (8009, 4004)]),
            Some(vec![1, 4005]),
        );
    }

    #[test]
    fn rejects_negative_starts_nonpositive_duration_and_overflow() {
        for spans in [
            vec![(-1, 5)],
            vec![(0, 0)],
            vec![(0, -1)],
            vec![(i64::MAX, 1)],
        ] {
            assert_eq!(compact_track_gaps(&spans), None);
        }
        assert_eq!(
            compact_track_gaps(&[(i64::MAX - 2, 2)]),
            Some(vec![i64::MAX - 2]),
        );
    }

    #[test]
    fn compaction_is_idempotent() {
        let spans = [(100, 10), (140, 15), (145, 20), (200, 7)];
        let starts = compact_track_gaps(&spans).unwrap();
        let compacted: Vec<_> = starts
            .iter()
            .zip(spans)
            .map(|(&start, (_, duration))| (start, duration))
            .collect();
        assert_eq!(compact_track_gaps(&compacted), Some(starts));
    }
}

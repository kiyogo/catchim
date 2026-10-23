import { getRustTrackGapBindings } from "@/native/rust-engine";

/** Keep the initial offset and overlaps while removing positive interior gaps. */
export function closeTrackGapStarts({
	starts,
	durations,
}: {
	starts: readonly number[];
	durations: readonly number[];
}): number[] | null {
	if (
		starts.length !== durations.length ||
		!starts.every((start, index) =>
			Number.isSafeInteger(start) && start >= 0 &&
			Number.isSafeInteger(durations[index]) && durations[index] > 0 &&
			Number.isSafeInteger(start + durations[index]),
		)
	) {
		return null;
	}
	if (starts.length === 0) return [];

	const bindings = getRustTrackGapBindings();
	if (bindings) {
		try {
			const result = bindings.planTrackGapStarts(
				Float64Array.from(starts),
				Float64Array.from(durations),
			);
			if (
				result?.length === starts.length &&
				result.every((start, index) =>
					Number.isSafeInteger(start) && start >= 0 && start <= starts[index],
				)
			) {
				return result;
			}
		} catch {
			// Older/unavailable WASM uses the compatible path below.
		}
	}

	// Compatibility for projects opened before the new WASM has loaded.
	const ordered = starts.map((start, index) => ({
		index,
		start,
		end: start + durations[index],
	})).sort((left, right) => left.start - right.start || left.index - right.index);
	const result = new Array<number>(starts.length);
	let componentEnd = ordered[0].end;
	let removedGap = 0;
	for (const span of ordered) {
		if (span.start > componentEnd) removedGap += span.start - componentEnd;
		result[span.index] = span.start - removedGap;
		componentEnd = Math.max(componentEnd, span.end);
	}
	return result;
}

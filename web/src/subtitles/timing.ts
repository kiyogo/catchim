import { getCaptionTimingBindings } from "@/native/caption-timing";
import { mediaTime, type MediaTime } from "@/wasm";

export interface CaptionRange {
	startTime: MediaTime;
	duration: MediaTime;
}

function range(startTime: number, endTime: number): CaptionRange | null {
	return Number.isSafeInteger(startTime) &&
		Number.isSafeInteger(endTime) &&
		startTime >= 0 &&
		endTime > startTime
		? {
				startTime: mediaTime({ ticks: startTime }),
				duration: mediaTime({ ticks: endTime - startTime }),
			}
		: null;
}

function pair(value: number[] | null): CaptionRange | null {
	return value && value.length === 2
		? range(value[0], value[0] + value[1])
		: null;
}

/** Compatibility path is only used when the optional Rust capability is absent. */
export function validateCaptionRange({
	startTime,
	endTime,
}: {
	startTime: MediaTime;
	endTime: MediaTime;
}): CaptionRange | null {
	const native = getCaptionTimingBindings();
	return native
		? pair(native.validate(startTime, endTime))
		: range(startTime, endTime);
}

export function offsetCaptionRanges({
	ranges,
	offset,
}: {
	ranges: CaptionRange[];
	offset: MediaTime;
}): CaptionRange[] | null {
	const native = getCaptionTimingBindings();
	if (native) {
		const values = native.offset(ranges, offset);
		if (!values || values.length !== ranges.length * 2) return null;
		const result = ranges.map((_, index) =>
			pair(values.slice(index * 2, index * 2 + 2)),
		);
		return result.every((value) => value !== null)
			? (result as CaptionRange[])
			: null;
	}
	if (!Number.isSafeInteger(offset)) return null;
	const result = ranges.map((value) =>
		range(value.startTime, value.startTime + value.duration)
			? range(
					value.startTime + offset,
					value.startTime + value.duration + offset,
				)
			: null,
	);
	return result.every((value) => value !== null)
		? (result as CaptionRange[])
		: null;
}

export function splitCaptionRange({
	startTime,
	duration,
	time,
}: CaptionRange & { time: MediaTime }): {
	leftDuration: MediaTime;
	rightDuration: MediaTime;
} | null {
	const native = getCaptionTimingBindings();
	const values = native
		? native.split(startTime, duration, time)
		: range(startTime, startTime + duration) &&
			  Number.isSafeInteger(time) &&
			  time > startTime &&
			  time < startTime + duration
			? [time - startTime, startTime + duration - time]
			: null;
	return values &&
		values.length === 2 &&
		values.every((value) => Number.isSafeInteger(value) && value > 0)
		? {
				leftDuration: mediaTime({ ticks: values[0] }),
				rightDuration: mediaTime({ ticks: values[1] }),
			}
		: null;
}

export function mergeCaptionRanges({
	first,
	second,
}: {
	first: CaptionRange;
	second: CaptionRange;
}): CaptionRange | null {
	const native = getCaptionTimingBindings();
	if (native)
		return pair(
			native.merge(
				first.startTime,
				first.duration,
				second.startTime,
				second.duration,
			),
		);
	if (
		!range(first.startTime, first.startTime + first.duration) ||
		!range(second.startTime, second.startTime + second.duration)
	)
		return null;
	return range(
		Math.min(first.startTime, second.startTime),
		Math.max(
			first.startTime + first.duration,
			second.startTime + second.duration,
		),
	);
}

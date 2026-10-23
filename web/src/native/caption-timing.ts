/** Thin C ABI adapter; caption timing rules belong to the Rust engine. */
export interface CaptionTimingBindings {
	validate(start: number, end: number): number[] | null;
	offset(
		ranges: ReadonlyArray<{ startTime: number; duration: number }>,
		offset: number,
	): number[] | null;
	split(start: number, duration: number, time: number): number[] | null;
	merge(
		firstStart: number,
		firstDuration: number,
		secondStart: number,
		secondDuration: number,
	): number[] | null;
}

type AbiExports = Record<string, unknown>;
let bindings: CaptionTimingBindings | null = null;

export function getCaptionTimingBindings(): CaptionTimingBindings | null {
	return bindings;
}

export function initializeCaptionTimingBindings(
	exp: AbiExports,
	memory: WebAssembly.Memory,
): void {
	bindings = createCaptionTimingBindings(exp, memory);
}

export function createCaptionTimingBindings(
	exp: AbiExports,
	memory: WebAssembly.Memory,
): CaptionTimingBindings | null {
	const required = [
		"oc_malloc",
		"oc_free",
		"ocw_caption_validate_range",
		"ocw_caption_offset_ranges",
		"ocw_caption_split_range",
		"ocw_caption_merge_ranges",
	];
	if (required.some((name) => typeof exp[name] !== "function")) return null;
	const call = (name: string, args: number[]): number =>
		(exp[name] as (...args: number[]) => number)(...args);
	const plan = (name: string, args: number[], length = 2): number[] | null => {
		let pointer = 0;
		try {
			pointer = call("oc_malloc", [length * Float64Array.BYTES_PER_ELEMENT]);
			if (!pointer || pointer % 8 !== 0) return null;
			if (call(name, [...args, pointer]) !== 1) return null;
			return Array.from(new Float64Array(memory.buffer, pointer, length));
		} finally {
			if (pointer) call("oc_free", [pointer]);
		}
	};
	return {
		validate: (start, end) => plan("ocw_caption_validate_range", [start, end]),
		split: (start, duration, time) =>
			plan("ocw_caption_split_range", [start, duration, time]),
		merge: (firstStart, firstDuration, secondStart, secondDuration) =>
			plan("ocw_caption_merge_ranges", [
				firstStart,
				firstDuration,
				secondStart,
				secondDuration,
			]),
		offset: (ranges, offset) => {
			if (ranges.length === 0) return Number.isSafeInteger(offset) ? [] : null;
			let input = 0;
			try {
				input = call("oc_malloc", [
					ranges.length * 2 * Float64Array.BYTES_PER_ELEMENT,
				]);
				if (!input || input % 8 !== 0) return null;
				new Float64Array(memory.buffer, input, ranges.length * 2).set(
					ranges.flatMap(({ startTime, duration }) => [startTime, duration]),
				);
				return plan(
					"ocw_caption_offset_ranges",
					[input, ranges.length, offset],
					ranges.length * 2,
				);
			} finally {
				if (input) call("oc_free", [input]);
			}
		},
	};
}

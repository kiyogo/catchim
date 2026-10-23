import { beforeEach, describe, expect, it, vi } from "vitest";
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";
import {
	createCaptionTimingBindings,
	getCaptionTimingBindings,
	initializeCaptionTimingBindings,
} from "@/native/caption-timing";
import { mediaTime } from "@/wasm";
import {
	mergeCaptionRanges,
	offsetCaptionRanges,
	splitCaptionRange,
	validateCaptionRange,
} from "./timing";

const t = (ticks: number) => mediaTime({ ticks });
beforeEach(() =>
	initializeCaptionTimingBindings({}, new WebAssembly.Memory({ initial: 1 })),
);

function plans() {
	return {
		valid: validateCaptionRange({ startTime: t(1), endTime: t(4005) }),
		invalid: validateCaptionRange({ startTime: t(0), endTime: t(0) }),
		split: splitCaptionRange({
			startTime: t(120),
			duration: t(91),
			time: t(150),
		}),
		boundary: splitCaptionRange({
			startTime: t(120),
			duration: t(91),
			time: t(211),
		}),
		offset: offsetCaptionRanges({
			ranges: [
				{ startTime: t(10), duration: t(3) },
				{ startTime: t(20), duration: t(4) },
			],
			offset: t(-10),
		}),
		negative: offsetCaptionRanges({
			ranges: [{ startTime: t(10), duration: t(3) }],
			offset: t(-11),
		}),
		unsafe: offsetCaptionRanges({
			ranges: [{ startTime: t(Number.MAX_SAFE_INTEGER - 1), duration: t(1) }],
			offset: t(1),
		}),
		overlap: mergeCaptionRanges({
			first: { startTime: t(10), duration: t(20) },
			second: { startTime: t(15), duration: t(5) },
		}),
		gap: mergeCaptionRanges({
			first: { startTime: t(10), duration: t(5) },
			second: { startTime: t(20), duration: t(5) },
		}),
	};
}

describe("caption timing Rust boundary", () => {
	it("matches compatibility behavior against the shipped real WASM for split, offset and merge", async () => {
		const fallback = plans();
		const bytes = await readFile(
			resolve(process.cwd(), "public/wasm/opencut_engine.wasm"),
		);
		const { instance } = await WebAssembly.instantiate(bytes, {});
		initializeCaptionTimingBindings(
			instance.exports,
			instance.exports.memory as WebAssembly.Memory,
		);
		expect(getCaptionTimingBindings()).not.toBeNull();
		expect(plans()).toEqual(fallback);
		expect(fallback.split).toEqual({ leftDuration: 30, rightDuration: 61 });
		expect(fallback.overlap).toEqual({ startTime: 10, duration: 20 });
		expect(fallback.gap).toEqual({ startTime: 10, duration: 15 });
		expect(fallback.negative).toBeNull();
		expect(fallback.unsafe).toBeNull();
		const native = getCaptionTimingBindings()!;
		for (const invalid of [NaN, Infinity, 1.5, Number.MAX_SAFE_INTEGER + 1]) {
			expect(native.validate(invalid, 100)).toBeNull();
			expect(native.split(0, 100, invalid)).toBeNull();
			expect(
				native.offset([{ startTime: 0, duration: 1 }], invalid),
			).toBeNull();
		}
	});

	it("does not replace a Rust rejection with fallback success and frees temporary buffers", () => {
		const memory = new WebAssembly.Memory({ initial: 1 });
		let offset = 32;
		const free = vi.fn();
		const exp = {
			oc_malloc: vi.fn((size: number) => {
				const pointer = offset;
				offset += size;
				return pointer;
			}),
			oc_free: free,
			ocw_caption_validate_range: () => 0,
			ocw_caption_split_range: () => 0,
			ocw_caption_merge_ranges: () => 0,
			ocw_caption_offset_ranges: () => 0,
		};
		initializeCaptionTimingBindings(exp, memory);
		expect(
			validateCaptionRange({ startTime: t(0), endTime: t(100) }),
		).toBeNull();
		expect(
			offsetCaptionRanges({
				ranges: [{ startTime: t(10), duration: t(3) }],
				offset: t(1),
			}),
		).toBeNull();
		expect(free).toHaveBeenCalledTimes(3);
		const throwing = createCaptionTimingBindings(
			{
				...exp,
				ocw_caption_validate_range: () => {
					throw new Error("WASM trap");
				},
			},
			memory,
		)!;
		expect(() => throwing.validate(0, 100)).toThrow("WASM trap");
		expect(free).toHaveBeenCalledTimes(4);
	});
});

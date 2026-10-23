import { describe, expect, it, vi } from "vitest";
import {
	createRustExportTimingBindings,
	createRustTrackGapBindings,
	getRustExportTimingBindings,
	getRustTrackGapBindings,
	initRustEngine,
} from "./rust-engine";
import { getExportFrameCount, getExportFrameSample } from "@/services/renderer/frame-count";

describe("Rust editing planner ABI", () => {
	it("uses real WASM and matches exact fallback timing and track gap rules", async () => {
		const cases = [
			{ duration: 0, fps: { numerator: 30, denominator: 1 } },
			{ duration: 1, fps: { numerator: 30, denominator: 1 } },
			{ duration: 120_001, fps: { numerator: 30, denominator: 1 } },
			{ duration: 120_000, fps: { numerator: 30_000, denominator: 1_001 } },
			{ duration: 432_000_000, fps: { numerator: 12_345, denominator: 101 } },
			{ duration: 2, fps: { numerator: 80_001, denominator: 1 } },
			{ duration: Number.MAX_SAFE_INTEGER, fps: { numerator: 60, denominator: 1 } },
		];
		expect(getRustExportTimingBindings()).toBeNull();
		const fallback = cases.map(({ duration, fps }) => {
			const count = getExportFrameCount(duration, fps);
			return { count, first: getExportFrameSample(duration, fps, 0), last: getExportFrameSample(duration, fps, count - 1) };
		});
		await initRustEngine();
		const native = getRustExportTimingBindings();
		const gaps = getRustTrackGapBindings();
		expect(native).not.toBeNull();
		expect(gaps).not.toBeNull();
		for (let index = 0; index < cases.length; index++) {
			const { duration, fps } = cases[index];
			const expected = fallback[index];
			expect(native!.frameCount(duration, fps.numerator, fps.denominator)).toBe(expected.count);
			expect(getExportFrameCount(duration, fps)).toBe(expected.count);
			expect(getExportFrameSample(duration, fps, 0)).toEqual(expected.first);
			expect(getExportFrameSample(duration, fps, expected.count - 1)).toEqual(expected.last);
			expect(getExportFrameSample(duration, fps, expected.count)).toBeNull();
		}
		expect(gaps!.planTrackGapStarts(new Float64Array([20, 5, 9, 30]), new Float64Array([2, 6, 4, 2]))).toEqual([13, 5, 9, 15]);
		for (const duration of [NaN, Infinity, -1, 1.5, Number.MAX_SAFE_INTEGER + 1]) {
			expect(native!.frameCount(duration, 30, 1)).toBeNull();
		}
		expect(native!.frameCount(120_000, 30.5, 1)).toBeNull();
		expect(native!.frameCount(120_000, 30, 0)).toBeNull();
		expect(native!.frameSample(120_000, 30, 1, NaN)).toBeNull();
		expect(gaps!.planTrackGapStarts(new Float64Array([1]), new Float64Array([0]))).toBeNull();
		expect(gaps!.planTrackGapStarts(new Float64Array([1]), new Float64Array())).toBeNull();
	});

	it("frees ABI scratch storage when a sample or gap plan is rejected", () => {
		const free = vi.fn();
		let offset = 32;
		const exp = {
			oc_malloc: vi.fn((size: number) => { const pointer = offset; offset += size; return pointer; }),
			oc_free: free,
			ocw_export_frame_count: () => -1,
			ocw_export_frame_sample: () => -1,
			ocw_timeline_compact_gaps: () => -1,
		};
		const memory = new WebAssembly.Memory({ initial: 1 });
		expect(createRustExportTimingBindings(exp, memory)!.frameSample(120_000, 30, 1, 1)).toBeNull();
		expect(free).toHaveBeenCalledTimes(1);
		expect(createRustTrackGapBindings(exp, memory)!.planTrackGapStarts(new Float64Array([1]), new Float64Array([1]))).toBeNull();
		expect(free).toHaveBeenCalledTimes(4);
	});
});

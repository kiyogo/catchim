import { beforeEach, describe, expect, test, vi } from "vitest";
import { getRustTrackGapBindings } from "@/native/rust-engine";
import { closeTrackGapStarts } from "./close-gaps";

vi.mock("@/native/rust-engine", () => ({
	getRustTrackGapBindings: vi.fn(() => null),
}));

beforeEach(() => {
	vi.mocked(getRustTrackGapBindings).mockReturnValue(null);
});

describe("closeTrackGapStarts", () => {
	test.each([
		{ starts: [], durations: [], expected: [] },
		{ starts: [123], durations: [1], expected: [123] },
		{ starts: [30, 10, 20], durations: [5, 5, 5], expected: [20, 10, 15] },
		{ starts: [10, 10, 15, 40, 42, 60], durations: [20, 5, 5, 5, 8, 5], expected: [10, 10, 15, 30, 32, 40] },
		{ starts: [5, 10, 25], durations: [5, 5, 5], expected: [5, 10, 15] },
		{ starts: [1, 8009], durations: [4004, 4004], expected: [1, 4005] },
	])("compatibility preserves offsets, overlaps, and input order: $starts", ({ starts, durations, expected }) => {
		const originalStarts = [...starts];
		const originalDurations = [...durations];
		expect(closeTrackGapStarts({ starts, durations })).toEqual(expected);
		expect(starts).toEqual(originalStarts);
		expect(durations).toEqual(originalDurations);
	});

	test.each([
		{ starts: [0], durations: [] },
		{ starts: [-1], durations: [1] },
		{ starts: [0], durations: [0] },
		{ starts: [0], durations: [-1] },
		{ starts: [0.5], durations: [1] },
		{ starts: [0], durations: [Number.NaN] },
		{ starts: [Number.POSITIVE_INFINITY], durations: [1] },
		{ starts: [Number.MAX_SAFE_INTEGER], durations: [1] },
	])("rejects invalid timing before calling WASM: $starts / $durations", ({ starts, durations }) => {
		const planTrackGapStarts = vi.fn(() => [0]);
		vi.mocked(getRustTrackGapBindings).mockReturnValue({ planTrackGapStarts });
		expect(closeTrackGapStarts({ starts, durations })).toBeNull();
		expect(planTrackGapStarts).not.toHaveBeenCalled();
	});

	test("uses the available Rust result through typed arrays", () => {
		const result = [20, 10, 15];
		const planTrackGapStarts = vi.fn(() => result);
		vi.mocked(getRustTrackGapBindings).mockReturnValue({ planTrackGapStarts });
		expect(closeTrackGapStarts({ starts: [30, 10, 20], durations: [5, 5, 5] })).toBe(result);
		expect(planTrackGapStarts).toHaveBeenCalledWith(
			new Float64Array([30, 10, 20]),
			new Float64Array([5, 5, 5]),
		);
	});

	test.each([null, [0], [Number.NaN, 10], [-1, 10], [31, 10]])(
		"falls back when WASM is unavailable or returns malformed data: %s",
		(result) => {
			vi.mocked(getRustTrackGapBindings).mockReturnValue({ planTrackGapStarts: () => result });
			expect(closeTrackGapStarts({ starts: [30, 10], durations: [5, 5] })).toEqual([15, 10]);
		},
	);

	test("recovers from a WASM exception using the compatible algorithm", () => {
		vi.mocked(getRustTrackGapBindings).mockReturnValue({
			planTrackGapStarts: () => { throw new Error("WASM memory unavailable"); },
		});
		expect(closeTrackGapStarts({ starts: [30, 10], durations: [5, 5] })).toEqual([15, 10]);
	});
});

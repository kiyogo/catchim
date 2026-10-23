import { describe, expect, test } from "bun:test";
import { getExportFrameCount } from "@/services/renderer/frame-count";

describe("getExportFrameCount", () => {
	test("includes a partial final frame", () => {
		expect(getExportFrameCount(120_001, { numerator: 30, denominator: 1 })).toBe(31);
	});

	test("covers timelines shorter than one frame", () => {
		expect(getExportFrameCount(1, { numerator: 30, denominator: 1 })).toBe(1);
	});

	test("uses rational frame rates without rounded frame durations", () => {
		// A one-second timeline at 30000/1001 fps requires 30 frames.
		expect(getExportFrameCount(120_000, { numerator: 30_000, denominator: 1_001 })).toBe(30);
	});

	test("returns zero for empty or invalid timelines", () => {
		expect(getExportFrameCount(0, { numerator: 30, denominator: 1 })).toBe(0);
		expect(getExportFrameCount(-1, { numerator: 30, denominator: 1 })).toBe(0);
		expect(getExportFrameCount(1, { numerator: 0, denominator: 1 })).toBe(0);
	});
});

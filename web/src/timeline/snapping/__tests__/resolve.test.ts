import { describe, expect, test } from "bun:test";
import { resolveTimelineSnap, type SnapPoint } from "@/timeline/snapping";
import { initRustEngine } from "@/native/rust-engine";
import { mediaTime } from "@/wasm";

function snapPoint(time: number): SnapPoint {
	return {
		time: mediaTime({ ticks: time }),
		type: "element-start",
		elementId: `element-${time}`,
		trackId: "track-1",
	};
}

describe("resolveTimelineSnap", () => {
	test("resolves the closest snap point from sorted input", () => {
		const result = resolveTimelineSnap({
			targetTime: mediaTime({ ticks: 103 }),
			snapPoints: [snapPoint(0), snapPoint(100), snapPoint(200)],
			maxSnapDistance: 5,
		});

		expect(result.snappedTime).toBe(mediaTime({ ticks: 100 }));
		expect(result.snapPoint?.elementId).toBe("element-100");
		expect(result.snapDistance).toBe(3);
	});

	test("falls back correctly for unsorted input", () => {
		const result = resolveTimelineSnap({
			targetTime: mediaTime({ ticks: 198 }),
			snapPoints: [snapPoint(200), snapPoint(0), snapPoint(100)],
			maxSnapDistance: 5,
		});

		expect(result.snappedTime).toBe(mediaTime({ ticks: 200 }));
		expect(result.snapPoint?.elementId).toBe("element-200");
		expect(result.snapDistance).toBe(2);
	});

	test("does not snap outside the threshold", () => {
		const result = resolveTimelineSnap({
			targetTime: mediaTime({ ticks: 90 }),
			snapPoints: [snapPoint(0), snapPoint(100), snapPoint(200)],
			maxSnapDistance: 5,
		});

		expect(result.snappedTime).toBe(mediaTime({ ticks: 90 }));
		expect(result.snapPoint).toBeNull();
	});

	test("uses Rust for sorted input while preserving the first duplicate", async () => {
		const bindings = await initRustEngine();
		expect(bindings?.snapPointsSorted).toBeTypeOf("function");
		if (!bindings) return;

		const firstDuplicate = { ...snapPoint(100), elementId: "first" };
		const secondDuplicate = { ...snapPoint(100), elementId: "second" };
		const result = resolveTimelineSnap({
			targetTime: mediaTime({ ticks: 100 }),
			snapPoints: [firstDuplicate, secondDuplicate],
			maxSnapDistance: 0,
		});

		expect(result.snappedTime).toBe(mediaTime({ ticks: 100 }));
		expect(result.snapPoint?.elementId).toBe("first");
		expect(result.snapDistance).toBe(0);

		const fractionalThresholdResult = resolveTimelineSnap({
			targetTime: mediaTime({ ticks: 103 }),
			snapPoints: [snapPoint(100)],
			maxSnapDistance: 3.5,
		});
		expect(fractionalThresholdResult.snappedTime).toBe(mediaTime({ ticks: 100 }));
	});
});

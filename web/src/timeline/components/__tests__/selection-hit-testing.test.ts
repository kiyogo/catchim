import { describe, expect, test } from "bun:test";
import { initRustEngine } from "@/native/rust-engine";
import type { TimelineTrack } from "@/timeline";
import { mediaTime, TICKS_PER_SECOND } from "@/wasm";
import { resolveTimelineElementIntersections } from "../selection-hit-testing";

function createTrack(): TimelineTrack[] {
	return [
		{
			id: "track-1",
			name: "Video 1",
			type: "video",
			hidden: false,
			muted: false,
			elements: [
				{
				id: "inside",
				startTime: mediaTime({ ticks: 0 }),
				duration: mediaTime({ ticks: TICKS_PER_SECOND }),
			},
				{
					id: "touching-edge",
					startTime: mediaTime({ ticks: TICKS_PER_SECOND * 2 }),
					duration: mediaTime({ ticks: TICKS_PER_SECOND }),
				},
				{
					id: "outside",
					startTime: mediaTime({ ticks: TICKS_PER_SECOND * 2 + 2400 }),
					duration: mediaTime({ ticks: 12000 }),
				},
			],
		},
	] as unknown as TimelineTrack[];
}

describe("resolveTimelineElementIntersections", () => {
	test("uses Rust to preserve inclusive marquee bounds and candidate order", async () => {
		await initRustEngine();
		const bounds = {
			left: 0,
			top: 0,
			right: 500,
			bottom: 500,
			width: 500,
			height: 500,
			x: 0,
			y: 0,
			toJSON: () => ({}),
		};
		const container = {
			getBoundingClientRect: () => bounds,
		} as unknown as HTMLElement;

		const result = resolveTimelineElementIntersections({
			container,
			scrollContainer: null,
			tracks: createTrack(),
			zoomLevel: 1,
			startPos: { x: 0, y: 0 },
			currentPos: { x: 100, y: 40 },
		});

		expect(result).toEqual([
			{ trackId: "track-1", elementId: "inside" },
			{ trackId: "track-1", elementId: "touching-edge" },
		]);
	});
});

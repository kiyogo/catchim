import { describe, expect, test } from "vitest";
import type { SceneTracks, TextElement } from "@/timeline";
import { buildEmptyTrack } from "@/timeline/placement/track-factory";
import { mediaTime } from "@/wasm";
import { computeRippleAdjustments } from "./diff";
import { applyRippleAdjustments } from "./apply";

function textElement(id: string, startTime: number, duration: number): TextElement {
	return {
		id,
		name: id,
		type: "text",
		startTime: mediaTime({ ticks: startTime }),
		duration: mediaTime({ ticks: duration }),
		trimStart: mediaTime({ ticks: 0 }),
		trimEnd: mediaTime({ ticks: 0 }),
		params: { content: id, opacity: 1 },
	};
}

function sceneTracks(elements: TextElement[]): SceneTracks {
	return {
		overlay: [{ ...buildEmptyTrack({ id: "text-track", type: "text" }), elements }],
		main: buildEmptyTrack({ id: "main", type: "video" }),
		audio: [],
	};
}

describe("ripple adjustments", () => {
	test("ripple delete closes only the interval vacated by the deleted clip", () => {
		const before = sceneTracks([
			textElement("left", 0, 100),
			textElement("removed", 100, 50),
			textElement("right", 150, 100),
		]);
		const after = sceneTracks([
			textElement("left", 0, 100),
			textElement("right", 150, 100),
		]);
		const adjustments = computeRippleAdjustments({ beforeTracks: before, afterTracks: after });
		const result = applyRippleAdjustments({ tracks: after, adjustments });
		expect(result.overlay[0].elements.map(({ id, startTime }) => [id, startTime])).toEqual([
			["left", 0],
			["right", 100],
		]);
	});

	test("ripple trim at a clip's leading edge shifts that clip and downstream clips together", () => {
		const before = sceneTracks([
			textElement("trimmed", 0, 100),
			textElement("next", 100, 100),
		]);
		const after = sceneTracks([
			textElement("trimmed", 20, 80),
			textElement("next", 100, 100),
		]);
		const adjustments = computeRippleAdjustments({
			beforeTracks: before,
			afterTracks: after,
			includeLeadingEdges: true,
		});
		const result = applyRippleAdjustments({ tracks: after, adjustments });
		expect(result.overlay[0].elements.map(({ id, startTime }) => [id, startTime])).toEqual([
			["trimmed", 0],
			["next", 80],
		]);
	});
});

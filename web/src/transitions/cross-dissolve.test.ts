import { describe, expect, test } from "vitest";
import type { SceneTracks, TextElement, TimelineTrack } from "@/timeline";
import { buildEmptyTrack } from "@/timeline/placement/track-factory";
import { mediaTime } from "@/wasm";
import { resolveCrossDissolve } from "./cross-dissolve";

function clip(id: string, startTime: number, duration: number): TextElement {
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

function sceneTracks(): SceneTracks {
	return {
		overlay: [
			{ ...buildEmptyTrack({ id: "incoming", type: "text" }), elements: [] },
		],
		main: buildEmptyTrack({ id: "main", type: "video" }),
		audio: [],
	};
}

function trackWithElement(
	track: TimelineTrack,
	element: TextElement,
): TimelineTrack {
	return { ...track, elements: [element] } as TimelineTrack;
}

describe("cross dissolve planning", () => {
	test("fades the upper incoming clip over the shared overlap", () => {
		const tracks = sceneTracks();
		const outgoing = { track: tracks.main, element: clip("out", 0, 1_000) };
		const incoming = {
			track: trackWithElement(tracks.overlay[0], clip("in", 600, 800)),
			element: clip("in", 600, 800),
		};

		expect(resolveCrossDissolve({ selected: [outgoing, incoming], tracks })).toEqual({
			target: { trackId: "incoming", elementId: "in" },
			mode: "in",
			duration: 400,
		});
	});

	test("rejects clips without overlap and layer ordering that hides the incoming clip", () => {
		const tracks = sceneTracks();
		const outgoing = { track: tracks.main, element: clip("out", 0, 500) };
		const incomingElement = clip("in", 600, 500);
		const incoming = {
			track: trackWithElement(tracks.overlay[0], incomingElement),
			element: incomingElement,
		};
		expect(resolveCrossDissolve({ selected: [outgoing, incoming], tracks })).toBeNull();

		const upperOutgoingElement = clip("out", 0, 1_000);
		const upperOutgoingTrack = trackWithElement(tracks.overlay[0], upperOutgoingElement);
		const lowerIncoming = { track: tracks.main, element: clip("in", 600, 800) };
		expect(resolveCrossDissolve({
			selected: [
				{ track: upperOutgoingTrack, element: upperOutgoingElement },
				lowerIncoming,
			],
			tracks,
		})).toBeNull();
	});
});

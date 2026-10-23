import { describe, expect, test } from "vitest";
import type { SceneTracks, VideoElement } from "@/timeline";
import { buildEmptyTrack } from "@/timeline/placement/track-factory";
import { mediaTime } from "@/wasm";
import { buildMoveGroup } from "./build-group";

function video(id: string, startTime: number, linkGroupId?: string): VideoElement {
	return {
		id,
		name: id,
		type: "video",
		mediaId: `media-${id}`,
		startTime: mediaTime({ ticks: startTime }),
		duration: mediaTime({ ticks: 100 }),
		trimStart: mediaTime({ ticks: 0 }),
		trimEnd: mediaTime({ ticks: 0 }),
		params: {},
		...(linkGroupId ? { linkGroupId } : {}),
	};
}

describe("linked clip movement", () => {
	test("dragging one linked clip includes linked peers on other tracks", () => {
		const tracks: SceneTracks = {
			overlay: [
				{ ...buildEmptyTrack({ id: "upper", type: "video" }), elements: [video("anchor", 0, "linked-1")] },
				{ ...buildEmptyTrack({ id: "lower", type: "video" }), elements: [video("peer", 300, "linked-1")] },
			],
			main: buildEmptyTrack({ id: "main", type: "video" }),
			audio: [],
		};

		const group = buildMoveGroup({
			anchorRef: { trackId: "upper", elementId: "anchor" },
			selectedElements: [],
			tracks,
		});

		expect(group?.members.map(({ trackId, elementId }) => [trackId, elementId])).toEqual([
			["upper", "anchor"],
			["lower", "peer"],
		]);
	});
});

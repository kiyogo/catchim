import { describe, expect, test } from "bun:test";
import type { SceneTracks, VideoElement } from "@/timeline";
import type { MoveGroup } from "../types";
import { resolveGroupMove } from "../resolve-move";
import { mediaTime, ZERO_MEDIA_TIME } from "@/wasm";

const element: VideoElement = {
	id: "clip-1",
	type: "video",
	name: "Clip",
	startTime: ZERO_MEDIA_TIME,
	duration: mediaTime({ ticks: 10 }),
	trimStart: ZERO_MEDIA_TIME,
	trimEnd: ZERO_MEDIA_TIME,
	mediaId: "media-1",
	params: {},
};

const tracks: SceneTracks = {
	overlay: [],
	main: {
		id: "main-track",
		type: "video",
		name: "Main",
		muted: false,
		hidden: false,
		elements: [element],
	},
	audio: [],
};

const member: MoveGroup["anchor"] = {
	trackId: tracks.main.id,
	elementId: element.id,
	elementType: "video",
	duration: element.duration,
	timeOffset: ZERO_MEDIA_TIME,
	trackSection: "main",
	sectionIndex: 0,
	displayIndex: 0,
};

describe("resolveGroupMove main-track gap handling", () => {
	test("allows moving the first main-track clip after time zero", () => {
		const result = resolveGroupMove({
			group: { anchor: member, members: [member] },
			tracks,
			anchorStartTime: mediaTime({ ticks: 25 }),
			target: { kind: "existingTrack", anchorTargetTrackId: tracks.main.id },
		});

		expect(result?.moves).toEqual([
			{
				sourceTrackId: tracks.main.id,
				targetTrackId: tracks.main.id,
				elementId: element.id,
				newStartTime: mediaTime({ ticks: 25 }),
			},
		]);
	});

	test("still rejects a move that overlaps a stationary main-track clip", () => {
		const stationary: VideoElement = {
			...element,
			id: "clip-2",
			startTime: mediaTime({ ticks: 25 }),
		};
		const sceneTracks: SceneTracks = {
			...tracks,
			main: { ...tracks.main, elements: [element, stationary] },
		};
		const result = resolveGroupMove({
			group: { anchor: member, members: [member] },
			tracks: sceneTracks,
			anchorStartTime: mediaTime({ ticks: 25 }),
			target: { kind: "existingTrack", anchorTargetTrackId: sceneTracks.main.id },
		});

		expect(result).toBeNull();
	});

	test("rejects moves from a locked source or onto a locked target", () => {
		const lockedSource: SceneTracks = {
			...tracks,
			main: { ...tracks.main, locked: true },
		};
		const sourceResult = resolveGroupMove({
			group: { anchor: member, members: [member] },
			tracks: lockedSource,
			anchorStartTime: mediaTime({ ticks: 20 }),
			target: { kind: "existingTrack", anchorTargetTrackId: tracks.main.id },
		});

		const lockedOverlay = {
			...tracks.main,
			id: "locked-overlay",
			name: "Locked overlay",
			elements: [],
			locked: true,
		};
		const sceneWithLockedTarget: SceneTracks = {
			...tracks,
			overlay: [lockedOverlay],
		};
		const targetResult = resolveGroupMove({
			group: { anchor: member, members: [member] },
			tracks: sceneWithLockedTarget,
			anchorStartTime: mediaTime({ ticks: 20 }),
			target: { kind: "existingTrack", anchorTargetTrackId: lockedOverlay.id },
		});

		expect(sourceResult).toBeNull();
		expect(targetResult).toBeNull();
	});
});

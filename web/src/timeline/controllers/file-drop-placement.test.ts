import { describe, expect, test } from "vitest";
import { mediaTimeFromSeconds } from "@/wasm";
import type { DropTarget } from "@/timeline";
import { preserveEmptyMainTrackStart } from "./file-drop-placement";

const mainTrackTarget: DropTarget = {
	trackIndex: 2,
	isNewTrack: false,
	insertPosition: null,
	xPosition: mediaTimeFromSeconds({ seconds: 0 }),
	targetElement: null,
};

describe("preserveEmptyMainTrackStart", () => {
	test("creates a video track above an empty main track to honor a later drop", () => {
		const target = preserveEmptyMainTrackStart({
			target: mainTrackTarget,
			mainTrackIsEmpty: true,
			mainTrackIndex: 2,
			requestedStartTime: mediaTimeFromSeconds({ seconds: 3 }),
		});

		expect(target).toMatchObject({
			trackIndex: 2,
			isNewTrack: true,
			insertPosition: "above",
			xPosition: mediaTimeFromSeconds({ seconds: 3 }),
		});
	});

	test("keeps a zero-time drop on the main track", () => {
		const target = preserveEmptyMainTrackStart({
			target: mainTrackTarget,
			mainTrackIsEmpty: true,
			mainTrackIndex: 2,
			requestedStartTime: mediaTimeFromSeconds({ seconds: 0 }),
		});

		expect(target).toBe(mainTrackTarget);
	});

	test("does not change placement when the main track already has clips", () => {
		const target = preserveEmptyMainTrackStart({
			target: mainTrackTarget,
			mainTrackIsEmpty: false,
			mainTrackIndex: 2,
			requestedStartTime: mediaTimeFromSeconds({ seconds: 3 }),
		});

		expect(target).toBe(mainTrackTarget);
	});
});

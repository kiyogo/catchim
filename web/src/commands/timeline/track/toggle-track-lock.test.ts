import { beforeEach, describe, expect, test, vi } from "vitest";
import { buildEmptyTrack } from "@/timeline/placement";

const mockState = vi.hoisted(() => ({ tracks: null as unknown }));

vi.mock("@/core", () => ({
	EditorCore: {
		getInstance: () => ({
			scenes: {
				getActiveScene: () => ({ tracks: mockState.tracks }),
			},
			timeline: {
				updateTracks: (tracks: unknown) => {
					mockState.tracks = tracks;
				},
			},
		}),
	},
}));

import { ToggleTrackLockCommand } from "./toggle-track-lock";

describe("ToggleTrackLockCommand", () => {
	beforeEach(() => {
		mockState.tracks = {
			main: buildEmptyTrack({ id: "main", type: "video" }),
			overlay: [],
			audio: [],
		};
	});

	test("toggles persisted model state and undo restores the previous state", () => {
		const command = new ToggleTrackLockCommand("main");
		command.execute();
		expect((mockState.tracks as { main: { locked?: boolean } }).main.locked).toBe(true);

		command.undo();
		expect((mockState.tracks as { main: { locked?: boolean } }).main.locked).toBeUndefined();
	});
});

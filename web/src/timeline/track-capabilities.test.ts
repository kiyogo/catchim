import { describe, expect, test } from "bun:test";
import { buildEmptyTrack } from "@/timeline/placement";
import {
	isTrackLocked,
	preserveLockedTrackContents,
} from "./track-capabilities";
import { mediaTime } from "@/wasm";

describe("track locking", () => {
	test("treats old tracks without a lock field as unlocked", () => {
		const track = buildEmptyTrack({ id: "main", type: "video" });
		expect(isTrackLocked(track)).toBe(false);
	});

	test("preserves content edits while a track stays locked", () => {
		const clip = {
			id: "clip",
			type: "video" as const,
			name: "Clip",
			startTime: mediaTime({ ticks: 0 }),
			duration: mediaTime({ ticks: 10 }),
			trimStart: mediaTime({ ticks: 0 }),
			trimEnd: mediaTime({ ticks: 0 }),
			mediaId: "media",
			params: {},
		};
		const currentTrack = {
			...buildEmptyTrack({ id: "main", type: "video" }),
			locked: true,
			elements: [clip],
		};
		const nextTrack = { ...currentTrack, elements: [] };
		const current = { main: currentTrack, overlay: [], audio: [] };
		const next = { main: nextTrack, overlay: [], audio: [] };

		expect(
			preserveLockedTrackContents({ current, next }).main.elements,
		).toEqual([clip]);
	});

	test("allows content changes as the track is unlocked", () => {
		const currentTrack = {
			...buildEmptyTrack({ id: "main", type: "video" }),
			locked: true,
			elements: [],
		};
		const nextTrack = { ...currentTrack, locked: false };
		const current = { main: currentTrack, overlay: [], audio: [] };
		const next = { main: nextTrack, overlay: [], audio: [] };

		expect(preserveLockedTrackContents({ current, next }).main.locked).toBe(false);
	});

	test("does not unlock old locked content when a command omits the lock field", () => {
		const currentTrack = {
			...buildEmptyTrack({ id: "main", type: "video" }),
			locked: true,
			elements: [],
		};
		const nextTrack = { ...buildEmptyTrack({ id: "main", type: "video" }) };
		const current = { main: currentTrack, overlay: [], audio: [] };
		const next = { main: nextTrack, overlay: [], audio: [] };

		expect(preserveLockedTrackContents({ current, next }).main.locked).toBe(true);
	});
});

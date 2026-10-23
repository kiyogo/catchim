import { describe, expect, test } from "vitest";
import { collectAudibleCandidates } from "@/media/audio";
import type { AudioElement, SceneTracks } from "@/timeline";
import { buildEmptyTrack } from "@/timeline/placement/track-factory";
import { mediaTime } from "@/wasm";

function audioClip(id: string): AudioElement {
	return {
		id,
		name: id,
		type: "audio",
		sourceType: "library",
		sourceUrl: `https://example.invalid/${id}.mp3`,
		startTime: mediaTime({ ticks: 0 }),
		duration: mediaTime({ ticks: 10_000 }),
		trimStart: mediaTime({ ticks: 0 }),
		trimEnd: mediaTime({ ticks: 0 }),
		params: { volume: 0, muted: false },
	};
}

function tracksWithAudioSolo(soloId?: string): SceneTracks {
	const first = { ...buildEmptyTrack({ id: "audio-a", type: "audio" }), elements: [audioClip("clip-a")] };
	const second = { ...buildEmptyTrack({ id: "audio-b", type: "audio" }), elements: [audioClip("clip-b")] };
	return {
		overlay: [],
		main: buildEmptyTrack({ id: "main", type: "video" }),
		audio: [
			{ ...first, solo: soloId === first.id },
			{ ...second, solo: soloId === second.id },
		],
	};
}

describe("audio track solo", () => {
	test("keeps all audible tracks when no solo is active", () => {
		const candidates = collectAudibleCandidates({ tracks: tracksWithAudioSolo(), mediaAssets: [] });
		expect(candidates.map(({ element }) => element.id).sort()).toEqual(["clip-a", "clip-b"]);
	});

	test("isolates the soloed audio track for preview and export collection", () => {
		const candidates = collectAudibleCandidates({ tracks: tracksWithAudioSolo("audio-b"), mediaAssets: [] });
		expect(candidates.map(({ element }) => element.id)).toEqual(["clip-b"]);
	});
});

import { describe, expect, it } from "vitest";
import { getMissingExportSources, getUnavailableExportEffects } from "./preflight";
import type { SceneTracks } from "@/timeline";
import type { MediaAsset } from "@/media/types";
import { mediaTime } from "@/wasm";

const range = {
	start: mediaTime({ ticks: 120_000 }),
	end: mediaTime({ ticks: 240_000 }),
};
function tracks(): SceneTracks {
	return {
		main: {
			id: "main",
			name: "Video",
			type: "video",
			hidden: false,
			muted: false,
			elements: [
				{
					id: "v",
					name: "Missing video",
					type: "video",
					mediaId: "v",
					startTime: 120_000,
					duration: 60_000,
					params: {},
				},
				{
					id: "outside",
					name: "Outside",
					type: "image",
					mediaId: "outside",
					startTime: 0,
					duration: 60_000,
				},
			],
		},
		overlay: [],
		audio: [],
	} as unknown as SceneTracks;
}
const media = [
	{
		id: "v",
		name: "Missing video",
		type: "video",
		missing: true,
		hasAudio: true,
	},
] as MediaAsset[];
describe("export source preflight", () => {
	it("blocks enabled unavailable effects without deleting their data, while respecting visibility and range", () => {
		const scene = tracks();
		const element = scene.main.elements[0];
		element.effects = [{ id: "unknown", type: "missing-plugin", enabled: true, params: { strength: 0.3 } }];
		expect(getUnavailableExportEffects({ tracks: scene, range, width: 1920, height: 1080 })).toEqual(["Missing video: missing-plugin"]);
		expect(element.effects[0].params.strength).toBe(0.3);
		element.effects[0].enabled = false;
		expect(getUnavailableExportEffects({ tracks: scene, range, width: 1920, height: 1080 })).toEqual([]);
	});
	it("blocks missing sources intersecting the range and excludes outside sources", () => {
		expect(
			getMissingExportSources({
				tracks: tracks(),
				mediaAssets: media,
				range,
				includeAudio: true,
				audioOnly: false,
			}),
		).toEqual(["Missing video"]);
	});
	it("uses picture visibility and independent audio mute/source settings", () => {
		const scene = tracks();
		scene.main.hidden = true;
		expect(
			getMissingExportSources({
				tracks: scene,
				mediaAssets: media,
				range,
				includeAudio: false,
				audioOnly: false,
			}),
		).toEqual([]);
		expect(
			getMissingExportSources({
				tracks: scene,
				mediaAssets: media,
				range,
				includeAudio: true,
				audioOnly: true,
			}),
		).toEqual(["Missing video"]);
		scene.main.muted = true;
		expect(
			getMissingExportSources({
				tracks: scene,
				mediaAssets: media,
				range,
				includeAudio: true,
				audioOnly: true,
			}),
		).toEqual([]);
	});
});

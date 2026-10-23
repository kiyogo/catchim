import { describe, expect, it } from "vitest";
import { getMissingExportSources } from "./preflight";
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

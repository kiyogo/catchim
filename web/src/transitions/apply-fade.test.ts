import { describe, expect, test } from "vitest";
import type { AudioElement, TimelineElement, VideoElement } from "@/timeline";
import type { ScalarAnimationChannel } from "@/animation/types";
import { mediaTime } from "@/wasm";
import { applyFadeToElement, getFadePropertyPaths } from "./apply-fade";

const clipDuration = mediaTime({ ticks: 10_000 });
const fadeDuration = mediaTime({ ticks: 1_000 });

function readKeys(element: TimelineElement, path: string) {
	return (element.animations?.[path] as ScalarAnimationChannel | undefined)?.keys ?? [];
}

describe("transition fades", () => {
	test("fades a visual clip from transparent to its existing opacity", () => {
		const visual = {
			id: "visual-1",
			name: "Title",
			type: "text",
			startTime: mediaTime({ ticks: 0 }),
			duration: clipDuration,
			trimStart: mediaTime({ ticks: 0 }),
			trimEnd: mediaTime({ ticks: 0 }),
			params: { opacity: 0.75 },
		} as TimelineElement;

		const faded = applyFadeToElement({ element: visual, duration: fadeDuration, mode: "in" });
		const keys = readKeys(faded, "opacity");
		expect(keys.map(({ time, value }) => [time, value])).toEqual([
			[0, 0],
			[fadeDuration, 0.75],
		]);
	});

	test("fades clip audio and picture together and caps both fades to half a short clip", () => {
		const video = {
			id: "video-1",
			name: "Shot",
			type: "video",
			mediaId: "media-1",
			startTime: mediaTime({ ticks: 0 }),
			duration: mediaTime({ ticks: 1_000 }),
			trimStart: mediaTime({ ticks: 0 }),
			trimEnd: mediaTime({ ticks: 0 }),
			params: { opacity: 1, volume: -6 },
		} as VideoElement;
		const faded = applyFadeToElement({
			element: video,
			duration: fadeDuration,
			mode: "in-out",
		});

		expect(getFadePropertyPaths({ element: video })).toEqual(["opacity", "volume"]);
		expect(readKeys(faded, "opacity").map(({ time, value }) => [time, value])).toEqual([
			[0, 0],
			[500, 1],
			[1_000, 0],
		]);
		expect(readKeys(faded, "volume").map(({ time, value }) => [time, value])).toEqual([
			[0, -60],
			[500, -6],
			[1_000, -60],
		]);
	});

	test("fades audio-only clips using the audio gain channel", () => {
		const audio = {
			id: "audio-1",
			name: "Music",
			type: "audio",
			sourceType: "library",
			sourceUrl: "https://example.invalid/audio.mp3",
			startTime: mediaTime({ ticks: 0 }),
			duration: clipDuration,
			trimStart: mediaTime({ ticks: 0 }),
			trimEnd: mediaTime({ ticks: 0 }),
			params: { volume: -3, muted: false },
		} as AudioElement;
		const faded = applyFadeToElement({ element: audio, duration: fadeDuration, mode: "out" });
		expect(readKeys(faded, "volume").map(({ time, value }) => [time, value])).toEqual([
			[clipDuration - fadeDuration, -3],
			[clipDuration, -60],
		]);
	});
});

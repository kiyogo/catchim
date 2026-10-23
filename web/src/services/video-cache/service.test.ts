import { describe, expect, test, vi } from "vitest";

vi.mock("mediabunny", () => {
	class MockInput {
		constructor(_options: unknown) {}
		getPrimaryVideoTrack = async () => ({ canDecode: async () => true });
		dispose() {}
	}
	class MockCanvasSink {
		constructor(_track: unknown, _options: unknown) {}
		async *canvases(time: number) {
			for (let frameIndex = 0; frameIndex < 2; frameIndex += 1) {
				const timestamp = time + frameIndex / 25;
				yield {
					canvas: { width: 2, height: 2 },
					timestamp,
					duration: 1 / 25,
				};
			}
		}
	}
	return {
		Input: MockInput,
		ALL_FORMATS: [],
		BlobSource: class MockBlobSource {
			constructor(_file: File) {}
		},
		CanvasSink: MockCanvasSink,
	};
});

import { VideoCache } from "./service";

describe("VideoCache concurrent frame requests", () => {
	test("resolves each same-media instance request at its own source time", async () => {
		const cache = new VideoCache();
		const file = new File([new Uint8Array([1])], "same-media.mp4");

		const [pipFrame, backdropFrame] = await Promise.all([
			cache.getFrameAt({ mediaId: "shared-asset", file, time: 2 }),
			cache.getFrameAt({ mediaId: "shared-asset", file, time: 7 }),
		]);

		expect(pipFrame?.timestamp).toBe(2);
		expect(backdropFrame?.timestamp).toBe(7);
		cache.clearAll();
	});
});

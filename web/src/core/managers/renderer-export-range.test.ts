import { beforeEach, describe, expect, it, vi } from "vitest";
import { RendererManager } from "./renderer-manager";
import type { EditorCore } from "@/core";
import { mediaTime } from "@/wasm";
import { createTimelineAudioBuffer } from "@/media/audio";
import { buildScene } from "@/services/renderer/scene-builder";
import { SceneExporter } from "@/services/renderer/scene-exporter";

const state = vi.hoisted(() => ({
	export: vi.fn(async () => new ArrayBuffer(8)),
}));
vi.mock("@/media/audio", () => ({ createTimelineAudioBuffer: vi.fn() }));
vi.mock("@/services/renderer/canvas-renderer", () => ({
	CanvasRenderer: vi.fn(),
}));
vi.mock("@/services/renderer/scene-builder", () => ({
	buildScene: vi.fn(() => ({ duration: 360_000 })),
}));
vi.mock("@/services/renderer/scene-exporter", () => ({
	SceneExporter: vi.fn(
		class {
			on() {}
			export = state.export;
			cancel() {}
		},
	),
}));

function manager() {
	return new RendererManager({
		scenes: {
			getActiveScene: () => ({
				tracks: {
					overlay: [],
					audio: [],
					main: {
						id: "main",
						type: "video",
						hidden: false,
						muted: false,
						elements: [],
					},
				},
			}),
		},
		media: { getAssets: () => [] },
		timeline: { getTotalDuration: () => mediaTime({ ticks: 360_000 }) },
		project: {
			getActive: () => ({
				settings: {
					fps: { numerator: 30, denominator: 1 },
					canvasSize: { width: 16, height: 16 },
					background: { type: "color", color: "#000" },
				},
			}),
		},
	} as unknown as EditorCore);
}
const range = {
	start: mediaTime({ ticks: 120_000 }),
	end: mediaTime({ ticks: 240_000 }),
};
beforeEach(() => {
	vi.clearAllMocks();
	vi.mocked(createTimelineAudioBuffer).mockResolvedValue({
		length: 4,
		duration: 1,
		numberOfChannels: 2,
		sampleRate: 4,
		getChannelData: () => new Float32Array([0, 0.1, -0.1, 0]),
	} as unknown as AudioBuffer);
});
describe("selected-range and audio-only export", () => {
	it("passes absolute range to scene export and crops mixed audio at the same origin", async () => {
		expect(
			(
				await manager().exportProject({
					options: {
						format: "mp4",
						quality: "high",
						includeAudio: true,
						range,
					},
				})
			).success,
		).toBe(true);
		expect(createTimelineAudioBuffer).toHaveBeenCalledWith(
			expect.objectContaining({ duration: 120_000, startTime: 120_000 }),
		);
		expect(state.export).toHaveBeenCalledWith(
			expect.objectContaining({
				range: { start: 120_000, end: 240_000, duration: 120_000 },
			}),
		);
	});
	it("exports WAV without constructing video renderer or scene exporter", async () => {
		const result = await manager().exportProject({
			options: { format: "mp4", quality: "high", audioOnly: true, range },
		});
		expect(result.success).toBe(true);
		expect(new TextDecoder().decode(new Uint8Array(result.buffer!, 0, 4))).toBe(
			"RIFF",
		);
		expect(SceneExporter).not.toHaveBeenCalled();
		expect(buildScene).not.toHaveBeenCalled();
	});
	it("rejects invalid ranges before preparing media", async () => {
		const result = await manager().exportProject({
			options: {
				format: "mp4",
				quality: "high",
				includeAudio: true,
				range: { start: range.end, end: range.start },
			},
		});
		expect(result.success).toBe(false);
		expect(createTimelineAudioBuffer).not.toHaveBeenCalled();
	});
});

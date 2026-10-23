import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import type { EditorCore } from "@/core";
import type { AudioElement, SceneTracks } from "@/timeline";
import type { MediaAsset } from "@/media/types";
import { mediaTime } from "@/wasm";
import { RendererManager } from "./renderer-manager";
import { buildScene } from "@/services/renderer/scene-builder";
import { SceneExporter } from "@/services/renderer/scene-exporter";

vi.mock("@/services/renderer/canvas-renderer", () => ({ CanvasRenderer: vi.fn() }));
vi.mock("@/services/renderer/scene-builder", () => ({ buildScene: vi.fn(() => ({})) }));
vi.mock("@/services/renderer/scene-exporter", () => ({ SceneExporter: vi.fn() }));
vi.mock("@/native/opencut-core", () => ({ getNativeTimelineBindings: () => null }));

function buffer(): AudioBuffer {
	const data = [new Float32Array(44100), new Float32Array(44100)];
	return { sampleRate: 44100, length: 44100, duration: 1, numberOfChannels: 2,
		getChannelData: (channel: number) => data[channel] } as AudioBuffer;
}

describe("export cancellation during audio preparation", () => {
	let contexts: Array<{ close: ReturnType<typeof vi.fn>; decodeAudioData: ReturnType<typeof vi.fn> }>;
	let decode: () => Promise<AudioBuffer>;
	beforeEach(() => {
		vi.useFakeTimers();
		vi.clearAllMocks();
		contexts = [];
		decode = async () => buffer();
		class AudioContext {
			sampleRate = 44100;
			close = vi.fn(async () => {});
			decodeAudioData = vi.fn(() => decode());
			constructor() { contexts.push(this); }
		}
		vi.stubGlobal("window", { AudioContext });
	});
	afterEach(() => { vi.useRealTimers(); vi.unstubAllGlobals(); });

	function manager(source: "upload" | "library" = "upload") {
		const ticks = (n: number) => mediaTime({ ticks: n });
		const clip = {
			id: "audio", name: "audio", type: "audio", sourceType: source,
			...(source === "upload" ? { mediaId: "asset" } : { sourceUrl: "https://example.test/music.mp3" }),
			startTime: ticks(0), duration: ticks(120000), trimStart: ticks(0), trimEnd: ticks(0), params: {},
		} as AudioElement;
		const tracks: SceneTracks = {
			overlay: [], main: { id: "main", name: "main", type: "video", muted: false, hidden: false, elements: [] },
			audio: [{ id: "audio", name: "audio", type: "audio", muted: false, elements: [clip] }],
		};
		const asset = { id: "asset", name: "audio", type: "audio", file: new File([new Uint8Array([1])], "audio.wav") } as MediaAsset;
		return new RendererManager({
			scenes: { getActiveScene: () => ({ tracks }) },
			media: { getAssets: () => [asset] },
			timeline: { getTotalDuration: () => ticks(120000) },
			project: { getActive: () => ({ settings: {
				fps: { numerator: 30, denominator: 1 }, canvasSize: { width: 1920, height: 1080 },
				background: { type: "color", color: "#000000" },
			} }) },
		} as unknown as EditorCore);
	}

	it("cancels before creating an audio context or scene", async () => {
		expect(await manager().exportProject({ options: { format: "mp4", quality: "high", includeAudio: true }, onCancel: () => true }))
			.toEqual({ success: false, cancelled: true });
		expect(contexts).toHaveLength(0);
		expect(buildScene).not.toHaveBeenCalled();
		expect(SceneExporter).not.toHaveBeenCalled();
		expect(vi.getTimerCount()).toBe(0);
	});

	it("returns while browser decode is pending, closes the owned context and discards late output", async () => {
		let finishDecode!: (result: AudioBuffer) => void;
		decode = () => new Promise((resolve) => { finishDecode = resolve; });
		let cancelled = false;
		const error = vi.spyOn(console, "error").mockImplementation(() => {});
		const result = manager().exportProject({ options: { format: "mp4", quality: "high", includeAudio: true }, onCancel: () => cancelled });
		await vi.waitFor(() => expect(contexts[0]?.decodeAudioData).toHaveBeenCalledTimes(1));
		cancelled = true;
		await vi.advanceTimersByTimeAsync(100);
		expect(await result).toEqual({ success: false, cancelled: true });
		expect(contexts[0].close).toHaveBeenCalledTimes(1);
		expect(buildScene).not.toHaveBeenCalled();
		expect(SceneExporter).not.toHaveBeenCalled();
		finishDecode(buffer());
		await Promise.resolve();
		expect(buildScene).not.toHaveBeenCalled();
		expect(error).not.toHaveBeenCalled();
		expect(vi.getTimerCount()).toBe(0);
		error.mockRestore();
	});

	it("propagates the signal to a pending library fetch and never starts decode", async () => {
		let fetchSignal: AbortSignal | undefined;
		vi.stubGlobal("fetch", vi.fn((_url: string, options: { signal?: AbortSignal }) => {
			fetchSignal = options.signal;
			return new Promise<Response>((_resolve, reject) => options.signal?.addEventListener("abort", () => reject(new DOMException("cancelled", "AbortError")), { once: true }));
		}));
		let cancelled = false;
		const result = manager("library").exportProject({ options: { format: "mp4", quality: "high", includeAudio: true }, onCancel: () => cancelled });
		await vi.waitFor(() => expect(fetchSignal).toBeDefined());
		cancelled = true;
		await vi.advanceTimersByTimeAsync(100);
		expect(await result).toEqual({ success: false, cancelled: true });
		expect(fetchSignal?.aborted).toBe(true);
		expect(contexts[0].decodeAudioData).not.toHaveBeenCalled();
		expect(contexts[0].close).toHaveBeenCalledTimes(1);
		expect(buildScene).not.toHaveBeenCalled();
		expect(vi.getTimerCount()).toBe(0);
	});
});

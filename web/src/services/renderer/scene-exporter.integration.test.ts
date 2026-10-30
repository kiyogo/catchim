import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { SceneExporter } from "./scene-exporter";
import type { RootNode } from "./nodes/root-node";

const state = vi.hoisted(() => ({
	frames: [] as number[][],
	gpuFrames: [] as Array<{ timestamp: number; duration: number; format: string }>,
	backend: "canvas" as "canvas" | "webgpu",
	gpuAvailable: false,
	gpuPixelFormat: "bgra8unorm" as "rgba8unorm" | "bgra8unorm",
	gpuTexture: { id: "export-target" },
	render: vi.fn(async (_params: unknown) => {}),
	release: vi.fn(),
	cancel: vi.fn(async () => {}),
	finalize: vi.fn(async () => {}),
}));
vi.mock("@/wasm", () => ({ TICKS_PER_SECOND: 120_000, mediaTime: ({ ticks }: { ticks: number }) => ticks }));
vi.mock("opencut-wasm", () => ({ initializeGpu: async () => {} }));
vi.mock("@/native/rust-engine", () => ({ getRustExportTimingBindings: () => null }));
vi.mock("@/native/webgpu-engine", () => ({
	webGpuEngine: {
		get preferredFormat() { return state.gpuPixelFormat; },
		get device() { return state.gpuAvailable ? {} : null; },
		acquireTexture: vi.fn(() => state.gpuTexture),
		readTexturePixels: vi.fn(async () => new Uint8Array([1, 2, 3, 4])),
		releaseTexture: vi.fn(),
	},
}));
vi.mock("./canvas-renderer", () => ({
	CanvasRenderer: class {
		fps: unknown;
		constructor({ fps }: { fps: unknown }) { this.fps = fps; }
		getOutputCanvas() { return {}; }
		getBackend() { return state.backend; }
		render = state.render;
	},
	beginExportRender: async () => state.release,
}));
vi.mock("mediabunny", () => ({
	Output: class {
		target = { buffer: new ArrayBuffer(8) };
		addVideoTrack() {}
		addAudioTrack() {}
		async start() {}
		cancel = state.cancel;
		finalize = state.finalize;
	},
	CanvasSource: class {
		async add(timestamp: number, duration: number) { state.frames.push([timestamp, duration]); }
		close() {}
	},
	VideoSample: class {
		timestamp: number;
		duration: number;
		format: string;
		constructor(_pixels: Uint8Array, init: { timestamp: number; duration: number; format: string }) {
			this.timestamp = init.timestamp;
			this.duration = init.duration;
			this.format = init.format;
		}
		close() {}
	},
	VideoSampleSource: class {
		async add(sample: { timestamp: number; duration: number; format: string }) {
			state.gpuFrames.push({ timestamp: sample.timestamp, duration: sample.duration, format: sample.format });
		}
		close() {}
	},
	BufferTarget: class {}, Mp4OutputFormat: class {}, WebMOutputFormat: class {}, AudioBufferSource: class {},
	QUALITY_LOW: 1, QUALITY_MEDIUM: 2, QUALITY_HIGH: 3, QUALITY_VERY_HIGH: 4,
}));

afterEach(() => vi.unstubAllGlobals());

beforeEach(() => {
	vi.clearAllMocks();
	state.frames.length = 0;
	state.gpuFrames.length = 0;
	state.backend = "canvas";
	state.gpuAvailable = false;
	state.gpuPixelFormat = "bgra8unorm";
	state.render.mockResolvedValue();
	state.finalize.mockResolvedValue();
});

function exporter() {
	return new SceneExporter({ width: 16, height: 16, fps: { numerator: 30, denominator: 1 }, format: "mp4", quality: "low" });
}
const scene = (duration: number) => ({ duration } as RootNode);

describe("scene export frame integration", () => {
	it("renders a selected window at absolute scene time while encoding from zero", async () => {
		const { mediaTime } = await import("@/wasm");
		await exporter().export({ rootNode: scene(360_000), range: { start: mediaTime({ticks:120_000}), end:mediaTime({ticks:240_001}) } });
		expect(state.frames).toHaveLength(31);
		expect(state.frames[0]).toEqual([0,1/30]);
		expect(state.frames[30]).toEqual([1,1/120_000]);
		expect(state.render.mock.calls[0][0]).toMatchObject({time:120_000});
		expect(state.render.mock.calls[30][0]).toMatchObject({time:240_000});
	});
	it("encodes copied WebGPU target pixels for each frame instead of sampling the swapchain canvas", async () => {
		state.backend = "webgpu";
		state.gpuAvailable = true;
		vi.stubGlobal("GPUTextureUsage", { RENDER_ATTACHMENT: 1, TEXTURE_BINDING: 2, COPY_SRC: 4 });
		const result = await exporter().export({ rootNode: scene(8_000) });
		expect(result?.byteLength).toBe(8);
		expect(state.frames).toHaveLength(0);
		expect(state.gpuFrames).toEqual([
			{ timestamp: 0, duration: 1 / 30, format: "BGRA" },
			{ timestamp: 1 / 30, duration: 1 / 30, format: "BGRA" },
		]);
		expect(state.render).toHaveBeenCalledTimes(2);
		expect(state.render).toHaveBeenNthCalledWith(1, expect.objectContaining({ targetTexture: state.gpuTexture }));
		const { webGpuEngine } = await import("@/native/webgpu-engine");
		expect(webGpuEngine.readTexturePixels).toHaveBeenCalledTimes(2);
		expect(webGpuEngine.releaseTexture).toHaveBeenCalledWith(state.gpuTexture);
	});
	it("sends the final partial duration to the encoder and samples inside the timeline", async () => {
		const result = await exporter().export({ rootNode: scene(120_001) });
		expect(result?.byteLength).toBe(8);
		expect(state.frames).toHaveLength(31);
		expect(state.frames[30]).toEqual([1, 1 / 120_000]);
		expect(state.render.mock.calls[30][0]).toMatchObject({ time: 120_000 });
		expect(state.finalize).toHaveBeenCalledTimes(1);
		expect(state.release).toHaveBeenCalledTimes(1);
	});
	it("does not encode a frame rendered after cancellation", async () => {
		const current = exporter();
		state.render.mockImplementationOnce(async () => { current.cancel(); });
		expect(await current.export({ rootNode: scene(120_000) })).toBeNull();
		expect(state.frames).toHaveLength(0);
		expect(state.cancel).toHaveBeenCalledTimes(1);
		expect(state.finalize).not.toHaveBeenCalled();
		expect(state.release).toHaveBeenCalledTimes(1);
	});
	it("cleans up the encoder and compositor after a render error", async () => {
		state.render.mockRejectedValueOnce(new Error("decode failed"));
		await expect(exporter().export({ rootNode: scene(1) })).rejects.toThrow("decode failed");
		expect(state.cancel).toHaveBeenCalledTimes(1);
		expect(state.release).toHaveBeenCalledTimes(1);
	});
	it("discards a finalized result if cancellation arrived during finalization", async () => {
		const current = exporter();
		const complete = vi.fn();
		current.on("complete", complete);
		state.finalize.mockImplementationOnce(async () => { current.cancel(); });
		expect(await current.export({ rootNode: scene(1) })).toBeNull();
		expect(complete).not.toHaveBeenCalled();
		expect(state.release).toHaveBeenCalledTimes(1);
	});
});

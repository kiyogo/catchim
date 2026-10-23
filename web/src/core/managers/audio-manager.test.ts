import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import type { EditorCore } from "@/core";
import type { AudioClipSource } from "@/media/audio";
import { collectAudioClips, createAudioContext } from "@/media/audio";
import { createAudioMasteringChain } from "@/media/audio-mastering";
import { AudioManager } from "./audio-manager";

const decoder = vi.hoisted(() => ({
	currentTime: 0,
	chunks: [] as Array<{ timestamp: number; duration: number; readyAt?: number }>,
	finished: false,
}));

vi.mock("@/media/audio", () => ({
	createAudioContext: vi.fn(),
	collectAudioClips: vi.fn(),
}));
vi.mock("@/media/audio-mastering", () => ({ createAudioMasteringChain: vi.fn() }));
vi.mock("@/native/rust-engine", () => ({
	getRustAudioSchedulingBindings: () => null,
	getRustEngineBindings: () => null,
}));
vi.mock("mediabunny", () => ({
	ALL_FORMATS: [],
	BlobSource: class {},
	InputDisposedError: class extends Error {},
	Input: class {
		disposed = false;
		async getPrimaryAudioTrack() { return {}; }
		dispose() { this.disposed = true; }
	},
	AudioBufferSink: class {
		async *buffers() {
			for (const { timestamp, duration, readyAt } of decoder.chunks) {
				if (readyAt !== undefined) decoder.currentTime = readyAt;
				yield { timestamp, buffer: { duration } };
			}
			decoder.finished = true;
		}
	},
}));

describe("AudioManager streaming playback", () => {
	let manager: AudioManager | undefined;
	beforeEach(() => {
		decoder.currentTime = 0;
		decoder.finished = false;
		decoder.chunks = [];
		vi.stubGlobal("window", { setInterval: () => 1, clearInterval: vi.fn() });
	});
	afterEach(() => {
		manager?.dispose();
		manager = undefined;
		vi.unstubAllGlobals();
	});

	async function play({
		rate = 1,
		seek = 0,
	}: { rate?: number; seek?: number } = {}) {
		const nodes: Array<{
			start: ReturnType<typeof vi.fn>;
			stop: ReturnType<typeof vi.fn>;
			disconnect: ReturnType<typeof vi.fn>;
			playbackRate: { value: number };
		}> = [];
		const context = {
			get currentTime() { return decoder.currentTime; },
			state: "running",
			destination: {},
			close: vi.fn(),
			createGain: () => ({ gain: { value: 1 }, connect: vi.fn(), disconnect: vi.fn() }),
			createBufferSource: () => {
				const node = {
					buffer: null,
					playbackRate: { value: 1 },
					connect: vi.fn(),
					disconnect: vi.fn(),
					start: vi.fn(),
					stop: vi.fn(),
					addEventListener: vi.fn(),
				};
				nodes.push(node);
				return node;
			},
		};
		vi.mocked(createAudioContext).mockReturnValue(context as unknown as AudioContext);
		vi.mocked(createAudioMasteringChain).mockReturnValue({
			input: context.createGain(),
		} as unknown as ReturnType<typeof createAudioMasteringChain>);
		vi.mocked(collectAudioClips).mockResolvedValue([{
			id: "clip",
			sourceKey: "media:file",
			file: new File([], "clip.mp4"),
			startTime: 0,
			duration: 2,
			trimStart: 4,
			trimEnd: 2,
			retime: { rate },
			volume: 1,
			muted: false,
			timelineElement: {
				id: "clip", name: "clip", type: "audio", sourceType: "upload", mediaId: "file",
				startTime: 0, duration: 240000, trimStart: 480000, trimEnd: 240000, params: {},
			},
		} as AudioClipSource]);
		let isPlaying = false;
		let notifyPlayback = () => {};
		const editor = {
			playback: {
				getVolume: () => 1,
				getIsPlaying: () => isPlaying,
				getCurrentTime: () => seek * 120000,
				subscribe: (listener: () => void) => { notifyPlayback = listener; return () => {}; },
				onSeek: () => () => {},
			},
			timeline: { subscribe: () => () => {}, getTotalDuration: () => 240000 },
			media: { subscribe: () => () => {}, getAssets: () => [] },
			scenes: { getActiveScene: () => ({ tracks: {} }) },
		} as unknown as EditorCore;
		manager = new AudioManager(editor);
		isPlaying = true;
		notifyPlayback();
		await vi.waitFor(() => expect(decoder.finished).toBe(true));
		return nodes;
	}

	it("limits the final decoded buffer to the trimmed clip end", async () => {
		decoder.chunks = [{ timestamp: 4, duration: 3 }];
		const nodes = await play();
		expect(nodes).toHaveLength(1);
		expect(nodes[0].start).toHaveBeenCalledWith(0, 0, 2);
	});

	it.each([0.5, 2])("scales late buffer offsets at %sx playback", async (rate) => {
		decoder.chunks = [{ timestamp: 4, duration: 10, readyAt: 0.5 }];
		const nodes = await play({ rate });
		expect(nodes).toHaveLength(1);
		expect(nodes[0].playbackRate.value).toBe(rate);
		expect(nodes[0].start).toHaveBeenCalledWith(0.5, 0.5 * rate, 1.5 * rate);
	});

	it("seeking into a decoded buffer never plays samples before the seek target", async () => {
		decoder.chunks = [{ timestamp: 4, duration: 3 }];
		const nodes = await play({ seek: 1 });
		expect(nodes[0].start).toHaveBeenCalledWith(0, 1, 1);
	});

	it("does not allocate audio nodes for a decoded buffer ending before the source trim", async () => {
		decoder.chunks = [{ timestamp: 3, duration: 1 }];
		const nodes = await play();
		expect(nodes).toHaveLength(0);
	});
});

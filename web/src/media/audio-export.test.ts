import { describe, expect, it, vi } from "vitest";
import { createTimelineAudioBuffer } from "./audio";
import type { NativeTimelineBindings } from "@/native/opencut-core";
import { initRustEngine } from "@/native/rust-engine";
import type { AudioElement, SceneTracks } from "@/timeline";
import type { MediaAsset } from "./types";
import { mediaTimeFromSeconds } from "@/wasm";

const state = vi.hoisted(() => ({ native: null as NativeTimelineBindings | null }));
vi.mock("@/native/opencut-core", () => ({ getNativeTimelineBindings: () => state.native }));

function audioBuffer(channels: number, length: number, sampleRate: number): AudioBuffer {
	const data = Array.from({ length: channels }, () => new Float32Array(length));
	return { length, numberOfChannels: channels, sampleRate, duration: length / sampleRate,
		getChannelData: (channel: number) => data[channel] } as AudioBuffer;
}

function fixture() {
	const source = audioBuffer(2, 240_000, 48_000);
	for (let channel = 0; channel < 2; channel++) {
		const data = source.getChannelData(channel);
		for (let index = 0; index < data.length; index++) data[index] = Math.sin(index / 97 + channel) * 0.1;
	}
	const at = (seconds: number) => mediaTimeFromSeconds({ seconds });
	const first = { id: "first", type: "audio", sourceType: "upload", mediaId: "asset", name: "voice",
		startTime: at(0.25), duration: at(2), trimStart: at(0.125), trimEnd: at(0), retime: { rate: 1.5 }, params: { volume: -6 } } as AudioElement;
	const second = { ...first, id: "second", startTime: at(0.5), duration: at(1), retime: { rate: 0.5 } } as AudioElement;
	const tracks: SceneTracks = { overlay: [], main: { id: "main", name: "main", type: "video", muted: false, hidden: false, elements: [] },
		audio: [{ id: "audio", name: "Audio", type: "audio", muted: false, elements: [first, second] }] };
	const mediaAssets: MediaAsset[] = [{ id: "asset", type: "audio", name: "voice", file: new File(["sample"], "voice.wav"), url: "blob:test", duration: 5, hasAudio: true }];
	const close = vi.fn(async () => {});
	const context = { sampleRate: 48_000, createBuffer: audioBuffer, decodeAudioData: async () => source, close } as unknown as AudioContext;
	return { tracks, mediaAssets, context, close, duration: at(3), sampleRate: 48_000 };
}

describe("cooperative export audio processing", () => {
	it("does not fetch or decode clips outside the marked audio window", async () => {
		const input=fixture();
		const fetch=vi.fn(async()=>new Response(new Blob(["unused"])));
		vi.stubGlobal("fetch",fetch);
		try {
			input.tracks.audio[0].elements.push({...input.tracks.audio[0].elements[0],id:"outside",sourceType:"library",sourceUrl:"https://example.test/outside.mp3",startTime:mediaTimeFromSeconds({seconds:2.5})} as AudioElement);
			const result=await createTimelineAudioBuffer({...input,audioContext:input.context,startTime:120_000,duration:90_000});
			expect(result?.length).toBe(36_000);
			expect(fetch).not.toHaveBeenCalled();
		} finally { vi.unstubAllGlobals(); }
	});
	it("mixes only the export window while preserving original trim and retime offsets", async () => {
		const input = fixture();
		for (const bindings of [null, await initRustEngine()]) {
			state.native = bindings;
			const full = await createTimelineAudioBuffer({...input,audioContext:input.context});
			const selected = await createTimelineAudioBuffer({...input,audioContext:input.context,startTime:120_000,duration:90_000});
			expect(selected?.length).toBe(36_000);
			for (let channel=0;channel<2;channel++) {
				const expected = full!.getChannelData(channel).subarray(48_000,84_000);
				const actual = selected!.getChannelData(channel);
				let error=0;
				for (let index=0;index<actual.length;index++) error=Math.max(error,Math.abs(expected[index]-actual[index]));
				expect(error).toBeLessThan(1e-6);
			}
		}
	});
	it("keeps trim, rate, overlap and chunk-boundary samples equivalent in JS and real Rust WASM", async () => {
		const input = fixture();
		state.native = null;
		const fallback = await createTimelineAudioBuffer({ ...input, audioContext: input.context });
		state.native = await initRustEngine();
		expect(state.native).not.toBeNull();
		const native = await createTimelineAudioBuffer({ ...input, audioContext: input.context, signal: new AbortController().signal });
		expect(native?.length).toBe(144_000);
		for (let channel = 0; channel < 2; channel++) {
			const actual = native!.getChannelData(channel);
			const expected = fallback!.getChannelData(channel);
			let maxError = 0;
			for (let index = 0; index < actual.length; index++) maxError = Math.max(maxError, Math.abs(actual[index] - expected[index]));
			expect(maxError).toBeLessThan(1e-6);
			expect(actual[0]).toBe(0);
			expect(actual[143_999]).toBe(0);
		}
		expect(input.close).not.toHaveBeenCalled();
	});

	it("frees WASM mix allocations after cancellation and keeps caller-owned context open", async () => {
		const native = await initRustEngine();
		if (!native) throw new Error("Rust WASM unavailable");
		const allocated = new Set<number>();
		state.native = new Proxy(native, { get(target, property) {
			if (property === "malloc") return (size: number) => { const pointer = target.malloc(size); if (pointer) allocated.add(pointer); return pointer; };
			if (property === "free") return (pointer: number) => { allocated.delete(pointer); target.free(pointer); };
			return Reflect.get(target, property);
		} });
		const input = fixture();
		const controller = new AbortController();
		setTimeout(() => controller.abort(), 0);
		await expect(createTimelineAudioBuffer({ ...input, audioContext: input.context, signal: controller.signal }))
			.rejects.toMatchObject({ name: "AbortError" });
		expect(allocated.size).toBe(0);
		expect(input.close).not.toHaveBeenCalled();
		state.native = native;
	});
});

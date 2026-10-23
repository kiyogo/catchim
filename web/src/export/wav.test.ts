import { describe, expect, it } from "vitest";
import { encodeAudioBufferWav } from "./wav";
import { initRustEngine } from "@/native/rust-engine";
import { getRustAudioExportBindings } from "@/native/audio-export";

function buffer(length = 4): AudioBuffer {
	const channels = [new Float32Array(length), new Float32Array(length)];
	channels[0].set([-2, 0, 2, NaN]);
	channels[1].set([1, -1, 0.5, Infinity]);
	return {
		length,
		numberOfChannels: 2,
		sampleRate: 48_000,
		getChannelData: (channel: number) => channels[channel],
	} as AudioBuffer;
}
describe("PCM WAV export", () => {
	it("writes a valid header and matches the actual Rust encoder byte for byte", async () => {
		const source = buffer(65_540);
		const fallback = await encodeAudioBufferWav({ audioBuffer: source });
		await initRustEngine();
		expect(getRustAudioExportBindings()).not.toBeNull();
		const native = await encodeAudioBufferWav({ audioBuffer: source });
		expect(new Uint8Array(native)).toEqual(new Uint8Array(fallback));
		const view = new DataView(native);
		expect(new TextDecoder().decode(new Uint8Array(native, 0, 4))).toBe("RIFF");
		expect(view.getUint16(22, true)).toBe(2);
		expect(view.getUint32(24, true)).toBe(48_000);
		expect(view.getUint16(34, true)).toBe(16);
		expect(view.getUint32(40, true)).toBe(65_540 * 4);
		expect(
			Array.from({ length: 8 }, (_, i) => view.getInt16(44 + i * 2, true)),
		).toEqual([-32768, 32767, 0, -32768, 32767, 16384, 0, 0]);
	});
	it("honors cancellation between bounded encoding chunks", async () => {
		const controller = new AbortController();
		setTimeout(() => controller.abort(), 0);
		await expect(
			encodeAudioBufferWav({
				audioBuffer: buffer(131_080),
				signal: controller.signal,
			}),
		).rejects.toMatchObject({ name: "AbortError" });
	});
});

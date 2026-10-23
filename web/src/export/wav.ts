import { getRustAudioExportBindings } from "@/native/audio-export";
import {
	throwIfAudioAborted,
	yieldAudioWork,
} from "@/media/audio-cancellation";

const CHUNK_FRAMES = 65_536;

/** RIFF is a file adapter; sample quantization/interleaving uses the Rust core. */
export async function encodeAudioBufferWav({
	audioBuffer,
	signal,
}: {
	audioBuffer: AudioBuffer;
	signal?: AbortSignal;
}): Promise<ArrayBuffer> {
	throwIfAudioAborted(signal);
	const channels = audioBuffer.numberOfChannels;
	const frames = audioBuffer.length;
	const sampleRate = audioBuffer.sampleRate;
	if (
		!Number.isInteger(channels) ||
		channels < 1 ||
		channels > 32 ||
		!Number.isSafeInteger(frames) ||
		frames <= 0 ||
		!Number.isInteger(sampleRate) ||
		sampleRate <= 0 ||
		sampleRate > 384_000
	)
		throw new Error("Invalid audio buffer for WAV export");
	const dataSize = frames * channels * 2;
	if (!Number.isSafeInteger(dataSize) || dataSize > 0xffff_ffff - 36)
		throw new Error("WAV export exceeds the RIFF file size limit");
	const output = new ArrayBuffer(44 + dataSize);
	const view = new DataView(output);
	const ascii = (offset: number, value: string) => {
		for (let i = 0; i < value.length; i++)
			view.setUint8(offset + i, value.charCodeAt(i));
	};
	ascii(0, "RIFF");
	view.setUint32(4, 36 + dataSize, true);
	ascii(8, "WAVE");
	ascii(12, "fmt ");
	view.setUint32(16, 16, true);
	view.setUint16(20, 1, true);
	view.setUint16(22, channels, true);
	view.setUint32(24, sampleRate, true);
	view.setUint32(28, sampleRate * channels * 2, true);
	view.setUint16(32, channels * 2, true);
	view.setUint16(34, 16, true);
	ascii(36, "data");
	view.setUint32(40, dataSize, true);
	const native = getRustAudioExportBindings();
	for (let start = 0; start < frames; start += CHUNK_FRAMES) {
		throwIfAudioAborted(signal);
		const count = Math.min(CHUNK_FRAMES, frames - start);
		const samples = new Float32Array(count * channels);
		for (let channel = 0; channel < channels; channel++)
			samples.set(
				audioBuffer.getChannelData(channel).subarray(start, start + count),
				channel * count,
			);
		if (native) {
			const encoded = native.encodePcm16(samples, channels);
			if (!encoded) throw new Error("Rust PCM encoding failed");
			new Uint8Array(output, 44 + start * channels * 2, encoded.length).set(
				encoded,
			);
		} else {
			// Compatibility with an older or unavailable Rust artifact.
			for (let frame = 0; frame < count; frame++)
				for (let channel = 0; channel < channels; channel++) {
					const value = samples[channel * count + frame];
					const sample = Number.isFinite(value)
						? Math.min(1, Math.max(-1, value))
						: 0;
					const scaled = Math.fround(sample * (sample < 0 ? 32768 : 32767));
					const quantized =
						scaled < 0 ? -Math.round(-scaled) : Math.round(scaled);
					view.setInt16(
						44 + ((start + frame) * channels + channel) * 2,
						quantized,
						true,
					);
				}
		}
		if (signal) await yieldAudioWork(signal);
	}
	throwIfAudioAborted(signal);
	return output;
}

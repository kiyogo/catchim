import { getNativeTimelineBindings } from "../native/opencut-core";
import { AUDIO_WORK_CHUNK_SAMPLES, runAudioTask, throwIfAudioAborted, yieldAudioWork } from "./audio-cancellation";

const MASTER_LIMITER_THRESHOLD_DB = -1;
const MASTER_LIMITER_KNEE_DB = 0;
const MASTER_LIMITER_RATIO = 20;
const MASTER_LIMITER_ATTACK_SECONDS = 0.001;
const MASTER_LIMITER_RELEASE_SECONDS = 0.12;
const MASTER_OUTPUT_HEADROOM = 0.98;

export function getAudioBufferPeak({
	audioBuffer,
}: {
	audioBuffer: AudioBuffer;
}): number {
	const bindings = getNativeTimelineBindings();
	if (bindings) {
		let maxPeak = 0;
		for (let channel = 0; channel < audioBuffer.numberOfChannels; channel++) {
			const channelData = audioBuffer.getChannelData(channel);
			if (channelData.length === 0) continue;
			const byteSize = channelData.length * 4;
			const ptr = bindings.malloc(byteSize);
			if (ptr !== 0) {
				try {
					bindings.HEAPF32.set(channelData, ptr >> 2);
					const chPeak = bindings.computeBufferPeak(ptr, channelData.length);
					if (chPeak > maxPeak) {
						maxPeak = chPeak;
					}
				} finally {
					bindings.free(ptr);
				}
			}
		}
		return maxPeak;
	}

	let peak = 0;

	for (let channel = 0; channel < audioBuffer.numberOfChannels; channel++) {
		const channelData = audioBuffer.getChannelData(channel);
		for (let index = 0; index < channelData.length; index++) {
			const magnitude = Math.abs(channelData[index]);
			if (magnitude > peak) {
				peak = magnitude;
			}
		}
	}

	return peak;
}

export function createAudioMasteringChain({
	audioContext,
	destination,
}: {
	audioContext: AudioContext | OfflineAudioContext;
	destination: AudioNode;
}): {
	input: GainNode;
} {
	const input = audioContext.createGain();
	const limiter = audioContext.createDynamicsCompressor();
	const outputGain = audioContext.createGain();

	limiter.threshold.value = MASTER_LIMITER_THRESHOLD_DB;
	limiter.knee.value = MASTER_LIMITER_KNEE_DB;
	limiter.ratio.value = MASTER_LIMITER_RATIO;
	limiter.attack.value = MASTER_LIMITER_ATTACK_SECONDS;
	limiter.release.value = MASTER_LIMITER_RELEASE_SECONDS;
	outputGain.gain.value = MASTER_OUTPUT_HEADROOM;

	input.connect(limiter);
	limiter.connect(outputGain);
	outputGain.connect(destination);

	return { input };
}

export async function applyAudioMasteringToBuffer({
	audioBuffer,
	signal,
}: {
	audioBuffer: AudioBuffer;
	signal?: AbortSignal;
}): Promise<AudioBuffer> {
	throwIfAudioAborted(signal);
	const peak = signal ? await getAudioBufferPeakCancellable(audioBuffer, signal) : getAudioBufferPeak({ audioBuffer });
	if (peak <= MASTER_OUTPUT_HEADROOM) {
		return audioBuffer;
	}

	const offlineContext = new OfflineAudioContext(
		audioBuffer.numberOfChannels,
		Math.max(1, audioBuffer.length),
		audioBuffer.sampleRate,
	);
	const source = offlineContext.createBufferSource();
	source.buffer = audioBuffer;

	const { input } = createAudioMasteringChain({
		audioContext: offlineContext,
		destination: offlineContext.destination,
	});
	source.connect(input);
	source.start(0);

	try {
		const renderedBuffer = await runAudioTask(() => offlineContext.startRendering(), signal, () => source.stop());
		await clampAudioBufferPeak({ audioBuffer: renderedBuffer, maxPeak: MASTER_OUTPUT_HEADROOM, signal });
		return renderedBuffer;
	} finally {
		source.disconnect();
		input.disconnect();
	}
}

async function getAudioBufferPeakCancellable(audioBuffer: AudioBuffer, signal: AbortSignal): Promise<number> {
	let peak = 0;
	for (let channel = 0; channel < audioBuffer.numberOfChannels; channel++) {
		peak = Math.max(peak, await getAudioChannelPeakCancellable(audioBuffer.getChannelData(channel), signal));
	}
	return peak;
}

async function getAudioChannelPeakCancellable(data: Float32Array, signal: AbortSignal): Promise<number> {
	const bindings = getNativeTimelineBindings();
	let peak = 0;
		for (let offset = 0; offset < data.length; offset += AUDIO_WORK_CHUNK_SAMPLES) {
			throwIfAudioAborted(signal);
			const chunk = data.subarray(offset, offset + AUDIO_WORK_CHUNK_SAMPLES);
			const ptr = bindings?.malloc(chunk.byteLength) ?? 0;
			if (bindings && ptr) {
				try {
					bindings.HEAPF32.set(chunk, ptr >> 2);
					peak = Math.max(peak, bindings.computeBufferPeak(ptr, chunk.length));
				} finally { bindings.free(ptr); }
			} else {
				for (const sample of chunk) peak = Math.max(peak, Math.abs(sample));
			}
			await yieldAudioWork(signal);
		}
	return peak;
}

async function clampAudioBufferPeak({
	audioBuffer,
	maxPeak,
	signal,
}: {
	audioBuffer: AudioBuffer;
	maxPeak: number;
	signal?: AbortSignal;
}): Promise<void> {
	throwIfAudioAborted(signal);
	const bindings = getNativeTimelineBindings();
	if (signal) {
		for (let channel = 0; channel < audioBuffer.numberOfChannels; channel++) {
			const data = audioBuffer.getChannelData(channel);
			const peak = bindings ? await getAudioChannelPeakCancellable(data, signal) : 0;
			const factor = peak > maxPeak ? maxPeak / peak : 1;
			for (let offset = 0; offset < data.length; offset += AUDIO_WORK_CHUNK_SAMPLES) {
				throwIfAudioAborted(signal);
				for (let index = offset; index < Math.min(offset + AUDIO_WORK_CHUNK_SAMPLES, data.length); index++) {
					data[index] = bindings ? data[index] * factor : Math.max(-maxPeak, Math.min(maxPeak, data[index]));
				}
				await yieldAudioWork(signal);
			}
		}
		return;
	}
	if (bindings) {
		for (let channel = 0; channel < audioBuffer.numberOfChannels; channel++) {
			const channelData = audioBuffer.getChannelData(channel);
			if (channelData.length === 0) continue;
			const byteSize = channelData.length * 4;
			const ptr = bindings.malloc(byteSize);
			if (ptr !== 0) {
				try {
					bindings.HEAPF32.set(channelData, ptr >> 2);
					bindings.clampBufferSamples(ptr, channelData.length, maxPeak);
					channelData.set(bindings.HEAPF32.subarray(ptr >> 2, (ptr >> 2) + channelData.length));
				} finally {
					bindings.free(ptr);
				}
			}
		}
		return;
	}

	for (let channel = 0; channel < audioBuffer.numberOfChannels; channel++) {
		const channelData = audioBuffer.getChannelData(channel);
		for (let index = 0; index < channelData.length; index++) {
			channelData[index] = Math.max(
				-maxPeak,
				Math.min(maxPeak, channelData[index]),
			);
		}
	}
}

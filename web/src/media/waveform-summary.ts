"use client";

import { getSourceTimeAtClipTime } from "@/retime";
import type { RetimeConfig } from "@/timeline";
import { getNativeTimelineBindings } from "@/native/opencut-core";
import { getRustAudioBindings } from "@/native/rust-engine";
import { webGpuEngine } from "@/native/webgpu-engine";

const RMS_ANALYSIS_WINDOW_SECONDS = 0.02;
const DEFAULT_SOURCE_WAVEFORM_BUCKET_SIZE = 128;

function computePeakBuckets({
	buffer,
	buckets,
}: {
	buffer: AudioBuffer;
	buckets: SampleBucket[];
}): number[] {
	const bindings = getNativeTimelineBindings();
	if (bindings && buckets.length > 0 && buffer.length > 0) {
		try {
			const numBuckets = buckets.length;
			const startsPtr = bindings.malloc(numBuckets * 4);
			const endsPtr = bindings.malloc(numBuckets * 4);
			const outPeaksPtr = bindings.malloc(numBuckets * 4);
			const starts = new Uint32Array(bindings.HEAPU32.buffer, startsPtr, numBuckets);
			const ends = new Uint32Array(bindings.HEAPU32.buffer, endsPtr, numBuckets);
			for (let i = 0; i < numBuckets; i++) {
				starts[i] = buckets[i].bucketStart;
				ends[i] = buckets[i].bucketEnd;
			}

			const chLen = buffer.length;
			const chPtr = bindings.malloc(chLen * 4);
			const outArray = new Float32Array(numBuckets);

			for (let c = 0; c < buffer.numberOfChannels; c++) {
				const chData = buffer.getChannelData(c);
				new Float32Array(bindings.HEAPF32.buffer, chPtr, chLen).set(chData);
				bindings.computePeakBuckets(chPtr, startsPtr, endsPtr, numBuckets, outPeaksPtr);
				const channelPeaks = new Float32Array(bindings.HEAPF32.buffer, outPeaksPtr, numBuckets);
				for (let i = 0; i < numBuckets; i++) {
					if (channelPeaks[i] > outArray[i]) {
						outArray[i] = channelPeaks[i];
					}
				}
			}

			bindings.free(chPtr);
			bindings.free(startsPtr);
			bindings.free(endsPtr);
			bindings.free(outPeaksPtr);

			return Array.from(outArray);
		} catch (e) {
			console.warn("[OpenCut] Native computePeakBuckets fallback:", e);
		}
	}

	const channels = buffer.numberOfChannels;
	const channelData: Float32Array[] = Array.from({ length: channels }, (_, c) =>
		buffer.getChannelData(c),
	);

	return buckets.map(({ bucketStart, bucketEnd }) => {
		let peak = 0;
		for (let c = 0; c < channels; c++) {
			const data = channelData[c];
			for (let j = bucketStart; j < bucketEnd; j++) {
				const abs = Math.abs(data[j] ?? 0);
				if (abs > peak) {
					peak = abs;
				}
			}
		}
		return peak;
	});
}

export interface SampleBucket {
	bucketStart: number;
	bucketEnd: number;
}

export interface SourceWaveformSummary {
	sourceKey: string;
	sampleRate: number;
	totalSamples: number;
	bucketSize: number;
	amplitudes: Float32Array;
}

export function buildWaveformSourceKey({
	kind,
	id,
}: {
	kind: "media" | "library";
	id: string;
}): string {
	return `${kind}:${id}`;
}

export function buildSourceWaveformSummary({
	sourceKey,
	buffer,
	bucketSize = DEFAULT_SOURCE_WAVEFORM_BUCKET_SIZE,
}: {
	sourceKey: string;
	buffer: AudioBuffer;
	bucketSize?: number;
}): SourceWaveformSummary {
	const safeBucketSize = Math.max(1, Math.floor(bucketSize));
	const bucketCount = Math.max(1, Math.ceil(buffer.length / safeBucketSize));
	const amplitudes = computePeakBuckets({
		buffer,
		buckets: Array.from({ length: bucketCount }, (_, bucketIndex) => {
			const bucketStart = bucketIndex * safeBucketSize;
			const bucketEnd = Math.min(buffer.length, bucketStart + safeBucketSize);
			return { bucketStart, bucketEnd };
		}),
	});

	return {
		sourceKey,
		sampleRate: buffer.sampleRate,
		totalSamples: buffer.length,
		bucketSize: safeBucketSize,
		amplitudes: Float32Array.from(amplitudes),
	};
}

export async function buildSourceWaveformSummaryAsync({
	sourceKey,
	buffer,
	bucketSize = DEFAULT_SOURCE_WAVEFORM_BUCKET_SIZE,
}: {
	sourceKey: string;
	buffer: AudioBuffer;
	bucketSize?: number;
}): Promise<SourceWaveformSummary> {
	const safeBucketSize = Math.max(1, Math.floor(bucketSize));
	const bucketCount = Math.max(1, Math.ceil(buffer.length / safeBucketSize));

	if (webGpuEngine.isWebGpuSupported() && buffer.length > 0 && bucketCount > 0) {
		try {
			if (!webGpuEngine.isAvailable) {
				await webGpuEngine.initialize();
			}
			if (webGpuEngine.isAvailable) {
				const channels = buffer.numberOfChannels;
				const outAmplitudes = new Float32Array(bucketCount);

				for (let c = 0; c < channels; c++) {
					const channelData = buffer.getChannelData(c);
					const peaks = await webGpuEngine.computeAudioWaveformPeaks({
						samples: channelData,
						numBuckets: bucketCount,
					});
					for (let b = 0; b < bucketCount; b++) {
						const peakAmp = Math.max(Math.abs(peaks[b * 2]), Math.abs(peaks[b * 2 + 1]));
						if (peakAmp > outAmplitudes[b]) {
							outAmplitudes[b] = peakAmp;
						}
					}
				}

				return {
					sourceKey,
					sampleRate: buffer.sampleRate,
					totalSamples: buffer.length,
					bucketSize: safeBucketSize,
					amplitudes: outAmplitudes,
				};
			}
		} catch (e) {
			console.warn("[OpenCut] WebGPU waveform compute error, falling back to WASM/CPU:", e);
		}
	}

	return buildSourceWaveformSummary({ sourceKey, buffer, bucketSize });
}

export function buildWaveformSampleBuckets({
	clipLeftPx,
	clipRightPx,
	barCount,
	pixelsPerSecond,
	clipDurationSec,
	sourceStartSec,
	retime,
	sampleRate,
	maxSampleExclusive,
	barStepPx,
}: {
	clipLeftPx: number;
	clipRightPx: number;
	barCount: number;
	pixelsPerSecond: number;
	clipDurationSec: number;
	sourceStartSec: number;
	retime?: RetimeConfig;
	sampleRate: number;
	maxSampleExclusive: number;
	barStepPx: number;
}): SampleBucket[] {
	return Array.from({ length: barCount }, (_, index) => {
		const bucketLeftPx = clipLeftPx + index * barStepPx;
		const bucketRightPx = Math.min(clipRightPx, bucketLeftPx + barStepPx);
		const clipStartSec = Math.max(
			0,
			Math.min(clipDurationSec, bucketLeftPx / pixelsPerSecond),
		);
		const clipEndSec = Math.max(
			clipStartSec,
			Math.min(clipDurationSec, bucketRightPx / pixelsPerSecond),
		);
		const sourceBucketStartSec =
			sourceStartSec +
			getSourceTimeAtClipTime({
				clipTime: clipStartSec,
				retime,
			});
		const sourceBucketEndSec =
			sourceStartSec +
			getSourceTimeAtClipTime({
				clipTime: clipEndSec,
				retime,
			});

		return {
			bucketStart: Math.max(0, Math.floor(sourceBucketStartSec * sampleRate)),
			bucketEnd: Math.min(
				maxSampleExclusive,
				Math.max(0, Math.ceil(sourceBucketEndSec * sampleRate)),
			),
		};
	});
}

export function sampleSourceWaveformSummary({
	summary,
	buckets,
}: {
	summary: SourceWaveformSummary;
	buckets: SampleBucket[];
}): number[] {
	const rustAudio = getRustAudioBindings();
	if (rustAudio && buckets.length > 0) {
		try {
			const peaks = rustAudio.samplePeakBuckets({
				amplitudes: summary.amplitudes,
				bucketSize: summary.bucketSize,
				starts: Float64Array.from(buckets, ({ bucketStart }) => bucketStart),
				ends: Float64Array.from(buckets, ({ bucketEnd }) => bucketEnd),
			});
			if (peaks) {
				return Array.from(peaks);
			}
		} catch (error) {
			console.warn("[OpenCut] Rust waveform sampling fallback:", error);
		}
	}

	return buckets.map(({ bucketStart, bucketEnd }) => {
		if (bucketEnd <= bucketStart) {
			return 0;
		}

		const startIndex = Math.max(
			0,
			Math.floor(bucketStart / summary.bucketSize),
		);
		const endIndex = Math.min(
			summary.amplitudes.length,
			Math.max(startIndex + 1, Math.ceil(bucketEnd / summary.bucketSize)),
		);

		let maxAmplitude = 0;
		for (let i = startIndex; i < endIndex; i++) {
			const amplitude = summary.amplitudes[i] ?? 0;
			if (amplitude > maxAmplitude) {
				maxAmplitude = amplitude;
			}
		}

		return maxAmplitude;
	});
}

export function computeRmsBuckets({
	buffer,
	buckets,
}: {
	buffer: AudioBuffer;
	buckets: SampleBucket[];
}): number[] {
	const maxWindowLength = Math.max(
		1,
		Math.floor(buffer.sampleRate * RMS_ANALYSIS_WINDOW_SECONDS),
	);

	const bindings = getNativeTimelineBindings();
	if (bindings && buckets.length > 0 && buffer.length > 0) {
		try {
			const numBuckets = buckets.length;
			const startsPtr = bindings.malloc(numBuckets * 4);
			const endsPtr = bindings.malloc(numBuckets * 4);
			const outRmsPtr = bindings.malloc(numBuckets * 4);
			const starts = new Uint32Array(bindings.HEAPU32.buffer, startsPtr, numBuckets);
			const ends = new Uint32Array(bindings.HEAPU32.buffer, endsPtr, numBuckets);
			for (let i = 0; i < numBuckets; i++) {
				starts[i] = buckets[i].bucketStart;
				ends[i] = buckets[i].bucketEnd;
			}

			const chLen = buffer.length;
			const chPtr = bindings.malloc(chLen * 4);
			const ch0 = buffer.getChannelData(0);
			if (buffer.numberOfChannels === 1) {
				new Float32Array(bindings.HEAPF32.buffer, chPtr, chLen).set(ch0);
			} else {
				const ch1 = buffer.getChannelData(1);
				const ch0Ptr = bindings.malloc(chLen * 4);
				const ch1Ptr = bindings.malloc(chLen * 4);
				try {
					bindings.HEAPF32.set(ch0, ch0Ptr >> 2);
					bindings.HEAPF32.set(ch1, ch1Ptr >> 2);
					bindings.downmixStereo(ch0Ptr, ch1Ptr, chPtr, chLen);
				} finally {
					bindings.free(ch0Ptr);
					bindings.free(ch1Ptr);
				}
			}

			bindings.computeRmsBuckets(chPtr, maxWindowLength, startsPtr, endsPtr, numBuckets, outRmsPtr);
			const rmsResult = Array.from(new Float32Array(bindings.HEAPF32.buffer, outRmsPtr, numBuckets));

			bindings.free(chPtr);
			bindings.free(startsPtr);
			bindings.free(endsPtr);
			bindings.free(outRmsPtr);

			return rmsResult;
		} catch (e) {
			console.warn("[OpenCut] Native computeRmsBuckets fallback:", e);
		}
	}

	const channels = buffer.numberOfChannels;

	const channelData: Float32Array[] = new Array(channels);
	for (let c = 0; c < channels; c++) {
		channelData[c] = buffer.getChannelData(c);
	}

	const result = new Array<number>(buckets.length);

	for (let i = 0; i < buckets.length; i++) {
		const { bucketStart, bucketEnd } = buckets[i];
		const bucketLength = bucketEnd - bucketStart;
		if (bucketLength <= 0) {
			result[i] = 0;
			continue;
		}

		const windowLength = Math.max(1, Math.min(bucketLength, maxWindowLength));
		let maxMeanSquare = 0;

		for (let winStart = bucketStart; winStart < bucketEnd; ) {
			const winEnd = Math.min(winStart + windowLength, bucketEnd);
			const n = winEnd - winStart;
			if (n > 0) {
				let sum = 0;
				for (let c = 0; c < channels; c++) {
					const data = channelData[c];
					for (let j = winStart; j < winEnd; j++) {
						const v = data[j];
						sum += v * v;
					}
				}
				const meanSquare = sum / (n * channels);
				if (meanSquare > maxMeanSquare) {
					maxMeanSquare = meanSquare;
				}
			}
			winStart = winEnd;
		}

		result[i] = Math.sqrt(maxMeanSquare);
	}

	return result;
}

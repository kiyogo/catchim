import { getNativeTimelineBindings } from "@/native/opencut-core";
import { detectAudioBeatsInWorker } from "./beat-detection-worker-client";

const ANALYSIS_WINDOW_SECONDS = 0.02;
const MIN_BEAT_GAP_SECONDS = 0.24;
export const MAX_WASM_INPUT_BYTES = 256 * 1024 * 1024;

function detectWithRust({
  channels,
  samplesPerChannel,
  sampleRate,
  startSeconds,
  endSeconds,
  maxBeats,
}: {
  channels: Float32Array[];
  samplesPerChannel: number;
  sampleRate: number;
  startSeconds: number;
  endSeconds: number;
  maxBeats: number;
}): number[] | null {
  const bindings = getNativeTimelineBindings();
  if (
    !bindings?.detectAudioBeats ||
    samplesPerChannel === 0 ||
    channels.length === 0 ||
    !Number.isSafeInteger(maxBeats) ||
    maxBeats <= 0 ||
    !Number.isFinite(sampleRate) ||
    sampleRate <= 0
  ) {
    return null;
  }

  const totalSamples = samplesPerChannel * channels.length;
  const inputBytes = totalSamples * Float32Array.BYTES_PER_ELEMENT;
  const outputBytes = maxBeats * Float64Array.BYTES_PER_ELEMENT;
  if (
    !Number.isSafeInteger(totalSamples) ||
    !Number.isSafeInteger(inputBytes) ||
    inputBytes > MAX_WASM_INPUT_BYTES ||
    !Number.isSafeInteger(outputBytes)
  ) {
    return null;
  }

  let samplesPtr = 0;
  let outTimesPtr = 0;
  try {
    samplesPtr = bindings.malloc(inputBytes);
    outTimesPtr = bindings.malloc(outputBytes);
    if (!samplesPtr || !outTimesPtr) {
      return null;
    }

    const samples = new Float32Array(
      bindings.HEAPF32.buffer,
      samplesPtr,
      totalSamples,
    );
    for (let channel = 0; channel < channels.length; channel += 1) {
      const channelData = channels[channel];
      if (channelData.length !== samplesPerChannel) {
        return null;
      }
      samples.set(channelData, channel * samplesPerChannel);
    }

    const count = bindings.detectAudioBeats(
      samplesPtr,
      samplesPerChannel,
      channels.length,
      sampleRate,
      startSeconds,
      endSeconds,
      maxBeats,
      outTimesPtr,
    );
    if (!Number.isSafeInteger(count) || count < 0 || count > maxBeats) {
      return null;
    }
    return Array.from(
      new Float64Array(bindings.HEAPF64.buffer, outTimesPtr, count),
    );
  } catch (error) {
    console.warn(
      "[Catchim] Rust beat detection failed; using TypeScript fallback:",
      error,
    );
    return null;
  } finally {
    if (outTimesPtr) bindings.free(outTimesPtr);
    if (samplesPtr) bindings.free(samplesPtr);
  }
}

/** Detect strong audio onsets as approximate, editable beat markers. */
function detectAudioBeatsFallback({
  channels,
  sampleRate,
  sampleCount,
  startSeconds = 0,
  endSeconds = sampleCount / sampleRate,
  maxBeats = 120,
}: {
  channels: Float32Array[];
  sampleRate: number;
  sampleCount: number;
  startSeconds?: number;
  endSeconds?: number;
  maxBeats?: number;
}): number[] {
  const windowSize = Math.max(
    1,
    Math.round(sampleRate * ANALYSIS_WINDOW_SECONDS),
  );
  const startSample = Math.max(0, Math.floor(startSeconds * sampleRate));
  const endSample = Math.min(sampleCount, Math.ceil(endSeconds * sampleRate));
  const windowCount = Math.ceil((endSample - startSample) / windowSize);
  if (windowCount < 3) return [];

  const energy = new Float32Array(windowCount);
  for (let window = 0; window < windowCount; window++) {
    const from = startSample + window * windowSize;
    const to = Math.min(endSample, from + windowSize);
    let sum = 0;
    let count = 0;
    for (const channel of channels) {
      for (let sample = from; sample < to; sample++) {
        const value = channel[sample] ?? 0;
        sum += value * value;
        count++;
      }
    }
    energy[window] = count === 0 ? 0 : Math.sqrt(sum / count);
  }

  const onset = new Float32Array(windowCount);
  let onsetTotal = 0;
  let onsetCount = 0;
  for (let i = 1; i < windowCount; i++) {
    const increase = Math.max(0, energy[i] - energy[i - 1]);
    onset[i] = increase;
    if (increase > 0) {
      onsetTotal += increase;
      onsetCount++;
    }
  }
  const meanOnset = onsetCount === 0 ? 0 : onsetTotal / onsetCount;
  if (meanOnset <= 0) return [];

  const minGapWindows = Math.max(
    1,
    Math.round(MIN_BEAT_GAP_SECONDS / ANALYSIS_WINDOW_SECONDS),
  );
  const beats: number[] = [];
  let lastBeatWindow = -minGapWindows;
  for (let i = 1; i < windowCount - 1; i++) {
    if (
      onset[i] < meanOnset * 1.65 ||
      onset[i] < onset[i - 1] ||
      onset[i] < onset[i + 1] ||
      energy[i] < 0.012 ||
      i - lastBeatWindow < minGapWindows
    ) {
      continue;
    }
    beats.push((startSample + i * windowSize) / sampleRate);
    lastBeatWindow = i;
    if (beats.length >= maxBeats) break;
  }

  return beats;
}

export interface AudioBeatDetectionOptions {
  buffer: AudioBuffer;
  startSeconds?: number;
  endSeconds?: number;
  maxBeats?: number;
}

export function detectAudioBeats({
  buffer,
  startSeconds = 0,
  endSeconds = buffer.duration,
  maxBeats = 120,
}: AudioBeatDetectionOptions): number[] {
  const channels = Array.from(
    { length: buffer.numberOfChannels },
    (_, channel) => buffer.getChannelData(channel),
  );
  const rustResult = detectWithRust({
    channels,
    samplesPerChannel: buffer.length,
    sampleRate: buffer.sampleRate,
    startSeconds,
    endSeconds,
    maxBeats,
  });
  if (rustResult !== null) {
    return rustResult;
  }
  return detectAudioBeatsFallback({
    channels,
    sampleRate: buffer.sampleRate,
    sampleCount: buffer.length,
    startSeconds,
    endSeconds,
    maxBeats,
  });
}

export function detectAudioBeatsFromPlanarSamples({
  samples,
  samplesPerChannel,
  channelCount,
  sampleRate,
  startSeconds = 0,
  endSeconds = samplesPerChannel / sampleRate,
  maxBeats = 120,
}: {
  samples: Float32Array;
  samplesPerChannel: number;
  channelCount: number;
  sampleRate: number;
  startSeconds?: number;
  endSeconds?: number;
  maxBeats?: number;
}): number[] {
  const totalSamples = samplesPerChannel * channelCount;
  if (
    !Number.isSafeInteger(samplesPerChannel) ||
    samplesPerChannel <= 0 ||
    !Number.isSafeInteger(channelCount) ||
    channelCount <= 0 ||
    !Number.isSafeInteger(totalSamples) ||
    samples.length < totalSamples
  ) {
    return [];
  }

  const channels = Array.from({ length: channelCount }, (_, channel) =>
    samples.subarray(
      channel * samplesPerChannel,
      (channel + 1) * samplesPerChannel,
    ),
  );
  const rustResult = detectWithRust({
    channels,
    samplesPerChannel,
    sampleRate,
    startSeconds,
    endSeconds,
    maxBeats,
  });
  if (rustResult !== null) {
    return rustResult;
  }
  return detectAudioBeatsFallback({
    channels,
    sampleRate,
    sampleCount: samplesPerChannel,
    startSeconds,
    endSeconds,
    maxBeats,
  });
}

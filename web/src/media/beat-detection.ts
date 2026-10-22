import { detectAudioBeats, MAX_WASM_INPUT_BYTES } from "./beat-detection-core";
import { detectAudioBeatsInWorker } from "./beat-detection-worker-client";
import type { AudioBeatDetectionOptions } from "./beat-detection-core";

export {
  detectAudioBeats,
  detectAudioBeatsFromPlanarSamples,
} from "./beat-detection-core";
export type { AudioBeatDetectionOptions } from "./beat-detection-core";
export async function detectAudioBeatsAsync(
  options: AudioBeatDetectionOptions,
): Promise<number[]> {
  const { buffer } = options;
  const sampleCount = buffer.length;
  const channelCount = buffer.numberOfChannels;
  const totalSamples = sampleCount * channelCount;
  const inputBytes = totalSamples * Float32Array.BYTES_PER_ELEMENT;

  if (
    typeof window === "undefined" ||
    typeof Worker === "undefined" ||
    !Number.isSafeInteger(totalSamples) ||
    totalSamples <= 0 ||
    inputBytes > MAX_WASM_INPUT_BYTES
  ) {
    return detectAudioBeats(options);
  }

  try {
    const samples = new Float32Array(totalSamples);
    for (let channel = 0; channel < channelCount; channel += 1) {
      samples.set(buffer.getChannelData(channel), channel * sampleCount);
    }
    return await detectAudioBeatsInWorker({
      samples: samples.buffer,
      samplesPerChannel: sampleCount,
      channelCount,
      sampleRate: buffer.sampleRate,
      startSeconds: options.startSeconds ?? 0,
      endSeconds: options.endSeconds ?? buffer.duration,
      maxBeats: options.maxBeats ?? 120,
    });
  } catch (error) {
    console.warn(
      "[Catchim] Beat detection worker failed; using the main-thread path:",
      error,
    );
    return detectAudioBeats(options);
  }
}

import { describe, expect, test } from "bun:test";
import { detectAudioBeats } from "./beat-detection";

function createBeatBuffer(): AudioBuffer {
  const sampleRate = 500;
  const samples = new Float32Array(1_000);
  for (const [window, level] of [
    [5, 0.05],
    [20, 0.1],
    [35, 0.9],
    [50, 0.15],
    [65, 0.2],
  ] as const) {
    samples.fill(level, window * 10, (window + 1) * 10);
  }

  return {
    duration: samples.length / sampleRate,
    length: samples.length,
    numberOfChannels: 1,
    sampleRate,
    getChannelData: () => samples,
  } as unknown as AudioBuffer;
}

describe("detectAudioBeats", () => {
  test("detects the strongest onset and respects the requested source range", () => {
    expect(
      detectAudioBeats({
        buffer: createBeatBuffer(),
        startSeconds: 0.5,
        endSeconds: 1.5,
        maxBeats: 1,
      }),
    ).toEqual([0.7]);
  });

  test("returns no markers for silence", () => {
    const buffer = createBeatBuffer();
    const silence = new Float32Array(buffer.length);
    const silentBuffer = {
      ...buffer,
      getChannelData: () => silence,
    } as AudioBuffer;

    expect(detectAudioBeats({ buffer: silentBuffer })).toEqual([]);
  });
});

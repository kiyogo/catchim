import { describe, expect, test } from "bun:test";
import {
  detectAudioBeats,
  detectAudioBeatsFromPlanarSamples,
} from "@/media/beat-detection";
import {
  getRustTimelineSelectionBindings,
  initRustEngine,
} from "./rust-engine";

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

describe("Rust WASM beat detection", () => {
  test("matches the TypeScript compatibility path on the same audio", async () => {
    const buffer = createBeatBuffer();
    const fallback = detectAudioBeats({
      buffer,
      startSeconds: 0.5,
      endSeconds: 1.5,
      maxBeats: 1,
    });

    const bindings = await initRustEngine();
    expect(bindings?.detectAudioBeats).toBeTypeOf("function");
    const rustResult = detectAudioBeats({
      buffer,
      startSeconds: 0.5,
      endSeconds: 1.5,
      maxBeats: 1,
    });

    expect(rustResult).toEqual(fallback);
    expect(rustResult).toEqual([0.7]);
  });

  test("returns the source onset through the C ABI", async () => {
    const bindings = await initRustEngine();
    expect(bindings?.detectAudioBeats).toBeTypeOf("function");
    if (!bindings?.detectAudioBeats) return;

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

    const maxBeats = 120;
    const samplesPtr = bindings.malloc(samples.byteLength);
    const outTimesPtr = bindings.malloc(
      maxBeats * Float64Array.BYTES_PER_ELEMENT,
    );
    try {
      bindings.HEAPF32.set(samples, samplesPtr >> 2);
      const count = bindings.detectAudioBeats(
        samplesPtr,
        samples.length,
        1,
        500,
        0,
        2,
        maxBeats,
        outTimesPtr,
      );
      const actual = Array.from(
        new Float64Array(bindings.HEAPF64.buffer, outTimesPtr, count),
      );
      expect(actual).toEqual([0.7]);
    } finally {
      bindings.free(outTimesPtr);
      bindings.free(samplesPtr);
    }
  });

  test("detects beats from planar audio supplied by a worker", async () => {
    const bindings = await initRustEngine();
    expect(bindings?.detectAudioBeats).toBeTypeOf("function");

    const buffer = createBeatBuffer();
    const planar = new Float32Array(buffer.length * buffer.numberOfChannels);
    for (let channel = 0; channel < buffer.numberOfChannels; channel += 1) {
      planar.set(buffer.getChannelData(channel), channel * buffer.length);
    }

    expect(
      detectAudioBeatsFromPlanarSamples({
        samples: planar,
        samplesPerChannel: buffer.length,
        channelCount: buffer.numberOfChannels,
        sampleRate: buffer.sampleRate,
        startSeconds: 0.5,
        endSeconds: 1.5,
        maxBeats: 1,
      }),
    ).toEqual([0.7]);
  });

  test("selects inclusive rectangle intersections through Rust WASM", async () => {
    await initRustEngine();
    const bindings = getRustTimelineSelectionBindings();
    expect(bindings?.selectRectIntersections).toBeTypeOf("function");
    if (!bindings) return;

    const rectangles = Float64Array.from([
      0, 0, 2, 2,
      10, 3, 20, 8,
      10.5, 0, 12, 2,
    ]);
    const selection = Float64Array.from([0, 0, 10, 10]);
    expect(bindings.selectRectIntersections(rectangles, selection)).toEqual([0, 1]);
  });
});

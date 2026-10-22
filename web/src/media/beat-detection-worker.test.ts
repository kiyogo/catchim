import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { runInNewContext } from "node:vm";
import { describe, expect, test } from "bun:test";
import type { DetectAudioBeatsWorkerRequest } from "./beat-detection-worker-client";

describe("beat detection worker", () => {
  test("loads the checked-in Rust WASM and returns detected source times", async () => {
    const workerSource = readFileSync(
      resolve(process.cwd(), "public/workers/beat-detection.worker.js"),
      "utf8",
    );
    const wasmBytes = Uint8Array.from(
      readFileSync(resolve(process.cwd(), "public/wasm/opencut_engine.wasm")),
    ).buffer;
    let handleMessage:
      ((event: { data: DetectAudioBeatsWorkerRequest }) => void) | undefined;
    let resolveResponse:
      | ((response: { id: number; type: string; times?: number[] }) => void)
      | undefined;
    const responsePromise = new Promise<{
      id: number;
      type: string;
      times?: number[];
    }>((resolvePromise) => {
      resolveResponse = resolvePromise;
    });
    const scope = {
      addEventListener: (
        type: string,
        listener: (event: { data: DetectAudioBeatsWorkerRequest }) => void,
      ) => {
        if (type === "message") handleMessage = listener;
      },
      postMessage: (response: { id: number; type: string; times?: number[] }) =>
        resolveResponse?.(response),
    };

    runInNewContext(workerSource, {
      self: scope,
      fetch: async () => ({
        ok: true,
        arrayBuffer: async () => wasmBytes,
      }),
      WebAssembly,
      Float32Array,
      Float64Array,
    });

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

    handleMessage?.({
      data: {
        id: 77,
        type: "detect",
        samples: samples.buffer,
        samplesPerChannel: samples.length,
        channelCount: 1,
        sampleRate: 500,
        startSeconds: 0,
        endSeconds: 2,
        maxBeats: 120,
      },
    });

    await expect(responsePromise).resolves.toEqual({
      id: 77,
      type: "result",
      times: [0.7],
    });
  });
});

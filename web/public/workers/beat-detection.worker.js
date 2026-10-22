let enginePromise;

self.addEventListener("message", (event) => {
  if (event.data?.type === "detect") {
    void handleRequest(event.data);
  }
});

async function getEngineExports() {
  if (!enginePromise) {
    enginePromise = (async () => {
      const response = await fetch("/wasm/opencut_engine.wasm");
      if (!response.ok) {
        throw new Error(`Failed to load Rust WASM: ${response.status}`);
      }

      const bytes = await response.arrayBuffer();
      const { instance } = await WebAssembly.instantiate(bytes, {});
      const exports = instance.exports;
      for (const name of [
        "memory",
        "oc_malloc",
        "oc_free",
        "ocw_audio_detect_beats",
      ]) {
        if (!(name in exports)) {
          throw new Error(`Rust WASM is missing export ${name}`);
        }
      }
      return exports;
    })().catch((error) => {
      enginePromise = undefined;
      throw error;
    });
  }
  return enginePromise;
}

async function handleRequest(request) {
  let exportsForFree;
  let samplesPointer = 0;
  let timesPointer = 0;
  try {
    exportsForFree = await getEngineExports();
    const exports = exportsForFree;
    const samples = new Float32Array(request.samples);
    const totalSamples = request.samplesPerChannel * request.channelCount;
    const samplesBytes = totalSamples * Float32Array.BYTES_PER_ELEMENT;
    const timesBytes = request.maxBeats * Float64Array.BYTES_PER_ELEMENT;
    if (
      !Number.isSafeInteger(totalSamples) ||
      totalSamples <= 0 ||
      samples.length < totalSamples ||
      !Number.isSafeInteger(request.maxBeats) ||
      request.maxBeats <= 0 ||
      timesBytes > 0x7fffffff
    ) {
      throw new Error("Invalid beat detection input");
    }

    samplesPointer = exports.oc_malloc(samplesBytes);
    timesPointer = exports.oc_malloc(timesBytes);
    if (!samplesPointer || !timesPointer) {
      throw new Error("Rust WASM could not allocate beat detection buffers");
    }

    new Float32Array(exports.memory.buffer, samplesPointer, totalSamples).set(
      samples.subarray(0, totalSamples),
    );
    const count = exports.ocw_audio_detect_beats(
      samplesPointer,
      request.samplesPerChannel,
      request.channelCount,
      request.sampleRate,
      request.startSeconds,
      request.endSeconds,
      request.maxBeats,
      timesPointer,
    );
    if (!Number.isSafeInteger(count) || count < 0 || count > request.maxBeats) {
      throw new Error("Rust WASM returned an invalid beat count");
    }

    const times = Array.from(
      new Float64Array(exports.memory.buffer, timesPointer, count),
    );
    self.postMessage({ id: request.id, type: "result", times });
  } catch (error) {
    self.postMessage({
      id: request.id,
      type: "error",
      message: error instanceof Error ? error.message : "Beat detection failed",
    });
  } finally {
    if (timesPointer) exportsForFree?.oc_free(timesPointer);
    if (samplesPointer) exportsForFree?.oc_free(samplesPointer);
  }
}

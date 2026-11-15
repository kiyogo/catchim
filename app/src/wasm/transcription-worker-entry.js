import { env, pipeline } from "@huggingface/transformers";

env.allowRemoteModels = true;
env.allowLocalModels = false;
env.backends.onnx.wasm.wasmPaths = new URL("./", import.meta.url).href;
env.backends.onnx.wasm.numThreads = Math.max(
  1,
  Math.min(4, globalThis.navigator?.hardwareConcurrency || 2),
);

const CHUNK_LENGTH_SECONDS = 30;
const STRIDE_SECONDS = 5;
let transcriber = null;
let activeModelId = "";
let cancelled = false;

self.onmessage = async ({ data }) => {
  if (data?.type === "cancel") {
    cancelled = true;
    return;
  }
  if (data?.type !== "transcribe") return;

  const { requestId, audio, language, modelId } = data;
  cancelled = false;
  try {
    await ensureModel(modelId, requestId);
    if (cancelled) return;

    self.postMessage({ type: "transcribe-progress", requestId, progress: 0 });
    const raw = await transcriber(audio, {
      chunk_length_s: CHUNK_LENGTH_SECONDS,
      stride_length_s: STRIDE_SECONDS,
      language: language === "auto" ? undefined : language,
      return_timestamps: true,
    });
    if (cancelled) return;

    const result = Array.isArray(raw) ? raw[0] : raw;
    const segments = (result?.chunks ?? [])
      .filter((chunk) => Array.isArray(chunk.timestamp) && chunk.timestamp.length >= 2)
      .map((chunk) => ({
        text: String(chunk.text ?? "").trim(),
        start: Number(chunk.timestamp[0] ?? 0),
        end: Number(chunk.timestamp[1] ?? chunk.timestamp[0] ?? 0),
      }))
      .filter((segment) => segment.text && Number.isFinite(segment.start) &&
        Number.isFinite(segment.end) && segment.end > segment.start);

    self.postMessage({
      type: "transcribe-complete",
      requestId,
      payload: JSON.stringify({ text: String(result?.text ?? ""), segments }),
    });
  } catch (error) {
    if (cancelled) return;
    self.postMessage({
      type: "transcribe-error",
      requestId,
      error: error instanceof Error ? error.message : "Không thể chép lời âm thanh.",
    });
  }
};

async function ensureModel(modelId, requestId) {
  if (transcriber && activeModelId === modelId) {
    self.postMessage({ type: "init-complete", requestId });
    return;
  }

  transcriber = null;
  activeModelId = modelId;
  const files = new Map();
  try {
    transcriber = await pipeline("automatic-speech-recognition", modelId, {
      dtype: "q4",
      device: "auto",
      progress_callback: (info) => {
        if (!info.file) return;
        if (info.status === "progress" && info.total > 0) {
          files.set(info.file, { loaded: info.loaded ?? 0, total: info.total });
        } else if (info.status === "done") {
          const previous = files.get(info.file);
          if (previous) files.set(info.file, { loaded: previous.total, total: previous.total });
        }

        let loaded = 0;
        let total = 0;
        for (const file of files.values()) {
          loaded += file.loaded;
          total += file.total;
        }
        if (total > 0) {
          self.postMessage({
            type: "init-progress",
            requestId,
            progress: Math.floor((loaded / total) * 100),
          });
        }
      },
    });
    self.postMessage({ type: "init-complete", requestId });
  } catch (error) {
    activeModelId = "";
    throw error;
  }
}

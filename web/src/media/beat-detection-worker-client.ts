export interface DetectAudioBeatsWorkerRequest {
  id: number;
  type: "detect";
  samples: ArrayBuffer;
  samplesPerChannel: number;
  channelCount: number;
  sampleRate: number;
  startSeconds: number;
  endSeconds: number;
  maxBeats: number;
}

export type DetectAudioBeatsWorkerResponse =
  | { id: number; type: "result"; times: number[] }
  | { id: number; type: "error"; message: string };

interface PendingRequest {
  resolve: (times: number[]) => void;
  reject: (error: Error) => void;
}

let worker: Worker | null = null;
let nextRequestId = 1;
const pendingRequests = new Map<number, PendingRequest>();

function rejectPendingRequests(error: Error): void {
  for (const { reject } of pendingRequests.values()) {
    reject(error);
  }
  pendingRequests.clear();
}

function terminateWorker(error: Error): void {
  worker?.terminate();
  worker = null;
  rejectPendingRequests(error);
}

function handleWorkerMessage(
  event: MessageEvent<DetectAudioBeatsWorkerResponse>,
): void {
  const pending = pendingRequests.get(event.data.id);
  if (!pending) return;
  pendingRequests.delete(event.data.id);

  if (event.data.type === "error") {
    pending.reject(new Error(event.data.message));
  } else {
    pending.resolve(event.data.times);
  }
}

function getWorker(): Worker {
  if (worker) return worker;
  if (typeof Worker === "undefined") {
    throw new Error("Web Workers are not available");
  }

  worker = new Worker("/workers/beat-detection.worker.js", { type: "module" });
  worker.addEventListener("message", handleWorkerMessage);
  worker.addEventListener("error", (event) => {
    event.preventDefault();
    terminateWorker(new Error(event.message || "Beat detection worker failed"));
  });
  worker.addEventListener("messageerror", () => {
    terminateWorker(
      new Error("Beat detection worker returned an invalid message"),
    );
  });
  return worker;
}

export function detectAudioBeatsInWorker({
  samples,
  samplesPerChannel,
  channelCount,
  sampleRate,
  startSeconds,
  endSeconds,
  maxBeats,
}: Omit<DetectAudioBeatsWorkerRequest, "id" | "type">): Promise<number[]> {
  const activeWorker = getWorker();
  const id = nextRequestId++;

  return new Promise((resolve, reject) => {
    pendingRequests.set(id, { resolve, reject });
    try {
      activeWorker.postMessage(
        {
          id,
          type: "detect",
          samples,
          samplesPerChannel,
          channelCount,
          sampleRate,
          startSeconds,
          endSeconds,
          maxBeats,
        } satisfies DetectAudioBeatsWorkerRequest,
        [samples],
      );
    } catch (error) {
      pendingRequests.delete(id);
      reject(error instanceof Error ? error : new Error(String(error)));
    }
  });
}

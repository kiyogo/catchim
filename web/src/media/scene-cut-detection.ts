const ANALYSIS_WIDTH = 64;
const ANALYSIS_HEIGHT = 36;
const MAX_SAMPLES = 180;
const MIN_SAMPLE_SECONDS = 0.35;
const CUT_SCORE_THRESHOLD = 0.2;
const MIN_CUT_GAP_SECONDS = 1.25;

export interface SceneCutSuggestion {
  clipTimeSeconds: number;
  score: number;
}

function waitForVideoEvent({
  video,
  eventName,
}: {
  video: HTMLVideoElement;
  eventName: "loadedmetadata" | "loadeddata" | "seeked";
}): Promise<void> {
  return new Promise((resolve, reject) => {
    const timeout = window.setTimeout(() => {
      cleanup();
      reject(new Error("Video decoding took too long."));
    }, 15_000);
    const cleanup = () => {
      window.clearTimeout(timeout);
      video.removeEventListener(eventName, onReady);
      video.removeEventListener("error", onError);
    };
    const onReady = () => {
      cleanup();
      resolve();
    };
    const onError = () => {
      cleanup();
      reject(new Error("This video could not be decoded for scene detection."));
    };
    video.addEventListener(eventName, onReady, { once: true });
    video.addEventListener("error", onError, { once: true });
  });
}

function makeFrameSignature({
  video,
  canvas,
  context,
}: {
  video: HTMLVideoElement;
  canvas: HTMLCanvasElement;
  context: CanvasRenderingContext2D;
}): Uint8ClampedArray {
  context.drawImage(video, 0, 0, canvas.width, canvas.height);
  return context.getImageData(0, 0, canvas.width, canvas.height).data;
}

function compareFrames({
  previous,
  current,
}: {
  previous: Uint8ClampedArray;
  current: Uint8ClampedArray;
}): number {
  let difference = 0;
  let pixelCount = 0;
  for (let index = 0; index < previous.length; index += 4) {
    const previousLuma =
      previous[index] * 0.2126 +
      previous[index + 1] * 0.7152 +
      previous[index + 2] * 0.0722;
    const currentLuma =
      current[index] * 0.2126 +
      current[index + 1] * 0.7152 +
      current[index + 2] * 0.0722;
    const colorDifference =
      (Math.abs(current[index] - previous[index]) +
        Math.abs(current[index + 1] - previous[index + 1]) +
        Math.abs(current[index + 2] - previous[index + 2])) /
      (3 * 255);
    difference +=
      0.6 * (Math.abs(currentLuma - previousLuma) / 255) +
      0.4 * colorDifference;
    pixelCount++;
  }
  return pixelCount ? difference / pixelCount : 0;
}

/** Sample downscaled video frames and return strong, editable scene-cut suggestions. */
export async function detectSceneCuts({
  file,
  sourceStartSeconds,
  clipDurationSeconds,
  playbackRate = 1,
  onProgress,
}: {
  file: File;
  sourceStartSeconds: number;
  clipDurationSeconds: number;
  playbackRate?: number;
  onProgress?: (progress: number) => void;
}): Promise<SceneCutSuggestion[]> {
  if (
    !Number.isFinite(sourceStartSeconds) ||
    !Number.isFinite(clipDurationSeconds) ||
    !Number.isFinite(playbackRate) ||
    sourceStartSeconds < 0 ||
    clipDurationSeconds <= 0 ||
    playbackRate <= 0
  ) {
    throw new Error(
      "The selected clip has invalid timing for scene detection.",
    );
  }

  const objectUrl = URL.createObjectURL(file);
  const video = document.createElement("video");
  video.muted = true;
  video.playsInline = true;
  video.preload = "auto";
  const metadataLoaded = waitForVideoEvent({
    video,
    eventName: "loadedmetadata",
  });
  const firstFrameLoaded = waitForVideoEvent({
    video,
    eventName: "loadeddata",
  });
  video.src = objectUrl;

  try {
    await Promise.all([metadataLoaded, firstFrameLoaded]);
    if (
      !Number.isFinite(video.duration) ||
      video.videoWidth === 0 ||
      video.videoHeight === 0
    ) {
      throw new Error("The selected video has no readable frames.");
    }

    const canvas = document.createElement("canvas");
    canvas.width = ANALYSIS_WIDTH;
    canvas.height = ANALYSIS_HEIGHT;
    const context = canvas.getContext("2d", { willReadFrequently: true });
    if (!context) throw new Error("Could not create a frame analysis canvas.");

    const sampleInterval = Math.max(
      MIN_SAMPLE_SECONDS,
      clipDurationSeconds / MAX_SAMPLES,
    );
    const sampleCount = Math.min(
      MAX_SAMPLES,
      Math.max(2, Math.ceil(clipDurationSeconds / sampleInterval)),
    );
    const scores: Array<{ clipTimeSeconds: number; score: number }> = [];
    let previousSignature: Uint8ClampedArray | null = null;

    for (let sampleIndex = 0; sampleIndex <= sampleCount; sampleIndex++) {
      const clipTimeSeconds = Math.max(
        0,
        Math.min(clipDurationSeconds - 0.02, sampleIndex * sampleInterval),
      );
      if (clipTimeSeconds < 0 || clipTimeSeconds >= clipDurationSeconds)
        continue;

      const sourceTime = sourceStartSeconds + clipTimeSeconds * playbackRate;
      if (sourceTime >= video.duration) break;
      if (Math.abs(video.currentTime - sourceTime) > 0.005) {
        const seeked = waitForVideoEvent({ video, eventName: "seeked" });
        video.currentTime = sourceTime;
        await seeked;
      }

      const signature = makeFrameSignature({ video, canvas, context });
      if (previousSignature) {
        scores.push({
          clipTimeSeconds,
          score: compareFrames({
            previous: previousSignature,
            current: signature,
          }),
        });
      }
      previousSignature = signature;
      onProgress?.((sampleIndex + 1) / (sampleCount + 1));
    }

    const suggestions: SceneCutSuggestion[] = [];
    for (let index = 0; index < scores.length; index++) {
      const current = scores[index];
      const previous = scores[index - 1]?.score ?? 0;
      const next = scores[index + 1]?.score ?? 0;
      const isPeak = current.score >= previous && current.score > next;
      const lastSuggestion = suggestions.at(-1);
      if (
        current.score >= CUT_SCORE_THRESHOLD &&
        isPeak &&
        (!lastSuggestion ||
          current.clipTimeSeconds - lastSuggestion.clipTimeSeconds >=
            MIN_CUT_GAP_SECONDS)
      ) {
        suggestions.push(current);
      }
    }
    return suggestions;
  } finally {
    video.pause();
    video.removeAttribute("src");
    video.load();
    URL.revokeObjectURL(objectUrl);
  }
}

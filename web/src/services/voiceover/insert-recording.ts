import type { EditorCore } from "@/core";
import { buildElementFromMedia } from "@/timeline/element-utils";
import { mediaTimeFromSeconds, type MediaTime } from "@/wasm";

/** Persist a browser recording and insert it as an upload-backed audio clip. */
export async function insertVoiceoverRecording({
  editor,
  projectId,
  sceneId,
  startTime,
  blob,
  durationSeconds,
  name,
}: {
  editor: EditorCore;
  projectId: string;
  sceneId: string;
  startTime: MediaTime;
  blob: Blob;
  durationSeconds: number;
  name: string;
}): Promise<void> {
  let assetUrl: string | null = null;
  let persisted = false;
  const requireOriginalScene = () => {
    if (
      editor.project.getActiveOrNull()?.metadata.id !== projectId ||
      editor.scenes.getActiveSceneOrNull()?.id !== sceneId
    ) {
      throw new Error(
        "Dự án hoặc cảnh đã thay đổi trong khi thu âm. Bản ghi đã lưu trong thư viện media.",
      );
    }
  };

  try {
    requireOriginalScene();
    if (
      !Number.isFinite(durationSeconds) ||
      durationSeconds <= 0 ||
      blob.size === 0
    ) {
      throw new Error("Bản ghi voiceover không có dữ liệu hợp lệ.");
    }

    const mimeType = blob.type || "audio/webm";
    const extension = mimeType.includes("mp4")
      ? "m4a"
      : mimeType.includes("ogg")
        ? "ogg"
        : "webm";
    const file = new File([blob], `${name}.${extension}`, { type: mimeType });
    assetUrl = URL.createObjectURL(file);
    const asset = await editor.media.addMediaAsset({
      projectId,
      asset: {
        name,
        type: "audio",
        file,
        url: assetUrl,
        duration: durationSeconds,
        hasAudio: true,
      },
    });
    if (!asset) {
      throw new Error("Không thể lưu voiceover vào thư viện media.");
    }
    persisted = true;

    requireOriginalScene();
    editor.timeline.insertElement({
      placement: { mode: "auto", trackType: "audio" },
      element: buildElementFromMedia({
        mediaId: asset.id,
        mediaType: "audio",
        name,
        duration: mediaTimeFromSeconds({ seconds: durationSeconds }),
        startTime,
      }),
    });
  } finally {
    if (assetUrl && !persisted) URL.revokeObjectURL(assetUrl);
  }
}

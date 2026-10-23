import type { EditorCore } from "@/core";
import { buildElementFromMedia } from "@/timeline/element-utils";
import { mediaTimeFromSeconds, type MediaTime } from "@/wasm";
import type { TtsSynthesisResult } from "./types";

/** Persist generated audio before creating a clip that can survive a reload. */
export async function insertGeneratedSpeech({
	editor,
	projectId,
	sceneId,
	result,
	name,
	startTime,
	beforeInsert,
}: {
	editor: EditorCore;
	projectId: string;
	sceneId: string;
	result: TtsSynthesisResult;
	name: string;
	startTime: MediaTime;
	beforeInsert?: () => void;
}): Promise<void> {
	let assetUrl: string | null = null;
	let persisted = false;

	const requireOriginalScene = () => {
		if (
			editor.project.getActiveOrNull()?.metadata.id !== projectId ||
			editor.scenes.getActiveSceneOrNull()?.id !== sceneId
		) {
			throw new Error(
				"Dự án hoặc cảnh đã thay đổi trong khi tạo giọng đọc. Vui lòng thử lại.",
			);
		}
	};

	try {
		requireOriginalScene();
		if (!Number.isFinite(result.durationSeconds) || result.durationSeconds <= 0) {
			throw new Error("Âm thanh giọng đọc không có thời lượng hợp lệ.");
		}

		const response = await fetch(result.blobUrl);
		if (!response.ok) {
			throw new Error("Không thể đọc âm thanh giọng đọc đã tạo.");
		}
		const blob = await response.blob();
		if (blob.size === 0) {
			throw new Error("Âm thanh giọng đọc đã tạo bị trống.");
		}

		requireOriginalScene();
		const file = new File([blob], `${name}.wav`, { type: "audio/wav" });
		assetUrl = URL.createObjectURL(file);
		const asset = await editor.media.addMediaAsset({
			projectId,
			asset: {
				name,
				type: "audio",
				file,
				url: assetUrl,
				duration: result.durationSeconds,
				hasAudio: true,
			},
		});
		if (!asset) {
			throw new Error("Không thể lưu giọng đọc vào bộ nhớ trình duyệt.");
		}
		persisted = true;

		requireOriginalScene();
		beforeInsert?.();
		editor.timeline.insertElement({
			placement: { mode: "auto", trackType: "audio" },
			element: buildElementFromMedia({
				mediaId: asset.id,
				mediaType: "audio",
				name,
				duration: mediaTimeFromSeconds({ seconds: result.durationSeconds }),
				startTime,
			}),
		});
	} finally {
		URL.revokeObjectURL(result.blobUrl);
		if (assetUrl && !persisted) {
			URL.revokeObjectURL(assetUrl);
		}
	}
}

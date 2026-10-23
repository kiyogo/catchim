import { afterEach, beforeEach, describe, expect, test, vi } from "vitest";
import type { EditorCore } from "@/core";
import type { MediaAsset } from "@/media/types";
import { collectAudioMixSources } from "@/media/audio";
import { StorageService } from "@/services/storage/service";
import { IndexedDBAdapter } from "@/services/storage/indexeddb-adapter";
import { OPFSAdapter } from "@/services/storage/opfs-adapter";
import type { AudioElement, CreateTimelineElement, SceneTracks } from "@/timeline";
import { buildEmptyTrack } from "@/timeline/placement";
import { mediaTimeFromSeconds } from "@/wasm";
import { insertGeneratedSpeech } from "./insert-generated-speech";
import type { TtsSynthesisResult } from "./types";

vi.mock("@/services/storage/migrations", () => ({
	migrations: [],
	runStorageMigrations: vi.fn(async () => undefined),
}));
vi.mock("@/timeline/scenes", () => ({
	getProjectDurationFromScenes: vi.fn(),
}));

describe("generated speech persistence", () => {
	const metadata = new Map<string, unknown>();
	const files = new Map<string, File>();
	const urls: string[] = [];
	const startTime = mediaTimeFromSeconds({ seconds: 3 });
	const name = "[TTS] Voice - Xin chào";

	beforeEach(() => {
		metadata.clear();
		files.clear();
		urls.length = 0;
		const createObjectURL = URL.createObjectURL.bind(URL);
		vi.spyOn(URL, "createObjectURL").mockImplementation((blob) => {
			const url = createObjectURL(blob);
			urls.push(url);
			return url;
		});
		vi.spyOn(URL, "revokeObjectURL");
		vi.spyOn(IndexedDBAdapter.prototype, "set").mockImplementation(async ({ key, value }) => {
			metadata.set(key, structuredClone(value));
		});
		vi.spyOn(IndexedDBAdapter.prototype, "get").mockImplementation(async (key) =>
			structuredClone(metadata.get(key) ?? null),
		);
		vi.spyOn(OPFSAdapter.prototype, "set").mockImplementation(async ({ key, value }) => {
			files.set(key, value);
		});
		vi.spyOn(OPFSAdapter.prototype, "get").mockImplementation(async (key) => files.get(key) ?? null);
	});

	afterEach(() => {
		for (const url of urls) URL.revokeObjectURL(url);
		vi.restoreAllMocks();
	});

	function buildResult(): TtsSynthesisResult {
		return {
			blobUrl: URL.createObjectURL(new Blob(["RIFF generated WAV bytes"], { type: "audio/wav" })),
			durationSeconds: 2.5,
			audioBuffer: {} as AudioBuffer,
		};
	}

	function buildEditor() {
		const storage = new StorageService();
		const state = { projectId: "project-1", sceneId: "scene-1" };
		const assets: MediaAsset[] = [];
		const inserted: CreateTimelineElement[] = [];
		const addMediaAsset = vi.fn(async ({ projectId, asset }: {
			projectId: string;
			asset: Omit<MediaAsset, "id">;
		}): Promise<MediaAsset | null> => {
			const savedAsset = { ...asset, id: "speech-asset" };
			await storage.saveMediaAsset({ projectId, mediaAsset: savedAsset });
			assets.push(savedAsset);
			return savedAsset;
		});
		const insertElement = vi.fn(({ element }: { element: CreateTimelineElement }) => {
			inserted.push(element);
		});
		const editor = {
			project: { getActiveOrNull: () => ({ metadata: { id: state.projectId } }) },
			scenes: { getActiveSceneOrNull: () => ({ id: state.sceneId }) },
			media: { addMediaAsset, getAssets: () => assets },
			timeline: { insertElement },
		} as unknown as EditorCore;
		return { editor, addMediaAsset, insertElement, state, assets, inserted };
	}

	test("reopens generated audio and supplies the export mix after original URLs and buffers are gone", async () => {
		const { editor, inserted } = buildEditor();
		const result = buildResult();
		await insertGeneratedSpeech({
			editor, projectId: "project-1", sceneId: "scene-1", result, name, startTime,
		});

		expect(inserted[0]).toMatchObject({
			type: "audio", sourceType: "upload", mediaId: "speech-asset", name, startTime,
			duration: mediaTimeFromSeconds({ seconds: 2.5 }),
		});
		expect(inserted[0]).not.toHaveProperty("sourceUrl");
		expect(inserted[0]).not.toHaveProperty("buffer");
		expect(URL.revokeObjectURL).toHaveBeenCalledWith(result.blobUrl);
		for (const url of [...urls]) URL.revokeObjectURL(url);

		const reopenedAsset = await new StorageService().loadMediaAsset({
			projectId: "project-1", id: "speech-asset",
		});
		expect(reopenedAsset?.file.type).toBe("audio/wav");
		expect(await reopenedAsset?.file.text()).toBe("RIFF generated WAV bytes");
		const reopenedClip = JSON.parse(JSON.stringify({ ...inserted[0], id: "speech-clip" })) as AudioElement;
		const tracks: SceneTracks = {
			main: buildEmptyTrack({ id: "main", type: "video" }), overlay: [],
			audio: [{ ...buildEmptyTrack({ id: "audio", type: "audio" }), elements: [reopenedClip] }],
		};
		const sources = await collectAudioMixSources({ tracks, mediaAssets: [reopenedAsset!] });
		expect(sources).toHaveLength(1);
		expect(sources[0].startTime).toBe(3);
		expect(await sources[0].file.text()).toBe("RIFF generated WAV bytes");
	});

	test("waits for durable media storage before inserting the clip", async () => {
		const { editor, addMediaAsset, insertElement } = buildEditor();
		const beforeInsert = vi.fn();
		let finishSave!: (asset: MediaAsset) => void;
		const saved = new Promise<MediaAsset>((resolve) => { finishSave = resolve; });
		addMediaAsset.mockImplementation(async () => saved);
		const insertion = insertGeneratedSpeech({
			editor, projectId: "project-1", sceneId: "scene-1", result: buildResult(), name, startTime, beforeInsert,
		});
		await vi.waitFor(() => expect(addMediaAsset).toHaveBeenCalledOnce());
		expect(beforeInsert).not.toHaveBeenCalled();
		expect(insertElement).not.toHaveBeenCalled();
		finishSave({ ...addMediaAsset.mock.calls[0][0].asset, id: "saved-asset" });
		await insertion;
		expect(insertElement).toHaveBeenCalledOnce();
		expect(beforeInsert).toHaveBeenCalledOnce();
		expect(beforeInsert.mock.invocationCallOrder[0]).toBeLessThan(insertElement.mock.invocationCallOrder[0]);
	});

	test.each(["null", "throw"])("does not insert broken audio when media storage returns %s", async (failure) => {
		const { editor, addMediaAsset, insertElement } = buildEditor();
		const beforeInsert = vi.fn();
		if (failure === "null") addMediaAsset.mockResolvedValue(null);
		else addMediaAsset.mockRejectedValue(new Error("disk failure"));
		const result = buildResult();

		await expect(insertGeneratedSpeech({
			editor, projectId: "project-1", sceneId: "scene-1", result, name, startTime, beforeInsert,
		})).rejects.toThrow(failure === "null" ? "Không thể lưu" : "disk failure");

		expect(insertElement).not.toHaveBeenCalled();
		expect(beforeInsert).not.toHaveBeenCalled();
		for (const url of urls) {
			expect(URL.revokeObjectURL).toHaveBeenCalledWith(url);
			await expect(fetch(url)).rejects.toThrow();
		}
	});

	test("does not insert into a different scene after an asynchronous save", async () => {
		const { editor, addMediaAsset, insertElement, state } = buildEditor();
		addMediaAsset.mockImplementation(async ({ asset }) => {
			state.sceneId = "another-scene";
			return { ...asset, id: "saved-asset" };
		});
		const result = buildResult();
		await expect(insertGeneratedSpeech({
			editor, projectId: "project-1", sceneId: "scene-1", result, name, startTime,
		})).rejects.toThrow("Dự án hoặc cảnh đã thay đổi");
		expect(insertElement).not.toHaveBeenCalled();
		expect(URL.revokeObjectURL).toHaveBeenCalledWith(result.blobUrl);
	});

	test("rejects an unreadable synthesis URL and releases it without creating an asset", async () => {
		const { editor, addMediaAsset, insertElement } = buildEditor();
		const result = buildResult();
		URL.revokeObjectURL(result.blobUrl);
		await expect(insertGeneratedSpeech({
			editor, projectId: "project-1", sceneId: "scene-1", result, name, startTime,
		})).rejects.toThrow();
		expect(addMediaAsset).not.toHaveBeenCalled();
		expect(insertElement).not.toHaveBeenCalled();
		expect(URL.revokeObjectURL).toHaveBeenCalledWith(result.blobUrl);
	});
});

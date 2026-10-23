import { afterEach, beforeEach, describe, expect, test, vi } from "vitest";
import type { MediaAsset } from "@/media/types";
import { IndexedDBAdapter } from "./indexeddb-adapter";
import { OPFSAdapter } from "./opfs-adapter";
import { StorageService } from "./service";
import type { MediaAssetData } from "./types";
import type { TProject } from "@/project/types";
import { buildEmptyTrack } from "@/timeline/placement";
import { floatToFrameRate } from "@/fps/utils";
import { ZERO_MEDIA_TIME } from "@/wasm";

const { metadataStores, fileStores } = vi.hoisted(() => ({
	metadataStores: new Map<string, Map<string, unknown>>(),
	fileStores: new Map<string, Map<string, File>>(),
}));

vi.mock("./indexeddb-adapter", () => ({
	IndexedDBAdapter: class {
		private store: Map<string, unknown>;

		constructor({ dbName, storeName }: { dbName: string; storeName: string }) {
			const namespace = `${dbName}/${storeName}`;
			if (!metadataStores.has(namespace)) {
				metadataStores.set(namespace, new Map());
			}
			this.store = metadataStores.get(namespace)!;
		}

		async set({ key, value }: { key: string; value: unknown }) {
			this.store.set(key, structuredClone(value));
		}

		async get(key: string) {
			return structuredClone(this.store.get(key) ?? null);
		}
	},
}));

vi.mock("./opfs-adapter", () => ({
	OPFSAdapter: class {
		private store: Map<string, File>;

		constructor(directoryName: string) {
			if (!fileStores.has(directoryName)) {
				fileStores.set(directoryName, new Map());
			}
			this.store = fileStores.get(directoryName)!;
		}

		async set({ key, value }: { key: string; value: File }) {
			this.store.set(key, value);
		}

		async get(key: string) {
			return this.store.get(key) ?? null;
		}

		async remove(key: string) { this.store.delete(key); }
	},
}));

// Project migration and timeline calculations are outside media persistence.
vi.mock("@/services/storage/migrations", () => ({
	migrations: [],
	runStorageMigrations: vi.fn(async () => undefined),
}));
vi.mock("@/timeline/scenes", () => ({
	getProjectDurationFromScenes: vi.fn(),
}));

describe("media asset persistence", () => {
	const projectId = "project-1";

	beforeEach(() => {
		metadataStores.clear();
		fileStores.clear();
		vi.spyOn(URL, "createObjectURL").mockReturnValue("blob:reopened-video");
	});

	afterEach(() => {
		vi.restoreAllMocks();
	});

	test.each([
		{ fps: 30000 / 1001, hasAudio: false },
		{ fps: 24000 / 1001, hasAudio: true },
	])("preserves fps $fps and hasAudio $hasAudio after reopening", async ({ fps, hasAudio }) => {
		const mediaAsset: MediaAsset = {
			id: "video-1",
			name: "source.mp4",
			type: "video",
			file: new File(["video bytes"], "source.mp4", {
				type: "video/mp4",
				lastModified: 1700000000000,
			}),
			url: "blob:imported-video",
			width: 1920,
			height: 1080,
			duration: 12.5,
			fps,
			hasAudio,
		};

		await new StorageService().saveMediaAsset({ projectId, mediaAsset });
		const metadataAdapter = new IndexedDBAdapter<MediaAssetData>({
			dbName: `video-editor-media-${projectId}`,
			storeName: "media-metadata",
		});
		expect(await metadataAdapter.get(mediaAsset.id)).toMatchObject({
			fps,
			hasAudio,
		});

		const reopened = await new StorageService().loadMediaAsset({
			projectId,
			id: mediaAsset.id,
		});

		expect(reopened).toEqual({
			...mediaAsset,
			url: "blob:reopened-video",
			thumbnailUrl: undefined,
			ephemeral: undefined,
			binId: undefined,
			tags: [],
			favorite: false,
		});
		expect(URL.createObjectURL).toHaveBeenCalledWith(mediaAsset.file);
	});

	test("loads legacy media without inventing missing fps or audio metadata", async () => {
		const file = new File(["legacy bytes"], "legacy.mp4", {
			type: "video/mp4",
		});
		const metadata: MediaAssetData = {
			id: "legacy-video",
			name: file.name,
			type: "video",
			size: file.size,
			lastModified: file.lastModified,
			duration: 5,
		};
		await new IndexedDBAdapter<MediaAssetData>({
			dbName: `video-editor-media-${projectId}`,
			storeName: "media-metadata",
		}).set({ key: metadata.id, value: metadata });
		await new OPFSAdapter(`media-files-${projectId}`).set({
			key: metadata.id,
			value: file,
		});

		const reopened = await new StorageService().loadMediaAsset({
			projectId,
			id: metadata.id,
		});

		expect(reopened).toMatchObject({
			id: metadata.id,
			name: file.name,
			type: "video",
			file,
			duration: 5,
			url: "blob:reopened-video",
		});
		expect(reopened?.fps).toBeUndefined();
		expect(reopened?.hasAudio).toBeUndefined();
		expect(reopened?.tags).toEqual([]);
		expect(reopened?.favorite).toBe(false);
	});

	test("preserves folder, tags and favorite through source replacement, offline state and relink", async () => {
		const storage = new StorageService();
		const original: MediaAsset = { id:"organized",name:"Voice",type:"audio",duration:3,file:new File(["original"],"voice.wav"),binId:"voice-bin",tags:["voice"],favorite:true };
		await storage.saveMediaAsset({projectId,mediaAsset:original});
		await storage.saveMediaLibrarySnapshot({projectId,snapshot:{bins:[{id:"voice-bin",name:"Voiceovers"}],assets:[{id:original.id,binId:"voice-bin",tags:["voice","edited"],favorite:true}]}});
		const reopened = (await new StorageService().loadMediaAsset({projectId,id:original.id}))!;
		expect(reopened).toMatchObject({binId:"voice-bin",tags:["voice","edited"],favorite:true});
		expect(await new StorageService().loadMediaLibrary({projectId})).toEqual({bins:[{id:"voice-bin",name:"Voiceovers"}]});
		await storage.replaceMediaAsset({projectId,mediaAsset:{...reopened,file:new File(["replacement"],"new.wav")}});
		await storage.saveMissingMediaAsset({projectId,mediaAsset:{...reopened,missing:true}});
		const missing = await storage.loadMediaAsset({projectId,id:original.id});
		expect(missing).toMatchObject({missing:true,binId:"voice-bin",tags:["voice","edited"],favorite:true});
		await storage.relinkMediaAsset({projectId,mediaAsset:{...original,file:new File(["relinked"],"relink.wav")}});
		expect(await storage.loadMediaAsset({projectId,id:original.id})).toMatchObject({binId:"voice-bin",tags:["voice","edited"],favorite:true});
	});

	test("preserves track lock state through project save and reopen", async () => {
		const now = new Date("2026-10-22T00:00:00.000Z");
		const project: TProject = {
			metadata: {
				id: "locked-project",
				name: "Locked project",
				duration: ZERO_MEDIA_TIME,
				createdAt: now,
				updatedAt: now,
			},
			scenes: [
				{
					id: "scene-1",
					name: "Main scene",
					isMain: true,
					tracks: {
						main: {
							...buildEmptyTrack({ id: "main-track", type: "video" }),
							locked: true,
						},
						overlay: [],
						audio: [],
					},
					bookmarks: [],
					createdAt: now,
					updatedAt: now,
				},
			],
			currentSceneId: "scene-1",
			settings: {
				fps: floatToFrameRate(30),
				canvasSize: { width: 1920, height: 1080 },
				background: { type: "color", color: "#000000" },
			},
			version: 1,
		};

		const storage = new StorageService();
		await storage.saveProject({ project });
		const reopened = await storage.loadProject({ id: project.metadata.id });

		expect(reopened?.project.scenes[0].tracks.main.locked).toBe(true);
	});
});

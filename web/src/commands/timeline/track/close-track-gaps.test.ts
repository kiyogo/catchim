import { beforeEach, describe, expect, test, vi } from "vitest";
import type { EditorCore } from "@/core";
import { CommandManager } from "@/core/managers/commands";
import { TimelineManager } from "@/core/managers/timeline-manager";
import { CloseTrackGapsCommand } from "./close-track-gaps";
import { MoveElementCommand } from "../element/move-elements";
import { buildEmptyTrack } from "@/timeline/placement/track-factory";
import type {
	SceneTracks,
	TimelineTrack,
	VideoElement,
} from "@/timeline/types";
import type {
	EditorSelectionPatch,
	EditorSelectionSnapshot,
} from "@/selection/editor-selection";
import { mediaTimeFromSeconds, ZERO_MEDIA_TIME } from "@/wasm";
import { buildScene } from "@/services/renderer/scene-builder";
import { VideoNode } from "@/services/renderer/nodes/video-node";
import { collectAudioClips } from "@/media/audio";
import type { MediaAsset } from "@/media/types";
import type { TProject } from "@/project/types";
import { StorageService } from "@/services/storage/service";

const state = vi.hoisted(() => ({
	editor: null as unknown,
	projects: new Map<string, unknown>(),
}));
vi.mock("@/core", () => ({
	EditorCore: { getInstance: () => state.editor },
}));
vi.mock("@/native/rust-engine", async (importOriginal) => ({
	...await importOriginal<typeof import("@/native/rust-engine")>(),
	getRustTrackGapBindings: () => null,
}));
vi.mock("@/services/storage/indexeddb-adapter", () => ({
	IndexedDBAdapter: class {
		async set({ key, value }: { key: string; value: unknown }) {
			state.projects.set(key, structuredClone(value));
		}
		async get(key: string) {
			return structuredClone(state.projects.get(key) ?? null);
		}
	},
}));
vi.mock("@/services/storage/opfs-adapter", () => ({ OPFSAdapter: class {} }));
vi.mock("@/services/storage/migrations", () => ({
	migrations: [],
	runStorageMigrations: vi.fn(async () => undefined),
}));

const at = (seconds: number) => mediaTimeFromSeconds({ seconds });

function video(id: string, start: number): VideoElement {
	return {
		id,
		type: "video",
		name: id,
		mediaId: "source-media",
		startTime: at(start),
		duration: at(5),
		trimStart: at(2),
		trimEnd: at(3),
		sourceDuration: at(12.5),
		retime: { rate: 1.5 },
		params: { opacity: 1, volume: -6 },
		animations: {
			opacity: {
				keys: [{ id: `${id}-key`, time: at(1), value: 0.7, segmentToNext: "linear", tangentMode: "auto" }],
			},
		},
	};
}

function createHarness(target: TimelineTrack) {
	let tracks: SceneTracks = {
		main: target.id === "main"
			? target as SceneTracks["main"]
			: buildEmptyTrack({ id: "main", type: "video" }),
		overlay: target.id !== "main" && target.type !== "audio" ? [target] : [],
		audio: target.type === "audio" ? [target] : [],
	};
	let selection: EditorSelectionSnapshot = {
		selectedElements: target.elements.map((element) => ({ trackId: target.id, elementId: element.id })),
		selectedKeyframes: [],
		keyframeSelectionAnchor: null,
		selectedMaskPoints: null,
	};
	const editor = {
		scenes: {
			getActiveScene: () => ({ tracks }),
			getActiveSceneOrNull: () => ({ tracks }),
			updateSceneTracks: ({ tracks: next }: { tracks: SceneTracks }) => { tracks = next; },
		},
		selection: {
			getSnapshot: () => selection,
			applySelectionPatch: ({ patch }: { patch: EditorSelectionPatch }) => {
				selection = { ...selection, ...patch };
				return selection;
			},
			restoreSnapshot: ({ snapshot }: { snapshot: EditorSelectionSnapshot }) => { selection = snapshot; },
		},
		timeline: null as unknown as TimelineManager,
		command: null as unknown as CommandManager,
	};
	state.editor = editor;
	editor.timeline = new TimelineManager(editor as unknown as EditorCore);
	editor.command = new CommandManager(editor as unknown as EditorCore);
	return { editor, tracks: () => tracks, selection: () => selection };
}

function targetTrack(id = "target") {
	return {
		...buildEmptyTrack({ id, type: "video" }),
		elements: [video("late", 30), video("first", 10), video("middle", 20)],
	};
}

beforeEach(() => {
	state.editor = null;
	state.projects.clear();
});

describe("CloseTrackGapsCommand", () => {
	test.each([false, true])("one history entry closes gaps with ripple=%s without shifting twice", (ripple) => {
		const harness = createHarness(targetTrack());
		harness.editor.command.isRippleEnabled = ripple;
		const before = harness.tracks();
		const selection = harness.selection();
		const reactor = vi.fn();
		harness.editor.command.registerReactor(reactor);
		harness.editor.timeline.closeTrackGaps({ trackId: "target" });
		const after = harness.tracks();
		expect(after.overlay[0].elements.map((element) => element.startTime)).toEqual([at(20), at(10), at(15)]);
		for (let index = 0; index < 3; index++) {
			expect(after.overlay[0].elements[index]).toEqual({
				...before.overlay[0].elements[index],
				startTime: [at(20), at(10), at(15)][index],
			});
		}
		expect(harness.selection()).toEqual(selection);
		expect(reactor).toHaveBeenCalledTimes(1);
		harness.editor.command.undo();
		expect(harness.tracks()).toEqual(before);
		expect(harness.editor.command.canUndo()).toBe(false);
		harness.editor.command.redo();
		expect(harness.tracks()).toEqual(after);
		expect(reactor).toHaveBeenCalledTimes(2);
		expect(harness.selection()).toEqual(selection);
	});

	test("a following move still addresses the same clip after undo twice and redo twice", () => {
		const harness = createHarness(targetTrack());
		harness.editor.timeline.closeTrackGaps({ trackId: "target" });
		harness.editor.command.execute({ command: new MoveElementCommand({ moves: [{
			sourceTrackId: "target", targetTrackId: "target", elementId: "late", newStartTime: at(40),
		}] }) });
		const after = harness.tracks();
		harness.editor.command.undo();
		harness.editor.command.undo();
		harness.editor.command.redo();
		harness.editor.command.redo();
		expect(harness.tracks()).toEqual(after);
	});

	test("locked tracks are blocked by both the manager and command boundary", () => {
		const harness = createHarness({ ...targetTrack(), locked: true });
		const before = harness.tracks();
		harness.editor.timeline.closeTrackGaps({ trackId: "target" });
		expect(harness.editor.command.canUndo()).toBe(false);
		new CloseTrackGapsCommand("target").execute();
		expect(harness.tracks()).toBe(before);
	});

	test("compacts the main track while preserving its leading gap", () => {
		const harness = createHarness(targetTrack("main"));
		harness.editor.timeline.closeTrackGaps({ trackId: "main" });
		expect(harness.tracks().main.elements.map((element) => element.startTime)).toEqual([at(20), at(10), at(15)]);
	});

	test("supports every timeline element kind without changing source, content, or duration", () => {
		const base = video("item", 10);
		const pairs: TimelineTrack[] = [
			{ ...buildEmptyTrack({ id: "video", type: "video" }), elements: [base, { ...video("image", 30), type: "image" }] },
			{ ...buildEmptyTrack({ id: "text", type: "text" }), elements: [
				{ ...base, type: "text", params: { content: "Xin chào Việt Nam" } },
				{ ...base, id: "title2", type: "text", startTime: at(30), params: { content: "Title 2" } },
			] },
			{ ...buildEmptyTrack({ id: "graphic", type: "graphic" }), elements: [
				{ ...base, type: "sticker", stickerId: "test:shape" },
				{ ...base, id: "shape", type: "graphic", startTime: at(30), definitionId: "rectangle" },
			] },
			{ ...buildEmptyTrack({ id: "effect", type: "effect" }), elements: [
				{ ...base, type: "effect", effectType: "blur" },
				{ ...base, id: "blur2", type: "effect", startTime: at(30), effectType: "blur" },
			] },
			{ ...buildEmptyTrack({ id: "audio", type: "audio" }), elements: [
				{ ...base, type: "audio", sourceType: "upload" },
				{ ...base, id: "music", type: "audio", sourceType: "library", sourceUrl: "https://example.test/music.wav", startTime: at(30) },
			] },
		];
		for (const target of pairs) {
			const harness = createHarness(target);
			harness.editor.timeline.closeTrackGaps({ trackId: target.id });
			const after = harness.editor.timeline.getTrackById({ trackId: target.id })!;
			expect(after.elements[0]).toEqual(target.elements[0]);
			expect(after.elements[1]).toEqual({ ...target.elements[1], startTime: at(15) });
		}
	});

	test.each([
		{ elements: [] },
		{ elements: [video("only", 10)] },
		{ elements: [video("a", 10), video("b", 15)] },
		{ elements: [video("a", 10), { ...video("bad", 30), duration: ZERO_MEDIA_TIME }] },
	])(
		"empty, single, contiguous, or invalid tracks remain unchanged",
		({ elements }) => {
			const harness = createHarness({ ...targetTrack(), elements });
			const before = harness.tracks();
			new CloseTrackGapsCommand("target").execute();
			expect(harness.tracks()).toBe(before);
			harness.editor.timeline.closeTrackGaps({ trackId: "target" });
			expect(harness.editor.command.canUndo()).toBe(false);
		},
	);

	test("changed positions reach preview/export scene and audio plans, and survive project save/reopen", async () => {
		const harness = createHarness(targetTrack());
		harness.editor.timeline.closeTrackGaps({ trackId: "target" });
		const tracks = harness.tracks();
		const media: MediaAsset = {
			id: "source-media", name: "source.mp4", type: "video",
			file: new File(["source"], "source.mp4", { type: "video/mp4" }),
			url: "blob:source", hasAudio: true, duration: 12.5, width: 1920, height: 1080,
		};
		for (const isPreview of [true, false]) {
			const scene = buildScene({
				canvasSize: { width: 1920, height: 1080 }, tracks,
				mediaAssets: [media], duration: at(25),
				background: { type: "color", color: "transparent" }, isPreview,
			});
			const nodes = scene.children.filter((node): node is VideoNode => node instanceof VideoNode);
			expect(nodes.map((node) => node.params.timeOffset)).toEqual([at(10), at(15), at(20)]);
			expect(nodes.every((node) => node.params.trimStart === at(2) && node.params.retime?.rate === 1.5)).toBe(true);
		}
		const audioClips = await collectAudioClips({ tracks, mediaAssets: [media] });
		expect(audioClips.map((clip) => clip.startTime)).toEqual([20, 10, 15]);
		expect(audioClips.every((clip) => clip.trimStart === 2 && clip.retime?.rate === 1.5)).toBe(true);
		const now = new Date("2026-10-22T00:00:00Z");
		const project: TProject = {
			metadata: { id: "gap-project", name: "Gap project", duration: at(25), createdAt: now, updatedAt: now },
			scenes: [{ id: "scene", name: "Scene", isMain: true, tracks, bookmarks: [], createdAt: now, updatedAt: now }],
			currentSceneId: "scene", version: 1,
			settings: { fps: { numerator: 30, denominator: 1 }, canvasSize: { width: 1920, height: 1080 }, background: { type: "color", color: "#000000" } },
		};
		const storage = new StorageService();
		await storage.saveProject({ project });
		const reopened = await storage.loadProject({ id: "gap-project" });
		expect(reopened?.project.scenes[0].tracks).toEqual(tracks);
	});
});

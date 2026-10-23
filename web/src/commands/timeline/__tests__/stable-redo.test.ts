import { beforeEach, describe, expect, test, vi } from "vitest";
import type { EditorCore } from "@/core";
import { CommandManager } from "@/core/managers/commands";
import { SplitElementsCommand } from "@/commands/timeline/element/split-elements";
import { DuplicateElementsCommand } from "@/commands/timeline/element/duplicate-elements";
import { PasteCommand } from "@/commands/timeline/clipboard/paste";
import { MoveElementCommand } from "@/commands/timeline/element/move-elements";
import { InsertElementCommand } from "@/commands/timeline/element/insert-element";
import { ToggleSourceAudioSeparationCommand } from "@/commands/timeline/element/toggle-source-audio-separation";
import { PasteKeyframesCommand } from "@/commands/timeline/clipboard/paste-keyframes";
import { RetimeKeyframeCommand } from "@/commands/timeline/element/keyframes/retime-keyframe";
import { getKeyframeAtTime } from "@/animation";
import type {
	EditorSelectionPatch,
	EditorSelectionSnapshot,
} from "@/selection/editor-selection";
import type {
	ElementRef,
	SceneTracks,
	TimelineElement,
	VideoElement,
} from "@/timeline";
import { mediaTimeFromSeconds, ZERO_MEDIA_TIME } from "@/wasm";

const editorState = vi.hoisted(() => ({ current: null as unknown }));

vi.mock("@/core", () => ({
	EditorCore: { getInstance: () => editorState.current },
}));

const at = (seconds: number) => mediaTimeFromSeconds({ seconds });

function video(): VideoElement {
	return {
		id: "source-clip",
		type: "video",
		name: "Source",
		mediaId: "source-media",
		startTime: ZERO_MEDIA_TIME,
		duration: at(10),
		sourceDuration: at(10),
		trimStart: ZERO_MEDIA_TIME,
		trimEnd: ZERO_MEDIA_TIME,
		params: { opacity: 1 },
		animations: {
			opacity: {
				keys: [
					{
						id: "key-start",
						time: at(0),
						value: 0,
						segmentToNext: "linear",
						tangentMode: "auto",
					},
					{
						id: "key-end",
						time: at(10),
						value: 1,
						segmentToNext: "linear",
						tangentMode: "auto",
					},
				],
			},
			volume: {
				keys: [
					{
						id: "volume-start",
						time: at(0),
						value: -6,
						segmentToNext: "linear",
						tangentMode: "auto",
					},
					{
						id: "volume-end",
						time: at(10),
						value: 0,
						segmentToNext: "linear",
						tangentMode: "auto",
					},
				],
			},
		},
	};
}

function createHarness({ occupyMain = false }: { occupyMain?: boolean } = {}) {
	let tracks: SceneTracks = {
		overlay: [{
			id: "video-track",
			name: "Video",
			type: "video",
			muted: false,
			hidden: false,
			elements: [video()],
		}],
		main: {
			id: "main-track",
			name: "Main",
			type: "video",
			muted: false,
			hidden: false,
			elements: occupyMain
				? [{ ...video(), id: "main-clip", animations: undefined }]
				: [],
		},
		audio: [],
	};
	let selection: EditorSelectionSnapshot = {
		selectedElements: [{ trackId: "video-track", elementId: "source-clip" }],
		selectedKeyframes: [],
		keyframeSelectionAnchor: null,
		selectedMaskPoints: null,
	};
	const editor = {
		media: { getAssets: () => [{ id: "source-media", hasAudio: true }] },
		scenes: {
			getActiveScene: () => ({ tracks }),
			getActiveSceneOrNull: () => ({ tracks }),
		},
		timeline: {
			updateTracks: (next: SceneTracks) => {
				tracks = next;
			},
		},
		selection: {
			getSnapshot: () => selection,
			applySelectionPatch: ({ patch }: { patch: EditorSelectionPatch }) => {
				selection = { ...selection, ...patch };
				return selection;
			},
			restoreSnapshot: ({ snapshot }: { snapshot: EditorSelectionSnapshot }) => {
				selection = snapshot;
			},
		},
	};
	editorState.current = editor;
	return {
		manager: new CommandManager(editor as unknown as EditorCore),
		tracks: () => tracks,
		selection: () => selection,
		find: ({ trackId, elementId }: ElementRef): TimelineElement | undefined =>
			[...tracks.overlay, tracks.main, ...tracks.audio]
				.find((track) => track.id === trackId)
				?.elements.find((element) => element.id === elementId),
	};
}

beforeEach(() => {
	editorState.current = null;
});

describe("timeline command identity through undo and redo", () => {
	for (const retainSide of ["both", "right"] as const) {
		test(`split (${retainSide}) restores the same clip and keyframes before redoing a move`, () => {
			const harness = createHarness();
			const split = new SplitElementsCommand({
				elements: [{ trackId: "video-track", elementId: "source-clip" }],
				splitTime: at(5),
				retainSide,
			});
			harness.manager.execute({ command: split });
			const [created] = split.getRightSideElements();
			expect(created).toBeDefined();
			assertRedoSequence(harness, created);
			expect(split.getRightSideElements()).toEqual([created]);
		});
	}

	test("duplicate restores the same new track, clip, and keyframes before redoing a move", () => {
		const harness = createHarness();
		const duplicate = new DuplicateElementsCommand({
			elements: [{ trackId: "video-track", elementId: "source-clip" }],
		});
		harness.manager.execute({ command: duplicate });
		const [created] = duplicate.getDuplicatedElements();
		expect(created).toBeDefined();
		assertRedoSequence(harness, created);
		expect(duplicate.getDuplicatedElements()).toEqual([created]);
	});

	for (const newTrack of [false, true]) {
		test(`paste (${newTrack ? "new" : "existing"} track) restores the same clip and keyframes before redoing a move`, () => {
			const harness = createHarness({ occupyMain: newTrack });
			const paste = new PasteCommand({
				time: at(3),
				clipboardItems: [{
					trackId: "video-track",
					trackType: "video",
					element: video(),
				}],
			});
			harness.manager.execute({ command: paste });
			const [created] = paste.getPastedElements();
			expect(created).toBeDefined();
			if (newTrack) {
				expect(created.trackId).not.toBe("video-track");
				expect(created.trackId).not.toBe("main-track");
			}
			assertRedoSequence(harness, created);
			expect(paste.getPastedElements()).toEqual([created]);
		});
	}

	test("auto insert restores its new track ID before redoing a move", () => {
		const harness = createHarness({ occupyMain: true });
		const insert = new InsertElementCommand({
			element: video(),
			placement: { mode: "auto", trackType: "video" },
		});
		harness.manager.execute({ command: insert });
		const created = {
			trackId: insert.getTrackId()!,
			elementId: insert.getElementId(),
		};
		expect(created.trackId).not.toBe("video-track");
		expect(created.trackId).not.toBe("main-track");
		assertRedoSequence(harness, created);
		expect(insert.getTrackId()).toBe(created.trackId);
	});

	test("audio separation restores the same audio track, clip, and volume keyframes before redoing a move", () => {
		const harness = createHarness();
		const separation = new ToggleSourceAudioSeparationCommand({
			trackId: "video-track",
			elementId: "source-clip",
		});
		harness.manager.execute({ command: separation });
		const audioTrack = harness.tracks().audio[0];
		expect(audioTrack).toBeDefined();
		const audioElement = audioTrack.elements[0];
		expect(audioElement.animations?.volume).toBeDefined();
		assertRedoSequence(harness, {
			trackId: audioTrack.id,
			elementId: audioElement.id,
		});
		expect(harness.find({
			trackId: "video-track",
			elementId: "source-clip",
		})).toMatchObject({ isSourceAudioEnabled: false });
	});

	test("pasted keyframes retain their ID before redoing a keyframe retime", () => {
		const harness = createHarness();
		const ref = { trackId: "video-track", elementId: "source-clip" };
		const paste = new PasteKeyframesCommand({
			...ref,
			time: at(2),
			clipboardItems: [{
				propertyPath: "opacity",
				timeOffset: at(0),
				value: 0.75,
				interpolation: "linear",
				curvePatches: [],
			}],
		});
		harness.manager.execute({ command: paste });
		const keyframe = getKeyframeAtTime({
			animations: harness.find(ref)?.animations,
			propertyPath: "opacity",
			time: at(2),
		});
		expect(keyframe).toBeDefined();
		const pastedTracks = harness.tracks();
		harness.manager.execute({
			command: new RetimeKeyframeCommand({
				...ref,
				propertyPath: "opacity",
				keyframeId: keyframe!.id,
				nextTime: at(3),
			}),
		});
		const editedTracks = harness.tracks();
		expect(getKeyframeAtTime({
			animations: harness.find(ref)?.animations,
			propertyPath: "opacity",
			time: at(3),
		})?.id).toBe(keyframe!.id);
		for (let cycle = 0; cycle < 3; cycle++) {
			harness.manager.undo();
			harness.manager.undo();
			harness.manager.redo();
			expect(harness.tracks()).toEqual(pastedTracks);
			harness.manager.redo();
			expect(harness.tracks()).toEqual(editedTracks);
			expect(getKeyframeAtTime({
				animations: harness.find(ref)?.animations,
				propertyPath: "opacity",
				time: at(3),
			})?.id).toBe(keyframe!.id);
		}
	});

	test("30 edits undo and redo with identical tracks, IDs, selection, and shared asset references", () => {
		const harness = createHarness();
		const initialTracks = harness.tracks();
		const initialSelection = harness.selection();
		const states: Array<{
			tracks: SceneTracks;
			selection: EditorSelectionSnapshot;
		}> = [];
		const recordState = () => states.push({
			tracks: harness.tracks(),
			selection: harness.selection(),
		});
		for (let index = 0; index < 15; index++) {
			const duplicate = new DuplicateElementsCommand({
				elements: [{ trackId: "video-track", elementId: "source-clip" }],
			});
			harness.manager.execute({ command: duplicate });
			recordState();
			const [created] = duplicate.getDuplicatedElements();
			harness.manager.execute({
				command: new MoveElementCommand({
					moves: [{
						sourceTrackId: created.trackId,
						targetTrackId: created.trackId,
						elementId: created.elementId,
						newStartTime: at(20 + index),
					}],
				}),
			});
			recordState();
		}
		expect(states).toHaveLength(30);
		for (let index = states.length - 1; index >= 0; index--) {
			harness.manager.undo();
			expect(harness.tracks()).toEqual(
				index === 0 ? initialTracks : states[index - 1].tracks,
			);
		}
		expect(harness.selection()).toEqual(initialSelection);
		expect(harness.manager.canUndo()).toBe(false);
		for (const state of states) {
			harness.manager.redo();
			expect(harness.tracks()).toEqual(state.tracks);
			expect(harness.selection()).toEqual(state.selection);
		}
		expect(harness.manager.canRedo()).toBe(false);
		const finalTracks = harness.tracks();
		const trackIds = [
			...finalTracks.overlay,
			finalTracks.main,
			...finalTracks.audio,
		].map((track) => track.id);
		const clips = finalTracks.overlay.flatMap<TimelineElement>(
			(track) => track.elements,
		);
		expect(new Set(trackIds).size).toBe(trackIds.length);
		expect(clips).toHaveLength(16);
		expect(new Set(clips.map((clip) => clip.id)).size).toBe(16);
		expect(clips.every(
			(clip) => "mediaId" in clip && clip.mediaId === "source-media",
		)).toBe(true);
	});
});

function assertRedoSequence(
	harness: ReturnType<typeof createHarness>,
	created: ElementRef,
) {
	const createdElement = harness.find(created);
	expect(createdElement).toBeDefined();
	expect(created.elementId).not.toBe("source-clip");
	const createdTracks = harness.tracks();
	const creationSelection = harness.selection().selectedElements;
	harness.manager.execute({
		command: new MoveElementCommand({
			moves: [{
				sourceTrackId: created.trackId,
				targetTrackId: created.trackId,
				elementId: created.elementId,
				newStartTime: at(20),
			}],
		}),
	});
	const editedTracks = harness.tracks();
	expect(harness.find(created)?.startTime).toBe(at(20));

	// Repeat the complete chain to catch IDs changing on later redo cycles too.
	for (let cycle = 0; cycle < 3; cycle++) {
		harness.manager.undo();
		expect(harness.tracks()).toEqual(createdTracks);
		harness.manager.undo();
		expect(harness.find(created)).toBeUndefined();
		harness.manager.redo();
		expect(harness.tracks()).toEqual(createdTracks);
		expect(harness.find(created)).toEqual(createdElement);
		expect(harness.selection().selectedElements).toEqual(creationSelection);
		expect(() => harness.manager.redo()).not.toThrow();
		expect(harness.tracks()).toEqual(editedTracks);
		expect(harness.find(created)?.startTime).toBe(at(20));
		expect(harness.selection().selectedElements).toEqual([created]);
	}
}

import { beforeEach, describe, expect, it, vi } from "vitest";
import type { EditorCore } from "@/core";
import { CommandManager } from "@/core/managers/commands";
import {
	EditCaptionTrackCommand,
	executeCaptionTrackEdit,
} from "@/commands/timeline/element/edit-caption-track";
import type {
	EditorSelectionPatch,
	EditorSelectionSnapshot,
} from "@/selection/editor-selection";
import type { SceneTracks, TextElement, TextTrack } from "@/timeline/types";
import { mediaTime } from "@/wasm";
import { planCaptionTrackEdit, type CaptionEditAction } from "./edit";
import { serializeSubtitleTrack } from "./export";
import { parseSubtitleFile } from "./parse";

const state = vi.hoisted(() => ({ editor: null as unknown }));
vi.mock("@/core", () => ({ EditorCore: { getInstance: () => state.editor } }));
const t = (ticks: number) => mediaTime({ ticks });
const cue = (
	id: string,
	start: number,
	duration: number,
	content: string,
): TextElement => ({
	id,
	type: "text",
	name: id,
	startTime: t(start),
	duration: t(duration),
	trimStart: t(0),
	trimEnd: t(0),
	params: { content, color: "#ffffff", "transform.positionX": 34 },
});
const track = (): TextTrack => ({
	id: "captions",
	name: "Captions",
	type: "text",
	hidden: false,
	elements: [
		cue("later", 500, 100, "Câu sau"),
		cue("first", 100, 300, "Xin chào 🎬 Việt Nam"),
	],
});

function harness(target: TextTrack) {
	let tracks: SceneTracks = {
		main: {
			id: "main",
			name: "Main",
			type: "video",
			muted: false,
			hidden: false,
			elements: [],
		},
		overlay: [
			target,
			{
				...track(),
				id: "other",
				elements: [cue("unrelated", 0, 1000, "Untouched")],
			},
		],
		audio: [],
	};
	let selection: EditorSelectionSnapshot = {
		selectedElements: [{ trackId: target.id, elementId: "first" }],
		selectedKeyframes: [],
		keyframeSelectionAnchor: null,
		selectedMaskPoints: null,
	};
	const editor = {
		scenes: {
			getActiveScene: () => ({ tracks }),
			getActiveSceneOrNull: () => ({ tracks }),
		},
		timeline: {
			updateTracks: vi.fn((next: SceneTracks) => {
				tracks = next;
			}),
		},
		selection: {
			getSnapshot: () => selection,
			applySelectionPatch: ({ patch }: { patch: EditorSelectionPatch }) => {
				selection = { ...selection, ...patch };
				return selection;
			},
			restoreSnapshot: ({
				snapshot,
			}: {
				snapshot: EditorSelectionSnapshot;
			}) => {
				selection = snapshot;
			},
		},
		command: null as unknown as CommandManager,
	};
	state.editor = editor;
	editor.command = new CommandManager(editor as unknown as EditorCore);
	return {
		editor: editor as unknown as EditorCore,
		tracks: () => tracks,
		target: () => tracks.overlay[0] as TextTrack,
		selection: () => selection,
	};
}

beforeEach(() => {
	state.editor = null;
});

describe("caption workbench operations", () => {
	it("edits only the explicitly chosen track and roundtrips edited cues through subtitle export", () => {
		const h = harness(track());
		const original = h.tracks();
		const updated = executeCaptionTrackEdit({
			editor: h.editor,
			trackId: "captions",
			action: {
				type: "update",
				elementId: "first",
				content: "Nội dung mới 🎬",
				startTime: t(120_000),
				endTime: t(240_000),
			},
		});
		expect(updated).toBe(true);
		expect(h.tracks().overlay[1]).toBe(original.overlay[1]);
		expect(h.target().elements[1].params["transform.positionX"]).toBe(34);
		expect(h.target().elements[1].id).toBe("first");
		const exported = serializeSubtitleTrack({
			track: h.target(),
			format: "srt",
		});
		const parsed = parseSubtitleFile({
			fileName: "edited.srt",
			input: exported,
		});
		expect(
			parsed.captions.find(({ text }) => text === "Nội dung mới 🎬"),
		).toEqual({ text: "Nội dung mới 🎬", startTime: 1, duration: 1 });
		h.editor.command.undo();
		expect(h.tracks()).toBe(original);
		h.editor.command.redo();
		expect(h.target().elements[1].params.content).toBe("Nội dung mới 🎬");
	});

	it("split keeps left ID, exact span and source trim provenance; IDs survive a dependent edit and undo/redo chain", () => {
		const initial = track();
		initial.elements[1] = {
			...initial.elements[1],
			trimStart: t(40),
			trimEnd: t(50),
		};
		const h = harness(initial);
		const before = h.tracks();
		executeCaptionTrackEdit({
			editor: h.editor,
			trackId: "captions",
			action: {
				type: "split",
				elementId: "first",
				time: t(210),
				textOffset: 9,
			},
		});
		const [left, right] = h
			.target()
			.elements.filter(({ id }) => id !== "later");
		expect(left.id).toBe("first");
		expect(left.params.content).toBe("Xin chào");
		expect(right.params.content).toBe("🎬 Việt Nam");
		expect(left.duration + right.duration).toBe(300);
		expect(left.trimEnd).toBe(240);
		expect(right.trimStart).toBe(150);
		expect(h.selection().selectedElements).toEqual([
			{ trackId: "captions", elementId: right.id },
		]);
		executeCaptionTrackEdit({
			editor: h.editor,
			trackId: "captions",
			action: {
				type: "update",
				elementId: right.id,
				content: "Edited right",
				startTime: right.startTime,
				endTime: t(right.startTime + right.duration),
			},
		});
		const after = h.tracks();
		h.editor.command.undo();
		h.editor.command.undo();
		expect(h.tracks()).toBe(before);
		h.editor.command.redo();
		h.editor.command.redo();
		expect(h.tracks()).toBe(after);
		expect(
			h.target().elements.find(({ id }) => id === right.id)?.params.content,
		).toBe("Edited right");
	});

	it("merges chronological next cue on unsorted input and retains the first style with overlapping or nested timing", () => {
		const current = track();
		current.elements[0] = {
			...current.elements[0],
			startTime: t(200),
			duration: t(100),
			params: { content: "next", color: "#00ff00" },
		};
		const plan = planCaptionTrackEdit({
			track: current,
			action: { type: "merge", elementId: "first" },
		})!;
		expect(plan.track.elements).toHaveLength(1);
		expect(plan.track.elements[0]).toMatchObject({
			id: "first",
			startTime: 100,
			duration: 300,
			params: { content: "Xin chào 🎬 Việt Nam\nnext", color: "#ffffff" },
		});
		expect(current.elements).toHaveLength(2);
	});

	it("treats find/replace as literal unicode text, including regex syntax and replacement dollars", () => {
		const current = track();
		current.elements[0].params.content = "[a].* [a].*";
		const plan = planCaptionTrackEdit({
			track: current,
			action: {
				type: "replace",
				find: "[a].*",
				replacement: "$&🎬",
				matchCase: false,
			},
		})!;
		expect(plan.track.elements[0].params.content).toBe("$&🎬 $&🎬");
		expect(plan.track.elements[1]).toBe(current.elements[1]);
		expect(
			planCaptionTrackEdit({
				track: current,
				action: {
					type: "replace",
					find: "absent",
					replacement: "x",
					matchCase: true,
				},
			}),
		).toBeNull();
		current.elements[0].params.content = "xin Xin";
		expect(
			planCaptionTrackEdit({
				track: current,
				action: {
					type: "replace",
					find: "Xin",
					replacement: "Xin",
					matchCase: false,
				},
			})!.track.elements[0].params.content,
		).toBe("Xin Xin");
	});

	it("style and timing offset are atomic, preserve positions relative to each other, and undo in one step", () => {
		const h = harness(track());
		h.editor.command.isRippleEnabled = true;
		const original = h.tracks();
		executeCaptionTrackEdit({
			editor: h.editor,
			trackId: "captions",
			action: { type: "offset", offset: t(100) },
		});
		expect(h.target().elements.map(({ startTime }) => startTime)).toEqual([
			600, 200,
		]);
		h.editor.command.undo();
		expect(h.tracks()).toBe(original);
		expect(h.editor.command.canUndo()).toBe(false);
		executeCaptionTrackEdit({
			editor: h.editor,
			trackId: "captions",
			action: { type: "style", preset: "yellow" },
		});
		expect(
			h
				.target()
				.elements.every(
					(element) =>
						element.params.color === "#ffe600" &&
						element.params["transform.positionX"] === 34,
				),
		).toBe(true);
		h.editor.command.undo();
		expect(h.tracks()).toBe(original);
	});

	it("preflights locked, empty, unchanged and invalid operations without adding undo history", () => {
		const h = harness(track());
		const original = h.tracks();
		const noops: CaptionEditAction[] = [
			{ type: "offset", offset: t(0) },
			{ type: "replace", find: "", replacement: "abc", matchCase: true },
			{
				type: "update",
				elementId: "first",
				content: "Xin chào 🎬 Việt Nam",
				startTime: t(100),
				endTime: t(400),
			},
		];
		for (const action of noops)
			expect(
				executeCaptionTrackEdit({
					editor: h.editor,
					trackId: "captions",
					action,
				}),
			).toBe(false);
		expect(() =>
			executeCaptionTrackEdit({
				editor: h.editor,
				trackId: "captions",
				action: { type: "offset", offset: t(-101) },
			}),
		).toThrow("invalidOffset");
		expect(() =>
			executeCaptionTrackEdit({
				editor: h.editor,
				trackId: "captions",
				action: {
					type: "split",
					elementId: "first",
					time: t(100),
					textOffset: 9,
				},
			}),
		).toThrow("invalidSplit");
		expect(h.tracks()).toBe(original);
		expect(h.editor.command.canUndo()).toBe(false);
		h.target().locked = true;
		expect(
			executeCaptionTrackEdit({
				editor: h.editor,
				trackId: "captions",
				action: { type: "style", preset: "yellow" },
			}),
		).toBe(false);
		new EditCaptionTrackCommand("captions", {
			type: "offset",
			offset: t(100),
		}).execute();
		expect(h.tracks()).toBe(original);
		expect(h.editor.command.canUndo()).toBe(false);
	});

	it("rejects splitting inside a surrogate pair and shrinking to invalid timing", () => {
		const current = track();
		expect(() =>
			planCaptionTrackEdit({
				track: current,
				action: {
					type: "split",
					elementId: "first",
					time: t(200),
					textOffset: 10,
				},
			}),
		).toThrow("invalidSplit");
		expect(() =>
			planCaptionTrackEdit({
				track: current,
				action: {
					type: "update",
					elementId: "first",
					content: "text",
					startTime: t(100),
					endTime: t(100),
				},
			}),
		).toThrow("invalidTiming");
	});

	it("uses the existing animation pipeline when shrinking or splitting a cue", () => {
		const current = track();
		current.elements[1].animations = {
			opacity: {
				keys: [
					{
						id: "opacity-start",
						time: t(0),
						value: 0,
						segmentToNext: "linear",
						tangentMode: "auto",
					},
					{
						id: "opacity-middle",
						time: t(150),
						value: 0.5,
						segmentToNext: "linear",
						tangentMode: "auto",
					},
					{
						id: "opacity-end",
						time: t(300),
						value: 1,
						segmentToNext: "linear",
						tangentMode: "auto",
					},
				],
			},
		};
		const h = harness(current);
		executeCaptionTrackEdit({
			editor: h.editor,
			trackId: "captions",
			action: {
				type: "split",
				elementId: "first",
				time: t(210),
				textOffset: 9,
			},
		});
		const [left, right] = h
			.target()
			.elements.filter(({ id }) => id !== "later");
		expect(left.animations?.opacity).toMatchObject({
			keys: expect.arrayContaining([
				expect.objectContaining({ id: "opacity-start", time: 0 }),
			]),
		});
		expect(right.animations?.opacity).toMatchObject({
			keys: expect.arrayContaining([
				expect.objectContaining({ id: "opacity-middle", time: 40 }),
			]),
		});
		h.editor.command.undo();
		executeCaptionTrackEdit({
			editor: h.editor,
			trackId: "captions",
			action: {
				type: "update",
				elementId: "first",
				content: "short",
				startTime: t(100),
				endTime: t(150),
			},
		});
		const shrunk = h.target().elements.find(({ id }) => id === "first")!;
		expect(shrunk.animations?.opacity).toMatchObject({
			keys: expect.arrayContaining([expect.objectContaining({ time: 50 })]),
		});
	});
});

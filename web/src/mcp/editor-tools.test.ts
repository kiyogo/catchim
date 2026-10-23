import { describe, expect, it, vi } from "vitest";
import type { EditorCore } from "@/core";
import { executeEditorTool } from "./editor-tools";
import { toolInputs } from "./contracts";
import { TICKS_PER_SECOND } from "@/wasm";

vi.mock("@/commands", () => ({
	BatchCommand: class {},
	ToggleTrackLockCommand: class {},
	ToggleTrackMuteCommand: class {},
	ToggleTrackVisibilityCommand: class {},
}));

function fixture() {
	const element = {
		id: "clip",
		type: "video",
		name: "clip",
		startTime: 2 * TICKS_PER_SECOND,
		duration: 3 * TICKS_PER_SECOND,
		trimStart: TICKS_PER_SECOND,
		trimEnd: 0,
		params: { volume: 0, opacity: 1 },
		mediaId: "media",
	};
	const track = {
		id: "track",
		name: "Track",
		type: "video",
		locked: false,
		elements: [element],
		muted: false,
		hidden: false,
	};
	const project = {
		metadata: { id: "project", name: "Project" },
		settings: {},
	};
	const updateElements = vi.fn(({ updates }) => {
		Object.assign(element, updates[0].patch);
	});
	const editor = {
		project: {
			getActiveOrNull: () => project,
			getIsLoading: () => false,
			saveCurrentProject: vi.fn().mockResolvedValue(undefined),
		},
		scenes: {
			getActiveScene: () => ({
				id: "scene",
				tracks: { overlay: [], main: track, audio: [] },
			}),
			getScenes: () => [],
		},
		timeline: {
			isPreviewActive: () => false,
			getTrackById: ({ trackId }: { trackId: string }) =>
				trackId === "track" ? track : null,
			updateElements,
			deleteElements: vi.fn(),
			getTotalDuration: () => 5 * TICKS_PER_SECOND,
		},
		media: {
			getAssets: () => [
				{
					id: "media",
					name: "Source",
					type: "video",
					duration: 12,
					file: new File(["private-bytes"], "source.mp4"),
					url: "blob:private-url",
					thumbnailUrl: "data:private-image",
				},
			],
		},
		playback: { getCurrentTime: () => TICKS_PER_SECOND },
		command: { canUndo: () => false, canRedo: () => false },
		save: { getIsDirty: () => false },
	};
	return {
		editor: editor as unknown as EditorCore,
		stub: editor,
		element,
		track,
	};
}
describe("MCP editor adapters", () => {
	it("exposes seconds and metadata without file bytes, object URLs or thumbnails", async () => {
		const { editor } = fixture();
		const timeline = await executeEditorTool(
			editor,
			"catchim_get_timeline",
			{},
		);
		expect(timeline.clips).toMatchObject([
			{ startSeconds: 2, durationSeconds: 3, trimStartSeconds: 1 },
		]);
		const media = JSON.stringify(
			await executeEditorTool(editor, "catchim_list_media", {}),
		);
		expect(media).toContain('"durationSeconds":12');
		expect(media).not.toMatch(/private-|blob:|data:/);
	});
	it("rejects locked tracks, unsupported fields and invalid time before invoking edit commands", async () => {
		const { editor, stub, track } = fixture();
		track.locked = true;
		await expect(
			executeEditorTool(editor, "catchim_delete_clips", {
				elements: [{ trackId: "track", elementId: "clip" }],
			}),
		).rejects.toThrow("locked");
		expect(stub.timeline.deleteElements).not.toHaveBeenCalled();
		track.locked = false;
		await expect(
			executeEditorTool(editor, "catchim_update_clip", {
				trackId: "track",
				elementId: "clip",
				content: "wrong-type",
			}),
		).rejects.toThrow("text");
		await expect(
			executeEditorTool(editor, "catchim_update_clip", {
				trackId: "track",
				elementId: "clip",
				startSeconds: -1,
			}),
		).rejects.toThrow();
		expect(stub.timeline.updateElements).not.toHaveBeenCalled();
	});
	it("maps audio gain to the existing dB field and preserves source trims", async () => {
		const { editor, element } = fixture();
		const result = await executeEditorTool(editor, "catchim_update_clip", {
			trackId: "track",
			elementId: "clip",
			volumeDb: -12,
		});
		expect(result.applied).toBe(true);
		expect(element.params.volume).toBe(-12);
		expect(element.trimStart).toBe(TICKS_PER_SECOND);
		expect(
			toolInputs.catchim_update_clip.safeParse({
				trackId: "track",
				elementId: "clip",
				volumeDb: 21,
			}).success,
		).toBe(false);
	});
	it("returns applied=false when the editor refuses a requested property", async () => {
		const { editor, stub } = fixture();
		stub.timeline.updateElements.mockImplementation(() => {});
		expect(
			(
				await executeEditorTool(editor, "catchim_update_clip", {
					trackId: "track",
					elementId: "clip",
					startSeconds: 10,
				})
			).applied,
		).toBe(false);
	});
	it("does not report a failed save as successful", async () => {
		const { editor, stub } = fixture();
		stub.project.saveCurrentProject.mockRejectedValue(
			new Error("Quota exceeded"),
		);
		await expect(
			executeEditorTool(editor, "catchim_save_project", {}),
		).rejects.toThrow("Quota exceeded");
		expect(stub.project.saveCurrentProject).toHaveBeenCalledWith({
			throwOnError: true,
		});
	});
});

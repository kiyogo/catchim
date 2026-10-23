import { z } from "zod";

const seconds = z.number().finite().min(0).max(604800);
const id = z.string().min(1).max(200);
const ref = z.object({ trackId: id, elementId: id }).strict();
const refs = z
	.array(ref)
	.min(1)
	.max(100)
	.refine(
		(items) =>
			new Set(items.map((item) => `${item.trackId}:${item.elementId}`)).size ===
			items.length,
		"Duplicate clip references are not allowed",
	);
const page = {
	offset: z.number().int().min(0).default(0),
	limit: z.number().int().min(1).max(500).default(200),
};

export const toolInputs = {
	catchim_get_project: z.object({}).strict(),
	catchim_list_media: z.object(page).strict(),
	catchim_get_timeline: z.object({ ...page, trackId: id.optional() }).strict(),
	catchim_get_selection: z.object({}).strict(),
	catchim_seek: z.object({ seconds }).strict(),
	catchim_select_elements: z
		.object({ elements: z.array(ref).max(100) })
		.strict(),
	catchim_add_text: z
		.object({
			content: z.string().min(1).max(10000),
			startSeconds: seconds,
			durationSeconds: seconds.positive().default(5),
			fontSize: z.number().min(1).max(1000).optional(),
			color: z
				.string()
				.regex(/^#[\da-fA-F]{6}$/)
				.optional(),
			trackId: id.optional(),
		})
		.strict(),
	catchim_add_media: z
		.object({
			mediaId: id,
			startSeconds: seconds,
			imageDurationSeconds: seconds.positive().default(5),
			trackId: id.optional(),
		})
		.strict(),
	catchim_update_clip: z
		.object({
			...ref.shape,
			name: z.string().trim().min(1).max(300).optional(),
			startSeconds: seconds.optional(),
			content: z.string().max(10000).optional(),
			opacity: z.number().min(0).max(1).optional(),
			volumeDb: z.number().min(-60).max(20).optional(),
		})
		.strict()
		.refine(
			(value) =>
				[
					value.name,
					value.startSeconds,
					value.content,
					value.opacity,
					value.volumeDb,
				].some((value) => value !== undefined),
			"Provide at least one property to update",
		),
	catchim_split_clip: z.object({ ...ref.shape, seconds }).strict(),
	catchim_duplicate_clips: z.object({ elements: refs }).strict(),
	catchim_delete_clips: z.object({ elements: refs }).strict(),
	catchim_set_track_state: z
		.object({
			trackId: id,
			locked: z.boolean().optional(),
			muted: z.boolean().optional(),
			hidden: z.boolean().optional(),
		})
		.strict()
		.refine(
			(value) =>
				value.locked !== undefined ||
				value.muted !== undefined ||
				value.hidden !== undefined,
			"Provide a track state",
		),
	catchim_close_track_gaps: z.object({ trackId: id }).strict(),
	catchim_set_clip_color: z
		.object({
			...ref.shape,
			brightness: z.number().min(-1).max(1).optional(),
			contrast: z.number().min(-1).max(1).optional(),
			saturation: z.number().min(-1).max(2).optional(),
			exposure: z.number().min(-4).max(4).optional(),
			temperature: z.number().min(-1).max(1).optional(),
			tint: z.number().min(-1).max(1).optional(),
			hue: z.number().min(-180).max(180).optional(),
			gamma: z.number().min(0.1).max(3).optional(),
		})
		.strict()
		.refine(
			(value) => Object.keys(value).length > 2,
			"Provide a color adjustment",
		),
	catchim_history: z.object({ action: z.enum(["undo", "redo"]) }).strict(),
	catchim_save_project: z.object({}).strict(),
};

export type EditorToolName = keyof typeof toolInputs;
export type ToolInput<N extends EditorToolName> = z.infer<
	(typeof toolInputs)[N]
>;
export function isEditorToolName(name: string): name is EditorToolName {
	return Object.hasOwn(toolInputs, name);
}
export function parseToolInput<N extends EditorToolName>(
	name: N,
	input: unknown,
): ToolInput<N> {
	return toolInputs[name].parse(input) as ToolInput<N>;
}

export const toolDetails: Record<
	EditorToolName,
	{ description: string; readOnly: boolean; destructive?: boolean }
> = {
	catchim_get_project: {
		description:
			"Read the connected project's settings, scenes, playhead and history availability. Times in this API are seconds. Only the explicitly connected browser tab is accessible.",
		readOnly: true,
	},
	catchim_list_media: {
		description:
			"List imported media metadata with pagination. Returns IDs and source duration in seconds; never returns files, bytes or local URLs.",
		readOnly: true,
	},
	catchim_get_timeline: {
		description:
			"Read active-scene track summaries and a paginated list of clips. Optionally filter by trackId. Times are seconds. Read before editing to obtain current clip IDs.",
		readOnly: true,
	},
	catchim_get_selection: {
		description: "Read selected clip and keyframe references.",
		readOnly: true,
	},
	catchim_seek: {
		description:
			"Seek the connected editor to an absolute timeline time in seconds. Returns the actual clamped time.",
		readOnly: false,
	},
	catchim_select_elements: {
		description:
			"Select existing clips in the active scene, or clear selection with an empty array.",
		readOnly: false,
	},
	catchim_add_text: {
		description:
			"Insert a text clip through the editor's normal placement and undo command. Optional trackId selects an explicit compatible, unlocked track. Returns actual inserted clip references/timing.",
		readOnly: false,
	},
	catchim_add_media: {
		description:
			"Insert an already imported media asset on the timeline. Audio/video uses its full source duration; imageDurationSeconds applies to still images. Does not import files or fetch URLs.",
		readOnly: false,
	},
	catchim_update_clip: {
		description:
			"Update a clip's name, start time, text content, visual opacity (0–1), or audio volumeDb (-60 to +20 dB, 0 = unchanged gain). Uses the normal editor update command and returns actual applied values; source trim/rate is preserved.",
		readOnly: false,
	},
	catchim_split_clip: {
		description:
			"Split one unlocked clip at an absolute timeline time strictly inside the clip. Uses the normal source-aware split command.",
		readOnly: false,
	},
	catchim_duplicate_clips: {
		description:
			"Duplicate existing unlocked clips through the normal editor command. Returns the new IDs and actual placement.",
		readOnly: false,
	},
	catchim_delete_clips: {
		description:
			"Delete specified unlocked clips from the active timeline. Imported source files remain available. The operation can be undone.",
		readOnly: false,
		destructive: true,
	},
	catchim_set_track_state: {
		description:
			"Set track lock, mute and/or visibility using existing commands. Audio tracks support mute; visual tracks support hidden. Request only supported properties.",
		readOnly: false,
	},
	catchim_close_track_gaps: {
		description:
			"Close gaps on one unlocked track using the existing Rust/WASM interval planner and undo command. Preserves leading offset, overlaps and source trims.",
		readOnly: false,
	},
	catchim_set_clip_color: {
		description:
			"Set supplied color-grading parameters on a visual clip, adding the effect if necessary. Uses one undoable update; omitted parameters retain their values.",
		readOnly: false,
	},
	catchim_history: {
		description:
			"Undo or redo one step in the connected editor's shared history, including human edits. Inspect project/history availability first.",
		readOnly: false,
		destructive: true,
	},
	catchim_save_project: {
		description:
			"Persist the current project to this browser's local storage and report storage failures. Does not export a video or send files elsewhere.",
		readOnly: false,
	},
};

export type BridgeResult =
	{ ok: true; data: Record<string, unknown> } | { ok: false; error: string };
export interface BridgeJob {
	id: string;
	name: EditorToolName;
	input: unknown;
	expiresAt: number;
}
export interface McpCredentials {
	id: string;
	accessToken: string;
	bridgeToken: string;
	projectId: string;
	allowEdits: boolean;
}

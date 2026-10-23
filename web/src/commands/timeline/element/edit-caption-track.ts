import {
	Command,
	createElementSelectionResult,
	type CommandResult,
} from "@/commands/base-command";
import { EditorCore } from "@/core";
import type { EditorCore as Editor } from "@/core";
import {
	planCaptionTrackEdit,
	type CaptionEditAction,
	type CaptionTrackEdit,
} from "@/subtitles/edit";
import { findTrackInSceneTracks } from "@/timeline/track-element-update";
import { applyElementUpdate } from "@/timeline/update-pipeline";
import type { SceneTracks, TextTrack } from "@/timeline/types";

/** One atomic caption edit, preserving normal project, renderer and history flow. */
export class EditCaptionTrackCommand extends Command {
	override readonly appliesOwnRipple = true;
	private savedState: SceneTracks | null = null;
	private appliedState: SceneTracks | null = null;
	private result: CommandResult | undefined;

	constructor(
		private readonly trackId: string,
		private readonly action: CaptionEditAction,
		private prepared?: { tracks: SceneTracks; plan: CaptionTrackEdit | null },
	) {
		super();
	}

	execute(): CommandResult | undefined {
		const editor = EditorCore.getInstance();
		const tracks = editor.scenes.getActiveScene().tracks;
		const track = findTrackInSceneTracks({ tracks, trackId: this.trackId });
		if (!track || track.type !== "text" || track.locked) return;
		const plan =
			this.prepared?.tracks === tracks
				? this.prepared.plan
				: planCaptionTrackEdit({ track, action: this.action });
		this.prepared = undefined;
		if (!plan) return;
		const previousById = new Map(
			track.elements.map((element) => [element.id, element]),
		);
		const nextTrack: TextTrack = {
			...plan.track,
			elements: plan.track.elements.map((element) => {
				const previous = previousById.get(element.id);
				return previous && previous.duration !== element.duration
					? (applyElementUpdate({
							element,
							patch: { duration: element.duration },
							context: { tracks, trackId: track.id },
						}) as typeof element)
					: element;
			}),
		};
		this.savedState = tracks;
		this.appliedState = {
			...tracks,
			overlay: tracks.overlay.map((current) =>
				current.id === track.id ? nextTrack : current,
			),
		};
		this.result = plan.selectedIds
			? createElementSelectionResult(
					plan.selectedIds.map((elementId) => ({
						trackId: track.id,
						elementId,
					})),
				)
			: undefined;
		editor.timeline.updateTracks(this.appliedState);
		return this.result;
	}

	undo(): void {
		if (this.savedState)
			EditorCore.getInstance().timeline.updateTracks(this.savedState);
	}
	redo(): CommandResult | undefined {
		if (this.appliedState)
			EditorCore.getInstance().timeline.updateTracks(this.appliedState);
		return this.result;
	}
}

/** Preflight prevents locked, empty and identical operations from creating undo entries. */
export function executeCaptionTrackEdit({
	editor,
	trackId,
	action,
}: {
	editor: Editor;
	trackId: string;
	action: CaptionEditAction;
}): boolean {
	const tracks = editor.scenes.getActiveScene().tracks;
	const track = findTrackInSceneTracks({ tracks, trackId });
	if (!track || track.type !== "text" || track.locked) return false;
	const plan = planCaptionTrackEdit({ track, action });
	if (!plan) return false;
	editor.command.execute({
		command: new EditCaptionTrackCommand(trackId, action, { tracks, plan }),
	});
	return true;
}

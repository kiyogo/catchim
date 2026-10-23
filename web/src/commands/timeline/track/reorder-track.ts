import { Command, type CommandResult } from "@/commands/base-command";
import { EditorCore } from "@/core";
import type { SceneTracks, TimelineTrack } from "@/timeline";

type Direction = "up" | "down";

export class ReorderTrackCommand extends Command {
	private savedState: SceneTracks | null = null;
	private appliedState: SceneTracks | null = null;

	constructor(
		private readonly trackId: string,
		private readonly direction: Direction,
	) {
		super();
	}

	execute(): CommandResult | undefined {
		const editor = EditorCore.getInstance();
		const tracks = editor.scenes.getActiveScene().tracks;
		const section = tracks.overlay.some((track) => track.id === this.trackId)
			? "overlay"
			: tracks.audio.some((track) => track.id === this.trackId)
				? "audio"
				: null;
		if (!section) return;

		const current = tracks[section];
		const index = current.findIndex((track) => track.id === this.trackId);
		const targetIndex = index + (this.direction === "up" ? -1 : 1);
		if (index < 0 || targetIndex < 0 || targetIndex >= current.length) return;

		const reordered = [...current];
		[reordered[index], reordered[targetIndex]] = [reordered[targetIndex], reordered[index]];
		this.savedState = tracks;
		this.appliedState = { ...tracks, [section]: reordered };
		editor.timeline.updateTracks(this.appliedState);
		return undefined;
	}

	undo(): void {
		if (this.savedState) EditorCore.getInstance().timeline.updateTracks(this.savedState);
	}

	redo(): CommandResult | undefined {
		if (this.appliedState) EditorCore.getInstance().timeline.updateTracks(this.appliedState);
		return undefined;
	}
}

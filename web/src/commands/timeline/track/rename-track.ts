import { Command, type CommandResult } from "@/commands/base-command";
import { EditorCore } from "@/core";
import type { SceneTracks } from "@/timeline";
import { updateTrackInSceneTracks } from "@/timeline/track-element-update";
import { isTrackLocked } from "@/timeline/track-capabilities";

export class RenameTrackCommand extends Command {
	private savedState: SceneTracks | null = null;
	private appliedState: SceneTracks | null = null;

	constructor(
		private readonly trackId: string,
		private readonly name: string,
	) {
		super();
	}

	execute(): CommandResult | undefined {
		const editor = EditorCore.getInstance();
		const tracks = editor.scenes.getActiveScene().tracks;
		const track = [...tracks.overlay, tracks.main, ...tracks.audio].find(
			(candidate) => candidate.id === this.trackId,
		);
		const name = this.name.trim().slice(0, 80);
		if (!track || isTrackLocked(track) || !name || track.name === name) return;

		this.savedState = tracks;
		this.appliedState = updateTrackInSceneTracks({
			tracks,
			trackId: this.trackId,
			update: (current) => ({ ...current, name }),
		});
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

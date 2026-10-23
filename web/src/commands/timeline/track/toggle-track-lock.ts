import { Command, type CommandResult } from "@/commands/base-command";
import { EditorCore } from "@/core";
import type { SceneTracks } from "@/timeline";
import {
	findTrackInSceneTracks,
	updateTrackInSceneTracks,
} from "@/timeline/track-element-update";

export class ToggleTrackLockCommand extends Command {
	private savedState: SceneTracks | null = null;

	constructor(private readonly trackId: string) {
		super();
	}

	execute(): CommandResult | undefined {
		const editor = EditorCore.getInstance();
		this.savedState = editor.scenes.getActiveScene().tracks;
		const track = findTrackInSceneTracks({
			tracks: this.savedState,
			trackId: this.trackId,
		});
		if (!track) return;

		editor.timeline.updateTracks(
			updateTrackInSceneTracks({
				tracks: this.savedState,
				trackId: this.trackId,
				update: (current) => ({ ...current, locked: current.locked !== true }),
			}),
		);
	}

	undo(): void {
		if (!this.savedState) return;
		EditorCore.getInstance().timeline.updateTracks(this.savedState);
	}
}

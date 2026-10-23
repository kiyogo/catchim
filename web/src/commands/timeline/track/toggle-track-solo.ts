import { Command, type CommandResult } from "@/commands/base-command";
import { EditorCore } from "@/core";
import type { SceneTracks } from "@/timeline";
import { canTrackHaveAudio, findTrackInSceneTracks, updateTrackInSceneTracks } from "@/timeline";

export class ToggleTrackSoloCommand extends Command {
	private savedState: SceneTracks | null = null;

	constructor(private readonly trackId: string) {
		super();
	}

	execute(): CommandResult | undefined {
		const editor = EditorCore.getInstance();
		this.savedState = editor.scenes.getActiveScene().tracks;
		const track = findTrackInSceneTracks({ tracks: this.savedState, trackId: this.trackId });
		if (!track || (!canTrackHaveAudio(track) && !canTrackBeSoloedVisually(track))) return;

		editor.timeline.updateTracks(updateTrackInSceneTracks({
			tracks: this.savedState,
			trackId: this.trackId,
			update: (current) => ({ ...current, solo: current.solo !== true }),
		}));
		return undefined;
	}

	undo(): void {
		if (this.savedState) EditorCore.getInstance().timeline.updateTracks(this.savedState);
	}
}

function canTrackBeSoloedVisually(track: { type: string }): boolean {
	return track.type !== "audio";
}

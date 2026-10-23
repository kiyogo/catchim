import { Command, type CommandResult } from "@/commands/base-command";
import { EditorCore } from "@/core";
import { closeTrackGapStarts } from "@/timeline/close-gaps";
import type { SceneTracks } from "@/timeline/types";
import { isTrackLocked } from "@/timeline/track-capabilities";
import {
	findTrackInSceneTracks,
	updateTrackInSceneTracks,
} from "@/timeline/track-element-update";
import { mediaTime } from "@/wasm";

export class CloseTrackGapsCommand extends Command {
	override readonly appliesOwnRipple = true;
	private savedState: SceneTracks | null = null;
	private appliedState: SceneTracks | null = null;

	constructor(private readonly trackId: string) {
		super();
	}

	execute(): CommandResult | undefined {
		const editor = EditorCore.getInstance();
		const tracks = editor.scenes.getActiveScene().tracks;
		const track = findTrackInSceneTracks({ tracks, trackId: this.trackId });
		if (!track || isTrackLocked(track) || track.elements.length < 2) return;
		const starts = closeTrackGapStarts({
			starts: track.elements.map((element) => element.startTime),
			durations: track.elements.map((element) => element.duration),
		});
		if (!starts || starts.every((start, index) => start === track.elements[index].startTime)) return;

		this.savedState = tracks;
		this.appliedState = updateTrackInSceneTracks({
			tracks,
			trackId: this.trackId,
			update: (current) => ({
				...current,
				elements: current.elements.map((element, index) => ({
					...element,
					startTime: mediaTime({ ticks: starts[index] }),
				})),
			}),
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

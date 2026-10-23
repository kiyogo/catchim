import { Command, type CommandResult } from "@/commands/base-command";
import type { SceneTracks } from "@/timeline";
import { EditorCore } from "@/core";
import type { TimelineTrack } from "@/timeline";
import { findTrackInSceneTracks } from "@/timeline/track-element-update";
import { isTrackLocked } from "@/timeline/track-capabilities";

function removeTrackElements<TTrack extends TimelineTrack>({
	track,
	elements,
}: {
	track: TTrack;
	elements: { trackId: string; elementId: string }[];
}): TTrack {
	const nextElements = track.elements.filter(
		(element) =>
			!elements.some(
				(target) =>
					target.trackId === track.id && target.elementId === element.id,
			),
	);

	return { ...track, elements: nextElements } as TTrack;
}

export class DeleteElementsCommand extends Command {
	private savedState: SceneTracks | null = null;
	private readonly elements: { trackId: string; elementId: string }[];

	constructor({
		elements,
	}: {
		elements: { trackId: string; elementId: string }[];
	}) {
		super();
		this.elements = elements;
	}

	execute(): CommandResult | undefined {
		const editor = EditorCore.getInstance();
		this.savedState = editor.scenes.getActiveScene().tracks;
		const editableElements = this.elements.filter(({ trackId }) => {
			const track = findTrackInSceneTracks({ tracks: this.savedState!, trackId });
			return track != null && !isTrackLocked(track);
		});
		if (editableElements.length === 0) return;
		const retainedLockedSelection = this.elements.filter(({ trackId }) => {
			const track = findTrackInSceneTracks({ tracks: this.savedState!, trackId });
			return track != null && isTrackLocked(track);
		});

		const updatedTracks: SceneTracks = {
			overlay: this.savedState.overlay.map((track) =>
				removeTrackElements({ track, elements: editableElements }),
			),
			main: removeTrackElements({
				track: this.savedState.main,
				elements: editableElements,
			}),
			audio: this.savedState.audio.map((track) =>
				removeTrackElements({ track, elements: editableElements }),
			),
		};

		editor.timeline.updateTracks(updatedTracks);

		return {
			selection: {
				selectedElements: retainedLockedSelection,
				selectedKeyframes: [],
				keyframeSelectionAnchor: null,
				selectedMaskPoints: null,
			},
		};
	}

	undo(): void {
		if (this.savedState) {
			const editor = EditorCore.getInstance();
			editor.timeline.updateTracks(this.savedState);
		}
	}
}

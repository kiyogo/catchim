import { Command, type CommandResult } from "@/commands/base-command";
import { EditorCore } from "@/core";
import type { ElementRef, SceneTracks } from "@/timeline";
import { generateUUID } from "@/utils/id";
import { isTrackLocked } from "@/timeline/track-capabilities";
import { findTrackInSceneTracks, updateElementInSceneTracks } from "@/timeline/track-element-update";

export class LinkElementsCommand extends Command {
	private savedState: SceneTracks | null = null;
	private appliedState: SceneTracks | null = null;
	private readonly linkGroupId = generateUUID();

	constructor(
		private readonly elements: ElementRef[],
		private readonly linked: boolean,
	) {
		super();
	}

	execute(): CommandResult | undefined {
		if (this.elements.length < 2 && this.linked) return;
		const editor = EditorCore.getInstance();
		const tracks = editor.scenes.getActiveScene().tracks;
		const editable = this.elements.filter(({ trackId, elementId }) => {
			const track = findTrackInSceneTracks({ tracks, trackId });
			return track && !isTrackLocked(track) && track.elements.some(({ id }) => id === elementId);
		});
		if (editable.length < (this.linked ? 2 : 1)) return;

		this.savedState = tracks;
		let next = tracks;
		for (const ref of editable) {
			next = updateElementInSceneTracks({
				tracks: next,
				trackId: ref.trackId,
				elementId: ref.elementId,
				update: (element) => {
					if (this.linked) return { ...element, linkGroupId: this.linkGroupId };
					const { linkGroupId: _linkGroupId, ...unlinkedElement } = element;
					return unlinkedElement;
				},
			});
		}
		this.appliedState = next;
		editor.timeline.updateTracks(next);
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

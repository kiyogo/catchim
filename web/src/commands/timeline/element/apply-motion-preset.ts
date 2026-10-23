import { Command, type CommandResult } from "@/commands/base-command";
import { EditorCore } from "@/core";
import type { ElementRef, SceneTracks, VisualElement } from "@/timeline";
import { findTrackInSceneTracks, updateElementInSceneTracks } from "@/timeline/track-element-update";
import { isTrackLocked } from "@/timeline/track-capabilities";
import { isVisualElement } from "@/timeline/element-utils";
import type { MotionDirection, MotionEasing, MotionPresetId } from "@/motion/presets";
import { applyMotionPresetToElement } from "@/motion/presets";
import type { MediaTime } from "@/wasm";

export class ApplyMotionPresetCommand extends Command {
	private savedState: SceneTracks | null = null;
	private appliedState: SceneTracks | null = null;

	constructor(
		private readonly elementRef: ElementRef,
		private readonly preset: MotionPresetId,
		private readonly direction: MotionDirection,
		private readonly duration: MediaTime,
		private readonly travelDistance: number,
		private readonly easing: MotionEasing,
	) {
		super();
	}

	execute(): CommandResult | undefined {
		const editor = EditorCore.getInstance();
		const tracks = editor.scenes.getActiveScene().tracks;
		this.savedState = tracks;
		const track = findTrackInSceneTracks({ tracks, trackId: this.elementRef.trackId });
		const element = track?.elements.find(({ id }) => id === this.elementRef.elementId);
		if (!track || !element || isTrackLocked(track) || !isVisualElement(element)) return;

		const nextElement = applyMotionPresetToElement({
			element: element as VisualElement,
			preset: this.preset,
			direction: this.direction,
			duration: this.duration,
			travelDistance: this.travelDistance,
			easing: this.easing,
		});
		if (nextElement === element) return;
		this.appliedState = updateElementInSceneTracks({
			tracks,
			trackId: track.id,
			elementId: element.id,
			update: () => nextElement,
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

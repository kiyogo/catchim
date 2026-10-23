import { Command, type CommandResult } from "@/commands/base-command";
import { EditorCore } from "@/core";
import { isVisualElement, updateElementInSceneTracks } from "@/timeline";
import type { SceneTracks, VisualElement } from "@/timeline";
import { buildDefaultEffectInstance } from "@/effects";
import { isTrackLocked } from "@/timeline/track-capabilities";
import { findTrackInSceneTracks } from "@/timeline/track-element-update";

function addEffectToElement({
	element,
	effectType,
}: {
	element: VisualElement;
	effectType: string;
}): VisualElement {
	const instance = buildDefaultEffectInstance({ effectType });
	const currentEffects = element.effects ?? [];
	return { ...element, effects: [...currentEffects, instance] };
}

export class AddClipEffectCommand extends Command {
	private savedState: SceneTracks | null = null;
	private effectId: string | null = null;
	private readonly trackId: string;
	private readonly elementId: string;
	private readonly effectType: string;

	constructor({
		trackId,
		elementId,
		effectType,
	}: {
		trackId: string;
		elementId: string;
		effectType: string;
	}) {
		super();
		this.trackId = trackId;
		this.elementId = elementId;
		this.effectType = effectType;
	}

	execute(): CommandResult | undefined {
		const editor = EditorCore.getInstance();
		this.savedState = editor.scenes.getActiveScene().tracks;
		const track = findTrackInSceneTracks({
			tracks: this.savedState,
			trackId: this.trackId,
		});
		const element = track?.elements.find(({ id }) => id === this.elementId);
		if (!track || !element || isTrackLocked(track) || !isVisualElement(element)) {
			this.savedState = null;
			return;
		}

		const updatedTracks = updateElementInSceneTracks({
			tracks: this.savedState,
			trackId: this.trackId,
			elementId: this.elementId,
			update: (currentElement) => {
				const updated = addEffectToElement({
					element: currentElement as VisualElement,
					effectType: this.effectType,
				});
				const effects = updated.effects ?? [];
				this.effectId = effects[effects.length - 1]?.id ?? null;
				return updated;
			},
		});

		editor.timeline.updateTracks(updatedTracks);
		return undefined;
	}

	undo(): void {
		if (this.savedState) {
			const editor = EditorCore.getInstance();
			editor.timeline.updateTracks(this.savedState);
		}
	}

	getEffectId(): string | null {
		return this.effectId;
	}
}

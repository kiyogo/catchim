import type { EditorSelectionPatch } from "@/selection/editor-selection";
import type { ElementRef } from "@/timeline/types";

export interface CommandResult {
	selection?: EditorSelectionPatch;
}

export function createElementSelectionResult(
	selectedElements: ElementRef[],
): CommandResult {
	return {
		selection: {
			selectedElements,
			selectedKeyframes: [],
			keyframeSelectionAnchor: null,
			selectedMaskPoints: null,
		},
	};
}

export abstract class Command {
	/** Commands that fully plan time positions can opt out of automatic ripple. */
	readonly appliesOwnRipple: boolean = false;
	/** Allows explicit ripple operations while the editor-wide ripple toggle is off. */
	readonly forcesRipple: boolean = false;
	/** Enables closing a vacated interval created by moving an element's leading edge. */
	readonly ripplesLeadingEdge: boolean = false;

	abstract execute(): CommandResult | undefined;

	undo(): void {
		throw new Error("Undo not implemented for this command");
	}

	redo(): CommandResult | undefined {
		return this.execute();
	}
}

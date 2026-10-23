import type { ElementRef, SceneTracks, TimelineElement, TimelineTrack } from "@/timeline";
import { isVisualElement } from "@/timeline/element-utils";
import { isTrackLocked } from "@/timeline/track-capabilities";
import type { FadeMode } from "./apply-fade";

type SelectedVisual = { track: TimelineTrack; element: TimelineElement };

export function resolveCrossDissolve({
	selected,
	tracks,
}: {
	selected: SelectedVisual[];
	tracks: SceneTracks | null;
}): { target: ElementRef; mode: FadeMode; duration: number } | null {
	if (!tracks || selected.length !== 2) return null;
	const [first, second] = selected;
	if (
		!first ||
		!second ||
		isTrackLocked(first.track) ||
		isTrackLocked(second.track) ||
		!isVisualElement(first.element) ||
		!isVisualElement(second.element)
	) {
		return null;
	}

	const [outgoing, incoming] =
		first.element.startTime < second.element.startTime
			? [first, second]
			: first.element.startTime > second.element.startTime
				? [second, first]
				: [null, null];
	if (!outgoing || !incoming) return null;

	const overlapStart = incoming.element.startTime;
	const overlapEnd = Math.min(
		outgoing.element.startTime + outgoing.element.duration,
		incoming.element.startTime + incoming.element.duration,
	);
	const duration = overlapEnd - overlapStart;
	if (duration <= 0) return null;

	const layerRank = (trackId: string) => {
		if (trackId === tracks.main.id) return 0;
		const overlayIndex = tracks.overlay.findIndex(({ id }) => id === trackId);
		return overlayIndex < 0 ? -1 : tracks.overlay.length - overlayIndex;
	};
	// Fading the incoming, upper clip reveals the outgoing image underneath it.
	// The opposite layer ordering needs a two-sided transition model so it is
	// rejected rather than applying a fade with misleading visual results.
	if (layerRank(incoming.track.id) <= layerRank(outgoing.track.id)) return null;

	return {
		target: { trackId: incoming.track.id, elementId: incoming.element.id },
		mode: "in",
		duration,
	};
}

import type { DropTarget } from "@/timeline";
import type { MediaTime } from "@/wasm";

export function preserveEmptyMainTrackStart({
	target,
	mainTrackIsEmpty,
	mainTrackIndex,
	requestedStartTime,
}: {
	target: DropTarget;
	mainTrackIsEmpty: boolean;
	mainTrackIndex: number;
	requestedStartTime: MediaTime;
}): DropTarget {
	if (
		!mainTrackIsEmpty ||
		target.isNewTrack ||
		target.trackIndex !== mainTrackIndex ||
		requestedStartTime === 0
	) {
		return target;
	}

	return {
		...target,
		trackIndex: mainTrackIndex,
		isNewTrack: true,
		insertPosition: "above",
		xPosition: requestedStartTime,
	};
}

import type { TimelineTrack } from "@/timeline";
import { getRustTimelineSelectionBindings } from "@/native/rust-engine";
import { timelineTimeToPixels } from "@/timeline/pixel-utils";
import {
	TIMELINE_CONTENT_TOP_PADDING_PX,
} from "./layout";
import { getCumulativeHeightBefore, getTrackHeight } from "./track-layout";

type TimelineElementRef = { trackId: string; elementId: string };

interface SelectionRectangle {
	left: number;
	top: number;
	right: number;
	bottom: number;
}

function getNormalizedRectangle({
	startPos,
	endPos,
}: {
	startPos: { x: number; y: number };
	endPos: { x: number; y: number };
}): SelectionRectangle {
	return {
		left: Math.min(startPos.x, endPos.x),
		top: Math.min(startPos.y, endPos.y),
		right: Math.max(startPos.x, endPos.x),
		bottom: Math.max(startPos.y, endPos.y),
	};
}

function getSelectionRectangleInContent({
	container,
	scrollContainer,
	startPos,
	endPos,
}: {
	container: HTMLElement;
	scrollContainer: HTMLDivElement | null;
	startPos: { x: number; y: number };
	endPos: { x: number; y: number };
}): SelectionRectangle {
	const containerRect = container.getBoundingClientRect();
	const scrollRect = scrollContainer?.getBoundingClientRect() ?? containerRect;
	const scrollLeft = scrollContainer?.scrollLeft ?? 0;
	const scrollTop = scrollContainer?.scrollTop ?? 0;

	const adjustedStart = {
		x: startPos.x - containerRect.left + scrollLeft,
		y: startPos.y - scrollRect.top + scrollTop,
	};
	const adjustedEnd = {
		x: endPos.x - containerRect.left + scrollLeft,
		y: endPos.y - scrollRect.top + scrollTop,
	};

	return getNormalizedRectangle({
		startPos: adjustedStart,
		endPos: adjustedEnd,
	});
}

function isRectangleIntersecting({
	elementRectangle,
	selectionRectangle,
}: {
	elementRectangle: SelectionRectangle;
	selectionRectangle: SelectionRectangle;
}): boolean {
	return !(
		elementRectangle.right < selectionRectangle.left ||
		elementRectangle.left > selectionRectangle.right ||
		elementRectangle.bottom < selectionRectangle.top ||
		elementRectangle.top > selectionRectangle.bottom
	);
}

export function resolveTimelineElementIntersections({
	container,
	scrollContainer,
	tracks,
	zoomLevel,
	startPos,
	currentPos,
}: {
	container: HTMLElement;
	scrollContainer: HTMLDivElement | null;
	tracks: TimelineTrack[];
	zoomLevel: number;
	startPos: { x: number; y: number };
	currentPos: { x: number; y: number };
}): TimelineElementRef[] {
	const selectionRectangle = getSelectionRectangleInContent({
		container,
		scrollContainer,
		startPos,
		endPos: currentPos,
	});
	const candidates: Array<{
		ref: TimelineElementRef;
		rectangle: SelectionRectangle;
	}> = [];

	for (const [trackIndex, track] of tracks.entries()) {
		const trackTop = getCumulativeHeightBefore({
			tracks,
			trackIndex,
		});
		const trackHeight = getTrackHeight({ type: track.type });
		const elementTop = TIMELINE_CONTENT_TOP_PADDING_PX + trackTop;
		const elementBottom = elementTop + trackHeight;

		// Fast vertical culling: skip track completely if outside selection rectangle
		if (
			elementBottom < selectionRectangle.top ||
			elementTop > selectionRectangle.bottom
		) {
			continue;
		}

		for (const element of track.elements) {
			const elementLeft = timelineTimeToPixels({ time: element.startTime, zoomLevel });
			const elementRight = timelineTimeToPixels({ time: element.startTime + element.duration, zoomLevel });

			candidates.push({
				ref: {
					trackId: track.id,
					elementId: element.id,
				},
				rectangle: {
					left: elementLeft,
					top: elementTop,
					right: elementRight,
					bottom: elementBottom,
				},
			});
		}
	}

	const rust = getRustTimelineSelectionBindings();
	if (rust && candidates.length > 0) {
		const rectangles = new Float64Array(candidates.length * 4);
		for (let index = 0; index < candidates.length; index += 1) {
			const { left, top, right, bottom } = candidates[index].rectangle;
			const offset = index * 4;
			rectangles[offset] = left;
			rectangles[offset + 1] = top;
			rectangles[offset + 2] = right;
			rectangles[offset + 3] = bottom;
		}

		const selectedIndices = rust.selectRectIntersections(
			rectangles,
			Float64Array.of(
				selectionRectangle.left,
				selectionRectangle.top,
				selectionRectangle.right,
				selectionRectangle.bottom,
			),
		);
		if (selectedIndices) {
			return selectedIndices.flatMap((index) =>
				candidates[index] ? [candidates[index].ref] : [],
			);
		}
	}

	return candidates
		.filter(({ rectangle }) =>
			isRectangleIntersecting({
				elementRectangle: rectangle,
				selectionRectangle,
			}),
		)
		.map(({ ref }) => ref);
}

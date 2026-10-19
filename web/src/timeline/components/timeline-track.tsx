"use client";

import { TimelineTrackCanvas } from "./timeline-track-canvas";
import type { TimelineTrack } from "@/timeline";
import type { TimelineElement as TimelineElementType } from "@/timeline";
import type { ElementDragView } from "@/timeline";

interface TimelineTrackContentProps {
	track: TimelineTrack;
	zoomLevel: number;
	dynamicTimelineWidth?: number;
	dragView: ElementDragView;
	onResizeStart: (params: {
		event: React.MouseEvent;
		element: TimelineElementType;
		track: TimelineTrack;
		side: "left" | "right";
	}) => void;
	onElementMouseDown: (params: {
		event: React.MouseEvent;
		element: TimelineElementType;
		track: TimelineTrack;
	}) => void;
	onElementClick: (params: {
		event: React.MouseEvent;
		element: TimelineElementType;
		track: TimelineTrack;
	}) => void;
	onTrackMouseDown?: (event: React.MouseEvent) => void;
	onTrackMouseUp?: (event: React.MouseEvent) => void;
	shouldIgnoreClick?: () => boolean;
	targetElementId?: string | null;
}

export function TimelineTrackContent({
	track,
	zoomLevel,
	dynamicTimelineWidth,
	dragView,
	onResizeStart,
	onElementMouseDown,
	onElementClick,
	onTrackMouseDown,
	onTrackMouseUp,
	shouldIgnoreClick,
	targetElementId = null,
}: TimelineTrackContentProps) {
	return (
		<div className="relative size-full">
			<TimelineTrackCanvas
				track={track}
				zoomLevel={zoomLevel}
				dynamicTimelineWidth={dynamicTimelineWidth}
				dragView={dragView}
				onResizeStart={onResizeStart}
				onElementMouseDown={onElementMouseDown}
				onElementClick={onElementClick}
				onTrackMouseDown={onTrackMouseDown}
				onTrackMouseUp={onTrackMouseUp}
				shouldIgnoreClick={shouldIgnoreClick}
				targetElementId={targetElementId}
			/>
		</div>
	);
}

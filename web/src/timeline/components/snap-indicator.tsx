"use client";

import { useRef, useEffect } from "react";
import { useSnapIndicatorPosition } from "@/timeline/hooks/use-snap-indicator-position";
import type { SnapPoint } from "@/timeline/snapping";
import {
	getCenteredLineLeft,
	TIMELINE_INDICATOR_LINE_WIDTH_PX,
} from "@/timeline";
import { TIMELINE_LAYERS } from "./layers";

interface SnapIndicatorProps {
	snapPoint: SnapPoint | null;
	zoomLevel: number;
	isVisible: boolean;
	timelineRef: React.RefObject<HTMLDivElement | null>;
	tracksScrollRef: React.RefObject<HTMLDivElement | null>;
}

export function SnapIndicator({
	snapPoint,
	zoomLevel,
	isVisible,
	timelineRef,
	tracksScrollRef,
}: SnapIndicatorProps) {
	const canvasRef = useRef<HTMLCanvasElement | null>(null);
	const { leftPosition, topPosition, height } = useSnapIndicatorPosition({
		snapPoint,
		zoomLevel,
		timelineRef,
		tracksScrollRef,
	});

	useEffect(() => {
		if (!isVisible || !snapPoint) return;
		const canvas = canvasRef.current;
		if (!canvas) return;

		const dpr = typeof window !== "undefined" ? window.devicePixelRatio || 1 : 1;
		const w = 4;
		const h = Math.max(10, height);

		canvas.width = Math.ceil(w * dpr);
		canvas.height = Math.ceil(h * dpr);
		canvas.style.width = `${w}px`;
		canvas.style.height = `${h}px`;

		const ctx = canvas.getContext("2d");
		if (!ctx) return;

		ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
		ctx.clearRect(0, 0, w, h);

		ctx.save();
		ctx.strokeStyle = "#38bdf8";
		ctx.shadowColor = "rgba(56, 189, 248, 0.75)";
		ctx.shadowBlur = 4;
		ctx.lineWidth = 1.5;
		ctx.beginPath();
		ctx.moveTo(w / 2, 0);
		ctx.lineTo(w / 2, h);
		ctx.stroke();
		ctx.restore();
	}, [isVisible, snapPoint, height]);

	if (!isVisible || !snapPoint) {
		return null;
	}

	return (
		<div
			className="pointer-events-none absolute"
			style={{
				left: `${getCenteredLineLeft({ centerPixel: leftPosition }) - 1}px`,
				top: topPosition,
				height: `${height}px`,
				width: `4px`,
				zIndex: TIMELINE_LAYERS.snapIndicator,
			}}
		>
			<canvas ref={canvasRef} data-engine="canvas-snap-indicator" className="block size-full" />
		</div>
	);
}

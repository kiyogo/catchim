"use client";

import { useEffect, useRef } from "react";
import { getDropLineY } from "./drop-target";
import type { TimelineTrack, DropTarget } from "@/timeline";
import { TIMELINE_LAYERS } from "./layers";

interface DragLineProps {
	dropTarget: DropTarget | null;
	tracks: TimelineTrack[];
	isVisible: boolean;
	headerHeight?: number;
}

export function DragLine({
	dropTarget,
	tracks,
	isVisible,
	headerHeight = 0,
}: DragLineProps) {
	const canvasRef = useRef<HTMLCanvasElement | null>(null);

	const y = dropTarget ? getDropLineY({ dropTarget, tracks }) : 0;
	const lineTop = y + headerHeight;

	useEffect(() => {
		const canvas = canvasRef.current;
		if (!canvas || !isVisible || !dropTarget) return;

		const parent = canvas.parentElement;
		const width = parent?.clientWidth || 0;
		if (width <= 0) return;

		const height = 4;
		const dpr = typeof window !== "undefined" ? window.devicePixelRatio || 1 : 1;

		if (canvas.width !== Math.round(width * dpr) || canvas.height !== Math.round(height * dpr)) {
			canvas.width = Math.round(width * dpr);
			canvas.height = Math.round(height * dpr);
			canvas.style.width = `${width}px`;
			canvas.style.height = `${height}px`;
		}

		const ctx = canvas.getContext("2d");
		if (!ctx) return;

		ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
		ctx.clearRect(0, 0, width, height);

		ctx.fillStyle = "#38bdf8";
		ctx.fillRect(0, 1, width, 2);
	}, [dropTarget, isVisible, lineTop, tracks]);

	if (!isVisible || !dropTarget) return null;

	return (
		<div
			className="pointer-events-none absolute right-0 left-0 overflow-hidden"
			style={{ top: `${lineTop - 1}px`, height: "4px", zIndex: TIMELINE_LAYERS.dragLine }}
		>
			<canvas ref={canvasRef} data-engine="canvas-drag-line" className="block size-full" />
		</div>
	);
}

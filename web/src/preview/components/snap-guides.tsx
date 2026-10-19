"use client";

import { useEffect, useRef } from "react";
import { usePreviewViewport } from "@/preview/components/preview-viewport";
import type { SnapLine } from "@/preview/preview-snap";

export function SnapGuides({ lines }: { lines: SnapLine[] }) {
	const viewport = usePreviewViewport();
	const canvasRef = useRef<HTMLCanvasElement | null>(null);

	const toOverlayX = (logicalX: number) =>
		viewport.positionToOverlay({
			positionX: logicalX,
			positionY: 0,
		}).x;

	const toOverlayY = (logicalY: number) =>
		viewport.positionToOverlay({
			positionX: 0,
			positionY: logicalY,
		}).y;

	useEffect(() => {
		const canvas = canvasRef.current;
		if (!canvas) return;

		const parent = canvas.parentElement;
		const width = parent?.clientWidth || 0;
		const height = parent?.clientHeight || 0;
		if (width <= 0 || height <= 0) return;

		const dpr = typeof window !== "undefined" ? window.devicePixelRatio || 1 : 1;
		const pixelW = Math.round(width * dpr);
		const pixelH = Math.round(height * dpr);

		if (canvas.width !== pixelW || canvas.height !== pixelH) {
			canvas.width = pixelW;
			canvas.height = pixelH;
			canvas.style.width = `${width}px`;
			canvas.style.height = `${height}px`;
		}

		const ctx = canvas.getContext("2d");
		if (!ctx) return;

		ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
		ctx.clearRect(0, 0, width, height);

		if (lines.length === 0) return;

		ctx.strokeStyle = "rgba(255, 255, 255, 0.70)";
		ctx.lineWidth = 1;
		ctx.beginPath();

		for (const line of lines) {
			if (line.type === "vertical") {
				const x = Math.round(toOverlayX(line.position)) + 0.5;
				ctx.moveTo(x, 0);
				ctx.lineTo(x, height);
			} else {
				const y = Math.round(toOverlayY(line.position)) + 0.5;
				ctx.moveTo(0, y);
				ctx.lineTo(width, y);
			}
		}

		ctx.stroke();
	}, [lines, viewport]);

	if (lines.length === 0) {
		return null;
	}

	return (
		<div className="pointer-events-none absolute inset-0 overflow-hidden" aria-hidden>
			<canvas ref={canvasRef} data-engine="canvas-preview-snap-guides" className="block size-full" />
		</div>
	);
}

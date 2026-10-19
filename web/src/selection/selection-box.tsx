"use client";

import { useEffect, useRef } from "react";
import type { SelectionBoxBounds } from "@/selection/types";

interface SelectionBoxProps {
	bounds: SelectionBoxBounds | null;
}

export function SelectionBox({ bounds }: SelectionBoxProps) {
	const canvasRef = useRef<HTMLCanvasElement | null>(null);

	useEffect(() => {
		const canvas = canvasRef.current;
		if (!canvas || !bounds) return;

		const dpr = typeof window !== "undefined" ? window.devicePixelRatio || 1 : 1;
		const width = Math.max(1, Math.round(bounds.width));
		const height = Math.max(1, Math.round(bounds.height));

		if (canvas.width !== Math.round(width * dpr) || canvas.height !== Math.round(height * dpr)) {
			canvas.width = Math.round(width * dpr);
			canvas.height = Math.round(height * dpr);
		}

		const ctx = canvas.getContext("2d");
		if (!ctx) return;

		ctx.save();
		ctx.scale(dpr, dpr);
		ctx.clearRect(0, 0, width, height);

		// Translucent fill matching bg-foreground/5
		ctx.fillStyle = "rgba(255, 255, 255, 0.05)";
		ctx.fillRect(0, 0, width, height);

		// 1px border matching border-foreground/50
		ctx.strokeStyle = "rgba(255, 255, 255, 0.5)";
		ctx.lineWidth = 1;
		ctx.strokeRect(0.5, 0.5, width - 1, height - 1);

		ctx.restore();
	}, [bounds]);

	if (!bounds) return null;

	return (
		<canvas
			ref={canvasRef}
			data-engine="canvas-selection"
			style={{
				position: "absolute",
				left: `${bounds.left}px`,
				top: `${bounds.top}px`,
				width: `${bounds.width}px`,
				height: `${bounds.height}px`,
				pointerEvents: "none",
				zIndex: 50,
			}}
		/>
	);
}

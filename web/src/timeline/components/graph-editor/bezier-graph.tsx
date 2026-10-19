"use client";

import { useRef, useState, useEffect, type PointerEvent } from "react";
import { useShiftKey } from "@/hooks/use-shift-key";
import { useCommittedRef } from "@/hooks/use-committed-ref";
import { getBezierPoint } from "@/animation/bezier";
import type { NormalizedCubicBezier } from "@/animation/types";
import { cn } from "@/utils/ui";

const GRAPH_WIDTH = 140;
const GRAPH_HEIGHT = 94;
const GRAPH_PADDING = 12;
const CANVAS_WIDTH = GRAPH_WIDTH + GRAPH_PADDING * 2;
const CANVAS_HEIGHT = GRAPH_HEIGHT + GRAPH_PADDING * 2;
const HANDLE_RADIUS = 3.5;
const ENDPOINT_RADIUS = 2;
const SNAP_THRESHOLD = 0.06;
const SNAP_TARGETS = [0, 1];
const CURVE_SEGMENTS = 64;
const Y_CLAMP_MIN = -0.5;
const Y_CLAMP_MAX = 1.5;

type BezierHandle = "c1" | "c2";

export const BEZIER_GRAPH_MIN_HEIGHT = CANVAS_HEIGHT;

function snap({
	value,
	targets,
	isEnabled,
}: {
	value: number;
	targets: number[];
	isEnabled: boolean;
}) {
	if (!isEnabled) return value;
	for (const target of targets) {
		if (Math.abs(value - target) < SNAP_THRESHOLD) return target;
	}
	return value;
}

function toCanvasX({ value }: { value: number }) {
	return GRAPH_PADDING + value * GRAPH_WIDTH;
}

function toCanvasY({ value }: { value: number }) {
	return GRAPH_PADDING + (1 - value) * GRAPH_HEIGHT;
}

function fromCanvasX({ canvasX }: { canvasX: number }) {
	return Math.max(0, Math.min(1, (canvasX - GRAPH_PADDING) / GRAPH_WIDTH));
}

function fromCanvasY({ canvasY }: { canvasY: number }) {
	return Math.max(
		Y_CLAMP_MIN,
		Math.min(Y_CLAMP_MAX, 1 - (canvasY - GRAPH_PADDING) / GRAPH_HEIGHT),
	);
}

function clampHandleY({ canvasY }: { canvasY: number }) {
	return Math.max(HANDLE_RADIUS, Math.min(CANVAS_HEIGHT - HANDLE_RADIUS, canvasY));
}

export function BezierGraph({
	value,
	onChange,
	onChangeEnd,
	onCancel,
}: {
	value: NormalizedCubicBezier;
	onChange?: (value: NormalizedCubicBezier) => void;
	onChangeEnd?: (value: NormalizedCubicBezier) => void;
	onCancel?: () => void;
}) {
	const canvasRef = useRef<HTMLCanvasElement>(null);
	const [activeHandle, setActiveHandle] = useState<BezierHandle | null>(null);
	const [hoverHandle, setHoverHandle] = useState<BezierHandle | null>(null);
	const isShiftPressedRef = useShiftKey();
	const latestValueRef = useCommittedRef(value);

	function getPointerPosition({
		event,
	}: {
		event: PointerEvent;
	}): { x: number; y: number } {
		const canvas = canvasRef.current;
		if (!canvas) return { x: 0, y: 0 };
		const rect = canvas.getBoundingClientRect();
		const scale = CANVAS_WIDTH / rect.width;
		return {
			x: (event.clientX - rect.left) * scale,
			y: (event.clientY - rect.top) * (CANVAS_HEIGHT / rect.height),
		};
	}

	const c1 = { x: toCanvasX({ value: value[0] }), y: toCanvasY({ value: value[1] }) };
	const c2 = { x: toCanvasX({ value: value[2] }), y: toCanvasY({ value: value[3] }) };
	const c1Clamped = { x: c1.x, y: clampHandleY({ canvasY: c1.y }) };
	const c2Clamped = { x: c2.x, y: clampHandleY({ canvasY: c2.y }) };
	const p0 = { x: toCanvasX({ value: 0 }), y: toCanvasY({ value: 0 }) };
	const p1 = { x: toCanvasX({ value: 1 }), y: toCanvasY({ value: 1 }) };

	// Render canvas
	useEffect(() => {
		const canvas = canvasRef.current;
		if (!canvas) return;

		const dpr = typeof window !== "undefined" ? window.devicePixelRatio || 1 : 1;
		if (canvas.width !== Math.round(CANVAS_WIDTH * dpr) || canvas.height !== Math.round(CANVAS_HEIGHT * dpr)) {
			canvas.width = Math.round(CANVAS_WIDTH * dpr);
			canvas.height = Math.round(CANVAS_HEIGHT * dpr);
		}

		const ctx = canvas.getContext("2d");
		if (!ctx) return;

		ctx.save();
		ctx.scale(dpr, dpr);
		ctx.clearRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);

		// Background subtle fill
		ctx.fillStyle = "rgba(255, 255, 255, 0.03)";
		ctx.fillRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);

		// Diagonal guide line (p0 to p1)
		ctx.save();
		ctx.strokeStyle = "rgba(255, 255, 255, 0.08)";
		ctx.lineWidth = 1;
		ctx.setLineDash([3, 3]);
		ctx.beginPath();
		ctx.moveTo(p0.x, p0.y);
		ctx.lineTo(p1.x, p1.y);
		ctx.stroke();
		ctx.restore();

		// Handle lines
		ctx.save();
		ctx.strokeStyle = "rgba(56, 189, 248, 0.35)";
		ctx.lineWidth = 1;
		ctx.beginPath();
		ctx.moveTo(p0.x, p0.y);
		ctx.lineTo(c1Clamped.x, c1Clamped.y);
		ctx.moveTo(p1.x, p1.y);
		ctx.lineTo(c2Clamped.x, c2Clamped.y);
		ctx.stroke();
		ctx.restore();

		// Cubic Bezier curve
		ctx.save();
		ctx.strokeStyle = "#0ea5e9";
		ctx.lineWidth = 2;
		ctx.lineCap = "round";
		ctx.beginPath();
		for (let i = 0; i <= CURVE_SEGMENTS; i++) {
			const progress = i / CURVE_SEGMENTS;
			const x = toCanvasX({ value: getBezierPoint({ progress, p0: 0, p1: value[0], p2: value[2], p3: 1 }) });
			const y = toCanvasY({ value: getBezierPoint({ progress, p0: 0, p1: value[1], p2: value[3], p3: 1 }) });
			if (i === 0) {
				ctx.moveTo(x, y);
			} else {
				ctx.lineTo(x, y);
			}
		}
		ctx.stroke();
		ctx.restore();

		// Endpoints p0 and p1
		ctx.save();
		ctx.fillStyle = "rgba(255, 255, 255, 0.3)";
		ctx.beginPath();
		ctx.arc(p0.x, p0.y, ENDPOINT_RADIUS, 0, Math.PI * 2);
		ctx.fill();
		ctx.beginPath();
		ctx.arc(p1.x, p1.y, ENDPOINT_RADIUS, 0, Math.PI * 2);
		ctx.fill();
		ctx.restore();

		// Handle circles c1 and c2
		const drawHandle = (pt: { x: number; y: number }, isActive: boolean, isHover: boolean) => {
			ctx.save();
			ctx.fillStyle = isActive || isHover ? "#38bdf8" : "#0ea5e9";
			ctx.shadowColor = "rgba(14, 165, 233, 0.5)";
			ctx.shadowBlur = isActive || isHover ? 6 : 2;
			ctx.beginPath();
			ctx.arc(pt.x, pt.y, HANDLE_RADIUS, 0, Math.PI * 2);
			ctx.fill();
			ctx.strokeStyle = "#ffffff";
			ctx.lineWidth = 1;
			ctx.stroke();
			ctx.restore();
		};

		drawHandle(c1Clamped, activeHandle === "c1", hoverHandle === "c1");
		drawHandle(c2Clamped, activeHandle === "c2", hoverHandle === "c2");

		ctx.restore();
	}, [value, c1Clamped, c2Clamped, p0, p1, activeHandle, hoverHandle]);

	function onPointerDown(event: PointerEvent<HTMLCanvasElement>) {
		const pos = getPointerPosition({ event });
		const d1 = Math.hypot(pos.x - c1Clamped.x, pos.y - c1Clamped.y);
		const d2 = Math.hypot(pos.x - c2Clamped.x, pos.y - c2Clamped.y);
		const hitTarget = 12;

		let selected: BezierHandle | null = null;
		if (d1 < hitTarget && d1 <= d2) {
			selected = "c1";
		} else if (d2 < hitTarget) {
			selected = "c2";
		}

		if (selected) {
			event.preventDefault();
			event.stopPropagation();
			setActiveHandle(selected);
			event.currentTarget.setPointerCapture(event.pointerId);
		}
	}

	function onPointerMove(event: PointerEvent<HTMLCanvasElement>) {
		const pointerPos = getPointerPosition({ event });

		if (!activeHandle) {
			const d1 = Math.hypot(pointerPos.x - c1Clamped.x, pointerPos.y - c1Clamped.y);
			const d2 = Math.hypot(pointerPos.x - c2Clamped.x, pointerPos.y - c2Clamped.y);
			const hitTarget = 12;
			if (d1 < hitTarget && d1 <= d2) {
				setHoverHandle("c1");
			} else if (d2 < hitTarget) {
				setHoverHandle("c2");
			} else {
				setHoverHandle(null);
			}
			return;
		}

		const x = fromCanvasX({ canvasX: pointerPos.x });
		const y = snap({
			value: fromCanvasY({ canvasY: pointerPos.y }),
			targets: SNAP_TARGETS,
			isEnabled: !isShiftPressedRef.current,
		});
		const next: NormalizedCubicBezier = [...value];
		if (activeHandle === "c1") {
			next[0] = x;
			next[1] = y;
		} else {
			next[2] = x;
			next[3] = y;
		}
		latestValueRef.current = next;
		onChange?.(next);
	}

	function onPointerUp() {
		if (!activeHandle) return;
		setActiveHandle(null);
		onChangeEnd?.(latestValueRef.current);
	}

	function onPointerCancel() {
		if (!activeHandle) return;
		setActiveHandle(null);
		onCancel?.();
	}

	return (
		<canvas
			ref={canvasRef}
			data-engine="canvas-bezier-graph"
			style={{
				width: `${CANVAS_WIDTH}px`,
				height: `${CANVAS_HEIGHT}px`,
			}}
			className={cn(
				"w-full select-none rounded",
				activeHandle
					? "cursor-grabbing"
					: hoverHandle
						? "cursor-grab"
						: "cursor-crosshair",
			)}
			onPointerDown={onPointerDown}
			onPointerMove={onPointerMove}
			onPointerUp={onPointerUp}
			onPointerCancel={onPointerCancel}
		/>
	);
}

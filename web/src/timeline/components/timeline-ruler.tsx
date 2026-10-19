"use client";

import { useEffect, useRef } from "react";
import { BASE_TIMELINE_PIXELS_PER_SECOND } from "@/timeline/scale";
import { mediaTimeToSeconds } from "opencut-wasm";
import { TIMELINE_RULER_HEIGHT_PX } from "./layout";
import { DEFAULT_FPS } from "@/fps/defaults";
import { useEditor } from "@/editor/use-editor";
import { getRulerConfig, shouldShowLabel, formatRulerLabel } from "@/timeline/ruler-utils";
import { useScrollPosition } from "@/timeline/hooks/use-scroll-position";
import { useTranslation } from "@/i18n";

interface TimelineRulerProps {
	zoomLevel: number;
	dynamicTimelineWidth: number;
	rulerRef: React.Ref<HTMLDivElement>;
	tracksScrollRef: React.RefObject<HTMLElement | null>;
	handleWheel: (e: React.WheelEvent) => void;
	handleTimelineContentClick: (e: React.MouseEvent) => void;
	handleRulerTrackingMouseDown: (e: React.MouseEvent) => void;
	handleRulerMouseDown: (e: React.MouseEvent) => void;
}

export function TimelineRuler({
	zoomLevel,
	dynamicTimelineWidth,
	rulerRef,
	tracksScrollRef,
	handleWheel,
	handleTimelineContentClick,
	handleRulerTrackingMouseDown,
	handleRulerMouseDown,
}: TimelineRulerProps) {
	const { t } = useTranslation();
	const canvasRef = useRef<HTMLCanvasElement | null>(null);
	const durationTicks = useEditor((e) => e.timeline.getTotalDuration());
	const durationSeconds = mediaTimeToSeconds({ time: durationTicks });
	const pixelsPerSecond = BASE_TIMELINE_PIXELS_PER_SECOND * zoomLevel;
	const visibleDurationSeconds = dynamicTimelineWidth / pixelsPerSecond;
	const effectiveDurationSeconds = Math.max(
		durationSeconds,
		visibleDurationSeconds,
	);
	const fps =
		useEditor((e) => e.project.getActiveOrNull()?.settings.fps) ?? DEFAULT_FPS;
	const { labelIntervalSeconds, tickIntervalSeconds } = getRulerConfig({
		zoomLevel,
		fps,
	});

	const { scrollLeft, viewportWidth } = useScrollPosition({
		scrollRef: tracksScrollRef,
	});

	useEffect(() => {
		const canvas = canvasRef.current;
		if (!canvas) return;

		const ctx = canvas.getContext("2d");
		if (!ctx) return;

		const dpr = typeof window !== "undefined" ? window.devicePixelRatio || 1 : 1;
		const width = dynamicTimelineWidth;
		const height = TIMELINE_RULER_HEIGHT_PX;

		if (canvas.width !== Math.round(width * dpr) || canvas.height !== Math.round(height * dpr)) {
			canvas.width = Math.round(width * dpr);
			canvas.height = Math.round(height * dpr);
		}

		ctx.save();
		ctx.scale(dpr, dpr);
		ctx.clearRect(0, 0, width, height);

		const bufferPx = Math.max(200, (scrollLeft + viewportWidth) * 0.15);
		const visibleStartTimeSeconds = Math.max(
			0,
			(scrollLeft - bufferPx) / pixelsPerSecond,
		);
		const visibleEndTimeSeconds =
			(scrollLeft + viewportWidth + bufferPx) / pixelsPerSecond;

		const tickCount =
			Math.ceil(effectiveDurationSeconds / tickIntervalSeconds) + 1;
		const startTickIndex = Math.max(
			0,
			Math.floor(visibleStartTimeSeconds / tickIntervalSeconds),
		);
		const endTickIndex = Math.min(
			tickCount - 1,
			Math.ceil(visibleEndTimeSeconds / tickIntervalSeconds),
		);

		ctx.font = '10px -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif';
		ctx.textBaseline = "top";
		ctx.textAlign = "left";

		ctx.fillStyle = "rgba(161, 161, 170, 0.85)";
		ctx.strokeStyle = "rgba(161, 161, 170, 0.25)";
		ctx.lineWidth = 1;

		ctx.beginPath();
		for (let i = startTickIndex; i <= endTickIndex; i++) {
			const timeSec = i * tickIntervalSeconds;
			if (timeSec > effectiveDurationSeconds) break;

			const x = Math.round(timeSec * pixelsPerSecond) + 0.5;
			const showLabel = shouldShowLabel({
				time: timeSec,
				labelIntervalSeconds,
			});

			if (showLabel) {
				const label = formatRulerLabel({ timeInSeconds: timeSec, fps });
				ctx.fillText(label, x + 2, 4);
				ctx.moveTo(x, 0);
				ctx.lineTo(x, 18);
			} else {
				ctx.moveTo(x, 6);
				ctx.lineTo(x, 12);
			}
		}
		ctx.stroke();

		ctx.restore();
	}, [
		dynamicTimelineWidth,
		effectiveDurationSeconds,
		fps,
		labelIntervalSeconds,
		pixelsPerSecond,
		scrollLeft,
		tickIntervalSeconds,
		viewportWidth,
		zoomLevel,
	]);

	return (
		<div
			role="slider"
			tabIndex={0}
			aria-label={t("timeline.timelineRuler")}
			aria-valuemin={0}
			aria-valuemax={effectiveDurationSeconds}
			aria-valuenow={0}
			className="relative flex-1 overflow-x-visible"
			style={{ height: TIMELINE_RULER_HEIGHT_PX }}
			onWheel={handleWheel}
			onClick={(event) => {
				if (event.target === event.currentTarget) {
					handleTimelineContentClick(event);
				}
			}}
			onMouseDown={handleRulerTrackingMouseDown}
			onKeyDown={() => {}}
		>
			<div
				role="none"
				ref={rulerRef}
				className="relative cursor-default select-none"
				style={{
					height: TIMELINE_RULER_HEIGHT_PX,
					width: `${dynamicTimelineWidth}px`,
				}}
				onMouseDown={handleRulerMouseDown}
			>
				<canvas
					ref={canvasRef}
					data-engine="canvas-ruler"
					style={{
						width: `${dynamicTimelineWidth}px`,
						height: `${TIMELINE_RULER_HEIGHT_PX}px`,
						display: "block",
					}}
				/>
			</div>
		</div>
	);
}

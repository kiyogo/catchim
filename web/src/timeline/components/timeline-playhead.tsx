"use client";

import { useRef, useEffect, useCallback } from "react";
import { useContainerSize } from "@/hooks/use-container-size";
import {
	getCenteredLineLeft,
	TIMELINE_INDICATOR_LINE_WIDTH_PX,
	timelineTimeToSnappedPixels,
} from "@/timeline";
import { useScrollPosition } from "@/timeline/hooks/use-scroll-position";
import { useTimelinePlayhead } from "@/timeline/hooks/use-timeline-playhead";
import {
	addMediaTime,
	maxMediaTime,
	mediaTime,
	subMediaTime,
	TICKS_PER_SECOND,
	ZERO_MEDIA_TIME,
} from "@/wasm";
import { useEditor } from "@/editor/use-editor";
import { TIMELINE_SCROLLBAR_SIZE_PX } from "./layout";
import { TIMELINE_LAYERS } from "./layers";
import { useTranslation } from "@/i18n";

interface TimelinePlayheadProps {
	zoomLevel: number;
	hasHorizontalScrollbar: boolean;
	rulerRef: React.RefObject<HTMLDivElement | null>;
	rulerScrollRef: React.RefObject<HTMLDivElement | null>;
	tracksScrollRef: React.RefObject<HTMLDivElement | null>;
	timelineRef: React.RefObject<HTMLDivElement | null>;
	playheadRef?: React.RefObject<HTMLDivElement | null>;
	isSnappingToPlayhead?: boolean;
}

const NEEDLE_CANVAS_WIDTH = 24;

export function TimelinePlayhead({
	zoomLevel,
	hasHorizontalScrollbar,
	rulerRef,
	rulerScrollRef,
	tracksScrollRef,
	timelineRef,
	playheadRef: externalPlayheadRef,
	isSnappingToPlayhead = false,
}: TimelinePlayheadProps) {
	const { t } = useTranslation();
	const editor = useEditor();
	const duration = editor.timeline.getTotalDuration();
	const internalPlayheadRef = useRef<HTMLDivElement>(null);
	const playheadRef = externalPlayheadRef || internalPlayheadRef;
	const canvasRef = useRef<HTMLCanvasElement | null>(null);

	const { handlePlayheadMouseDown } = useTimelinePlayhead({
		zoomLevel,
		rulerRef,
		rulerScrollRef,
		tracksScrollRef,
		playheadRef,
	});
	const { height: timelineHeight } = useContainerSize({
		containerRef: timelineRef,
	});
	const { height: tracksHeight } = useContainerSize({
		containerRef: tracksScrollRef,
	});
	const { scrollLeft } = useScrollPosition({ scrollRef: tracksScrollRef });

	const timelineContainerHeight = timelineHeight || tracksHeight || 400;
	const totalHeight = Math.max(
		0,
		timelineContainerHeight -
			(hasHorizontalScrollbar ? TIMELINE_SCROLLBAR_SIZE_PX - 5 : 0),
	);

	const currentTime = editor.playback.getCurrentTime();
	const centerPosition = timelineTimeToSnappedPixels({
		time: currentTime,
		zoomLevel,
	});
	const leftPosition =
		getCenteredLineLeft({ centerPixel: centerPosition }) - scrollLeft;

	// Render Playhead on High-DPI Canvas
	const drawPlayhead = useCallback(() => {
		const canvas = canvasRef.current;
		if (!canvas) return;

		const dpr = typeof window !== "undefined" ? window.devicePixelRatio || 1 : 1;
		const width = NEEDLE_CANVAS_WIDTH;
		const height = Math.max(20, totalHeight);

		const pixelW = Math.ceil(width * dpr);
		const pixelH = Math.ceil(height * dpr);

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

		const centerX = width / 2;

		// 1. Playhead vertical line (#0ea5e9 with subtle outer glow)
		ctx.save();
		ctx.shadowColor = "rgba(14, 165, 233, 0.45)";
		ctx.shadowBlur = 4;
		ctx.strokeStyle = "#0ea5e9";
		ctx.lineWidth = 1.5;
		ctx.beginPath();
		ctx.moveTo(centerX, 14);
		ctx.lineTo(centerX, height);
		ctx.stroke();
		ctx.restore();

		// 2. Playhead circular needle handle at the top
		const needleY = 8;
		const needleRadius = 5.5;

		ctx.save();
		ctx.shadowColor = "rgba(14, 165, 233, 0.6)";
		ctx.shadowBlur = 6;

		// Needle fill
		ctx.fillStyle = "#0ea5e9";
		ctx.beginPath();
		ctx.arc(centerX, needleY, needleRadius, 0, Math.PI * 2);
		ctx.fill();

		// Needle ring
		ctx.strokeStyle = isSnappingToPlayhead ? "#ffffff" : "#38bdf8";
		ctx.lineWidth = 2.0;
		ctx.stroke();
		ctx.restore();
	}, [totalHeight, isSnappingToPlayhead]);

	useEffect(() => {
		drawPlayhead();
	}, [drawPlayhead]);

	const handlePlayheadKeyDown = (
		event: React.KeyboardEvent<HTMLDivElement>,
	) => {
		if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;

		event.preventDefault();
		const fps = editor.project.getActive().settings.fps;
		const ticksPerFrame = mediaTime({
			ticks: Math.round((TICKS_PER_SECOND * fps.denominator) / fps.numerator),
		});
		const direction = event.key === "ArrowRight" ? 1 : -1;
		const now = editor.playback.getCurrentTime();
		const nextTime =
			direction > 0
				? addMediaTime({ a: now, b: ticksPerFrame })
				: subMediaTime({ a: now, b: ticksPerFrame });

		editor.playback.seek({
			time: maxMediaTime({
				a: ZERO_MEDIA_TIME,
				b: duration < nextTime ? duration : nextTime,
			}),
		});
	};

	return (
		<div
			ref={playheadRef}
			data-timeline-playhead="true"
			role="slider"
			aria-label={t("timeline.timelinePlayhead")}
			aria-valuemin={0}
			aria-valuemax={duration}
			aria-valuenow={currentTime}
			tabIndex={0}
			className="pointer-events-none absolute"
			style={{
				left: 0,
				transform: `translate3d(${leftPosition - NEEDLE_CANVAS_WIDTH / 2 + TIMELINE_INDICATOR_LINE_WIDTH_PX / 2}px, 0, 0)`,
				willChange: "transform",
				top: 0,
				height: `${totalHeight}px`,
				width: `${NEEDLE_CANVAS_WIDTH}px`,
				zIndex: TIMELINE_LAYERS.playhead,
			}}
			onKeyDown={handlePlayheadKeyDown}
		>
			<canvas
				ref={canvasRef}
				data-engine="canvas-playhead"
				className="pointer-events-none block size-full"
			/>
			{/* Interactive transparent grip surface over the circular handle */}
			<button
				type="button"
				aria-label={t("timeline.dragPlayhead")}
				className="pointer-events-auto absolute top-0 left-1/2 size-5 -translate-x-1/2 cursor-col-resize appearance-none border-0 bg-transparent p-0"
				onMouseDown={handlePlayheadMouseDown}
			/>
		</div>
	);
}

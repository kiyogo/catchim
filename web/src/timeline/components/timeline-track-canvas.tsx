"use client";

import { useEffect, useRef, useCallback, useMemo } from "react";
import type { TimelineTrack, TimelineElement as TimelineElementType, ElementDragView } from "@/timeline";
import { getTrackHeight } from "./track-layout";
import { TIMELINE_TRACK_THEME, TIMELINE_AUDIO_WAVEFORM_COLOR } from "./theme";
import {
	timelineTimeToPixels,
	timelineTimeToSnappedPixels,
	findTrackInSceneTracks,
} from "@/timeline";
import { useEditor } from "@/editor/use-editor";
import { useElementSelection } from "@/timeline/hooks/element/use-element-selection";
import { getNativeTimelineBindings } from "@/native/opencut-core";
import { TIMELINE_LAYERS } from "./layers";
import { addMediaTime, TICKS_PER_SECOND, type MediaTime } from "@/wasm";
import { waveformCache } from "@/services/waveform-cache/service";
import {
	buildWaveformSourceKey,
	type SourceWaveformSummary,
} from "@/media/waveform-summary";

const THUMBNAIL_ASPECT_RATIO = 16 / 9;
const TRIM_HANDLE_WIDTH = 10;
const CORNER_RADIUS = 4;

// Global persistent image and waveform cache
const thumbnailImageCache = new Map<string, HTMLImageElement>();
const waveformSummaryCache = new Map<string, SourceWaveformSummary>();

function getOrLoadThumbnail(url: string, onLoaded: () => void): HTMLImageElement | null {
	if (!url) return null;
	let img = thumbnailImageCache.get(url);
	if (img) {
		if (img.complete && img.naturalWidth > 0) return img;
		return null;
	}
	img = new Image();
	img.crossOrigin = "anonymous";
	img.src = url;
	img.onload = onLoaded;
	thumbnailImageCache.set(url, img);
	return null;
}

interface TimelineTrackCanvasProps {
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

export function TimelineTrackCanvas({
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
}: TimelineTrackCanvasProps) {
	const canvasRef = useRef<HTMLCanvasElement | null>(null);
	const containerRef = useRef<HTMLDivElement | null>(null);
	const rafIdRef = useRef<number | null>(null);
	const viewportOffsetRef = useRef(0);
	const viewportTopRef = useRef(0);
	const drawRef = useRef<() => void>(() => {});

	const { isElementSelected } = useElementSelection();
	const mediaAssets = useEditor((e) => e.media.getAssets());
	const previewTracks = useEditor((e) => e.timeline.getPreviewTracks());
	const totalDuration = useEditor((e) => e.timeline.getTotalDuration());

	const trackHeight = getTrackHeight({ type: track.type });

	// Use active preview tracks if an edit/trim/drag preview is in progress
	const activeTrack = useMemo(() => {
		if (!previewTracks) return track;
		return (
			findTrackInSceneTracks({
				tracks: previewTracks,
				trackId: track.id,
			}) ?? track
		);
	}, [previewTracks, track]);

	// Compute clip geometries with Rust WASM assistance & live drag state
	const draggingView = dragView.kind === "dragging" ? dragView : null;
	const isDragging = draggingView !== null;
	const hasDraggedElements = activeTrack.elements.some((element) =>
		draggingView?.memberTimeOffsets.has(element.id),
	);
	const elementLayouts = useMemo(() => {
		return activeTrack.elements.map((el) => {
			const dragTimeOffset = draggingView
				? draggingView.memberTimeOffsets.get(el.id)
				: undefined;
			const isBeingDragged = dragTimeOffset !== undefined && draggingView !== null;
			const displayedStartTime =
				isBeingDragged && draggingView
					? addMediaTime({ a: draggingView.currentTime, b: dragTimeOffset })
					: el.startTime;
			const dragOffsetY =
				isBeingDragged && draggingView
					? draggingView.currentMouseY - draggingView.startMouseY
					: 0;

			const left = timelineTimeToSnappedPixels({
				time: displayedStartTime,
				zoomLevel,
			});
			const width = Math.max(
				1,
				timelineTimeToPixels({
					time: el.duration,
					zoomLevel,
				}),
			);

			const isSelected = isElementSelected({
				trackId: track.id,
				elementId: el.id,
			});

			return {
				element: el,
				left,
				width,
				dragOffsetY,
				isBeingDragged,
				isSelected,
				height: trackHeight,
			};
		});
	}, [
		activeTrack.elements,
		track.id,
		zoomLevel,
		dragView,
		isDragging,
		isElementSelected,
		trackHeight,
	]);

	const draw = useCallback(() => {
		const canvas = canvasRef.current;
		const container = containerRef.current;
		if (!canvas || !container) return;

		const dpr = typeof window !== "undefined" ? window.devicePixelRatio || 1 : 1;
		// Allocate pixels for the visible viewport, never the whole project.
		// At high zoom a full-length canvas can exceed GPU texture limits.
		const scroll = container.closest<HTMLElement>("[data-timeline-scroll]");
		const offset = scroll?.scrollLeft ?? 0;
		viewportOffsetRef.current = offset;
		const width = Math.max(1, scroll?.clientWidth ?? container.clientWidth);
		// A dragged clip can leave its source row in either direction. Allocate
		// its full vertical bounds instead of clipping it to the source height.
		const viewportTop = Math.min(0, ...elementLayouts.map((item) => item.dragOffsetY));
		const viewportBottom = trackHeight + Math.max(0, ...elementLayouts.map((item) => item.dragOffsetY));
		viewportTopRef.current = viewportTop;
		const height = trackHeight;
		const canvasHeight = viewportBottom - viewportTop;

		const pixelW = Math.ceil(width * dpr);
		const pixelH = Math.ceil(canvasHeight * dpr);

		if (canvas.width !== pixelW || canvas.height !== pixelH) {
			canvas.width = pixelW;
			canvas.height = pixelH;
			canvas.style.width = `${width}px`;
			canvas.style.height = `${canvasHeight}px`;
		}
		canvas.style.transform = `translate(${offset}px, ${viewportTop}px)`;

		const ctx = canvas.getContext("2d");
		if (!ctx) return;

		ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
		ctx.clearRect(0, 0, width, canvasHeight);
		ctx.translate(-offset, -viewportTop);

		// 1. Draw empty dashed placeholder if no elements
		if (activeTrack.elements.length === 0) {
			ctx.save();
			ctx.strokeStyle = "rgba(113, 113, 122, 0.3)";
			ctx.lineWidth = 1.5;
			ctx.setLineDash([6, 6]);
			roundRect(ctx, offset + 4, 2, width - 8, height - 4, CORNER_RADIUS);
			ctx.stroke();
			ctx.restore();
			return;
		}

		// 2. Draw each element
		const tileWidth = trackHeight * THUMBNAIL_ASPECT_RATIO;

		for (const item of elementLayouts) {
			const { element, left, width: elWidth, dragOffsetY, isSelected } = item;
			if (elWidth <= 0 || !Number.isFinite(left)) continue;
			if (left + elWidth < offset || left > offset + width) continue;

			const top = Math.round(1 + dragOffsetY);
			const clipHeight = height - 2;

			ctx.save();
			ctx.beginPath();
			roundRect(ctx, left, top, elWidth, clipHeight, CORNER_RADIUS);
			ctx.clip();

			// Clip Background by Track Type
			if (track.type === "video") {
				ctx.fillStyle = "#18181b"; // dark background
				ctx.fillRect(left, top, elWidth, clipHeight);

				// Draw repeated thumbnail tiles
				const asset = mediaAssets.find((a) => a.id === (element as any).mediaId);
				const imgUrl = asset?.thumbnailUrl || asset?.url;
				if (imgUrl) {
					const img = getOrLoadThumbnail(imgUrl, () => {
						if (!rafIdRef.current) {
							rafIdRef.current = requestAnimationFrame(() => {
								rafIdRef.current = null;
								draw();
							});
						}
					});
					if (img) {
						let curX = left + Math.max(0, Math.floor((offset - left) / tileWidth)) * tileWidth;
						const maxX = Math.min(left + elWidth, offset + width);
						while (curX < maxX) {
							const drawW = Math.min(tileWidth, maxX - curX);
							ctx.drawImage(
								img,
								0,
								0,
								img.naturalWidth,
								img.naturalHeight,
								curX,
								top,
								drawW,
								clipHeight,
							);
							curX += tileWidth;
						}
					}
				}
			} else if (track.type === "audio") {
				ctx.fillStyle = "#8F5DBA";
				ctx.fillRect(left, top, elWidth, clipHeight);

				// Real Audio Waveform Visualization
				ctx.fillStyle = TIMELINE_AUDIO_WAVEFORM_COLOR;
				const barWidth = 2;
				const gap = 1.5;
				const numBars = Math.floor(elWidth / (barWidth + gap));
				const centerY = top + clipHeight / 2;
				const maxAmplitude = (clipHeight - 12) / 2;

				const asset = mediaAssets.find((a) => a.id === (element as any).mediaId);
				const sourceKey = asset
					? buildWaveformSourceKey({ kind: "media", id: asset.id })
					: (element as any).sourceUrl
					? buildWaveformSourceKey({ kind: "library", id: (element as any).sourceUrl })
					: null;

				let summary: SourceWaveformSummary | null = null;
				if (sourceKey) {
					summary = waveformSummaryCache.get(sourceKey) ?? null;
					if (!summary && (asset?.file || (element as any).sourceUrl || asset?.url)) {
						waveformCache
							.getSourceSummary({
								sourceKey,
								sourceFile: asset?.file,
								audioUrl: (element as any).sourceUrl ?? asset?.url,
							})
							.then((s) => {
								waveformSummaryCache.set(sourceKey, s);
								if (!rafIdRef.current) {
									rafIdRef.current = requestAnimationFrame(() => {
										rafIdRef.current = null;
										draw();
									});
								}
							})
							.catch(() => {});
					}
				}

				if (summary && summary.amplitudes.length > 0) {
					const totalAmps = summary.amplitudes.length;
					const trimStartSec = (element.trimStart || 0) / TICKS_PER_SECOND;
					const durationSec = element.duration / TICKS_PER_SECOND;
					const totalDurationSec = Math.max(1, summary.totalSamples / summary.sampleRate);

					for (let b = Math.max(0, Math.floor((offset - left) / (barWidth + gap))); b < Math.min(numBars, Math.ceil((offset + width - left) / (barWidth + gap))); b++) {
						const frac = b / Math.max(1, numBars);
						const currentSec = trimStartSec + frac * durationSec;
						const ampIndex = Math.min(
							totalAmps - 1,
							Math.max(0, Math.floor((currentSec / totalDurationSec) * totalAmps)),
						);
						const amp = summary.amplitudes[ampIndex] || 0;
						const barH = Math.max(2, amp * maxAmplitude);
						const bx = left + b * (barWidth + gap);
						ctx.fillRect(bx, centerY - barH, barWidth, barH * 2);
					}
				} else {
					// A baseline until real peaks arrive; do not invent waveforms.
					ctx.fillRect(Math.max(left, offset), centerY, Math.min(elWidth, width), 1);
				}

				// Draw audio volume baseline
				const volumeDb = (element as any).volumeDb ?? 0;
				const normGain = Math.max(0, Math.min(1, (10 ** (volumeDb / 20) - 0.01) / 1.99));
				const progress = normGain ** 0.5;
				const lineY = Math.round(top + (1 - progress) * clipHeight);
				ctx.save();
				ctx.strokeStyle = "rgba(255, 255, 255, 0.45)";
				ctx.lineWidth = 1;
				ctx.beginPath();
				ctx.moveTo(left, lineY);
				ctx.lineTo(left + elWidth, lineY);
				ctx.stroke();
				ctx.restore();
			} else if (track.type === "text") {
				ctx.fillStyle = "#5DBAA0";
				ctx.fillRect(left, top, elWidth, clipHeight);
			} else if (track.type === "graphic") {
				ctx.fillStyle = "#BA5D7A";
				ctx.fillRect(left, top, elWidth, clipHeight);
			} else if (track.type === "effect") {
				ctx.fillStyle = "#5d93ba";
				ctx.fillRect(left, top, elWidth, clipHeight);
			}

			// Subtle top scrim for title readability
			const scrimHeight = Math.min(22, clipHeight);
			const gradient = ctx.createLinearGradient(left, top, left, top + scrimHeight);
			gradient.addColorStop(0, "rgba(0, 0, 0, 0.60)");
			gradient.addColorStop(1, "rgba(0, 0, 0, 0.0)");
			ctx.fillStyle = gradient;
			ctx.fillRect(left, top, elWidth, scrimHeight);

			// Title Text
			const asset = mediaAssets.find((a) => a.id === (element as any).mediaId);
			const title =
				(element as any).params?.content ||
				asset?.name ||
				element.name ||
				"Clip";
			ctx.font = "500 11px Inter, system-ui, sans-serif";
			ctx.fillStyle = "#ffffff";
			ctx.shadowColor = "rgba(0, 0, 0, 0.85)";
			ctx.shadowBlur = 3;
			ctx.fillText(title, left + 8, top + Math.min(14, clipHeight - 5));
			ctx.shadowBlur = 0;

			ctx.restore();

			// Selection Outline and Trim Handles
			if (isSelected) {
				ctx.save();
				ctx.strokeStyle = "#ffffff";
				ctx.lineWidth = 2.0;
				ctx.beginPath();
				roundRect(ctx, left, top, elWidth, clipHeight, CORNER_RADIUS);
				ctx.stroke();

				// Left Trim Handle Pill
				const pillH = Math.min(22, clipHeight - 10);
				const pillY = top + (clipHeight - pillH) / 2;
				ctx.fillStyle = "#ffffff";
				roundRect(ctx, left + 1, pillY, 4, pillH, 2);
				ctx.fill();

				// Right Trim Handle Pill
				roundRect(ctx, left + elWidth - 5, pillY, 4, pillH, 2);
				ctx.fill();
				ctx.restore();
			} else {
				// Normal subtle border
				ctx.save();
				ctx.strokeStyle = "rgba(255, 255, 255, 0.16)";
				ctx.lineWidth = 1.0;
				ctx.beginPath();
				roundRect(ctx, left, top, elWidth, clipHeight, CORNER_RADIUS);
				ctx.stroke();
				ctx.restore();
			}

			// Drop target highlight
			if (element.id === targetElementId) {
				ctx.save();
				ctx.strokeStyle = "#38bdf8";
				ctx.lineWidth = 2.5;
				roundRect(ctx, left, top, elWidth, clipHeight, CORNER_RADIUS);
				ctx.stroke();
				ctx.restore();
			}
		}
	}, [
		elementLayouts,
		track.type,
		activeTrack.elements.length,
		trackHeight,
		mediaAssets,
		targetElementId,
		totalDuration,
		zoomLevel,
		dynamicTimelineWidth,
	]);

	drawRef.current = draw;
	useEffect(() => {
		const schedule = () => {
			if (rafIdRef.current !== null) return;
			rafIdRef.current = requestAnimationFrame(() => {
				rafIdRef.current = null;
				drawRef.current();
			});
		};
		const container = containerRef.current;
		const scroll = container?.closest<HTMLElement>("[data-timeline-scroll]");
		scroll?.addEventListener("scroll", schedule, { passive: true });
		const observer = new ResizeObserver(schedule);
		if (scroll) observer.observe(scroll);
		schedule();
		return () => {
			scroll?.removeEventListener("scroll", schedule);
			observer.disconnect();
			if (rafIdRef.current !== null) cancelAnimationFrame(rafIdRef.current);
			rafIdRef.current = null;
		};
	}, []);
	useEffect(() => {
		if (rafIdRef.current === null) rafIdRef.current = requestAnimationFrame(() => {
			rafIdRef.current = null;
			drawRef.current();
		});
	}, [draw]);

	// Rust WASM Accelerated Hit Testing Helper
	const hitTestAt = useCallback(
		(px: number, py: number) => {
			const native = getNativeTimelineBindings();
			for (let i = elementLayouts.length - 1; i >= 0; i--) {
				const item = elementLayouts[i];
				const { element, left, width, dragOffsetY } = item;
				const top = dragOffsetY;
				if (
					px >= left &&
					px <= left + width &&
					py >= top &&
					py <= top + trackHeight
				) {
					let zone = 1; // 1 = ClipBody, 2 = LeftTrimHandle, 3 = RightTrimHandle
					if (native?.timelineHitTestClip) {
						zone = native.timelineHitTestClip(
							px,
							py - top,
							left,
							0,
							width,
							trackHeight,
							TRIM_HANDLE_WIDTH,
						);
					} else {
						if (px <= left + TRIM_HANDLE_WIDTH) zone = 2;
						else if (px >= left + width - TRIM_HANDLE_WIDTH) zone = 3;
					}
					return { item, element, zone, left, width, top };
				}
			}
			return null;
		},
		[elementLayouts, trackHeight],
	);

	// Interactive Pointer Move (cursor adaptation)
	const handlePointerMove = (e: React.MouseEvent<HTMLCanvasElement>) => {
		const canvas = canvasRef.current;
		if (!canvas) return;
		const px = e.nativeEvent.offsetX + viewportOffsetRef.current;
		const py = e.nativeEvent.offsetY + viewportTopRef.current;

		const hit = hitTestAt(px, py);
		const newCursor = hit
			? hit.zone === 2 || hit.zone === 3
				? "col-resize"
				: "pointer"
			: "default";
		if (canvas.style.cursor !== newCursor) {
			canvas.style.cursor = newCursor;
		}
	};

	// Interactive Mouse Down
	const handleMouseDown = (e: React.MouseEvent<HTMLCanvasElement>) => {
		const canvas = canvasRef.current;
		if (!canvas) return;
		const px = e.nativeEvent.offsetX + viewportOffsetRef.current;
		const py = e.nativeEvent.offsetY + viewportTopRef.current;

		const hit = hitTestAt(px, py);

		if (e.button === 0) {
			// Left Click
			if (hit) {
				e.stopPropagation();

				if (hit.zone === 2) {
					onResizeStart({ event: e, element: hit.element, track, side: "left" });
					return;
				}
				if (hit.zone === 3) {
					onResizeStart({ event: e, element: hit.element, track, side: "right" });
					return;
				}

				const rect = containerRef.current!.getBoundingClientRect();
				const proxyEvent = {
					...e,
					clientX: e.clientX,
					clientY: e.clientY,
					currentTarget: {
						getBoundingClientRect: () => ({
							left: rect.left + hit.left,
							top: rect.top + hit.top,
							width: hit.width,
							height: trackHeight,
							right: rect.left + hit.left + hit.width,
							bottom: rect.top + hit.top + trackHeight,
							x: rect.left + hit.left,
							y: rect.top + hit.top,
							toJSON: () => {},
						}),
					},
					stopPropagation: () => e.stopPropagation(),
					preventDefault: () => e.preventDefault(),
				} as unknown as React.MouseEvent;

				onElementMouseDown({ event: proxyEvent, element: hit.element, track });
				return;
			}

			// Clicked on empty track space
			onTrackMouseDown?.(e);
		} else if (e.button === 2) {
			// Right Click: Select clip if hit
			if (hit) {
				onElementMouseDown({ event: e, element: hit.element, track });
			}
		}
	};

	// Interactive Click
	const handleClick = (e: React.MouseEvent<HTMLCanvasElement>) => {
		if (shouldIgnoreClick?.()) return;
		const canvas = canvasRef.current;
		if (!canvas) return;
		const px = e.nativeEvent.offsetX + viewportOffsetRef.current;
		const py = e.nativeEvent.offsetY + viewportTopRef.current;

		const hit = hitTestAt(px, py);
		if (hit) {
			e.stopPropagation();
			onElementClick({ event: e, element: hit.element, track });
			return;
		}

		onTrackMouseUp?.(e);
	};

	return (
		<div
			ref={containerRef}
			className={`relative size-full ${hasDraggedElements ? "overflow-visible" : "overflow-hidden"}`}
			style={{ zIndex: TIMELINE_LAYERS.trackContent }}
		>
			<canvas
				ref={canvasRef}
				className="absolute top-0 left-0 block"
				data-engine="canvas-2d"
				data-track-type={track.type}
				data-track-id={track.id}
				onMouseMove={handlePointerMove}
				onMouseDown={handleMouseDown}
				onClick={handleClick}
			/>
		</div>
	);
}

function roundRect(
	ctx: CanvasRenderingContext2D,
	x: number,
	y: number,
	w: number,
	h: number,
	r: number,
) {
	const radius = Math.min(r, w / 2, h / 2);
	ctx.beginPath();
	ctx.moveTo(x + radius, y);
	ctx.arcTo(x + w, y, x + w, y + h, radius);
	ctx.arcTo(x + w, y + h, x, y + h, radius);
	ctx.arcTo(x, y + h, x, y, radius);
	ctx.arcTo(x, y, x + w, y, radius);
	ctx.closePath();
}

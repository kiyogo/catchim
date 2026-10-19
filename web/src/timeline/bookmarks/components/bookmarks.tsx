"use client";

import { useState, useEffect, useRef } from "react";
import type { EditorCore } from "@/core";
import { useEditor } from "@/editor/use-editor";
import type { BookmarkDragState } from "../hooks/use-bookmark-drag";
import { DEFAULT_TIMELINE_BOOKMARK_COLOR } from "@/timeline/components/theme";
import { TIMELINE_BOOKMARK_ROW_HEIGHT_PX } from "@/timeline/components/layout";
import { DEFAULT_FPS } from "@/fps/defaults";
import {
	ArrowTurnBackwardIcon,
	Delete02Icon,
} from "@hugeicons/core-free-icons";
import { HugeiconsIcon } from "@hugeicons/react";
import type { Bookmark } from "@/timeline";
import {
	Popover,
	PopoverAnchor,
	PopoverContent,
} from "@/components/ui/popover";
import { Input } from "@/components/ui/input";
import { ColorPicker } from "@/components/ui/color-picker";
import { Button } from "@/components/ui/button";
import { Label } from "@/components/ui/label";
import { uppercase } from "@/utils/string";
import { clamp, formatNumberForDisplay } from "@/utils/math";
import { timelineTimeToPixels, timelineTimeToSnappedPixels } from "@/timeline";
import {
	type MediaTime,
	mediaTimeFromSeconds,
	mediaTimeToSeconds,
	snapSeekMediaTime,
	subMediaTime,
	ZERO_MEDIA_TIME,
} from "@/wasm";
import { useTranslation } from "@/i18n";

const MIN_BOOKMARK_WIDTH_PX = 2;
const BOOKMARK_MARKER_WIDTH_PX = 12;
const BOOKMARK_MARKER_HEIGHT_PX = 15;
const BOOKMARK_HALF_WIDTH_PX = BOOKMARK_MARKER_WIDTH_PX / 2;
const BOOKMARK_MARKER_CLIP_PATH =
	"polygon(50% 100%, 12% 72%, 12% 10%, 88% 10%, 88% 72%)";

function seekToBookmarkTime({
	editor,
	time,
}: {
	editor: EditorCore;
	time: MediaTime;
}) {
	const activeProject = editor.project.getActive();
	const duration = editor.timeline.getTotalDuration();
	const rate = activeProject?.settings.fps ?? DEFAULT_FPS;
	const snappedTime = snapSeekMediaTime({ time, duration, fps: rate });
	editor.playback.seek({ time: snappedTime });
}

interface TimelineBookmarksRowProps {
	zoomLevel: number;
	dynamicTimelineWidth: number;
	dragState: BookmarkDragState;
	onBookmarkMouseDown: (params: {
		event: React.MouseEvent;
		bookmark: Bookmark;
	}) => void;
	handleWheel: (event: React.WheelEvent) => void;
	handleTimelineContentClick: (event: React.MouseEvent) => void;
	handleRulerTrackingMouseDown: (event: React.MouseEvent) => void;
	handleRulerMouseDown: (event: React.MouseEvent) => void;
}

export function TimelineBookmarksRow({
	zoomLevel,
	dynamicTimelineWidth,
	dragState,
	onBookmarkMouseDown,
	handleWheel,
	handleTimelineContentClick,
	handleRulerTrackingMouseDown,
	handleRulerMouseDown,
}: TimelineBookmarksRowProps) {
	const { t } = useTranslation();
	const bookmarks = useEditor((e) => e.scenes.getActiveScene().bookmarks);
	const canvasRef = useRef<HTMLCanvasElement | null>(null);

	useEffect(() => {
		const canvas = canvasRef.current;
		if (!canvas) return;

		const dpr = typeof window !== "undefined" ? window.devicePixelRatio || 1 : 1;
		const width = dynamicTimelineWidth;
		const height = TIMELINE_BOOKMARK_ROW_HEIGHT_PX;

		if (canvas.width !== Math.round(width * dpr) || canvas.height !== Math.round(height * dpr)) {
			canvas.width = Math.round(width * dpr);
			canvas.height = Math.round(height * dpr);
		}

		const ctx = canvas.getContext("2d");
		if (!ctx) return;

		ctx.save();
		ctx.scale(dpr, dpr);
		ctx.clearRect(0, 0, width, height);

		for (const bookmark of bookmarks) {
			const isDragging =
				dragState.isDragging &&
				dragState.bookmarkTime !== null &&
				dragState.bookmarkTime === bookmark.time;
			const displayTime = isDragging ? dragState.currentTime : bookmark.time;
			const bookmarkDuration = bookmark.duration ?? ZERO_MEDIA_TIME;
			const durationWidth =
				bookmarkDuration > 0
					? timelineTimeToPixels({ time: bookmarkDuration, zoomLevel })
					: 0;
			const left = timelineTimeToSnappedPixels({ time: displayTime, zoomLevel });
			const iconColor = bookmark.color ?? DEFAULT_TIMELINE_BOOKMARK_COLOR;

			if (durationWidth > MIN_BOOKMARK_WIDTH_PX) {
				ctx.save();
				ctx.fillStyle = iconColor;
				ctx.globalAlpha = 0.3;
				ctx.fillRect(left, 1.5, durationWidth, BOOKMARK_MARKER_HEIGHT_PX - 2.5);
				ctx.restore();
			}

			const bx = left - BOOKMARK_HALF_WIDTH_PX;
			const bw = BOOKMARK_MARKER_WIDTH_PX;
			const bh = BOOKMARK_MARKER_HEIGHT_PX;

			ctx.save();
			ctx.fillStyle = iconColor;
			ctx.beginPath();
			ctx.moveTo(bx + bw * 0.5, bh);
			ctx.lineTo(bx + bw * 0.12, bh * 0.72);
			ctx.lineTo(bx + bw * 0.12, bh * 0.1);
			ctx.lineTo(bx + bw * 0.88, bh * 0.1);
			ctx.lineTo(bx + bw * 0.88, bh * 0.72);
			ctx.closePath();
			ctx.fill();

			ctx.fillStyle = "#ffffff";
			ctx.beginPath();
			ctx.arc(bx + bw * 0.5, bh * 0.42, 1.5, 0, Math.PI * 2);
			ctx.fill();
			ctx.restore();
		}

		ctx.restore();
	}, [bookmarks, dragState, dynamicTimelineWidth, zoomLevel]);

	return (
		<div
			className="relative flex-1 overflow-hidden"
			style={{ height: TIMELINE_BOOKMARK_ROW_HEIGHT_PX }}
		>
			<canvas
				ref={canvasRef}
				data-engine="canvas-bookmarks"
				style={{
					position: "absolute",
					top: 0,
					left: 0,
					width: `${dynamicTimelineWidth}px`,
					height: `${TIMELINE_BOOKMARK_ROW_HEIGHT_PX}px`,
					pointerEvents: "none",
				}}
			/>
			<button
				className="relative w-full cursor-default select-none border-0 bg-transparent p-0"
				style={{
					height: TIMELINE_BOOKMARK_ROW_HEIGHT_PX,
					width: `${dynamicTimelineWidth}px`,
				}}
				aria-label={t("timeline.timelineRuler")}
				type="button"
				onWheel={handleWheel}
				onClick={(event) => {
					if (
						!(event.target instanceof Node) ||
						!event.currentTarget.contains(event.target)
					)
						return;
					handleTimelineContentClick(event);
				}}
				onMouseDown={(event) => {
					if (
						!(event.target instanceof Node) ||
						!event.currentTarget.contains(event.target)
					)
						return;
					handleRulerMouseDown(event);
					handleRulerTrackingMouseDown(event);
				}}
			>
				{bookmarks.map((bookmark) => (
					<TimelineBookmark
						key={`bookmark-${bookmark.time}`}
						bookmark={bookmark}
						zoomLevel={zoomLevel}
						dragState={dragState}
						onBookmarkMouseDown={onBookmarkMouseDown}
					/>
				))}
			</button>
		</div>
	);
}

function TimelineBookmark({
	bookmark,
	zoomLevel,
	dragState,
	onBookmarkMouseDown,
}: {
	bookmark: Bookmark;
	zoomLevel: number;
	dragState: BookmarkDragState;
	onBookmarkMouseDown: (params: {
		event: React.MouseEvent;
		bookmark: Bookmark;
	}) => void;
}) {
	const editor = useEditor();
	const duration = editor.timeline.getTotalDuration();
	const [isPopoverOpen, setIsPopoverOpen] = useState(false);

	const isDragging =
		dragState.isDragging &&
		dragState.bookmarkTime !== null &&
		dragState.bookmarkTime === bookmark.time;

	const displayTime = isDragging ? dragState.currentTime : bookmark.time;
	const time = bookmark.time;
	const bookmarkDuration = bookmark.duration ?? ZERO_MEDIA_TIME;
	const durationWidth =
		bookmarkDuration > 0
			? timelineTimeToPixels({ time: bookmarkDuration, zoomLevel })
			: 0;
	const hasDurationRange = durationWidth > MIN_BOOKMARK_WIDTH_PX;
	const bookmarkWidth = BOOKMARK_MARKER_WIDTH_PX + Math.max(durationWidth, 0);
	const left = timelineTimeToSnappedPixels({ time: displayTime, zoomLevel });
	const bookmarkLeft = left - BOOKMARK_HALF_WIDTH_PX;
	const rightHalfLeft = BOOKMARK_HALF_WIDTH_PX + Math.max(durationWidth, 0);
	const iconColor = bookmark.color ?? DEFAULT_TIMELINE_BOOKMARK_COLOR;

	const handleSeek = () => seekToBookmarkTime({ editor, time });

	const handleClick = (event: React.MouseEvent) => {
		event.preventDefault();
		event.stopPropagation();
		if (event.detail === 2) {
			setIsPopoverOpen(true);
		} else {
			handleSeek();
		}
	};

	const handleKeyDown = (event: React.KeyboardEvent) => {
		if (event.key !== "Enter" && event.key !== " ") return;
		event.preventDefault();
		handleSeek();
	};

	const handleMouseDown = (event: React.MouseEvent) => {
		onBookmarkMouseDown({ event, bookmark });
		event.preventDefault();
		event.stopPropagation();
	};

	return (
		<Popover open={isPopoverOpen} onOpenChange={setIsPopoverOpen}>
			<PopoverAnchor asChild>
				<button
					className="absolute top-0 h-full min-w-0.5 border-0 bg-transparent p-0"
					style={{
						left: `${bookmarkLeft}px`,
						width: `${bookmarkWidth}px`,
					}}
					aria-label={`Bookmark at ${formatNumberForDisplay({ value: mediaTimeToSeconds({ time }), fractionDigits: 1 })}s`}
					type="button"
					onMouseDown={handleMouseDown}
					onClick={handleClick}
					onKeyDown={handleKeyDown}
				>
					{hasDurationRange ? (
						<div
							className="absolute opacity-30"
							style={{
								top: 1.5,
								height: BOOKMARK_MARKER_HEIGHT_PX - 2.5,
								left: BOOKMARK_HALF_WIDTH_PX,
								width: durationWidth,
								backgroundColor: iconColor,
							}}
						/>
					) : null}
					<div
						className="absolute top-0 overflow-hidden"
						style={{
							left: 0,
							width: BOOKMARK_HALF_WIDTH_PX,
							height: BOOKMARK_MARKER_HEIGHT_PX,
						}}
					>
						<div
							className="absolute inset-0"
							style={{
								width: BOOKMARK_MARKER_WIDTH_PX,
								height: BOOKMARK_MARKER_HEIGHT_PX,
								clipPath: BOOKMARK_MARKER_CLIP_PATH,
								backgroundColor: "hsl(var(--background))",
							}}
						/>
						<div
							className="absolute"
							style={{
								top: 1,
								left: 1,
								width: BOOKMARK_MARKER_WIDTH_PX - 2,
								height: BOOKMARK_MARKER_HEIGHT_PX - 2,
								clipPath: BOOKMARK_MARKER_CLIP_PATH,
								backgroundColor: iconColor,
							}}
						/>
					</div>
					<div
						className="absolute top-0 overflow-hidden"
						style={{
							left: rightHalfLeft,
							width: BOOKMARK_HALF_WIDTH_PX,
							height: BOOKMARK_MARKER_HEIGHT_PX,
						}}
					>
						<div
							className="absolute top-0"
							style={{
								left: -BOOKMARK_HALF_WIDTH_PX,
								width: BOOKMARK_MARKER_WIDTH_PX,
								height: BOOKMARK_MARKER_HEIGHT_PX,
								clipPath: BOOKMARK_MARKER_CLIP_PATH,
								backgroundColor: "hsl(var(--background))",
							}}
						/>
						<div
							className="absolute"
							style={{
								top: 1,
								left: 1 - BOOKMARK_HALF_WIDTH_PX,
								width: BOOKMARK_MARKER_WIDTH_PX - 2,
								height: BOOKMARK_MARKER_HEIGHT_PX - 2,
								clipPath: BOOKMARK_MARKER_CLIP_PATH,
								backgroundColor: iconColor,
							}}
						/>
					</div>
				</button>
			</PopoverAnchor>
			<PopoverContent
				className="w-64 flex flex-col gap-3 p-3"
				align="start"
				side="bottom"
				sideOffset={8}
				onOpenAutoFocus={(event) => event.preventDefault()}
			>
				<BookmarkPopoverContent
					key={`${bookmark.time}:${bookmark.color ?? "default"}`}
					bookmark={bookmark}
					time={time}
					timelineDuration={duration}
					onPopoverClose={() => setIsPopoverOpen(false)}
				/>
			</PopoverContent>
		</Popover>
	);
}

function BookmarkPopoverContent({
	bookmark,
	time,
	timelineDuration,
	onPopoverClose,
}: {
	bookmark: Bookmark;
	time: MediaTime;
	timelineDuration: MediaTime;
	onPopoverClose: () => void;
}) {
	const { t } = useTranslation();
	const editor = useEditor();
	const [draftColorHex, setDraftColorHex] = useState(
		(bookmark.color ?? DEFAULT_TIMELINE_BOOKMARK_COLOR)
			.replace("#", "")
			.toUpperCase(),
	);

	const handleRemove = () => {
		editor.scenes.removeBookmark({ time });
		onPopoverClose();
	};

	const handleUpdate = ({
		note,
		color,
		duration,
	}: Partial<Omit<Bookmark, "time">>) => {
		const updates: Partial<Omit<Bookmark, "time">> = {};
		if (note !== undefined && note !== bookmark.note) updates.note = note;
		if (
			color !== undefined &&
			color.toUpperCase() !== (bookmark.color ?? "").toUpperCase()
		) {
			updates.color = color;
		}
		if (duration !== undefined && duration !== bookmark.duration) {
			updates.duration = duration;
		}
		if (Object.keys(updates).length === 0) return;
		editor.scenes.updateBookmark({ time, updates });
	};
	const maxDuration = mediaTimeToSeconds({
		time:
			timelineDuration > time
				? subMediaTime({ a: timelineDuration, b: time })
				: ZERO_MEDIA_TIME,
	});
	const durationSeconds = mediaTimeToSeconds({
		time: bookmark.duration ?? ZERO_MEDIA_TIME,
	});

	return (
		<>
			<div className="flex flex-col gap-2">
				<Label className="text-xs">{t("timeline.note")}</Label>
				<Input
					placeholder={t("timeline.addNote")}
					value={bookmark.note ?? ""}
					onChange={(event) => handleUpdate({ note: event.target.value })}
					className="h-8 text-sm"
				/>
			</div>
			<div className="flex flex-col gap-2">
				<Label className="text-xs">{t("properties.color")}</Label>
				<div className="relative">
					<ColorPicker
						value={uppercase({ string: draftColorHex })}
						onChange={(color) => setDraftColorHex(uppercase({ string: color }))}
						onChangeEnd={(color) =>
							handleUpdate({ color: `#${uppercase({ string: color })}` })
						}
						className="bg-background border"
					/>
					{bookmark.color &&
						bookmark.color.replace(/^#/, "").toUpperCase() !==
							DEFAULT_TIMELINE_BOOKMARK_COLOR.replace(
								/^#/,
								"",
							).toUpperCase() && (
							<Button
								type="button"
								variant="text"
								size="text"
								aria-label={t("timeline.resetToDefaultColor")}
								className="absolute top-1/2 right-1 -translate-y-1/2 mr-1"
								onClick={() =>
									editor.scenes.updateBookmark({
										time,
										updates: { color: undefined },
									})
								}
							>
								<HugeiconsIcon
									icon={ArrowTurnBackwardIcon}
									className="!size-3.5"
								/>
							</Button>
						)}
				</div>
			</div>
			<div className="flex flex-col gap-2">
				<Label className="text-xs">{t("project.duration")}</Label>
				<div className="flex items-center gap-1.5">
					<Input
						type="number"
						min={0}
						step={0.1}
						value={durationSeconds}
						onChange={(event) => {
							const parsed = parseFloat(event.target.value);
							const value = Number.isNaN(parsed)
								? 0
								: clamp({
										value: parsed,
										min: 0,
										max: maxDuration,
									});
							handleUpdate({
								duration: mediaTimeFromSeconds({ seconds: value }),
							});
						}}
						className="h-8 text-sm"
						containerClassName="w-full"
					/>
				</div>
			</div>
			<Button
				type="button"
				variant="outline"
				size="sm"
				className="text-destructive hover:bg-destructive/10"
				onClick={handleRemove}
				onKeyDown={(event) => {
					if (event.key === "Enter" || event.key === " ") {
						handleRemove();
					}
				}}
				aria-label={t("timeline.deleteBookmark")}
			>
				<HugeiconsIcon icon={Delete02Icon} className="!size-3.5" />
				{t("timeline.deleteBookmark")}
			</Button>
		</>
	);
}

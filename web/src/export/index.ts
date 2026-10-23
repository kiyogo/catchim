import type { FrameRate } from "opencut-wasm";
import type { TCanvasSize } from "@/project/types";
import type { MediaTime } from "@/wasm";
import { EXPORT_MIME_TYPES } from "./mime-types";

export const EXPORT_QUALITY_VALUES = [
	"low",
	"medium",
	"high",
	"very_high",
] as const;

export const EXPORT_FORMAT_VALUES = ["mp4", "webm"] as const;
export const EXPORT_RESOLUTION_VALUES = [
	"source",
	"480p",
	"720p",
	"1080p",
	"1440p",
	"2160p",
	"4320p",
] as const;

const EXPORT_RESOLUTION_HEIGHTS: Record<
	Exclude<ExportResolution, "source">,
	number
> = {
	"480p": 480,
	"720p": 720,
	"1080p": 1080,
	"1440p": 1440,
	"2160p": 2160,
	"4320p": 4320,
};

export type ExportFormat = (typeof EXPORT_FORMAT_VALUES)[number];
export type ExportQuality = (typeof EXPORT_QUALITY_VALUES)[number];
export type ExportResolution = (typeof EXPORT_RESOLUTION_VALUES)[number];

export interface ExportOptions {
	format: ExportFormat;
	quality: ExportQuality;
	resolution?: ExportResolution;
	fps?: FrameRate;
	includeAudio?: boolean;
	range?: { start: MediaTime; end: MediaTime };
	audioOnly?: boolean;
}

export interface ExportResult {
	success: boolean;
	buffer?: ArrayBuffer;
	error?: string;
	cancelled?: boolean;
}

export interface ExportState {
	isExporting: boolean;
	progress: number;
	result: ExportResult | null;
}

export function getExportMimeType({
	format,
}: {
	format: ExportFormat;
}): string {
	return EXPORT_MIME_TYPES[format];
}

export function getExportFileExtension({
	format,
}: {
	format: ExportFormat;
}): string {
	return `.${format}`;
}

function roundToEven(value: number) {
	return Math.max(2, Math.round(value / 2) * 2);
}

export function resolveExportCanvasSize({
	sourceSize,
	resolution,
}: {
	sourceSize: TCanvasSize;
	resolution: ExportResolution | undefined;
}): TCanvasSize {
	if (!resolution || resolution === "source") return sourceSize;

	const targetHeight = EXPORT_RESOLUTION_HEIGHTS[resolution];
	const aspectRatio = sourceSize.width / sourceSize.height;

	return {
		width: roundToEven(targetHeight * aspectRatio),
		height: targetHeight,
	};
}

export function downloadBuffer({
	buffer,
	filename,
	mimeType,
}: {
	buffer: ArrayBuffer;
	filename: string;
	mimeType: string;
}): void {
	const blob = new Blob([buffer], { type: mimeType });
	const url = URL.createObjectURL(blob);
	const downloadLink = document.createElement("a");
	downloadLink.href = url;
	downloadLink.download = filename;
	document.body.appendChild(downloadLink);
	downloadLink.click();
	document.body.removeChild(downloadLink);
	URL.revokeObjectURL(url);
}

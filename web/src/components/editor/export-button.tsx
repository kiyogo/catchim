"use client";

import { useState } from "react";
import { TransitionTopIcon } from "@hugeicons/core-free-icons";
import { HugeiconsIcon } from "@hugeicons/react";
import {
	Dialog,
	DialogContent,
	DialogTrigger,
	DialogTitle,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Progress } from "@/components/ui/progress";
import { cn } from "@/utils/ui";
import {
	getExportMimeType,
	getExportFileExtension,
	downloadBuffer,
	resolveExportCanvasSize,
} from "@/export";
import { Check, Copy, Download, RotateCcw } from "lucide-react";
import {
	EXPORT_FORMAT_VALUES,
	EXPORT_QUALITY_VALUES,
	EXPORT_RESOLUTION_VALUES,
	type ExportFormat,
	type ExportQuality,
	type ExportResolution,
} from "@/export";
import {
	Select,
	SelectContent,
	SelectItem,
	SelectTrigger,
	SelectValue,
} from "@/components/ui/select";
import { useEditor } from "@/editor/use-editor";
import { DEFAULT_EXPORT_OPTIONS } from "@/export/defaults";
import { useTranslation, type TranslationKey } from "@/i18n";

function isExportFormat(value: string): value is ExportFormat {
	return EXPORT_FORMAT_VALUES.some((formatValue) => formatValue === value);
}

function isExportQuality(value: string): value is ExportQuality {
	return EXPORT_QUALITY_VALUES.some((qualityValue) => qualityValue === value);
}

function isExportResolution(value: string): value is ExportResolution {
	return EXPORT_RESOLUTION_VALUES.some(
		(resolutionValue) => resolutionValue === value,
	);
}

const EXPORT_RESOLUTION_LABEL_KEYS: Record<ExportResolution, TranslationKey> = {
	source: "export.resolutionSource",
	"480p": "export.resolution480p",
	"720p": "export.resolution720p",
	"1080p": "export.resolution1080p",
	"1440p": "export.resolution1440p",
	"2160p": "export.resolution2160p",
	"4320p": "export.resolution4320p",
};

const EXPORT_FORMAT_LABEL_KEYS: Record<ExportFormat, TranslationKey> = {
	mp4: "export.formatMp4",
	webm: "export.formatWebm",
};

const EXPORT_QUALITY_LABEL_KEYS: Record<ExportQuality, TranslationKey> = {
	low: "export.qualityLow",
	medium: "export.qualityMedium",
	high: "export.qualityHigh",
	very_high: "export.qualityVeryHigh",
};

export function ExportButton() {
	const { t } = useTranslation();
	const [isExportDialogOpen, setIsExportDialogOpen] = useState(false);
	const editor = useEditor();
	const activeProject = useEditor((e) => e.project.getActiveOrNull());
	const hasProject = !!activeProject;

	const handleDialogOpenChange = ({ open }: { open: boolean }) => {
		if (!open) {
			editor.project.cancelExport();
			editor.project.clearExportState();
		}
		setIsExportDialogOpen(open);
	};

	return (
		<Dialog
			open={isExportDialogOpen}
			onOpenChange={(open) => handleDialogOpenChange({ open })}
		>
			<DialogTrigger asChild>
				<button
					type="button"
					className={cn(
						"flex items-center gap-1.5 rounded-md bg-[#38BDF8] px-[0.12rem] py-[0.12rem] text-white",
						hasProject ? "cursor-pointer" : "cursor-not-allowed opacity-50",
					)}
					onClick={hasProject ? () => setIsExportDialogOpen(true) : undefined}
					disabled={!hasProject}
					onKeyDown={(event) => {
						if (hasProject && (event.key === "Enter" || event.key === " ")) {
							event.preventDefault();
							setIsExportDialogOpen(true);
						}
					}}
				>
					<div className="relative flex items-center gap-1.5 rounded-[0.6rem] bg-linear-270 from-[#2567EC] to-[#37B6F7] px-4 py-1 shadow-[0_1px_3px_0px_rgba(0,0,0,0.65)]">
						<HugeiconsIcon icon={TransitionTopIcon} className="z-50 size-3.5" />
						<span className="z-50 text-[0.875rem]">{t("export.button")}</span>
						<div className="absolute top-0 left-0 z-10 flex size-full items-center justify-center rounded-[0.6rem] bg-linear-to-t from-white/0 to-white/50">
							<div className="absolute top-[0.08rem] z-50 h-[calc(100%-2px)] w-[calc(100%-2px)] rounded-[0.6rem] bg-linear-270 from-[#2567EC] to-[#37B6F7]"></div>
						</div>
					</div>
				</button>
			</DialogTrigger>
			{hasProject && <ExportDialog onOpenChange={setIsExportDialogOpen} />}
		</Dialog>
	);
}

function ExportDialog({
	onOpenChange,
}: {
	onOpenChange: (open: boolean) => void;
}) {
	const { t } = useTranslation();
	const editor = useEditor();
	const activeProject = useEditor((e) => e.project.getActive());
	const exportState = useEditor((e) => e.project.getExportState());
	const { isExporting, progress, result: exportResult } = exportState;
	const [format, setFormat] = useState<ExportFormat>(
		DEFAULT_EXPORT_OPTIONS.format,
	);
	const [quality, setQuality] = useState<ExportQuality>(
		DEFAULT_EXPORT_OPTIONS.quality,
	);
	const [resolution, setResolution] = useState<ExportResolution>(
		DEFAULT_EXPORT_OPTIONS.resolution,
	);
	const [shouldIncludeAudio, setShouldIncludeAudio] = useState<boolean>(
		DEFAULT_EXPORT_OPTIONS.includeAudio ?? true,
	);
	const outputCanvasSize = resolveExportCanvasSize({
		sourceSize: activeProject.settings.canvasSize,
		resolution,
	});

	const handleExport = async () => {
		if (!activeProject) return;

		const result = await editor.project.export({
			options: {
				format,
				quality,
				resolution,
				fps: activeProject.settings.fps,
				includeAudio: shouldIncludeAudio,
			},
		});

		if (result.cancelled) {
			editor.project.clearExportState();
			return;
		}

		if (result.success && result.buffer) {
			downloadBuffer({
				buffer: result.buffer,
				filename: `${activeProject.metadata.name}${getExportFileExtension({ format })}`,
				mimeType: getExportMimeType({ format }),
			});

			editor.project.clearExportState();
			onOpenChange(false);
		}
	};

	const handleCancel = () => {
		editor.project.cancelExport();
	};

	return (
		<DialogContent
			className="bg-background flex max-w-[26rem] flex-col overflow-hidden p-0"
			closeClassName="top-3.5 right-3.5"
		>
			{exportResult && !exportResult.success ? (
				<ExportError
					error={exportResult.error || t("export.errorFallback")}
					onRetry={handleExport}
				/>
			) : (
				<>
					<div className="flex items-center justify-between p-3 pr-14 border-b">
						<DialogTitle className="font-medium text-sm">
							{isExporting ? t("export.exportingProject") : t("export.title")}
						</DialogTitle>
					</div>

					<div className="flex flex-col gap-4">
						{!isExporting && (
							<>
								<div className="flex flex-col divide-y">
									<ExportSelectRow
										label={t("export.resolution")}
										value={resolution}
										onValueChange={(value) => {
											if (isExportResolution(value)) {
												setResolution(value);
											}
										}}
										items={EXPORT_RESOLUTION_VALUES.map((value) => ({
											value,
											label: t(EXPORT_RESOLUTION_LABEL_KEYS[value]),
										}))}
									/>
									<ExportSelectRow
										label={t("export.format")}
										value={format}
										onValueChange={(value) => {
											if (isExportFormat(value)) {
												setFormat(value);
											}
										}}
										items={EXPORT_FORMAT_VALUES.map((value) => ({
											value,
											label: t(EXPORT_FORMAT_LABEL_KEYS[value]),
										}))}
									/>
									<ExportSelectRow
										label={t("export.quality")}
										value={quality}
										onValueChange={(value) => {
											if (isExportQuality(value)) {
												setQuality(value);
											}
										}}
										items={EXPORT_QUALITY_VALUES.map((value) => ({
											value,
											label: t(EXPORT_QUALITY_LABEL_KEYS[value]),
										}))}
									/>
									<ExportSelectRow
										label={t("export.audio")}
										value={shouldIncludeAudio ? "include" : "mute"}
										onValueChange={(value) => {
											setShouldIncludeAudio(value === "include");
										}}
										items={[
											{ value: "include", label: t("export.audioInclude") },
											{ value: "mute", label: t("export.audioMute") },
										]}
									/>
									<div className="px-3 py-2 text-xs text-muted-foreground">
										{t("export.outputSize", {
											width: outputCanvasSize.width,
											height: outputCanvasSize.height,
										})}
									</div>
								</div>

								<div className="p-3 pt-0">
									<Button onClick={handleExport} className="w-full gap-2">
										<Download className="size-4" />
										{t("export.button")}
									</Button>
								</div>
							</>
						)}

						{isExporting && (
							<div className="space-y-4 p-3">
								<div className="flex flex-col gap-2">
									<div className="flex items-center justify-between text-center">
										<p className="text-muted-foreground text-sm">
											{Math.round(progress * 100)}%
										</p>
										<p className="text-muted-foreground text-sm">100%</p>
									</div>
									<Progress value={progress * 100} className="w-full" />
								</div>

								<Button
									variant="outline"
									className="w-full rounded-md"
									onClick={handleCancel}
								>
									{t("export.cancel")}
								</Button>
							</div>
						)}
					</div>
				</>
			)}
		</DialogContent>
	);
}

function ExportSelectRow({
	label,
	value,
	onValueChange,
	items,
}: {
	label: string;
	value: string;
	onValueChange: (value: string) => void;
	items: Array<{ value: string; label: string }>;
}) {
	return (
		<div className="flex items-center justify-between gap-3 px-3 py-2.5">
			<span className="text-sm font-medium text-muted-foreground">{label}</span>
			<Select value={value} onValueChange={onValueChange}>
				<SelectTrigger className="h-8 min-w-44 justify-between bg-accent">
					<SelectValue />
				</SelectTrigger>
				<SelectContent className="min-w-56">
					{items.map((item) => (
						<SelectItem key={item.value} value={item.value}>
							{item.label}
						</SelectItem>
					))}
				</SelectContent>
			</Select>
		</div>
	);
}

function ExportError({
	error,
	onRetry,
}: {
	error: string;
	onRetry: () => void;
}) {
	const { t } = useTranslation();
	const [copied, setCopied] = useState(false);

	const handleCopy = async () => {
		await navigator.clipboard.writeText(error);
		setCopied(true);
		setTimeout(() => setCopied(false), 1000);
	};

	return (
		<div className="space-y-4 p-3">
			<div className="flex flex-col gap-1.5">
				<p className="text-destructive text-sm font-medium">
					{t("export.exportFailed")}
				</p>
				<p className="text-muted-foreground text-xs">{error}</p>
			</div>

			<div className="flex gap-2">
				<Button
					variant="outline"
					size="sm"
					className="h-8 flex-1 text-xs"
					onClick={handleCopy}
				>
					{copied ? <Check className="text-constructive" /> : <Copy />}
					{t("export.copyError")}
				</Button>
				<Button
					variant="outline"
					size="sm"
					className="h-8 flex-1 text-xs"
					onClick={onRetry}
				>
					<RotateCcw />
					{t("common.retry")}
				</Button>
			</div>
		</div>
	);
}

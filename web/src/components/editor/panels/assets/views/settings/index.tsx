"use client";

import { useState } from "react";
import { PanelView } from "@/components/editor/panels/assets/views/base-panel";
import {
	Select,
	SelectContent,
	SelectItem,
	SelectTrigger,
	SelectValue,
} from "@/components/ui/select";
import { FPS_PRESETS } from "@/fps/presets";
import { floatToFrameRate, frameRateToFloat } from "@/fps/utils";
import { useEditor } from "@/editor/use-editor";
import {
	Section,
	SectionContent,
	SectionHeader,
	SectionTitle,
} from "@/components/section";
import { BackgroundContent } from "./background";
import { Tabs, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { Button } from "@/components/ui/button";
import { NumberField } from "@/components/ui/number-field";
import { useEditorStore } from "@/editor/editor-store";
import { HugeiconsIcon } from "@hugeicons/react";
import { Tick02Icon } from "@hugeicons/core-free-icons";
import { cn } from "@/utils/ui";
import { dimensionToAspectRatio } from "@/utils/geometry";
import { formatNumberForDisplay } from "@/utils/math";
import { OcSquarePlusIcon } from "@/components/icons";
import type { TCanvasSize } from "@/project/types";
import { isLanguage, LANGUAGE_OPTIONS, useTranslation } from "@/i18n";
import { McpSettingsView } from "@/mcp/settings-view";
import {
	Dialog,
	DialogContent,
	DialogHeader,
	DialogTitle,
	DialogBody,
	DialogFooter,
} from "@/components/ui/dialog";

type SettingsView = "project-info" | "background" | "mcp";

function isSettingsView(value: string): value is SettingsView {
	return value === "project-info" || value === "background" || value === "mcp";
}

const PRESET_LABELS: Record<string, string> = {
	"1:1": "1:1",
	"16:9": "16:9",
	"9:16": "9:16",
	"4:3": "4:3",
};

function areCanvasSizesEqual({
	left,
	right,
}: {
	left: TCanvasSize;
	right: TCanvasSize;
}) {
	return left.width === right.width && left.height === right.height;
}

function formatCanvasDimension({ value }: { value: number }) {
	return formatNumberForDisplay({ value, maxFractionDigits: 0 });
}

function parseCanvasDimension({ input }: { input: string }): number | null {
	const trimmed = input.trim();
	if (!trimmed) return null;

	const parsed = Number(trimmed);
	if (!Number.isFinite(parsed)) return null;

	const rounded = Math.round(parsed);
	return rounded > 0 ? rounded : null;
}

export function SettingsView() {
	const [view, setView] = useState<SettingsView>("project-info");
	const [isCustomCanvasDialogOpen, setIsCustomCanvasDialogOpen] =
		useState(false);
	const [customCanvasDraft, setCustomCanvasDraft] = useState({
		width: "",
		height: "",
	});
	const editor = useEditor();
	const { language, setLanguage, t } = useTranslation();
	const activeProject = useEditor((e) => e.project.getActive());
	const { canvasPresets } = useEditorStore();
	const currentCanvasSize = activeProject.settings.canvasSize;
	const canvasSizeMode = activeProject.settings.canvasSizeMode ?? "preset";
	const lastCustomCanvasSize =
		activeProject.settings.lastCustomCanvasSize ?? null;

	const presetItems = canvasPresets.map((preset, index) => {
		const ratio = dimensionToAspectRatio(preset);
		return {
			id: index.toString(),
			label: PRESET_LABELS[ratio] ?? ratio,
			ratio,
			canvasSize: preset,
		};
	});

	const selectedPresetId =
		canvasSizeMode === "preset"
			? (presetItems.find((preset) =>
					areCanvasSizesEqual({
						left: preset.canvasSize,
						right: currentCanvasSize,
					}),
				)?.id ?? null)
			: null;

	const updateCustomCanvasSize = ({
		canvasSize,
	}: {
		canvasSize: TCanvasSize;
	}) => {
		const shouldUpdateCanvasSize = !areCanvasSizesEqual({
			left: canvasSize,
			right: currentCanvasSize,
		});
		const shouldUpdateLastCustomCanvasSize =
			lastCustomCanvasSize === null ||
			!areCanvasSizesEqual({
				left: canvasSize,
				right: lastCustomCanvasSize,
			});
		const shouldUpdateCanvasSizeMode = canvasSizeMode !== "custom";

		if (
			!shouldUpdateCanvasSize &&
			!shouldUpdateLastCustomCanvasSize &&
			!shouldUpdateCanvasSizeMode
		) {
			return;
		}

		editor.project.updateSettings({
			settings: {
				...(shouldUpdateCanvasSize ? { canvasSize } : {}),
				...(shouldUpdateCanvasSizeMode
					? { canvasSizeMode: "custom" as const }
					: {}),
				lastCustomCanvasSize: canvasSize,
			},
		});
	};

	const selectPresetCanvasSize = ({
		canvasSize,
	}: {
		canvasSize: TCanvasSize;
	}) => {
		const shouldUpdateCanvasSize = !areCanvasSizesEqual({
			left: canvasSize,
			right: currentCanvasSize,
		});
		const shouldUpdateCanvasSizeMode = canvasSizeMode !== "preset";

		if (!shouldUpdateCanvasSize && !shouldUpdateCanvasSizeMode) return;

		editor.project.updateSettings({
			settings: {
				...(shouldUpdateCanvasSize ? { canvasSize } : {}),
				...(shouldUpdateCanvasSizeMode
					? { canvasSizeMode: "preset" as const }
					: {}),
			},
		});
	};

	const openCustomCanvasDialog = () => {
		const canvasSize =
			canvasSizeMode === "custom"
				? currentCanvasSize
				: (lastCustomCanvasSize ?? currentCanvasSize);
		setCustomCanvasDraft({
			width: formatCanvasDimension({ value: canvasSize.width }),
			height: formatCanvasDimension({ value: canvasSize.height }),
		});
		setIsCustomCanvasDialogOpen(true);
	};

	const applyCustomCanvasSize = () => {
		const width = parseCanvasDimension({ input: customCanvasDraft.width });
		const height = parseCanvasDimension({ input: customCanvasDraft.height });
		if (width === null || height === null) return;

		updateCustomCanvasSize({
			canvasSize: { width, height },
		});
		setIsCustomCanvasDialogOpen(false);
	};

	const isCustomSelected = canvasSizeMode === "custom";
	const canApplyCustomCanvasSize =
		parseCanvasDimension({ input: customCanvasDraft.width }) !== null &&
		parseCanvasDimension({ input: customCanvasDraft.height }) !== null;
	const projectName =
		language === "vi" && activeProject.metadata.name === "New project"
			? t("assets.projectNameDefault")
			: activeProject.metadata.name;

	return (
		<PanelView
			contentClassName="px-0"
			scrollClassName="pt-0"
			actions={
				<Tabs
					value={view}
					onValueChange={(value) => {
						if (isSettingsView(value)) {
							setView(value);
						}
					}}
				>
					<TabsList>
						<TabsTrigger value="project-info">
							{t("settings.projectInfo")}
						</TabsTrigger>
						<TabsTrigger value="background">
							{t("settings.background")}
						</TabsTrigger>
						<TabsTrigger value="mcp">MCP</TabsTrigger>
					</TabsList>
				</Tabs>
			}
		>
			{view === "project-info" && (
				<div className="flex flex-col">
					<Section showTopBorder={false}>
						<SectionHeader>
							<SectionTitle className="flex-1">
								{t("settings.name")}
							</SectionTitle>
							<span className="text-sm truncate">{projectName}</span>
						</SectionHeader>
					</Section>
					<Section showTopBorder={false}>
						<SectionHeader className="justify-between">
							<SectionTitle className="flex-1">
								{t("settings.language")}
							</SectionTitle>
							<Select
								value={language}
								onValueChange={(value) => {
									if (isLanguage(value)) {
										setLanguage(value);
									}
								}}
							>
								<SelectTrigger className="bg-transparent border-none p-1 h-auto">
									<SelectValue />
								</SelectTrigger>
								<SelectContent>
									{LANGUAGE_OPTIONS.map((option) => (
										<SelectItem key={option.value} value={option.value}>
											{t(option.labelKey)}
										</SelectItem>
									))}
								</SelectContent>
							</Select>
						</SectionHeader>
					</Section>
					<Section showTopBorder={false}>
						<SectionHeader className="justify-between">
							<SectionTitle className="flex-1">
								{t("settings.frameRate")}
							</SectionTitle>
							<Select
								value={String(
									Math.round(frameRateToFloat(activeProject.settings.fps)),
								)}
								onValueChange={(value) => {
									const fps = floatToFrameRate(parseFloat(value));
									editor.project.updateSettings({ settings: { fps } });
								}}
							>
								<SelectTrigger className="bg-transparent border-none p-1 h-auto">
									<SelectValue placeholder={t("settings.selectFrameRate")} />
								</SelectTrigger>
								<SelectContent>
									{FPS_PRESETS.map((preset) => (
										<SelectItem key={preset.value} value={preset.value}>
											{preset.label}
										</SelectItem>
									))}
								</SelectContent>
							</Select>
						</SectionHeader>
					</Section>
					<Section
						showTopBorder={false}
						collapsible
						sectionKey="settings:aspect-ratio"
					>
						<SectionHeader>
							<SectionTitle className="flex-1">
								{t("settings.aspectRatio")}
							</SectionTitle>
						</SectionHeader>
						<SectionContent className="px-2 flex flex-col gap-1 pb-2">
							{presetItems.map((preset) => (
								<AspectRatioItem
									key={preset.id}
									label={preset.label}
									previewIcon={<AspectRatioPreview ratio={preset.ratio} />}
									isSelected={selectedPresetId === preset.id}
									onClick={() => {
										selectPresetCanvasSize({
											canvasSize: preset.canvasSize,
										});
									}}
								/>
							))}
							<div className="pb-2">
								<AspectRatioItem
									key="custom"
									label={t("settings.custom")}
									previewIcon={<OcSquarePlusIcon />}
									isSelected={isCustomSelected}
									onClick={openCustomCanvasDialog}
								/>
							</div>
						</SectionContent>
					</Section>
					<Dialog
						open={isCustomCanvasDialogOpen}
						onOpenChange={setIsCustomCanvasDialogOpen}
					>
						<DialogContent className="max-w-sm overflow-hidden p-0">
							<DialogHeader>
								<DialogTitle>{t("settings.customCanvasSize")}</DialogTitle>
							</DialogHeader>
							<DialogBody>
								<div className="grid grid-cols-2 gap-3">
									<div className="flex flex-col gap-1.5">
										<label
											className="text-xs font-medium text-muted-foreground"
											htmlFor="custom-canvas-width"
										>
											{t("settings.canvasWidth")}
										</label>
										<NumberField
											id="custom-canvas-width"
											value={customCanvasDraft.width}
											aria-label={t("settings.canvasWidth")}
											onChange={(event) =>
												setCustomCanvasDraft((draft) => ({
													...draft,
													width: event.target.value,
												}))
											}
										/>
									</div>
									<div className="flex flex-col gap-1.5">
										<label
											className="text-xs font-medium text-muted-foreground"
											htmlFor="custom-canvas-height"
										>
											{t("settings.canvasHeight")}
										</label>
										<NumberField
											id="custom-canvas-height"
											value={customCanvasDraft.height}
											aria-label={t("settings.canvasHeight")}
											onChange={(event) =>
												setCustomCanvasDraft((draft) => ({
													...draft,
													height: event.target.value,
												}))
											}
										/>
									</div>
								</div>
							</DialogBody>
							<DialogFooter>
								<Button
									variant="outline"
									onClick={() => setIsCustomCanvasDialogOpen(false)}
								>
									{t("common.cancel")}
								</Button>
								<Button
									onClick={applyCustomCanvasSize}
									disabled={!canApplyCustomCanvasSize}
								>
									{t("common.apply")}
								</Button>
							</DialogFooter>
						</DialogContent>
					</Dialog>
				</div>
			)}
			{view === "background" && <BackgroundContent />}
			{view === "mcp" && <McpSettingsView />}
		</PanelView>
	);
}

function AspectRatioItem({
	label,
	previewIcon,
	isSelected,
	onClick,
	uiOptions,
}: {
	label: string;
	previewIcon: React.ReactNode;
	isSelected: boolean;
	onClick: () => void;
	uiOptions?: React.ReactNode;
}) {
	return (
		<Button
			variant={isSelected ? "secondary" : "ghost"}
			className={cn(
				"px-2 py-0 flex flex-col h-fit w-full",
				!isSelected && "border border-transparent opacity-75!",
			)}
			onClick={onClick}
		>
			<div className="w-full flex justify-between items-center h-8">
				<div className="flex-1 flex items-center gap-2">
					<div className="flex items-center justify-center size-5">
						{previewIcon}
					</div>
					<span className="text-sm truncate">{label}</span>
				</div>
				<div>
					{isSelected && <HugeiconsIcon icon={Tick02Icon} className="size-4" />}
				</div>
			</div>
			{uiOptions && isSelected && (
				<div className="w-full pb-2">{uiOptions}</div>
			)}
		</Button>
	);
}

function AspectRatioPreview({ ratio }: { ratio?: string }) {
	if (!ratio) return null;

	const [w, h] = ratio.split(":").map(Number);
	const maxSize = 16;
	const width = w >= h ? maxSize : (w / h) * maxSize;
	const height = h >= w ? maxSize : (h / w) * maxSize;

	return (
		<div
			style={{ width, height, borderWidth: 1.5 }}
			className="rounded-xs border-current opacity-60"
		/>
	);
}

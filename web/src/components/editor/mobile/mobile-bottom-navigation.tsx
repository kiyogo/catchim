"use client";

import { useMemo } from "react";
import { useTranslation } from "@/i18n";
import { useElementSelection } from "@/timeline/hooks/element/use-element-selection";
import { useAssetsPanelStore, type Tab } from "@/components/editor/panels/assets/assets-panel-store";
import { usePropertiesStore } from "@/components/editor/panels/properties/stores/properties-store";
import { useEditor } from "@/editor/use-editor";
import { invokeAction } from "@/actions";
import { type MobileSheetType } from "./mobile-bottom-sheet";
import { cn } from "@/utils/ui";
import {
	Scissors,
	Music,
	Type,
	Smile,
	Sparkles,
	Link2,
	Subtitles,
	RectangleVertical,
	Palette,
	Settings,
	ArrowLeft,
	Gauge,
	Volume2,
	Move,
	Trash2,
	Copy,
	Layers,
	Sliders,
	Music2,
} from "lucide-react";

interface MobileBottomNavigationProps {
	activeSheet: MobileSheetType;
	onOpenSheet: (sheet: MobileSheetType) => void;
	onCloseSheet: () => void;
}

export function MobileBottomNavigation({
	activeSheet,
	onOpenSheet,
	onCloseSheet,
}: MobileBottomNavigationProps) {
	const { t } = useTranslation();
	const editor = useEditor();
	const { selectedElements, clearElementSelection, setElementSelection } =
		useElementSelection();
	const { activeTab, setActiveTab } = useAssetsPanelStore();
	const hasSelection = selectedElements.length > 0;

	// Detect type of selected element (e.g. video, audio, text, sticker)
	const selectedElementType = useMemo(() => {
		if (selectedElements.length === 0) return null;
		const first = selectedElements[0];
		const elementsWithTracks = editor.timeline.getElementsWithTracks({
			elements: [first],
		});
		return elementsWithTracks[0]?.element?.type ?? "video";
	}, [selectedElements, editor.timeline]);

	const vibrate = (ms = 10) => {
		if (typeof navigator !== "undefined" && navigator.vibrate) {
			navigator.vibrate(ms);
		}
	};

	// Open an asset tab in bottom sheet
	const handleOpenAssetTab = (tab: Tab) => {
		vibrate();
		setActiveTab(tab);
		onOpenSheet("assets");
	};

	// Open a specific properties tab for the selected element
	const handleOpenPropertyTab = (tabId: string) => {
		vibrate();
		if (selectedElementType) {
			usePropertiesStore
				.getState()
				.setActiveTab({ elementType: selectedElementType, tabId });
		}
		onOpenSheet("properties");
	};

	// When user clicks "Edit" in main menu, select current clip or first clip
	const handleEditMainClick = () => {
		vibrate();
		const currentTime = editor.playback.getCurrentTime();
		const scene = editor.scenes.getActiveSceneOrNull();
		if (!scene) return;

		// 1. Try to find element at current playhead on main track or overlays
		const allTracks = [...scene.tracks.overlay, scene.tracks.main];
		for (const track of allTracks) {
			for (const el of track.elements) {
				const start = el.startTime;
				const end = el.startTime + el.duration;
				if (currentTime >= start && currentTime < end) {
					setElementSelection({ elements: [{ trackId: track.id, elementId: el.id }] });
					return;
				}
			}
		}

		// 2. If nothing at playhead, select the first available main clip
		if (scene.tracks.main.elements.length > 0) {
			const firstEl = scene.tracks.main.elements[0];
			setElementSelection({
				elements: [{ trackId: scene.tracks.main.id, elementId: firstEl.id }],
			});
			return;
		}

		// 3. Otherwise open media sheet to import
		handleOpenAssetTab("media");
	};

	// Contextual Actions
	const handleSplit = () => {
		vibrate(15);
		invokeAction("split");
	};

	const handleDelete = () => {
		vibrate(20);
		invokeAction("delete-selected");
	};

	const handleDuplicate = () => {
		vibrate(12);
		invokeAction("duplicate-selected");
	};

	const handleExtractAudio = () => {
		vibrate(12);
		invokeAction("toggle-source-audio");
	};

	const handleDeselect = () => {
		vibrate(8);
		clearElementSelection();
		onCloseSheet();
	};
	const mcpButton = (
		<button type="button" onClick={() => handleOpenAssetTab("mcp")}
			className="flex h-14 min-w-[4.15rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground hover:bg-accent hover:text-foreground"
			aria-label={t("assets.mcp")}>
			<Link2 className="size-5" /><span className="truncate max-w-full">{t("assets.mcp")}</span>
		</button>
	);

	return (
		<nav className="fixed inset-x-0 bottom-0 z-50 select-none border-t bg-background/95 pb-[calc(0.4rem+env(safe-area-inset-bottom))] pt-1 backdrop-blur-lg shadow-[0_-4px_20px_rgba(0,0,0,0.12)]">
			{hasSelection ? (
				/* ============================================================ */
				/* MODE B: ELEMENT SELECTED (CapCut Contextual Clip Action Bar)  */
				/* ============================================================ */
				<div className="scrollbar-hidden flex items-center gap-1 overflow-x-auto px-2">
					{/* Back / Deselect button */}
					<button
						type="button"
						onClick={handleDeselect}
						className="flex h-14 min-w-[3.9rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-foreground transition-all active:scale-90 bg-muted/60"
						aria-label={t("mobile.back")}
					>
						<ArrowLeft className="size-5" />
						<span className="truncate max-w-full">{t("mobile.back")}</span>
					</button>

					<div className="h-6 w-px bg-border/60 mx-0.5 shrink-0" />

					{/* 1. Split (Tách) - CapCut's most important button */}
					<button
						type="button"
						onClick={handleSplit}
						className="flex h-14 min-w-[4rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-foreground transition-all active:scale-90 hover:bg-accent"
						aria-label={t("mobile.split")}
					>
						<Scissors className="size-5 text-primary" />
						<span className="truncate max-w-full font-semibold">{t("mobile.split")}</span>
					</button>

					{/* 2. Speed (Tốc độ) */}
					<button
						type="button"
						onClick={() => handleOpenPropertyTab("speed")}
						className={cn(
							"flex h-14 min-w-[4rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent",
							activeSheet === "properties" && "text-foreground bg-accent",
						)}
						aria-label={t("mobile.speed")}
					>
						<Gauge className="size-5" />
						<span className="truncate max-w-full">{t("mobile.speed")}</span>
					</button>

					{/* 3. Volume (Âm lượng) */}
					<button
						type="button"
						onClick={() => handleOpenPropertyTab("audio")}
						className="flex h-14 min-w-[4rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent"
						aria-label={t("mobile.volume")}
					>
						<Volume2 className="size-5" />
						<span className="truncate max-w-full">{t("mobile.volume")}</span>
					</button>

					{/* 4. Transform (Biến đổi) */}
					<button
						type="button"
						onClick={() => handleOpenPropertyTab("transform")}
						className="flex h-14 min-w-[4rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent"
						aria-label={t("mobile.transform")}
					>
						<Move className="size-5" />
						<span className="truncate max-w-full">{t("mobile.transform")}</span>
					</button>

					{/* 5. Animation (Động) */}
					<button
						type="button"
						onClick={() => handleOpenPropertyTab("effects")}
						className="flex h-14 min-w-[4rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent"
						aria-label={t("mobile.animation")}
					>
						<Sparkles className="size-5" />
						<span className="truncate max-w-full">{t("mobile.animation")}</span>
					</button>

					{/* 6. Delete (Xóa) - Highlighted in Red */}
					<button
						type="button"
						onClick={handleDelete}
						className="flex h-14 min-w-[4rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-destructive transition-all active:scale-90 hover:bg-destructive/10"
						aria-label={t("mobile.delete")}
					>
						<Trash2 className="size-5" />
						<span className="truncate max-w-full font-semibold">{t("mobile.delete")}</span>
					</button>

					{/* 7. Duplicate (Nhân bản) */}
					<button
						type="button"
						onClick={handleDuplicate}
						className="flex h-14 min-w-[4rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent"
						aria-label={t("mobile.duplicate")}
					>
						<Copy className="size-5" />
						<span className="truncate max-w-full">{t("mobile.duplicate")}</span>
					</button>

					{/* 8. Opacity / Blending (Độ mờ) */}
					<button
						type="button"
						onClick={() => handleOpenPropertyTab("blending")}
						className="flex h-14 min-w-[4rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent"
						aria-label={t("mobile.opacity")}
					>
						<Layers className="size-5" />
						<span className="truncate max-w-full">{t("mobile.opacity")}</span>
					</button>

					{/* 9. Extract Audio (Tách âm thanh) */}
					<button
						type="button"
						onClick={handleExtractAudio}
						className="flex h-14 min-w-[4rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent"
						aria-label={t("mobile.extractAudio")}
					>
						<Music2 className="size-5" />
						<span className="truncate max-w-full">{t("mobile.extractAudio")}</span>
					</button>

					{/* 10. Properties / All (Thuộc tính) */}
					<button
						type="button"
						onClick={() => {
							vibrate();
							onOpenSheet("properties");
						}}
						className="flex h-14 min-w-[4rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent"
						aria-label={t("mobile.properties")}
					>
						<Sliders className="size-5" />
						<span className="truncate max-w-full">{t("mobile.properties")}</span>
					</button>
					{mcpButton}
				</div>
			) : (
				/* ============================================================ */
				/* MODE A: MAIN NAVIGATION BAR (CapCut Standard Bottom Bar)     */
				/* ============================================================ */
				<div className="scrollbar-hidden flex items-center gap-1 overflow-x-auto px-2">
					{/* 1. Edit (Chỉnh sửa) */}
					<button
						type="button"
						onClick={handleEditMainClick}
						className="flex h-14 min-w-[4.15rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent hover:text-foreground"
						aria-label={t("mobile.edit")}
					>
						<Scissors className="size-5" />
						<span className="truncate max-w-full">{t("mobile.edit")}</span>
					</button>

					{/* 2. Audio (Âm thanh) */}
					<button
						type="button"
						onClick={() => handleOpenAssetTab("sounds")}
						className={cn(
							"flex h-14 min-w-[4.15rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent hover:text-foreground",
							activeSheet === "assets" && activeTab === "sounds" && "bg-secondary text-secondary-foreground font-semibold",
						)}
						aria-label={t("mobile.audio")}
					>
						<Music className="size-5" />
						<span className="truncate max-w-full">{t("mobile.audio")}</span>
					</button>

					{/* 3. Text (Văn bản) */}
					<button
						type="button"
						onClick={() => handleOpenAssetTab("text")}
						className={cn(
							"flex h-14 min-w-[4.15rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent hover:text-foreground",
							activeSheet === "assets" && activeTab === "text" && "bg-secondary text-secondary-foreground font-semibold",
						)}
						aria-label={t("mobile.text")}
					>
						<Type className="size-5" />
						<span className="truncate max-w-full">{t("mobile.text")}</span>
					</button>

					{/* 4. Stickers (Nhãn dán) */}
					<button
						type="button"
						onClick={() => handleOpenAssetTab("stickers")}
						className={cn(
							"flex h-14 min-w-[4.15rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent hover:text-foreground",
							activeSheet === "assets" && activeTab === "stickers" && "bg-secondary text-secondary-foreground font-semibold",
						)}
						aria-label={t("mobile.stickers")}
					>
						<Smile className="size-5" />
						<span className="truncate max-w-full">{t("mobile.stickers")}</span>
					</button>

					{/* 5. Effects (Hiệu ứng) */}
					<button
						type="button"
						onClick={() => handleOpenAssetTab("effects")}
						className={cn(
							"flex h-14 min-w-[4.15rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent hover:text-foreground",
							activeSheet === "assets" && activeTab === "effects" && "bg-secondary text-secondary-foreground font-semibold",
						)}
						aria-label={t("mobile.effects")}
					>
						<Sparkles className="size-5" />
						<span className="truncate max-w-full">{t("mobile.effects")}</span>
					</button>

					{/* 6. Captions (Phụ đề) */}
					<button
						type="button"
						onClick={() => handleOpenAssetTab("captions")}
						className={cn(
							"flex h-14 min-w-[4.15rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent hover:text-foreground",
							activeSheet === "assets" && activeTab === "captions" && "bg-secondary text-secondary-foreground font-semibold",
						)}
						aria-label={t("mobile.captions")}
					>
						<Subtitles className="size-5" />
						<span className="truncate max-w-full">{t("mobile.captions")}</span>
					</button>

					{/* 7. Ratio (Tỷ lệ khung hình) */}
					<button
						type="button"
						onClick={() => {
							vibrate();
							onOpenSheet("ratio");
						}}
						className={cn(
							"flex h-14 min-w-[4.15rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent hover:text-foreground",
							activeSheet === "ratio" && "bg-secondary text-secondary-foreground font-semibold",
						)}
						aria-label={t("mobile.ratio")}
					>
						<RectangleVertical className="size-5" />
						<span className="truncate max-w-full">{t("mobile.ratio")}</span>
					</button>

					{/* 8. Background (Phông nền) */}
					<button
						type="button"
						onClick={() => {
							vibrate();
							onOpenSheet("background");
						}}
						className={cn(
							"flex h-14 min-w-[4.15rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent hover:text-foreground",
							activeSheet === "background" && "bg-secondary text-secondary-foreground font-semibold",
						)}
						aria-label={t("mobile.background")}
					>
						<Palette className="size-5" />
						<span className="truncate max-w-full">{t("mobile.background")}</span>
					</button>

					{/* 9. Settings (Cài đặt) */}
					<button
						type="button"
						onClick={() => handleOpenAssetTab("settings")}
						className={cn(
							"flex h-14 min-w-[4.15rem] flex-col items-center justify-center gap-1 rounded-lg px-2 text-[11px] font-medium text-muted-foreground transition-all active:scale-90 hover:bg-accent hover:text-foreground",
							activeSheet === "assets" && activeTab === "settings" && "bg-secondary text-secondary-foreground font-semibold",
						)}
						aria-label={t("mobile.settings")}
					>
						<Settings className="size-5" />
						<span className="truncate max-w-full">{t("mobile.settings")}</span>
					</button>
					{mcpButton}
				</div>
			)}
		</nav>
	);
}

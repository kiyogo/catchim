"use client";

import { useEffect, useMemo, useRef, useState } from "react";
import { Layers, Search, Star } from "lucide-react";
import { toast } from "sonner";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { PanelView } from "@/components/editor/panels/assets/views/base-panel";
import { DraggableItem } from "@/components/editor/panels/assets/draggable-item";
import { effectsRegistry, EFFECT_TARGET_ELEMENT_TYPES } from "@/effects";
import type { EffectCategory, EffectDefinition } from "@/effects/types";
import { effectPreviewService } from "@/services/renderer/effect-preview";
import { useEditor } from "@/editor/use-editor";
import { buildEffectElement, isVisualElement } from "@/timeline/element-utils";
import type { VisualElement } from "@/timeline";
import { isTrackLocked } from "@/timeline/track-capabilities";
import { useTranslation, type TranslationKey } from "@/i18n";

const FAVORITES_STORAGE_KEY = "catchim-effects-favorites-v1";

type EffectCatalogCategory = "all" | "favorites" | EffectCategory;
type SelectedVisualClip = {
	trackId: string;
	element: VisualElement;
	locked: boolean;
} | null;

const CATEGORIES: Array<{
	id: EffectCatalogCategory;
	label: TranslationKey;
}> = [
	{ id: "all", label: "assets.effectCategoryAll" },
	{ id: "favorites", label: "assets.effectCategoryFavorites" },
	{ id: "blur", label: "assets.effectCategoryBlur" },
	{ id: "color", label: "assets.effectCategoryColor" },
	{ id: "keying", label: "assets.effectCategoryKeying" },
	{ id: "stylize", label: "assets.effectCategoryStylize" },
];

export function EffectsView() {
	const { t } = useTranslation();
	const effects = effectsRegistry.getAll();
	const selectedElements = useEditor((state) =>
		state.selection.getSelectedElements(),
	);
	const tracks = useEditor(
		(state) => state.scenes.getActiveSceneOrNull()?.tracks ?? null,
	);
	const [query, setQuery] = useState("");
	const [category, setCategory] = useState<EffectCatalogCategory>("all");
	const [favoriteTypes, setFavoriteTypes] = useState<string[]>([]);
	const [favoritesLoaded, setFavoritesLoaded] = useState(false);

	useEffect(() => {
		try {
			const saved = window.localStorage.getItem(FAVORITES_STORAGE_KEY);
			const parsed: unknown = saved ? JSON.parse(saved) : [];
			if (Array.isArray(parsed)) {
				setFavoriteTypes(parsed.filter((item): item is string => typeof item === "string"));
			}
		} catch {
			setFavoriteTypes([]);
		} finally {
			setFavoritesLoaded(true);
		}
	}, []);

	useEffect(() => {
		if (!favoritesLoaded) return;
		try {
			window.localStorage.setItem(
				FAVORITES_STORAGE_KEY,
				JSON.stringify(favoriteTypes),
			);
		} catch {
			// Favorites remain usable for this session when browser storage is unavailable.
		}
	}, [favoriteTypes, favoritesLoaded]);

	const selectedClip = useMemo<SelectedVisualClip>(() => {
		if (selectedElements.length !== 1 || !tracks) return null;
		const selectedRef = selectedElements[0];
		const track = [tracks.main, ...tracks.overlay, ...tracks.audio].find(
			(candidate) => candidate.id === selectedRef.trackId,
		);
		const element = track?.elements.find(
			(candidate) => candidate.id === selectedRef.elementId,
		);
		return track && element && isVisualElement(element)
			? {
					trackId: track.id,
					element: element as VisualElement,
					locked: isTrackLocked(track),
				}
			: null;
	}, [selectedElements, tracks]);

	const filteredEffects = useMemo(() => {
		const normalizedQuery = query.trim().toLocaleLowerCase();
		return effects.filter((effect) => {
			if (category === "favorites" && !favoriteTypes.includes(effect.type)) {
				return false;
			}
			if (
				category !== "all" &&
				category !== "favorites" &&
				effect.category !== category
			) {
				return false;
			}
			if (!normalizedQuery) return true;
			return [effect.name, effect.type, ...effect.keywords]
				.join(" ")
				.toLocaleLowerCase()
				.includes(normalizedQuery);
		});
	}, [category, effects, favoriteTypes, query]);

	const toggleFavorite = (effectType: string) => {
		setFavoriteTypes((current) =>
			current.includes(effectType)
				? current.filter((item) => item !== effectType)
				: [...current, effectType],
		);
	};

	return (
		<PanelView title={t("assets.effects")}>
			<div className="mb-3 flex flex-col gap-3">
				<div className="relative">
					<Search className="text-muted-foreground pointer-events-none absolute left-2.5 top-1/2 size-4 -translate-y-1/2" />
					<Input
						aria-label={t("assets.searchEffects")}
						className="h-8 pl-8 text-xs"
						placeholder={t("assets.searchEffects")}
						value={query}
						onChange={(event) => setQuery(event.currentTarget.value)}
					/>
				</div>
				<div className="scrollbar-hidden -mx-1 flex gap-1 overflow-x-auto px-1 pb-1">
					{CATEGORIES.map((item) => (
						<Button
							key={item.id}
							variant={category === item.id ? "secondary" : "ghost"}
							size="sm"
							className="h-7 shrink-0 text-xs"
							onClick={() => setCategory(item.id)}
						>
							{t(item.label)}
						</Button>
					))}
				</div>
			</div>

			{filteredEffects.length === 0 ? (
				<div className="text-muted-foreground flex min-h-32 items-center justify-center p-4 text-center text-sm">
					{t("assets.noEffectsFound")}
				</div>
			) : (
				<EffectsGrid
					effects={filteredEffects}
					favoriteTypes={favoriteTypes}
					selectedClip={selectedClip}
					toggleFavorite={toggleFavorite}
				/>
			)}
		</PanelView>
	);
}

function EffectsGrid({
	effects,
	favoriteTypes,
	selectedClip,
	toggleFavorite,
}: {
	effects: EffectDefinition[];
	favoriteTypes: string[];
	selectedClip: SelectedVisualClip;
	toggleFavorite: (effectType: string) => void;
}) {
	return (
		<div
			className="grid gap-2"
			style={{ gridTemplateColumns: "repeat(auto-fill, minmax(96px, 1fr))" }}
		>
			{effects.map((effect) => (
				<EffectItem
					key={effect.type}
					effect={effect}
					isFavorite={favoriteTypes.includes(effect.type)}
					selectedClip={selectedClip}
					onToggleFavorite={() => toggleFavorite(effect.type)}
				/>
			))}
		</div>
	);
}

function EffectPreviewCanvas({ effectType }: { effectType: string }) {
	const canvasRef = useRef<HTMLCanvasElement>(null);

	useEffect(() => {
		const render = () => {
			if (canvasRef.current) {
				effectPreviewService.renderPreview({
					effectType,
					params: {},
					targetCanvas: canvasRef.current,
				});
			}
		};

		render();
		return effectPreviewService.onPreviewImageReady({ callback: render });
	}, [effectType]);

	return <canvas ref={canvasRef} className="size-full" />;
}

function EffectItem({
	effect,
	isFavorite,
	selectedClip,
	onToggleFavorite,
}: {
	effect: EffectDefinition;
	isFavorite: boolean;
	selectedClip: SelectedVisualClip;
	onToggleFavorite: () => void;
}) {
	const { t } = useTranslation();
	const editor = useEditor();
	const canApplyToSelected = Boolean(selectedClip && !selectedClip.locked);

	const handleAddToTimeline = () => {
		const currentTime = editor.playback.getCurrentTime();
		const element = buildEffectElement({
			effectType: effect.type,
			startTime: currentTime,
		});
		editor.timeline.insertElement({
			placement: { mode: "auto", trackType: "effect" },
			element,
		});
	};

	const handleApplyToSelected = () => {
		if (!selectedClip || selectedClip.locked) return;
		const effectId = editor.timeline.addClipEffect({
			trackId: selectedClip.trackId,
			elementId: selectedClip.element.id,
			effectType: effect.type,
		});
		if (effectId) toast.success(t("assets.effectAddedToClip"));
		else toast.error(t("common.unknownError"));
	};

	const preview = (
		<div className="group/effect-preview relative size-full">
			<EffectPreviewCanvas effectType={effect.type} />
			<Button
				variant="background"
				size="icon"
				className="absolute left-1 top-1 z-10 size-6 bg-background/90 opacity-0 shadow transition-opacity group-hover/effect-preview:opacity-100 focus-visible:opacity-100"
				aria-label={t(
					isFavorite
						? "assets.removeEffectFavorite"
						: "assets.addEffectFavorite",
				)}
				title={t(
					isFavorite
						? "assets.removeEffectFavorite"
						: "assets.addEffectFavorite",
				)}
				onPointerDown={(event) => event.stopPropagation()}
				onClick={(event) => {
					event.preventDefault();
					event.stopPropagation();
					onToggleFavorite();
				}}
			>
				<Star className="size-3.5" fill={isFavorite ? "currentColor" : "none"} />
			</Button>
			<Button
				variant="background"
				size="icon"
				className="absolute right-1 top-1 z-10 size-6 bg-background/90 opacity-0 shadow transition-opacity group-hover/effect-preview:opacity-100 focus-visible:opacity-100"
				aria-label={t("assets.addEffectToSelectedClip")}
				title={
					canApplyToSelected
						? t("assets.addEffectToSelectedClip")
						: t("assets.selectClipToApplyEffect")
				}
				disabled={!canApplyToSelected}
				onPointerDown={(event) => event.stopPropagation()}
				onClick={(event) => {
					event.preventDefault();
					event.stopPropagation();
					handleApplyToSelected();
				}}
			>
				<Layers className="size-3.5" />
			</Button>
		</div>
	);

	return (
		<DraggableItem
			name={effect.name}
			preview={preview}
			dragData={{
				id: effect.type,
				name: effect.name,
				type: "effect",
				effectType: effect.type,
				targetElementTypes: EFFECT_TARGET_ELEMENT_TYPES,
			}}
			onAddToTimeline={handleAddToTimeline}
			aspectRatio={1}
			isRounded
			variant="card"
		/>
	);
}

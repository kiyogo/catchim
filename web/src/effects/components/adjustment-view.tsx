"use client";

import { Button } from "@/components/ui/button";
import { PanelView } from "@/components/editor/panels/assets/views/base-panel";
import { ClipEffectsTab } from "@/effects/components/effects-tab";
import { isTrackLocked } from "@/timeline/track-capabilities";
import type { VisualElement } from "@/timeline";
import { useEditor } from "@/editor/use-editor";
import { useTranslation } from "@/i18n";

export function AdjustmentView() {
	const { t } = useTranslation();
	const editor = useEditor();
	const selectedElements = useEditor((state) =>
		state.selection.getSelectedElements(),
	);
	const tracks = useEditor(
		(state) => state.scenes.getActiveSceneOrNull()?.tracks ?? null,
	);

	const selected =
		selectedElements.length === 1 && tracks
			? (() => {
					const selectedRef = selectedElements[0];
					const track = [...tracks.overlay, tracks.main, ...tracks.audio].find(
						(candidate) => candidate.id === selectedRef.trackId,
					);
					const element = track?.elements.find(
						(candidate) => candidate.id === selectedRef.elementId,
					);
					return track && element ? { track, element } : null;
				})()
			: null;

	const isVisual =
		selected?.element.type === "video" ||
		selected?.element.type === "image" ||
		selected?.element.type === "text" ||
		selected?.element.type === "sticker" ||
		selected?.element.type === "graphic";
	const visualElement = isVisual
		? (selected?.element as VisualElement)
		: null;
	const colorEffect = visualElement?.effects?.some(
		(effect) => effect.type === "color-grading",
	);

	return (
		<PanelView title={t("assets.adjustment")}>
			{!selected || !visualElement ? (
				<div className="text-muted-foreground flex h-full items-center justify-center p-4 text-center text-sm">
					{t("assets.adjustmentSelectClip")}
				</div>
			) : colorEffect ? (
				<ClipEffectsTab element={visualElement} trackId={selected.track.id} />
			) : (
				<div className="flex h-full flex-col items-center justify-center gap-3 p-4 text-center">
					<p className="text-muted-foreground text-sm">
						{t("assets.colorAdjustmentHint")}
					</p>
					<Button
						disabled={isTrackLocked(selected.track)}
						onClick={() =>
							editor.timeline.addClipEffect({
								trackId: selected.track.id,
								elementId: visualElement.id,
								effectType: "color-grading",
							})
						}
					>
						{t("assets.addColorAdjustment")}
					</Button>
				</div>
			)}
		</PanelView>
	);
}

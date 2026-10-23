"use client";

import { useMemo, useState } from "react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { useEditor } from "@/editor/use-editor";
import { useElementSelection } from "@/timeline/hooks/element/use-element-selection";
import { getFadePropertyPaths, ApplyFadeCommand, type FadeMode } from "./apply-fade";
import { mediaTimeFromSeconds, TICKS_PER_SECOND } from "@/wasm";
import { useTranslation } from "@/i18n";
import { isTrackLocked } from "@/timeline/track-capabilities";
import { toast } from "sonner";
import { BatchCommand } from "@/commands";
import { resolveCrossDissolve } from "./cross-dissolve";

const FADE_ACTIONS: Array<{ mode: FadeMode; labelKey: "fadeIn" | "fadeOut" | "fadeInOut" }> = [
	{ mode: "in", labelKey: "fadeIn" },
	{ mode: "out", labelKey: "fadeOut" },
	{ mode: "in-out", labelKey: "fadeInOut" },
];

export function TransitionsView() {
	const { t } = useTranslation();
	const editor = useEditor();
	const { selectedElements } = useElementSelection();
	const [durationText, setDurationText] = useState("0.5");
	const durationSeconds = Number(durationText);
	const applicable = useMemo(
		() =>
			editor.timeline.getElementsWithTracks({ elements: selectedElements }).filter(
				({ track, element }) => !isTrackLocked(track) && getFadePropertyPaths({ element }).length > 0,
			),
		[editor, selectedElements],
	);
	const crossDissolve = useMemo(
		() =>
			resolveCrossDissolve({
				selected: editor.timeline.getElementsWithTracks({ elements: selectedElements }),
				tracks: editor.scenes.getActiveSceneOrNull()?.tracks ?? null,
			}),
		[editor, selectedElements],
	);

	const apply = (mode: FadeMode) => {
		if (applicable.length === 0) return;
		const safeDuration = Number.isFinite(durationSeconds)
			? Math.min(10, Math.max(0.05, durationSeconds))
			: 0.5;
		editor.command.execute({
			command: new ApplyFadeCommand(
				applicable.map(({ track, element }) => ({ trackId: track.id, elementId: element.id })),
				mediaTimeFromSeconds({ seconds: safeDuration }),
				mode,
			),
		});
		toast.success(t("timeline.transitionApplied"));
	};

	const applyCrossDissolve = () => {
		if (!crossDissolve) return;
		const requestedDuration = Number.isFinite(durationSeconds)
			? Math.min(10, Math.max(0.05, durationSeconds))
			: 0.5;
		const duration = mediaTimeFromSeconds({
			seconds: Math.min(requestedDuration, crossDissolve.duration / TICKS_PER_SECOND),
		});
		editor.command.execute({
			command: new BatchCommand([
				new ApplyFadeCommand([crossDissolve.target], duration, crossDissolve.mode),
			]),
		});
		toast.success(t("timeline.transitionApplied"));
	};

	return (
		<div className="flex h-full flex-col gap-4 overflow-y-auto p-4">
			<div>
				<h2 className="text-sm font-semibold">{t("timeline.transitions")}</h2>
				<p className="mt-1 text-xs leading-relaxed text-muted-foreground">
					{t("timeline.transitionsDescription")}
				</p>
			</div>

			<label className="flex items-center justify-between gap-3 rounded-md border p-3 text-xs">
				<span>{t("timeline.transitionDuration")}</span>
				<span className="flex items-center gap-2">
					<Input
						type="number"
						min={0.05}
						max={10}
						step={0.05}
					value={durationText}
					onChange={(event) => setDurationText(event.currentTarget.value)}
						className="h-8 w-20 text-right"
						aria-label={t("timeline.transitionDuration")}
					/>
					<span className="text-muted-foreground">{t("timeline.secondsShort")}</span>
				</span>
			</label>

			<div className="grid grid-cols-1 gap-2">
				<Button
					variant="secondary"
					className="h-11 justify-start"
					disabled={!crossDissolve}
					onClick={applyCrossDissolve}
				>
					<span className="mr-2 inline-flex w-8 justify-center text-primary">◧</span>
					{t("timeline.crossDissolve")}
				</Button>
				{FADE_ACTIONS.map(({ mode, labelKey }) => (
					<Button
						key={mode}
						variant="outline"
						className="h-11 justify-start"
						disabled={applicable.length === 0}
						onClick={() => apply(mode)}
					>
						<span className="mr-2 inline-flex w-8 justify-center text-primary">◧</span>
						{t(`timeline.${labelKey}`)}
					</Button>
				))}
			</div>

			<p className="text-xs text-muted-foreground">
				{applicable.length > 0
					? t("timeline.transitionSelectionCount", { count: applicable.length })
					: t("timeline.selectClipsForTransition")}
			</p>
		</div>
	);
}

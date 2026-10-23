"use client";

import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { useEditor } from "@/editor/use-editor";
import { useTranslation, type TranslationKey } from "@/i18n";
import { isTrackLocked } from "@/timeline/track-capabilities";
import type { VisualElement } from "@/timeline";
import { ApplyMotionPresetCommand } from "@/commands/timeline/element/apply-motion-preset";
import type { MotionDirection, MotionEasing, MotionPresetId } from "../presets";
import { mediaTimeFromSeconds } from "@/wasm";
import { toast } from "sonner";

const MOTION_PRESETS: Array<{
	id: MotionPresetId;
	label: TranslationKey;
	symbol: string;
}> = [
	{ id: "fade", label: "properties.motionFade", symbol: "◌" },
	{ id: "zoom", label: "properties.motionZoom", symbol: "⊕" },
	{ id: "left", label: "properties.motionFromLeft", symbol: "←" },
	{ id: "right", label: "properties.motionFromRight", symbol: "→" },
	{ id: "up", label: "properties.motionFromTop", symbol: "↑" },
	{ id: "down", label: "properties.motionFromBottom", symbol: "↓" },
];

export function MotionTab({
	element,
	trackId,
}: {
	element: VisualElement;
	trackId: string;
}) {
	const { t } = useTranslation();
	const editor = useEditor();
	const [direction, setDirection] = useState<MotionDirection>("in");
	const [easing, setEasing] = useState<MotionEasing>("ease-in-out");
	const [durationText, setDurationText] = useState("0.5");
	const track = editor.timeline.getTrackById({ trackId });
	const isLocked = !track || isTrackLocked(track);
	const duration = Number(durationText);

	const apply = (preset: MotionPresetId) => {
		if (!Number.isFinite(duration) || duration <= 0 || isLocked) return;
		const canvas = editor.project.getActive()?.settings.canvasSize;
		const travelDistance = Math.max(80, (canvas?.width ?? 1920) * 0.3);
		editor.command.execute({
			command: new ApplyMotionPresetCommand(
				{ trackId, elementId: element.id },
				preset,
				direction,
				mediaTimeFromSeconds({ seconds: Math.min(duration, 10) }),
				travelDistance,
				easing,
			),
		});
		toast.success(t("properties.motionApplied"));
	};

	return (
		<div className="flex flex-col gap-4 p-3">
			<div>
				<h2 className="text-sm font-semibold">{t("properties.motion")}</h2>
				<p className="mt-1 text-xs leading-relaxed text-muted-foreground">
					{t("properties.motionDescription")}
				</p>
			</div>

			<label className="flex items-center justify-between gap-2 text-xs">
				<span>{t("properties.motionDirection")}</span>
				<select
					className="h-8 rounded-md border bg-background px-2"
					value={direction}
					onChange={(event) =>
						setDirection(event.currentTarget.value as MotionDirection)
					}
				>
					<option value="in">{t("properties.motionIn")}</option>
					<option value="out">{t("properties.motionOut")}</option>
				</select>
			</label>
			<label className="flex items-center justify-between gap-2 text-xs">
				<span>{t("properties.motionDuration")}</span>
				<span className="flex items-center gap-2">
					<Input
						aria-label={t("properties.motionDuration")}
						className="h-8 w-20 text-right"
						type="number"
						min={0.05}
						max={10}
						step={0.05}
						value={durationText}
						onChange={(event) =>
							setDurationText(event.currentTarget.value)
						}
					/>
					<span className="text-muted-foreground">
						{t("properties.seconds")}
					</span>
				</span>
			</label>
			<label className="flex items-center justify-between gap-2 text-xs">
				<span>{t("properties.motionEasing")}</span>
				<select
					className="h-8 rounded-md border bg-background px-2"
					value={easing}
					onChange={(event) =>
						setEasing(event.currentTarget.value as MotionEasing)
					}
				>
					<option value="linear">{t("properties.easingLinear")}</option>
					<option value="ease-in">{t("properties.easingIn")}</option>
					<option value="ease-out">{t("properties.easingOut")}</option>
					<option value="ease-in-out">{t("properties.easingInOut")}</option>
				</select>
			</label>

			<div className="grid grid-cols-2 gap-2">
				{MOTION_PRESETS.map(({ id, label, symbol }) => (
					<Button
						key={id}
						variant="outline"
						className="h-10 justify-start gap-2 px-2"
						disabled={
							isLocked || !Number.isFinite(duration) || duration <= 0
						}
						onClick={() => apply(id)}
					>
						<span className="inline-flex w-5 justify-center text-primary">
							{symbol}
						</span>
						<span className="text-xs">{t(label)}</span>
					</Button>
				))}
			</div>
		</div>
	);
}

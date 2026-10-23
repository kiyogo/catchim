import { Command, type CommandResult } from "@/commands/base-command";
import { EditorCore } from "@/core";
import { resolveAnimationPathValueAtTime } from "@/animation/resolve";
import { upsertPathKeyframe } from "@/animation/keyframes";
import type { AnimationPath } from "@/animation/types";
import type { SceneTracks, TimelineElement } from "@/timeline";
import { findTrackInSceneTracks, updateElementInSceneTracks } from "@/timeline/track-element-update";
import { canElementHaveAudio, isVisualElement } from "@/timeline/element-utils";
import { isTrackLocked } from "@/timeline/track-capabilities";
import { VOLUME_DB_MIN } from "@/timeline/audio-constants";
import type { ElementRef } from "@/timeline/types";
import { mediaTime, type MediaTime } from "@/wasm";
import { coerceParamValue, getParamChannelLayout } from "@/params";
import { getElementParam } from "@/params/registry";

export type FadeMode = "in" | "out" | "in-out";

export function applyFadeToElement({
	element,
	duration,
	mode,
}: {
	element: TimelineElement;
	duration: MediaTime;
	mode: FadeMode;
}): TimelineElement {
	if (element.duration <= 1) return element;
	const fadeLength = mediaTime({
		ticks: Math.max(
			1,
			Math.min(
				duration,
				mode === "in-out"
					? Math.floor(element.duration / 2)
					: element.duration - 1,
			),
		),
	});
	const fadeStart = mediaTime({ ticks: element.duration - fadeLength });
	let animations = element.animations;

	for (const propertyPath of getFadePropertyPaths({ element })) {
		const param = getElementParam({ element, key: propertyPath });
		if (!param) continue;
		const channelLayout = getParamChannelLayout({ param });
		const baseValue = element.params[param.key] ?? param.default;
		if (typeof baseValue !== "number") continue;
		const coerceValue = ({ value }: { value: import("@/params").ParamValue }) =>
			coerceParamValue({ param, value });
		const sample = (time: MediaTime) => {
			const value = resolveAnimationPathValueAtTime({
				animations,
				propertyPath,
				localTime: time,
				fallbackValue: baseValue,
			});
			return typeof value === "number" ? value : baseValue;
		};
		const upsert = (time: MediaTime, value: number) => {
			animations = upsertPathKeyframe({
				animations,
				propertyPath,
				time,
				value,
				interpolation: "linear",
				channelLayout,
				coerceValue,
			});
		};
		const silence = propertyPath === "volume" ? VOLUME_DB_MIN : 0;

		if (mode === "in" || mode === "in-out") {
			const targetAtFadeEnd = sample(fadeLength);
			upsert(mediaTime({ ticks: 0 }), silence);
			upsert(fadeLength, targetAtFadeEnd);
		}
		if (mode === "out" || mode === "in-out") {
			const valueAtFadeStart = sample(fadeStart);
			upsert(fadeStart, valueAtFadeStart);
			upsert(element.duration, silence);
		}
	}

	return { ...element, animations };
}

export class ApplyFadeCommand extends Command {
	private savedState: SceneTracks | null = null;
	private appliedState: SceneTracks | null = null;

	constructor(
		private readonly elements: ElementRef[],
		private readonly duration: MediaTime,
		private readonly mode: FadeMode,
	) {
		super();
	}

	execute(): CommandResult | undefined {
		const editor = EditorCore.getInstance();
		const tracks = editor.scenes.getActiveScene().tracks;
		this.savedState = tracks;
		let next = tracks;

		for (const ref of this.elements) {
			const track = findTrackInSceneTracks({ tracks: next, trackId: ref.trackId });
			const element = track?.elements.find(({ id }) => id === ref.elementId);
			if (!track || !element || isTrackLocked(track) || element.duration <= 1) continue;

			next = updateElementInSceneTracks({
				tracks: next,
				trackId: ref.trackId,
				elementId: ref.elementId,
				update: (current) => applyFadeToElement({
					element: current,
					duration: this.duration,
					mode: this.mode,
				}),
			});
		}

		if (next === tracks) return undefined;
		this.appliedState = next;
		editor.timeline.updateTracks(next);
		return undefined;
	}

	undo(): void {
		if (this.savedState) EditorCore.getInstance().timeline.updateTracks(this.savedState);
	}

	redo(): CommandResult | undefined {
		if (this.appliedState) EditorCore.getInstance().timeline.updateTracks(this.appliedState);
		return undefined;
	}
}

export function getFadePropertyPath({
	element,
}: {
	element: TimelineElement;
}): AnimationPath | null {
	return getFadePropertyPaths({ element })[0] ?? null;
}

export function getFadePropertyPaths({
	element,
}: {
	element: TimelineElement;
}): AnimationPath[] {
	const paths: AnimationPath[] = [];
	if (isVisualElement(element)) paths.push("opacity");
	if (canElementHaveAudio(element)) paths.push("volume");
	return paths;
}

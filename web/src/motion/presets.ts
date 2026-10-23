import {
	resolveAnimationPathValueAtTime,
	updateScalarKeyframeCurve,
	upsertPathKeyframe,
} from "@/animation";
import { getChannelEntriesFromData } from "@/animation/channel-data";
import { isScalarChannel } from "@/animation/interpolation";
import type { AnimationPath, ElementAnimations } from "@/animation/types";
import { coerceParamValue, getParamChannelLayout } from "@/params";
import type { ParamValue } from "@/params";
import { getElementParam } from "@/params/registry";
import type { VisualElement } from "@/timeline";
import { mediaTime, roundMediaTime, type MediaTime } from "@/wasm";

export type MotionPresetId = "fade" | "zoom" | "left" | "right" | "up" | "down";
export type MotionDirection = "in" | "out";
export type MotionEasing = "linear" | "ease-in" | "ease-out" | "ease-in-out";

type ScalarMotion = { path: AnimationPath; start: number; end: number };

export function applyMotionPresetToElement({
	element,
	preset,
	direction,
	duration,
	travelDistance,
	easing,
}: {
	element: VisualElement;
	preset: MotionPresetId;
	direction: MotionDirection;
	duration: MediaTime;
	travelDistance: number;
	easing: MotionEasing;
}): VisualElement {
	if (element.duration <= 1 || duration <= 0) return element;
	const durationTicks = Math.min(element.duration, duration);
	const startTime = direction === "in" ? 0 : element.duration - durationTicks;
	const endTime = startTime + durationTicks;
	let animations = element.animations;
	const motions = buildScalarMotions({
		element,
		preset,
		direction,
		startTime,
		endTime,
		travelDistance,
	});

	for (const motion of motions) {
		const param = getElementParam({ element, key: motion.path });
		if (!param) continue;
		const layout = getParamChannelLayout({ param });
		const coerce = ({ value }: { value: ParamValue }) =>
			coerceParamValue({ param, value });
		animations = upsertPathKeyframe({
			animations,
			propertyPath: motion.path,
			time: mediaTime({ ticks: startTime }),
			value: motion.start,
			interpolation: easing === "linear" ? "linear" : "bezier",
			channelLayout: layout,
			coerceValue: coerce,
		});
		animations = upsertPathKeyframe({
			animations,
			propertyPath: motion.path,
			time: mediaTime({ ticks: endTime }),
			value: motion.end,
			interpolation: easing === "linear" ? "linear" : "bezier",
			channelLayout: layout,
			coerceValue: coerce,
		});
		if (easing !== "linear") {
			animations = applyEasing({
				animations,
				path: motion.path,
				startTime,
				endTime,
				easing,
			});
		}
	}

	return animations === element.animations ? element : { ...element, animations };
}

function buildScalarMotions({
	element,
	preset,
	direction,
	startTime,
	endTime,
	travelDistance,
}: {
	element: VisualElement;
	preset: MotionPresetId;
	direction: MotionDirection;
	startTime: number;
	endTime: number;
	travelDistance: number;
}): ScalarMotion[] {
	const getBase = (path: string, time: number): number | null => {
		const param = getElementParam({ element, key: path });
		if (!param) return null;
		const fallback = element.params[param.key] ?? param.default;
		if (typeof fallback !== "number") return null;
		const value = resolveAnimationPathValueAtTime({
			animations: element.animations,
			propertyPath: path,
			localTime: time,
			fallbackValue: fallback,
		});
		return typeof value === "number" ? value : null;
	};
	const opacityStart = getBase("opacity", startTime);
	const opacityEnd = getBase("opacity", endTime);
	const motions: ScalarMotion[] = [];
	if (opacityStart !== null && opacityEnd !== null) {
		motions.push({
			path: "opacity",
			start: direction === "in" ? 0 : opacityStart,
			end: direction === "in" ? opacityEnd : 0,
		});
	}

	if (preset === "fade") return motions;
	if (preset === "zoom") {
		for (const path of ["transform.scaleX", "transform.scaleY"]) {
			const from = getBase(path, startTime);
			const to = getBase(path, endTime);
			if (from === null || to === null) continue;
			motions.push({
				path,
				start: direction === "in" ? from * 0.75 : from,
				end: direction === "in" ? to : to * 1.25,
			});
		}
		return motions;
	}

	const axis = preset === "left" || preset === "right" ? "transform.positionX" : "transform.positionY";
	const baseStart = getBase(axis, startTime);
	const baseEnd = getBase(axis, endTime);
	if (baseStart === null || baseEnd === null) return motions;
	const sign = preset === "left" || preset === "up" ? -1 : 1;
	const offset = sign * Math.max(1, travelDistance);
	motions.push({
		path: axis,
		start: direction === "in" ? baseStart + offset : baseStart,
		end: direction === "in" ? baseEnd : baseEnd + offset,
	});
	return motions;
}

function applyEasing({
	animations,
	path,
	startTime,
	endTime,
	easing,
}: {
	animations: ElementAnimations | undefined;
	path: AnimationPath;
	startTime: number;
	endTime: number;
	easing: Exclude<MotionEasing, "linear">;
}): ElementAnimations | undefined {
	const entry = getChannelEntriesFromData({ data: animations?.[path] }).find(([, channel]) =>
		channel.keys.some((key) => key.time === startTime) &&
		channel.keys.some((key) => key.time === endTime),
	);
	if (!entry) return animations;
	const [componentKey, channel] = entry;
	if (!isScalarChannel(channel)) return animations;
	const start = channel.keys.find((key) => key.time === startTime);
	const end = channel.keys.find((key) => key.time === endTime);
	if (!start || !end || typeof start.value !== "number" || typeof end.value !== "number") {
		return animations;
	}

	const span = endTime - startTime;
	const startHandle = easing === "ease-out" ? 0 : span * 0.42;
	const endHandle = easing === "ease-in" ? 0 : -span * 0.42;
	let next = updateScalarKeyframeCurve({
		animations,
		propertyPath: path,
		componentKey,
		keyframeId: start.id,
		patch: {
			segmentToNext: "bezier",
			tangentMode: "broken",
			rightHandle: { dt: roundMediaTime({ time: startHandle }), dv: 0 },
		},
	});
	next = updateScalarKeyframeCurve({
		animations: next,
		propertyPath: path,
		componentKey,
		keyframeId: end.id,
		patch: {
			leftHandle: { dt: roundMediaTime({ time: endHandle }), dv: 0 },
			tangentMode: "broken",
		},
	});
	return next;
}

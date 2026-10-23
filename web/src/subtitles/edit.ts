import { splitAnimationsAtTime } from "@/animation";
import type { ParamValues } from "@/params";
import type { TextElement, TextTrack } from "@/timeline/types";
import { isTrackLocked } from "@/timeline/track-capabilities";
import { generateUUID } from "@/utils/id";
import { addMediaTime, type MediaTime } from "@/wasm";
import {
	mergeCaptionRanges,
	offsetCaptionRanges,
	splitCaptionRange,
	validateCaptionRange,
} from "./timing";
import type { CaptionMessage } from "./messages";

export type CaptionStylePreset = "classic" | "yellow" | "box" | "minimal";
export const CAPTION_STYLE_PRESETS: Record<CaptionStylePreset, ParamValues> = {
	classic: {
		color: "#ffffff",
		fontSize: 5,
		fontWeight: "bold",
		fontStyle: "normal",
		textDecoration: "none",
		textAlign: "center",
		"background.enabled": false,
	},
	yellow: {
		color: "#ffe600",
		fontSize: 5,
		fontWeight: "bold",
		fontStyle: "normal",
		textDecoration: "none",
		textAlign: "center",
		"background.enabled": false,
	},
	box: {
		color: "#ffffff",
		fontSize: 5,
		fontWeight: "bold",
		fontStyle: "normal",
		textDecoration: "none",
		textAlign: "center",
		"background.enabled": true,
		"background.color": "#000000cc",
		"background.paddingX": 12,
		"background.paddingY": 6,
		"background.cornerRadius": 6,
	},
	minimal: {
		color: "#ffffff",
		fontSize: 4,
		fontWeight: "normal",
		fontStyle: "normal",
		textDecoration: "none",
		textAlign: "center",
		"background.enabled": false,
	},
};

export type CaptionEditAction =
	| {
			type: "update";
			elementId: string;
			content: string;
			startTime: MediaTime;
			endTime: MediaTime;
	  }
	| { type: "split"; elementId: string; time: MediaTime; textOffset: number }
	| { type: "merge"; elementId: string }
	| { type: "offset"; offset: MediaTime }
	| { type: "replace"; find: string; replacement: string; matchCase: boolean }
	| { type: "style"; preset: CaptionStylePreset };

export class CaptionEditError extends Error {
	constructor(readonly code: CaptionMessage) {
		super(code);
	}
}

export interface CaptionTrackEdit {
	track: TextTrack;
	selectedIds?: string[];
}

export function orderedCaptionCues(track: TextTrack): TextElement[] {
	return track.elements
		.map((element, index) => ({ element, index }))
		.sort(
			(a, b) => a.element.startTime - b.element.startTime || a.index - b.index,
		)
		.map(({ element }) => element);
}

function updateParams(element: TextElement, patch: ParamValues): TextElement {
	if (
		Object.entries(patch).every(([key, value]) => element.params[key] === value)
	)
		return element;
	return { ...element, params: { ...element.params, ...patch } };
}

/** Operates on the existing text track only; never maintains a parallel transcript model. */
export function planCaptionTrackEdit({
	track,
	action,
}: {
	track: TextTrack;
	action: CaptionEditAction;
}): CaptionTrackEdit | null {
	if (isTrackLocked(track)) return null;
	let elements = track.elements;
	let selectedIds: string[] | undefined;
	if (action.type === "offset") {
		if (action.offset === 0 || elements.length === 0) return null;
		const ranges = offsetCaptionRanges({
			ranges: elements,
			offset: action.offset,
		});
		if (!ranges) throw new CaptionEditError("invalidOffset");
		elements = elements.map((element, index) => ({
			...element,
			startTime: ranges[index].startTime,
		}));
	} else if (action.type === "replace") {
		if (!action.find) return null;
		const escaped = action.find.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
		const expression = new RegExp(escaped, action.matchCase ? "gu" : "giu");
		elements = elements.map((element) => {
			const current = String(element.params.content ?? "");
			const content = current.replace(expression, () => action.replacement);
			if (content === current) return element;
			if (!content.trim()) throw new CaptionEditError("emptyText");
			return updateParams(element, { content });
		});
	} else if (action.type === "style") {
		elements = elements.map((element) =>
			updateParams(element, CAPTION_STYLE_PRESETS[action.preset]),
		);
	} else {
		const index = elements.findIndex(({ id }) => id === action.elementId);
		if (index < 0) throw new CaptionEditError("stale");
		const element = elements[index];
		if (action.type === "update") {
			if (!action.content.trim()) throw new CaptionEditError("emptyText");
			const range = validateCaptionRange(action);
			if (!range) throw new CaptionEditError("invalidTiming");
			let next = updateParams(element, {
				content: action.content.replace(/\r\n?/g, "\n"),
			});
			if (
				range.startTime !== element.startTime ||
				range.duration !== element.duration
			) {
				// The standard update pipeline owns animation clamping after duration edits.
				next = { ...next, ...range };
			}
			elements = elements.map((current) =>
				current === element ? next : current,
			);
		} else if (action.type === "split") {
			const range = splitCaptionRange({ ...element, time: action.time });
			const content = String(element.params.content ?? "");
			const offset = action.textOffset;
			const leftText = content.slice(0, offset).trimEnd();
			const rightText = content.slice(offset).trimStart();
			const splitsSurrogate =
				offset > 0 &&
				/[\uD800-\uDBFF]/.test(content[offset - 1]) &&
				/[\uDC00-\uDFFF]/.test(content[offset] ?? "");
			if (
				!range ||
				!Number.isSafeInteger(offset) ||
				offset <= 0 ||
				offset >= content.length ||
				!leftText.trim() ||
				!rightText.trim() ||
				splitsSurrogate
			)
				throw new CaptionEditError("invalidSplit");
			const { leftAnimations, rightAnimations } = splitAnimationsAtTime({
				animations: element.animations,
				splitTime: range.leftDuration,
				shouldIncludeSplitBoundary: true,
			});
			const rightId = generateUUID();
			const left = {
				...element,
				duration: range.leftDuration,
				trimEnd: addMediaTime({ a: element.trimEnd, b: range.rightDuration }),
				animations: leftAnimations,
				params: { ...element.params, content: leftText },
			};
			const right = {
				...element,
				id: rightId,
				startTime: action.time,
				duration: range.rightDuration,
				trimStart: addMediaTime({
					a: element.trimStart,
					b: range.leftDuration,
				}),
				animations: rightAnimations,
				params: { ...element.params, content: rightText },
			};
			elements = elements.flatMap((current) =>
				current === element ? [left, right] : [current],
			);
			selectedIds = [rightId];
		} else {
			const ordered = orderedCaptionCues(track);
			const next =
				ordered[ordered.findIndex(({ id }) => id === element.id) + 1];
			if (!next) throw new CaptionEditError("mergeUnavailable");
			const range = mergeCaptionRanges({ first: element, second: next });
			if (!range) throw new CaptionEditError("invalidTiming");
			const content = `${String(element.params.content ?? "").trimEnd()}\n${String(next.params.content ?? "").trimStart()}`;
			const merged = {
				...element,
				...range,
				params: { ...element.params, content },
			};
			elements = elements
				.filter((current) => current !== next)
				.map((current) => (current === element ? merged : current));
			selectedIds = [element.id];
		}
	}
	if (
		elements.length === track.elements.length &&
		elements.every((element, index) => element === track.elements[index])
	)
		return null;
	return { track: { ...track, elements }, selectedIds };
}

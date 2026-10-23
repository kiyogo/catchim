import { parseSrt } from "./srt";
import type { ParseSubtitleResult } from "./types";

/** Browser file-format adapter: cue times reuse the existing Rust-backed SRT parser. */
export function parseVtt({ input }: { input: string }): ParseSubtitleResult {
	const blocks = input.replace(/^\uFEFF/, "").replace(/\r\n?/g, "\n").trim().split(/\n{2,}/);
	if (!/^WEBVTT(?:[ \t].*)?(?:\n|$)/.test(blocks[0])) throw new Error("Invalid WebVTT header");
	const result: ParseSubtitleResult = { captions: [], skippedCueCount: 0, warnings: [] };
	let discardedStyling = false;
	for (const block of blocks.slice(1)) {
		if (/^NOTE(?:[ \t\n]|$)/.test(block)) continue;
		if (/^(?:STYLE|REGION)(?:\n|$)/.test(block)) { discardedStyling = true; continue; }
		const lines = block.split("\n");
		const timingIndex = lines[0].includes("-->") ? 0 : 1;
		const timing = lines[timingIndex]?.match(/^((?:\d{2,}:)?\d{2}:\d{2}\.\d{3})[ \t]+-->[ \t]+((?:\d{2,}:)?\d{2}:\d{2}\.\d{3})([ \t].*)?$/);
		if (!timing) { result.skippedCueCount++; continue; }
		if (timing[3]?.trim()) discardedStyling = true;
		const stamp = (value: string) => (value.split(":").length === 2 ? `00:${value}` : value).replace(".", ",");
		const text = lines.slice(timingIndex + 1).join("\n");
		const parsed = parseSrt({ input: `${stamp(timing[1])} --> ${stamp(timing[2])}\n${text}` });
		result.captions.push(...parsed.captions);
		result.skippedCueCount += parsed.skippedCueCount;
	}
	if (discardedStyling) result.warnings.push("WebVTT styles and cue positioning were imported as plain text.");
	return result;
}

import type { TimelineTrack } from "@/timeline";
import { getNativeTimelineBindings } from "@/native/opencut-core";
import { TICKS_PER_SECOND } from "@/wasm";

export type SubtitleExportFormat = "srt" | "vtt";

function timestamp(ticks: number, format: SubtitleExportFormat): string {
	const bindings = getNativeTimelineBindings();
	let value = "";
	if (bindings) {
		let pointer = 0;
		try {
			pointer = bindings.malloc(32);
			if (pointer && bindings.formatSubtitleTimestamp(ticks, pointer, 32) === 1) {
				const bytes = bindings.HEAPU8.subarray(pointer, pointer + 32);
				value = new TextDecoder().decode(bytes.subarray(0, bytes.indexOf(0)));
			}
		} finally { if (pointer) bindings.free(pointer); }
	}
	if (!/^\d{2,}:\d{2}:\d{2},\d{3}$/.test(value)) {
		const milliseconds = Math.round(ticks * 1000 / TICKS_PER_SECOND);
		const pad = (number: number, size = 2) => String(number).padStart(size, "0");
		value = `${pad(Math.floor(milliseconds / 3_600_000))}:${pad(Math.floor(milliseconds / 60_000) % 60)}:${pad(Math.floor(milliseconds / 1000) % 60)},${pad(milliseconds % 1000, 3)}`;
	}
	return format === "vtt" ? value.replace(",", ".") : value;
}

/** Serialize exactly one user-selected text track, including edited timing/content. */
export function serializeSubtitleTrack({ track, format }: { track: TimelineTrack; format: SubtitleExportFormat }): string {
	if (track.type !== "text") throw new Error("Select a text track to export subtitles");
	const cues = track.elements.map((element, index) => ({ element, index }))
		.sort((left, right) => left.element.startTime - right.element.startTime || left.index - right.index)
		.map(({ element }) => {
			if (!Number.isSafeInteger(element.startTime) || element.startTime < 0 || !Number.isSafeInteger(element.duration) || element.duration <= 0 || !Number.isSafeInteger(element.startTime + element.duration)) {
				throw new Error("Subtitle track contains invalid timing");
			}
			const content = String(element.params.content ?? "").replace(/\r\n?/g, "\n").replace(/\n{2,}/g, "\n").trim();
			if (!content) return null;
			const text = content.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
			// Sidecars have millisecond precision; retain even sub-millisecond cues.
			const startMs = Math.round(element.startTime * 1000 / TICKS_PER_SECOND);
			const endMs = Math.max(startMs + 1, Math.round((element.startTime + element.duration) * 1000 / TICKS_PER_SECOND));
			return { text, start: startMs * (TICKS_PER_SECOND / 1000), end: endMs * (TICKS_PER_SECOND / 1000) };
		}).filter((cue) => cue !== null);
	if (cues.length === 0) throw new Error("Subtitle track has no text to export");
	const body = cues.map((cue, index) => `${index + 1}\n${timestamp(cue.start, format)} --> ${timestamp(cue.end, format)}\n${cue.text}`).join("\n\n");
	return `${format === "vtt" ? "WEBVTT\n\n" : ""}${body}\n`;
}

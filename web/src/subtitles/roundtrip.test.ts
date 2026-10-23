import { describe, expect, it, vi } from "vitest";
import type { NativeTimelineBindings } from "@/native/opencut-core";
import { initRustEngine } from "@/native/rust-engine";
import { mediaTime } from "@/wasm";
import type { TextTrack, TextElement } from "@/timeline";
import { parseSubtitleFile } from "./parse";
import { serializeSubtitleTrack } from "./export";

const state = vi.hoisted(() => ({ native: null as NativeTimelineBindings | null }));
vi.mock("@/native/opencut-core", () => ({ getNativeTimelineBindings: () => state.native }));

function track(): TextTrack {
	const cue = (id: string, ticks: number, duration: number, content: string) => ({
		id, type: "text", name: id, startTime: mediaTime({ ticks }), duration: mediaTime({ ticks: duration }),
		trimStart: mediaTime({ ticks: 0 }), trimEnd: mediaTime({ ticks: 0 }), params: { content },
	} as TextElement);
	return { id: "captions", type: "text", name: "Phụ đề", hidden: false, elements: [
		cue("later", 60 * 120_000 - 1, 12_001, "Dòng 2\nTiếng Việt 🎬"),
		cue("first", 60_000, 120_000, "A --> B & <title> &lt;"),
	] };
}

describe("subtitle sidecars", () => {
	it("imports WebVTT identifiers, shorthand timestamps, markup and skips metadata blocks", () => {
		const result = parseSubtitleFile({ fileName: "captions.VTT", input: "\uFEFFWEBVTT\r\n\r\nNOTE a comment\r\nignore this\r\n\r\nSTYLE\r\n::cue { color: lime }\r\n\r\nvoice-1\r\n00:01.250 --> 00:03.500 align:start\r\n<v Alice><b>Xin chào</b> &amp; bạn\r\nDòng 2\r\n\r\n00:04.000 --> 00:03.000\r\ninvalid" });
		expect(result.captions).toEqual([{ text: "Xin chào & bạn\nDòng 2", startTime: 1.25, duration: 2.25 }]);
		expect(result.skippedCueCount).toBe(1);
		expect(result.warnings).toHaveLength(1);
		expect(() => parseSubtitleFile({ fileName: "bad.vtt", input: "00:01.000 --> 00:02.000\nmissing header" })).toThrow("header");
	});

	it("exports edited track timing/content in chronological order and roundtrips both formats", async () => {
		const current = track();
		const original = structuredClone(current);
		state.native = null;
		const fallback = (["srt", "vtt"] as const).map((format) => serializeSubtitleTrack({ track: current, format }));
		state.native = await initRustEngine();
		expect(state.native).not.toBeNull();
		for (const [index, format] of (["srt", "vtt"] as const).entries()) {
			const output = serializeSubtitleTrack({ track: current, format });
			expect(output).toBe(fallback[index]);
			expect(output).toContain(format === "srt" ? "00:01:00,000" : "00:01:00.000");
			const reopened = parseSubtitleFile({ fileName: `edited.${format}`, input: output });
			expect(reopened.skippedCueCount).toBe(0);
			expect(reopened.captions[0]).toEqual({ text: "A --> B & <title> &lt;", startTime: 0.5, duration: 1 });
			expect(reopened.captions[1].text).toBe("Dòng 2\nTiếng Việt 🎬");
			expect(reopened.captions[1].startTime).toBe(60);
		}
		expect(current).toEqual(original);
		expect(state.native!.parseSubtitleTimestamp("00:61:00.000")).toBe(-1);
		expect(state.native!.parseSubtitleTimestamp("invalid")).toBe(-1);
		expect(state.native!.formatSubtitleTimestamp(1, 0, 0)).toBe(0);
	});

	it("preserves sub-millisecond cues and rejects invalid or empty tracks", () => {
		const current = track();
		current.elements = [{ ...current.elements[0], startTime: mediaTime({ ticks: 1 }), duration: mediaTime({ ticks: 1 }) }];
		expect(serializeSubtitleTrack({ track: current, format: "vtt" })).toContain("00:00:00.000 --> 00:00:00.001");
		current.elements[0].duration = mediaTime({ ticks: 0 });
		expect(() => serializeSubtitleTrack({ track: current, format: "srt" })).toThrow("timing");
		current.elements = [];
		expect(() => serializeSubtitleTrack({ track: current, format: "srt" })).toThrow("no text");
	});
});

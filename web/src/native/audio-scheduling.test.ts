import { describe, expect, it } from "vitest";
import {
	planAudioBufferSchedule,
	type AudioBufferScheduleRequest,
} from "@/media/audio-scheduling";
import { getRustAudioSchedulingBindings, initRustEngine } from "./rust-engine";

describe("Rust WASM audio scheduling", () => {
	it("matches the compatibility path for split, trim, seek, rate and invalid windows", async () => {
		const base: AudioBufferScheduleRequest = {
			clipStart: 10,
			clipDuration: 2,
			sourceStart: 4,
			playbackRate: 1,
			bufferStart: 4,
			bufferDuration: 10,
			requestedStart: 10,
			contextOffset: 90,
			contextNow: 99,
		};
		const cases: AudioBufferScheduleRequest[] = [
			base,
			{ ...base, playbackRate: 0.5, contextNow: 100.5 },
			{ ...base, playbackRate: 2, contextNow: 100.5 },
			{ ...base, bufferStart: 3, requestedStart: 11 },
			{ ...base, bufferStart: -0.02, sourceStart: 0 },
			{ ...base, bufferStart: 6 },
			{ ...base, requestedStart: 12 },
			{ ...base, bufferDuration: 0 },
			{ ...base, clipDuration: 0 },
			{ ...base, contextNow: 102 },
			{ ...base, playbackRate: Number.NaN },
			{ ...base, bufferDuration: Number.POSITIVE_INFINITY },
			{ ...base, clipDuration: -1 },
			{ ...base, clipStart: Number.MAX_VALUE, clipDuration: Number.MAX_VALUE },
		];
		expect(getRustAudioSchedulingBindings()).toBeNull();
		const fallback = cases.map(planAudioBufferSchedule);
		await initRustEngine();
		const native = getRustAudioSchedulingBindings();
		expect(native?.planBufferSchedule).toBeTypeOf("function");
		if (!native) throw new Error("Rust audio scheduling C ABI was not loaded");

		for (let index = 0; index < cases.length; index++) {
			const actual = native.planBufferSchedule(cases[index]);
			const expected = fallback[index];
			expect(actual?.status).toBe(expected.status);
			if (actual?.status === "ready" && expected.status === "ready") {
				for (const key of ["contextStart", "sourceOffset", "sourceDuration", "timelineStart"] as const) {
					expect(actual.window[key]).toBeCloseTo(expected.window[key], 12);
				}
			}
			expect(planAudioBufferSchedule(cases[index])).toEqual(actual);
		}
	});
});

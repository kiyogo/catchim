import { beforeEach, describe, expect, it, vi } from "vitest";
import {
	planAudioBufferSchedule,
	type AudioBufferScheduleRequest,
} from "./audio-scheduling";
import { getRustAudioSchedulingBindings } from "@/native/rust-engine";

vi.mock("@/native/rust-engine", () => ({
	getRustAudioSchedulingBindings: vi.fn(() => null),
}));

const request: AudioBufferScheduleRequest = {
	clipStart: 10,
	clipDuration: 2,
	sourceStart: 4,
	playbackRate: 1,
	bufferStart: 4,
	bufferDuration: 3,
	requestedStart: 10,
	contextOffset: 90,
	contextNow: 99,
};

describe("audio scheduling compatibility path", () => {
	beforeEach(() => {
		vi.mocked(getRustAudioSchedulingBindings).mockReturnValue(null);
	});

	it("stops the decoded buffer at the split boundary before the following gap", () => {
		expect(planAudioBufferSchedule(request)).toEqual({
			status: "ready",
			window: { contextStart: 100, sourceOffset: 0, sourceDuration: 2, timelineStart: 10 },
		});
	});

	it.each([0.5, 2])("uses source seconds for a late %sx buffer", (rate) => {
		expect(planAudioBufferSchedule({
			...request,
			playbackRate: rate,
			bufferDuration: 10,
			contextNow: 100.5,
		})).toEqual({
			status: "ready",
			window: {
				contextStart: 100.5,
				sourceOffset: 0.5 * rate,
				sourceDuration: 1.5 * rate,
				timelineStart: 10.5,
			},
		});
	});

	it("starts inside the decoded buffer when seeking across a source trim", () => {
		expect(planAudioBufferSchedule({
			...request, bufferStart: 3, bufferDuration: 4, requestedStart: 11,
		})).toEqual({
			status: "ready",
			window: { contextStart: 101, sourceOffset: 2, sourceDuration: 1, timelineStart: 11 },
		});
	});

	it.each([
		{ bufferStart: 6 },
		{ bufferStart: 1 },
		{ contextNow: 102 },
		{ requestedStart: 12 },
		{ bufferDuration: 0 },
		{ clipDuration: 0 },
	])("does not schedule an empty intersection: %j", (patch) => {
		expect(planAudioBufferSchedule({ ...request, ...patch })).toEqual({ status: "empty" });
	});

	it.each([
		{ playbackRate: 0 },
		{ playbackRate: Number.NaN },
		{ clipDuration: -1 },
		{ bufferDuration: Number.POSITIVE_INFINITY },
		{ clipStart: Number.MAX_VALUE, clipDuration: Number.MAX_VALUE },
	])("rejects invalid input or overflow: %j", (patch) => {
		expect(planAudioBufferSchedule({ ...request, ...patch })).toEqual({ status: "invalid" });
	});

	it("uses the compatibility calculation after a failed WASM call", () => {
		const nativePlan = vi.fn(() => null);
		vi.mocked(getRustAudioSchedulingBindings).mockReturnValue({ planBufferSchedule: nativePlan });
		expect(planAudioBufferSchedule(request).status).toBe("ready");
		expect(nativePlan).toHaveBeenCalledWith(request);
	});

	it("preserves an explicit empty result from the native engine", () => {
		vi.mocked(getRustAudioSchedulingBindings).mockReturnValue({
			planBufferSchedule: () => ({ status: "empty" }),
		});
		expect(planAudioBufferSchedule(request)).toEqual({ status: "empty" });
	});
});

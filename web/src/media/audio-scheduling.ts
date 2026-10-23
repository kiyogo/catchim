import { getRustAudioSchedulingBindings } from "@/native/rust-engine";

/** All times are seconds; source times are independent of the playback rate. */
export interface AudioBufferScheduleRequest {
	clipStart: number;
	clipDuration: number;
	sourceStart: number;
	playbackRate: number;
	bufferStart: number;
	bufferDuration: number;
	requestedStart: number;
	/** Add to a timeline time to obtain its AudioContext time. */
	contextOffset: number;
	contextNow: number;
}

export interface AudioBufferScheduleWindow {
	contextStart: number;
	sourceOffset: number;
	sourceDuration: number;
	timelineStart: number;
}

export type AudioBufferScheduleResult =
	| { status: "ready"; window: AudioBufferScheduleWindow }
	| { status: "empty" }
	| { status: "invalid" };

export function planAudioBufferSchedule(
	request: AudioBufferScheduleRequest,
): AudioBufferScheduleResult {
	const rust = getRustAudioSchedulingBindings();
	if (rust) {
		const result = rust.planBufferSchedule(request);
		if (result) return result;
	}
	// Compatibility path for unavailable or older Rust WASM artifacts. Keep the
	// interval and source-second semantics equivalent to audio.rs.
	const {
		clipStart,
		clipDuration,
		sourceStart,
		playbackRate,
		bufferStart,
		bufferDuration,
		requestedStart,
		contextOffset,
		contextNow,
	} = request;
	if (
		!Object.values(request).every(Number.isFinite) ||
		clipStart < 0 ||
		clipDuration < 0 ||
		sourceStart < 0 ||
		playbackRate <= 0 ||
		bufferDuration < 0 ||
		requestedStart < 0 ||
		contextNow < 0
	) {
		return { status: "invalid" };
	}
	if (clipDuration === 0 || bufferDuration === 0) return { status: "empty" };

	const clipEnd = clipStart + clipDuration;
	const bufferTimelineStart = clipStart + (bufferStart - sourceStart) / playbackRate;
	const bufferTimelineEnd = bufferTimelineStart + bufferDuration / playbackRate;
	const currentTimelineTime = contextNow - contextOffset;
	if (
		![clipEnd, bufferTimelineStart, bufferTimelineEnd, currentTimelineTime].every(
			Number.isFinite,
		)
	) {
		return { status: "invalid" };
	}

	const timelineStart = Math.max(
		clipStart,
		bufferTimelineStart,
		requestedStart,
		currentTimelineTime,
	);
	const timelineEnd = Math.min(clipEnd, bufferTimelineEnd);
	if (timelineStart >= timelineEnd) return { status: "empty" };

	const contextStart = Math.max(timelineStart + contextOffset, contextNow);
	const sourceOffset = Math.min(
		bufferDuration,
		Math.max(0, (timelineStart - bufferTimelineStart) * playbackRate),
	);
	const sourceDuration = Math.min(
		(timelineEnd - timelineStart) * playbackRate,
		bufferDuration - sourceOffset,
	);
	if (![contextStart, sourceOffset, sourceDuration].every(Number.isFinite)) {
		return { status: "invalid" };
	}
	if (sourceDuration <= 0) return { status: "empty" };

	return {
		status: "ready",
		window: { contextStart, sourceOffset, sourceDuration, timelineStart },
	};
}

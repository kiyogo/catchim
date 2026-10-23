/**
 * OpenCut Rust WebAssembly Native Engine
 * Direct zero-copy bridge to opencut_engine.wasm
 */

import type { NativeTimelineBindings } from "./opencut-core";
import { initializeExportRangeBindings } from "./export-range";
import { initializeAudioExportBindings } from "./audio-export";
import { initializeCaptionTimingBindings } from "./caption-timing";
import type {
	AudioBufferScheduleRequest,
	AudioBufferScheduleResult,
} from "@/media/audio-scheduling";

let rustBindings: NativeTimelineBindings | null = null;
let rustLoadPromise: Promise<NativeTimelineBindings | null> | null = null;
let rustSelectionBindings: RustTimelineSelectionBindings | null = null;
let rustAudioBindings: RustAudioBindings | null = null;
let rustAudioSchedulingBindings: RustAudioSchedulingBindings | null = null;
let rustExportTimingBindings: RustExportTimingBindings | null = null;
let rustTrackGapBindings: RustTrackGapBindings | null = null;

export interface ExportFrameSample {
	timeTicks: number;
	timestampSeconds: number;
	durationSeconds: number;
}

export interface RustExportTimingBindings {
	frameCount: (duration: number, numerator: number, denominator: number) => number | null;
	frameSample: (duration: number, numerator: number, denominator: number, index: number) => ExportFrameSample | null;
}

export interface RustTrackGapBindings {
	planTrackGapStarts: (starts: Float64Array, durations: Float64Array) => number[] | null;
}

export interface RustAudioSchedulingBindings {
	planBufferSchedule: (
		request: AudioBufferScheduleRequest,
	) => AudioBufferScheduleResult | null;
}

export interface RustTimelineSelectionBindings {
	selectRectIntersections: (
		rectangles: Float64Array,
		selection: Float64Array,
	) => number[] | null;
}

export interface RustAudioBindings {
	samplePeakBuckets: (args: {
		amplitudes: Float32Array;
		bucketSize: number;
		starts: Float64Array;
		ends: Float64Array;
	}) => Float32Array | null;
}

export async function initRustEngine(): Promise<NativeTimelineBindings | null> {
	if (rustBindings) {
		return rustBindings;
	}

	if (rustLoadPromise) {
		return rustLoadPromise;
	}

	rustLoadPromise = (async () => {
		try {
			let wasmBytes: ArrayBuffer | null = null;

			if (typeof window !== "undefined") {
				const response = await fetch("/wasm/opencut_engine.wasm");
				if (!response.ok) {
					throw new Error(`Failed to fetch /wasm/opencut_engine.wasm: ${response.status}`);
				}
				wasmBytes = await response.arrayBuffer();
			} else {
				// Node.js environment
				try {
					const fs = await import("fs");
					const path = await import("path");
					const wasmPath = path.resolve(process.cwd(), "public/wasm/opencut_engine.wasm");
					wasmBytes = fs.readFileSync(wasmPath).buffer;
				} catch (e) {
					console.warn("[RustEngine] Failed to load local wasm file in node:", e);
					return null;
				}
			}

			if (!wasmBytes) {
				return null;
			}

			const { instance } = await WebAssembly.instantiate(wasmBytes, {});
			const exp = instance.exports as any;
			const memory = exp.memory as WebAssembly.Memory;

			rustBindings = {
				create: () => exp.oc_timeline_create(),
				destroy: (timeline: number) => exp.oc_timeline_destroy(timeline),
				addTrack: (timeline: number, trackId: string) => {
					const trackIdPtr = stringToCStr(exp, trackId);
					try {
						return exp.oc_timeline_add_track(timeline, trackIdPtr);
					} finally {
						exp.oc_free(trackIdPtr);
					}
				},
				insertClip: (
					timeline: number,
					trackId: string,
					clipId: string,
					start: number,
					duration: number,
				) => {
					const trackIdPtr = stringToCStr(exp, trackId);
					const clipIdPtr = stringToCStr(exp, clipId);
					try {
						return exp.ocw_timeline_insert_clip(timeline, trackIdPtr, clipIdPtr, start, duration);
					} finally {
						exp.oc_free(trackIdPtr);
						exp.oc_free(clipIdPtr);
					}
				},
				canPlace: (
					timeline: number,
					trackId: string,
					start: number,
					duration: number,
					excludeClipId: string,
				) => {
					const trackIdPtr = stringToCStr(exp, trackId);
					const excludeClipIdPtr = stringToCStr(exp, excludeClipId);
					try {
						return exp.ocw_timeline_can_place(timeline, trackIdPtr, start, duration, excludeClipIdPtr);
					} finally {
						exp.oc_free(trackIdPtr);
						exp.oc_free(excludeClipIdPtr);
					}
				},
				snap: (
					_timeline: number,
					_targetTime: number,
					_playhead: number,
					_threshold: number,
					_outSnappedPtr: number,
					_outDeltaPtr: number,
				) => 0,
				ticksPerSecond: () => exp.oc_time_ticks_per_second(),
				fromSeconds: (seconds: number) => exp.ocw_time_from_seconds(seconds),
				toSeconds: (ticks: number) => exp.ocw_time_to_seconds(ticks),
				roundToFrame: (ticks: number, fpsNum: number, fpsDen: number) =>
					exp.ocw_time_round_to_frame(ticks, fpsNum, fpsDen),
				floorToFrame: (ticks: number, fpsNum: number, fpsDen: number) =>
					exp.ocw_time_floor_to_frame(ticks, fpsNum, fpsDen),
				lastFrame: (duration: number, fpsNum: number, fpsDen: number) =>
					exp.ocw_time_last_frame(duration, fpsNum, fpsDen),
				snappedSeek: (time: number, duration: number, fpsNum: number, fpsDen: number) =>
					exp.ocw_time_snapped_seek(time, duration, fpsNum, fpsDen),
				formatTimecode: (ticks: number, format: number, fpsNum: number, fpsDen: number, outBuf: number, outLen: number) =>
					exp.ocw_time_format_timecode(ticks, format, fpsNum, fpsDen, outBuf, outLen),
				parseTimecode: (str: string, format: number, fpsNum: number, fpsDen: number) => {
					const strPtr = stringToCStr(exp, str);
					try {
						return exp.ocw_time_parse_timecode(strPtr, format, fpsNum, fpsDen);
					} finally {
						exp.oc_free(strPtr);
					}
				},
				evaluateFade: (offset: number, duration: number, fadeIn: number, fadeOut: number) =>
					exp.ocw_audio_evaluate_fade(offset, duration, fadeIn, fadeOut),
				evaluateMaskAlpha: (px: number, py: number, maskType: number, cx: number, cy: number, sx: number, sy: number, rot: number, feather: number, inverted: number) =>
					exp.ocw_mask_evaluate_alpha(px, py, maskType, cx, cy, sx, sy, rot, feather, inverted),
				solveBezier: (time: number, t0: number, t1: number, t2: number, t3: number) =>
					exp.ocw_animation_solve_bezier(time, t0, t1, t2, t3),
				evaluateBezierPoint: (progress: number, p0: number, p1: number, p2: number, p3: number) =>
					exp.ocw_animation_evaluate_bezier_point(progress, p0, p1, p2, p3),
				evaluateChannel: (keyTimesPtr: number, keyValuesPtr: number, keyInterpPtr: number, numKeys: number, evalTime: number) =>
					exp.ocw_animation_evaluate_channel(keyTimesPtr, keyValuesPtr, keyInterpPtr, numKeys, evalTime),
				mapTimelineToSourceSpeed: (offset: number, duration: number, ratiosPtr: number, multsPtr: number, numPoints: number, constantSpeed: number) =>
					exp.ocw_speed_map_timeline_to_source(offset, duration, ratiosPtr, multsPtr, numPoints, constantSpeed),
				findAvailableGap: (timeline: number, trackId: string, duration: number, minStart: number) => {
					const trackIdPtr = stringToCStr(exp, trackId);
					try {
						return exp.ocw_timeline_find_available_gap(timeline, trackIdPtr, duration, minStart);
					} finally {
						exp.oc_free(trackIdPtr);
					}
				},
				applyRipple: (timeline: number, trackId: string, afterTime: number, deltaTicks: number) => {
					const trackIdPtr = stringToCStr(exp, trackId);
					try {
						return exp.ocw_timeline_apply_ripple(timeline, trackIdPtr, afterTime, deltaTicks);
					} finally {
						exp.oc_free(trackIdPtr);
					}
				},
				sceneCreate: () => exp.oc_scene_create(),
				sceneDestroy: (scene: number) => exp.oc_scene_destroy(scene),
				sceneAddItem: (scene: number, id: string, type: number, zIndex: number, opacity: number, blendMode: number, cx: number, cy: number, w: number, h: number, rot: number, flipX: number, flipY: number, assetId: string) => {
					const idPtr = stringToCStr(exp, id);
					const assetIdPtr = stringToCStr(exp, assetId);
					try {
						exp.ocw_scene_add_item(scene, idPtr, type, zIndex, opacity, blendMode, cx, cy, w, h, rot, flipX, flipY, assetIdPtr);
					} finally {
						exp.oc_free(idPtr);
						exp.oc_free(assetIdPtr);
					}
				},
				sceneBuildDisplayList: (scene: number, viewportW: number, viewportH: number, enableCulling: number) =>
					exp.ocw_scene_build_display_list(scene, viewportW, viewportH, enableCulling),
				parseSubtitleTimestamp: (ts: string) => {
					const tsPtr = stringToCStr(exp, ts);
					try {
						return exp.ocw_subtitles_parse_timestamp(tsPtr);
					} finally {
						exp.oc_free(tsPtr);
					}
				},
				formatSubtitleTimestamp: (ticks: number, outBuf: number, outLen: number) =>
					exp.ocw_subtitles_format_timestamp(ticks, outBuf, outLen),
				measureLineHeight: (fontSize: number, lineHeightRatio: number) =>
					exp.ocw_text_measure_line_height(fontSize, lineHeightRatio),
				breakLinesCount: (text: string, maxWidth: number, avgCharWidth: number) => {
					const textPtr = stringToCStr(exp, text);
					try {
						return exp.ocw_text_break_lines_count(textPtr, maxWidth, avgCharWidth);
					} finally {
						exp.oc_free(textPtr);
					}
				},
				pointInRotatedRect: (px: number, py: number, cx: number, cy: number, w: number, h: number, rot: number) =>
					exp.ocw_geometry_point_in_rotated_rect(px, py, cx, cy, w, h, rot),
				testSnapAxis: (sourceVal: number, targetVal: number, threshold: number, outSnapped: number, outDelta: number) =>
					exp.ocw_geometry_test_snap(sourceVal, targetVal, threshold, outSnapped, outDelta),
				clearBufferRgba: (bufferPtr: number, width: number, height: number, r: number, g: number, b: number, a: number) =>
					exp.ocw_compositor_clear_buffer(bufferPtr, width, height, r, g, b, a),
				compositeLayerRgba: (destPtr: number, destW: number, destH: number, srcPtr: number, srcW: number, srcH: number, cx: number, cy: number, w: number, h: number, rot: number, flipX: number, flipY: number, opacity: number, blendMode: number, maskAlphaPtr: number) =>
					exp.ocw_compositor_composite_layer(destPtr, destW, destH, srcPtr, srcW, srcH, cx, cy, w, h, rot, flipX, flipY, opacity, blendMode, maskAlphaPtr),
				applyMaskToBuffer: (pixelsPtr: number, width: number, height: number, maskType: number, cx: number, cy: number, sx: number, sy: number, rot: number, feather: number, inverted: number) =>
					exp.ocw_mask_apply_to_buffer(pixelsPtr, width, height, maskType, cx, cy, sx, sy, rot, feather, inverted),
				applyColorGradingRgba: (pixelsPtr: number, width: number, height: number, brightness: number, contrast: number, saturation: number, exposure: number, temp: number, tint: number, hue: number, gamma: number) =>
					exp.ocw_effects_apply_color_grading(pixelsPtr, width, height, brightness, contrast, saturation, exposure, temp, tint, hue, gamma),
				applyGaussianBlurRgba: (pixelsPtr: number, width: number, height: number, radius: number, sigma: number) =>
					exp.ocw_effects_apply_gaussian_blur(pixelsPtr, width, height, radius, sigma),
				applyVignetteRgba: (pixelsPtr: number, width: number, height: number, amount: number, softness: number, roundness: number) =>
					exp.ocw_effects_apply_vignette(pixelsPtr, width, height, amount, softness, roundness),
				applyChromaKeyRgba: (pixelsPtr: number, width: number, height: number, keyR: number, keyG: number, keyB: number, similarity: number, smoothness: number, spill: number) =>
					exp.ocw_effects_apply_chroma_key(pixelsPtr, width, height, keyR, keyG, keyB, similarity, smoothness, spill),
				applyGainRamp: (samplesPtr: number, numSamples: number, startGain: number, endGain: number) =>
					exp.ocw_audio_apply_gain_ramp(samplesPtr, numSamples, startGain, endGain),
				mixAudioBuffers: (destPtr: number, srcPtr: number, numSamples: number, volume: number) =>
					exp.ocw_audio_mix_buffers(destPtr, srcPtr, numSamples, volume),
				resampleAudioLinear: (srcPtr: number, srcLen: number, dstPtr: number, dstLen: number) =>
					exp.ocw_audio_resample_linear(srcPtr, srcLen, dstPtr, dstLen),
				computePeakBuckets: (channelPtr: number, startsPtr: number, endsPtr: number, numBuckets: number, outPeaksPtr: number) =>
					exp.ocw_audio_compute_peak_buckets(channelPtr, startsPtr, endsPtr, numBuckets, outPeaksPtr),
				computeRmsBuckets: (channelPtr: number, maxWindowLength: number, startsPtr: number, endsPtr: number, numBuckets: number, outRmsPtr: number) =>
					exp.ocw_audio_compute_rms_buckets(channelPtr, maxWindowLength, startsPtr, endsPtr, numBuckets, outRmsPtr),
				mixAudioChannelRetime: (outputDataPtr: number, outputStartSample: number, renderedLength: number, outputLength: number, sampleRate: number, sourceDataPtr: number, sourceLength: number, sourceSampleRate: number, trimStart: number, retimeRate: number, gain: number) =>
					exp.ocw_audio_mix_channel_retime(outputDataPtr, outputStartSample, renderedLength, outputLength, sampleRate, sourceDataPtr, sourceLength, sourceSampleRate, trimStart, retimeRate, gain),
				snapPointsSorted: (targetTime: number, pointsTimesPtr: number, numPoints: number, maxDistance: number, outSnappedPtr: number, outMatchedIndexPtr: number) =>
					exp.ocw_snap_points_sorted(targetTime, pointsTimesPtr, numPoints, maxDistance, outSnappedPtr, outMatchedIndexPtr),
				snapPointsLinear: (targetTime: number, pointsTimesPtr: number, numPoints: number, maxDistance: number, outSnappedPtr: number, outMatchedIndexPtr: number) =>
					exp.ocw_snap_points_linear(targetTime, pointsTimesPtr, numPoints, maxDistance, outSnappedPtr, outMatchedIndexPtr),
				computeBufferPeak: (samplesPtr: number, numSamples: number) =>
					exp.ocw_audio_compute_buffer_peak(samplesPtr, numSamples),
				clampBufferSamples: (samplesPtr: number, numSamples: number, maxPeak: number) =>
					exp.ocw_audio_clamp_buffer_samples(samplesPtr, numSamples, maxPeak),
				downmixStereo: (leftPtr: number, rightPtr: number, outPtr: number, numSamples: number) =>
					exp.ocw_audio_downmix_stereo(leftPtr, rightPtr, outPtr, numSamples),
				detectAudioBeats: (
					samplesPtr: number,
					samplesPerChannel: number,
					channelCount: number,
					sampleRate: number,
					startSeconds: number,
					endSeconds: number,
					maxBeats: number,
					outTimesPtr: number,
				) =>
					exp.ocw_audio_detect_beats(
						samplesPtr,
						samplesPerChannel,
						channelCount,
						sampleRate,
						startSeconds,
						endSeconds,
						maxBeats,
						outTimesPtr,
					),
				uiSceneCreate: () => exp.ocw_ui_scene_create(),
				uiSceneDestroy: (scene: number) => exp.ocw_ui_scene_destroy(scene),
				uiCreateNode: (scene: number, nodeType: number, x: number, y: number, w: number, h: number) =>
					exp.ocw_ui_create_node(scene, nodeType, x, y, w, h),
				uiAddChild: (scene: number, parentId: number, childId: number) =>
					exp.ocw_ui_add_child(scene, parentId, childId),
				uiComputeLayout: (scene: number, rootW: number, rootH: number) =>
					exp.ocw_ui_compute_layout(scene, rootW, rootH),
				uiHitTest: (scene: number, px: number, py: number) =>
					exp.ocw_ui_hit_test(scene, px, py),
				uiSetHover: (scene: number, px: number, py: number) =>
					exp.ocw_ui_set_hover(scene, px, py),
				timelineHitTestClip: (px: number, py: number, clipX: number, clipY: number, clipW: number, clipH: number, handleW: number) =>
					exp.ocw_timeline_hit_test_clip(px, py, clipX, clipY, clipW, clipH, handleW),
				malloc: (size: number) => exp.oc_malloc(size),
				free: (ptr: number) => exp.oc_free(ptr),
				UTF8ToString: (ptr: number, maxBytes?: number) => {
					const mem = new Uint8Array(memory.buffer);
					let end = ptr;
					const max = maxBytes ? ptr + maxBytes : mem.length;
					while (end < max && mem[end] !== 0) {
						end++;
					}
					const bytes = mem.subarray(ptr, end);
					return new TextDecoder("utf-8").decode(bytes);
				},
				get HEAPF32() {
					return new Float32Array(memory.buffer);
				},
				get HEAPF64() {
					return new Float64Array(memory.buffer);
				},
				get HEAPU32() {
					return new Uint32Array(memory.buffer);
				},
				get HEAPU8() {
					return new Uint8Array(memory.buffer);
				},
			};
			rustSelectionBindings = createRustTimelineSelectionBindings(exp, memory);
			rustAudioBindings = createRustAudioBindings(exp, memory);
			rustAudioSchedulingBindings = createRustAudioSchedulingBindings(exp, memory);
			rustExportTimingBindings = createRustExportTimingBindings(exp, memory);
			rustTrackGapBindings = createRustTrackGapBindings(exp, memory);
			initializeExportRangeBindings(exp, memory);
			initializeAudioExportBindings(exp, memory);
			initializeCaptionTimingBindings(exp, memory);

			console.info("[RustEngine] WebAssembly core loaded and bound.");
			return rustBindings;
		} catch (error) {
			console.warn("[RustEngine] WebAssembly initialization failed:", error);
			return null;
		}
	})();

	return rustLoadPromise;
}

function stringToCStr(exp: any, str: string): number {
	const bytes = new TextEncoder().encode(str);
	const ptr = exp.oc_malloc(bytes.length + 1);
	const mem = new Uint8Array(exp.memory.buffer);
	mem.set(bytes, ptr);
	mem[ptr + bytes.length] = 0;
	return ptr;
}

export function getRustEngineBindings(): NativeTimelineBindings | null {
	return rustBindings;
}

export function getRustTimelineSelectionBindings(): RustTimelineSelectionBindings | null {
	return rustSelectionBindings;
}

export function getRustAudioBindings(): RustAudioBindings | null {
	return rustAudioBindings;
}

export function getRustAudioSchedulingBindings(): RustAudioSchedulingBindings | null {
	return rustAudioSchedulingBindings;
}

export function getRustExportTimingBindings(): RustExportTimingBindings | null {
	return rustExportTimingBindings;
}

export function getRustTrackGapBindings(): RustTrackGapBindings | null {
	return rustTrackGapBindings;
}

type EditingWasmExports = {
	oc_malloc: (size: number) => number;
	oc_free: (pointer: number) => void;
	ocw_export_frame_count?: (duration: number, numerator: number, denominator: number) => number;
	ocw_export_frame_sample?: (duration: number, numerator: number, denominator: number, index: number, out: number) => number;
	ocw_timeline_compact_gaps?: (starts: number, durations: number, count: number, out: number) => number;
};

function validExportInputs(duration: number, numerator: number, denominator: number): boolean {
	return Number.isSafeInteger(duration) && duration >= 0 &&
		[numerator, denominator].every((value) => Number.isInteger(value) && value > 0 && value <= 0xffff_ffff);
}

export function createRustExportTimingBindings(exp: EditingWasmExports, memory: WebAssembly.Memory): RustExportTimingBindings | null {
	const count = exp.ocw_export_frame_count;
	const sample = exp.ocw_export_frame_sample;
	if (!count || !sample) return null;
	return {
		frameCount(duration, numerator, denominator) {
			if (!validExportInputs(duration, numerator, denominator)) return null;
			const result = count(duration, numerator, denominator);
			return Number.isSafeInteger(result) && result >= 0 ? result : null;
		},
		frameSample(duration, numerator, denominator, index) {
			if (!validExportInputs(duration, numerator, denominator) || !Number.isSafeInteger(index) || index < 0) return null;
			let pointer = 0;
			try {
				pointer = exp.oc_malloc(3 * Float64Array.BYTES_PER_ELEMENT);
				if (!pointer || sample(duration, numerator, denominator, index, pointer) !== 1) return null;
				const [timeTicks, timestampSeconds, durationSeconds] = new Float64Array(memory.buffer, pointer, 3);
				if (!Number.isSafeInteger(timeTicks) || timeTicks < 0 || timeTicks >= duration ||
					!Number.isFinite(timestampSeconds) || timestampSeconds < 0 ||
					!Number.isFinite(durationSeconds) || durationSeconds <= 0) return null;
				return { timeTicks, timestampSeconds, durationSeconds };
			} finally {
				if (pointer) exp.oc_free(pointer);
			}
		},
	};
}

export function createRustTrackGapBindings(exp: EditingWasmExports, memory: WebAssembly.Memory): RustTrackGapBindings | null {
	const plan = exp.ocw_timeline_compact_gaps;
	if (!plan) return null;
	return {
		planTrackGapStarts(starts, durations) {
			if (starts.length !== durations.length || starts.length > 1_000_000) return null;
			if (starts.length === 0) return [];
			for (let index = 0; index < starts.length; index++) {
				if (!Number.isSafeInteger(starts[index]) || starts[index] < 0 ||
					!Number.isSafeInteger(durations[index]) || durations[index] <= 0 ||
					!Number.isSafeInteger(starts[index] + durations[index])) return null;
			}
			const pointers: number[] = [];
			try {
				for (let index = 0; index < 3; index++) {
					const pointer = exp.oc_malloc(starts.byteLength);
					if (!pointer) return null;
					pointers.push(pointer);
				}
				new Float64Array(memory.buffer, pointers[0], starts.length).set(starts);
				new Float64Array(memory.buffer, pointers[1], durations.length).set(durations);
				if (plan(pointers[0], pointers[1], starts.length, pointers[2]) !== 1) return null;
				const result = Array.from(new Float64Array(memory.buffer, pointers[2], starts.length));
				return result.every((value) => Number.isSafeInteger(value) && value >= 0) ? result : null;
			} finally {
				for (const pointer of pointers) exp.oc_free(pointer);
			}
		},
	};
}

type AudioSchedulingWasmExports = {
	oc_malloc: (size: number) => number;
	oc_free: (pointer: number) => void;
	ocw_audio_plan_buffer_schedule?: (
		clipStart: number,
		clipDuration: number,
		sourceStart: number,
		playbackRate: number,
		bufferStart: number,
		bufferDuration: number,
		requestedStart: number,
		contextOffset: number,
		contextNow: number,
		outWindow: number,
	) => number;
};

function createRustAudioSchedulingBindings(
	exp: AudioSchedulingWasmExports,
	memory: WebAssembly.Memory,
): RustAudioSchedulingBindings | null {
	const plan = exp.ocw_audio_plan_buffer_schedule;
	if (typeof plan !== "function") return null;

	return {
		planBufferSchedule(request) {
			let windowPtr = 0;
			try {
				windowPtr = exp.oc_malloc(4 * Float64Array.BYTES_PER_ELEMENT);
				if (!windowPtr) return null;
				const status = plan(
					request.clipStart,
					request.clipDuration,
					request.sourceStart,
					request.playbackRate,
					request.bufferStart,
					request.bufferDuration,
					request.requestedStart,
					request.contextOffset,
					request.contextNow,
					windowPtr,
				);
				if (status === -1) return { status: "invalid" };
				if (status === 0) return { status: "empty" };
				if (status !== 1) return null;

				const [contextStart, sourceOffset, sourceDuration, timelineStart] =
					new Float64Array(memory.buffer, windowPtr, 4);
				if (
					![contextStart, sourceOffset, sourceDuration, timelineStart].every(Number.isFinite) ||
					contextStart < 0 || sourceOffset < 0 || sourceDuration <= 0 || timelineStart < 0
				) {
					return null;
				}
				return {
					status: "ready",
					window: { contextStart, sourceOffset, sourceDuration, timelineStart },
				};
			} catch {
				return null;
			} finally {
				if (windowPtr) exp.oc_free(windowPtr);
			}
		},
	};
}

function createRustAudioBindings(
	exp: Record<string, any>,
	memory: WebAssembly.Memory,
): RustAudioBindings | null {
	const samplePeakBuckets = exp.ocw_audio_sample_peak_buckets;
	if (typeof samplePeakBuckets !== "function") {
		return null;
	}

	return {
		samplePeakBuckets({ amplitudes, bucketSize, starts, ends }) {
			if (
				starts.length !== ends.length ||
				!Number.isFinite(bucketSize) ||
				bucketSize <= 0
			) {
				return null;
			}
			if (starts.length === 0) {
				return new Float32Array();
			}
			if (amplitudes.length === 0) {
				return new Float32Array(starts.length);
			}

			let amplitudesPtr = 0;
			let startsPtr = 0;
			let endsPtr = 0;
			let outPeaksPtr = 0;
			try {
				amplitudesPtr = exp.oc_malloc(amplitudes.byteLength);
				startsPtr = exp.oc_malloc(starts.byteLength);
				endsPtr = exp.oc_malloc(ends.byteLength);
				outPeaksPtr = exp.oc_malloc(starts.length * Float32Array.BYTES_PER_ELEMENT);
				if (!amplitudesPtr || !startsPtr || !endsPtr || !outPeaksPtr) {
					return null;
				}

				const heapF32 = new Float32Array(memory.buffer);
				const heapF64 = new Float64Array(memory.buffer);
				heapF32.set(amplitudes, amplitudesPtr >> 2);
				heapF64.set(starts, startsPtr >> 3);
				heapF64.set(ends, endsPtr >> 3);
				samplePeakBuckets(
					amplitudesPtr,
					amplitudes.length,
					bucketSize,
					startsPtr,
					endsPtr,
					starts.length,
					outPeaksPtr,
				);
				return new Float32Array(memory.buffer, outPeaksPtr, starts.length).slice();
			} catch {
				return null;
			} finally {
				if (outPeaksPtr) exp.oc_free(outPeaksPtr);
				if (endsPtr) exp.oc_free(endsPtr);
				if (startsPtr) exp.oc_free(startsPtr);
				if (amplitudesPtr) exp.oc_free(amplitudesPtr);
			}
		},
	};
}

function createRustTimelineSelectionBindings(
	exp: Record<string, any>,
	memory: WebAssembly.Memory,
): RustTimelineSelectionBindings | null {
	const select = exp.ocw_canvas_select_rect_intersections;
	if (typeof select !== "function") {
		return null;
	}

	return {
		selectRectIntersections(rectangles, selection) {
			if (
				rectangles.length === 0 ||
				rectangles.length % 4 !== 0 ||
				selection.length !== 4
			) {
				return null;
			}

			const rectangleCount = rectangles.length / 4;
			let rectanglesPtr = 0;
			let selectionPtr = 0;
			let outIndicesPtr = 0;
			try {
				rectanglesPtr = exp.oc_malloc(rectangles.byteLength);
				selectionPtr = exp.oc_malloc(selection.byteLength);
				outIndicesPtr = exp.oc_malloc(rectangleCount * Int32Array.BYTES_PER_ELEMENT);
				if (!rectanglesPtr || !selectionPtr || !outIndicesPtr) {
					return null;
				}

				const heap = new Float64Array(memory.buffer);
				heap.set(rectangles, rectanglesPtr >> 3);
				heap.set(selection, selectionPtr >> 3);
				const selectedCount = select(
					rectanglesPtr,
					rectangleCount,
					selectionPtr,
					outIndicesPtr,
					rectangleCount,
				);
				if (
					!Number.isInteger(selectedCount) ||
					selectedCount < 0 ||
					selectedCount > rectangleCount
				) {
					return null;
				}

				return Array.from(
					new Int32Array(memory.buffer, outIndicesPtr, selectedCount),
				);
			} catch {
				return null;
			} finally {
				if (outIndicesPtr) exp.oc_free(outIndicesPtr);
				if (selectionPtr) exp.oc_free(selectionPtr);
				if (rectanglesPtr) exp.oc_free(rectanglesPtr);
			}
		},
	};
}

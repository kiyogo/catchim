/**
 * OpenCut Rust WebAssembly Native Engine
 * Direct zero-copy bridge to opencut_engine.wasm
 */

import type { NativeTimelineBindings } from "./opencut-core";

let rustBindings: NativeTimelineBindings | null = null;
let rustLoadPromise: Promise<NativeTimelineBindings | null> | null = null;

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

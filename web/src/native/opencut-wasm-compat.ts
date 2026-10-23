/**
 * OpenCut C++ Native Core WebAssembly Compatibility Layer
 * Replaces the legacy opencut-wasm (Rust) package with 100% C++20 core engine.
 */

import { getNativeTimelineBindings, warmNativeTimelineCore } from "./opencut-core";
import { webGpuEngine } from "./webgpu-engine";

const TICKS_PER_SECOND_CONST = 120_000;
export function TICKS_PER_SECOND(): number {
	const bindings = getNativeTimelineBindings();
	return bindings ? bindings.ticksPerSecond() : TICKS_PER_SECOND_CONST;
}
const TICKS_PER_SECOND_F64 = 120_000.0;
const SECONDS_PER_HOUR = 3_600;
const SECONDS_PER_MINUTE = 60;
const CENTISECONDS_PER_SECOND = 100;
const TICKS_PER_CENTISECOND = TICKS_PER_SECOND_CONST / CENTISECONDS_PER_SECOND;

export interface FrameRate {
	numerator: number;
	denominator: number;
}

export type TimeCodeFormat = "MM:SS" | "HH:MM:SS" | "HH:MM:SS:CS" | "HH:MM:SS:FF";

// Ensure native C++ core and WebGPU acceleration are loaded in browser
if (typeof window !== "undefined") {
	warmNativeTimelineCore();
	initializeGpu().catch((err) => {
		console.warn("[WebGPU] Auto-init fallback notice:", err);
	});
}

function getTicksPerFrame(rate: FrameRate): number | null {
	if (!rate || rate.numerator <= 0 || rate.denominator <= 0) return null;
	const tickNum = TICKS_PER_SECOND_CONST * rate.denominator;
	const tickDen = rate.numerator;
	if (tickNum % tickDen !== 0) return null;
	return Math.floor(tickNum / tickDen);
}

function divEuclid(a: number, b: number): number {
	const q = Math.trunc(a / b);
	const r = a % b;
	return r < 0 ? q + (b > 0 ? -1 : 1) : q;
}

function remEuclid(a: number, b: number): number {
	const r = a % b;
	return r < 0 ? r + (b > 0 ? b : -b) : r;
}

export const _TICKS_PER_SECOND = TICKS_PER_SECOND;

export function mediaTimeFromSeconds({ seconds }: { seconds: number }): number {
	if (!Number.isFinite(seconds)) return 0;
	const bindings = getNativeTimelineBindings();
	if (bindings) {
		return bindings.fromSeconds(seconds);
	}
	return Math.round(seconds * TICKS_PER_SECOND_F64);
}

export function mediaTimeToSeconds({ time }: { time: number }): number {
	const bindings = getNativeTimelineBindings();
	if (bindings) {
		return bindings.toSeconds(time);
	}
	return time / TICKS_PER_SECOND_F64;
}

export function roundToFrame({
	time,
	rate,
}: {
	time: number;
	rate: FrameRate;
}): number | null {
	const bindings = getNativeTimelineBindings();
	if (bindings && rate) {
		return bindings.roundToFrame(time, rate.numerator, rate.denominator);
	}

	const tpf = getTicksPerFrame(rate);
	if (!tpf) return null;
	const remainder = remEuclid(time, tpf);
	const floor = divEuclid(time, tpf);
	const frame = remainder * 2 >= tpf ? floor + 1 : floor;
	return frame * tpf;
}

export function floorToFrame({
	time,
	rate,
}: {
	time: number;
	rate: FrameRate;
}): number | null {
	const bindings = getNativeTimelineBindings();
	if (bindings && rate) {
		return bindings.floorToFrame(time, rate.numerator, rate.denominator);
	}

	const tpf = getTicksPerFrame(rate);
	if (!tpf) return null;
	const floor = divEuclid(time, tpf);
	return floor * tpf;
}

export function lastFrameTime({
	duration,
	rate,
}: {
	duration: number;
	rate: FrameRate;
}): number | null {
	if (duration <= 0) return 0;
	const bindings = getNativeTimelineBindings();
	if (bindings && rate) {
		return bindings.lastFrame(duration, rate.numerator, rate.denominator);
	}

	const lastTick = duration - 1;
	return floorToFrame({ time: lastTick, rate });
}

export function snappedSeekTime({
	time,
	duration,
	rate,
}: {
	time: number;
	duration: number;
	rate: FrameRate;
}): number | null {
	const bindings = getNativeTimelineBindings();
	if (bindings && rate) {
		return bindings.snappedSeek(time, duration, rate.numerator, rate.denominator);
	}

	const snapped = roundToFrame({ time, rate });
	if (snapped === null) return null;
	return Math.max(0, Math.min(snapped, duration));
}

export function guessTimecodeFormat({
	timeCode,
}: {
	timeCode: string;
}): TimeCodeFormat | null {
	const trimmed = timeCode.trim();
	if (!trimmed) return null;
	const parts = trimmed.split(":");
	for (const p of parts) {
		if (isNaN(Number(p))) return null;
	}
	if (parts.length === 2) return "MM:SS";
	if (parts.length === 3) return "HH:MM:SS";
	if (parts.length === 4) return "HH:MM:SS:FF";
	return null;
}

let timecodeBufPtr = 0;

export function formatTimecode({
	time,
	format = "HH:MM:SS:CS",
	rate,
}: {
	time: number;
	format?: TimeCodeFormat;
	rate?: FrameRate;
}): string {
	const bindings = getNativeTimelineBindings();
	if (bindings) {
		if (timecodeBufPtr === 0) {
			timecodeBufPtr = bindings.malloc(64);
		}
		if (timecodeBufPtr !== 0) {
			const fmtIdx =
				format === "MM:SS"
					? 0
					: format === "HH:MM:SS"
						? 1
						: format === "HH:MM:SS:FF"
							? 3
							: 2;
			const ok = bindings.formatTimecode(
				time,
				fmtIdx,
				rate?.numerator ?? 30,
				rate?.denominator ?? 1,
				timecodeBufPtr,
				64,
			);
			if (ok === 1) {
				return bindings.UTF8ToString(timecodeBufPtr);
			}
		}
	}

	const totalTicks = Math.max(0, Math.trunc(time));
	const totalSeconds = Math.floor(totalTicks / TICKS_PER_SECOND_CONST);
	const hourTicks = SECONDS_PER_HOUR * TICKS_PER_SECOND_CONST;
	const minuteTicks = SECONDS_PER_MINUTE * TICKS_PER_SECOND_CONST;

	const hours = Math.floor(totalTicks / hourTicks);
	const minutes = Math.floor((totalTicks % hourTicks) / minuteTicks);
	const seconds = totalSeconds % SECONDS_PER_MINUTE;
	const secondTicks = totalTicks % TICKS_PER_SECOND_CONST;
	const centiseconds = Math.floor(secondTicks / TICKS_PER_CENTISECOND);

	const pad = (n: number) => (n < 10 ? `0${n}` : `${n}`);

	switch (format) {
		case "MM:SS":
			return `${pad(minutes)}:${pad(seconds)}`;
		case "HH:MM:SS":
			return `${pad(hours)}:${pad(minutes)}:${pad(seconds)}`;
		case "HH:MM:SS:CS":
			return `${pad(hours)}:${pad(minutes)}:${pad(seconds)}:${pad(centiseconds)}`;
		case "HH:MM:SS:FF": {
			if (!rate) return `${pad(hours)}:${pad(minutes)}:${pad(seconds)}:00`;
			const tpf = getTicksPerFrame(rate) ?? 4000;
			const frames = Math.floor(secondTicks / tpf);
			return `${pad(hours)}:${pad(minutes)}:${pad(seconds)}:${pad(frames)}`;
		}
		default:
			return `${pad(hours)}:${pad(minutes)}:${pad(seconds)}:${pad(centiseconds)}`;
	}
}

export function parseTimecode({
	timeCode,
	format = "HH:MM:SS:CS",
	rate,
}: {
	timeCode: string;
	format?: TimeCodeFormat;
	rate?: FrameRate;
}): number | null {
	const trimmed = timeCode.trim();
	if (!trimmed) return null;

	const bindings = getNativeTimelineBindings();
	if (bindings) {
		const fmtIdx = format === "MM:SS" ? 0 : format === "HH:MM:SS" ? 1 : format === "HH:MM:SS:FF" ? 3 : 2;
		const res = bindings.parseTimecode(trimmed, fmtIdx, rate?.numerator ?? 30, rate?.denominator ?? 1);
		if (res >= 0) return res;
	}

	const parts = trimmed.split(":").map(Number);
	if (parts.some(isNaN)) return null;

	switch (format) {
		case "MM:SS": {
			if (parts.length !== 2) return null;
			const [minutes, seconds] = parts;
			if (seconds >= SECONDS_PER_MINUTE) return null;
			return (minutes * SECONDS_PER_MINUTE + seconds) * TICKS_PER_SECOND_CONST;
		}
		case "HH:MM:SS": {
			if (parts.length !== 3) return null;
			const [hours, minutes, seconds] = parts;
			if (minutes >= SECONDS_PER_MINUTE || seconds >= SECONDS_PER_MINUTE) return null;
			return (hours * SECONDS_PER_HOUR + minutes * SECONDS_PER_MINUTE + seconds) * TICKS_PER_SECOND_CONST;
		}
		case "HH:MM:SS:CS": {
			if (parts.length !== 4) return null;
			const [hours, minutes, seconds, centiseconds] = parts;
			if (minutes >= SECONDS_PER_MINUTE || seconds >= SECONDS_PER_MINUTE || centiseconds >= CENTISECONDS_PER_SECOND) {
				return null;
			}
			return (hours * SECONDS_PER_HOUR + minutes * SECONDS_PER_MINUTE + seconds) * TICKS_PER_SECOND_CONST +
				centiseconds * TICKS_PER_CENTISECOND;
		}
		case "HH:MM:SS:FF": {
			if (!rate || parts.length !== 4) return null;
			const tpf = getTicksPerFrame(rate);
			if (!tpf) return null;
			const [hours, minutes, seconds, frames] = parts;
			const bound = Math.ceil(rate.numerator / rate.denominator);
			if (minutes >= SECONDS_PER_MINUTE || seconds >= SECONDS_PER_MINUTE || frames >= bound) {
				return null;
			}
			return (hours * SECONDS_PER_HOUR + minutes * SECONDS_PER_MINUTE + seconds) * TICKS_PER_SECOND_CONST +
				frames * tpf;
		}
		default:
			return null;
	}
}

// Compositor & WebGPU Engine Implementation
let compositorCanvas: HTMLCanvasElement | null = null;
let compositorGpuContext: GPUCanvasContext | null = null;
let compositor2dContext: CanvasRenderingContext2D | null = null;
interface TextureEntry {
	source: CanvasImageSource;
	width: number;
	height: number;
	gpuTexture?: GPUTexture | null;
	lastVideoTime?: number;
	version?: number | string;
	dirty?: boolean;
}
const textures = new Map<string, TextureEntry>();

function ensureCompositorContext(): void {
	if (!compositorCanvas) return;
	if ((compositorGpuContext && !webGpuEngine.isAvailable) ||
		(compositor2dContext && webGpuEngine.isAvailable)) {
		// A canvas cannot change context type after creation. On device loss,
		// or when the GPU becomes ready, replace the surface with the right context.
		const next = document.createElement("canvas");
		next.width = compositorCanvas.width; next.height = compositorCanvas.height;
		next.id = compositorCanvas.id; next.className = compositorCanvas.className;
		next.style.cssText = compositorCanvas.style.cssText;
		compositorCanvas.replaceWith(next); compositorCanvas = next;
		compositorGpuContext = null; compositor2dContext = null;
	}
	if (compositorGpuContext || compositor2dContext) return;

	if (webGpuEngine.isAvailable && webGpuEngine.device) {
		try {
			const ctx = compositorCanvas.getContext("webgpu") as GPUCanvasContext | null;
			if (ctx) {
				compositorGpuContext = ctx;
				compositorCanvas.setAttribute("data-engine", "webgpu");
				if (webGpuEngine.device) {
					ctx.configure({
						device: webGpuEngine.device,
						format: webGpuEngine.preferredFormat,
						alphaMode: "premultiplied",
					});
					console.info("%c[Compositor] WebGPU hardware context configured successfully.", "color: #06b6d4; font-weight: bold;");
				}
				return;
			}
		} catch (e) {
			console.warn("[WebGPU] Could not acquire webgpu context:", e);
		}
	}

	try {
		compositor2dContext = compositorCanvas.getContext("2d");
		compositorCanvas.setAttribute("data-engine", "canvas-2d-fallback");
	} catch (e) {
		// Context creation failure fallback
	}
}

export function getCompositorBackend(): "webgpu" | "canvas2d" | "unavailable" {
	ensureCompositorContext();
	return compositorGpuContext && webGpuEngine.isAvailable
		? "webgpu" : compositor2dContext ? "canvas2d" : "unavailable";
}

export function initCompositor(width: number, height: number): void {
	if (typeof document === "undefined") return;
	if (!compositorCanvas) {
		compositorCanvas = document.createElement("canvas");
		compositorCanvas.id = "opencut-webgpu-viewport";
		compositorCanvas.className = "opencut-webgpu-compositor";
		compositorCanvas.setAttribute("data-engine", "webgpu");
	}
	compositorCanvas.width = width;
	compositorCanvas.height = height;
	ensureCompositorContext();
}

export function resizeCompositor(width: number, height: number): void {
	if (compositorCanvas) {
		compositorCanvas.width = width;
		compositorCanvas.height = height;
		if (compositorGpuContext && webGpuEngine.device) {
			try {
				compositorGpuContext.configure({
					device: webGpuEngine.device,
					format: webGpuEngine.preferredFormat,
					alphaMode: "premultiplied",
				});
			} catch (e) {
				console.warn("[WebGPU] Resize reconfigure error:", e);
			}
		}
	}
}

export function getCompositorCanvas(): HTMLCanvasElement {
	if (!compositorCanvas && typeof document !== "undefined") {
		compositorCanvas = document.createElement("canvas");
		compositorCanvas.width = 1920;
		compositorCanvas.height = 1080;
		compositorCanvas.id = "opencut-webgpu-viewport";
		compositorCanvas.className = "opencut-webgpu-compositor";
		compositorCanvas.setAttribute("data-engine", "webgpu");
	}
	ensureCompositorContext();
	return compositorCanvas!;
}

export function uploadTexture(descriptor: {
	id: string;
	source: CanvasImageSource;
	width: number;
	height: number;
	version?: number | string;
}): void {
	const existing = textures.get(descriptor.id);
	if (existing) {
		const isSameSize =
			existing.width === descriptor.width && existing.height === descriptor.height;
		if (isSameSize) {
			const sourceChanged = existing.source !== descriptor.source;
			existing.source = descriptor.source;
			if (descriptor.version !== undefined) {
				if (existing.version !== descriptor.version) {
					existing.version = descriptor.version;
					existing.dirty = true;
				}
			} else if (
				typeof HTMLVideoElement !== "undefined" &&
				descriptor.source instanceof HTMLVideoElement
			) {
				if (descriptor.source.currentTime !== existing.lastVideoTime) {
					existing.lastVideoTime = descriptor.source.currentTime;
					existing.dirty = true;
				}
			} else if (sourceChanged) {
				existing.dirty = true;
			} else {
				// Rendered canvases keep identity while their pixels change.
				existing.dirty = true;
			}
			return;
		}
		if (existing.gpuTexture) {
			webGpuEngine.releaseTexture(existing.gpuTexture);
		}
	}

	textures.set(descriptor.id, {
		source: descriptor.source,
		width: descriptor.width,
		height: descriptor.height,
		gpuTexture: null,
		version: descriptor.version,
		lastVideoTime:
			typeof HTMLVideoElement !== "undefined" &&
			descriptor.source instanceof HTMLVideoElement
				? descriptor.source.currentTime
				: undefined,
		dirty: true,
	});
}

export function releaseTexture(id: string): void {
	const existing = textures.get(id);
	if (existing?.gpuTexture) {
		webGpuEngine.releaseTexture(existing.gpuTexture);
	}
	textures.delete(id);
}

let lastFrameProfile = {
	renderTimeMs: 0,
	layerCount: 0,
};

let blendSurface: OffscreenCanvas | null = null;
let layerSurface: OffscreenCanvas | null = null;
let filterSurface: OffscreenCanvas | null = null;
let blendTexture: GPUTexture | null = null;
function surface(previous: OffscreenCanvas | null, width: number, height: number) {
	return previous?.width === width && previous.height === height ? previous : new OffscreenCanvas(width, height);
}
function blurSigma(groups: any[][] = []) {
	let x = 0, y = 0;
	for (const pass of groups.flat()) {
		if (pass.shader !== "gaussian-blur") continue;
		const sigma = Number(pass.uniforms.u_sigma ?? 0) * Number(pass.uniforms.u_step ?? 1);
		const direction = pass.uniforms.u_direction ?? [1, 0];
		x += sigma * sigma * direction[0] * direction[0]; y += sigma * sigma * direction[1] * direction[1];
	}
	return Math.sqrt(Math.max(x, y));
}

function pixelEffectPasses(groups: any[][] = []): any[] {
	return groups.flat().filter((pass) => pass.shader !== "gaussian-blur");
}

function drawFrame2D(frame: any, ctx: CanvasRenderingContext2D | OffscreenCanvasRenderingContext2D) {
	const { width, height } = frame;
	layerSurface = surface(layerSurface, width, height);
	filterSurface = surface(filterSurface, width, height);
	const layer = layerSurface.getContext("2d")!;
	const filter = filterSurface.getContext("2d")!;
	ctx.resetTransform(); ctx.globalAlpha = 1; ctx.globalCompositeOperation = "source-over"; ctx.filter = "none";
	ctx.clearRect(0, 0, width, height);
	const [r, g, b, a] = frame.clear?.color ?? [0, 0, 0, 1];
	ctx.fillStyle = `rgba(${r * 255},${g * 255},${b * 255},${a})`; ctx.fillRect(0, 0, width, height);
	for (const item of frame.items ?? []) {
		if (item.type === "sceneEffect") {
			const sigma = blurSigma(item.effectPassGroups);
			const pixelPasses = pixelEffectPasses(item.effectPassGroups);
			if (sigma > 0 || pixelPasses.length > 0) {
				filter.clearRect(0, 0, width, height);
				filter.filter = sigma > 0 ? `blur(${sigma}px)` : "none";
				filter.drawImage(ctx.canvas, 0, 0);
				filter.filter = "none";
				if (pixelPasses.length > 0) {
					applyEffectPasses({
						source: filterSurface,
						width,
						height,
						passes: pixelPasses,
					});
				}
				ctx.clearRect(0, 0, width, height); ctx.drawImage(filterSurface, 0, 0);
			}
			continue;
		}
		const texture = textures.get(item.textureId);
		if (!texture) continue;
		layer.resetTransform(); layer.globalAlpha = 1; layer.globalCompositeOperation = "source-over"; layer.filter = "none";
		layer.clearRect(0, 0, width, height);
		layer.save();
		const sigma = blurSigma(item.effectPassGroups);
		if (sigma > 0) layer.filter = `blur(${sigma}px)`;
		const t = item.transform;
		if (t) {
			layer.translate(t.centerX, t.centerY); layer.rotate((t.rotationDegrees || 0) * Math.PI / 180);
			layer.scale(t.flipX ? -1 : 1, t.flipY ? -1 : 1);
			layer.drawImage(texture.source, -t.width / 2, -t.height / 2, t.width, t.height);
		} else layer.drawImage(texture.source, 0, 0, width, height);
		layer.restore();
		const pixelPasses = pixelEffectPasses(item.effectPassGroups);
		if (pixelPasses.length > 0) {
			applyEffectPasses({
				source: layerSurface,
				width,
				height,
				passes: pixelPasses,
			});
		}
		const mask = item.mask ? textures.get(item.mask.textureId) : null;
		if (mask) {
			layer.save(); layer.globalCompositeOperation = item.mask.inverted ? "destination-out" : "destination-in";
			layer.filter = item.mask.feather > 0 ? `blur(${item.mask.feather / 2}px)` : "none";
			layer.drawImage(mask.source, 0, 0, width, height); layer.restore();
		}
		ctx.save(); ctx.globalAlpha = item.opacity ?? 1;
		ctx.globalCompositeOperation = !item.blendMode || item.blendMode === "normal" ? "source-over" : item.blendMode;
		ctx.drawImage(layerSurface, 0, 0); ctx.restore();
	}
}

export function renderFrame(frame: any): void {
	if (!compositorCanvas) return;
	const startTime = typeof performance !== "undefined" ? performance.now() : 0;
	ensureCompositorContext();

	let layerCount = 0;
	// Canvas blend modes preserve the editor's full blend catalogue. Keep the
	// normal video/transform path on WebGPU without these intermediate surfaces.
	const canvasBlendRequired = frame.items?.some((item: any) => item.type === "layer" && item.blendMode && item.blendMode !== "normal");
	if (compositorGpuContext && webGpuEngine.isAvailable && canvasBlendRequired) {
		blendSurface = surface(blendSurface, frame.width, frame.height);
		drawFrame2D(frame, blendSurface.getContext("2d")!);
		if (blendTexture && (blendTexture.width !== frame.width || blendTexture.height !== frame.height)) { blendTexture.destroy(); blendTexture = null; }
		if (!blendTexture) blendTexture = webGpuEngine.uploadImageToTexture(blendSurface);
		else webGpuEngine.updateTextureFromImage(blendTexture, blendSurface, frame.width, frame.height);
		if (blendTexture) webGpuEngine.renderFrameLayers({ context: compositorGpuContext, width: frame.width, height: frame.height, layers: [{ texture: blendTexture }] });
		lastFrameProfile = { renderTimeMs: performance.now() - startTime, layerCount: frame.items.length };
		return;
	}

	if (compositorGpuContext && webGpuEngine.isAvailable && webGpuEngine.device) {
		const temporaries: GPUTexture[] = [];
		webGpuEngine.beginEffectsFrame();
		const clearColor: [number, number, number, number] = frame.clear?.color ?? [0, 0, 0, 1];
		const gpuLayers: Array<{
			texture: GPUTexture;
			maskTexture?: GPUTexture;
			maskInverted?: boolean;
			opacity?: number;
			transform?: any;
		}> = [];

		if (frame.items) {
			for (const item of frame.items) {
				if (item.type === "sceneEffect") {
					const target = webGpuEngine.acquireTexture({ width: compositorCanvas.width, height: compositorCanvas.height, format: webGpuEngine.preferredFormat, usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING });
					temporaries.push(target);
					webGpuEngine.renderFrameLayers({ context: compositorGpuContext, targetTexture: target, width: compositorCanvas.width, height: compositorCanvas.height, clearColor, layers: gpuLayers });
					const filtered = webGpuEngine.applyBlurPasses(target, (item.effectPassGroups ?? []).flat(), temporaries);
					gpuLayers.splice(0, gpuLayers.length, { texture: filtered });
					continue;
				}
				if (item.type === "layer" && item.textureId) {
					const tex = textures.get(item.textureId);
					if (!tex) continue;

					// Check if video element advanced to a new frame
					if (typeof HTMLVideoElement !== "undefined" && tex.source instanceof HTMLVideoElement) {
						if (tex.source.currentTime !== tex.lastVideoTime) {
							tex.lastVideoTime = tex.source.currentTime;
							tex.dirty = true;
						}
					}

					// Update or upload GPU texture ONLY when dirty or not yet on GPU
					if (!tex.gpuTexture || tex.dirty) {
						if (tex.gpuTexture) {
							const updated = webGpuEngine.updateTextureFromImage(
								tex.gpuTexture,
								tex.source as any,
								tex.width,
								tex.height,
							);
							if (updated) {
								tex.dirty = false;
							} else {
								webGpuEngine.releaseTexture(tex.gpuTexture);
								tex.gpuTexture = webGpuEngine.uploadImageToTexture(tex.source as any);
								tex.dirty = false;
							}
						} else {
							try {
								tex.gpuTexture = webGpuEngine.uploadImageToTexture(tex.source as any);
								tex.dirty = false;
							} catch (err) {
								console.warn("[WebGPU] Texture upload failed:", err);
							}
						}
					}

					if (tex.gpuTexture) {
						let maskTexture: GPUTexture | undefined;
						const mask = item.mask ? textures.get(item.mask.textureId) : undefined;
						if (mask) {
							if (!mask.gpuTexture) mask.gpuTexture = webGpuEngine.uploadImageToTexture(mask.source as any);
							else if (mask.dirty) webGpuEngine.updateTextureFromImage(mask.gpuTexture, mask.source as any, mask.width, mask.height);
							mask.dirty = false;
							maskTexture = mask.gpuTexture ?? undefined;
							if (maskTexture && item.mask.feather > 0) {
								const sigma = Math.max(.1, item.mask.feather / 2);
								maskTexture = webGpuEngine.applyBlurPasses(maskTexture, [
									{ shader: "gaussian-blur", uniforms: { u_sigma: sigma, u_direction: [1, 0] } },
									{ shader: "gaussian-blur", uniforms: { u_sigma: sigma, u_direction: [0, 1] } },
								], temporaries);
							}
						}
						gpuLayers.push({
							texture: webGpuEngine.applyBlurPasses(tex.gpuTexture, (item.effectPassGroups ?? []).flat(), temporaries),
							maskTexture, maskInverted: item.mask?.inverted,
							opacity: item.opacity ?? 1.0,
							transform: item.transform,
						});
						layerCount++;
					}
				}
			}
		}

		try {
			webGpuEngine.renderFrameLayers({
				context: compositorGpuContext,
				width: compositorCanvas.width,
				height: compositorCanvas.height,
				clearColor,
				layers: gpuLayers,
			});
		} catch (err) {
			console.error("[WebGPU] Render pass failed:", err);
			throw err;
		} finally {
			for (const texture of temporaries) webGpuEngine.releaseTexture(texture);
		}
	} else if (compositor2dContext) {
		drawFrame2D(frame, compositor2dContext);
		lastFrameProfile = { renderTimeMs: performance.now() - startTime, layerCount: frame.items?.length ?? 0 };
		return;
	}

	if (typeof performance !== "undefined") {
		lastFrameProfile = {
			renderTimeMs: performance.now() - startTime,
			layerCount,
		};
	}
}

export function getLastFrameProfile(): any {
	return [{ name: "compositorSubmit", durationMs: lastFrameProfile.renderTimeMs }];
}

export async function initializeGpu(): Promise<void> {
	warmNativeTimelineCore();
	if (typeof window !== "undefined" && typeof navigator !== "undefined" && navigator.gpu) {
		try {
			const success = await webGpuEngine.initialize();
			if (success && compositorCanvas) ensureCompositorContext();
		} catch (err) {
			console.warn("[WebGPU] Initialization error:", err);
		}
	}
}

export function applyEffectPasses({
	source,
	width,
	height,
	passes,
}: {
	source: OffscreenCanvas;
	width: number;
	height: number;
	passes: any;
}): OffscreenCanvas {
	if (!passes || passes.length === 0) return source;

	const bindings = getNativeTimelineBindings();
	if (!bindings) return source;

	const ctx = source.getContext("2d");
	if (!ctx) return source;

	try {
		const imgData = ctx.getImageData(0, 0, width, height);
		const byteLen = width * height * 4;
		const ptr = bindings.malloc(byteLen);
		if (!ptr) return source;

		bindings.HEAPU8.set(new Uint8Array(imgData.data.buffer), ptr);

		for (const pass of passes) {
			const type = pass.type || pass.shader || "";
			const getUniform = (name: string, fallback = 0) => {
				if (pass[name] !== undefined) return pass[name];
				if (Array.isArray(pass.uniforms)) {
					const u = pass.uniforms.find((item: any) => item.name === name);
					return u ? (Array.isArray(u.value) ? u.value[0] : u.value) : fallback;
				}
				if (pass.uniforms && typeof pass.uniforms === "object") {
					const val = pass.uniforms[name];
					return val !== undefined ? (Array.isArray(val) ? val[0] : val) : fallback;
				}
				return fallback;
			};

			if (type === "color_grading" || type === "adjustments" || type.includes("color")) {
				bindings.applyColorGradingRgba(
					ptr,
					width,
					height,
					getUniform("brightness", 0),
					getUniform("contrast", 0),
					getUniform("saturation", 0),
					getUniform("exposure", 0),
					getUniform("temperature", 0),
					getUniform("tint", 0),
					getUniform("hue", 0),
					getUniform("gamma", 1.0),
				);
			} else if (type === "gaussian_blur" || type === "blur" || type.includes("blur")) {
				const sigma = getUniform("u_sigma", getUniform("sigma", 0));
				const radius = getUniform("radius", Math.round(sigma * 2.5) || 5);
				bindings.applyGaussianBlurRgba(
					ptr,
					width,
					height,
					Math.max(1, Math.round(radius)),
					sigma,
				);
			} else if (type === "vignette" || type.includes("vignette")) {
				bindings.applyVignetteRgba(
					ptr,
					width,
					height,
					getUniform("amount", 0.5),
					getUniform("softness", 0.5),
					getUniform("roundness", 1.0),
				);
			} else if (type === "chroma_key" || type.includes("chroma")) {
				bindings.applyChromaKeyRgba(
					ptr,
					width,
					height,
					getUniform("keyR", 0),
					getUniform("keyG", 1),
					getUniform("keyB", 0),
					getUniform("similarity", 0.4),
					getUniform("smoothness", 0.1),
					getUniform("spill", 0.1),
				);
			}
		}

		const resultBytes = bindings.HEAPU8.subarray(ptr, ptr + byteLen);
		imgData.data.set(resultBytes);
		bindings.free(ptr);

		ctx.putImageData(imgData, 0, 0);
	} catch (err) {
		console.warn("[OpenCut] Native effect pass execution failed:", err);
	}

	return source;
}

export function applyMaskFeather({
	mask,
	width,
	height,
	feather,
}: {
	mask: any;
	width: number;
	height: number;
	feather: number;
}): OffscreenCanvas {
	if (feather <= 0.01) return mask;

	const bindings = getNativeTimelineBindings();
	if (!bindings) return mask;

	const canvas = mask as OffscreenCanvas;
	const ctx = canvas.getContext("2d");
	if (!ctx) return mask;

	try {
		const imgData = ctx.getImageData(0, 0, width, height);
		const byteLen = width * height * 4;
		const ptr = bindings.malloc(byteLen);
		if (!ptr) return mask;

		bindings.HEAPU8.set(new Uint8Array(imgData.data.buffer), ptr);
		bindings.applyGaussianBlurRgba(ptr, width, height, Math.round(feather), feather * 0.5);

		const resultBytes = bindings.HEAPU8.subarray(ptr, ptr + byteLen);
		imgData.data.set(resultBytes);
		bindings.free(ptr);

		ctx.putImageData(imgData, 0, 0);
	} catch (err) {
		console.warn("[OpenCut] Native mask feather failed:", err);
	}

	return mask;
}

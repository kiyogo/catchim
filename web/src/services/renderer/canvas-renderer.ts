import type { FrameRate } from "opencut-wasm";
import type { AnyBaseNode } from "./nodes/base-node";
import { createCanvasSurface } from "./canvas-utils";
import { buildFrameDescriptor } from "./compositor/frame-descriptor";
import { wasmCompositor } from "./compositor/wasm-compositor";
import { resolveRenderTree } from "./resolve";
import { previewVideoPool } from "@/services/video-cache/preview-video";
import { recordPreviewSubmission } from "@/diagnostics/browser-performance";
import {
	measureSpanAsync,
	measureSpanSync,
	onRenderPerfFrameComplete,
} from "@/diagnostics/render-perf";

export type CanvasRendererParams = {
	width: number;
	height: number;
	fps: FrameRate;
	isPreview?: boolean;
};

let exportOwnsCompositor = false;
let compositorEpoch = 0;
let previewRenders = 0;
let previewIdle: (() => void) | null = null;

export async function beginExportRender() {
	if (exportOwnsCompositor) throw new Error("Đã có tác vụ xuất video đang chạy");
	exportOwnsCompositor = true;
	if (previewRenders > 0) await new Promise<void>(resolve => { previewIdle = resolve; });
	return () => { exportOwnsCompositor = false; compositorEpoch++; };
}

export function getCompositorEpoch() { return compositorEpoch; }

export class CanvasRenderer {
	canvas: OffscreenCanvas;
	context: OffscreenCanvasRenderingContext2D;
	width: number;
	height: number;
	fps: FrameRate;
	isPreview: boolean;
	playing = false;
	private disposed = false;
	activate() { this.disposed = false; }
	dispose() { this.disposed = true; }

	constructor({ width, height, fps, isPreview = false }: CanvasRendererParams) {
		this.isPreview = isPreview;
		this.width = width;
		this.height = height;
		this.fps = fps;

		const surface = createCanvasSurface({ width, height });
		this.canvas = surface.canvas;
		this.context = surface.context;
	}

	getOutputCanvas(): HTMLCanvasElement {
		wasmCompositor.ensureInitialized({
			width: this.width,
			height: this.height,
		});
		return wasmCompositor.getCanvas();
	}

	getBackend() { return wasmCompositor.getBackend(); }

	setSize({ width, height }: { width: number; height: number }) {
		this.width = width;
		this.height = height;

		const surface = createCanvasSurface({ width, height });
		this.canvas = surface.canvas;
		this.context = surface.context;
	}

	async render({ node, time, targetTexture }: { node: AnyBaseNode; time: number; targetTexture?: GPUTexture }) {
		if (this.disposed) return;
		if (this.isPreview && exportOwnsCompositor) return;
		if (this.isPreview) previewRenders++;
		if (this.isPreview) previewVideoPool.beginFrame();
		try {
		await measureSpanAsync({
			name: "resolve",
			fn: () => resolveRenderTree({ node, renderer: this, time }),
		});
		if (this.disposed) return;
		const { frame, textures } = await measureSpanAsync({
			name: "buildFrame",
			fn: () => buildFrameDescriptor({ node, renderer: this }),
		});
		if (this.disposed) return;
		wasmCompositor.ensureInitialized({
			width: this.width,
			height: this.height,
		});
		measureSpanSync({
			name: "syncTextures",
			fn: () => wasmCompositor.syncTextures(textures),
		});
		measureSpanSync({
			name: "renderFrame",
			fn: () => wasmCompositor.render(frame, targetTexture),
		});
		if (this.isPreview) recordPreviewSubmission();
		} finally {
			if (this.isPreview) previewVideoPool.endFrame();
			if (this.isPreview && --previewRenders === 0) { previewIdle?.(); previewIdle = null; }
		}
	}

	async renderToCanvas({
		node,
		time,
		targetCanvas,
	}: {
		node: AnyBaseNode;
		time: number;
		targetCanvas: HTMLCanvasElement;
	}) {
		await this.render({ node, time });

		const ctx = targetCanvas.getContext("2d");
		if (!ctx) {
			throw new Error("Failed to get target canvas context");
		}

		measureSpanSync({
			name: "drawImage",
			fn: () =>
				ctx.drawImage(
					wasmCompositor.getCanvas(),
					0,
					0,
					targetCanvas.width,
					targetCanvas.height,
				),
		});
		onRenderPerfFrameComplete();
	}
}

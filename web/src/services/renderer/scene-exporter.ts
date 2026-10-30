import EventEmitter from "eventemitter3";

import {
	Output,
	Mp4OutputFormat,
	WebMOutputFormat,
	BufferTarget,
	CanvasSource,
	VideoSample,
	VideoSampleSource,
	AudioBufferSource,
	QUALITY_LOW,
	QUALITY_MEDIUM,
	QUALITY_HIGH,
	QUALITY_VERY_HIGH,
} from "mediabunny";
import type { FrameRate } from "opencut-wasm";
import { frameRateToFloat } from "@/fps/utils";
import { getExportFrameCount, getExportFrameSample } from "./frame-count";
import type { RootNode } from "./nodes/root-node";
import type { ExportFormat, ExportQuality } from "@/export";
import { CanvasRenderer } from "./canvas-renderer";
import { beginExportRender } from "./canvas-renderer";
import { initializeGpu } from "opencut-wasm";
import { webGpuEngine } from "@/native/webgpu-engine";
import { resolveExportRange, type ExportRange } from "@/export/range";

type ExportParams = {
	width: number;
	height: number;
	fps: FrameRate;
	format: ExportFormat;
	quality: ExportQuality;
	shouldIncludeAudio?: boolean;
	audioBuffer?: AudioBuffer;
};

const qualityMap = {
	low: QUALITY_LOW,
	medium: QUALITY_MEDIUM,
	high: QUALITY_HIGH,
	very_high: QUALITY_VERY_HIGH,
};

export type SceneExporterEvents = {
	progress: [progress: number];
	complete: [buffer: ArrayBuffer];
	error: [error: Error];
	cancelled: [];
};

export class SceneExporter extends EventEmitter<SceneExporterEvents> {
	private renderer: CanvasRenderer;
	private format: ExportFormat;
	private quality: ExportQuality;
	private shouldIncludeAudio: boolean;
	private audioBuffer?: AudioBuffer;

	private isCancelled = false;

	constructor({
		width,
		height,
		fps,
		format,
		quality,
		shouldIncludeAudio,
		audioBuffer,
	}: ExportParams) {
		super();
		this.renderer = new CanvasRenderer({
			width,
			height,
			fps,
		});

		this.format = format;
		this.quality = quality;
		this.shouldIncludeAudio = shouldIncludeAudio ?? false;
		this.audioBuffer = audioBuffer;
	}

	cancel(): void {
		this.isCancelled = true;
	}

	async export({
		rootNode,
		range,
	}: {
		rootNode: RootNode;
		range?: ExportRange;
	}): Promise<ArrayBuffer | null> {
		if (this.isCancelled) {
			this.emit("cancelled");
			return null;
		}
		const resolvedRange = resolveExportRange({ duration: rootNode.duration, range });
		const releaseCompositor = await beginExportRender();
		try {
			await initializeGpu();
			this.renderer.getOutputCanvas();
			if ((globalThis as typeof globalThis & { __CATCHIM_BROWSER__?: boolean }).__CATCHIM_BROWSER__ &&
				(!webGpuEngine.isHardwareAdapter || this.renderer.getBackend() !== "webgpu")) {
				throw new Error("Không có GPU phần cứng WebGPU đang hoạt động để xuất video. Kiểm tra chrome://gpu và bật tăng tốc phần cứng.");
			}
			return await this.exportFrames({ rootNode, range: resolvedRange });
		} finally {
			releaseCompositor();
		}
	}

	private async exportFrames({ rootNode, range }: { rootNode: RootNode; range: ExportRange & { duration: number } }): Promise<ArrayBuffer | null> {
		const fps = this.renderer.fps;
		const fpsFloat = frameRateToFloat(fps);
		const frameCount = getExportFrameCount(range.duration, fps);
		if (frameCount === 0) throw new Error("No frames to export: invalid duration or frame rate");

		const outputFormat =
			this.format === "webm" ? new WebMOutputFormat() : new Mp4OutputFormat();

		const output = new Output({
			format: outputFormat,
			target: new BufferTarget(),
		});

		const encodingConfig = {
			codec: this.format === "webm" ? "vp9" : "avc",
			bitrate: qualityMap[this.quality],
			hardwareAcceleration: "prefer-hardware",
		} as const;
		// Do not sample the WebGPU swapchain canvas: some browsers expose its previously presented frame.
		const useGpuReadback = this.renderer.getBackend() === "webgpu" && !!webGpuEngine.device;
		const gpuPixelFormat = webGpuEngine.preferredFormat;
		if (useGpuReadback && gpuPixelFormat !== "rgba8unorm" && gpuPixelFormat !== "bgra8unorm") {
			throw new Error(`Không thể đọc frame WebGPU định dạng ${gpuPixelFormat} để xuất video`);
		}
		const gpuVideoSource = useGpuReadback ? new VideoSampleSource(encodingConfig) : null;
		const videoSource = gpuVideoSource ?? new CanvasSource(this.renderer.getOutputCanvas(), encodingConfig);

		output.addVideoTrack(videoSource, { frameRate: fpsFloat });

		let audioSource: AudioBufferSource | null = null;
		if (this.shouldIncludeAudio && this.audioBuffer) {
			let audioCodec: "aac" | "opus" = this.format === "webm" ? "opus" : "aac";

			if (audioCodec === "aac" && typeof AudioEncoder !== "undefined") {
				const { supported } = await AudioEncoder.isConfigSupported({
					codec: "mp4a.40.2",
					sampleRate: this.audioBuffer.sampleRate,
					numberOfChannels: this.audioBuffer.numberOfChannels,
					bitrate: 192000,
				});
				if (!supported) audioCodec = "opus";
			}

			audioSource = new AudioBufferSource({
				codec: audioCodec,
				bitrate: qualityMap[this.quality],
			});
			output.addAudioTrack(audioSource);
		}

		const exportTexture = useGpuReadback
			? webGpuEngine.acquireTexture({
				width: this.renderer.width,
				height: this.renderer.height,
				format: gpuPixelFormat,
				usage: GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_SRC,
			})
			: null;

		try {
		await output.start();

		if (audioSource && this.audioBuffer) {
			await audioSource.add(this.audioBuffer);
			audioSource.close();
		}

		for (let i = 0; i < frameCount; i++) {
			if (this.isCancelled) {
				await output.cancel();
				this.emit("cancelled");
				return null;
			}

			const sample = getExportFrameSample(range.duration, fps, i);
			if (!sample) throw new Error("Invalid export frame sample");
			await this.renderer.render({ node: rootNode, time: range.start + sample.timeTicks, targetTexture: exportTexture ?? undefined });
			if ((globalThis as typeof globalThis & { __CATCHIM_BROWSER__?: boolean }).__CATCHIM_BROWSER__ &&
				this.renderer.getBackend() !== "webgpu") {
				throw new Error("WebGPU ngừng hoạt động trong lúc xuất video");
			}
			if (this.isCancelled) {
				await output.cancel();
				this.emit("cancelled");
				return null;
			}
			if (exportTexture) {
				if (!gpuVideoSource) throw new Error("Video export source mismatch");
				const pixels = await webGpuEngine.readTexturePixels(exportTexture);
				const videoSample = new VideoSample(pixels, {
					format: gpuPixelFormat === "bgra8unorm" ? "BGRA" : "RGBA",
					codedWidth: this.renderer.width,
					codedHeight: this.renderer.height,
					timestamp: sample.timestampSeconds,
					duration: sample.durationSeconds,
				});
				try {
					await gpuVideoSource.add(videoSample);
				} finally {
					videoSample.close();
				}
			} else {
				if (!(videoSource instanceof CanvasSource)) throw new Error("Canvas video export source mismatch");
				await videoSource.add(sample.timestampSeconds, sample.durationSeconds);
			}

			this.emit("progress", i / frameCount);
		}

		if (this.isCancelled) {
			await output.cancel();
			this.emit("cancelled");
			return null;
		}

		videoSource.close();
		await output.finalize();
		if (this.isCancelled) {
			this.emit("cancelled");
			return null;
		}
		this.emit("progress", 1);

		const buffer = output.target.buffer;
		if (!buffer) {
			this.emit("error", new Error("Failed to export video"));
			return null;
		}

		this.emit("complete", buffer);
		return buffer;
		} catch (error) {
			await output.cancel().catch(() => {});
			throw error;
		} finally {
			if (exportTexture) webGpuEngine.releaseTexture(exportTexture);
		}
	}
}

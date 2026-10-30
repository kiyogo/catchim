import type { EditorCore } from "@/core";
import type { RootNode } from "@/services/renderer/nodes/root-node";
import {
	resolveExportCanvasSize,
	type ExportOptions,
	type ExportResult,
} from "@/export";
import { CanvasRenderer } from "@/services/renderer/canvas-renderer";
import { SceneExporter } from "@/services/renderer/scene-exporter";
import { buildScene } from "@/services/renderer/scene-builder";
import { createTimelineAudioBuffer } from "@/media/audio";
import { isAudioAbortError, throwIfAudioAborted } from "@/media/audio-cancellation";
import { formatTimecode } from "opencut-wasm";
import { downloadBlob } from "@/utils/browser";
import { resolveExportRange } from "@/export/range";
import { encodeAudioBufferWav } from "@/export/wav";
import { getMissingExportSources, getUnavailableExportEffects } from "@/export/preflight";
import { initRustEngine } from "@/native/rust-engine";

type SnapshotResult =
	| { success: true; blob: Blob; filename: string }
	| { success: false; error: string };

export class RendererManager {
	private renderTree: RootNode | null = null;
	private _isDegraded = false;
	private listeners = new Set<() => void>();

	constructor(private editor: EditorCore) {}

	get isDegraded(): boolean {
		return this._isDegraded;
	}

	setDegraded(degraded: boolean): void {
		if (this._isDegraded === degraded) return;
		this._isDegraded = degraded;
		this.notify();
	}

	setRenderTree({ renderTree }: { renderTree: RootNode | null }): void {
		this.renderTree = renderTree;
		this.notify();
	}

	getRenderTree(): RootNode | null {
		return this.renderTree;
	}

	async saveSnapshot(): Promise<{ success: boolean; error?: string }> {
		const snapshot = await this.createSnapshot();
		if (!snapshot.success) {
			return snapshot;
		}

		downloadBlob({ blob: snapshot.blob, filename: snapshot.filename });
		return { success: true };
	}

	async copySnapshot(): Promise<{ success: boolean; error?: string }> {
		if (typeof ClipboardItem === "undefined" || !navigator.clipboard?.write) {
			return {
				success: false,
				error: "Clipboard image copy is not supported in this browser",
			};
		}

		const snapshot = await this.createSnapshot();
		if (!snapshot.success) {
			return snapshot;
		}

		try {
			await navigator.clipboard.write([
				new ClipboardItem({
					[snapshot.blob.type || "image/png"]: snapshot.blob,
				}),
			]);
			return { success: true };
		} catch (error) {
			console.error("Copy snapshot failed:", error);
			return {
				success: false,
				error: error instanceof Error ? error.message : "Unknown error",
			};
		}
	}

	private async createSnapshot(): Promise<SnapshotResult> {
		try {
			const renderTree = this.getRenderTree();
			const activeProject = this.editor.project.getActive();

			if (!renderTree || !activeProject) {
				return { success: false, error: "No project or scene to capture" };
			}

			const duration = this.editor.timeline.getTotalDuration();
			if (duration === 0) {
				return { success: false, error: "Project is empty" };
			}

			const { canvasSize, fps } = activeProject.settings;
			const renderTime = Math.min(
				this.editor.playback.getCurrentTime(),
				this.editor.timeline.getLastFrameTime(),
			);

			const renderer = new CanvasRenderer({
				width: canvasSize.width,
				height: canvasSize.height,
				fps,
			});

			const tempCanvas = document.createElement("canvas");
			tempCanvas.width = canvasSize.width;
			tempCanvas.height = canvasSize.height;

			await renderer.renderToCanvas({
				node: renderTree,
				time: renderTime,
				targetCanvas: tempCanvas,
			});

			const blob = await new Promise<Blob | null>((resolve) => {
				tempCanvas.toBlob((result) => resolve(result), "image/png");
			});

			if (!blob) {
				return { success: false, error: "Failed to create image" };
			}

			const timecode = formatTimecode({ time: renderTime, rate: fps })!.replace(
				/:/g,
				"-",
			);
			const safeName =
				activeProject.metadata.name.replace(/[<>:"/\\|?*]/g, "-").trim() ||
				"snapshot";
			const filename = `${safeName}-${timecode}.png`;

			return { success: true, blob, filename };
		} catch (error) {
			console.error("Snapshot capture failed:", error);
			return {
				success: false,
				error: error instanceof Error ? error.message : "Unknown error",
			};
		}
	}

	async exportProject({
		options,
		onProgress,
		onCancel,
	}: {
		options: ExportOptions;
		onProgress?: ({ progress }: { progress: number }) => void;
		onCancel?: () => boolean;
	}): Promise<ExportResult> {
		const { format, quality, fps, includeAudio, resolution, audioOnly } = options;
		const abortController = new AbortController();
		let cancelled = false;
		let exporter: SceneExporter | null = null;
		const checkCancel = () => {
			if (onCancel?.()) {
				cancelled = true;
				abortController.abort();
				exporter?.cancel();
			}
		};
		const cancelInterval = setInterval(checkCancel, 100);

		try {
			const tracks = this.editor.scenes.getActiveScene().tracks;
			const mediaAssets = this.editor.media.getAssets();
			const activeProject = this.editor.project.getActive();

			if (!activeProject) {
				return { success: false, error: "No active project" };
			}

			const duration = this.editor.timeline.getTotalDuration();
			if (duration === 0) {
				return { success: false, error: "Project is empty" };
			}
			const range = resolveExportRange({ duration, range: options.range });
			const missingSources = getMissingExportSources({ tracks, mediaAssets, range, includeAudio: !!includeAudio || !!audioOnly, audioOnly: !!audioOnly });
			if (missingSources.length > 0) return { success: false, error: `Missing media source: ${missingSources.join(", ")}. Relink these files before exporting.` };
			checkCancel();
			if (cancelled) return { success: false, cancelled: true };

			const exportFps = fps ?? activeProject.settings.fps;
			const sourceCanvasSize = activeProject.settings.canvasSize;
			const canvasSize = resolveExportCanvasSize({
				sourceSize: sourceCanvasSize,
				resolution,
			});
			const coordinateScale = canvasSize.height / sourceCanvasSize.height;
			if (!audioOnly) {
				if ([...tracks.overlay, tracks.main].some((t) => t.elements.some((e) => "effects" in e && e.effects?.some((effect) => effect.generated)))) await initRustEngine();
				const unavailable = getUnavailableExportEffects({ tracks, range, ...canvasSize });
				if (unavailable.length) return { success: false, error: `Effects unavailable or over resource budget: ${unavailable.join(", ")}. Restore dependencies, adjust settings or disable these effects before exporting.` };
			}

			let audioBuffer: AudioBuffer | null = null;
			if (includeAudio || audioOnly) {
				onProgress?.({ progress: 0.05 });
				audioBuffer = await createTimelineAudioBuffer({
					tracks,
					mediaAssets,
					duration: range.duration,
					startTime: range.start,
					signal: abortController.signal,
				});
			}
			checkCancel();
			if (cancelled) return { success: false, cancelled: true };
			throwIfAudioAborted(abortController.signal);
			if (audioOnly) {
				if (!audioBuffer) return { success: false, error: "No audio is available in this timeline" };
				const buffer = await encodeAudioBufferWav({ audioBuffer, signal: abortController.signal });
				checkCancel();
				if (cancelled) return { success: false, cancelled: true };
				onProgress?.({ progress: 1 });
				return { success: true, buffer };
			}

			const scene = buildScene({
				tracks,
				mediaAssets,
				duration,
				canvasSize,
				coordinateScale,
				background: activeProject.settings.background,
			});

			exporter = new SceneExporter({
				width: canvasSize.width,
				height: canvasSize.height,
				fps: exportFps,
				format,
				quality,
				shouldIncludeAudio: !!includeAudio,
				audioBuffer: audioBuffer || undefined,
			});

			exporter.on("progress", (progress) => {
				const adjustedProgress = includeAudio
					? 0.05 + progress * 0.95
					: progress;
				onProgress?.({ progress: adjustedProgress });
			});

			const buffer = await exporter.export({ rootNode: scene, range });

			if (cancelled) {
				return { success: false, cancelled: true };
			}

			if (!buffer) {
				return { success: false, error: "Export failed to produce buffer" };
			}

			return {
				success: true,
				buffer,
			};
		} catch (error) {
			if (cancelled || isAudioAbortError(error)) {
				return { success: false, cancelled: true };
			}
			console.error("Export failed:", error);
			return {
				success: false,
				error: error instanceof Error ? error.message : "Unknown export error",
			};
		} finally {
			clearInterval(cancelInterval);
		}
	}

	subscribe(listener: () => void): () => void {
		this.listeners.add(listener);
		return () => this.listeners.delete(listener);
	}

	private notify(): void {
		this.listeners.forEach((fn) => {
			fn();
		});
	}
}

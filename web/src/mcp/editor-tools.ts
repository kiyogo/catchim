import type { EditorCore } from "@/core";
import {
	BatchCommand,
	ToggleTrackLockCommand,
	ToggleTrackMuteCommand,
	ToggleTrackVisibilityCommand,
} from "@/commands";
import { buildDefaultEffectInstance } from "@/effects";
import {
	buildElementFromMedia,
	buildTextElement,
	canElementHaveAudio,
	isVisualElement,
} from "@/timeline/element-utils";
import { isTrackLocked } from "@/timeline/track-capabilities";
import type { ElementRef, TimelineElement, TimelineTrack } from "@/timeline";
import { mediaTimeFromSeconds, TICKS_PER_SECOND } from "@/wasm";
import { parseToolInput, toolDetails, type EditorToolName } from "./contracts";
import { effectsRegistry, registerDefaultEffects } from "@/effects";
import { PREVIEW_PERMISSION, type McpPermission } from "@/mcp/edit-plan";
import { createTextPlan, createEffectPlan } from "./plan-builders";
import { compileGeneratedEffect } from "@/effects/generated";
import { initRustEngine } from "@/native/rust-engine";

function allTracks(editor: EditorCore): TimelineTrack[] {
	const tracks = editor.scenes.getActiveScene().tracks;
	return [...tracks.overlay, tracks.main, ...tracks.audio];
}
function allClips(editor: EditorCore) {
	return allTracks(editor).flatMap((track) =>
		track.elements.map((element) => ({ track, element })),
	);
}
function clipSnapshot(track: TimelineTrack, element: TimelineElement) {
	return {
		trackId: track.id,
		id: element.id,
		name: element.name,
		type: element.type,
		startSeconds: element.startTime / TICKS_PER_SECOND,
		durationSeconds: element.duration / TICKS_PER_SECOND,
		trimStartSeconds: element.trimStart / TICKS_PER_SECOND,
		trimEndSeconds: element.trimEnd / TICKS_PER_SECOND,
		sourceDurationSeconds:
			element.sourceDuration === undefined
				? undefined
				: element.sourceDuration / TICKS_PER_SECOND,
		mediaId: "mediaId" in element ? element.mediaId : undefined,
		params: element.params,
		retime: "retime" in element ? element.retime : undefined,
		hidden: "hidden" in element ? element.hidden : undefined,
		effects: isVisualElement(element) ? element.effects : undefined,
	};
}
function requireClips(
	editor: EditorCore,
	elements: ElementRef[],
	editable = true,
) {
	return elements.map((reference) => {
		const track = editor.timeline.getTrackById({ trackId: reference.trackId });
		const element = track?.elements.find(
			(element) => element.id === reference.elementId,
		);
		if (!track || !element)
			throw new Error(
				`Clip not found: ${reference.elementId}. Refresh the timeline snapshot.`,
			);
		if (editable && isTrackLocked(track))
			throw new Error(`Track is locked: ${track.name}`);
		return { track, element };
	});
}
function insertedClips(editor: EditorCore, before: Set<string>) {
	const added = allClips(editor).filter(
		({ element }) => !before.has(element.id),
	);
	if (!added.length)
		throw new Error(
			"The editor rejected placement. Check track compatibility, locks and collisions.",
		);
	return {
		clips: added.map(({ track, element }) => clipSnapshot(track, element)),
	};
}

/** Thin, typed orchestration only: timeline/source rules stay in existing commands and Rust/WASM. */
export async function executeEditorTool(
	editor: EditorCore,
	name: EditorToolName,
	input: unknown,
	authority: { permission: McpPermission; getPermission?: () => McpPermission; signal?: AbortSignal } = { permission: PREVIEW_PERMISSION },
): Promise<Record<string, unknown>> {
	const project = editor.project.getActiveOrNull();
	if (!project || editor.project.getIsLoading())
		throw new Error("Wait for a project to finish loading.");
	if (!toolDetails[name].readOnly && editor.timeline.isPreviewActive())
		throw new Error(
			"A human edit is in progress. Finish the active drag or property edit first.",
		);
	switch (name) {
		case "catchim_create_text_plan":
			return { plan: await createTextPlan(editor, parseToolInput(name, input), authority.signal) };
		case "catchim_create_effect_plan": {
			const value = parseToolInput(name, input);
			return { plan: await createEffectPlan(editor, value.definition, value.elements, authority.signal) };
		}
		case "catchim_validate_effect": {
			await initRustEngine();
			const definition = compileGeneratedEffect(parseToolInput(name, input).definition);
			return { valid: true, type: definition.type, name: definition.name, parameters: definition.params, backend: "builtin-passes", arbitraryWgsl: false, maxNodes: 8, maxPasses: 32, maxPixels: 16777216 };
		}
		case "catchim_get_context":
			parseToolInput(name, input);
			return editor.mcp.getContext();
		case "catchim_preview_plan":
			return editor.mcp.previewPlan(parseToolInput(name, input).plan, authority.permission, authority.signal);
		case "catchim_apply_plan":
			return editor.mcp.applyPlan(parseToolInput(name, input).plan, authority.getPermission ?? authority.permission, authority.signal);
		case "catchim_get_edit_history":
			parseToolInput(name, input);
			return { history: editor.mcp.getHistory() };
		case "catchim_list_effects":
			parseToolInput(name, input);
			registerDefaultEffects();
			return { effects: effectsRegistry.getAll().map(({ type, name, category, params }) => ({ type, name, category, params: params.map(({ channels: _channels, ...rest }) => rest) })) };
		case "catchim_get_project": {
			parseToolInput(name, input);
			return {
				project: {
					id: project.metadata.id,
					name: project.metadata.name,
					settings: project.settings,
					scenes: editor.scenes
						.getScenes()
						.map((scene) => ({
							id: scene.id,
							name: scene.name,
							isMain: scene.isMain,
						})),
					activeSceneId: editor.scenes.getActiveScene().id,
				},
				playheadSeconds: editor.playback.getCurrentTime() / TICKS_PER_SECOND,
				durationSeconds: editor.timeline.getTotalDuration() / TICKS_PER_SECOND,
				canUndo: editor.command.canUndo(),
				canRedo: editor.command.canRedo(),
				isDirty: editor.save.getIsDirty(),
				timeUnit: "seconds",
			};
		}
		case "catchim_list_media": {
			const { offset, limit } = parseToolInput(name, input);
			const assets = editor.media.getAssets();
			return {
				total: assets.length,
				offset,
				nextOffset: offset + limit < assets.length ? offset + limit : null,
				media: assets
					.slice(offset, offset + limit)
					.map((asset) => ({
						id: asset.id,
						name: asset.name,
						type: asset.type,
						durationSeconds: asset.duration,
						width: asset.width,
						height: asset.height,
						fps: asset.fps,
						hasAudio: asset.hasAudio,
						size: asset.file.size,
					})),
			};
		}
		case "catchim_get_timeline": {
			const { trackId, offset, limit } = parseToolInput(name, input);
			const tracks = allTracks(editor).filter(
				(track) => !trackId || track.id === trackId,
			);
			if (trackId && !tracks.length) throw new Error("Track not found");
			const clips = tracks.flatMap((track) =>
				track.elements.map((element) => clipSnapshot(track, element)),
			);
			return {
				sceneId: editor.scenes.getActiveScene().id,
				tracks: tracks.map((track) => ({
					id: track.id,
					name: track.name,
					type: track.type,
					isMain: track.id === editor.scenes.getActiveScene().tracks.main.id,
					locked: isTrackLocked(track),
					muted: "muted" in track ? track.muted : undefined,
					hidden: "hidden" in track ? track.hidden : undefined,
					elementCount: track.elements.length,
				})),
				total: clips.length,
				offset,
				nextOffset: offset + limit < clips.length ? offset + limit : null,
				clips: clips.slice(offset, offset + limit),
				timeUnit: "seconds",
			};
		}
		case "catchim_get_selection":
			parseToolInput(name, input);
			return { ...editor.selection.getSnapshot() };
		case "catchim_seek": {
			const value = parseToolInput(name, input);
			editor.playback.seek({
				time: mediaTimeFromSeconds({ seconds: value.seconds }),
			});
			return {
				playheadSeconds: editor.playback.getCurrentTime() / TICKS_PER_SECOND,
			};
		}
		case "catchim_select_elements": {
			const { elements } = parseToolInput(name, input);
			requireClips(editor, elements, false);
			editor.selection.setSelectedElements({ elements });
			return { selectedElements: editor.selection.getSelectedElements() };
		}
		case "catchim_add_text": {
			const value = parseToolInput(name, input);
			if (value.trackId) {
				const track = editor.timeline.getTrackById({ trackId: value.trackId });
				if (!track || track.type !== "text" || isTrackLocked(track))
					throw new Error("Choose an existing, unlocked text track");
			}
			const before = new Set(allClips(editor).map(({ element }) => element.id));
			const element = buildTextElement({
				raw: {
					name: value.content.slice(0, 60),
					duration: mediaTimeFromSeconds({ seconds: value.durationSeconds }),
					params: {
						content: value.content,
						...(value.fontSize !== undefined
							? { fontSize: value.fontSize }
							: {}),
						...(value.color ? { color: value.color } : {}),
					},
				},
				startTime: mediaTimeFromSeconds({ seconds: value.startSeconds }),
			});
			editor.timeline.insertElement({
				element,
				placement: value.trackId
					? { mode: "explicit", trackId: value.trackId }
					: { mode: "auto" },
			});
			return insertedClips(editor, before);
		}
		case "catchim_add_media": {
			const value = parseToolInput(name, input);
			const asset = editor.media
				.getAssets()
				.find((asset) => asset.id === value.mediaId);
			if (!asset)
				throw new Error("Media not found. Import it in the browser first.");
			if (value.trackId) {
				const track = editor.timeline.getTrackById({ trackId: value.trackId });
				if (
					!track ||
					isTrackLocked(track) ||
					track.type !== (asset.type === "audio" ? "audio" : "video")
				)
					throw new Error("Choose an existing, compatible unlocked track");
			}
			const durationSeconds =
				asset.type === "image" ? value.imageDurationSeconds : asset.duration;
			if (
				!durationSeconds ||
				!Number.isFinite(durationSeconds) ||
				durationSeconds <= 0
			)
				throw new Error("Media source has no valid duration");
			const before = new Set(allClips(editor).map(({ element }) => element.id));
			const element = buildElementFromMedia({
				mediaId: asset.id,
				mediaType: asset.type,
				name: asset.name,
				duration: mediaTimeFromSeconds({ seconds: durationSeconds }),
				startTime: mediaTimeFromSeconds({ seconds: value.startSeconds }),
			});
			editor.timeline.insertElement({
				element,
				placement: value.trackId
					? { mode: "explicit", trackId: value.trackId }
					: { mode: "auto" },
			});
			return insertedClips(editor, before);
		}
		case "catchim_update_clip": {
			const value = parseToolInput(name, input);
			const [{ element }] = requireClips(editor, [value]);
			if (value.content !== undefined && element.type !== "text")
				throw new Error("content is supported for text clips only");
			if (value.opacity !== undefined && !isVisualElement(element))
				throw new Error("opacity requires a visual clip");
			if (value.volumeDb !== undefined && !canElementHaveAudio(element))
				throw new Error("volumeDb requires an audio or video clip");
			const params = {
				...element.params,
				...(value.content !== undefined ? { content: value.content } : {}),
				...(value.opacity !== undefined ? { opacity: value.opacity } : {}),
				...(value.volumeDb !== undefined ? { volume: value.volumeDb } : {}),
			};
			editor.timeline.updateElements({
				updates: [
					{
						trackId: value.trackId,
						elementId: value.elementId,
						patch: {
							...(value.name !== undefined ? { name: value.name } : {}),
							...(value.startSeconds !== undefined
								? {
										startTime: mediaTimeFromSeconds({
											seconds: value.startSeconds,
										}),
									}
								: {}),
							params,
						},
					},
				],
			});
			const [current] = requireClips(editor, [value], false);
			const applied =
				(value.name === undefined || current.element.name === value.name) &&
				(value.startSeconds === undefined ||
					current.element.startTime ===
						mediaTimeFromSeconds({ seconds: value.startSeconds })) &&
				(value.content === undefined ||
					current.element.params.content === value.content) &&
				(value.opacity === undefined ||
					current.element.params.opacity === value.opacity) &&
				(value.volumeDb === undefined ||
					current.element.params.volume === value.volumeDb);
			return { applied, clip: clipSnapshot(current.track, current.element) };
		}
		case "catchim_split_clip": {
			const value = parseToolInput(name, input);
			const [{ element }] = requireClips(editor, [value]);
			const splitTime = mediaTimeFromSeconds({ seconds: value.seconds });
			if (
				splitTime <= element.startTime ||
				splitTime >= element.startTime + element.duration
			)
				throw new Error("Split time must be strictly inside the clip");
			const right = editor.timeline.splitElements({
				elements: [{ trackId: value.trackId, elementId: value.elementId }],
				splitTime,
			});
			if (!right.length) throw new Error("The editor rejected this split");
			return {
				rightClips: requireClips(editor, right, false).map(
					({ track, element }) => clipSnapshot(track, element),
				),
			};
		}
		case "catchim_duplicate_clips": {
			const { elements } = parseToolInput(name, input);
			requireClips(editor, elements);
			const duplicated = editor.timeline.duplicateElements({ elements });
			if (!duplicated.length)
				throw new Error("The editor could not place the duplicated clips");
			return {
				clips: requireClips(editor, duplicated, false).map(
					({ track, element }) => clipSnapshot(track, element),
				),
			};
		}
		case "catchim_delete_clips": {
			const { elements } = parseToolInput(name, input);
			requireClips(editor, elements);
			editor.timeline.deleteElements({ elements });
			return {
				deleted: elements.filter(
					(reference) =>
						!editor.timeline.getElementsWithTracks({ elements: [reference] })
							.length,
				),
			};
		}
		case "catchim_set_track_state": {
			const value = parseToolInput(name, input);
			const track = editor.timeline.getTrackById({ trackId: value.trackId });
			if (!track) throw new Error("Track not found");
			if (value.muted !== undefined && !("muted" in track))
				throw new Error("This track does not support mute");
			if (value.hidden !== undefined && !("hidden" in track))
				throw new Error("This track does not support visibility");
			const commands = [
				...(value.locked !== undefined && isTrackLocked(track) !== value.locked
					? [new ToggleTrackLockCommand(track.id)]
					: []),
				...(value.muted !== undefined &&
				"muted" in track &&
				track.muted !== value.muted
					? [new ToggleTrackMuteCommand(track.id)]
					: []),
				...(value.hidden !== undefined &&
				"hidden" in track &&
				track.hidden !== value.hidden
					? [new ToggleTrackVisibilityCommand(track.id)]
					: []),
			];
			if (commands.length)
				editor.command.execute({ command: new BatchCommand(commands) });
			const current = editor.timeline.getTrackById({ trackId: track.id })!;
			return {
				trackId: current.id,
				locked: isTrackLocked(current),
				muted: "muted" in current ? current.muted : undefined,
				hidden: "hidden" in current ? current.hidden : undefined,
			};
		}
		case "catchim_close_track_gaps": {
			const value = parseToolInput(name, input);
			const track = editor.timeline.getTrackById(value);
			if (!track || isTrackLocked(track))
				throw new Error("Choose an existing unlocked track");
			editor.timeline.closeTrackGaps(value);
			return {
				clips: editor.timeline
					.getTrackById(value)!
					.elements.map((element) => clipSnapshot(track, element)),
			};
		}
		case "catchim_set_clip_color": {
			const { trackId, elementId, ...params } = parseToolInput(name, input);
			const [{ element }] = requireClips(editor, [{ trackId, elementId }]);
			if (!isVisualElement(element))
				throw new Error("Color grading requires a visual clip");
			const existing = element.effects?.find(
				(effect) => effect.type === "color-grading",
			);
			const effect =
				existing ?? buildDefaultEffectInstance({ effectType: "color-grading" });
			const updated = {
				...effect,
				enabled: true,
				params: { ...effect.params, ...params },
			};
			const effects = existing
				? element.effects!.map((effect) =>
						effect.id === existing.id ? updated : effect,
					)
				: [...(element.effects ?? []), updated];
			editor.timeline.updateElements({
				updates: [{ trackId, elementId, patch: { effects } }],
			});
			const [current] = requireClips(editor, [{ trackId, elementId }], false);
			return { clip: clipSnapshot(current.track, current.element) };
		}
		case "catchim_history": {
			const { action } = parseToolInput(name, input);
			const available =
				action === "undo" ? editor.command.canUndo() : editor.command.canRedo();
			if (!available) throw new Error(`Nothing to ${action}`);
			editor.command[action]();
			return {
				action,
				canUndo: editor.command.canUndo(),
				canRedo: editor.command.canRedo(),
			};
		}
		case "catchim_save_project": {
			parseToolInput(name, input);
			await editor.project.saveCurrentProject({ throwOnError: true });
			return { saved: true, projectId: project.metadata.id };
		}
	}
}

import type { MediaAsset } from "@/media/types";
import type { TProjectSettings } from "@/project/types";
import type { SceneTracks, TimelineElement, TimelineTrack } from "@/timeline/types";
import { findTrackInSceneTracks, updateElementInSceneTracks, updateTrackInSceneTracks } from "@/timeline/track-element-update";
import { buildElementFromMedia, buildTextElement, isVisualElement } from "@/timeline/element-utils";
import { applyElementUpdate } from "@/timeline/update-pipeline";
import { applyPlacement, resolveTrackPlacement } from "@/timeline/placement";
import { canPlaceTimeSpansOnTrack } from "@/timeline/placement/overlap";
import { isTrackLocked } from "@/timeline/track-capabilities";
import { resolveAnimationTarget } from "@/timeline/animation-targets";
import { upsertPathKeyframe } from "@/animation";
import { buildDefaultEffectInstance, effectsRegistry, registerDefaultEffects } from "@/effects";
import { coerceParamValue, type ParamDefinition, type ParamValues } from "@/params";
import { getElementParam } from "@/params/registry";
import { mediaTimeFromSeconds, mediaTime, type MediaTime } from "@/wasm";
import { AgentError, type AgentPermission, type AgentScope, type EditPlan } from "./contracts";

export function orderedTracks(tracks: SceneTracks): TimelineTrack[] {
	return [...tracks.overlay, tracks.main, ...tracks.audio];
}
function invalid(message: string): never { throw new AgentError("INVALID_PLAN", message); }
function denied(message: string): never { throw new AgentError("PERMISSION_DENIED", message); }

function checkedParams(values: ParamValues, find: (key: string) => ParamDefinition | undefined): ParamValues {
	const result: ParamValues = {};
	for (const [key, value] of Object.entries(values)) {
		if (["__proto__", "constructor", "prototype"].includes(key)) invalid("Invalid parameter key");
		const definition = find(key);
		if (!definition) invalid(`Unsupported parameter: ${key}`);
		const next = coerceParamValue({ param: definition, value });
		if (next === null) invalid(`Invalid value for parameter: ${key}`);
		// Do not silently accept a requested value outside the advertised range.
		if (definition.type === "number" && typeof value === "number" && (value < definition.min || (definition.max !== undefined && value > definition.max))) invalid(`Parameter out of range: ${key}`);
		if (definition.type === "color" && (typeof value !== "string" || !/^#[\da-f]{6}([\da-f]{2})?$/i.test(value))) invalid(`Invalid color: ${key}`);
		result[key] = next;
	}
	return result;
}

/** Detached projection; reuses normal placement, parameter, animation and Rust time adapters. */
export function projectEditPlan({ plan, tracks: original, settings, assets, permission, commit = false, signal }: {
	plan: EditPlan; tracks: SceneTracks; settings: TProjectSettings; assets: MediaAsset[];
	permission: AgentPermission; commit?: boolean; signal?: AbortSignal;
}) {
	if (permission.level < (commit ? 3 : 2) || !permission.scopes.includes("project:read")) denied("This permission level cannot preview/apply an edit plan");
	const toTime = (frame: number): MediaTime => mediaTimeFromSeconds({ seconds: frame * settings.fps.denominator / settings.fps.numerator });
	if (!Number.isFinite(settings.fps.numerator / settings.fps.denominator) || settings.fps.numerator <= 0 || settings.fps.denominator <= 0) invalid("Project frame rate is invalid");
	let tracks = original;
	const completed = new Set<string>();
	const changes: Array<{ operationId: string; type: string; trackId: string; elementId: string }> = [];
	const requiredPermissions = new Set<AgentScope>(["project:read"]);
	registerDefaultEffects();
	for (const entry of plan.operations) {
		if (signal?.aborted) throw new AgentError("CANCELLED", "Plan cancelled before commit");
		if (completed.has(entry.operationId) || entry.dependsOn.some((id) => !completed.has(id))) invalid("Operation IDs must be unique and dependencies must precede their dependants");
		completed.add(entry.operationId);
		const op = entry.operation;
		const required: AgentScope = op.type === "delete_clip" ? "clips:delete" : op.type.includes("effect") ? "effects:write" : "timeline:write";
		requiredPermissions.add(required);
		if (commit && !permission.scopes.includes(required)) denied(`Missing scope: ${required}`);
		let track = op.trackId ? findTrackInSceneTracks({ tracks, trackId: op.trackId }) : undefined;
		if (op.trackId && !track) invalid(`Track not found: ${op.trackId}`);
		if (track) {
			if (isTrackLocked(track)) denied(`Track is locked: ${track.id}`);
			if (!plan.targetScope.trackIds.includes(track.id) || (permission.trackIds && !permission.trackIds.includes(track.id))) denied(`Track outside approved scope: ${track.id}`);
		}
		const isInsertion = op.type === "add_text" || op.type === "add_media";
		if ((!isInsertion || plan.targetScope.elementIds.length) && !plan.targetScope.elementIds.includes(op.elementId)) denied(`Clip outside plan scope: ${op.elementId}`);
		if (permission.elementIds && !permission.elementIds.includes(op.elementId)) denied(`Clip outside approved scope: ${op.elementId}`);
		let before: TimelineElement | undefined;
		let after: TimelineElement;
		if (isInsertion) {
			if (orderedTracks(tracks).some((t) => t.elements.some((el) => el.id === op.elementId))) invalid(`Duplicate clip ID: ${op.elementId}`);
			const startTime = toTime(op.startFrame), duration = toTime(op.durationFrames);
			if (duration <= 0) invalid("Clip duration is less than one tick");
			if (op.type === "add_text") {
				after = { ...buildTextElement({ raw: { name: op.content.slice(0, 60), duration, params: { content: op.content, ...(op.color ? { color: op.color } : {}), ...(op.fontSize ? { fontSize: op.fontSize } : {}) } }, startTime }), id: op.elementId };
			} else {
				const asset = assets.find((a) => a.id === op.mediaId);
				if (!asset) invalid(`Imported media not found: ${op.mediaId}`);
				const sourceStart = toTime(op.sourceStartFrame);
				if (asset.type === "image" && sourceStart !== 0) invalid("Still images have no source offset");
				const sourceDuration = asset.type === "image" ? duration : mediaTimeFromSeconds({ seconds: asset.duration ?? 0 });
				if (sourceDuration <= 0 || sourceStart + duration > sourceDuration) invalid(`Media source range exceeded: ${asset.id}`);
				after = { ...buildElementFromMedia({ mediaId: asset.id, mediaType: asset.type, name: asset.name, duration, startTime }), id: op.elementId, sourceDuration, trimStart: sourceStart, trimEnd: mediaTime({ ticks: sourceDuration - sourceStart - duration }) };
			}
			if (!track && (!plan.targetScope.allowNewTracks || (commit && !permission.allowNewTracks))) denied("Creating tracks is outside the approved scope");
			const placement = resolveTrackPlacement({ tracks, elementType: after.type, timeSpans: [{ startTime, duration }], strategy: track ? { type: "explicit", trackId: track.id } : { type: "alwaysNew", position: "highest" } });
			if (!placement) invalid("No compatible placement for inserted clip");
			if (placement.kind === "existingTrack" && placement.adjustedStartTime !== undefined && placement.adjustedStartTime !== startTime) invalid("Placement would move the clip outside the requested frame");
			if (track && !canPlaceTimeSpansOnTrack({ track, timeSpans: [{ startTime, duration }] })) invalid("Inserted clip would overlap existing content");
			const placed = applyPlacement({ tracks, placementResult: placement, elements: [after] });
			if (!placed) invalid("Editor rejected placement");
			tracks = placed.updatedTracks;
			track = findTrackInSceneTracks({ tracks, trackId: placed.targetTrackId })!;
		} else {
			if (!track) invalid("Existing clip operations require a track");
			before = track.elements.find((el) => el.id === op.elementId);
			if (!before) invalid(`Clip not found: ${op.elementId}`);
			after = before;
			if ("mediaId" in before && !assets.some((a) => a.id === (before as { mediaId: string }).mediaId)) invalid(`Clip references missing media: ${op.elementId}`);
			switch (op.type) {
				case "delete_clip":
					tracks = updateTrackInSceneTracks({ tracks, trackId: track.id, update: (t) => ({ ...t, elements: t.elements.filter((el) => el.id !== op.elementId) }) });
					break;
				case "update_clip": {
					if (op.name === undefined && op.startFrame === undefined && !Object.keys(op.params ?? {}).length) invalid("Empty clip update");
					const values = checkedParams(op.params ?? {}, (key) => getElementParam({ element: before!, key }) ?? undefined);
					after = applyElementUpdate({ element: before, patch: { ...(op.name === undefined ? {} : { name: op.name }), ...(op.startFrame === undefined ? {} : { startTime: toTime(op.startFrame) }), params: values }, context: { tracks, trackId: track.id } });
					if (after.startTime !== before.startTime && !canPlaceTimeSpansOnTrack({ track, timeSpans: [{ startTime: after.startTime, duration: after.duration, excludeElementId: before.id }] })) invalid("Moved clip would overlap existing content");
					break;
				}
				case "keyframe": {
					const target = resolveAnimationTarget({ element: before, path: op.propertyPath });
					const time = toTime(op.frame);
					if (!target || time > before.duration || target.coerceValue({ value: op.value }) === null) invalid("Invalid keyframe target, value or time");
					const base = target.getBaseValue();
					if (base !== null && typeof base !== typeof op.value) invalid("Keyframe value type mismatch");
					const coerced = target.coerceValue({ value: op.value });
					if (typeof op.value === "number" && typeof coerced === "number" && Math.abs(coerced - op.value) > 1e-9) invalid("Keyframe value is outside the parameter range or step");
					if (typeof base === "string" && base.startsWith("#") && (typeof op.value !== "string" || !/^#[\da-f]{6}([\da-f]{2})?$/i.test(op.value))) invalid("Invalid keyframe color");
					if (typeof op.value !== "number" && op.interpolation !== "hold") invalid("Discrete keyframes require hold interpolation");
					after = { ...before, animations: upsertPathKeyframe({ animations: before.animations, propertyPath: op.propertyPath, time, value: op.value, interpolation: op.interpolation, keyframeId: entry.operationId, channelLayout: target.channelLayout, coerceValue: target.coerceValue }) };
					break;
				}
				default: {
					if (!isVisualElement(before)) invalid("Effects require a visual clip");
					const effects = before.effects ?? [];
					if (op.type === "add_effect") {
						if (effects.some((e) => e.id === op.effectId)) invalid("Duplicate effect ID");
						if (!effectsRegistry.has(op.effectType)) invalid(`Unsupported effect: ${op.effectType}`);
						if (effects.length >= 16) throw new AgentError("RESOURCE_LIMIT", "Maximum 16 effects per clip");
						const definition = effectsRegistry.get(op.effectType);
						const values = checkedParams(op.params, (key) => definition.params.find((p) => p.key === key));
						const instance = buildDefaultEffectInstance({ effectType: op.effectType });
						after = { ...before, effects: [...effects, { ...instance, id: op.effectId, params: { ...instance.params, ...values } }] };
					} else {
						const effect = effects.find((e) => e.id === op.effectId);
						if (!effect) invalid(`Effect not found: ${op.effectId}`);
						if (op.type === "remove_effect") after = { ...before, effects: effects.filter((e) => e.id !== op.effectId) };
						else {
							if (!effectsRegistry.has(effect.type)) invalid("Effect dependency is unavailable");
							const definition = effectsRegistry.get(effect.type);
							const values = checkedParams(op.params, (key) => definition.params.find((p) => p.key === key));
							after = { ...before, effects: effects.map((e) => e.id === op.effectId ? { ...e, enabled: op.enabled ?? e.enabled, params: { ...e.params, ...values } } : e) };
						}
					}
				}
			}
			if (op.type !== "delete_clip") tracks = updateElementInSceneTracks({ tracks, trackId: track.id, elementId: op.elementId, update: () => after });
		}
		// Authorize both old and new extents so a move cannot pull content across scope boundaries.
		if (permission.timeRange) {
			for (const element of [before, after]) {
				if (element && (element.startTime < toTime(permission.timeRange.startFrame) || element.startTime + element.duration > toTime(permission.timeRange.endFrame))) denied("Clip extent is outside the approved time range");
			}
		}
		changes.push({ operationId: entry.operationId, type: op.type, trackId: track!.id, elementId: op.elementId });
	}
	// Match the existing command reactor, without deleting an empty locked track.
	tracks = { ...tracks, overlay: tracks.overlay.filter((t) => t.elements.length || isTrackLocked(t)), audio: tracks.audio.filter((t) => t.elements.length || isTrackLocked(t)) };
	return { tracks, changes, requiredPermissions: [...requiredPermissions] };
}

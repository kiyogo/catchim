import type { EditorCore } from "@/core";
import { editPlanSchema, McpEditError, type EditPlan } from "./edit-plan";
import { generatedEffectSchema } from "@/effects/generated-schema";
import type { ElementRef } from "@/timeline/types";
import { generateUUID } from "@/utils/id";

/** Deterministic MCP builders; natural-language planning belongs to the external MCP client. */
export async function createEffectPlan(editor: EditorCore, definition: unknown, elements: ElementRef[], signal?: AbortSignal): Promise<EditPlan> {
	if (!elements.length || elements.length > 100) throw new McpEditError("INVALID_PLAN", "Select 1–100 visual clips");
	const parsed = generatedEffectSchema.parse(definition);
	const context = await editor.mcp.getContext();
	const plan = editPlanSchema.parse({
		version: 1, planId: generateUUID(), projectId: context.projectId, sceneId: context.sceneId, expectedRevision: context.revision,
		createdAt: new Date().toISOString(), userRequest: `Create editable effect: ${parsed.name}`,
		targetScope: { trackIds: [...new Set(elements.map((r) => r.trackId))], elementIds: [...new Set(elements.map((r) => r.elementId))], allowNewTracks: false },
		operations: elements.map((ref) => ({ operationId: generateUUID(), dependsOn: [], operation: { type: "create_effect", ...ref, effectId: generateUUID(), definition: parsed, params: {} } })),
		transactionGrouping: "atomic", rollbackStrategy: "restore_snapshot",
	});
	await editor.mcp.previewPlan(plan, undefined, signal);
	return plan;
}

export async function createTextPlan(editor: EditorCore, input: { content: string; startFrame: number; durationFrames: number }, signal?: AbortSignal): Promise<EditPlan> {
	const context = await editor.mcp.getContext();
	const elementId = generateUUID();
	const newTrackId = generateUUID();
	const plan = editPlanSchema.parse({
		version: 1, planId: generateUUID(), projectId: context.projectId, sceneId: context.sceneId, expectedRevision: context.revision,
		createdAt: new Date().toISOString(), userRequest: `Create text: ${input.content.slice(0, 200)}`,
		targetScope: { trackIds: [newTrackId], elementIds: [elementId], allowNewTracks: true },
		operations: [{ operationId: generateUUID(), dependsOn: [], operation: { type: "add_text", elementId, newTrackId, ...input } }],
		transactionGrouping: "atomic", rollbackStrategy: "restore_snapshot",
	});
	await editor.mcp.previewPlan(plan, undefined, signal);
	return plan;
}

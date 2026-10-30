import { z } from "zod";
import { generatedEffectSchema } from "@/effects/generated-schema";

export const planId = z.string().min(1).max(200);
const frame = z.number().int().min(0).max(120_000_000);
const params = z.record(z.string().min(1).max(100), z.union([
	z.number().finite(), z.string().max(10000), z.boolean(),
])).refine((value) => Object.keys(value).length <= 64, "Too many parameters");
const target = { trackId: planId, elementId: planId };
const insertion = { elementId: planId, trackId: planId.optional(), newTrackId: planId.optional(), startFrame: frame, durationFrames: frame.positive() };

export const editOperationSchema = z.discriminatedUnion("type", [
	z.object({ type: z.literal("update_clip"), ...target, name: z.string().min(1).max(300).optional(), startFrame: frame.optional(), params: params.optional() }).strict(),
	z.object({ type: z.literal("delete_clip"), ...target }).strict(),
	z.object({ type: z.literal("add_text"), ...insertion, content: z.string().min(1).max(10000), color: z.string().regex(/^#[\da-fA-F]{6}$/).optional(), fontSize: z.number().min(1).max(1000).optional() }).strict(),
	z.object({ type: z.literal("add_media"), ...insertion, mediaId: planId, sourceStartFrame: frame.default(0) }).strict(),
	z.object({ type: z.literal("add_effect"), ...target, effectId: planId, effectType: planId, params: params.default({}) }).strict(),
	z.object({ type: z.literal("create_effect"), ...target, effectId: planId, definition: generatedEffectSchema, params: params.default({}) }).strict(),
	z.object({ type: z.literal("update_effect"), ...target, effectId: planId, params, enabled: z.boolean().optional() }).strict(),
	z.object({ type: z.literal("remove_effect"), ...target, effectId: planId }).strict(),
	z.object({ type: z.literal("keyframe"), ...target, propertyPath: z.string().min(1).max(300), frame, value: z.union([z.number().finite(), z.string().max(10000), z.boolean()]), interpolation: z.enum(["linear", "hold", "bezier"]).default("linear") }).strict(),
]);

export const editPlanSchema = z.object({
	version: z.literal(1),
	planId: planId,
	projectId: planId,
	sceneId: planId,
	expectedRevision: z.string().regex(/^[a-f0-9]{64}$/),
	createdAt: z.string().datetime(),
	userRequest: z.string().min(1).max(10000),
	targetScope: z.object({ trackIds: z.array(planId).max(100), elementIds: z.array(planId).max(500), allowNewTracks: z.boolean() }).strict(),
	operations: z.array(z.object({
		operationId: planId,
		dependsOn: z.array(planId).max(100).default([]),
		operation: editOperationSchema,
	}).strict()).min(1).max(100),
	transactionGrouping: z.literal("atomic"),
	rollbackStrategy: z.literal("restore_snapshot"),
}).strict();

export type EditPlan = z.infer<typeof editPlanSchema>;
export type EditOperation = z.infer<typeof editOperationSchema>;
export type McpScope = "project:read" | "timeline:write" | "effects:write" | "clips:delete";
/** Trusted host policy, never read from MCP tool arguments. No privilege escalation at levels 4/5. */
export interface McpPermission {
	level: 0 | 1 | 2 | 3 | 4 | 5;
	scopes: readonly McpScope[];
	trackIds?: readonly string[];
	elementIds?: readonly string[];
	timeRange?: { startFrame: number; endFrame: number };
	allowNewTracks?: boolean;
}
export const PREVIEW_PERMISSION: McpPermission = { level: 2, scopes: ["project:read"] };
export const EDIT_PERMISSION: McpPermission = { level: 3, scopes: ["project:read", "timeline:write", "effects:write", "clips:delete"], allowNewTracks: true };

export class McpEditError extends Error {
	constructor(public readonly code: "INVALID_PLAN" | "CONFLICT" | "PERMISSION_DENIED" | "CANCELLED" | "RESOURCE_LIMIT" | "UNSUPPORTED", message: string) {
		super(message);
		this.name = "McpEditError";
	}
}

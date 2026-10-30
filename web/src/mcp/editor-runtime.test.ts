import { describe, expect, it, vi } from "vitest";
import { McpEditorRuntime } from "./editor-runtime";
import { EDIT_PERMISSION, PREVIEW_PERMISSION, type EditPlan } from "./edit-plan";
import { createTextPlan, createEffectPlan } from "./plan-builders";
import { CommandManager } from "@/core/managers/commands";
import type { EditorCore } from "@/core";
import type { TProject } from "@/project/types";
import type { SceneTracks } from "@/timeline/types";
import type { EditorSelectionSnapshot } from "@/selection/editor-selection";
import { buildEmptyTrack } from "@/timeline/placement";
import { buildTextElement } from "@/timeline/element-utils";
import { mediaTimeFromSeconds } from "@/wasm";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { McpBroker } from "@/mcp/broker.server";
import { handleMcpRequest } from "@/mcp/server";
import { executeEditorTool } from "@/mcp/editor-tools";

// Node integration exercises the injected core managers; do not initialize browser services.
vi.mock("@/core", () => ({ EditorCore: { getInstance: () => { throw new Error("Use the injected test editor"); } } }));

function fixture() {
	const at = (seconds: number) => mediaTimeFromSeconds({ seconds });
	const text = { ...buildTextElement({ raw: { name: "Text", duration: at(5), params: { content: "Original" } }, startTime: at(0) }), id: "clip" };
	if (text.type !== "text") throw new Error("Expected a text fixture");
	let tracks: SceneTracks = { main: buildEmptyTrack({ id: "main", type: "video" }), audio: [], overlay: [{ ...buildEmptyTrack({ id: "text", type: "text" }), elements: [text] }] };
	let selection: EditorSelectionSnapshot = { selectedElements: [{ trackId: "text", elementId: "clip" }], selectedKeyframes: [], keyframeSelectionAnchor: null, selectedMaskPoints: null };
	const project: TProject = { metadata: { id: "project", name: "Test", duration: at(5), createdAt: new Date(), updatedAt: new Date() }, settings: { fps: { numerator: 30000, denominator: 1001 }, canvasSize: { width: 1920, height: 1080 }, background: { type: "color", color: "#000000" } }, scenes: [], currentSceneId: "scene", version: 1 };
	const scene = () => ({ id: "scene", name: "Scene", isMain: true, bookmarks: [], tracks, createdAt: new Date(), updatedAt: new Date() });
	const base = {
		project: { getActive: () => project, getActiveOrNull: () => project, getIsLoading: () => false },
		scenes: { getScenes: () => [scene()], getActiveScene: scene, getActiveSceneOrNull: scene },
		media: { getAssets: () => [] },
		playback: { getCurrentTime: () => at(1) },
		timeline: { isPreviewActive: () => false, updateTracks: (next: SceneTracks) => { tracks = next; } },
		selection: { getSnapshot: () => selection, getSelectedElements: () => selection.selectedElements, applySelectionPatch: ({ patch }: { patch: Partial<EditorSelectionSnapshot> }) => { selection = { ...selection, ...patch }; return selection; }, restoreSnapshot: ({ snapshot }: { snapshot: EditorSelectionSnapshot }) => { selection = snapshot; } },
	};
	const editor = base as unknown as EditorCore;
	Object.assign(editor, { command: new CommandManager(editor), mcp: new McpEditorRuntime(editor) });
	return { editor, project, base, tracks: () => tracks, setTracks: (next: SceneTracks) => { tracks = next; }, at };
}
async function planFor(editor: EditorCore): Promise<EditPlan> {
	const c = await editor.mcp.getContext();
	return { version: 1, planId: crypto.randomUUID(), projectId: c.projectId, sceneId: c.sceneId, expectedRevision: c.revision, createdAt: new Date().toISOString(), userRequest: "Change text", targetScope: { trackIds: ["text"], elementIds: ["clip"], allowNewTracks: false }, transactionGrouping: "atomic", rollbackStrategy: "restore_snapshot", operations: [{ operationId: "change", dependsOn: [], operation: { type: "update_clip", trackId: "text", elementId: "clip", params: { content: "Edited", opacity: 0.5 } } }] };
}

describe("MCP atomic edit plans with the real command history", () => {
	it("previews without mutation and applies editable text, effect and keyframes as one undo step", async () => {
		const h = fixture(); const plan = await planFor(h.editor); const before = h.tracks();
		plan.operations.push({ operationId: "effect", dependsOn: ["change"], operation: { type: "add_effect", trackId: "text", elementId: "clip", effectType: "color-grading", effectId: "grade", params: { brightness: 0.4 } } }, { operationId: "animate", dependsOn: ["effect"], operation: { type: "keyframe", trackId: "text", elementId: "clip", propertyPath: "effects.grade.params.brightness", frame: 30, value: 0.8, interpolation: "linear" } });
		const preview = await h.editor.mcp.previewPlan(plan);
		expect(preview.renderedPreview).toBe(false); expect(h.tracks()).toBe(before); expect(h.editor.command.canUndo()).toBe(false);
		await h.editor.mcp.applyPlan(plan, EDIT_PERMISSION);
		const after = h.tracks();
		expect(after.overlay[0].elements[0]).toMatchObject({ params: { content: "Edited", opacity: 0.5 }, effects: [{ id: "grade", params: { brightness: 0.4 } }], animations: { "effects.grade.params.brightness": { keys: [{ id: "animate", value: 0.8 }] } } });
		h.editor.command.undo(); expect(h.tracks()).toBe(before); expect(h.editor.command.canUndo()).toBe(false);
		h.editor.command.redo(); expect(h.tracks()).toBe(after);
		expect(await h.editor.mcp.applyPlan(plan, EDIT_PERMISSION)).toMatchObject({ alreadyCommitted: true });
	});
	it("does not change state or history when a later operation fails", async () => {
		const h = fixture(); const before = h.tracks(); const plan = await planFor(h.editor);
		plan.operations.push({ operationId: "bad", dependsOn: ["change"], operation: { type: "delete_clip", trackId: "text", elementId: "missing" } }); plan.targetScope.elementIds.push("missing");
		await expect(h.editor.mcp.applyPlan(plan, EDIT_PERMISSION)).rejects.toThrow("not found");
		expect(h.tracks()).toBe(before); expect(h.editor.command.canUndo()).toBe(false);
	});
	it("rolls back publication when a command reactor fails", async () => {
		const h = fixture(); const before = h.tracks(); const plan = await planFor(h.editor);
		h.editor.command.registerReactor(() => { throw new Error("reactor failure"); });
		await expect(h.editor.mcp.applyPlan(plan, EDIT_PERMISSION)).rejects.toThrow("reactor failure");
		expect(h.tracks()).toBe(before); expect(h.editor.command.canUndo()).toBe(false);
	});
	it("rejects a plan after manual edits, settings changes, or switching projects", async () => {
		for (const mutate of [
			(h: ReturnType<typeof fixture>) => h.setTracks({ ...h.tracks(), main: { ...h.tracks().main, name: "Human renamed" } }),
			(h: ReturnType<typeof fixture>) => { h.project.settings.fps = { numerator: 24, denominator: 1 }; },
			(h: ReturnType<typeof fixture>) => { h.project.metadata.id = "other"; },
		]) { const h = fixture(); const plan = await planFor(h.editor); mutate(h); await expect(h.editor.mcp.applyPlan(plan, EDIT_PERMISSION)).rejects.toMatchObject({ code: "CONFLICT" }); expect(h.editor.command.canUndo()).toBe(false); }
	});
	it("enforces host scopes, track locks and old/new extents, even for otherwise valid plans", async () => {
		const h = fixture(); const plan = await planFor(h.editor);
		for (const permission of [PREVIEW_PERMISSION, { ...EDIT_PERMISSION, scopes: ["project:read"] as const }, { ...EDIT_PERMISSION, trackIds: ["other"] }, { ...EDIT_PERMISSION, elementIds: ["other"] }, { ...EDIT_PERMISSION, timeRange: { startFrame: 0, endFrame: 30 } }]) await expect(h.editor.mcp.applyPlan(plan, permission)).rejects.toMatchObject({ code: "PERMISSION_DENIED" });
		h.tracks().overlay[0].locked = true;
		const lockedPlan = await planFor(h.editor);
		await expect(h.editor.mcp.applyPlan(lockedPlan, EDIT_PERMISSION)).rejects.toThrow("locked");
		expect(h.editor.command.canUndo()).toBe(false);
	});
	it("rejects cancelled, invalid, cyclic and ID-reused plans", async () => {
		const h = fixture(); const plan = await planFor(h.editor);
		await expect(h.editor.mcp.applyPlan(plan, EDIT_PERMISSION, AbortSignal.abort())).rejects.toMatchObject({ code: "CANCELLED" });
		await expect(h.editor.mcp.applyPlan({ ...plan, operations: [{ ...plan.operations[0], dependsOn: ["change"] }] }, EDIT_PERMISSION)).rejects.toThrow("dependencies");
		await h.editor.mcp.applyPlan(plan, EDIT_PERMISSION);
		await expect(h.editor.mcp.applyPlan({ ...plan, userRequest: "different" }, EDIT_PERMISSION)).rejects.toMatchObject({ code: "CONFLICT" });
	});
	it("commits concurrent identical requests at most once", async () => {
		const h = fixture(); const plan = await planFor(h.editor);
		const results = await Promise.allSettled([h.editor.mcp.applyPlan(plan, EDIT_PERMISSION), h.editor.mcp.applyPlan(plan, EDIT_PERMISSION)]);
		expect(results.some((r) => r.status === "fulfilled")).toBe(true);
		h.editor.command.undo(); expect(h.editor.command.canUndo()).toBe(false);
	});
	it("creates real editable text using a deterministic MCP plan builder", async () => {
		const h = fixture();
		const plan = await createTextPlan(h.editor, { content: "CATCHIM", startFrame: 0, durationFrames: 150 });
		await h.editor.mcp.applyPlan(plan, EDIT_PERMISSION);
		expect(h.tracks().overlay[0].elements[0]).toMatchObject({ type: "text", params: { content: "CATCHIM" } });
	});
	it("keeps new track IDs stable across preview and commit so dependent effects/keyframes are atomic", async () => {
		const h = fixture();
		const plan = await createTextPlan(h.editor, { content: "Atomic title", startFrame: 0, durationFrames: 150 });
		const insert = plan.operations[0];
		if (insert.operation.type !== "add_text" || !insert.operation.newTrackId) throw new Error("Missing stable track ID");
		const target = { trackId: insert.operation.newTrackId, elementId: insert.operation.elementId };
		plan.operations.push({ operationId: "add-grade", dependsOn: [insert.operationId], operation: { type: "add_effect", ...target, effectType: "color-grading", effectId: "grade", params: { brightness: 0.2 } } }, { operationId: "animate-title", dependsOn: ["add-grade"], operation: { type: "keyframe", ...target, propertyPath: "opacity", frame: 0, value: 0.5, interpolation: "linear" } });
		const first = await h.editor.mcp.previewPlan(plan);
		const second = await h.editor.mcp.previewPlan(plan);
		expect(first.changes.map((c) => c.trackId)).toEqual([target.trackId, target.trackId, target.trackId]);
		expect(second.changes).toEqual(first.changes);
		await h.editor.mcp.applyPlan(plan, EDIT_PERMISSION);
		expect(h.tracks().overlay[0]).toMatchObject({ id: target.trackId, elements: [{ id: target.elementId, effects: [{ id: "grade" }], animations: { opacity: { keys: [{ value: 0.5 }] } } }] });
		h.editor.command.undo(); expect(h.tracks().overlay).toHaveLength(1);
	});
	it("treats MCP proposals as untrusted data and refuses arbitrary code fields", async () => {
		const h = fixture(); const before = h.tracks();
		await expect(h.editor.mcp.previewPlan({ shell: "whoami" })).rejects.toMatchObject({ code: "INVALID_PLAN" });
		expect(h.tracks()).toBe(before);
	});
	it("executes a real editable plan through official MCP client, server, broker and core", async () => {
		const h = fixture(); const plan = await planFor(h.editor);
		const broker = new McpBroker();
		const connection = broker.create({ projectId: "project", projectName: "Test", allowEdits: true });
		const client = new Client({ name: "real-core-test", version: "1" });
		const relay = setInterval(() => {
			const job = broker.takeJob(connection);
			if (!job) return;
			void executeEditorTool(h.editor, job.name, job.input, { permission: EDIT_PERMISSION }).then(
				(data) => broker.complete(connection, job.id, { ok: true, data }),
				(error) => broker.complete(connection, job.id, { ok: false, error: error.message, code: error.code }),
			);
		}, 1);
		try {
			await client.connect(new StreamableHTTPClientTransport(new URL("http://localhost:3300/api/mcp"), { fetch: async (url, init) => handleMcpRequest(new Request(url, init), connection, broker) }));
			expect((await client.callTool({ name: "catchim_preview_plan", arguments: { plan } })).isError).not.toBe(true);
			expect(h.tracks().overlay[0].elements[0].params.content).toBe("Original");
			const committed = await client.callTool({ name: "catchim_apply_plan", arguments: { plan } });
			expect(committed.structuredContent).toMatchObject({ committed: true, undoSteps: 1 });
			expect(h.tracks().overlay[0].elements[0].params.content).toBe("Edited");
			broker.setAllowEdits(connection, false);
			expect((await client.callTool({ name: "catchim_apply_plan", arguments: { plan } })).isError).toBe(true);
			h.editor.command.undo(); expect(h.tracks().overlay[0].elements[0].params.content).toBe("Original");
		} finally { clearInterval(relay); broker.disconnect(connection); await client.close(); }
	});
	it("rechecks revoked authority after awaiting revision hashing", async () => {
		const h = fixture(); const plan = await planFor(h.editor);
		let checks = 0;
		await expect(h.editor.mcp.applyPlan(plan, () => ++checks < 3 ? EDIT_PERMISSION : PREVIEW_PERMISSION)).rejects.toMatchObject({ code: "PERMISSION_DENIED" });
		expect(h.editor.command.canUndo()).toBe(false);
	});
	it("inserts a self-contained generated effect, refines its existing ID and keyframes its exposed parameter", async () => {
		const h = fixture();
		const definition = { version: 1, id: "my-grade", name: "My grade", description: "Editable generated grade", createdBy: "ai", parameters: [{ key: "strength", label: "Strength", type: "number", default: 0, min: -1, max: 1, step: 0.01 }], nodes: [{ id: "grade", input: "source", effectType: "color-grading", bindings: { brightness: { parameter: "strength" } } }], output: "grade" };
		const plan = await createEffectPlan(h.editor, definition, [{ trackId: "text", elementId: "clip" }]);
		const op = plan.operations[0].operation;
		if (op.type !== "create_effect") throw new Error("Expected a generated effect operation");
		await h.editor.mcp.applyPlan(plan, EDIT_PERMISSION);
		const next = await planFor(h.editor);
		next.operations = [{ operationId: "refine", dependsOn: [], operation: { type: "update_effect", trackId: "text", elementId: "clip", effectId: op.effectId, params: { strength: 0.5 } } }, { operationId: "key", dependsOn: ["refine"], operation: { type: "keyframe", trackId: "text", elementId: "clip", propertyPath: `effects.${op.effectId}.params.strength`, frame: 30, value: 0.8, interpolation: "linear" } }];
		await h.editor.mcp.applyPlan(next, EDIT_PERMISSION);
		const element = h.tracks().overlay[0].elements[0];
		expect(element).toMatchObject({ effects: [{ id: op.effectId, type: "generated:my-grade", params: { strength: 0.5 }, generated: definition }] });
		expect(element.animations?.[`effects.${op.effectId}.params.strength`]).toMatchObject({ keys: [{ value: 0.8 }] });
		h.editor.command.undo();
		expect(h.tracks().overlay[0].elements[0]).toMatchObject({ effects: [{ id: op.effectId, params: { strength: 0 } }] });
		h.editor.command.undo(); expect(h.tracks().overlay[0].elements[0]).not.toHaveProperty("effects");
	});
});

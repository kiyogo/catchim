import type { EditorCore } from "@/core";
import { Command, type CommandResult } from "@/commands/base-command";
import { McpEditError, editPlanSchema, PREVIEW_PERMISSION, type McpPermission, type EditPlan } from "./edit-plan";
import { orderedTracks, projectEditPlan } from "./plan-projection";
import type { SceneTracks } from "@/timeline/types";
import { initRustEngine } from "@/native/rust-engine";

function signature(editor: EditorCore): string {
	const project = editor.project.getActiveOrNull();
	if (!project || editor.project.getIsLoading()) throw new McpEditError("CONFLICT", "Wait for the project to finish loading");
	return JSON.stringify({
		projectId: project.metadata.id,
		settings: project.settings,
		sceneId: editor.scenes.getActiveScene().id,
		scenes: editor.scenes.getScenes().map((scene) => ({ id: scene.id, name: scene.name, tracks: scene.tracks, bookmarks: scene.bookmarks })),
		assets: editor.media.getAssets().map((a) => ({ id: a.id, type: a.type, duration: a.duration, size: a.file.size, modified: a.file.lastModified, name: a.file.name })),
	}, (key, value) => key === "buffer" ? undefined : value);
}
async function digest(value: string) {
	const bytes = new TextEncoder().encode(value);
	if (bytes.length > 32 * 1024 * 1024) throw new McpEditError("RESOURCE_LIMIT", "MCP context exceeds 32 MiB");
	return Array.from(new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)), (n) => n.toString(16).padStart(2, "0")).join("");
}

class ApplyMcpPlanCommand extends Command {
	readonly appliesOwnRipple = true;
	readonly atomic = true;
	constructor(private editor: EditorCore, private before: SceneTracks, private after: SceneTracks) { super(); }

	execute(): CommandResult {
		this.editor.timeline.updateTracks(this.after);
		const surviving = new Set(orderedTracks(this.after).flatMap((t) => t.elements.map((e) => `${t.id}:${e.id}`)));
		return { selection: { selectedElements: this.editor.selection.getSelectedElements().filter((r) => surviving.has(`${r.trackId}:${r.elementId}`)), selectedKeyframes: [], keyframeSelectionAnchor: null, selectedMaskPoints: null } };
	}
	undo() { this.editor.timeline.updateTracks(this.before); }
}

export class McpEditorRuntime {
	private receipts = new Map<string, { fingerprint: string; receipt: Record<string, unknown> }>();
	private audit: Array<{ planId: string; time: string; status: "committed" | "rejected"; code?: string; operations: number }> = [];
	constructor(private editor: EditorCore) {}

	async getContext() {
		const raw = signature(this.editor);
		const revision = await digest(raw);
		if (raw !== signature(this.editor)) throw new McpEditError("CONFLICT", "Project changed while reading context; retry");
		const project = this.editor.project.getActive()!;
		const scene = this.editor.scenes.getActiveScene();
		return {
			projectId: project.metadata.id, sceneId: scene.id, revision,
			settings: project.settings, playheadTicks: this.editor.playback.getCurrentTime(),
			selection: this.editor.selection.getSnapshot(),
			tracks: orderedTracks(scene.tracks).map((t) => ({ id: t.id, name: t.name, type: t.type, locked: !!t.locked, elements: t.elements.map(({ id, name, type, startTime, duration, params, animations, ...rest }) => ({ id, name, type, startTime, duration, params, animations, ...("effects" in rest ? { effects: rest.effects } : {}) })) })),
			media: this.editor.media.getAssets().map((a) => ({ id: a.id, name: a.name, type: a.type, durationSeconds: a.duration, width: a.width, height: a.height })),
			timeUnit: "integer frames in operations; integer media ticks in context",
			capabilities: { operationTypes: ["add_text", "add_media", "update_clip", "delete_clip", "add_effect", "create_effect", "update_effect", "remove_effect", "keyframe"], generatedEffects: "bounded single-input graphs of implemented passes", arbitraryCode: false, arbitraryShaders: false, semanticMediaAnalysis: false, maxOperations: 100 },
		};
	}

	private async prepare(input: unknown, permission: McpPermission, commit: boolean, signal?: AbortSignal) {
		const parsed = editPlanSchema.safeParse(input);
		if (!parsed.success) throw new McpEditError("INVALID_PLAN", parsed.error.message);
		const plan = parsed.data;
		await initRustEngine();
		if (this.editor.timeline.isPreviewActive()) throw new McpEditError("CONFLICT", "Finish the active manual edit first");
		const raw = signature(this.editor);
		const revision = await digest(raw);
		if (plan.projectId !== this.editor.project.getActive().metadata.id || plan.sceneId !== this.editor.scenes.getActiveScene().id || plan.expectedRevision !== revision || raw !== signature(this.editor)) throw new McpEditError("CONFLICT", "Project changed since planning; create a new plan from current context");
		const before = this.editor.scenes.getActiveScene().tracks;
		let projected: ReturnType<typeof projectEditPlan>;
		try {
			projected = projectEditPlan({ plan, tracks: before, settings: this.editor.project.getActive().settings, assets: this.editor.media.getAssets(), permission, commit, signal });
		} catch (error) {
			if (error instanceof McpEditError) throw error;
			throw new McpEditError("INVALID_PLAN", error instanceof Error ? error.message : "Invalid edit plan");
		}
		return { plan, raw, revision, before, ...projected };
	}

	async previewPlan(input: unknown, permission = PREVIEW_PERMISSION, signal?: AbortSignal) {
		const prepared = await this.prepare(input, permission, false, signal);
		return { planId: prepared.plan.planId, revision: prepared.revision, validationStatus: "validated", requiredPermissions: prepared.requiredPermissions, changes: prepared.changes, tracks: prepared.tracks, previewKind: "timeline_diff", renderedPreview: false };
	}

	async applyPlan(input: unknown, authority: McpPermission | (() => McpPermission) = PREVIEW_PERMISSION, signal?: AbortSignal): Promise<Record<string, unknown>> {
		const permission = () => typeof authority === "function" ? authority() : authority;
		if (permission().level < 3) throw new McpEditError("PERMISSION_DENIED", "Edit permission is required");
		const parsed = editPlanSchema.safeParse(input);
		if (!parsed.success) throw new McpEditError("INVALID_PLAN", parsed.error.message);
		const plan = parsed.data;
		const key = `${plan.projectId}:${plan.planId}`;
		if (this.editor.project.getActiveOrNull()?.metadata.id !== plan.projectId) throw new McpEditError("CONFLICT", "The active project changed");
		const fingerprint = JSON.stringify(plan);
		const previous = this.receipts.get(key);
		if (previous) {
			if (previous.fingerprint !== fingerprint) throw new McpEditError("CONFLICT", "Plan ID was already used for different operations");
			return { ...previous.receipt, alreadyCommitted: true };
		}
		try {
			const prepared = await this.prepare(plan, permission(), true, signal);
			// No await from the last revision check through publication; browser edits cannot interleave.
			if (signal?.aborted) throw new McpEditError("CANCELLED", "Plan cancelled before commit");
			if (prepared.raw !== signature(this.editor)) throw new McpEditError("CONFLICT", "Project changed before commit");
			// Recheck host policy after asynchronous hashing; a revoked grant must never commit.
			projectEditPlan({ plan, tracks: prepared.before, settings: this.editor.project.getActive().settings, assets: this.editor.media.getAssets(), permission: permission(), commit: true, signal });
			// A concurrent duplicate may have completed while this call awaited SHA-256.
			const concurrent = this.receipts.get(key);
			if (concurrent) {
				if (concurrent.fingerprint !== fingerprint) throw new McpEditError("CONFLICT", "Plan ID was already used for different operations");
				return { ...concurrent.receipt, alreadyCommitted: true };
			}
			this.editor.command.execute({ command: new ApplyMcpPlanCommand(this.editor, prepared.before, prepared.tracks) });
			const receipt = { planId: plan.planId, committed: true, alreadyCommitted: false, changes: prepared.changes, undoSteps: 1 };
			this.receipts.set(key, { fingerprint, receipt });
			if (this.receipts.size > 100) this.receipts.delete(this.receipts.keys().next().value!);
			this.log(plan, "committed");
			return receipt;
		} catch (error) {
			this.log(plan, "rejected", error instanceof McpEditError ? error.code : "EXECUTION_ERROR");
			throw error;
		}
	}
	getHistory() { return this.audit.map((item) => ({ ...item })); }
	private log(plan: EditPlan, status: "committed" | "rejected", code?: string) {
		this.audit.push({ planId: plan.planId, time: new Date().toISOString(), status, code, operations: plan.operations.length });
		if (this.audit.length > 100) this.audit.shift();
	}
}

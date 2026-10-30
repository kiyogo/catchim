"use client";

import { useEffect, useRef, useState } from "react";
import { PanelView } from "@/components/editor/panels/assets/views/base-panel";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";
import { useEditor } from "@/editor/use-editor";
import { useTranslation } from "@/i18n";
import { editPlanSchema, type EditPlan } from "./edit-plan";
import { generatedEffectSchema } from "@/effects/generated-schema";
import { McpSettingsView } from "./settings-view";
import { useMcpConnection } from "./connection-store";
import { callEditorMcp } from "./browser-client";

/** The local workspace uses the same authenticated MCP protocol as external clients. */
export function McpWorkspace() {
	const { language } = useTranslation();
	const vi = language === "vi";
	const selected = useEditor((editor) => editor.selection.getSelectedElements());
	const projectId = useEditor((editor) => editor.project.getActive().metadata.id);
	const { connection, allowEdits } = useMcpConnection();
	const [content, setContent] = useState("CATCHIM");
	const [startFrame, setStartFrame] = useState("0");
	const [durationFrames, setDurationFrames] = useState("150");
	const [plan, setPlan] = useState<EditPlan | null>(null);
	const [draft, setDraft] = useState("");
	const [effectDraft, setEffectDraft] = useState("");
	const [context, setContext] = useState("");
	const [history, setHistory] = useState("");
	const [status, setStatus] = useState("");
	const [busy, setBusy] = useState(false);
	const [validated, setValidated] = useState(false);
	const [applied, setApplied] = useState(false);
	const controller = useRef<AbortController | null>(null);
	useEffect(() => () => controller.current?.abort(), []);
	useEffect(() => {
		controller.current?.abort();
		setPlan(null); setDraft(""); setContext(""); setHistory(""); setValidated(false); setApplied(false);
	}, [projectId, connection?.id]);
	async function run(task: (signal: AbortSignal) => Promise<void>) {
		const current = new AbortController();
		controller.current?.abort(); controller.current = current;
		setBusy(true); setStatus("");
		try { await task(current.signal); }
		catch (error) { setStatus(current.signal.aborted ? (vi ? "Đã hủy. Kiểm tra timeline trước khi thử lại thao tác đã gửi." : "Cancelled. Inspect the timeline before retrying a delivered edit.") : error instanceof Error ? error.message : String(error)); }
		finally { if (controller.current === current) setBusy(false); }
	}
	function acceptPlan(value: unknown) {
		const parsed = editPlanSchema.parse(value);
		setPlan(parsed); setDraft(JSON.stringify(parsed, null, 2)); setValidated(true); setApplied(false);
		setStatus(vi ? "Kế hoạch MCP hợp lệ. Duyệt các thao tác trước khi áp dụng." : "MCP plan validated. Review operations before applying.");
	}
	return (
		<PanelView title="MCP">
			<div className="flex flex-col gap-3 pb-4 text-xs">
				<p className="text-muted-foreground">{vi ? "Kết nối MCP để công cụ bên ngoài điều khiển Catchim. Mỗi kế hoạch là một bước Undo; clip, effect và keyframe vẫn chỉnh sửa được." : "Connect an external MCP client to control Catchim. Each plan applies as one Undo step; clips, effects and keyframes stay editable."}</p>
				<details open><summary className="cursor-pointer">{vi ? "Kết nối và quyền MCP" : "MCP connection and permissions"}</summary><McpSettingsView /></details>
				<p>{vi ? "Clip đang chọn: " : "Selected clips: "}{selected.length}</p>
				<div className="flex flex-wrap gap-2">
					<Button size="sm" variant="outline" disabled={busy || !connection} onClick={() => void run(async (signal) => { setContext(JSON.stringify(await callEditorMcp("catchim_get_context", {}, signal), null, 2)); })}>{vi ? "Đọc ngữ cảnh MCP" : "Read MCP context"}</Button>
					<Button size="sm" variant="outline" disabled={busy || !connection} onClick={() => void run(async (signal) => { setHistory(JSON.stringify(await callEditorMcp("catchim_get_edit_history", {}, signal), null, 2)); })}>{vi ? "Đọc lịch sử MCP" : "Read MCP history"}</Button>
				</div>
				{context && <details><summary>{vi ? "Ngữ cảnh dự án" : "Project context"}</summary><pre className="max-h-60 overflow-auto whitespace-pre-wrap break-all">{context}</pre></details>}
				{history && <details open><summary>{vi ? "Lịch sử transaction" : "Transaction history"}</summary><pre className="max-h-60 overflow-auto whitespace-pre-wrap break-all">{history}</pre></details>}
				<details>
					<summary className="cursor-pointer">{vi ? "Tạo kế hoạch chữ qua MCP" : "Create text plan through MCP"}</summary>
					<div className="mt-2 flex flex-col gap-2">
						<label htmlFor="mcp-text-content">{vi ? "Nội dung" : "Content"}</label><Textarea id="mcp-text-content" value={content} disabled={busy} maxLength={10000} onChange={(event) => setContent(event.target.value)} />
						<label htmlFor="mcp-start-frame">{vi ? "Frame bắt đầu" : "Start frame"}</label><Input id="mcp-start-frame" type="number" min={0} value={startFrame} disabled={busy} onChange={(event) => setStartFrame(event.target.value)} />
						<label htmlFor="mcp-duration-frames">{vi ? "Thời lượng (frame theo FPS dự án)" : "Duration (frames at project FPS)"}</label><Input id="mcp-duration-frames" type="number" min={1} value={durationFrames} disabled={busy} onChange={(event) => setDurationFrames(event.target.value)} />
						<Button size="sm" variant="outline" disabled={busy || !connection || !content.trim()} onClick={() => void run(async (signal) => { const result = await callEditorMcp("catchim_create_text_plan", { content, startFrame: Number(startFrame), durationFrames: Number(durationFrames) }, signal); acceptPlan(result.plan); })}>{vi ? "Lập kế hoạch chữ" : "Create text plan"}</Button>
					</div>
				</details>
				<details>
					<summary className="cursor-pointer">{vi ? "Tạo effect qua MCP" : "Create effect through MCP"}</summary>
					<p className="my-2 text-muted-foreground">{vi ? "Graph gồm blur, color-grading, vignette hoặc chroma-key. Tham số riêng dùng được trong inspector và keyframe. Chưa hỗ trợ WGSL tùy ý." : "Graphs compose blur, color-grading, vignette or chroma-key nodes. Exposed parameters work in the inspector and keyframes. Arbitrary WGSL is not supported yet."}</p>
					<label htmlFor="mcp-effect-json">Effect JSON</label><Textarea id="mcp-effect-json" value={effectDraft} disabled={busy} maxLength={50000} rows={8} className="my-2 font-mono text-xs" onChange={(event) => setEffectDraft(event.target.value)} />
					<Button size="sm" variant="outline" disabled={busy || !connection || !selected.length || !effectDraft.trim()} onClick={() => void run(async (signal) => {
						const definition = generatedEffectSchema.parse(JSON.parse(effectDraft));
						const result = await callEditorMcp("catchim_create_effect_plan", { definition, elements: selected }, signal); acceptPlan(result.plan);
					})}>{vi ? "Lập kế hoạch effect" : "Create effect plan"}</Button>
				</details>
				<label htmlFor="mcp-plan-json">{vi ? "Kế hoạch MCP JSON" : "MCP plan JSON"}</label><Textarea id="mcp-plan-json" value={draft} disabled={busy} rows={8} className="font-mono text-xs" onChange={(event) => { setDraft(event.target.value); setValidated(false); setApplied(false); }} />
				{plan && <><p className="text-muted-foreground">{vi ? "Đây là diff dữ liệu timeline. Preview video cập nhật sau khi áp dụng." : "This is a timeline data diff. Video preview updates after applying."}</p><ol className="list-decimal space-y-1 pl-5">{plan.operations.map(({ operationId, operation }) => <li key={operationId} className="break-all">{operation.type}: {operation.elementId}{"effectId" in operation ? ` / ${operation.effectId}` : ""}</li>)}</ol></>}
				<div className="flex flex-wrap gap-2">
					<Button size="sm" variant="outline" disabled={busy || !connection || !draft.trim()} onClick={() => void run(async (signal) => { const candidate = editPlanSchema.parse(JSON.parse(draft)); await callEditorMcp("catchim_preview_plan", { plan: candidate }, signal); acceptPlan(candidate); })}>{vi ? "Kiểm tra kế hoạch" : "Validate plan"}</Button>
					<Button size="sm" disabled={busy || !connection || !allowEdits || !validated || applied} onClick={() => void run(async (signal) => { await callEditorMcp("catchim_apply_plan", { plan: editPlanSchema.parse(JSON.parse(draft)) }, signal); setApplied(true); setStatus(vi ? "MCP đã áp dụng kế hoạch. Có thể chỉnh tiếp, lưu và xuất từ editor." : "MCP plan applied. Continue editing, saving and exporting in the editor."); })}>{vi ? "Áp dụng qua MCP" : "Apply through MCP"}</Button>
					{busy && <Button size="sm" variant="outline" onClick={() => controller.current?.abort()}>{vi ? "Hủy" : "Cancel"}</Button>}
				</div>
				<p role="status" className="break-words">{status}</p>
				<div className="flex gap-2">{(["undo", "redo"] as const).map((action) => <Button key={action} size="sm" variant="outline" disabled={busy || !connection || !allowEdits} onClick={() => void run(async (signal) => { await callEditorMcp("catchim_history", { action }, signal); setStatus(action === "undo" ? "Undo" : "Redo"); })}>{action === "undo" ? (vi ? "Hoàn tác bước cuối" : "Undo last edit") : "Redo"}</Button>)}</div>
			</div>
		</PanelView>
	);
}

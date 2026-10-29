"use client";

import { useEffect, useRef, useState } from "react";
import { PanelView } from "@/components/editor/panels/assets/views/base-panel";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";
import { useEditor } from "@/editor/use-editor";
import { useTranslation } from "@/i18n";
import { EDIT_PERMISSION, editPlanSchema, type EditPlan } from "./contracts";
import { createEditPlan } from "./orchestrator";
import { HttpPlanningProvider } from "./provider";

export function AgentPanel() {
	const editor = useEditor();
	const { language } = useTranslation();
	const vi = language === "vi";
	const [request, setRequest] = useState('Thêm chữ "CATCHIM" 5 giây');
	const [provider, setProvider] = useState("offline");
	const [endpoint, setEndpoint] = useState("http://localhost:11434/api/chat");
	const [model, setModel] = useState("");
	const [apiKey, setApiKey] = useState("");
	const [consent, setConsent] = useState(false);
	const [plan, setPlan] = useState<EditPlan | null>(null);
	const [draft, setDraft] = useState("");
	const [status, setStatus] = useState("");
	const [busy, setBusy] = useState(false);
	const [validated, setValidated] = useState(false);
	const [applied, setApplied] = useState(false);
	const [auditVersion, setAuditVersion] = useState(0);
	const controller = useRef<AbortController | null>(null);
	useEffect(() => () => controller.current?.abort(), []);
	const selected = useEditor((e) => e.selection.getSelectedElements());
	const projectId = useEditor((e) => e.project.getActive().metadata.id);
	useEffect(() => {
		controller.current?.abort();
		setPlan(null); setDraft(""); setValidated(false); setApplied(false);
	}, [projectId]);
	async function run(task: (signal: AbortSignal) => Promise<void>) {
		const current = new AbortController();
		controller.current?.abort(); controller.current = current;
		setBusy(true); setStatus("");
		try { await task(current.signal); }
		catch (error) { setStatus(current.signal.aborted ? (vi ? "Đã hủy." : "Cancelled.") : error instanceof Error ? error.message : String(error)); }
		finally { if (controller.current === current) { setBusy(false); setAuditVersion((n) => n + 1); } }
	}
	function readDraft() { return editPlanSchema.parse(JSON.parse(draft)); }
	const history = editor.agent.getHistory();
	return (
		<PanelView title="AI Agent">
			<div className="flex flex-col gap-3 pb-4 text-xs" data-audit-version={auditVersion}>
				<p className="text-muted-foreground">{vi ? "AI lập kế hoạch để bạn duyệt. Mỗi lần áp dụng là một bước Undo; media gốc được giữ lại." : "Review an edit plan before applying it as one Undo step. Original media is preserved."}</p>
				<p>{vi ? "Clip đang chọn: " : "Selected clips: "}{selected.length}</p>
				<details>
					<summary className="cursor-pointer">{vi ? "Cấu hình model" : "Model settings"}</summary>
					<div className="mt-2 flex flex-col gap-2">
						<label htmlFor="agent-provider">Provider</label>
						<select id="agent-provider" className="bg-background rounded-md border p-2" value={provider} disabled={busy} onChange={(e) => { setProvider(e.target.value); setConsent(false); setEndpoint(e.target.value === "ollama" ? "http://localhost:11434/api/chat" : ""); }}>
							<option value="offline">Offline</option><option value="ollama">Ollama</option><option value="openai-compatible">OpenAI-compatible / custom</option>
						</select>
						{provider !== "offline" && <>
							<label htmlFor="agent-endpoint">Endpoint</label><Input id="agent-endpoint" value={endpoint} disabled={busy} onChange={(e) => { setEndpoint(e.target.value); setConsent(false); }} placeholder={provider === "ollama" ? "http://localhost:11434/api/chat" : "https://provider.example/v1/chat/completions"} />
							<label htmlFor="agent-model">Model</label><Input id="agent-model" value={model} disabled={busy} onChange={(e) => setModel(e.target.value)} />
							<label htmlFor="agent-key">API key</label><Input id="agent-key" type="password" autoComplete="off" value={apiKey} disabled={busy} onChange={(e) => setApiKey(e.target.value)} />
							<label className="flex items-start gap-2"><input type="checkbox" checked={consent} disabled={busy} onChange={(e) => setConsent(e.target.checked)} />{vi ? "Cho phép gửi metadata dự án, nội dung chữ và yêu cầu đến endpoint này. Không gửi tệp media." : "Allow sending project metadata, text and requests to this endpoint. Media files are not sent."}</label>
							<p className="text-muted-foreground">{vi ? "Model phải hỗ trợ JSON; endpoint cần cho phép CORS từ Catchim. API key chỉ giữ trong bộ nhớ của panel." : "The model must support JSON; the endpoint must allow CORS from Catchim. The API key is kept only in panel memory."}</p>
						</>}
					</div>
				</details>
				<label htmlFor="agent-request">{vi ? "Yêu cầu chỉnh sửa" : "Editing request"}</label>
				<Textarea id="agent-request" value={request} disabled={busy} onChange={(e) => setRequest(e.target.value)} rows={3} maxLength={10000} />
				{provider === "offline" && <p className="text-muted-foreground">{vi ? 'Offline hiện hỗ trợ: Thêm chữ "Nội dung" 5 giây, tại playhead. Chọn model cho yêu cầu khác.' : 'Offline currently supports: Add text "Content" 5s at the playhead. Configure a model for other requests.'}</p>}
				<div className="flex flex-wrap gap-2">
					<Button size="sm" disabled={busy || !request.trim() || (provider !== "offline" && !consent)} onClick={() => void run(async (signal) => {
						setValidated(false); setApplied(false);
						const result = await createEditPlan(editor, request, { signal, previousPlan: plan ?? undefined, provider: provider === "offline" ? undefined : new HttpPlanningProvider({ kind: provider as "ollama" | "openai-compatible", endpoint, model, apiKey, allowContextTransfer: consent }) });
						signal.throwIfAborted(); setPlan(result); setDraft(JSON.stringify(result, null, 2)); setValidated(true);
						setStatus(vi ? "Kế hoạch hợp lệ. Duyệt các thao tác trước khi áp dụng." : "Plan validated. Review operations before applying.");
					})}>{busy ? (vi ? "Đang xử lý…" : "Working…") : (vi ? "Lập kế hoạch" : "Create plan")}</Button>
					{busy && <Button variant="outline" size="sm" onClick={() => controller.current?.abort()}>{vi ? "Hủy" : "Cancel"}</Button>}
				</div>
				{draft && <>
					<p className="font-medium">{vi ? "Thay đổi dự kiến trên timeline" : "Proposed timeline changes"}</p>
					<p className="text-muted-foreground">{vi ? "Đây là diff dữ liệu, chưa phải frame đã render. Preview video cập nhật sau khi áp dụng." : "This is a data diff, not rendered frames. Video preview updates after applying."}</p>
					<ol className="list-decimal space-y-1 pl-5">{plan?.operations.map(({ operationId, operation }) => <li key={operationId} className="break-words">{operation.type}: {operation.elementId}{"effectId" in operation ? ` / ${operation.effectId}` : ""}{operation.trackId ? ` (${operation.trackId})` : ""}</li>)}</ol>
					<details><summary className="cursor-pointer">{vi ? "Sửa kế hoạch JSON" : "Edit plan JSON"}</summary><Textarea aria-label="Edit plan JSON" value={draft} disabled={busy || applied} rows={12} className="mt-2 font-mono text-xs" onChange={(e) => { setDraft(e.target.value); setValidated(false); }} /></details>
					<div className="flex flex-wrap gap-2">
						<Button size="sm" variant="outline" disabled={busy || applied} onClick={() => void run(async (signal) => { const candidate = readDraft(); await editor.agent.previewPlan(candidate, undefined, signal); signal.throwIfAborted(); setPlan(candidate); setValidated(true); setStatus(vi ? "Kế hoạch hợp lệ." : "Plan validated."); })}>{vi ? "Kiểm tra lại" : "Validate again"}</Button>
						<Button size="sm" disabled={busy || applied || !validated} onClick={() => void run(async (signal) => {
							const candidate = readDraft();
							await editor.agent.applyPlan(candidate, { ...EDIT_PERMISSION, trackIds: candidate.targetScope.trackIds, elementIds: candidate.targetScope.elementIds.length ? candidate.targetScope.elementIds : undefined }, signal);
							setApplied(true); setStatus(vi ? "Đã áp dụng. Có thể chỉnh trực tiếp trên timeline và bảng thuộc tính, lưu hoặc xuất bằng các nút hiện có." : "Applied. Edit the timeline and properties; use existing Save/Export controls.");
						})}>{vi ? "Áp dụng kế hoạch" : "Apply plan"}</Button>
					</div>
				</>}
				<p role="status" className="break-words">{status}</p>
				<div className="flex gap-2"><Button size="sm" variant="outline" disabled={busy} onClick={() => { editor.command.undo(); setAuditVersion((n) => n + 1); }}>{vi ? "Hoàn tác bước cuối" : "Undo last edit"}</Button><Button size="sm" variant="outline" disabled={busy} onClick={() => { editor.command.redo(); setAuditVersion((n) => n + 1); }}>Redo</Button></div>
				<details><summary className="cursor-pointer">{vi ? "Lịch sử kế hoạch" : "Plan history"} ({history.length})</summary><ul className="mt-2 space-y-1">{history.map((entry, i) => <li key={`${entry.planId}:${i}`} className="break-words">{entry.time}: {entry.status} · {entry.operations}{entry.code ? ` · ${entry.code}` : ""}</li>)}</ul></details>
			</div>
		</PanelView>
	);
}

"use client";

import { useEffect, useState } from "react";
import {
	CheckCircle2,
	Circle,
	Copy,
	Eye,
	EyeOff,
	Link2,
	Loader2,
	RefreshCw,
	ShieldCheck,
} from "lucide-react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Switch } from "@/components/ui/switch";
import { useEditor } from "@/editor/use-editor";
import { useTranslation } from "@/i18n";
import { toast } from "sonner";
import { toolDetails } from "./contracts";
import {
	disableMcp,
	enableMcp,
	isLocalMcpHost,
	setMcpEditing,
	useMcpConnection,
} from "./connection-store";

export function McpSettingsView() {
	const { t, language } = useTranslation();
	const project = useEditor((editor) => editor.project.getActive());
	const { connection, status, allowEdits, error, activity } =
		useMcpConnection();
	const [origin, setOrigin] = useState("");
	const [isLocal, setIsLocal] = useState(false);
	const [showToken, setShowToken] = useState(false);
	const [updatingPolicy, setUpdatingPolicy] = useState(false);
	useEffect(() => {
		setOrigin(window.location.origin);
		setIsLocal(isLocalMcpHost(window.location.hostname));
	}, []);
	const endpoint = `${origin}/api/mcp`;
	const config = connection
		? JSON.stringify(
				{
					mcpServers: {
						catchim: {
							url: endpoint,
							headers: { Authorization: `Bearer ${connection.accessToken}` },
						},
					},
				},
				null,
				2,
			)
		: "";
	const copy = async (value: string) => {
		try {
			await navigator.clipboard.writeText(value);
			toast.success(t("mcp.copied"));
		} catch {
			toast.error(t("mcp.copyFailed"));
		}
	};
	const connect = () => {
		setShowToken(false);
		void enableMcp({ id: project.metadata.id, name: project.metadata.name });
	};
	const statusText =
		status === "connected"
			? t("mcp.connected")
			: status === "connecting"
				? t("mcp.connecting")
				: status === "error"
					? t("mcp.failed")
					: t("mcp.disabled");

	return (
		<div className="flex flex-col gap-4 p-3.5 text-sm">
			<div className="flex items-start gap-2">
				<Link2 className="mt-0.5 size-4 shrink-0" />
				<div className="space-y-1">
					<h3 className="font-medium">Model Context Protocol</h3>
					<p className="text-muted-foreground text-xs leading-relaxed">
						{t("mcp.description")}
					</p>
				</div>
			</div>
			<div className="flex items-center justify-between gap-2 rounded-md border p-3">
				<div className="flex items-center gap-2">
					{status === "connecting" ? (
						<Loader2 className="size-4 animate-spin" />
					) : status === "connected" ? (
						<CheckCircle2 className="size-4 text-emerald-500" />
					) : (
						<Circle className="text-muted-foreground size-4" />
					)}
					<span role="status">{statusText}</span>
				</div>
				<Button
					size="sm"
					variant={connection ? "outline" : "default"}
					disabled={!isLocal || status === "connecting"}
					onClick={() => (connection ? void disableMcp() : connect())}
				>
					{connection ? t("mcp.disable") : t("mcp.enable")}
				</Button>
			</div>
			{!isLocal && (
				<p className="text-muted-foreground text-xs">{t("mcp.localOnly")}</p>
			)}
			{error && (
				<p role="alert" className="text-destructive break-words text-xs">
					{error}
				</p>
			)}
			{connection && (
				<>
					<div className="flex items-start justify-between gap-3 rounded-md border p-3">
						<div className="space-y-1">
							<Label htmlFor="mcp-allow-edits">{t("mcp.allowEdits")}</Label>
							<p className="text-muted-foreground text-xs">
								{allowEdits ? t("mcp.readWrite") : t("mcp.readOnly")}
							</p>
						</div>
						<Switch
							id="mcp-allow-edits"
							checked={allowEdits}
							disabled={updatingPolicy}
							onCheckedChange={async (value) => {
								setUpdatingPolicy(true);
								try {
									await setMcpEditing(value);
								} finally {
									setUpdatingPolicy(false);
								}
							}}
						/>
					</div>
					<div className="space-y-1.5">
						<Label htmlFor="mcp-endpoint">{t("mcp.endpoint")}</Label>
						<div className="flex gap-1">
							<Input
								id="mcp-endpoint"
								readOnly
								value={endpoint}
								className="font-mono text-xs"
							/>
							<Button
								size="icon"
								variant="outline"
								aria-label={t("mcp.copyEndpoint")}
								title={t("mcp.copyEndpoint")}
								onClick={() => void copy(endpoint)}
							>
								<Copy className="size-4" />
							</Button>
						</div>
					</div>
					<div className="space-y-1.5">
						<Label htmlFor="mcp-token">{t("mcp.token")}</Label>
						<div className="flex gap-1">
							<Input
								id="mcp-token"
								readOnly
								type={showToken ? "text" : "password"}
								value={connection.accessToken}
								autoComplete="off"
								className="font-mono text-xs"
							/>
							<Button
								size="icon"
								variant="outline"
								aria-label={showToken ? t("mcp.hideToken") : t("mcp.showToken")}
								onClick={() => setShowToken(!showToken)}
							>
								{showToken ? (
									<EyeOff className="size-4" />
								) : (
									<Eye className="size-4" />
								)}
							</Button>
							<Button
								size="icon"
								variant="outline"
								aria-label={t("mcp.copyToken")}
								title={t("mcp.copyToken")}
								onClick={() => void copy(connection.accessToken)}
							>
								<Copy className="size-4" />
							</Button>
						</div>
					</div>
					<div className="flex flex-wrap gap-2">
						<Button size="sm" onClick={() => void copy(config)}>
							<Copy className="size-3.5" />
							{t("mcp.copyConfig")}
						</Button>
						<Button size="sm" variant="outline" onClick={connect}>
							<RefreshCw className="size-3.5" />
							{t("mcp.regenerate")}
						</Button>
					</div>
					<p className="text-muted-foreground flex items-start gap-2 text-xs leading-relaxed">
						<ShieldCheck className="mt-0.5 size-3.5 shrink-0" />
						{t("mcp.sessionHint")}
					</p>
				</>
			)}
			<p className="text-muted-foreground text-xs leading-relaxed">
				{t("mcp.keepTabOpen")}
			</p>
			<details className="rounded-md border p-3">
				<summary className="cursor-pointer font-medium">
					{t("mcp.tools")} · {Object.keys(toolDetails).length}
				</summary>
				<ul className="mt-2 space-y-1.5">
					{Object.entries(toolDetails).map(([name, detail]) => (
						<li
							key={name}
							className="flex items-center justify-between gap-2 text-xs"
						>
							<code className="break-all">{name.replace("catchim_", "")}</code>
							<span className="text-muted-foreground shrink-0">
								{detail.readOnly ? t("mcp.readOnlyLabel") : t("mcp.editLabel")}
							</span>
						</li>
					))}
				</ul>
			</details>
			<div className="space-y-2 border-t pt-3">
				<h4 className="font-medium">{t("mcp.activity")}</h4>
				{!activity.length ? (
					<p className="text-muted-foreground text-xs">
						{t("mcp.emptyActivity")}
					</p>
				) : (
					<ul className="space-y-2">
						{activity.map((item) => (
							<li key={item.id} className="rounded border p-2 text-xs">
								<div className="flex justify-between gap-2">
									<code className="break-all">
										{item.name.replace("catchim_", "")}
									</code>
									<span
										className={
											item.ok ? "text-emerald-500" : "text-destructive"
										}
									>
										{item.ok ? t("mcp.success") : t("mcp.error")}
									</span>
								</div>
								<time className="text-muted-foreground">
									{new Date(item.time).toLocaleTimeString(
										language === "vi" ? "vi-VN" : "en-US",
									)}
								</time>
								{item.error && (
									<p className="text-destructive mt-1 break-words">
										{item.error}
									</p>
								)}
							</li>
						))}
					</ul>
				)}
			</div>
		</div>
	);
}

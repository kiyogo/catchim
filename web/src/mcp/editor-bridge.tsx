"use client";

import { useEffect } from "react";
import { useEditor } from "@/editor/use-editor";
import { executeEditorTool } from "./editor-tools";
import {
	isEditorToolName,
	toolDetails,
	type BridgeJob,
	type BridgeResult,
} from "./contracts";
import {
	bridgeHeaders,
	disableMcp,
	logMcpActivity,
	useMcpConnection,
} from "./connection-store";

export function McpEditorBridge() {
	const editor = useEditor();
	const connection = useMcpConnection((state) => state.connection);
	useEffect(
		() => () => {
			void disableMcp();
		},
		[],
	);
	useEffect(() => {
		if (!connection) return;
		if (
			editor.project.getActiveOrNull()?.metadata.id !== connection.projectId
		) {
			void disableMcp();
			return;
		}
		const controller = new AbortController();
		const { signal } = controller;
		let timer: ReturnType<typeof setTimeout> | undefined;
		const pause = () =>
			new Promise<void>((resolve) => {
				const done = () => {
					clearTimeout(timer);
					signal.removeEventListener("abort", done);
					resolve();
				};
				timer = setTimeout(done, 750);
				signal.addEventListener("abort", done, { once: true });
			});
		const stop = () => {
			if (useMcpConnection.getState().connection?.id === connection.id)
				void disableMcp();
		};
		const unsubscribe = editor.project.subscribe(() => {
			if (
				editor.project.getActiveOrNull()?.metadata.id !==
					connection.projectId ||
				editor.project.getIsLoading()
			)
				stop();
		});
		window.addEventListener("pagehide", stop);
		void (async () => {
			let failures = 0;
			while (!signal.aborted) {
				try {
					const response = await fetch("/api/mcp/bridge", {
						headers: bridgeHeaders(connection),
						cache: "no-store",
						signal: AbortSignal.any([signal, AbortSignal.timeout(10000)]),
					});
					if (response.status === 401)
						throw new Error(
							"MCP connection expired. Enable it again in Settings → MCP.",
						);
					if (!response.ok)
						throw new Error(`MCP bridge returned ${response.status}`);
					const { job } = (await response.json()) as { job: BridgeJob | null };
					if (signal.aborted) break;
					failures = 0;
					if (job) {
						let result: BridgeResult;
						try {
							if (!isEditorToolName(job.name)) throw new Error("Unknown tool");
							if (job.expiresAt <= Date.now())
								throw new Error("Request expired before execution");
							if (
								editor.project.getActiveOrNull()?.metadata.id !==
									connection.projectId ||
								signal.aborted
							)
								throw new Error("The connected project changed");
							const state = useMcpConnection.getState();
							if (
								state.connection?.id !== connection.id ||
								(!toolDetails[job.name].readOnly && !state.allowEdits)
							)
								throw new Error("MCP editing permission is disabled");
							result = {
								ok: true,
								data: await executeEditorTool(editor, job.name, job.input),
							};
						} catch (error) {
							result = {
								ok: false,
								error: (error instanceof Error
									? error.message
									: "Editor operation failed"
								).slice(0, 2000),
							};
						}
						if (
							signal.aborted ||
							useMcpConnection.getState().connection?.id !== connection.id
						)
							break;
						logMcpActivity({
							id: job.id,
							name: job.name,
							time: Date.now(),
							ok: result.ok,
							error: result.ok ? undefined : result.error,
						});
						if (signal.aborted) break;
						// Deliver each command once. An acknowledgement failure never replays an edit.
						const acknowledgement = await fetch("/api/mcp/bridge", {
							method: "POST",
							headers: bridgeHeaders(connection),
							body: JSON.stringify({ jobId: job.id, result }),
							signal: AbortSignal.any([signal, AbortSignal.timeout(10000)]),
						});
						if (!acknowledgement.ok)
							throw new Error(
								"MCP result delivery failed. Inspect editor state before retrying.",
							);
						const receipt = (await acknowledgement.json()) as {
							accepted: boolean;
						};
						if (!receipt.accepted)
							logMcpActivity({
								id: job.id,
								name: job.name,
								time: Date.now(),
								ok: false,
								error:
									"Request expired or was cancelled after delivery. Inspect editor state before retrying.",
							});
					}
				} catch (error) {
					if (signal.aborted) break;
					if (++failures >= 3) {
						await disableMcp(
							error instanceof Error ? error.message : "MCP disconnected",
						);
						break;
					}
				}
				if (!signal.aborted) await pause();
			}
		})();
		return () => {
			controller.abort();
			clearTimeout(timer);
			unsubscribe();
			window.removeEventListener("pagehide", stop);
			stop();
		};
	}, [editor, connection]);
	return null;
}

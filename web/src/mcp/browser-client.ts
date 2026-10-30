"use client";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { useMcpConnection } from "./connection-store";
import { McpEditError } from "./edit-plan";
import { parseToolInput, type EditorToolName, type ToolInput } from "./contracts";

/** The editor's own MCP workspace uses the same authenticated protocol as external clients. */
export async function callEditorMcp<N extends EditorToolName>(name: N, input: ToolInput<N>, signal?: AbortSignal): Promise<Record<string, unknown>> {
	const { connection } = useMcpConnection.getState();
	if (!connection) throw new McpEditError("PERMISSION_DENIED", "Enable the MCP connection first");
	const client = new Client({ name: "catchim-mcp-workspace", version: "1.0.0" });
	const deadline = AbortSignal.any([signal ?? new AbortController().signal, AbortSignal.timeout(25000)]);
	const transport = new StreamableHTTPClientTransport(new URL("/api/mcp", window.location.origin), { requestInit: { headers: { Authorization: `Bearer ${connection.accessToken}` }, signal: deadline, redirect: "error" } });
	try {
		await client.connect(transport);
		const result = await client.callTool({ name, arguments: parseToolInput(name, input) }, undefined, { signal: deadline, timeout: 22000 });
		const data = result.structuredContent && typeof result.structuredContent === "object" && !Array.isArray(result.structuredContent) ? result.structuredContent as Record<string, unknown> : null;
		if (result.isError) {
			const structured = data?.error as { code?: string; message?: string } | undefined;
			const text = result.content as Array<{ type: string; text?: string }>;
			throw new Error(structured?.message ?? text.find((item) => item.type === "text")?.text ?? "MCP tool failed");
		}
		if (!data) throw new Error("MCP tool returned no structured result");
		return data;
	} finally { await client.close(); }
}

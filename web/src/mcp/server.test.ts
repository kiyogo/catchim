import { describe, expect, it } from "vitest";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { McpBroker, mcpBroker } from "./broker.server";
import { handleMcpRequest } from "./server";
import { localRequestError } from "./http.server";
import {
	GET as poll,
	POST as bridgePost,
	DELETE as disconnect,
} from "@/app/api/mcp/bridge/route";
import { POST as mcpPost } from "@/app/api/mcp/route";

describe("MCP Streamable HTTP", () => {
	it("negotiates with the official client, lists tools/resources, relays reads and rejects unauthorized edits", async () => {
		const broker = new McpBroker();
		const connection = broker.create({
			projectId: "fixture",
			projectName: "Fixture",
			allowEdits: false,
		});
		const client = new Client({
			name: "catchim-integration-test",
			version: "1.0.0",
		});
		const transport = new StreamableHTTPClientTransport(
			new URL("http://localhost:3300/api/mcp"),
			{
				fetch: async (url, options) =>
					handleMcpRequest(new Request(url, options), connection, broker),
			},
		);
		const browser = setInterval(() => {
			const job = broker.takeJob(connection);
			if (job)
				broker.complete(connection, job.id, {
					ok: true,
					data: { projectId: "fixture", name: job.name },
				});
		}, 5);
		try {
			await client.connect(transport);
			const tools = await client.listTools();
			expect(tools.tools).toHaveLength(17);
			expect(
				tools.tools.find((tool) => tool.name === "catchim_get_timeline")
					?.annotations?.readOnlyHint,
			).toBe(true);
			expect(
				(await client.callTool({ name: "catchim_get_project", arguments: {} }))
					.structuredContent,
			).toMatchObject({ projectId: "fixture" });
			const denied = await client.callTool({
				name: "catchim_history",
				arguments: { action: "undo" },
			});
			expect(denied.isError).toBe(true);
			const invalid = await client.callTool({
				name: "catchim_seek",
				arguments: { seconds: -1 },
			});
			expect(invalid.isError).toBe(true);
			expect((await client.listResources()).resources).toHaveLength(3);
			expect(
				(await client.readResource({ uri: "catchim://project" })).contents[0],
			).toMatchObject({ mimeType: "application/json" });
		} finally {
			clearInterval(browser);
			broker.disconnect(connection);
			await client.close();
		}
	});
	it("rejects foreign origins, DNS-rebinding hosts and non-local endpoints", () => {
		for (const request of [
			new Request("http://evil.example/api/mcp"),
			new Request("http://localhost:3300/api/mcp", {
				headers: { Origin: "https://evil.example" },
			}),
			new Request("http://localhost:3300/api/mcp", {
				headers: { Host: "rebind.example:3300" },
			}),
		])
			expect(localRequestError(request)?.status).toBe(403);
		expect(
			localRequestError(new Request("http://localhost:3300/api/mcp")),
		).toBeNull();
		expect(
			localRequestError(new Request("http://localhost:3300/api/mcp"), true)
				?.status,
		).toBe(403);
	});
	it("accepts Next.js internal bind URLs only with a valid public loopback Host and matching Origin", () => {
		expect(
			localRequestError(
				new Request("http://0.0.0.0:3300/api/mcp/bridge", {
					headers: { Host: "localhost:3300", Origin: "http://localhost:3300" },
				}),
				true,
			),
		).toBeNull();
		expect(
			localRequestError(
				new Request("http://0.0.0.0:3300/api/mcp/bridge", {
					headers: { Host: "localhost:3300", Origin: "https://evil.example" },
				}),
				true,
			)?.status,
		).toBe(403);
		expect(
			localRequestError(new Request("http://0.0.0.0:3300/api/mcp"))?.status,
		).toBe(403);
	});
	it("cancels undelivered browser work when the HTTP request is aborted", async () => {
		const broker = new McpBroker();
		const connection = broker.create({
			projectId: "fixture",
			projectName: "Fixture",
			allowEdits: true,
		});
		const controller = new AbortController();
		const response = handleMcpRequest(
			new Request("http://localhost:3300/api/mcp", {
				method: "POST",
				headers: {
					"Content-Type": "application/json",
					Accept: "application/json, text/event-stream",
				},
				body: JSON.stringify({
					jsonrpc: "2.0",
					id: 1,
					method: "tools/call",
					params: { name: "catchim_seek", arguments: { seconds: 1 } },
				}),
				signal: controller.signal,
			}),
			connection,
			broker,
		);
		await new Promise((resolve) => setTimeout(resolve, 10));
		controller.abort();
		expect((await (await response).json()).result.isError).toBe(true);
		expect(broker.takeJob(connection)).toBeNull();
		broker.disconnect(connection);
	});
	it("requires different tokens for MCP calls and browser control, with immediate revocation", async () => {
		const origin = "http://localhost:3300";
		const created = await bridgePost(
			new Request(`${origin}/api/mcp/bridge`, {
				method: "POST",
				headers: { Origin: origin, "Content-Type": "application/json" },
				body: JSON.stringify({
					projectId: "test-fixture",
					projectName: "Fixture",
				}),
			}),
		);
		expect(created.status).toBe(201);
		const credentials = await created.json();
		try {
			expect(
				(
					await poll(
						new Request(`${origin}/api/mcp/bridge`, {
							headers: { Authorization: `Bearer ${credentials.accessToken}` },
						}),
					)
				).status,
			).toBe(401);
			expect(
				(
					await mcpPost(
						new Request(`${origin}/api/mcp`, {
							method: "POST",
							headers: { Authorization: `Bearer ${credentials.bridgeToken}` },
						}),
					)
				).status,
			).toBe(401);
			expect(
				(
					await poll(
						new Request(`${origin}/api/mcp/bridge`, {
							headers: { Authorization: `Bearer ${credentials.bridgeToken}` },
						}),
					)
				).status,
			).toBe(200);
			await disconnect(
				new Request(`${origin}/api/mcp/bridge`, {
					method: "DELETE",
					headers: {
						Origin: origin,
						Authorization: `Bearer ${credentials.bridgeToken}`,
					},
				}),
			);
			expect(mcpBroker.authorize(credentials.accessToken, "access")).toBeNull();
		} finally {
			const connection = mcpBroker.authorize(credentials.bridgeToken, "bridge");
			if (connection) mcpBroker.disconnect(connection);
		}
	});
});

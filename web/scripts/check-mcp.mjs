import assert from "node:assert/strict";
import { setTimeout as pause } from "node:timers/promises";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";

const origin = process.argv[2] ?? "http://localhost:3300";
const base = new URL(origin);
assert.ok(["localhost", "127.0.0.1", "[::1]"].includes(base.hostname) && ["http:", "https:"].includes(base.protocol) && !base.username && !base.password && base.origin === origin, "Use a loopback origin without credentials or paths");
const bridgeUrl = `${origin}/api/mcp/bridge`;
const endpoint = `${origin}/api/mcp`;
const create = await fetch(bridgeUrl, {
	method: "POST",
	headers: { Origin: origin, "Content-Type": "application/json" },
	body: JSON.stringify({
		projectId: "mcp-smoke-fixture",
		projectName: "Synthetic MCP fixture",
		allowEdits: false,
	}),
});
assert.equal(create.status, 201, "The Next.js MCP bridge must be available");
const credentials = await create.json();
const headers = {
	Origin: origin,
	"Content-Type": "application/json",
	Authorization: `Bearer ${credentials.bridgeToken}`,
};
const controller = new AbortController();
const client = new Client({ name: "catchim-smoke", version: "1.0.0" });
const browser = (async () => {
	while (!controller.signal.aborted) {
		const response = await fetch(bridgeUrl, {
			headers,
			signal: controller.signal,
		});
		assert.equal(response.status, 200);
		const { job } = await response.json();
		if (job) {
			const result = await fetch(bridgeUrl, {
				method: "POST",
				headers,
				body: JSON.stringify({
					jobId: job.id,
					result: {
						ok: true,
						data: { synthetic: true, tool: job.name, input: job.input },
					},
				}),
				signal: controller.signal,
			});
			assert.equal((await result.json()).accepted, true);
		}
		await pause(30, undefined, { signal: controller.signal });
	}
})().catch((error) => {
	if (!controller.signal.aborted) throw error;
});

try {
	await client.connect(
		new StreamableHTTPClientTransport(new URL(endpoint), {
			requestInit: {
				headers: { Authorization: `Bearer ${credentials.accessToken}` },
			},
		}),
	);
	const listed = await client.listTools();
	assert.equal(listed.tools.length, 25);
	assert.equal(
		(await client.callTool({ name: "catchim_get_project", arguments: {} }))
			.structuredContent.synthetic,
		true,
	);
	assert.equal(
		(
			await client.callTool({
				name: "catchim_history",
				arguments: { action: "undo" },
			})
		).isError,
		true,
	);
	assert.equal((await client.listResources()).resources.length, 5);
	assert.equal((await client.listPrompts()).prompts.length, 2);
	assert.equal(
		JSON.parse(
			(await client.readResource({ uri: "catchim://timeline" })).contents[0]
				.text,
		).synthetic,
		true,
	);
	assert.equal(
		(
			await fetch(bridgeUrl, {
				headers: { Authorization: `Bearer ${credentials.accessToken}` },
			})
		).status,
		401,
	);
	assert.equal(
		(
			await fetch(endpoint, {
				method: "POST",
				headers: { Authorization: `Bearer ${credentials.bridgeToken}` },
			})
		).status,
		401,
	);
	assert.equal(
		(
			await fetch(endpoint, {
				method: "POST",
				headers: { Origin: "https://unrelated.example" },
			})
		).status,
		403,
	);
	assert.equal(
		(
			await fetch(bridgeUrl, {
				method: "PATCH",
				headers,
				body: JSON.stringify({ allowEdits: true }),
			})
		).status,
		200,
	);
	const edit = await client.callTool({
		name: "catchim_seek",
		arguments: { seconds: 1.25 },
	});
	assert.equal(edit.structuredContent.input.seconds, 1.25);
	const stdioClient = new Client({ name: "catchim-stdio-smoke", version: "1.0.0" });
	try {
		await stdioClient.connect(new StdioClientTransport({
			command: process.execPath,
			args: ["scripts/mcp-stdio.mjs"],
			cwd: process.cwd(),
			env: { ...process.env, CATCHIM_MCP_URL: endpoint, CATCHIM_MCP_TOKEN: credentials.accessToken },
		}));
		assert.equal((await stdioClient.listTools()).tools.length, 25);
		assert.equal((await stdioClient.listPrompts()).prompts.length, 2);
		assert.equal((await stdioClient.callTool({ name: "catchim_get_project", arguments: {} })).structuredContent.synthetic, true);
	} finally { await stdioClient.close(); }
} finally {
	controller.abort();
	await browser;
	await client.close();
	await fetch(bridgeUrl, { method: "DELETE", headers });
	assert.equal(
		(
			await fetch(endpoint, {
				method: "POST",
				headers: { Authorization: `Bearer ${credentials.accessToken}` },
			})
		).status,
		401,
		"Disabled tokens must be rejected",
	);
}
console.log(
	"MCP smoke PASS: HTTP + stdio, 25 tools, 5 resources, 2 prompts, protocol negotiation, browser relay, permissions, token isolation and revocation. Synthetic connection only.",
);

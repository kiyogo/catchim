import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { CallToolRequestSchema, ListToolsRequestSchema, ListResourcesRequestSchema, ReadResourceRequestSchema, ListPromptsRequestSchema, GetPromptRequestSchema } from "@modelcontextprotocol/sdk/types.js";

// Compatibility adapter for the installed SDK's 2025-11-25 protocol.
const endpoint = new URL(process.env.CATCHIM_MCP_URL ?? "http://localhost:3300/api/mcp");
if (!["localhost", "127.0.0.1", "[::1]"].includes(endpoint.hostname) || !["http:", "https:"].includes(endpoint.protocol) || endpoint.username || endpoint.password || endpoint.search || endpoint.hash) throw new Error("CATCHIM_MCP_URL must be a loopback HTTP(S) endpoint");
const token = process.env.CATCHIM_MCP_TOKEN;
if (!/^[A-Za-z0-9_-]{43}$/.test(token ?? "")) throw new Error("Set CATCHIM_MCP_TOKEN to the access token from Catchim Settings → MCP");
const client = new Client({ name: "catchim-stdio-adapter", version: "1.0.0" });
const http = new StreamableHTTPClientTransport(endpoint, { requestInit: { headers: { Authorization: `Bearer ${token}` }, redirect: "error" } });
await client.connect(http);
const server = new Server({ name: "catchim-stdio", version: "1.0.0" }, { capabilities: { tools: {}, resources: {}, prompts: {} } });
server.setRequestHandler(ListToolsRequestSchema, (request) => client.listTools(request.params));
server.setRequestHandler(ListResourcesRequestSchema, (request) => client.listResources(request.params));
server.setRequestHandler(ReadResourceRequestSchema, (request, extra) => client.readResource(request.params, { signal: extra.signal, timeout: 25000 }));
server.setRequestHandler(CallToolRequestSchema, (request, extra) => client.callTool(request.params, undefined, { signal: extra.signal, timeout: 25000 }));
server.setRequestHandler(ListPromptsRequestSchema, (request) => client.listPrompts(request.params));
server.setRequestHandler(GetPromptRequestSchema, (request, extra) => client.getPrompt(request.params, { signal: extra.signal, timeout: 25000 }));
let closing = false;
async function close() { if (closing) return; closing = true; await server.close(); await client.close(); }
process.on("SIGINT", () => void close());
process.on("SIGTERM", () => void close());
server.onclose = () => void close();
await server.connect(new StdioServerTransport());
